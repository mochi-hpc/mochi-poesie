#include "poesie-client.h"
#include "poesie-rpc-types.h"

struct poesie_client {
    margo_instance_id mid;

    hg_id_t poesie_get_vm_info_id;
    hg_id_t poesie_execute_id;

    uint64_t num_provider_handles;
};

struct poesie_provider_handle {
    poesie_client_t client;
    hg_addr_t      addr;
    uint8_t        mplex_id;
    uint64_t       refcount;
};

static int poesie_client_register(poesie_client_t client, margo_instance_id mid)
{
    client->mid = mid;

    /* check if RPCs have already been registered */
    hg_bool_t flag;
    hg_id_t id;
    margo_registered_name(mid, "poesie_get_vm_info_rpc", &id, &flag);

    if(flag == HG_TRUE) { /* RPCs already registered */

        margo_registered_name(mid, "poesie_get_vm_info_rpc", &client->poesie_get_vm_info_id, &flag);
        margo_registered_name(mid, "poesie_execute_rpc",     &client->poesie_execute_id,     &flag);

    } else {

        client->poesie_get_vm_info_id =
            MARGO_REGISTER(mid, "poesie_get_vm_info_rpc", get_vm_info_in_t, get_vm_info_out_t, NULL);
        client->poesie_execute_id =
            MARGO_REGISTER(mid, "poesie_execute_rpc", execute_in_t, execute_out_t, NULL);
    }

    return POESIE_SUCCESS;
}

int poesie_client_init(margo_instance_id mid, poesie_client_t* client)
{
    poesie_client_t c = (poesie_client_t)calloc(1, sizeof(*c));
    if(!c) return POESIE_ERR_ALLOCATION;

    c->num_provider_handles = 0;

    int ret = poesie_client_register(c, mid);
    if(ret != 0) return ret;

    *client = c;
    return POESIE_SUCCESS;
}

int poesie_client_finalize(poesie_client_t client)
{
    if(client->num_provider_handles != 0) {
        fprintf(stderr,
                "[POESIE] Warning: %d provider handles not released before poesie_client_finalize was called\n",
                client->num_provider_handles);
    }
    free(client);
    return POESIE_SUCCESS;
}

int poesie_provider_handle_create(
        poesie_client_t client,
        hg_addr_t addr,
        uint8_t mplex_id,
        poesie_provider_handle_t* handle)
{
    if(client == POESIE_CLIENT_NULL) 
        return POESIE_ERR_INVALID_ARG;

    poesie_provider_handle_t provider =
        (poesie_provider_handle_t)calloc(1, sizeof(*provider));

    if(!provider) return POESIE_ERR_ALLOCATION;

    hg_return_t ret = margo_addr_dup(client->mid, addr, &(provider->addr));
    if(ret != HG_SUCCESS) {
        free(provider);
        return POESIE_ERR_MERCURY;
    }

    provider->client   = client;
    provider->mplex_id = mplex_id;
    provider->refcount = 1;

    client->num_provider_handles += 1;

    *handle = provider;
    return POESIE_SUCCESS;
}

int poesie_provider_handle_ref_incr(
        poesie_provider_handle_t handle)
{
    if(handle == POESIE_PROVIDER_HANDLE_NULL)
        return POESIE_ERR_INVALID_ARG;
    handle->refcount += 1;
    return POESIE_SUCCESS;
}

int poesie_provider_handle_release(poesie_provider_handle_t handle)
{
    if(handle == POESIE_PROVIDER_HANDLE_NULL)
        return POESIE_SUCCESS;
    handle->refcount -= 1;
    if(handle->refcount == 0) {
        margo_addr_free(handle->client->mid, handle->addr);
        handle->client->num_provider_handles -= 1;
        free(handle);
    }
    return POESIE_SUCCESS;
}

int poesie_get_vm_info(
        poesie_provider_handle_t provider,
        const char* vm_name,
        poesie_vm_id_t* vm_id,
        poesie_lang_t* lang)
{
    hg_return_t hret;
    int ret;
    get_vm_info_in_t in;
    get_vm_info_out_t out;
    hg_handle_t handle;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->poesie_get_vm_info_id,
            &handle);
    if(hret != HG_SUCCESS) return POESIE_ERR_MERCURY;

    hret = margo_set_target_id(handle, provider->mplex_id);

    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    in.name = (char*)vm_name;

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    ret    = out.ret;
    *vm_id = out.vm_id;
    *lang  = out.lang;

    margo_free_output(handle, &out);
    margo_destroy(handle);

    return ret;
}

int poesie_execute(
        poesie_provider_handle_t provider, 
        poesie_vm_id_t vm_id,
        poesie_lang_t lang,
        const char* code,
        char** output)
{
    hg_return_t hret;
    int ret;
    hg_handle_t handle;

    execute_in_t in;
    execute_out_t out;

    in.vm_id = vm_id;
    in.lang  = lang;
    in.code  = (char*)code;

    /* create handle */
    hret = margo_create(
            provider->client->mid,
            provider->addr,
            provider->client->poesie_execute_id,
            &handle);
    if(hret != HG_SUCCESS) {
        fprintf(stderr,"[POESIE] margo_create() failed in poesie_execute()\n");
        return POESIE_ERR_MERCURY;
    }

    hret = margo_set_target_id(handle, provider->mplex_id);

    if(hret != HG_SUCCESS) {
        fprintf(stderr,"[POESIE] margo_set_target_id() failed in poesie_execute()\n");
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    hret = margo_forward(handle, &in);
    if(hret != HG_SUCCESS) {
        fprintf(stderr,"[POESIE] margo_forward() failed in poesie_execute()\n");
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    hret = margo_get_output(handle, &out);
    if(hret != HG_SUCCESS) {
        fprintf(stderr,"[POESIE] margo_get_output() failed in poesie_execute()\n");
        margo_destroy(handle);
        return POESIE_ERR_MERCURY;
    }

    ret = out.ret;
    *output = strdup(out.output);

    margo_free_output(handle, &out);

    margo_destroy(handle);
    return POESIE_SUCCESS;
}

int poesie_shutdown_service(poesie_client_t client, hg_addr_t addr)
{
    return margo_shutdown_remote_instance(client->mid, addr);
}

int poesie_free_output(char* output)
{
    free(output);
    return POESIE_SUCCESS;
}
