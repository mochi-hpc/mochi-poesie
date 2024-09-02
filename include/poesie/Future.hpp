/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __POESIE_FUTURE_HPP
#define __POESIE_FUTURE_HPP

#include <poesie/Exception.hpp>
#include <poesie/Result.hpp>
#include <thallium.hpp>
#include <memory>
#include <functional>

namespace poesie {

/**
 * @brief Future objects are used to keep track of
 * on-going asynchronous operations.
 */
template<typename T, typename Wrapper = T>
class Future {

    public:

    /**
     * @brief Copy constructor.
     */
    Future() = default;

    /**
     * @brief Copy constructor.
     */
    Future(const Future& other) = default;

    /**
     * @brief Move constructor.
     */
    Future(Future&& other) = default;

    /**
     * @brief Copy-assignment operator.
     */
    Future& operator=(const Future& other) = default;

    /**
     * @brief Move-assignment operator.
     */
    Future& operator=(Future&& other) = default;

    /**
     * @brief Destructor.
     */
    ~Future() = default;

    /**
     * @brief Wait for the request to complete.
     */
    T wait() {
        Result<Wrapper> result = m_resp.wait();
        return std::move(result).valueOrThrow();
    }

    /**
     * @brief Test if the request has completed, without blocking.
     */
    bool completed() const {
        return m_resp.received();
    }

    /**
     * @brief Constructor.
     */
    Future(thallium::async_response resp)
    : m_resp(std::move(resp)) {}

    private:

    thallium::async_response m_resp;
};

}

#endif
