/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "poesie-client.h"

/* client program that will shut down a BAKE server. */

int main(int argc, char **argv) 
{
    int i;
    char cli_addr_prefix[64] = {0};
    char *svr_addr_str;
    hg_addr_t svr_addr;
    margo_instance_id mid;
    hg_return_t hret;
    int ret;
 
    if(argc != 2)
    {
        fprintf(stderr, "Usage: poesie-shutdown <server addr to stop>\n");
        fprintf(stderr, "  Example: ./poesie-shutdown bmi+tcp://localhost:1234\n");
        return(-1);
    }
    svr_addr_str = argv[1];

    /* initialize Margo using the transport portion of the server
     * address (i.e., the part before the first : character if present)
     */
    for(i=0; (i<63 && svr_addr_str[i] != '\0' && svr_addr_str[i] != ':'); i++)
        cli_addr_prefix[i] = svr_addr_str[i];

    mid = margo_init(cli_addr_prefix, MARGO_CLIENT_MODE, 0, -1);
    if(mid == MARGO_INSTANCE_NULL)
    {
        fprintf(stderr, "Error: margo_init()\n");
        return -1;
    }

    hret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if(hret != HG_SUCCESS)
    {
        fprintf(stderr, "Error: margo_addr_lookup()\n");
        margo_finalize(mid);
        return(-1);
    }

    poesie_client_t kvc;
    ret = poesie_client_init(mid, &kvc);

    if(ret < 0)
    {
        fprintf(stderr, "Error: poesie_client_init()\n");
        margo_addr_free(mid, svr_addr);
        margo_finalize(mid);
        return(-1);
    }

    /* shutdown server */
    poesie_shutdown_service(kvc, svr_addr);

    margo_addr_free(mid, svr_addr);

    poesie_client_finalize(kvc);

    margo_finalize(mid);

    return(0);
}

