/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_PYTHON_H
#define __POESIE_PYTHON_H

#include <margo.h>
#include "../poesie-vm.h"

int poesie_py_vm_init(poesie_vm_t vm, margo_instance_id mid, const char* vm_name);

#endif
