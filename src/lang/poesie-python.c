/*
 * (C) 2018 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <Python.h>
#include <stdatomic.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <margo.h>
#include "poesie-python.h"
#include "../poesie-vm-impl.h"

static atomic_int num_open_vms = 0;

typedef struct python_vm {
#ifdef ALLOW_SUBINT
    PyThreadState* subint;
#endif
    PyObject* main;
    PyObject* globalDictionary;
    PyObject* localDictionary;
    ABT_mutex mutex;
}* python_vm_t;

static int poesie_py_execute(void* impl, const char* code, char** output);
static int poesie_py_finalize(void* impl);

static int init_python();
static int finalize_python();

int poesie_py_vm_init(poesie_vm_t vm, margo_instance_id mid, const char* name)
{
    int ret;
#ifndef ALLOW_SUBINT
    if(atomic_load(&num_open_vms) == 1) {
        margo_error(mid, "[poesie] Multiple Python VMs not yet supported");
        return POESIE_ERR_VM_INIT;
    }
#endif
    atomic_fetch_add(&num_open_vms, 1);
    ret = init_python();
    if(ret != 0)
        return POESIE_ERR_VM_INIT;

    python_vm_t pvm = (python_vm_t)calloc(1,sizeof(*pvm));
    if(!pvm)
        return POESIE_ERR_ALLOCATION;

    if(ABT_SUCCESS != ABT_mutex_create(&(pvm->mutex))) {
        free(pvm);
        return POESIE_ERR_ARGOBOTS;
    }

#ifdef ALLOW_SUBINT
    PyThreadState* main_state = PyThreadState_Get();
    /* create sub-interpreter */
    pvm->subint = Py_NewInterpreter();
    PyThreadState_Swap(main_state);
#endif
    /* acquire lock */
    PyGILState_STATE gstate = PyGILState_Ensure();
#ifdef ALLOW_SUBINT
    /* create new thread state */
    PyThreadState* new_ts = PyThreadState_New(pvm->subint->interp);
    /* swap to new thread state */
    PyThreadState* old_ts = PyThreadState_Swap(new_ts);
#endif
    /* initialize the context */
    pvm->main             = PyImport_AddModule("__main__");
    pvm->globalDictionary = PyModule_GetDict(pvm->main);
    pvm->localDictionary  = PyDict_New();

    if(name) {
        PyObject* namePyObj = PyUnicode_FromString(name);
        PyDict_SetItemString(pvm->localDictionary, "__name__", namePyObj);
        Py_DECREF(namePyObj);
    }

    PyObject* pyMid = PyCapsule_New((void*)mid, "margo_instance_id", NULL);
    PyDict_SetItemString(pvm->localDictionary, "__mid__", pyMid);
    Py_DECREF(pyMid);

#ifdef ALLOW_SUBINT
    /* swap to back to old thread state */
    PyThreadState_Swap(old_ts);
    /* cleanup new thread state */
    PyThreadState_Clear(new_ts);
    PyThreadState_Delete(new_ts);
#endif
    /* release lock */
    PyGILState_Release(gstate);

    vm->lang = POESIE_LANG_PYTHON;
    vm->impl = (void*)pvm;
    vm->execute  = &poesie_py_execute;
    vm->finalize = &poesie_py_finalize;

    return 0;
}

