/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __RUBY_BACKEND_HPP
#define __RUBY_BACKEND_HPP

#include <string_view>
#include <poesie/Backend.hpp>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>

using json = nlohmann::json;

/**
 * Ruby implementation of an poesie Backend.
 */
class RubyVm : public poesie::Backend {

    thallium::engine m_engine;
    json             m_config;
    thallium::mutex  m_mtx;
    mrb_state*       m_mrb;
    struct RClass*   m_memoryview_class;

    public:

    /**
     * @brief Constructor.
     */
    RubyVm(thallium::engine engine, const json& config);

    /**
     * @brief Move-constructor.
     */
    RubyVm(RubyVm&&) = default;

    /**
     * @brief Copy-constructor.
     */
    RubyVm(const RubyVm&) = default;

    /**
     * @brief Move-assignment operator.
     */
    RubyVm& operator=(RubyVm&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    RubyVm& operator=(const RubyVm&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~RubyVm();

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
     * create a RubyVm.
     *
     * @param engine Thallium engine
     * @param config JSON configuration for the vm
     *
     * @return a unique_ptr to a vm
     */
    static std::unique_ptr<poesie::Backend> create(const thallium::engine& engine, const json& config);

};

#endif
