/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_VM_HANDLE_IMPL_H
#define __POESIE_VM_HANDLE_IMPL_H

#include "ClientImpl.hpp"

namespace poesie {

class VmHandleImpl {

    public:

    std::shared_ptr<ClientImpl> m_client;
    tl::provider_handle         m_ph;

    VmHandleImpl() = default;

    VmHandleImpl(std::shared_ptr<ClientImpl> client,
                       tl::provider_handle&& ph)
    : m_client(std::move(client))
    , m_ph(std::move(ph)) {}
};

}

#endif
