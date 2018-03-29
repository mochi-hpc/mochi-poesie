/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */

#ifndef __POESIE_SERVER_H
#define __POESIE_SERVER_H

#include <margo.h>
#include <poesie-common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POESIE_ABT_POOL_DEFAULT ABT_POOL_NULL
#define POESIE_PROVIDER_ID_DEFAULT 0
#define POESIE_PROVIDER_IGNORE NULL
#define POESIE_PROVIDER_NULL   NULL

typedef struct poesie_provider* poesie_provider_t;

/**
 * @brief Creates a new provider.
 *
 * @param[in] mid Margo instance
 * @param[in] mplex_id multiplex id
 * @param[in] pool Argobots pool
 * @param[out] provider provider handle
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        ABT_pool pool,
        poesie_provider_t* provider);

/**
 * Makes the provider start managing a vm.
 *
 * @param[in] provider provider
 * @param[in] vm_name name of the vm
 * @param[in] vm_lang language of the vm
 * @param[out] vm_id resulting id identifying the vm
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_add_vm(
        poesie_provider_t provider,
        const char* vm_name,
        poesie_lang_t vm_lang,
        poesie_vm_id_t* vm_id);

/**
 * Makes the provider stop managing a vm and deletes the vm. 
 *
 * @param provider provider
 * @param vm_id id of the vm to remove
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_remove_vm(
        poesie_provider_t provider,
        poesie_vm_id_t vm_id);

/**
 * Removes all the vms associated with a provider.
 *
 * @param provider provider
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_remove_all_vms(
        poesie_provider_t provider);

/**
 * Returns the number of vms that this provider manages.
 *
 * @param provider provider
 * @param num_vms resulting number of vms
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_count_vms(
        poesie_provider_t provider,
        uint64_t* num_vms);

/**
 * List the vm ids of the vms managed by this provider.
 * The vms array must be pre-allocated with at least enough
 * space to hold all the ids (use poesie_provider_count_vms
 * to know how many vms are managed).
 *
 * @param[in] provider provider
 * @param[out] vms resulting vm ids
 *
 * @return POESIE_SUCCESS or error code defined in poesie-common.h
 */
int poesie_provider_list_vms(
        poesie_provider_t provider,
        poesie_vm_id_t* vms);

#ifdef __cplusplus
}
#endif

#endif /* __BAKE_SERVER_H */
