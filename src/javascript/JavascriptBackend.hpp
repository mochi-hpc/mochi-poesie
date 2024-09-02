/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __JAVASCRIPT_BACKEND_HPP
#define __JAVASCRIPT_BACKEND_HPP

#include "duktape/duktape.h"
#include <poesie/Backend.hpp>

using json = nlohmann::json;

/**
 * Javascript implementation of an poesie Backend.
 */
class JavascriptVm : public poesie::Backend {

    struct FunctionHolder {

        thallium::engine engine;
        std::function<json(const std::vector<json>& args)> func;
        size_t expected_args;

        FunctionHolder(thallium::engine engine,
                       std::function<json(const std::vector<json>)> f,
                       size_t n)
        : engine(std::move(engine))
        , func(std::move(f))
        , expected_args(n) {}

        static duk_ret_t binding(duk_context* ctx);

    };

    thallium::engine m_engine;
    json             m_config;
    thallium::mutex  m_mtx;
    duk_context*     m_ctx;
    std::unordered_map<std::string, std::unique_ptr<FunctionHolder>> m_ffuncs;

    public:

    /**
     * @brief Constructor.
     */
    JavascriptVm(thallium::engine engine, const json& config);

    /**
     * @brief Move-constructor.
     */
    JavascriptVm(JavascriptVm&&) = default;

    /**
     * @brief Copy-constructor.
     */
    JavascriptVm(const JavascriptVm&) = default;

    /**
     * @brief Move-assignment operator.
     */
    JavascriptVm& operator=(JavascriptVm&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    JavascriptVm& operator=(const JavascriptVm&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~JavascriptVm();

    /**
     * @brief Get the vm's configuration as a JSON-formatted string.
     */
    std::string getConfig() const override;

    /**
     * @see Backend::execute.
     */
    poesie::Result<json> execute(
            std::string_view code,
            const std::vector<json>& args) override;

    /**
     * @see Backend::load.
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
     * create a JavascriptVm.
     *
     * @param engine Thallium engine
     * @param config JSON configuration for the vm
     *
     * @return a unique_ptr to a vm
     */
    static std::unique_ptr<poesie::Backend> create(const thallium::engine& engine, const json& config);

};

#endif
