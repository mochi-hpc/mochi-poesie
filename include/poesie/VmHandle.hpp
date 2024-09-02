/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_VM_HANDLE_HPP
#define __POESIE_VM_HANDLE_HPP

#include <thallium.hpp>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <poesie/Client.hpp>
#include <poesie/Exception.hpp>
#include <poesie/Future.hpp>
#include <poesie/JsonSerialize.hpp>

namespace poesie {

namespace tl = thallium;

class Client;
class VmHandleImpl;

/**
 * @brief A VmHandle object is a handle for a remote vm
 * on a server. It enables invoking the vm's functionalities.
 */
class VmHandle {

    friend class Client;

    public:

    using ReturnType = nlohmann::json;
    using ArgsType = std::vector<nlohmann::json>;
    using FutureType = Future<ReturnType, JsonWrapper>;

    /**
     * @brief Constructor. The resulting VmHandle handle will be invalid.
     */
    VmHandle();

    /**
     * @brief Copy-constructor.
     */
    VmHandle(const VmHandle&);

    /**
     * @brief Move-constructor.
     */
    VmHandle(VmHandle&&);

    /**
     * @brief Copy-assignment operator.
     */
    VmHandle& operator=(const VmHandle&);

    /**
     * @brief Move-assignment operator.
     */
    VmHandle& operator=(VmHandle&&);

    /**
     * @brief Destructor.
     */
    ~VmHandle();

    /**
     * @brief Returns the client this vm has been opened with.
     */
    Client client() const;


    /**
     * @brief Checks if the VmHandle instance is valid.
     */
    operator bool() const;

    /**
     * @brief Requests the target vm to execute the provided code.
     *
     * @param[in] code Code to execute.
     * @param[in] args Arguments to pass to the script.
     *
     * @return a Future that the caller can wait on.
     */
    FutureType execute(std::string_view code,
                       const ArgsType& args = ArgsType{}) const;

    /**
     * @brief Requests the target vm to load the specified file.
     *
     * @param[in] filename File to load.
     * @param[in] args Arguments to pass to the script.
     *
     * @return a Future that the caller can wait on.
     */
    FutureType load(std::string_view filename,
                    const ArgsType& args = ArgsType{}) const;

    /**
     * @brief Requests the target vm to call the specified
     * function on the specified target with the specified
     * positional and keyword arguments.
     *
     * @param[in] function Function to call.
     * @param[in] target Target object (may be empty).
     * @param[in] args array of arguments.
     *
     * @return a Future that the caller can wait on.
     */
    FutureType call(
        std::string_view function,
        std::string_view target,
        const ArgsType& args) const;

    private:

    /**
     * @brief Constructor is private. Use a Client object
     * to create a VmHandle instance.
     *
     * @param impl Pointer to implementation.
     */
    VmHandle(const std::shared_ptr<VmHandleImpl>& impl);

    std::shared_ptr<VmHandleImpl> self;
};

}

#endif
