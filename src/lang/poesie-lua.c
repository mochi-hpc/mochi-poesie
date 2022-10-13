/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include <margo.h>
#include "poesie-lua.h"
#include "../poesie-vm-impl.h"

typedef struct lua_vm {
    lua_State *L;
    ABT_mutex mutex;
}* lua_vm_t;

static int poesie_lua_execute(void* impl, const char* code, char** output);
static int poesie_lua_finalize(void* impl);

int poesie_lua_vm_init(poesie_vm_t vm, const char* name)
{
    lua_vm_t lvm = (lua_vm_t)calloc(1,sizeof(*vm));
    if(!lvm) return POESIE_ERR_ALLOCATION;

    int ret = ABT_mutex_create(&(lvm->mutex));
    if(ret != ABT_SUCCESS) {
        free(lvm);
        return POESIE_ERR_ARGOBOTS;
    }

    lvm->L = luaL_newstate();
    if(!(lvm->L)) {
        ABT_mutex_free(&(lvm->mutex));
        free(lvm);
        return POESIE_ERR_VM_INIT;
    }
    luaL_openlibs(lvm->L);

    vm->lang = POESIE_LANG_LUA;
    vm->impl = (void*)lvm;
    vm->execute  = &poesie_lua_execute;
    vm->finalize = &poesie_lua_finalize;

    return 0;
}

static int poesie_lua_execute(void* impl, const char* code, char** output)
{
    lua_vm_t lvm = (lua_vm_t)impl;
    if(!lvm) return POESIE_ERR_INVALID_ARG;
    if(ABT_SUCCESS != ABT_mutex_lock(lvm->mutex))
        return POESIE_ERR_ARGOBOTS;
    luaL_dostring(lvm->L, code);
    const char* str = lua_tostring(lvm->L, -1);
    *output = strdup(str);
    lua_pop(lvm->L, 1);
    ABT_mutex_unlock(lvm->mutex);
    return 0;
}

static int poesie_lua_finalize(void* impl)
{
    lua_vm_t lvm = (lua_vm_t)impl;
    if(!lvm) return POESIE_ERR_INVALID_ARG;
    lua_close(lvm->L);
    ABT_mutex_free(&(lvm->mutex));
    free(lvm);
    return 0;
}
