/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __JX9_BACKEND_HPP
#define __JX9_BACKEND_HPP

#include <string_view>
#include <poesie/Backend.hpp>
#include "jx9/jx9.h"

using json = nlohmann::json;

/**
 * Jx9 implementation of an poesie Backend.
 */
class Jx9Vm : public poesie::Backend {

    struct FunctionHolder {

        ForeignFn func;
        size_t    expected_args;

        FunctionHolder(
                std::function<json(const std::vector<json>)> f,
                size_t n)
        : func(std::move(f))
        , expected_args(n) {}

        static int binding(jx9_context* pCtx, int argc, jx9_value** argv);

    };

    thallium::engine  m_engine;
    json              m_config;
    json              m_global = json::object();
    std::vector<json> m_args;
    thallium::mutex   m_mtx;
    jx9*              m_jx9_engine = nullptr;
    std::string       m_preamble;
    std::unordered_map<std::string, std::unique_ptr<FunctionHolder>> m_ffuncs;

    public:

    /**
     * @brief Constructor.
     */
    Jx9Vm(thallium::engine engine, const json& config);

    /**
     * @brief Move-constructor.
     */
    Jx9Vm(Jx9Vm&&) = default;

    /**
     * @brief Copy-constructor.
     */
    Jx9Vm(const Jx9Vm&) = default;

    /**
     * @brief Move-assignment operator.
     */
    Jx9Vm& operator=(Jx9Vm&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    Jx9Vm& operator=(const Jx9Vm&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~Jx9Vm();

    /**
     * @brief Get the vm's configuration as a JSON-formatted string.
     */
    std::string getConfig() const override;

    /**
     * @see Backend::execute
     */
    poesie::Result<json> execute(
            std::string_view code,
            const std::vector<json>& args) override;

    /**
     * @see Backend::load
     */
    poesie::Result<json> load(
            std::string_view filename,
            const std::vector<json>& args) override;

    /**
     * @see Backend::call.
     */
    poesie::Result<json> call(
          std::string_view function,
          std::string_view target,
          const std::vector<json>& args) override;

    /**
     * @see Backend::install.
     */
    poesie::Result<bool> install(
            std::string_view name,
            ForeignFn function,
            size_t nargs) override;

    /**
     * @brief Destroys the underlying vm.
     *
     * @return a Result<bool> instance indicating
     * whether the database was successfully destroyed.
     */
    poesie::Result<bool> destroy() override;

    /**
     * @brief Static factory function used by the VmFactory to
     * create a Jx9Vm.
     *
     * @param engine Thallium engine
     * @param config JSON configuration for the vm
     *
     * @return a unique_ptr to a vm
     */
    static std::unique_ptr<poesie::Backend> create(const thallium::engine& engine, const json& config);

};

#endif
