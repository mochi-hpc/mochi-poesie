/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include "config.h"
#include "poesie-vm.h"
#include "poesie-vm-impl.h"
#ifdef POESIE_HAS_PYTHON
#include "lang/poesie-python.h"
#endif
#ifdef POESIE_HAS_LUA
#include "lang/poesie-lua.h"
#endif
#ifdef POESIE_HAS_JAVASCRIPT
#include "lang/poesie-javascript.h"
#endif

int poesie_vm_execute(poesie_vm_t vm, const char* code, char** output)
{
    if(vm) {
        return vm->execute(vm->impl,code,output);
    } else {
        return -1;
    }
}

int poesie_vm_create(const char* name, margo_instance_id mid, poesie_lang_t lang, poesie_vm_t* vm)
{
    poesie_vm_t tmp_vm = NULL;
    switch(lang) {
        case POESIE_LANG_PYTHON:
#ifdef POESIE_HAS_PYTHON
            tmp_vm = calloc(1, sizeof(*tmp_vm));
            tmp_vm->mid = mid;
            poesie_py_vm_init(tmp_vm, mid, name);
#endif
            break;
        case POESIE_LANG_LUA:
#ifdef POESIE_HAS_LUA
            tmp_vm = calloc(1, sizeof(*tmp_vm));
            tmp_vm->mid = mid;
            poesie_lua_vm_init(tmp_vm, mid, name);
#endif
            break;
        case POESIE_LANG_JAVASCRIPT:
#ifdef POESIE_HAS_JAVASCRIPT
            tmp_vm = calloc(1, sizeof(*tmp_vm));
            tmp_vm->mid = mid;
            poesie_js_vm_init(tmp_vm, mid, name);
#endif
            break;
        default:
            break;
    }
    if(tmp_vm) {
        *vm = tmp_vm;
        return 0;
    } else {
        return -1;
    }
}

int poesie_vm_destroy(poesie_vm_t vm)
{
    int ret;
    if(vm) {
        ret = vm->finalize(vm->impl);
        free(vm->preamble);
        free(vm);
        return ret;
    } else {
        return -1;
    }
}

int poesie_vm_get_lang(poesie_vm_t vm, poesie_lang_t* lang)
{
    if(vm) {
        *lang = vm->lang;
        return POESIE_SUCCESS;
    } else {
        return POESIE_ERR_INVALID_ARG;
    }
}

struct json_object* poesie_vm_get_config(poesie_vm_t vm)
{
    if(!vm) return NULL;
    struct json_object* config = vm->get_config(vm->impl);
    if(vm->preamble)
        json_object_object_add(config, "preamble",
            json_object_new_string(vm->preamble));
    return config;
}
