/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <bedrock/module.h>
#include "poesie/server.h"
#include "poesie/client.h"
#include <string.h>

static int poesie_register_provider(
        bedrock_args_t args,
        bedrock_module_provider_t* provider)
{
    margo_instance_id mid = bedrock_args_get_margo_instance(args);
    uint16_t provider_id  = bedrock_args_get_provider_id(args);

    struct poesie_provider_args poesie_args = { 0 };
    poesie_args.config = bedrock_args_get_config(args);
    poesie_args.pool   = bedrock_args_get_pool(args);

    return poesie_provider_register(mid, provider_id, &poesie_args,
                                   (poesie_provider_t*)provider);
}

static int poesie_deregister_provider(
        bedrock_module_provider_t provider)
{
    return poesie_provider_destroy((poesie_provider_t)provider);
}

static char* poesie_get_provider_config(
        bedrock_module_provider_t provider) {
    return poesie_provider_get_config(provider);
}

static int poesie_init_client(
        bedrock_args_t args,
        bedrock_module_client_t* client)
{
    margo_instance_id mid = bedrock_args_get_margo_instance(args);
    return poesie_client_init(mid, (poesie_client_t*)client);
}

static int poesie_finalize_client(
        bedrock_module_client_t client)
{
    return poesie_client_finalize((poesie_client_t)client);
}

static char* poesie_get_client_config(
        bedrock_module_client_t client) {
    (void)client;
    // TODO
    return strdup("{}");
}

static int poesie_create_provider_handle(
        bedrock_module_client_t client,
        hg_addr_t address,
        uint16_t provider_id,
        bedrock_module_provider_handle_t* ph)
{
    return poesie_provider_handle_create(
        (poesie_client_t)client, address,
        provider_id, (poesie_provider_handle_t*)ph);
}

static int poesie_destroy_provider_handle(
        bedrock_module_provider_handle_t ph)
{
    return poesie_provider_handle_release(
        (poesie_provider_handle_t)ph);
}

static struct bedrock_module poesie = {
    .register_provider       = poesie_register_provider,
    .deregister_provider     = poesie_deregister_provider,
    .get_provider_config     = poesie_get_provider_config,
    .init_client             = poesie_init_client,
    .finalize_client         = poesie_finalize_client,
    .get_client_config       = poesie_get_client_config,
    .create_provider_handle  = poesie_create_provider_handle,
    .destroy_provider_handle = poesie_destroy_provider_handle,
    .provider_dependencies   = NULL,
    .client_dependencies     = NULL
};

BEDROCK_REGISTER_MODULE(poesie, poesie)
