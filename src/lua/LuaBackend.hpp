/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __LUA_BACKEND_HPP
#define __LUA_BACKEND_HPP

#include <string_view>
#include <poesie/Backend.hpp>
#include <sol/sol.hpp>

using json = nlohmann::json;

/**
 * Lua implementation of an poesie Backend.
 */
class LuaVm : public poesie::Backend {

    thallium::engine m_engine;
    json             m_config;
    thallium::mutex  m_mtx;
    sol::state       m_lua_state;

    public:

    /**
     * @brief Constructor.
     */
    LuaVm(thallium::engine engine, const json& config);

    /**
     * @brief Move-constructor.
     */
    LuaVm(LuaVm&&) = default;

    /**
     * @brief Copy-constructor.
     */
    LuaVm(const LuaVm&) = default;

    /**
     * @brief Move-assignment operator.
     */
    LuaVm& operator=(LuaVm&&) = default;

    /**
     * @brief Copy-assignment operator.
     */
    LuaVm& operator=(const LuaVm&) = default;

    /**
     * @brief Destructor.
     */
    virtual ~LuaVm() = default;

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
     * create a LuaVm.
     *
     * @param engine Thallium engine
     * @param config JSON configuration for the vm
     *
     * @return a unique_ptr to a vm
     */
    static std::unique_ptr<poesie::Backend> create(const thallium::engine& engine, const json& config);

};

#endif
