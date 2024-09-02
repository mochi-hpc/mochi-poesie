/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "JavascriptBackend.hpp"
#include "poesie/Exception.hpp"
#include "poesie/MemoryView.hpp"
#include <fstream>
#include <iostream>
#include <mutex>

POESIE_REGISTER_BACKEND(javascript, JavascriptVm);

// Function to convert a Duktape value at a given stack index to a nlohmann::json object
static nlohmann::json dukValueToJSON(duk_context* ctx, duk_idx_t index) {
    nlohmann::json result;

    if (duk_is_undefined(ctx, index) || duk_is_null(ctx, index)) {
        result = nullptr;  // JSON null
    }
    else if (duk_is_boolean(ctx, index)) {
        result = static_cast<bool>(duk_get_boolean(ctx, index));
    }
    else if (duk_is_number(ctx, index)) {
        result = duk_get_number(ctx, index);
    }
    else if (duk_is_string(ctx, index)) {
        result = duk_get_string(ctx, index);
    }
    else if (duk_is_array(ctx, index)) {
        result = nlohmann::json::array();
        duk_enum(ctx, index, DUK_ENUM_ARRAY_INDICES_ONLY);
        while (duk_next(ctx, -1, 1)) {
            result.push_back(dukValueToJSON(ctx, -1));
            duk_pop_2(ctx);  // Pop value and key (index) from stack
        }
        duk_pop(ctx);  // Pop the enum object
    }
    else if (duk_is_object(ctx, index)) {
        result = nlohmann::json::object();
        duk_enum(ctx, index, DUK_ENUM_OWN_PROPERTIES_ONLY);
        while (duk_next(ctx, -1, 1)) {
            std::string key = duk_get_string(ctx, -2);
            result[key] = dukValueToJSON(ctx, -1);
            duk_pop_2(ctx);  // Pop value and key from stack
        }
        duk_pop(ctx);  // Pop the enum object
    }
    else {
        result = nullptr;  // Fallback to JSON null for unsupported types
    }
    return result;
}

static void JSONtoDukValue(
        const thallium::engine& engine,
        duk_context* ctx,
        const nlohmann::json& value,
        std::vector<poesie::MemoryView> createdViews) {
    if (value.is_null()) {
        duk_push_null(ctx);
    } else if (value.is_boolean()) {
        duk_push_boolean(ctx, value.get<bool>());
    } else if (value.is_number()) {
        duk_push_number(ctx, value.get<double>());
    } else if (value.is_string()) {
        duk_push_string(ctx, value.get<std::string>().c_str());
    } else if (value.is_array()) {
        duk_idx_t arr_idx = duk_push_array(ctx);
        int i = 0;
        for (const auto& item : value) {
            JSONtoDukValue(engine, ctx, item, createdViews);
            duk_put_prop_index(ctx, arr_idx, i++);
        }
    } else if (value.is_object()) {
        duk_push_object(ctx);
        for (auto it = value.begin(); it != value.end(); ++it) {
            JSONtoDukValue(engine, ctx, it.value(), createdViews);
            duk_put_prop_string(ctx, -2, it.key().c_str());
        }
    } else if(value.is_binary()) {
        if(!poesie::MemoryView::IsMemoryView(value)) {
            auto& b = value.get_binary();
            auto p = duk_push_buffer(ctx, b.size(), 1);
            std::memcpy(p, b.data(), b.size());
            return;
        }
        // binary data represents a MemoryView
        auto view = poesie::MemoryView{engine, value};
        createdViews.push_back(view);
        auto data = view.data();
        auto size = view.size();
        duk_push_external_buffer(ctx);
        duk_config_buffer(ctx, -1, data, size);
    } else {
        duk_push_null(ctx); // Fallback for unsupported types
    }
}


JavascriptVm::JavascriptVm(thallium::engine engine, const json& config)
: m_engine(std::move(engine)),
  m_config(config) {
    m_ctx = duk_create_heap_default();
    if(!m_ctx) {
        throw poesie::Exception{"Duktape initialization failed"};
    }
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
}

JavascriptVm::~JavascriptVm() {
    if (m_ctx) {
        duk_destroy_heap(m_ctx);
    }
}

std::string JavascriptVm::getConfig() const {
    return m_config.dump();
}

poesie::Result<json> JavascriptVm::execute(
        std::string_view code,
        const std::vector<json>& args) {
    std::unique_lock<thallium::mutex> guard{m_mtx};
    poesie::Result<json> result;
    duk_push_global_object(m_ctx);
    if (!duk_get_prop_string(m_ctx, -1, "process")) {
        // If 'process' does not exist, create it
        duk_pop(m_ctx);  // Pop the undefined value
        duk_push_object(m_ctx);
        duk_put_prop_string(m_ctx, -2, "process");
        duk_get_prop_string(m_ctx, -1, "process");
    }
    std::vector<poesie::MemoryView> createdViews;
    // Create 'argv' array
    duk_push_array(m_ctx);
    // Push "poesie" as filename
    JSONtoDukValue(m_engine, m_ctx, std::string{"poesie"}, createdViews);
    duk_put_prop_index(m_ctx, -2, 0);
    for (size_t i = 0; i < args.size(); i++) {
        JSONtoDukValue(m_engine, m_ctx, args[i], createdViews);
        duk_put_prop_index(m_ctx, -2, i+1);
    }
    // Assign 'argv' array to 'process.argv'
    duk_put_prop_string(m_ctx, -2, "argv");
    // Pop 'process' and global object from the stack
    duk_pop_2(m_ctx);
    // Execute script
    if (duk_peval_lstring(m_ctx, code.data(), code.size()) != 0) {
        result.success() = false;
        result.error() = "Error executing JavaScript code: ";
        result.error() += duk_safe_to_string(m_ctx, -1);
    } else {
        result.value() = dukValueToJSON(m_ctx, -1);
    }
    duk_pop(m_ctx);  // Pop the result or error from the stack
    return result;
}