static int poesie_py_execute(void* impl, const char* code, char** output)
{
    *output = NULL;
    PyObject *pExcType , *pExcValue , *pExcTraceback;
    python_vm_t pvm = (python_vm_t)impl;
    ABT_mutex_lock(pvm->mutex);
    /* acquire lock */
    PyGILState_STATE gstate = PyGILState_Ensure();
#ifdef ALLOW_SUBINT
    /* create new thread state */
    PyThreadState* new_ts = PyThreadState_New(pvm->subint->interp);
    /* swap to new thread state */
    PyThreadState* old_ts = PyThreadState_Swap(new_ts);
#endif
    // reset __poesie_output__
    PyObject* ex = NULL;
    PyDict_SetItemString(pvm->globalDictionary, "__poesie_output__", Py_None);
    // execute user code
    ex = PyErr_Occurred();
    if(ex) goto pyerror;
    PyRun_String(code, Py_file_input,
                 pvm->globalDictionary, pvm->localDictionary);
    ex = PyErr_Occurred();
    if(ex) goto pyerror;
    // get __poesie_output__
    PyObject* pyOutput = PyRun_String(
        "__poesie_output__", Py_eval_input,
        pvm->globalDictionary, pvm->localDictionary);
    ex = PyErr_Occurred();
    if(ex) goto pyerror;

    PyObject* resultStr = PyObject_Str(pyOutput);
    PyObject* pBytes = PyUnicode_AsEncodedString(resultStr, "UTF-8", "strict");
    *output = PyBytes_AS_STRING(pBytes);
    *output = strdup(*output);
    Py_DECREF(pBytes);
    Py_DECREF(pyOutput);
    Py_DECREF(resultStr);

finish:
#ifdef ALLOW_SUBINT
    /* swap to back to old thread state */
    PyThreadState_Swap(old_ts);
    /* cleanup new thread state */
    PyThreadState_Clear(new_ts);
    PyThreadState_Delete(new_ts);
#endif
    /* release lock */
    PyGILState_Release(gstate);
    ABT_mutex_unlock(pvm->mutex);
    return 0;

pyerror:
    PyErr_Fetch( &pExcType , &pExcValue , &pExcTraceback );
    if(pExcValue != NULL) {
        PyObject* pRepr = PyObject_Str(pExcValue);
        PyObject* pBytes = PyUnicode_AsEncodedString(pRepr, "UTF-8", "strict");
        *output = PyBytes_AS_STRING(pBytes);
        *output = strdup(*output);
        Py_DECREF(pBytes);
        Py_DECREF(pExcValue);
        Py_DECREF(pRepr);
    }
    if(pExcType != NULL)
        Py_DECREF(pExcType);
    if(pExcTraceback != NULL)
        Py_DECREF(pExcTraceback);
    Py_DECREF(ex);
    PyErr_Clear();
    goto finish;
}

static int poesie_py_finalize(void* impl)
{
    if(!impl) return 0;
    python_vm_t pvm = (python_vm_t)impl;
    /* acquire lock */
    PyGILState_STATE gstate = PyGILState_Ensure();
#ifdef ALLOW_SUBINT
    /* create new thread state */
    PyThreadState* new_ts = PyThreadState_New(pvm->subint->interp);
    /* swap to new thread state */
    PyThreadState* old_ts = PyThreadState_Swap(new_ts);
#endif
    /* decrement references */
    Py_DECREF(pvm->localDictionary);
    Py_DECREF(pvm->globalDictionary);
    Py_DECREF(pvm->main);
#ifdef ALLOW_SUBINT
    /* swap to back to old thread state */
    PyThreadState_Swap(old_ts);
    /* cleanup new thread state */
    PyThreadState_Clear(new_ts);
    PyThreadState_Delete(new_ts);
#endif
    /* release lock */
    PyGILState_Release(gstate);
#ifdef ALLOW_SUBINT
    /* swap to thread state of subint */
    old_ts = PyThreadState_Swap(pvm->subint);
    /* end subinterpreter */
    Py_EndInterpreter(pvm->subint);
    PyThreadState_Swap(old_ts);
#endif
    ABT_mutex_free(&pvm->mutex);
    free(pvm);
    if(atomic_fetch_sub(&num_open_vms, 1) == 1) {
        finalize_python();
    }
    return 0;
}

static int init_python() {
    if(Py_IsInitialized())
        return 0;
    Py_InitializeEx(0);
    return 0;
}

static int finalize_python() {
    if(!Py_IsInitialized())
        return 0;
    Py_Finalize();
    return 0;
}
