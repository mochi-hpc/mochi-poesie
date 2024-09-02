/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <poesie/MemoryView.hpp>
#include "PythonBackend.hpp"
#include <pybind11/stl.h>
#include <fstream>
#include <iostream>

namespace py = pybind11;
using json = nlohmann::json;

static inline py::object from_json(
        const thallium::engine& engine, const json& j,
        std::vector<poesie::MemoryView>& createdViews) {
    if (j.is_null()) {
        return py::none();
    } else if (j.is_boolean()) {
        return py::bool_(j.get<bool>());
    } else if (j.is_number_unsigned()) {
        return py::int_(j.get<json::number_unsigned_t>());
    } else if (j.is_number_integer()) {
        return py::int_(j.get<json::number_integer_t>());
    } else if (j.is_number_float()) {
        return py::float_(j.get<double>());
    } else if (j.is_string()) {
        return py::str(j.get<std::string>());
    } else if (j.is_array()) {
        py::list obj(j.size());
        for (std::size_t i = 0; i < j.size(); i++) {
            obj[i] = from_json(engine, j[i], createdViews);
        }
        return obj;
    } else if (j.is_object()) {
        py::dict obj;
        for (auto& p : j.items()) {
            obj[py::str(p.key())] = from_json(engine, p.value(), createdViews);
        }
        return obj;
    } else if (j.is_binary()) {
        if(!poesie::MemoryView::IsMemoryView(j)) {
            auto& b = j.get_binary();
            return py::bytes((char*)b.data(), b.size());
        }
        // else, this is a MemoryView object
        auto view = poesie::MemoryView{engine, j};
        createdViews.push_back(view);
        return py::memoryview::from_memory((void*)view.data(), (ssize_t)view.size(), false);
    }
    return py::none();
}

static inline json to_json(const py::handle& obj)
{
    if (obj.ptr() == nullptr || obj.is_none()) {
        return nullptr;
    }
    if (py::isinstance<py::bool_>(obj)) {
        return obj.cast<bool>();
    }
    if (py::isinstance<py::int_>(obj)) {
        try {
            auto s = obj.cast<json::number_integer_t>();
            if (py::int_(s).equal(obj)) {
                return s;
            }
        } catch (...) {}
        try {
            auto u = obj.cast<json::number_unsigned_t>();
            if (py::int_(u).equal(obj)) {
                return u;
            }
        } catch (...) {}
        throw poesie::Exception{
            "to_json received an integer out of range for both "
            "json::number_integer_t and json::number_unsigned_t type: "
            + py::repr(obj).cast<std::string>()};
    }
    if (py::isinstance<py::float_>(obj)) {
        return obj.cast<double>();
    }
    if (py::isinstance<py::bytes>(obj)) {
        py::module base64 = py::module::import("base64");
        return base64.attr("b64encode")(obj).attr("decode")("utf-8").cast<std::string>();
    }
    if (py::isinstance<py::str>(obj)) {
        return obj.cast<std::string>();
    }
    if (py::isinstance<py::tuple>(obj) || py::isinstance<py::list>(obj)) {
        auto out = json::array();
        for (const py::handle value : obj) {
            out.push_back(to_json(value));
        }
        return out;
    }
    if (py::isinstance<py::dict>(obj)) {
        auto out = json::object();
        for (const py::handle key : obj) {
            out[py::str(key).cast<std::string>()] = to_json(obj[key]);
        }
        return out;
    }
    throw poesie::Exception{
        "to_json not implemented for this type of object: "
        + py::repr(obj).cast<std::string>()};
}

POESIE_REGISTER_BACKEND(python, PythonVm);

ABT_mutex_memory PythonVm::s_mtx = ABT_MUTEX_INITIALIZER;

PythonVm::PythonVm(thallium::engine engine, const json& config)
: m_engine(std::move(engine))
, m_config(config)
, m_mtx(ABT_MUTEX_MEMORY_GET_HANDLE(&s_mtx))
, m_main_module(py::module::import("__main__"))
, m_main_namespace(m_main_module.attr("__dict__"))
{
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

std::string PythonVm::getConfig() const {
    return m_config.dump();
}

poesie::Result<json> PythonVm::execute(
        std::string_view code,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    ABT_mutex_lock(m_mtx);
    try {
        auto sys = py::module::import("sys");
        py::list argv;
        argv.append(py::str("poesie"));
        std::vector<poesie::MemoryView> createdViews;
        for(auto& arg : args) argv.append(from_json(m_engine, arg, createdViews));
        sys.attr("argv") = argv;
        py::exec(code.data(), m_main_namespace);
    } catch (const py::error_already_set &e) {
        result.success() = false;
        result.error() = "Error running Python code: ";
        result.error() += e.what();
    }
    ABT_mutex_unlock(m_mtx);
    return result;
}

poesie::Result<json> PythonVm::load(
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
    result = execute(buffer.str(), args);
    return result;
}

poesie::Result<json> PythonVm::call(
        std::string_view function,
        std::string_view target,
        const std::vector<json>& args) {
    poesie::Result<json> result;
    ABT_mutex_lock(m_mtx);
    py::object pytarget;
    if(target.empty()) {
        pytarget = m_main_module;
    } else {
        if(!m_main_namespace.contains(target.data())) {
            result.success() = false;
            result.error() = "Could not find target ";
            result.error() += target;
            ABT_mutex_unlock(m_mtx);
            return result;
        }
        pytarget = m_main_namespace[target.data()];
    }
    if(!py::hasattr(pytarget, function.data())) {
        result.success() = false;
        result.error() = "Could not find function or method ";
        result.error() += function;
        ABT_mutex_unlock(m_mtx);
        return result;
    }
    py::tuple pyargs(args.size());
    std::vector<poesie::MemoryView> createdViews;
    for(unsigned i=0; i < args.size(); ++i) {
        pyargs[i] = from_json(m_engine, args[i], createdViews);
    }
    try {
        py::object ret = pytarget.attr(function.data())(*pyargs);
        result.value() = to_json(ret);
    } catch(const py::error_already_set &e) {
        result.success() = false;
        result.error() = "Error running Python code: ";
        result.error() += e.what();
    }
    ABT_mutex_unlock(m_mtx);
    return result;
}

/**
 * @see Backend::install.
 */
poesie::Result<bool> PythonVm::install(
        std::string_view name,
        ForeignFn function,
        size_t nargs) {
    poesie::Result<bool> result;
    ABT_mutex_lock(m_mtx);
    try {
        m_main_namespace[name.data()] = py::cpp_function{
            [func=std::move(function), nargs, this](py::args args) {
                std::vector<json> jargs;
                if(nargs != args.size()) {
                    throw std::runtime_error("Invalid number of arguments");
                }
                jargs.reserve(args.size());
                for(auto& arg : args) {
                    jargs.push_back(to_json(arg));
                }
                std::vector<poesie::MemoryView> createdViews;
                return from_json(m_engine, func(jargs), createdViews);
            }};
    } catch(const py::error_already_set &e) {
        result.success() = false;
        result.error() = "Could not install foreign function ";
        result.error() += name;
        result.error() += ": ";
        result.error() += e.what();
    }
    ABT_mutex_unlock(m_mtx);
    return result;
}

poesie::Result<bool> PythonVm::destroy() {
    poesie::Result<bool> result;
    result.value() = true;
    // or result.success() = true
    return result;
}

std::unique_ptr<poesie::Backend> PythonVm::create(const thallium::engine& engine, const json& config) {
    (void)engine;
    return std::unique_ptr<poesie::Backend>(new PythonVm(engine, config));
}
