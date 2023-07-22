/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <margo-logging.h>
#include "poesie-vm.h"
#include "poesie-vm-impl.h"
#include "poesie-rpc-types.h"
#include "poesie/server.h"

struct poesie_provider
{
    margo_instance_id  mid;
    uint64_t           vm_arr_size;
    uint64_t           num_vms;
    poesie_vm_t*       vms;
    char**             vm_names;

    hg_id_t get_vm_info_id;
    hg_id_t execute_id;
    hg_id_t create_vm_id;
    hg_id_t delete_vm_id;
};

DECLARE_MARGO_RPC_HANDLER(poesie_get_vm_info_ult)
DECLARE_MARGO_RPC_HANDLER(poesie_execute_ult)
DECLARE_MARGO_RPC_HANDLER(poesie_create_vm_ult)
DECLARE_MARGO_RPC_HANDLER(poesie_delete_vm_ult)

poesie_vm_id_t insert_poesie_vm(poesie_provider_t provider, const char* name, poesie_vm_t vm);
poesie_vm_id_t find_poesie_vm_id(poesie_provider_t provider, const char* name);
int remove_poesie_vm(poesie_provider_t provider, poesie_vm_id_t id);

static void poesie_server_finalize_cb(void *data);
static int validate_config(margo_instance_id mid, struct json_object* config);
static int create_vms_from_config(poesie_provider_t provider, struct json_object* config);

