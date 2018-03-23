/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_COMMON_H 
#define __POESIE_COMMON_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum poesie_lang_t 
{
    POESIE_LANG_DEFAULT = 0, /* Poesie will use the language of the target VM */
    POESIE_LANG_PYTHON,      /* Python VM */
    POESIE_LANG_LUA          /* Lua VM */
} poesie_lang_t;

typedef int64_t poesie_vm_id_t;
#define POESIE_VM_ID_INVALID -1
#define POESIE_VM_ANONYMOUS  -2

#define POESIE_SUCCESS          0 /* Success */
#define POESIE_ERR_ALLOCATION  -1 /* Error when allocating something */
#define POESIE_ERR_INVALID_ARG -2 /* Invalid argument to some call */
#define POESIE_ERR_MERCURY     -3 /* Error from a Mercury or Margo call */
#define POESIE_ERR_LANGUAGE    -4 /* Invalid language used */
#define POESIE_ERR_CODE        -5 /* Code interpretation failed */
#define POESIE_ERR_UNKNOWN_PR  -6 /* Unknown provider */
#define POESIE_ERR_VM_EXISTS   -7 /* VM already exists */
#define POESIE_ERR_NO_VM       -8 /* No VM exists with this name or id */
#define POESIE_ERR_VM_INIT     -9 /* Could not initialize a VM */
#define POESIE_ERR_ARGOBOTS   -10 /* Error on an ABT call */

#if defined(__cplusplus)
}
#endif

#endif 