poesie::Result<json> JavascriptVm::load(
        std::string_view filename,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::ifstream file{std::string(filename)};
    if (!file) {
        result.success() = false;
        result.error() = "Error opening file: ";
        result.error() += filename;
        return result;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    result = execute(code, args);
    return result;
}

poesie::Result<json> JavascriptVm::call(
        std::string_view function,
        std::string_view target,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    std::vector<poesie::MemoryView> createdViews;
    if (target.empty()) {
        // Look up the global function
        if (!duk_get_global_string(m_ctx, function.data())) {
            result.success() = false;
            result.error() = "Function not found: ";
            result.error()+= std::string(function);
            return result;
        }
        // Push arguments onto the stack
        for (const auto& arg : args) {
            JSONtoDukValue(m_engine, m_ctx, arg, createdViews);
        }
        // Call the function
        if (duk_pcall(m_ctx, args.size()) != 0) {
            result.success() = false;
            result.error() = "Error calling function: ";
            result.error() += duk_safe_to_string(m_ctx, -1);
            duk_pop(m_ctx);  // Pop the error
            return result;
        }
    } else {
        // Look up the target object
        if (!duk_get_global_string(m_ctx, target.data())) {
            result.success() = false;
            result.error() = "Target not found: ";
            result.error()+= std::string(target);
            return result;
        }
        // Look up the method in the target object
        if (!duk_get_prop_string(m_ctx, -1, function.data())) {
            duk_pop(m_ctx);  // Pop the target object
            result.success() = false;
            result.error() = "Method not found: ";
            result.error()+= std::string(function);
            return result;
        }
        // Set the target object as the "this" context for the method call
        duk_dup(m_ctx, -2);  // Duplicate the target object below the method
        duk_remove(m_ctx, -3);  // Remove the original target object, leave duplicated as "this"
        // Push arguments onto the stack
        for (const auto& arg : args) {
            JSONtoDukValue(m_engine, m_ctx, arg, createdViews);
        }
        // Call the method
        if (duk_pcall_method(m_ctx, args.size()) != 0) {
            result.success() = false;
            result.error() = "Error calling function: ";
            result.error() += duk_safe_to_string(m_ctx, -1);
            duk_pop(m_ctx);  // Pop the error
            return result;
        }
    }

    // Convert the result back to nlohmann::json
    result.value() = dukValueToJSON(m_ctx, -1);

    // Pop the result from the stack
    duk_pop(m_ctx);

    return result;
}

/**
 * @see Backend::install.
 */
poesie::Result<bool> JavascriptVm::install(
        std::string_view name,
        ForeignFn function,
        size_t nargs) {
    poesie::Result<bool> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};

    // Allocate a new FunctionHolder and set its members
    auto holder = std::make_unique<FunctionHolder>(m_engine, std::move(function), nargs);
    auto holder_ptr = holder.get();

    // Push the function to the Duktape stack
    duk_push_c_function(m_ctx, FunctionHolder::binding, nargs);

    // Attach the FunctionHolder to the function (for access in the binding)
    duk_push_pointer(m_ctx, holder_ptr);
    duk_put_prop_string(m_ctx, -2, "\xff" "holder");

    // Set the function in the global scope with the given name
    duk_put_global_string(m_ctx, name.data());

    // Insert the unique_ptr into the map, for proper cleanup later
    m_ffuncs.insert(std::make_pair(
            std::string{name},
            std::move(holder)));

    return result;
}

duk_ret_t JavascriptVm::FunctionHolder::binding(duk_context* ctx) {
    // Retrieve the function holder
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, "\xff" "holder");
    FunctionHolder* holder = static_cast<FunctionHolder*>(duk_get_pointer(ctx, -1));
    duk_pop_2(ctx); // Pop the holder and the current_function

    // Check the number of arguments
    duk_idx_t arg_count = duk_get_top(ctx);
    if (arg_count != (decltype(arg_count))holder->expected_args) {
        return DUK_RET_ERROR;
    }

    // Prepare the arguments vector
    std::vector<json> args;
    args.reserve(arg_count);

    // Convert JavaScript arguments to JSON and add them to the vector
    for (duk_idx_t i = 0; i < arg_count; ++i) {
        args.push_back(dukValueToJSON(ctx, i));
    }

    // Call the C++ function
    json result = holder->func(args);

    // Convert the result back to a Duktape value
    std::vector<poesie::MemoryView> createdViews;
    JSONtoDukValue(holder->engine, ctx, result, createdViews);

    // Return the result
    return 1; // Number of return values
}

poesie::Result<bool> JavascriptVm::destroy() {
    poesie::Result<bool> result;
    result.value() = true;
    // or result.success() = true
    return result;
}

std::unique_ptr<poesie::Backend> JavascriptVm::create(const thallium::engine& engine, const json& config) {
    (void)engine;
    return std::unique_ptr<poesie::Backend>(new JavascriptVm(engine, config));
}
