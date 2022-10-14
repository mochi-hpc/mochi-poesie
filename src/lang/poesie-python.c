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
    PyThreadState* subint;
    PyObject* main;
    PyObject* globalDictionary;
    PyObject* localDictionary;
    ABT_mutex mutex;
}* python_vm_t;

static int poesie_py_execute(void* impl, const char* code, char** output);
static int poesie_py_finalize(void* impl);

static int init_python();
static int finalize_python();

int poesie_py_vm_init(poesie_vm_t vm, const char* name)
{
    (void)name; // XXX
    int ret;
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

    /* save current thread state */
    PyThreadState* old_ts = PyThreadState_Get();
    /* create sub-interpreter */
    pvm->subint = Py_NewInterpreter();
    /* initialize the context */
    pvm->main             = PyImport_AddModule("__main__");
    pvm->globalDictionary = PyModule_GetDict(pvm->main);
    pvm->localDictionary  = PyDict_New();

    vm->lang = POESIE_LANG_PYTHON;
    vm->impl = (void*)pvm;
    vm->execute  = &poesie_py_execute;
    vm->finalize = &poesie_py_finalize;

    /* swap back to main interpreter */
    PyThreadState_Swap(old_ts);

    return 0;
}

static int poesie_py_execute(void* impl, const char* code, char** output)
{
    python_vm_t pvm = (python_vm_t)impl;
    ABT_mutex_lock(pvm->mutex);
    PyThreadState* _state = PyEval_SaveThread();
    // acquire the GIL
    // Acquire the GIL
    PyGILState_STATE gstate = PyGILState_Ensure();
    // create a new thread state for the the sub interpreter interp
    PyThreadState* ts = PyThreadState_New(pvm->subint->interp);
    // make ts the current thread state
    PyThreadState* oldts = PyThreadState_Swap(ts);
    // execute code
    PyObject* result = PyRun_String(code,
            Py_file_input, pvm->globalDictionary, pvm->localDictionary);
    /* check if error occured */
    PyObject* ex = PyErr_Occurred();
    if (ex) { /* error did occure */
        /* fetch the error */
        PyObject *pExcType , *pExcValue , *pExcTraceback;
        PyErr_Fetch( &pExcType , &pExcValue , &pExcTraceback );
        if(pExcValue != NULL) {
            PyObject* pRepr = PyObject_Repr(pExcValue);
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
    } else {
        PyObject* resultStr = PyObject_Repr(result);
        PyObject* pBytes = PyUnicode_AsEncodedString(resultStr, "UTF-8", "strict");
        *output = PyBytes_AS_STRING(pBytes);
        *output = strdup(*output);
        Py_DECREF(pBytes);
        Py_DECREF(result);
        Py_DECREF(resultStr);
    }
    // release ts
    PyThreadState_Swap(oldts);
    // clear and delete ts
    PyThreadState_Clear(ts);
    PyThreadState_Delete(ts);

    // release the GIL
    PyGILState_Release(gstate);
    PyEval_RestoreThread(_state);
    ABT_mutex_unlock(pvm->mutex);
    return 0;
}

static int poesie_py_finalize(void* impl)
{
    if(!impl) return 0;
    python_vm_t pvm = (python_vm_t)impl;
#if 0
    Py_DECREF(pvm->localDictionary);
    Py_DECREF(pvm->globalDictionary);
    Py_DECREF(pvm->main);
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
