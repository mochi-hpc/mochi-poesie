/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "LuaBackend.hpp"
#include "poesie/MemoryView.hpp"
#include <sol/sol.hpp>
#include <iostream>

#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include "memory/luamem.h"

extern "C" int luaopen_memory(lua_State *L);

static nlohmann::json LuaObjectToJSON(const sol::object& data) {
    nlohmann::json result;

    // Check the type of the Lua object
    switch (data.get_type()) {
        case sol::type::nil:
            result = nullptr;
            break;
        case sol::type::boolean:
            result = data.as<bool>();
            break;
        case sol::type::number:
            // Use double to cover both integer and floating-point numbers
            result = data.as<double>();
            break;
        case sol::type::string:
            result = data.as<std::string>();
            break;
        case sol::type::table: {
            sol::table lua_table = data.as<sol::table>();

            // Check if the table is an array or an object (dictionary)
            bool is_array = true;
            std::size_t expected_index = 1;

            for (auto& kv : lua_table) {
                sol::object key = kv.first;
                if (key.get_type() != sol::type::number || key.as<std::size_t>() != expected_index) {
                    is_array = false;
                    break;
                }
                expected_index++;
            }

            if (is_array) {
                result = json::array();
                // Convert the Lua table to a JSON array
                for (auto& kv : lua_table) {
                    result.push_back(LuaObjectToJSON(kv.second));
                }
            } else {
                result = json::object();
                // Convert the Lua table to a JSON object
                for (auto& kv : lua_table) {
                    std::string key = kv.first.as<std::string>();
                    result[key] = LuaObjectToJSON(kv.second);
                }
            }
            break;
        }
        case sol::type::function:
            // Functions cannot be represented in JSON
            result = "<function>";
            break;
        case sol::type::userdata:
            // Userdata cannot be directly represented in JSON
            result = "<userdata>";
            break;
        case sol::type::thread:
            // Threads cannot be directly represented in JSON
            result = "<thread>";
            break;
        case sol::type::lightuserdata:
            // Light userdata cannot be directly represented in JSON
            result = "<lightuserdata>";
            break;
        default:
            result = "<unknown>"; // Fallback for unknown types
            break;
    }

    return result;
}

static sol::object JSONToLuaObject(
        const thallium::engine& engine,
        const nlohmann::json& json,
        sol::state_view lua,
        std::vector<poesie::MemoryView>& createdViews) {
    switch (json.type()) {
        case nlohmann::json::value_t::null:
            return sol::make_object(lua, sol::nil);
        case nlohmann::json::value_t::boolean:
            return sol::make_object(lua, json.get<bool>());
        case nlohmann::json::value_t::number_integer:
        case nlohmann::json::value_t::number_unsigned:
            return sol::make_object(lua, json.get<int>());
        case nlohmann::json::value_t::number_float:
            return sol::make_object(lua, json.get<double>());
        case nlohmann::json::value_t::string:
            return sol::make_object(lua, json.get<std::string>());
        case nlohmann::json::value_t::array: {
            sol::table lua_table = lua.create_table();
            for (size_t i = 0; i < json.size(); ++i) {
                // Lua arrays are 1-indexed
                lua_table[i + 1] = JSONToLuaObject(engine, json[i], lua, createdViews);
            }
            return lua_table;
        }
        case nlohmann::json::value_t::object: {
            sol::table lua_table = lua.create_table();
            for (auto it = json.begin(); it != json.end(); ++it) {
                lua_table[it.key()] = JSONToLuaObject(engine, it.value(), lua, createdViews);
            }
            return lua_table;
        }
        case nlohmann::json::value_t::binary: {
            if(!poesie::MemoryView::IsMemoryView(json)) {
                auto& b = json.get_binary();
                return sol::make_object(lua, std::string((char*)b.data(), b.size()));
            }
            // else, this is a MemoryView object
            auto view = poesie::MemoryView{engine, json};
            createdViews.push_back(view);
            lua_State* L = lua.lua_state();
            luamem_newref(L);
            int ref_idx = lua_gettop(L);
            static auto cleanup = [](lua_State*, void*, size_t) {};
            luamem_setref(L, ref_idx, view.data(), view.size(), cleanup);
            return sol::object{L, ref_idx};
        }
        default:
            return sol::make_object(lua, sol::nil); // Fallback for unsupported types
    }
}

POESIE_REGISTER_BACKEND(lua, LuaVm);