int poesie_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        const struct poesie_provider_args* args,
        poesie_provider_t* provider)
{
    poesie_provider_t tmp_provider;
    struct json_object* json_config = NULL;

    margo_trace(mid, "[poesie] Registing provider with id %d",
                (unsigned)provider_id);

    struct poesie_provider_args aargs = POESIE_PROVIDER_ARGS_DEFAULT;
    if(args) {
        memcpy(&aargs, args, sizeof(aargs));
    }
    ABT_pool abt_pool = aargs.pool;

    /* parse JSON config */
    if(aargs.config && aargs.config[0]) {
        struct json_tokener*    tokener = json_tokener_new();
        enum json_tokener_error jerr;
        json_config = json_tokener_parse_ex(tokener, aargs.config,
                                            strlen(aargs.config));
        if(!json_config) {
            jerr = json_tokener_get_error(tokener);
            margo_error(mid, "[poesie] JSON parse error: %s",
                        json_tokener_error_desc(jerr));
            json_tokener_free(tokener);
            return -1;
        }
        json_tokener_free(tokener);
    }
    if(validate_config(mid, json_config) != 0) {
        json_object_put(json_config);
        return -1;
    }

    /* check if a provider with the same multiplex id already exists */
    {
        hg_id_t id;
        hg_bool_t flag;
        margo_provider_registered_name(mid, "poesie_get_vm_info_rpc", provider_id, &id, &flag);
        if(flag == HG_TRUE) {
            margo_error(mid, "[poesie] a provider with the same id (%d) already exists", provider_id);
            return POESIE_ERR_MERCURY;
        }
    }

    /* allocate the resulting structure */
    tmp_provider = (poesie_provider_t)calloc(1,sizeof(*tmp_provider));
    if(!tmp_provider)
        return POESIE_ERR_ALLOCATION;

    tmp_provider->mid = mid;

    /* register RPCs */
    hg_id_t rpc_id;
    rpc_id = MARGO_REGISTER_PROVIDER(mid, "poesie_get_vm_info_rpc",
            get_vm_info_in_t, get_vm_info_out_t,
            poesie_get_vm_info_ult, provider_id, abt_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->get_vm_info_id = rpc_id;

    rpc_id = MARGO_REGISTER_PROVIDER(mid, "poesie_execute_rpc",
            execute_in_t, execute_out_t,
            poesie_execute_ult, provider_id, abt_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->execute_id = rpc_id;

    rpc_id = MARGO_REGISTER_PROVIDER(mid, "poesie_create_vm_rpc",
            create_vm_in_t, create_vm_out_t,
            poesie_create_vm_ult, provider_id, abt_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->create_vm_id = rpc_id;

    rpc_id = MARGO_REGISTER_PROVIDER(mid, "poesie_delete_vm_rpc",
            delete_vm_in_t, delete_vm_out_t,
            poesie_delete_vm_ult, provider_id, abt_pool);
    margo_register_data(mid, rpc_id, (void*)tmp_provider, NULL);
    tmp_provider->delete_vm_id = rpc_id;

    /* install the bake server finalize callback */
    margo_provider_push_finalize_callback(mid, tmp_provider,
        &poesie_server_finalize_cb, tmp_provider);

    create_vms_from_config(tmp_provider, json_config);

    if(json_config)
        json_object_put(json_config);

    if(provider != POESIE_PROVIDER_IGNORE)
        *provider = tmp_provider;

    margo_trace(mid, "[poesie] Successfully registed provider with id %d",
                (unsigned)provider_id);

    return POESIE_SUCCESS;
}

int poesie_provider_destroy(poesie_provider_t provider)
{
    if(!provider) {
        margo_error(MARGO_INSTANCE_NULL,
            "[poesie] poesie_provider_destroy called with invalid provider");
        return POESIE_ERR_INVALID_ARG;
    }
    margo_provider_pop_finalize_callback(
        provider->mid, provider);

    poesie_server_finalize_cb((void*)provider);

    return POESIE_SUCCESS;
}

static void poesie_server_finalize_cb(void *data)
{
    poesie_provider_t provider = (poesie_provider_t)data;
    if(!provider) {
        margo_error(MARGO_INSTANCE_NULL,
            "[poesie] Finalization callback called with invalid provider");
        return;
    }

    margo_deregister(provider->mid, provider->get_vm_info_id);
    margo_deregister(provider->mid, provider->execute_id);
    margo_deregister(provider->mid, provider->create_vm_id);
    margo_deregister(provider->mid, provider->delete_vm_id);

    poesie_provider_remove_all_vms(provider);

    free(provider);
}

int poesie_provider_add_vm(
        poesie_provider_t provider,
        const char *vm_name,
        poesie_lang_t vm_lang,
        poesie_vm_id_t* vm_id)
{
    int ret;

    if(provider == POESIE_PROVIDER_NULL)
        return POESIE_ERR_INVALID_ARG;

    poesie_vm_id_t id = find_poesie_vm_id(provider, vm_name);
    if(id != POESIE_VM_ID_INVALID) {
        // VM with this name exists
        margo_error(provider->mid,
            "[poesie] A VM with name %s already exists", vm_name);
        return POESIE_ERR_VM_EXISTS;
    }

    poesie_vm_t vm;
    ret = poesie_vm_create(vm_name, provider->mid, vm_lang, &vm);

    if(ret != 0) {
        margo_error(provider->mid,
            "[poesie] Could not create VM %s", vm_name);
        return ret;
    }

    id = insert_poesie_vm(provider, vm_name, vm);
    *vm_id = id;

    margo_trace(provider->mid,
        "[poesie] Successfully added VM %s", vm_name);
    return POESIE_SUCCESS;
}

int poesie_provider_remove_vm(
        poesie_provider_t provider,
        poesie_vm_id_t vm_id)
{
    int ret;
    if(provider == POESIE_PROVIDER_NULL)
        return POESIE_ERR_INVALID_ARG;

    if(vm_id < 0 || provider->vm_arr_size > (uint64_t)vm_id)
        return POESIE_ERR_INVALID_ARG;

    if(provider->vms[vm_id] == NULL)
        return POESIE_ERR_INVALID_ARG;

    poesie_vm_t vm = provider->vms[vm_id];
    remove_poesie_vm(provider, vm_id);

    ret = poesie_vm_destroy(vm);

    if(ret == POESIE_SUCCESS) {
        margo_trace(provider->mid,
            "[poesie] Successfully removed VM with id %ld", vm_id);
    }
    return ret;
}

int poesie_provider_remove_all_vms(
        poesie_provider_t provider)
{
    if(provider == POESIE_PROVIDER_NULL)
        return POESIE_ERR_INVALID_ARG;

    poesie_vm_id_t id;
    for(id = 0; id < (poesie_vm_id_t)provider->vm_arr_size; id++) {
        poesie_provider_remove_vm(provider, id);
    }
    margo_trace(provider->mid,
        "[poesie] Successfully removed all VMs");
    return POESIE_SUCCESS;
}

int poesie_provider_count_vms(
        poesie_provider_t provider,
        uint64_t* num_vms)
{
    if(provider == POESIE_PROVIDER_NULL)
        return POESIE_ERR_INVALID_ARG;
    *num_vms = provider->num_vms;
    return POESIE_SUCCESS;
}

int poesie_provider_list_vms(
        poesie_provider_t provider,
        poesie_vm_id_t* vms)
{
    poesie_vm_id_t id;
    unsigned i = 0;
    for(id = 0; id < (poesie_vm_id_t)provider->vm_arr_size; id++) {
        if(provider->vms[id]) {
            vms[i] = id;
            i += 1;
        }
    }
    return POESIE_SUCCESS;
}

char* poesie_provider_get_config(poesie_provider_t provider)
{
    if(!provider) return NULL;
    struct json_object* config = json_object_new_object();
    struct json_object* vms = json_object_new_object();

    for(uint64_t i=0; i < provider->num_vms; i++) {
        const char* vm_name = provider->vm_names[i];
        poesie_vm_t vm      = provider->vms[i];
        struct json_object* vm_config = poesie_vm_get_config(vm);
        if(!vm_config) vm_config = json_object_new_null();
        json_object_object_add(vms, vm_name, vm_config);
    }
    json_object_object_add(config, "vms", vms);

    char* result = strdup(json_object_to_json_string(config));
    json_object_put(config);
    return result;
}

static void poesie_get_vm_info_ult(hg_handle_t handle)
{
    hg_return_t       hret;
    get_vm_info_in_t  in;
    get_vm_info_out_t out;

    margo_instance_id mid = margo_hg_handle_get_instance(handle);
    assert(mid);
    const struct hg_info* info = margo_get_info(handle);
    poesie_provider_t provider =
        (poesie_provider_t)margo_registered_data(mid, info->id);
    if(!provider) {
        margo_error(mid,
            "[poesie] poesie_get_vm_info_ult: no provider found");
        out.ret = POESIE_ERR_UNKNOWN_PR;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    hret = margo_get_input(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid,
            "[poesie] poesie_get_vm_info_ult: margo_get_input failed (%d)", hret);
        out.ret = POESIE_ERR_MERCURY;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    poesie_vm_id_t vm_id = find_poesie_vm_id(provider, in.name);
    if(vm_id == POESIE_VM_ID_INVALID) {
        out.ret = POESIE_ERR_NO_VM;
        goto finish;
    }

    poesie_lang_t lang = POESIE_LANG_DEFAULT;
    out.ret            = poesie_vm_get_lang(provider->vms[vm_id], &lang);
    out.lang           = lang;
    out.vm_id          = vm_id;

finish:
    margo_respond(handle, &out);
    margo_free_input(handle, &in);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(poesie_get_vm_info_ult)

static void poesie_create_vm_ult(hg_handle_t handle)
{

    hg_return_t   hret;
    create_vm_in_t  in;
    create_vm_out_t out;
    out.ret   = POESIE_SUCCESS;
    out.vm_id = POESIE_VM_ID_INVALID;

    margo_instance_id mid = margo_hg_handle_get_instance(handle);
    const struct hg_info* info = margo_get_info(handle);
    poesie_provider_t provider =
        (poesie_provider_t)margo_registered_data(mid, info->id);
    if(!provider) {
        margo_error(mid,
            "[poesie] poesie_create_vm_ult: no provider found");
        out.ret = POESIE_ERR_UNKNOWN_PR;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    hret = margo_get_input(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid,
            "[poesie] poesie_create_vm_ult: margo_get_input failed (%d)", hret);
        out.ret = POESIE_ERR_MERCURY;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    poesie_vm_id_t vm_id;
    out.ret = poesie_provider_add_vm(provider, in.name, in.lang, &vm_id);
    if(out.ret == POESIE_SUCCESS)
        out.vm_id = vm_id;

    margo_respond(handle, &out);
    margo_free_input(handle, &in);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(poesie_create_vm_ult)

static void poesie_delete_vm_ult(hg_handle_t handle)
{

    hg_return_t   hret;
    delete_vm_in_t  in;
    delete_vm_out_t out;
    out.ret   = POESIE_SUCCESS;

    margo_instance_id mid = margo_hg_handle_get_instance(handle);
    assert(mid);
    const struct hg_info* info = margo_get_info(handle);
    poesie_provider_t provider =
        (poesie_provider_t)margo_registered_data(mid, info->id);
    if(!provider) {
        margo_error(mid,
            "[poesie] poesie_delete_vm_ult: no provider found");
        out.ret = POESIE_ERR_UNKNOWN_PR;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    hret = margo_get_input(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid,
            "[poesie] poesie_delete_vm_ult: margo_get_input failed (%d)", hret);
        out.ret = POESIE_ERR_MERCURY;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    out.ret = poesie_provider_remove_vm(provider, in.vm_id);

    margo_respond(handle, &out);
    margo_free_input(handle, &in);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(poesie_delete_vm_ult)

static void poesie_execute_ult(hg_handle_t handle)
{

    hg_return_t   hret;
    int ret;
    execute_in_t  in;
    execute_out_t out;
    out.output = NULL;

    margo_instance_id mid = margo_hg_handle_get_instance(handle);
    assert(mid);
    const struct hg_info* info = margo_get_info(handle);
    poesie_provider_t provider =
        (poesie_provider_t)margo_registered_data(mid, info->id);
    if(!provider) {
        margo_error(mid,
            "[poesie] poesie_execute_ult: no provider found");
        out.ret = POESIE_ERR_UNKNOWN_PR;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    hret = margo_get_input(handle, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid,
            "[poesie] poesie_execute_ult: margo_get_input failed (%d)", hret);
        out.ret = POESIE_ERR_MERCURY;
        margo_respond(handle, &out);
        margo_destroy(handle);
        return;
    }

    poesie_vm_id_t vm_id = in.vm_id;
    poesie_vm_t vm = NULL;
    if(vm_id == POESIE_VM_ANONYMOUS) {
        poesie_vm_t vm;
        ret = poesie_vm_create(NULL, provider->mid, in.lang, &vm);
        if(ret != POESIE_SUCCESS) {
            out.ret = ret;
            out.output = NULL;
            goto finish;
        }
        ret = poesie_vm_execute(vm, in.code, &(out.output));
        if(ret != POESIE_SUCCESS) {
            out.ret = ret;
            out.output = NULL;
            poesie_vm_destroy(vm);
            goto finish;
        }
        out.ret = ret;
    } else {
        vm = provider->vms[vm_id];
        if(!vm) {
            out.ret = POESIE_ERR_NO_VM;
            out.output = NULL;
            goto finish;
        }
        out.ret = poesie_vm_execute(vm, in.code, &(out.output));
    }

finish:
    margo_respond(handle, &out);
    free(out.output);
    margo_free_input(handle, &in);
    margo_destroy(handle);
}
DEFINE_MARGO_RPC_HANDLER(poesie_execute_ult)

poesie_vm_id_t find_poesie_vm_id(poesie_provider_t provider, const char* name)
{
    int64_t i;
    for(i=0; i < (poesie_vm_id_t)provider->vm_arr_size; i++) {
        if(provider->vms[i] == NULL)
            continue;
        if(provider->vm_names[i] == NULL)
            continue;
        if(strcmp(provider->vm_names[i], name) == 0) {
            return i;
        }
    }
    return POESIE_VM_ID_INVALID;
}

int remove_poesie_vm(poesie_provider_t provider, poesie_vm_id_t id) {
    if(id < 0 || id >= (poesie_vm_id_t)provider->vm_arr_size) return -1;
    if(provider->vms[id]) {
        provider->vms[id] = NULL;
        free(provider->vm_names[id]);
        provider->vm_names[id] = NULL;
        provider->num_vms -= 1;
        return 0;
    }
    return -1;
}

poesie_vm_id_t insert_poesie_vm(poesie_provider_t provider, const char* name, poesie_vm_t vm)
{
    if(provider->vm_arr_size == 0) {
        provider->vms      = calloc(1,sizeof(vm));
        provider->vm_names = calloc(1,sizeof(char*));
        provider->vm_arr_size = 1;
        provider->num_vms     = 1;
        provider->vms[0]      = vm;
        provider->vm_names[0] = name ? strdup(name) : NULL;
        return 0;
    }
    if(provider->num_vms == provider->vm_arr_size) {
        poesie_vm_id_t id      = provider->vm_arr_size;
        size_t new_size_vms    = provider->vm_arr_size * 2 * sizeof(vm);
        size_t new_size_names  = provider->vm_arr_size * 2 * sizeof(char*);
        provider->vms          = realloc(provider->vms, new_size_vms);
        provider->vm_names     = realloc(provider->vm_names, new_size_names);
        memset(provider->vms + provider->vm_arr_size, 0, provider->vm_arr_size * sizeof(vm));
        memset(provider->vm_names + provider->vm_arr_size, 0, provider->vm_arr_size * sizeof(char*));
        provider->vm_arr_size *= 2;
        provider->vms[id]      = vm;
        provider->vm_names[id] = name ? strdup(name) : NULL;
        provider->num_vms     += 1;
        return id;
    } else {
        poesie_vm_id_t id;
        for(id = 0; id < (poesie_vm_id_t)provider->vm_arr_size; id++) {
            if(provider->vms[id] == NULL) {
                provider->vms[id]      = vm;
                provider->vm_names[id] = strdup(name);
                provider->num_vms     += 1;
                return id;
            }
        }
    }
    return POESIE_VM_ID_INVALID;
}

static int validate_config(margo_instance_id mid, struct json_object* config)
{
    if(!config) return 0;
    // config should be an object
    if(!json_object_is_type(config, json_type_object)) {
        margo_error(mid, "[poesie] Invalid type for provider config (object expected)");
        return -1;
    }
    // config["vms"]
    struct json_object* vms = json_object_object_get(config, "vms");
    if(!vms) return 0;
    // vms should be associated with an object
    if(!json_object_is_type(vms, json_type_object)) {
        margo_error(mid, "[poesie] Invalid type for \"vms\" in config (object expected)");
        return -1;
    }
    // check all the entries in vms
    json_object_object_foreach(vms, vm_name, vm_config) {
        if(!json_object_is_type(vm_config, json_type_object)) {
            margo_error(mid,
                "[poesie] Invalid type for \"%s\" in config (object expected)",
                vm_name);
            return -1;
        }
        // make sure that vm_config has a language entry
        struct json_object* vm_lang = json_object_object_get(vm_config, "language");
        if(!vm_lang) {
            margo_error(mid,
                "[poesie] No language specified for VM \"%s\" in config",
                vm_name);
            return -1;
        }
        if(!json_object_is_type(vm_lang, json_type_string)) {
            margo_error(mid,
                "[poesie] Invalid type for \"language\" in config (string expected)");
            return -1;
        }
        const char* lang = json_object_get_string(vm_lang);
        if(strcmp(lang, "lua") != 0 && strcmp(lang, "python") != 0) {
            margo_error(mid,
                "[poesie] Invalid language \"%s\" for VM \"%s\"",
                lang, vm_name);
            return -1;
        }
        // check if the vm_config has a preamble string entry
        struct json_object* preamble = json_object_object_get(vm_config, "preamble");
        if(preamble) {
            if(!json_object_is_type(preamble, json_type_string)) {
                margo_error(mid,
                    "[poesie] Invalid preamble type for VM \"%s\" (string expected)",
                    vm_name);
                return -1;
            }
        }
    }
    return 0;
}

static int create_vms_from_config(poesie_provider_t provider, struct json_object* config)
{
    if(!config) return 0;
    struct json_object* vms = json_object_object_get(config, "vms");
    if(!vms) return 0;
    json_object_object_foreach(vms, vm_name, vm_config) {
        // check language
        const char* vm_lang = json_object_get_string(
            json_object_object_get(vm_config, "language"));
        poesie_lang_t lang = POESIE_LANG_DEFAULT;
        if(strcmp(vm_lang, "python") == 0) lang = POESIE_LANG_PYTHON;
        if(strcmp(vm_lang, "lua") == 0) lang = POESIE_LANG_LUA;
        poesie_vm_id_t vm_id;
        poesie_provider_add_vm(provider, vm_name, lang, &vm_id);
        // check preamble
        struct json_object* json_preamble = json_object_object_get(vm_config, "preamble");
        if(json_preamble) {
            const char* preamble = json_object_get_string(json_preamble);
            char* output = NULL;
            int ret = poesie_vm_execute(provider->vms[vm_id], preamble, &output);
            if(ret != POESIE_SUCCESS) {
                margo_warning(provider->mid,
                    "[poesie] Error while executing preamble for VM %s: %s",
                    vm_name, output ? output : "(not output)");
            }
            free(output);
            provider->vms[vm_id]->preamble = strdup(preamble);
        }
    }
    return 0;
}
