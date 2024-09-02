/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "Jx9Backend.hpp"
#include "poesie/MemoryView.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <stdexcept>

/**
 * Converts a jx9_value into a nlohmann::json object.
 */
static nlohmann::json Jx9ValueToJSON(jx9_value* value) {
    if (jx9_value_is_null(value)) {
        return nullptr;
    } else if (jx9_value_is_bool(value)) {
        return jx9_value_to_bool(value) != 0;
    } else if (jx9_value_is_int(value)) {
        return jx9_value_to_int(value);
    } else if (jx9_value_is_float(value)) {
        return jx9_value_to_double(value);
    } else if (jx9_value_is_string(value)) {
        return std::string(jx9_value_to_string(value, nullptr));
    } else if (jx9_value_is_resource(value)) {
        std::vector<uint8_t> b(sizeof(intptr_t));
        intptr_t ptr = (intptr_t)jx9_value_to_resource(value);
        std::memcpy(b.data(), &ptr, sizeof(ptr));
        return nlohmann::json::binary_t(b);
    } else if (jx9_value_is_json_object(value)) {
        nlohmann::json jsonObject = nlohmann::json::object();

        auto object_walk = [](jx9_value* pKey, jx9_value* pValue, void* pUserData) -> int {
            nlohmann::json* jsonObject = static_cast<nlohmann::json*>(pUserData);
            std::string key = jx9_value_to_string(pKey, nullptr);
            (*jsonObject)[key] = Jx9ValueToJSON(pValue);
            return JX9_OK;
        };

        jx9_array_walk(value, object_walk, &jsonObject);
        return jsonObject;
    } else if (jx9_value_is_json_array(value)) {
        nlohmann::json jsonArray = nlohmann::json::array();

        auto array_walk = [](jx9_value* pKey, jx9_value* pValue, void* pUserData) -> int {
            (void)pKey;
            nlohmann::json* jsonArray = static_cast<nlohmann::json*>(pUserData);
            jsonArray->push_back(Jx9ValueToJSON(pValue));
            return JX9_OK;
        };

        jx9_array_walk(value, array_walk, &jsonArray);
        return jsonArray;
    } else {
        return nullptr;
    }
}

/**
 * Converts a nlohmann::json object into a jx9_value (from outside VM).
 */
static jx9_value* JSONtoJx9Value(
        const thallium::engine& engine,
        jx9_vm* vm,
        const nlohmann::json& data,
        std::vector<std::unique_ptr<poesie::MemoryView>>& createdViews) {
    if (data.is_null()) {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_null(value);
        return value;
    } else if (data.is_boolean()) {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_bool(value, data.get<bool>());
        return value;
    } else if (data.is_number_integer()) {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_int(value, data.get<int>());
        return value;
    } else if (data.is_number_float()) {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_double(value, data.get<double>());
        return value;
    } else if (data.is_string()) {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_string(value, data.get<std::string>().c_str(), -1);
        return value;
    } else if (data.is_array()) {
        jx9_value* arrayValue = jx9_new_array(vm);
        for (const auto& item : data) {
            jx9_array_add_elem(arrayValue, nullptr,\
                JSONtoJx9Value(engine, vm, item, createdViews));
        }
        return arrayValue;
    } else if (data.is_object()) {
        jx9_value* objectValue = jx9_new_array(vm);
        for (auto it = data.begin(); it != data.end(); ++it) {
            jx9_value* entry = JSONtoJx9Value(engine, vm, it.value(), createdViews);
            jx9_array_add_strkey_elem(objectValue, it.key().c_str(), entry);
        }
        return objectValue;
    } else if(data.is_binary()) {
        jx9_value* value = jx9_new_scalar(vm);;
        if(!poesie::MemoryView::IsMemoryView(data)) {
            auto& b = data.get_binary();
            jx9_value_string(value, (char*)b.data(), b.size());
            return value;
        }
        // else, this is a MemoryView object
        auto view = std::make_unique<poesie::MemoryView>(engine, data);
        jx9_value_resource(value, view.get());
        createdViews.push_back(std::move(view));
        return value;
    } else {
        jx9_value* value = jx9_new_scalar(vm);
        jx9_value_null(value);
        return value;
    }
}

