/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_PROVIDER_HPP
#define __POESIE_PROVIDER_HPP

#include <thallium.hpp>
#include <memory>

namespace poesie {

namespace tl = thallium;

class ProviderImpl;
class Backend;

/**
 * @brief A Provider is an object that can receive RPCs
 * and dispatch them to specific vms.
 */
class Provider {

    public:

    /**
     * @brief Constructor.
     *
     * @param engine Thallium engine to use to receive RPCs.
     * @param provider_id Provider id.
     * @param config JSON-formatted configuration.
     * @param pool Argobots pool to use to handle RPCs.
     */
    Provider(const tl::engine& engine,
             uint16_t provider_id,
             const std::string& config,
             const tl::pool& pool = tl::pool());

    /**
     * @brief Copy-constructor is deleted.
     */
    Provider(const Provider&) = delete;

    /**
     * @brief Move-constructor.
     */
    Provider(Provider&&);

    /**
     * @brief Copy-assignment operator is deleted.
     */
    Provider& operator=(const Provider&) = delete;

    /**
     * @brief Move-assignment operator is deleted.
     */
    Provider& operator=(Provider&&) = delete;

    /**
     * @brief Destructor.
     */
    ~Provider();

    /**
     * @brief Return a JSON-formatted configuration of the provider.
     *
     * @return JSON formatted string.
     */
    std::string getConfig() const;

    /**
     * @brief Get a pointer to the internal backend.
     */
    Backend* getBackend() const;

    /**
     * @brief Checks whether the Provider instance is valid.
     */
    operator bool() const;

    private:

    std::shared_ptr<ProviderImpl> self;
};

}

#endif
