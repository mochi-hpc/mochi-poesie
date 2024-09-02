/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __PYTHON_BACKEND_HPP
#define __PYTHON_BACKEND_HPP

#include <string_view>
#include <poesie/Backend.hpp>
#include <pybind11/embed.h>

using json = nlohmann::json;
namespace py = pybind11;

/**
 * Python implementation of an poesie Backend.
 */
#pragma GCC visibility push(hidden)
class PythonVm : public poesie::Backend {

    thallium::engine       m_engine;
    json                   m_config;
    ABT_mutex              m_mtx;
    py::scoped_interpreter m_guard;          // Initialize the interpreter and keep it alive
    py::object             m_main_module;    // Holds the main module for the subinterpreter
    py::object             m_main_namespace; // Holds the namespace of the main module

    static ABT_mutex_memory s_mtx;

    public:

    /**
     * @brief Constructor.
     */
    PythonVm(thallium::engine engine, const json& config);

    /**
     * @brief Move-constructor.
     */
    PythonVm(PythonVm&&) = default;

    /**
     * @brief Copy-constructor.
     */
    PythonVm(const PythonVm&) = default;

    /**
     * @brief Move-assignment operator.
     */
    PythonVm& operator=(PythonVm&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    PythonVm& operator=(const PythonVm&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~PythonVm() = default;

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
     * @see Backend::call.
     */
    poesie::Result<json> call(
            std::string_view function,
            std::string_view target,
            const std::vector<json>& args) override;

    /**
     * @see Backend::load
     */
    poesie::Result<json> load(
            std::string_view filename,
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
     * create a PythonVm.
     *
     * @param engine Thallium engine
     * @param config JSON configuration for the vm
     *
     * @return a unique_ptr to a vm
     */
    static std::unique_ptr<poesie::Backend> create(const thallium::engine& engine, const json& config);

};
#pragma GCC visibility pop

#endif
