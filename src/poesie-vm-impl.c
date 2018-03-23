/*
 * (C) 2018 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#include <stdlib.h>
#include "src/poesie-config.h"
#include "src/poesie-vm.h"
#include "src/poesie-vm-impl.h"
#ifdef USE_PYTHON
#include "src/lang/poesie-python.h"
#endif
#ifdef USE_LUA
#include "src/lang/poesie-lua.h"
#endif

int poesie_vm_execute(poesie_vm_t vm, const char* code, char** output)
{
    if(vm) {
        return vm->execute(vm->impl,code,output);
    } else {
        return -1;
    }
}

int poesie_vm_create(const char* name, poesie_lang_t lang, poesie_vm_t* vm)
{
    poesie_vm_t tmp_vm = NULL;
    switch(lang) {
        case POESIE_LANG_PYTHON:
#ifdef USE_PYTHON
            tmp_vm = calloc(1, sizeof(*tmp_vm));
            poesie_py_vm_init(tmp_vm, name);
#endif
            break;
        case POESIE_LANG_LUA:
#ifdef USE_LUA
            tmp_vm = calloc(1, sizeof(*tmp_vm));
            poesie_lua_vm_init(tmp_vm, name);
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
