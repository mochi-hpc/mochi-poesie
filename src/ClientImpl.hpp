/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_CLIENT_IMPL_H
#define __POESIE_CLIENT_IMPL_H

#include <thallium.hpp>
#include <thallium/serialization/stl/unordered_set.hpp>
#include <thallium/serialization/stl/unordered_map.hpp>
#include <thallium/serialization/stl/string.hpp>

namespace poesie {

namespace tl = thallium;

class ClientImpl {

    public:

    tl::engine           m_engine;
    tl::remote_procedure m_execute;
    tl::remote_procedure m_load;
    tl::remote_procedure m_call;

    ClientImpl(const tl::engine& engine)
    : m_engine(engine)
    , m_execute(m_engine.define("poesie_execute"))
    , m_load(m_engine.define("poesie_load"))
    , m_call(m_engine.define("poesie_call"))
    {}

    ClientImpl(margo_instance_id mid)
    : ClientImpl(tl::engine(mid)) {}

    ~ClientImpl() {}
};

}

#endif
