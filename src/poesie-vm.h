/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_VM_H
#define __POESIE_VM_H

#include "poesie-common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct poesie_vm* poesie_vm_t;

int poesie_vm_create(const char* name, poesie_lang_t lang, poesie_vm_t* vm);

int poesie_vm_execute(poesie_vm_t vm, const char* code, char** output);

int poesie_vm_destroy(poesie_vm_t vm);

int poesie_vm_get_lang(poesie_vm_t vm, poesie_lang_t* lang);

#ifdef __cplusplus
}
#endif

#endif