/**
 * Converts a nlohmann::json object into a jx9_value (from inside VM).
 */
static jx9_value* JSONtoJx9Value(
        jx9_context* ctx,
        const nlohmann::json& data) {
    if (data.is_null()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_null(value);
        return value;
    } else if (data.is_boolean()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_bool(value, data.get<bool>());
        return value;
    } else if (data.is_number_integer()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_int(value, data.get<int>());
        return value;
    } else if (data.is_number_float()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_double(value, data.get<double>());
        return value;
    } else if (data.is_string()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_string(value, data.get<std::string>().c_str(), -1);
        return value;
    } else if (data.is_array()) {
        jx9_value* arrayValue = jx9_context_new_array(ctx);
        for (const auto& item : data) {
            jx9_array_add_elem(arrayValue, nullptr,
                JSONtoJx9Value(ctx, item));
        }
        return arrayValue;
    } else if (data.is_object()) {
        jx9_value* objectValue = jx9_context_new_array(ctx);
        for (auto it = data.begin(); it != data.end(); ++it) {
            jx9_value* entry = JSONtoJx9Value(ctx, it.value());
            jx9_array_add_strkey_elem(objectValue, it.key().c_str(), entry);
        }
        return objectValue;
    } else if(data.is_binary()) {
        jx9_value* value = jx9_context_new_scalar(ctx);
        auto& b = data.get_binary();
        jx9_value_string(value, (char*)b.data(), b.size());
        return value;
    } else {
        jx9_value* value = jx9_context_new_scalar(ctx);
        jx9_value_null(value);
        return value;
    }
}

static inline nlohmann::json MemoryView_length(const std::vector<nlohmann::json>& argv) {
    if(!argv[0].is_binary()) return nullptr;
    auto& binary = argv[0].get_binary();
    if(binary.size() != sizeof(intptr_t)) return nullptr;
    poesie::MemoryView* view = nullptr;
    std::memcpy(&view, binary.data(), sizeof(view));
    if(!view) return nullptr;
    return view->size();
}

static inline nlohmann::json MemoryView_to_string(const std::vector<nlohmann::json>& argv) {
    if(!argv[0].is_binary()) return nullptr;
    auto& binary = argv[0].get_binary();
    if(binary.size() != sizeof(intptr_t)) return nullptr;
    poesie::MemoryView* view = nullptr;
    std::memcpy(&view, binary.data(), sizeof(view));
    if(!view) return nullptr;
    return std::string{view->data(), view->size()};
}

static inline nlohmann::json MemoryView_get(const std::vector<nlohmann::json>& argv) {
    if(!argv[0].is_binary()) return nullptr;
    if(!argv[1].is_number()) return nullptr;
    auto i = argv[1].get<int64_t>();
    auto& binary = argv[0].get_binary();
    if(binary.size() != sizeof(intptr_t)) return nullptr;
    poesie::MemoryView* view = nullptr;
    std::memcpy(&view, binary.data(), sizeof(view));
    if(!view) return nullptr;
    if(i < 0 || i >= (ssize_t)view->size()) return nullptr;
    return int(view->data()[i]);
}

static inline nlohmann::json MemoryView_set(const std::vector<nlohmann::json>& argv) {
    if(!argv[0].is_binary()) return false;
    if(!argv[1].is_number()) return false;
    if(!argv[2].is_number()) return false;
    auto i = argv[1].get<int64_t>();
    auto b = argv[2].get<int64_t>();
    if(b < 0 || b > 255) return false;
    auto& binary = argv[0].get_binary();
    if(binary.size() != sizeof(intptr_t)) return false;
    poesie::MemoryView* view = nullptr;
    std::memcpy(&view, binary.data(), sizeof(view));
    if(!view) return false;
    if(i < 0 || i >= (int64_t)view->size()) return false;
    view->data()[i] = (char)b;
    return true;
}

