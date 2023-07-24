/*
 * (C) 2023 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_JAVASCRIPT_H
#define __POESIE_JAVASCRIPT_H

#include <margo.h>
#include "../poesie-vm.h"

int poesie_js_vm_init(poesie_vm_t vm, margo_instance_id mid, const char* vm_name);

#endif
