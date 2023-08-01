/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <stdatomic.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <margo.h>
#include "poesie-javascript.h"
#include "../poesie-vm-impl.h"

#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>

typedef struct javascript_vm {
    char*             name;
    JSRuntime*        js_rt;
    JSContext*        js_ctx;
    ABT_mutex         mutex;
}* javascript_vm_t;

static int poesie_js_execute(void* impl, const char* code, char** output);
static int poesie_js_finalize(void* impl);
static struct json_object* poesie_js_get_config(void* impl);

/* also used to initialize the worker context */
static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    JS_AddIntrinsicBigFloat(ctx);
    JS_AddIntrinsicBigDecimal(ctx);
    JS_AddIntrinsicOperators(ctx);
    JS_EnableBignumExt(ctx, true);
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    const char *str = "import * as std from 'std';\n"
                      "import * as os from 'os';\n"
                      "globalThis.std = std;\n"
                      "globalThis.os = os;\n";

    JSValue val = JS_Eval(ctx, str, strlen(str), "<init>",
                          JS_EVAL_TYPE_MODULE|JS_EVAL_FLAG_COMPILE_ONLY);
    if (!JS_IsException(val)) {
        js_module_set_import_meta(ctx, val, 1, 1);
        val = JS_EvalFunction(ctx, val);
    } else {
        js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, val);
    return ctx;
}

int poesie_js_vm_init(poesie_vm_t vm, margo_instance_id mid, const char* name)
{
    javascript_vm_t pvm = (javascript_vm_t)calloc(1,sizeof(*pvm));
    if(!pvm)
        return POESIE_ERR_ALLOCATION;

    if(ABT_SUCCESS != ABT_mutex_create(&(pvm->mutex))) {
        free(pvm);
        return POESIE_ERR_ARGOBOTS;
    }

    /* initialize quickjs objects */
    pvm->js_rt = JS_NewRuntime();
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(pvm->js_rt);
    pvm->js_ctx = JS_NewCustomContext(pvm->js_rt);
    JS_SetModuleLoaderFunc(pvm->js_rt, NULL, js_module_loader, NULL);
    pvm->name = strdup(name);

    vm->lang = POESIE_LANG_JAVASCRIPT;
    vm->impl = (void*)pvm;
    vm->execute    = &poesie_js_execute;
    vm->finalize   = &poesie_js_finalize;
    vm->get_config = &poesie_js_get_config;

    return 0;
}

static int poesie_js_execute(void* impl, const char* code, char** output)
{
    *output = NULL;
    javascript_vm_t pvm = (javascript_vm_t)impl;
    ABT_mutex_lock(pvm->mutex);
    JSContext* ctx = pvm->js_ctx;
    JSValue val = JS_Eval(ctx, code, strlen(code), pvm->name, 0);
    if(JS_IsException(val)) {
        JSValue exception_val = JS_GetException(ctx);
        const char* str = JS_ToCString(ctx, exception_val);
        if(str) *output = strdup(str);
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, exception_val);
    } else {
        const char* str = JS_ToCString(ctx, val);
        if(str) *output = strdup(str);
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, val);
    ABT_mutex_unlock(pvm->mutex);
    return 0;
}

static int poesie_js_finalize(void* impl)
{
    if(!impl) return 0;
    javascript_vm_t pvm = (javascript_vm_t)impl;
    js_std_free_handlers(pvm->js_rt);
    JS_FreeContext(pvm->js_ctx);
    JS_FreeRuntime(pvm->js_rt);
    return 0;
}

static struct json_object* poesie_js_get_config(void* impl)
{
    (void)impl;
    struct json_object* config = json_object_new_object();
    json_object_object_add(config, "language", json_object_new_string("javascript"));
    return config;
}
