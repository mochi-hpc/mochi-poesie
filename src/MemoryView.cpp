/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "poesie/Exception.hpp"
#include "poesie/MemoryView.hpp"
#include "poesie/VmHandle.hpp"
#include "poesie/Result.hpp"

#include "MemoryViewImpl.hpp"
#include "VmHandleImpl.hpp"

#include <thallium/serialization/stl/string.hpp>

namespace tl = thallium;

namespace poesie {

MemoryView::MemoryView() = default;

MemoryView::MemoryView(tl::engine engine, const char* data, size_t size, Intent intent) {
    auto bulk = engine.expose({{const_cast<char*>(data), size}}, tl::bulk_mode::read_write);
    auto owner = engine.self();
    auto view = MemoryView{engine, bulk, owner, intent, 0, size};
    self = std::move(view).self;
}

MemoryView::MemoryView(tl::engine engine,
                       tl::bulk bulk,
                       tl::endpoint owner,
                       Intent intent,
                       std::optional<size_t> offset_opt,
                       std::optional<size_t> size_opt) {
    if(offset_opt.value_or(0) + size_opt.value_or(bulk.size()) > bulk.size())
        throw Exception{"Invalid offset and/or size when exposing bulk handle as MemoryView"};
    size_t offset = offset_opt.value_or(0);
    size_t size  = size_opt.value_or(bulk.size());
    self = std::make_shared<MemoryViewImpl>();
    self->m_intent        = intent;
    self->m_engine        = engine;
    self->m_remote_bulk   = bulk;
    self->m_remote_ep     = owner;
    self->m_remote_offset = offset;
    self->m_remote_size   = size;
}

MemoryView::MemoryView(tl::engine engine,
                       const nlohmann::json& j) {
    if(!IsMemoryView(j)) {
        throw Exception{"JSON object is not a MemoryView representation"};
    }
    nlohmann::json::binary_t binary;
    if(j.is_binary()) {
        binary = j.get_binary();
    } else {
        binary.reserve(j["bytes"].size());
        for(auto& b : j["bytes"])
            binary.push_back(b.get<std::uint8_t>());
    }
    size_t owner_size;
    std::string owner;
    hg_bulk_t bulk = HG_BULK_NULL;
    size_t bulk_size;
    size_t remote_offset, remote_size;
    size_t off = 0;
    Intent intent;
    std::memcpy(&intent, &binary[off], sizeof(intent));
    off += sizeof(intent);
    std::memcpy(&owner_size, &binary[off], sizeof(owner_size));
    off += sizeof(owner_size);
    owner.resize(owner_size);
    std::memcpy(const_cast<char*>(owner.data()), &binary[off], owner_size);
    off += owner_size;
    std::memcpy(&bulk_size, &binary[off], sizeof(bulk_size));
    off += sizeof(bulk_size);
    margo_bulk_deserialize(
            engine.get_margo_instance(),
            &bulk, &binary[off], bulk_size);
    off += bulk_size;
    std::memcpy(&remote_offset, &binary[off], sizeof(remote_offset));
    off += sizeof(remote_offset);
    std::memcpy(&remote_size, &binary[off], sizeof(remote_size));
    auto remote_ep = engine.lookup(owner);
    auto view = MemoryView{
        engine,
            engine.wrap(bulk, remote_ep == engine.self()),
            remote_ep,
            intent,
            remote_offset,
            remote_size
    };
    self = std::move(view).self;
}

MemoryView::MemoryView(MemoryView&& other) = default;

MemoryView& MemoryView::operator=(MemoryView&& other) = default;

MemoryView::MemoryView(const MemoryView& other) = default;

MemoryView& MemoryView::operator=(const MemoryView& other) = default;

MemoryView::~MemoryView() = default;

MemoryView::operator bool() const {
    return static_cast<bool>(self);
}

MemoryView::Intent MemoryView::intent() const {
    return self ? self->m_intent : MemoryView::Intent::IN;
}

char* MemoryView::data() const {
    if(!self) return nullptr;
    self->prepareLocalData();
    return self->m_local_data;
}

size_t MemoryView::size() const {
    return self ? self->m_remote_size : 0;
}

// very arbitrary
#define MEMORY_VIEW_SUBTYPE_CODE 2388

nlohmann::json MemoryView::toJson() const {
    if(!self) return nlohmann::json();
    nlohmann::json::binary_t binary;
    binary.set_subtype(MEMORY_VIEW_SUBTYPE_CODE);
    // get the owner as a string
    auto owner = static_cast<std::string>(self->m_remote_ep);
    size_t owner_size = owner.size();
    // get the bulk's serialized size
    size_t bulk_size = HG_Bulk_get_serialize_size(self->m_remote_bulk.get_bulk(), 0);
    // calculate the needed size
    auto bin_size = sizeof(self->m_intent)
                  + bulk_size + sizeof(bulk_size)
                  + owner_size + sizeof(owner_size)
                  + sizeof(self->m_remote_offset)
                  + sizeof(self->m_remote_size);
    // resize
    binary.resize(bin_size);
    // serialize
    size_t off = 0;
    std::memcpy(&binary[off], &self->m_intent, sizeof(self->m_intent));
    off += sizeof(self->m_intent);
    std::memcpy(&binary[off], &owner_size, sizeof(owner_size));
    off += sizeof(owner_size);
    std::memcpy(&binary[off], owner.data(), owner_size);
    off += owner_size;
    std::memcpy(&binary[off], &bulk_size, sizeof(bulk_size));
    off += sizeof(bulk_size);
    margo_bulk_serialize(&binary[off], bulk_size, 0, self->m_remote_bulk.get_bulk());
    off += bulk_size;
    std::memcpy(&binary[off], &self->m_remote_offset, sizeof(self->m_remote_offset));
    off += sizeof(self->m_remote_offset);
    std::memcpy(&binary[off], &self->m_remote_size, sizeof(self->m_remote_size));
    off += sizeof(self->m_remote_size);
    return binary;
}

bool MemoryView::IsMemoryView(const nlohmann::json& j) {
    if(j.is_binary()) {
        auto binary = j.get_binary();
        if(!binary.has_subtype()) return false;
        if(binary.subtype() != MEMORY_VIEW_SUBTYPE_CODE) return false;
        return true;
    } else if(j.is_object()) {
        // if the JSON object has been dumped, the binary data will have been converted
        // into an object with a "bytes" property and a "subtype" property.
        if(!(j.contains("bytes") && j.contains("subtype"))) return false;
        auto& bytes = j["bytes"];
        auto& subtype = j["subtype"];
        if(!subtype.is_number_unsigned() || subtype.get<int>() != MEMORY_VIEW_SUBTYPE_CODE)
            return false;
        if(!bytes.is_array()) return false;
        for(auto& b : bytes) if(!b.is_number_unsigned() || b.get<size_t>() >= 256) return false;
        return true;
    } else {
        return false;
    }
}

#undef MEMORY_VIEW_SUBTYPE_CODE

}
