/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <margo.h>
#include <poesie-server.h>

struct options
{
    char*           listen_addr_str;
    unsigned        num_vms;
    char**          vm_names;
    poesie_lang_t*  vm_langs;
    char*           host_file;
};

static void usage(int argc, char **argv)
{
    fprintf(stderr, "Usage: poesie-server-daemon [OPTIONS] <listen_addr> <vm name 1>{:py|:lua} <vm name 2>{:py|:lua} ...\n");
    fprintf(stderr, "       listen_addr is the Mercury address to listen on\n");
    fprintf(stderr, "       vm name X are the names of the VMs\n");
    fprintf(stderr, "       [-f filename] to write the server address to a file\n");
    fprintf(stderr, "Example: ./poesie-server-daemon tcp://localhost:1234 foo:py bar:lua\n");
    return;
}

static poesie_lang_t parse_vm_lang(char* vm_fullname) {
    char* column = strstr(vm_fullname, ":");
    if(column == NULL) {
        return POESIE_LANG_DEFAULT;
    }
    *column = '\0';
    char* vm_lang = column + 1;
    if(strcmp(vm_lang, "py") == 0) {
        return POESIE_LANG_PYTHON;
    } else if(strcmp(vm_lang, "lua") == 0) {
        return POESIE_LANG_LUA;
    }
    fprintf(stderr, "Unknown database type \"%s\"\n", vm_lang);
    exit(-1);
}

static void parse_args(int argc, char **argv, struct options *opts)
{
    int opt;

    memset(opts, 0, sizeof(*opts));

    /* get options */
    while((opt = getopt(argc, argv, "f:")) != -1)
    {
        switch(opt)
        {
            case 'f':
                opts->host_file = optarg;
                break;
            default:
                usage(argc, argv);
                exit(EXIT_FAILURE);
        }
    }

    /* get required arguments after options */
    if((argc - optind) < 1)
    {
        usage(argc, argv);
        exit(EXIT_FAILURE);
    }
    opts->num_vms = argc - optind - 1;
    opts->listen_addr_str = argv[optind++];
    if(opts->num_vms) {
        opts->vm_names = calloc(opts->num_vms, sizeof(char*));
        opts->vm_langs = calloc(opts->num_vms, sizeof(poesie_lang_t));
    } else {
        opts->vm_names = NULL;
        opts->vm_langs = NULL;
    }
    int i;
    for(i=0; i < opts->num_vms; i++) {
        opts->vm_names[i] = argv[optind++];
        opts->vm_langs[i] = parse_vm_lang(opts->vm_names[i]);
    }

    return;
}

int main(int argc, char **argv) 
{
    struct options opts;
    margo_instance_id mid;
    int ret;

    parse_args(argc, argv, &opts);

    /* start margo */
    /* use the main xstream for driving progress and executing rpc handlers */
    mid = margo_init(opts.listen_addr_str, MARGO_SERVER_MODE, 0, -1);
    if(mid == MARGO_INSTANCE_NULL)
    {
        fprintf(stderr, "Error: margo_init()\n");
        return(-1);
    }

    margo_enable_remote_shutdown(mid);

    if(opts.host_file)
    {
        /* write the server address to file if requested */
        FILE *fp;
        hg_addr_t self_addr;
        char self_addr_str[128];
        hg_size_t self_addr_str_sz = 128;
        hg_return_t hret;

        /* figure out what address this server is listening on */
        hret = margo_addr_self(mid, &self_addr);
        if(hret != HG_SUCCESS)
        {
            fprintf(stderr, "Error: margo_addr_self()\n");
            margo_finalize(mid);
            return(-1);
        }
        hret = margo_addr_to_string(mid, self_addr_str, &self_addr_str_sz, self_addr);
        if(hret != HG_SUCCESS)
        {
            fprintf(stderr, "Error: margo_addr_to_string()\n");
            margo_addr_free(mid, self_addr);
            margo_finalize(mid);
            return(-1);
        }
        margo_addr_free(mid, self_addr);

        fp = fopen(opts.host_file, "w");
        if(!fp)
        {
            perror("fopen");
            margo_finalize(mid);
            return(-1);
        }

        fprintf(fp, "%s", self_addr_str);
        fclose(fp);
    }

    int i;
    poesie_provider_t provider;
    ret = poesie_provider_register(mid, 1,
            POESIE_ABT_POOL_DEFAULT,
            &provider);

    if(ret != 0)
    {
        fprintf(stderr, "Error: poesie_provider_register()\n");
        margo_finalize(mid);                                    
        return(-1);
    }

    for(i=0; i < opts.num_vms; i++) {
        poesie_vm_id_t vm_id;
        ret = poesie_provider_add_vm(provider, 
                opts.vm_names[i], opts.vm_langs[i],
                &vm_id);

        if(ret != 0)
        {
            fprintf(stderr, "Error: poesie_provider_add_vm()\n");
            margo_finalize(mid);                                    
            return(-1);
        }
        printf("Managing VM \"%s\" at multiplex id %d\n", opts.vm_names[i] , 1);
    }

    margo_wait_for_finalize(mid);

    free(opts.vm_names);

    return(0);
}
