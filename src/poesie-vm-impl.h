/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_VM_IMPL_H
#define __POESIE_VM_IMPL_H

#include <margo.h>
#include <json-c/json.h>
#include "poesie/common.h"

typedef int (*execute_fn)(void*, const char*, char**);
typedef int (*finalize_fn)(void*);
typedef struct json_object* (*get_config_fn)(void*);

struct poesie_vm {
    margo_instance_id mid;
    poesie_lang_t     lang;
    char*             preamble;
    void*             impl;
    execute_fn        execute;
    finalize_fn       finalize;
    get_config_fn     get_config;
};

#endif
