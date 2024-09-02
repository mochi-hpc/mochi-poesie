/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "RubyBackend.hpp"
#include "RubyMemoryView.hpp"
#include "poesie/MemoryView.hpp"
#include <fstream>
#include <iostream>
#include <mruby/error.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/value.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/data.h>
#include <mruby/class.h>

static nlohmann::json mrb_value_to_json(mrb_state* mrb, mrb_value val) {
    nlohmann::json json_obj;

    // Check the type of mrb_value and convert accordingly
    if (mrb_nil_p(val)) {
        json_obj = nullptr;
    } else if (mrb_fixnum_p(val)) {
        json_obj = mrb_fixnum(val);
    } else if (mrb_float_p(val)) {
        json_obj = mrb_float(val);
    } else if (mrb_true_p(val)) {
        json_obj = true;
    } else if (mrb_false_p(val)) {
        json_obj = false;
    } else if (mrb_string_p(val)) {
        json_obj = mrb_str_to_cstr(mrb, val);
    } else if (mrb_array_p(val)) {
        json_obj = nlohmann::json::array();
        mrb_int len = RARRAY_LEN(val);
        for (mrb_int i = 0; i < len; i++) {
            mrb_value elem = mrb_ary_ref(mrb, val, i);
            json_obj.push_back(mrb_value_to_json(mrb, elem));
        }
    } else if (mrb_hash_p(val)) {
        json_obj = nlohmann::json::object();
        mrb_value keys = mrb_hash_keys(mrb, val);
        mrb_int len = RARRAY_LEN(keys);
        for (mrb_int i = 0; i < len; i++) {
            mrb_value key = mrb_ary_ref(mrb, keys, i);
            mrb_value value = mrb_hash_get(mrb, val, key);
            json_obj[mrb_str_to_cstr(mrb, key)] = mrb_value_to_json(mrb, value);
        }
    } else {
        // Handle other types if necessary, or set to null
        json_obj = nullptr;
    }

    return json_obj;
}

static mrb_value json_to_mrb_value(
        const thallium::engine& engine,
        mrb_state* mrb,
        RClass* memoryview_class,
        const nlohmann::json& val,
        std::vector<poesie::MemoryView>& createdViews) {
    if (val.is_null()) {
        return mrb_nil_value();
    } else if (val.is_number_integer()) {
        return mrb_fixnum_value(val.get<int>());
    } else if (val.is_number_float()) {
        return mrb_float_value(mrb, val.get<double>());
    } else if (val.is_boolean()) {
        return val.get<bool>() ? mrb_true_value() : mrb_false_value();
    } else if (val.is_string()) {
        return mrb_str_new_cstr(mrb, val.get<std::string>().c_str());
    } else if (val.is_array()) {
        mrb_value ary = mrb_ary_new(mrb);
        for (const auto& item : val) {
            mrb_value mrb_item = json_to_mrb_value(engine, mrb, memoryview_class, item, createdViews);
            mrb_ary_push(mrb, ary, mrb_item);
        }
        return ary;
    } else if (val.is_object()) {
        mrb_value hash = mrb_hash_new(mrb);
        for (const auto& [key, item] : val.items()) {
            mrb_value mrb_key = mrb_str_new_cstr(mrb, key.c_str());
            mrb_value mrb_value = json_to_mrb_value(engine, mrb, memoryview_class, item, createdViews);
            mrb_hash_set(mrb, hash, mrb_key, mrb_value);
        }
        return hash;
    } else if (val.is_binary()) {
        if(!poesie::MemoryView::IsMemoryView(val)) {
            auto& b = val.get_binary();
            return mrb_str_new(mrb, (char*)b.data(), b.size());
        }
        // else, this is a MemoryView object
        auto view = poesie::MemoryView{engine, val};
        createdViews.push_back(view);
        return memory_view_new(mrb, memoryview_class, view);
    } else {
        // Handle other types or throw an error if needed
        return mrb_nil_value();
    }
}

POESIE_REGISTER_BACKEND(ruby, RubyVm);

