/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "StringView.hpp"
#include "poesie/VmHandle.hpp"
#include "poesie/Result.hpp"
#include "poesie/Exception.hpp"

#include "ClientImpl.hpp"
#include "VmHandleImpl.hpp"

#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include "poesie/JsonSerialize.hpp"

namespace poesie {

VmHandle::VmHandle() = default;

VmHandle::VmHandle(const std::shared_ptr<VmHandleImpl>& impl)
: self(impl) {}

VmHandle::VmHandle(const VmHandle&) = default;

VmHandle::VmHandle(VmHandle&&) = default;

VmHandle& VmHandle::operator=(const VmHandle&) = default;

VmHandle& VmHandle::operator=(VmHandle&&) = default;

VmHandle::~VmHandle() = default;

VmHandle::operator bool() const {
    return static_cast<bool>(self);
}

Client VmHandle::client() const {
    return Client(self->m_client);
}

VmHandle::FutureType VmHandle::execute(std::string_view code,
                                       const VmHandle::ArgsType& args) const
{
    if(not self) throw Exception("Invalid poesie::VmHandle object");
    std::vector<ConstJsonRefWrapper> jargs{args.begin(), args.end()};
    auto& rpc = self->m_client->m_execute;
    auto& ph  = self->m_ph;
    auto async_response = rpc.on(ph).async(code, jargs);
    return FutureType{std::move(async_response)};
}

VmHandle::FutureType VmHandle::load(
        std::string_view filename,
        const VmHandle::ArgsType& args) const
{
    if(not self) throw Exception("Invalid poesie::VmHandle object");
    std::vector<ConstJsonRefWrapper> jargs{args.begin(), args.end()};
    auto& rpc = self->m_client->m_load;
    auto& ph  = self->m_ph;
    auto async_response = rpc.on(ph).async(filename, jargs);
    return FutureType{std::move(async_response)};
}

VmHandle::FutureType VmHandle::call(
        std::string_view function,
        std::string_view target,
        const VmHandle::ArgsType& args) const
{
    if(not self) throw Exception("Invalid poesie::VmHandle object");
    std::vector<ConstJsonRefWrapper> jargs{args.begin(), args.end()};
    auto& rpc = self->m_client->m_call;
    auto& ph  = self->m_ph;
    auto async_response = rpc.on(ph).async(function, target, jargs);
    return FutureType{std::move(async_response)};
}

}
