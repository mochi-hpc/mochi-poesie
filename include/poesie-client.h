/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_CLIENT_H
#define __POESIE_CLIENT_H

#include <margo.h>
#include <stdint.h>
#include <poesie-common.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct poesie_client* poesie_client_t;
#define POESIE_CLIENT_NULL ((poesie_client_t)NULL)

typedef struct poesie_provider_handle* poesie_provider_handle_t;
#define POESIE_PROVIDER_HANDLE_NULL ((poesie_provider_handle_t)NULL)

/**
 * @brief Initialize a POESIE client.
 *
 * @param mid Margo instance
 * @param client Resulting POESIE client
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_client_init(margo_instance_id mid, poesie_client_t* client);

/**
 * @brief Finalize a POESIE client.
 *
 * @param client POESIE client to finalize
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_client_finalize(poesie_client_t client);

/**
 * @brief Creates a provider handle for a POESIE provider.
 *
 * @param client POESIE client
 * @param addr Mercury address of the POESIE provider
 * @param mplex_id Multiplex id of the POESIE provider
 * @param handle Resulting provider handle
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_provider_handle_create(
        poesie_client_t client,
        hg_addr_t addr,
        uint8_t mplex_id,
        poesie_provider_handle_t* handle);

/**
 * @brief Increases the reference counter of the provider handle.
 *
 * @param handle POESIE provider handle
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_provider_handle_ref_incr(
        poesie_provider_handle_t handle);

/**
 * @brief Decreases the reference counter of the provider handle,
 * destroying the provider handle when the counter reaches 0.
 *
 * @param handle POESIE provider handle
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_provider_handle_release(poesie_provider_handle_t handle);

/**
 * @brief Retrieve the VM id and VM language associated with a VM
 * name in a given provider.
 *
 * @param handle POESIE provider handle
 * @param vm_name Name of the target VM
 * @param vm_id Resulting VM ID
 * @param language Language of the VM
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_get_vm_info(
        poesie_provider_handle_t handle, 
        const char* vm_name,
        poesie_vm_id_t* vm_id,
        poesie_lang_t* language);

/**
 * @brief Creates a VM on a target provider.
 *
 * @param handle Provider handle
 * @param vm_name Name of the VM to create
 * @param language Language of the VM
 * @param vm_id Resulting VM id
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_create_vm(
        poesie_provider_handle_t handle,
        const char* vm_name,
        poesie_lang_t language,
        poesie_vm_id_t* vm_id);

/**
 * @brief Deletes a VM created by a client.
 *
 * @param handle Provider handle
 * @param vm_id Resulting VM id
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_delete_vm(
        poesie_provider_handle_t handle,
        poesie_vm_id_t vm_id);

/**
 * @brief Executes some code in the target VM.
 * If vm_id is POESIE_VM_ANONYMOUS then a new VM
 * will be created for the purpose of only executing
 * the code. The language of this VM shoud be different
 * from POESIE_LANG_DEFAULT. If the target VM is an existing
 * VM, POESIE_LANG_DEFAULT can be used to say "whatever
 * language this VM is using". Upon success, this function 
 * will allocate the output pointer. This pointer should be
 * freed using poesie_free_output.
 *
 * @param handle POESIE provider handle
 * @param vm_id ID of the VM to send the code to
 * @param lang Language of the VM
 * @param code Code to send
 * @param output Output of the code
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_execute(
        poesie_provider_handle_t handle,
        poesie_vm_id_t vm_id,
        poesie_lang_t lang,
        const char* code,
        char** output);

/**
 * @brief Free the output of a call to poesie_execute.
 *
 * @param output Pointer to the output to free
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_free_output(char* output);

/**
 * @brief Request the Margo instance at the target address to shut down.
 *
 * @param client POESIE client
 * @param addr Mercury address of the Margo instance to shutdown
 *
 * @return POESIE_SUCCESS or other error code from poesie-common.h
 */
int poesie_shutdown_service(poesie_client_t client, hg_addr_t addr);

#if defined(__cplusplus)
}
#endif

#endif
