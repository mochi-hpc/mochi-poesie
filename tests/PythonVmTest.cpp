/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>
#include "Ensure.hpp"
#include <poesie/Client.hpp>
#include <poesie/Provider.hpp>
#include <poesie/Backend.hpp>
#include <poesie/MemoryView.hpp>

TEST_CASE("Python vm test", "[python]") {
    auto engine = thallium::engine("na+sm", THALLIUM_SERVER_MODE);
    ENSURE(engine.finalize());
    const auto provider_config = R"(
    {
        "vm": {
            "type": "python",
            "config": {
                "preamble_file": "example-preamble.py"
            }
        }
    }
    )";
    poesie::Provider provider(engine, 42, provider_config);
    provider.getBackend()->install("my_print",
        [](poesie::Backend::ArgsType args) -> poesie::Backend::ReturnType {
            std::cout << "From foreign function: " << args[0].get<std::string>() << std::endl;
            return "Return from foreign function";
        }, 1);

    SECTION("Create VmHandle") {
        poesie::Client client(engine);
        std::string addr = engine.self();

        auto rh = client.makeVmHandle(addr, 42);

        SECTION("Execute code") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("print(\"Hello World\")"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_null());
            // python VM doesn't return a value
        }

        SECTION("Execute code with argv") {

            poesie::VmHandle::ArgsType argv = {"Matthieu"};
            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() {
                future = rh.execute("import sys; print(f\"Hello {sys.argv[1]}\")", argv); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_null());
            // python VM doesn't return a value
        }

        SECTION("Execute foreign function") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("my_print(\"Matthieu\")"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_null());
            // python VM doesn't return a value
        }

        SECTION("Execute code (bad syntax)") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("retu42 +33/"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Load a file") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("example.py"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_null());
            // python VM doesn't return a value
        }

        SECTION("Load a file (bad file)") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("bad.py"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Call function") {

            auto code = R"(
def transform(obj, v):
    # Increment x by v
    obj["x"] = obj["x"] + v
    # Prepend "hello " to y
    obj["y"] = "hello " + obj["y"]
    # Sort the array z
    obj["z"] = list(sorted(obj["z"]))
    # Switch the boolean t
    obj["t"] = not obj["t"]
    return obj)";

            REQUIRE_NOTHROW([&]() { rh.execute(code).wait(); }());

            poesie::VmHandle::ArgsType args(2);
            args[0] = nlohmann::json::object();
            args[0]["x"] = 42;
            args[0]["y"] = "Matthieu";
            args[0]["z"] = { 3, 6, 2, 8 };
            args[0]["t"] = true;
            args[1] = 3;

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.call("transform", "", args); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            auto content = result.dump();
            auto expected = std::string{
                R"({"t":false,"x":45,"y":"hello Matthieu","z":[2,3,6,8]})"};
            REQUIRE(content == expected);
        }

        SECTION("Call method on object") {

            REQUIRE_NOTHROW([&]() { rh.load("example-my-class.py").wait(); }());

            poesie::VmHandle::ArgsType args(1);
            args[0] = 3;

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.call("transform", "my_obj", args); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            auto content = result.dump();
            auto expected = std::string{
                R"({"t":false,"x":13,"y":"hello world","z":[1,2,3]})"};
            REQUIRE(content == expected);
        }

        SECTION("Use MemoryView") {

            auto code = R"(
def use_memory_view(view):
    print(type(view))
    assert str(view, 'utf8') == 'ABCDEFGHIJKLMNOP'
    for i in range(len(view)):
        view[i] = 97 + (i % 26)
    )";

            REQUIRE_NOTHROW([&]() { rh.execute(code).wait(); }());

            std::string data = "ABCDEFGHIJKLMNOP";

            poesie::VmHandle::ArgsType args(1);
            args[0] = poesie::MemoryView{
                engine, data.data(), data.size(),
                poesie::MemoryView::Intent::INOUT};

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.call("use_memory_view", "", args); }());

            REQUIRE_NOTHROW(future.wait());
            REQUIRE(data == "abcdefghijklmnop");
        }

    }
}
