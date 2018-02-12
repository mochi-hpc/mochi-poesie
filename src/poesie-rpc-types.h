#ifndef __POESIE_RPC_TYPE_H
#define __POESIE_RPC_TYPE_H

#include <stdint.h>

#include <mercury.h>
#include <mercury_macros.h>
#include <mercury_proc_string.h>

#include <margo.h>

#include "poesie-common.h"

// ------------- GET_VM_INFO ------------- //
MERCURY_GEN_PROC(get_vm_info_in_t, 
        ((hg_string_t)(name)))
MERCURY_GEN_PROC(get_vm_info_out_t, ((int64_t)(vm_id))\
        ((int32_t)(lang))\
        ((int32_t)(ret)))

// ------------- EXECUTE ------------- //
MERCURY_GEN_PROC(execute_in_t, ((int64_t)(vm_id))\
        ((int32_t)(lang))\
        ((hg_string_t)(code)))
MERCURY_GEN_PROC(execute_out_t, ((int32_t)(ret))\
        ((hg_string_t)(output)))

// ------------- CREATE_VM ------------- //
MERCURY_GEN_PROC(create_vm_in_t, ((hg_string_t)(name))\
        ((int32_t)(lang)))
MERCURY_GEN_PROC(create_vm_out_t, ((int32_t)(ret))\
        ((int64_t)(vm_id)))

// ------------- DELETE_VM ------------- //
MERCURY_GEN_PROC(delete_vm_in_t, ((int64_t)(vm_id)))
MERCURY_GEN_PROC(delete_vm_out_t, ((int32_t)(ret)))
#endif
