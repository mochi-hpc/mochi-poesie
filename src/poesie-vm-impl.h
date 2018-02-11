#ifndef __POESIE_VM_IMPL_H
#define __POESIE_VM_IMPL_H

#include "poesie-common.h"

typedef int (*execute_fn)(void*, const char*, char**);
typedef int (*finalize_fn)(void*);

struct poesie_vm {
    poesie_lang_t lang;
    void*         impl;
    execute_fn    execute;
    finalize_fn   finalize;
};

#endif
