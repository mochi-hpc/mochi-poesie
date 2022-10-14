/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <margo.h>

#include <poesie-client.h>

int main(int argc, char *argv[])
{
    char cli_addr_prefix[64] = {0};
    char *poesie_svr_addr_str;
    char *vm_name;
    margo_instance_id mid;
    hg_addr_t svr_addr;
    uint16_t provider_id;
    poesie_client_t pcl;
    poesie_provider_handle_t pph;
    hg_return_t hret;
    int ret;

    if(argc != 4)
    {
        fprintf(stderr, "Usage: %s <poesie_server_addr> <provider_id> <vm_name>\n", argv[0]);
        fprintf(stderr, "  Example: %s tcp://127.0.0.1:1234 1 foo\n", argv[0]);
        return(-1);
    }
    poesie_svr_addr_str = argv[1];
    provider_id         = atoi(argv[2]);
    vm_name             = argv[3];

    /* initialize Margo using the transport portion of the server
     * address (i.e., the part before the first : character if present)
     */
    for(unsigned i=0; (i<63 && poesie_svr_addr_str[i] != '\0' && poesie_svr_addr_str[i] != ':'); i++)
        cli_addr_prefix[i] = poesie_svr_addr_str[i];

    /* start margo */
    mid = margo_init(cli_addr_prefix, MARGO_CLIENT_MODE, 0, 0);
    if(mid == MARGO_INSTANCE_NULL)
    {
        fprintf(stderr, "Error: margo_init()\n");
        return(-1);
    }

    ret = poesie_client_init(mid, &pcl);
    if(ret != 0)
    {
        fprintf(stderr, "Error: poesie_client_init()\n");
        margo_finalize(mid);
        return -1;
    }

    /* look up the POESIE server address */
    hret = margo_addr_lookup(mid, poesie_svr_addr_str, &svr_addr);
    if(hret != HG_SUCCESS)
    {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        poesie_client_finalize(pcl);
        margo_finalize(mid);
        return(-1);
    }

    /* create a POESIE provider handle */
    ret = poesie_provider_handle_create(pcl, svr_addr, provider_id, &pph);
    if(ret != 0)
    {
        fprintf(stderr, "Error: poesie_provider_handle_create()\n");
        margo_addr_free(mid, svr_addr);
        poesie_client_finalize(pcl);
        margo_finalize(mid);
        return(-1);
    }

    /* get the id of the vm */
    poesie_vm_id_t vm_id;
    poesie_lang_t  lang;
    ret = poesie_get_vm_info(pph, vm_name, &vm_id, &lang);
    if(ret == 0) {
        printf("Successfuly open VM %s, id is %ld\n", vm_name, vm_id);
    } else {
        fprintf(stderr, "Error: could not open database %s\n", vm_name);
        fprintf(stderr, "       return code is %d\n", ret);
        poesie_provider_handle_release(pph);
        margo_addr_free(mid, svr_addr);
        poesie_client_finalize(pcl);
        margo_finalize(mid);
        return(-1);
    }

    /* executing something */
    const char* pycode = "print(\"Hello World from Python\")";
    const char* luacode = "print(\"Hello World from Lua\"); return \"Bonjour\"";

    const char* code = (lang == POESIE_LANG_PYTHON) ? pycode : luacode;

    char* output = NULL;
    ret = poesie_execute(pph, vm_id, POESIE_LANG_DEFAULT, code, &output);
    if(ret != POESIE_SUCCESS) {
        fprintf(stderr, "Error: poesie_execute() returned %d\n",ret);
        poesie_provider_handle_release(pph);
        margo_addr_free(mid, svr_addr);
        poesie_client_finalize(pcl);
        margo_finalize(mid);
        return -1;
    }

    fprintf(stderr, "RESPONSE from VM %s: %s\n", vm_name, output);

    ret = poesie_free_output(output);
    if(ret != POESIE_SUCCESS) {
        fprintf(stderr, "Error: poesie_free_output() returned %d\n", ret);
        ret = -1;
        goto finish;
    }

    ret = poesie_execute(pph, POESIE_VM_ANONYMOUS, POESIE_LANG_LUA, luacode, &output);
    if(ret != POESIE_SUCCESS) {
        fprintf(stderr, "Error: poesie_execute() (on anonymous VM) returned %d\n", ret);
        poesie_provider_handle_release(pph);
        margo_addr_free(mid, svr_addr);
        poesie_client_finalize(pcl);
        margo_finalize(mid);
        return -1;
    }

    fprintf(stderr, "RESPONSE from anonymous VM: %s\n", output);

    ret = poesie_free_output(output);
    if(ret != POESIE_SUCCESS) {
        fprintf(stderr, "Error: poesie_free_output() returned %d\n", ret);
        ret = -1;
        goto finish;
    }

finish:
    /**** cleanup ****/
    poesie_provider_handle_release(pph);
    margo_addr_free(mid, svr_addr);
    poesie_client_finalize(pcl);
    margo_finalize(mid);
    return(ret);
}