LuaVm::LuaVm(thallium::engine engine, const json& config)
: m_engine(std::move(engine))
, m_config(config)
, m_lua_state(sol::state()) {
    m_lua_state.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::package,
        sol::lib::table);
    if(m_config.is_object()) {
        std::vector<json> args;
        if(m_config.contains("preamble_argv")) {
            if(!m_config["preamble_argv"].is_array())
                throw poesie::Exception{"\"preamble_argv\" should be an array"};
            for(auto& arg : m_config["preamble_argv"]) {
                args.push_back(arg);
            }
        }
        if(m_config.contains("preamble_file") && m_config["preamble_file"].is_string()) {
            auto result = load(m_config["preamble_file"].get_ref<const std::string&>(), args);
            if(!result.success()) {
                throw poesie::Exception{
                    std::string("Could not load preamble file: ") + result.error()};
            }
        }
        if(m_config.contains("preamble") && m_config["preamble"].is_string()) {
            auto result = execute(m_config["preamble"].get_ref<const std::string&>(), args);
            if(!result.success()) {
                throw poesie::Exception{
                    std::string("Could not execute preamble: ") + result.error()};
            }
        }
    }
    luaopen_memory(m_lua_state.lua_state());
    lua_setglobal(m_lua_state.lua_state(), "memory");
}

std::string LuaVm::getConfig() const {
    return m_config.dump();
}

poesie::Result<json> LuaVm::execute(
        std::string_view code,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    try {
        std::vector<poesie::MemoryView> createdViews;
        m_lua_state["arg"] = JSONToLuaObject(m_engine, args, m_lua_state, createdViews);
        sol::protected_function_result r = m_lua_state.script(std::string(code));
        if(r.valid()) {
            result.value() = LuaObjectToJSON(r);
        } else {
            throw sol::error{r};
        }
    } catch (const sol::error& e) {
        result.error() = "Error executing Lua code: ";
        result.error() += e.what();
        result.success() = false;
    }
    return result;
}

poesie::Result<json> LuaVm::load(
        std::string_view filename,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    try {
        std::vector<poesie::MemoryView> createdViews;
        m_lua_state["arg"] = JSONToLuaObject(m_engine, args, m_lua_state, createdViews);
        sol::protected_function_result r = m_lua_state.script_file(std::string(filename));
        if(r.valid()) {
            result.value() = LuaObjectToJSON(r);
        } else {
            throw sol::error{r};
        }
    } catch (const sol::error& e) {
        result.error() = "Error executing Lua code: ";
        result.error() += e.what();
        result.success() = false;
    }
    return result;
}

poesie::Result<json> LuaVm::call(
        std::string_view function,
        std::string_view target,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    try {
        std::vector<poesie::MemoryView> createdViews;
        std::vector<sol::object> lua_args;
        for(auto& arg : args) {
            lua_args.push_back(JSONToLuaObject(m_engine, arg, m_lua_state, createdViews));
        }
        if(target.empty()) {
            sol::protected_function lua_function = m_lua_state[function];
            sol::protected_function_result r = lua_function(sol::as_args(lua_args));
            if(r.valid()) {
                result.value() = LuaObjectToJSON(r);
            } else {
                throw sol::error{r};
            }
        } else {
            sol::table lua_target = m_lua_state[target];
            if(!lua_target.valid()) {
                result.error() = "Could not find target ";
                result.error() += target;
                result.success() = false;
            } else {
                if(!lua_target[function].valid()) {
                    result.error() = "Could not find method ";
                    result.error() += function;
                    result.success() = false;
                } else {
                    sol::protected_function lua_function = lua_target[function];
                    sol::protected_function_result r = lua_function(lua_target, sol::as_args(lua_args));
                    if(r.valid()) {
                        result.value() = LuaObjectToJSON(r);
                    } else {
                        throw sol::error{r};
                    }
                }
            }
        }
    } catch (const sol::error& e) {
        result.error() = "Error executing Lua code: ";
        result.error() += e.what();
        result.success() = false;
    }
    return result;
}

/**
 * @see Backend::install.
 */
poesie::Result<bool> LuaVm::install(
        std::string_view name,
        ForeignFn function,
        size_t nargs) {
    poesie::Result<bool> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    m_lua_state[name] =
        [func=std::move(function), nargs, this](sol::variadic_args args) -> sol::object {
        // Check the number of arguments
        if (args.size() != nargs) {
            throw std::runtime_error("Invalid number of arguments passed to function.");
        }

        // Convert Lua arguments to JSON
        std::vector<json> json_args;
        json_args.reserve(nargs);
        for (size_t i = 0; i < nargs; ++i) {
            json_args.push_back(LuaObjectToJSON(args[i]));
        }

        // Call the C++ function with the JSON arguments
        json result = func(json_args);

        // Convert the JSON result back to a Lua object
        std::vector<poesie::MemoryView> createdViews;
        return JSONToLuaObject(m_engine, result, m_lua_state, createdViews);
    };
    return result;
}

poesie::Result<bool> LuaVm::destroy() {
    poesie::Result<bool> result;
    result.value() = true;
    // or result.success() = true
    return result;
}

std::unique_ptr<poesie::Backend> LuaVm::create(const thallium::engine& engine, const json& config) {
    (void)engine;
    return std::unique_ptr<poesie::Backend>(new LuaVm(engine, config));
}
