/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_BACKEND_HPP
#define __POESIE_BACKEND_HPP

#include <poesie/Result.hpp>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <string_view>
#include <nlohmann/json.hpp>
#include <thallium.hpp>

/**
 * @brief Helper class to register backend types into the backend factory.
 */
template<typename BackendType>
class __PoesieBackendRegistration;

namespace poesie {

/**
 * @brief Interface for vm backends. To build a new backend,
 * implement a class MyBackend that inherits from Backend, and put
 * POESIE_REGISTER_BACKEND(mybackend, MyBackend); in a cpp file
 * that includes your backend class' header file.
 *
 * Your backend class should also have two static functions to
 * respectively create and open a vm:
 *
 * std::unique_ptr<Backend> create(const json& config)
 * std::unique_ptr<Backend> attach(const json& config)
 */
class Backend {

    template<typename BackendType>
    friend class ::__PoesieBackendRegistration;

    std::string m_name;

    public:

    using ReturnType = nlohmann::json;
    using ArgsType = std::vector<nlohmann::json>;
    using ForeignFn = std::function<ReturnType(const ArgsType&)>;

    /**
     * @brief Constructor.
     */
    Backend() = default;

    /**
     * @brief Move-constructor.
     */
    Backend(Backend&&) = default;

    /**
     * @brief Copy-constructor.
     */
    Backend(const Backend&) = default;

    /**
     * @brief Move-assignment operator.
     */
    Backend& operator=(Backend&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    Backend& operator=(const Backend&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~Backend() = default;

    /**
     * @brief Return the name of backend.
     */
    const std::string& name() const {
        return m_name;
    }

    /**
     * @brief Returns a JSON-formatted configuration string.
     */
    virtual std::string getConfig() const = 0;

    /**
     * @brief Execute the provided code in the VM.
     *
     * @param code Code to execute.
     * @param args Arguments to pass to the script.
     *
     * @return a Result containing the result.
     */
    virtual Result<nlohmann::json> execute(std::string_view code,
                                           const std::vector<nlohmann::json>& args) = 0;

    /**
     * @brief Load the file in the VM and execute its content.
     *
     * @param filename File load load.
     * @param args Arguments to pass to the script.
     *
     * @return a Result containing the result.
     */
    virtual Result<nlohmann::json> load(std::string_view filename,
                                        const std::vector<nlohmann::json>& args) = 0;

    /**
     * @brief Invoke the designated function or method.
     *
     * @param function Name of the function/method.
     * @param target Target object (may be empty for a global function).
     * @param args Array of positional arguments.
     *
     * @return A Result containing the JSON representation of the
     * function's return value.
     */
    virtual Result<nlohmann::json> call(
            std::string_view function,
            std::string_view target,
            const std::vector<nlohmann::json>& args) = 0;

    /**
     * @brief Install a foreign function that the backend will be able to use.
     *
     * @param name Name of the function.
     * @param function Function.
     * @param nargs Number of expected arguments.
     *
     * @return Result.
     */
    virtual Result<bool> install(
            std::string_view name,
            ForeignFn function,
            size_t nargs) = 0;

    /**
     * @brief Destroys the underlying vm.
     *
     * @return a Result<bool> instance indicating
     * whether the database was successfully destroyed.
     */
    virtual Result<bool> destroy() = 0;

};

/**
 * @brief The VmFactory contains functions to create
 * or open vms.
 */
class VmFactory {

    template<typename BackendType>
    friend class ::__PoesieBackendRegistration;

    using json = nlohmann::json;

    public:

    VmFactory() = delete;

    /**
     * @brief Creates a vm and returns a unique_ptr to the created instance.
     *
     * @param backend_name Name of the backend to use.
     * @param engine Thallium engine.
     * @param config Configuration object to pass to the backend's create function.
     *
     * @return a unique_ptr to the created Vm.
     */
    static std::unique_ptr<Backend> createVm(const std::string& backend_name,
                                                   const thallium::engine& engine,
                                                   const json& config);

    private:

    static std::unordered_map<std::string,
                std::function<std::unique_ptr<Backend>(const thallium::engine&, const json&)>> create_fn;
};

} // namespace poesie


#define POESIE_REGISTER_BACKEND(__backend_name, __backend_type) \
    static __PoesieBackendRegistration<__backend_type> __poesie ## __backend_name ## _backend( #__backend_name )

template<typename BackendType>
class __PoesieBackendRegistration {

    using json = nlohmann::json;

    public:

    __PoesieBackendRegistration(const std::string& backend_name)
    {
        poesie::VmFactory::create_fn[backend_name] = [backend_name](const thallium::engine& engine, const json& config) {
            auto p = BackendType::create(engine, config);
            p->m_name = backend_name;
            return p;
        };
    }
};

#endif
