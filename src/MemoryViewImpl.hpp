/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_MEMORY_VIEW_IMPL_H
#define __POESIE_MEMORY_VIEW_IMPL_H

#include <thallium.hpp>
#include <poesie/Exception.hpp>
#include <poesie/MemoryView.hpp>

namespace poesie {

namespace tl = thallium;

class MemoryViewImpl : public std::enable_shared_from_this<MemoryViewImpl> {

    public:
    // engine
    tl::engine m_engine;
    // intent
    MemoryView::Intent m_intent;
    // remote data
    tl::bulk     m_remote_bulk;
    tl::endpoint m_remote_ep;
    size_t       m_remote_offset;
    size_t       m_remote_size;
    // local data, lazy-initialized
    char*    m_local_data = nullptr;
    tl::bulk m_local_bulk;
    bool     m_owns_local_data = false;

    MemoryViewImpl() = default;

    ~MemoryViewImpl() {
        if(!m_engine) return;
        if(m_local_bulk.is_null()) return;
        if((m_intent == MemoryView::Intent::INOUT || m_intent == MemoryView::Intent::OUT)) {
            m_remote_bulk.on(m_remote_ep)(m_remote_offset, m_remote_size) << m_local_bulk;
        }
        if(m_owns_local_data) delete[] m_local_data;
    }

    void prepareLocalData() {
        if(m_local_data) return;
        if(m_remote_ep == m_engine.self()) {
            // this is a local bulk handle, try to extract the underlying data
            void* buf_ptr = nullptr;
            hg_size_t buf_size = 0;
            uint32_t actual_count = 1;
            auto hret = HG_Bulk_access(
                    m_remote_bulk.get_bulk(), m_remote_offset, m_remote_size,
                    HG_BULK_READ_ONLY, 1, &buf_ptr, &buf_size, &actual_count);
            if(hret != HG_SUCCESS)
                throw Exception{"HG_Bulk_access failed in MemoryView constructor"};
            if(actual_count != 1 || buf_size != m_remote_size)
                goto fetch_from_remote;
            m_local_data = static_cast<char*>(buf_ptr);
            m_local_bulk = m_remote_bulk;
            return;
        }
        fetch_from_remote:
        // this is a remove bulk handle or a non-contiguous local one
        m_local_data = new char[m_remote_size];
        if(m_intent == MemoryView::Intent::IN)
            m_local_bulk = m_engine.expose({{m_local_data, m_remote_size}}, tl::bulk_mode::write_only);
        else if(m_intent == MemoryView::Intent::INOUT)
            m_local_bulk = m_engine.expose({{m_local_data, m_remote_size}}, tl::bulk_mode::read_write);
        else
            m_local_bulk = m_engine.expose({{m_local_data, m_remote_size}}, tl::bulk_mode::read_only);
        m_owns_local_data = true;
        if(m_intent == MemoryView::Intent::IN || m_intent == MemoryView::Intent::INOUT) {
            try {
                m_remote_bulk.on(m_remote_ep)(m_remote_offset, m_remote_size) >> m_local_bulk;
            } catch(const thallium::exception& ex) {
                throw Exception{ex.what()};
            }
        }
    }
};

}

#endif