RubyVm::RubyVm(thallium::engine engine, const json& config)
: m_engine(std::move(engine))
, m_config(config) {
    m_mrb = mrb_open();
    if (!m_mrb) {
        throw poesie::Exception{"Failed to initialize mruby"};
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
    m_memoryview_class = mrb_mruby_memory_view_gem_init(m_mrb);
}

RubyVm::~RubyVm() {
    if (m_mrb) {
        mrb_close(m_mrb);
    }
}

std::string RubyVm::getConfig() const {
    return m_config.dump();
}

poesie::Result<json> RubyVm::execute(
        std::string_view code,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    std::vector<poesie::MemoryView> createdViews;
    // Install ARGV
    mrb_value ARGV = mrb_ary_new(m_mrb);
    mrb_ary_push(m_mrb, ARGV, mrb_str_new_lit(m_mrb, "poesie"));
    for (size_t i = 0; i < args.size(); i++) {
        mrb_ary_push(m_mrb, ARGV,
            json_to_mrb_value(m_engine, m_mrb, m_memoryview_class, args[i], createdViews));
    }
    mrb_const_set(m_mrb, mrb_obj_value(m_mrb->object_class), mrb_intern_lit(m_mrb, "ARGV"), ARGV);
    mrb_value ret = mrb_load_string(m_mrb, code.data());
    if (m_mrb->exc) {
        mrb_value exc = mrb_obj_value(m_mrb->exc);
        mrb_value exc_message = mrb_funcall(m_mrb, exc, "inspect", 0);
        const char* error_message = mrb_str_to_cstr(m_mrb, exc_message);
        m_mrb->exc = nullptr;
        result.success() = false;
        result.error() = "Error executing Ruby code: ";
        result.error() += error_message;
    } else {
        result.value() = mrb_value_to_json(m_mrb, ret);
    }
    return result;
}

poesie::Result<json> RubyVm::load(
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

poesie::Result<nlohmann::json> RubyVm::call(
        std::string_view function,
        std::string_view target,
        const std::vector<json>& args) {
    poesie::Result<nlohmann::json> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};
    std::vector<poesie::MemoryView> createdViews;
    // Convert JSON arguments to mrb_value
    std::vector<mrb_value> mrb_args(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        mrb_args[i] = json_to_mrb_value(m_engine, m_mrb, m_memoryview_class, args[i], createdViews);
    }
    mrb_value ret;
    if (target.empty()) {
        // Target is empty, call a global function
        mrb_sym sym = mrb_intern_cstr(m_mrb, function.data());
        ret = mrb_funcall_argv(m_mrb, mrb_top_self(m_mrb), sym, args.size(), mrb_args.data());
    } else {
        // Target is not empty, call a method on the target object
        mrb_sym target_sym = mrb_intern_cstr(m_mrb, target.data());
        mrb_value target_obj = mrb_gv_get(m_mrb, target_sym);
        mrb_sym function_sym = mrb_intern_cstr(m_mrb, function.data());
        ret = mrb_funcall_argv(m_mrb, target_obj, function_sym, args.size(), mrb_args.data());
    }

    // Check for Ruby exceptions
    if (m_mrb->exc) {
        // Ruby exception occurred
        mrb_value exc = mrb_obj_value(m_mrb->exc);
        mrb_value exc_message = mrb_funcall(m_mrb, exc, "inspect", 0);
        const char* error_message = mrb_str_to_cstr(m_mrb, exc_message);
        result.success() = false;
        result.error() = "Error executing Ruby code: ";
        result.error() += error_message;
        m_mrb->exc = nullptr;
    } else {
        // Convert the result from mrb_value to JSON
        result.value() = mrb_value_to_json(m_mrb, ret);
    }
    return result;
}

static auto make_iv_name(const char* name) {
    return std::string{"__"} + name + "__";
};

/**
 * @see Backend::install.
 */
poesie::Result<bool> RubyVm::install(
        std::string_view name,
        ForeignFn func,
        size_t nargs) {
    poesie::Result<bool> result;
    std::unique_lock<thallium::mutex> guard{m_mtx};

    struct FuncInfo {
        thallium::engine engine;
        RClass*          memoryview_class;
        ForeignFn        func;
    };

    static const mrb_data_type func_data_type = {
        "CppFunction",
        [](mrb_state *mrb, void *p) {
            (void)mrb;
            delete static_cast<FuncInfo*>(p);
        }
    };

    auto resolver = [](mrb_state *mrb, mrb_value self) -> mrb_value {
        // get module or class containing the function
        RClass* mod = mrb_class(mrb, self);
        mrb_value mod_val = mrb_obj_value(mod);
        // extract arguments
        mrb_value *argv;
        mrb_int argc;
        mrb_get_args(mrb, "*", &argv, &argc);
        // convert them into json
        std::vector<json> jargs;
        jargs.reserve(argc);
        for(int i = 0; i < argc; ++i) {
            jargs.push_back(mrb_value_to_json(mrb, argv[i]));
        }
        // get current function name
        mrb_sym func_name_sym = mrb_get_mid(mrb);
        mrb_int func_name_len = 0;
        const char* func_name_cstr = mrb_sym_name_len(mrb, func_name_sym, &func_name_len);
        // make name of the instance variable containing function pointer
        auto func_iv_name = make_iv_name(func_name_cstr);
        // retrieve the function pointer
        auto func_iv_sym = mrb_intern_cstr(mrb, func_iv_name.c_str());
        auto func_value = mrb_cv_get(mrb, mod_val, func_iv_sym);
        auto func_info = static_cast<FuncInfo*>(mrb_data_get_ptr(mrb, func_value, &func_data_type));
        // call the function
        auto result = func_info->func(jargs);
        // return the converted json value
        std::vector<poesie::MemoryView> createdViews;
        return json_to_mrb_value(
            func_info->engine, mrb, func_info->memoryview_class, result, createdViews);
    };
    auto func_info    = new FuncInfo{m_engine, m_memoryview_class, std::move(func)};
    auto func_data    = mrb_data_object_alloc(m_mrb, m_mrb->object_class, func_info, &func_data_type);
    auto func_value   = mrb_obj_value(func_data);
    auto func_iv_name = make_iv_name(name.data());
    auto func_iv_sym  = mrb_intern_cstr(m_mrb, func_iv_name.c_str());
    mrb_mod_cv_set(m_mrb, m_mrb->kernel_module, func_iv_sym, func_value);
    mrb_define_module_function(m_mrb, m_mrb->kernel_module, name.data(), resolver, MRB_ARGS_REQ(nargs));
    return result;
}

poesie::Result<bool> RubyVm::destroy() {
    poesie::Result<bool> result;
    result.value() = true;
    // or result.success() = true
    return result;
}

std::unique_ptr<poesie::Backend> RubyVm::create(const thallium::engine& engine, const json& config) {
    (void)engine;
    return std::unique_ptr<poesie::Backend>(new RubyVm(engine, config));
}