POESIE_REGISTER_BACKEND(jx9, Jx9Vm);

Jx9Vm::Jx9Vm(thallium::engine engine, const json& config)
: m_engine(std::move(engine))
, m_config(config) {
    int rc = jx9_init(&m_jx9_engine);
    if (rc != JX9_OK) {
        throw poesie::Exception("Failed to initialize Jx9 engine");
    }
    if(m_config.is_object()) {
        if(m_config.contains("preamble_file") && m_config["preamble_file"].is_string()) {
            auto& filename = m_config["preamble_file"].get_ref<const std::string&>();
            std::ifstream file{filename};
            if (!file) {
                throw poesie::Exception{"Could not open preamble file"};
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            m_preamble += buffer.str();
        }
        if(m_config.contains("preamble") && m_config["preamble"].is_string()) {
            m_preamble += "\n" + m_config["preamble"].get<std::string>();
        }
    }
    install("memory_view_length", MemoryView_length, 1);
    install("memory_view_to_string", MemoryView_to_string, 1);
    install("memory_view_get", MemoryView_get, 2);
    install("memory_view_set", MemoryView_set, 3);
}

Jx9Vm::~Jx9Vm() {
    if (m_jx9_engine) {
        jx9_release(m_jx9_engine);
    }
}

std::string Jx9Vm::getConfig() const {
    return m_config.dump();
}

poesie::Result<json> Jx9Vm::execute(
        std::string_view code,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    std::vector<std::unique_ptr<poesie::MemoryView>> createdViews;

    int rc;
    jx9_vm* pJx9VM = nullptr;
    if(m_preamble.empty()) {
        rc = jx9_compile(m_jx9_engine, code.data(), code.size(), &pJx9VM);
    } else {
        auto full_code = m_preamble;
        full_code.append(code.data(), 0, code.size());
        rc = jx9_compile(m_jx9_engine, full_code.data(), full_code.size(), &pJx9VM);
    }
    if (rc != JX9_OK) {
        const char* errBuf;
        int iLen;
        jx9_config(m_jx9_engine, JX9_CONFIG_ERR_LOG, &errBuf, &iLen);
        result.success() = false;
        result.error() =  "Failed to compile Jx9 code: ";
        result.error() += errBuf;
        return result;
    }

    // Install __global__ variable
    jx9_value* pGlobal = JSONtoJx9Value(m_engine, pJx9VM, m_global, createdViews);
    if(pGlobal) {
        rc = jx9_vm_config(pJx9VM, JX9_VM_CONFIG_CREATE_VAR, "__global__", pGlobal);
        // Release the jx9_value as it has been copied into the VM
        jx9_release_value(pJx9VM, pGlobal);
        if (rc != JX9_OK) {
            jx9_vm_release(pJx9VM);
            result.success() = false;
            result.error() = "Failed to install __global__ variable in JX9 VM";
            return result;
        }
    } else {
        jx9_vm_release(pJx9VM);
        result.success() = false;
        result.error() = "Failed to create __global__ variable";
        return result;
    }

    // Install __argv__ variable
    std::vector<json> argv;
    argv.push_back("poesie");
    argv.insert(argv.end(), args.begin(), args.end());
    jx9_value* pArgv = JSONtoJx9Value(m_engine, pJx9VM, argv, createdViews);
    if(pArgv) {
        rc = jx9_vm_config(pJx9VM, JX9_VM_CONFIG_CREATE_VAR, "__argv__", pArgv);
        // Release the jx9_value as it has been copied into the VM
        jx9_release_value(pJx9VM, pArgv);
        if (rc != JX9_OK) {
            jx9_vm_release(pJx9VM);
            result.success() = false;
            result.error() = "Failed to install __argv__ variable in JX9 VM";
            return result;
        }
    } else {
        jx9_vm_release(pJx9VM);
        result.success() = false;
        result.error() = "Failed to create __argv__ variable";
        return result;
    }

    for(auto& p : m_ffuncs) {
        auto holder_ptr = p.second.get();
        auto& name = p.first;
        rc = jx9_create_function(pJx9VM, name.c_str(), FunctionHolder::binding, holder_ptr);
        if (rc != JX9_OK) {
            result.success() = false;
            result.error() =  "Failed to install foreign function: ";
            result.error() += name;
            jx9_vm_release(pJx9VM);
            return result;
        }
    }

    // Execute the script
    rc = jx9_vm_exec(pJx9VM, nullptr);
    if (rc != JX9_OK) {
        const char* errBuf;
        int iLen;
        jx9_config(m_jx9_engine, JX9_CONFIG_ERR_LOG, &errBuf, &iLen);
        result.success() = false;
        result.error() =  "Failed to execute Jx9 code: ";
        result.error() += errBuf;
        jx9_vm_release(pJx9VM);
        return result;
    }

    // Extract VM return value
    jx9_value* ret_value;
    rc = jx9_vm_config(pJx9VM, JX9_VM_CONFIG_EXEC_VALUE, &ret_value);
    if (rc != JX9_OK) {
        result.success() = false;
        result.error() = "Could not extract return value from Jx9 VM";
        jx9_vm_release(pJx9VM);
        return result;
    }
    result.value() = Jx9ValueToJSON(ret_value);

    // Try to extract the __global__ variable
    pGlobal = jx9_vm_extract_variable(pJx9VM, "__global__");
    if (pGlobal != nullptr) {
        m_global = Jx9ValueToJSON(pGlobal);
        jx9_release_value(pJx9VM, pGlobal);
    } else {
        m_global = json::object();
    }

    // Release the VM after execution
    jx9_vm_release(pJx9VM);
    return result;
}

poesie::Result<json> Jx9Vm::load(
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

poesie::Result<json> Jx9Vm::call(
        std::string_view function,
        std::string_view target,
        const std::vector<nlohmann::json>& args) {
    poesie::Result<json> result;
    std::stringstream code;
    code << "return "
         << function
         << '(';
    if(!target.empty())
        code << target << ',';
    for(size_t i = 0; i < args; ++i) {
        code << args[i].dump() << (i == args.size()-1 ? ' ' : ',');
    }
    code << ");";
    return execute(code.str(), {});
}

/**
 * @see Backend::install.
 */
poesie::Result<bool> Jx9Vm::install(
        std::string_view name,
        ForeignFn function,
        size_t nargs) {
    poesie::Result<bool> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    auto holder = std::make_unique<FunctionHolder>(std::move(function), nargs);
    m_ffuncs.insert(std::make_pair(std::string{name}, std::move(holder)));
    return result;
}

poesie::Result<bool> Jx9Vm::destroy() {
    poesie::Result<bool> result;
    result.value() = true;
    // or result.success() = true
    return result;
}

std::unique_ptr<poesie::Backend> Jx9Vm::create(const thallium::engine& engine, const json& config) {
    (void)engine;
    return std::unique_ptr<poesie::Backend>(new Jx9Vm(engine, config));
}

int Jx9Vm::FunctionHolder::binding(jx9_context* pCtx, int argc, jx9_value** argv) {
    auto holder_ptr = static_cast<FunctionHolder*>(jx9_context_user_data(pCtx));
    if (!holder_ptr) {
        jx9_context_throw_error(pCtx, JX9_CTX_ERR, "Internal function holder is null");
        return JX9_CTX_ERR;
    }
    if ((unsigned)argc != holder_ptr->expected_args) {
        jx9_context_throw_error(pCtx, JX9_CTX_ERR, "Invalid number of arguments");
        return JX9_CTX_ERR;
    }
    auto& func = holder_ptr->func;

    // Convert Jx9 values to JSON
    std::vector<json> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(Jx9ValueToJSON(argv[i]));
    }

    // Call the C++ function
    json result = func(args);

    // Convert the result back to a Jx9 value
    jx9_value* pResult = JSONtoJx9Value(pCtx, result);
    jx9_result_value(pCtx, pResult);

    return JX9_OK;
}
