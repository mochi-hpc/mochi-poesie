/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_PROVIDER_IMPL_H
#define __POESIE_PROVIDER_IMPL_H

#include "poesie/JsonSerialize.hpp"
#include "poesie/Backend.hpp"

#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <tuple>

namespace poesie {

using namespace std::string_literals;
namespace tl = thallium;

class ProviderImpl : public tl::provider<ProviderImpl> {

    auto id() const { return get_provider_id(); } // for convenience

    using json = nlohmann::json;

    #define DEF_LOGGING_FUNCTION(__name__)                         \
    template<typename ... Args>                                    \
    void __name__(Args&&... args) {                                \
        auto msg = fmt::format(std::forward<Args>(args)...);       \
        spdlog::__name__("[poesie:{}] {}", get_provider_id(), msg); \
    }

    DEF_LOGGING_FUNCTION(trace)
    DEF_LOGGING_FUNCTION(debug)
    DEF_LOGGING_FUNCTION(info)
    DEF_LOGGING_FUNCTION(warn)
    DEF_LOGGING_FUNCTION(error)
    DEF_LOGGING_FUNCTION(critical)

    #undef DEF_LOGGING_FUNCTION

    public:

    tl::engine           m_engine;
    tl::pool             m_pool;
    // Client RPC
    tl::auto_remote_procedure m_execute;
    tl::auto_remote_procedure m_load;
    tl::auto_remote_procedure m_call;
    // FIXME: other RPCs go here ...
    // Backend
    std::shared_ptr<Backend> m_backend;

    ProviderImpl(const tl::engine& engine, uint16_t provider_id,
                 const std::string& config, const tl::pool& pool)
    : tl::provider<ProviderImpl>(engine, provider_id, "poesie")
    , m_engine(engine)
    , m_pool(pool)
    , m_execute(define("poesie_execute",  &ProviderImpl::executeRPC, pool))
    , m_load(define("poesie_load",  &ProviderImpl::loadRPC, pool))
    , m_call(define("poesie_call",  &ProviderImpl::callRPC, pool))
    {
        trace("Registered provider with id {}", get_provider_id());
        json json_config;
        try {
            json_config = json::parse(config);
        } catch(json::parse_error& e) {
            error("Could not parse provider configuration: {}", e.what());
            return;
        }
        if(!json_config.is_object()) return;
        if(!json_config.contains("vm")) return;
        auto& vm = json_config["vm"];
        if(!vm.is_object()) return;
        if(vm.contains("type") && vm["type"].is_string()) {
            auto& vm_type = vm["type"].get_ref<const std::string&>();
            auto vm_config = vm.contains("config") ? vm["config"] : json::object();
            auto result = createVm(vm_type, vm_config);
            result.check();
        }
    }

    ~ProviderImpl() {
        trace("Deregistering provider");
        if(m_backend) {
            m_backend->destroy();
        }
    }

    std::string getConfig() const {
        auto config = json::object();
        if(m_backend) {
            config["vm"] = json::object();
            auto vm_config = json::object();
            vm_config["type"] = m_backend->name();
            vm_config["config"] = json::parse(m_backend->getConfig());
            config["vm"] = std::move(vm_config);
        }
        return config.dump();
    }

    Result<bool> createVm(const std::string& vm_type,
                          const json& vm_config) {

        Result<bool> result;

        try {
            m_backend = VmFactory::createVm(vm_type, get_engine(), vm_config);
        } catch(const std::exception& ex) {
            result.success() = false;
            result.error() = ex.what();
            error("Error when creating vm of type {}: {}",
                  vm_type, result.error());
            return result;
        }

        if(not m_backend) {
            result.success() = false;
            result.error() = "Unknown vm type "s + vm_type;
            error("Unknown vm type {}", vm_type);
            return result;
        }

        trace("Successfully created vm of type {}", vm_type);
        return result;
    }

    void executeRPC(const tl::request& req,
                    const std::string& code,
                    std::vector<JsonWrapper>& jargs) {
        trace("Received execute request");
        std::vector<json> args{
            std::move(jargs).begin(),
            std::move(jargs).end()
        };
        Result<JsonWrapper> result;
        tl::auto_respond<decltype(result)> response{req, result};
        if(!m_backend) {
            result.success() = false;
            result.error() = "Provider has no VM attached";
        } else {
            result = m_backend->execute(code, args);
        }
        trace("Successfully executed execute RPC");
    }

    void loadRPC(const tl::request& req,
                 const std::string& filename,
                 std::vector<JsonWrapper>& jargs) {
        trace("Received load request");
        std::vector<json> args{
            std::move(jargs).begin(),
            std::move(jargs).end()
        };
        Result<JsonWrapper> result;
        tl::auto_respond<decltype(result)> response{req, result};
        if(!m_backend) {
            result.success() = false;
            result.error() = "Provider has no VM attached";
        } else {
            result = m_backend->load(filename, args);
        }
        trace("Successfully executed load RPC");
    }

    void callRPC(const tl::request& req,
                 const std::string& function,
                 const std::string& target,
                 std::vector<JsonWrapper>& jargs) {
        trace("Received call request");
        std::vector<json> args{
            std::move(jargs).begin(),
            std::move(jargs).end()
        };
        Result<JsonWrapper> result;
        tl::auto_respond<decltype(result)> response{req, result};
        if(!m_backend) {
            result.success() = false;
            result.error() = "Provider has no VM attached";
        } else {
            result = m_backend->call(function, target, args);
        }
        trace("Successfully executed call RPC");
    }

};

}

#endif
