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

TEST_CASE("Lua vm test", "[lua]") {
    auto engine = thallium::engine("na+sm", THALLIUM_SERVER_MODE);
    ENSURE(engine.finalize());
    const auto provider_config = R"(
    {
        "vm": {
            "type": "lua",
            "config": {
                "preamble_file": "example-preamble.lua"
            }
        }
    }
    )";
    poesie::Provider provider(engine, 42, provider_config);
    provider.getBackend()->install("my_mult",
        [](poesie::Backend::ArgsType args) -> poesie::Backend::ReturnType {
            return args[0].get<int>() * args[1].get<int>();
        }, 2);

    SECTION("Create VmHandle") {
        poesie::Client client(engine);
        std::string addr = engine.self();

        auto rh = client.makeVmHandle(addr, 42);

        SECTION("Execute code") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("return my_add(42, 33)"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Execute code with argv") {

            poesie::VmHandle::ArgsType argv = {42};
            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("return arg[1] + 33", argv); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Execute foreign function") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("return my_mult(42, 33)"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 1386);
        }

        SECTION("Execute code (bad syntax)") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("retu42 +33/"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Execute code with error") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() {
                future = rh.execute(R"(
                    function my_error_function()
                        error("This is an error")
                    end
                    my_error_function()
                )"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Load a file") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("example.lua"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Load a file (bad file)") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("bad.lua"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Return JSON data") {

            auto code = R"(
            my_var = {
                x = 42,
                y = 4.2,
                z = true,
                a = {2, 4, 6, 8},
                b = {
                    c = "abcd",
                    d = "efgh"
                },
                e = nil
            }
            return my_var

            )";

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute(code); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_object());
            auto content = result.dump();
            auto expected = std::string{
                R"({"a":[2.0,4.0,6.0,8.0],"b":{"c":"abcd","d":"efgh"},"x":42.0,"y":4.2,"z":true})"};
            REQUIRE(content == expected);
        }

        SECTION("Call function") {

            auto code = R"(
            function transform(obj, v)
                -- Increment x by v
                obj.x = obj.x + v
                -- Prepend "hello " to y
                obj.y = "hello " .. obj.y
                -- Sort the array z
                table.sort(obj.z)
                -- Switch the boolean t
                obj.t = not obj.t
                return obj
            end
            )";

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
                R"({"t":false,"x":45.0,"y":"hello Matthieu","z":[2.0,3.0,6.0,8.0]})"};
            REQUIRE(content == expected);
        }

        SECTION("Call method on object") {

            REQUIRE_NOTHROW([&]() { rh.load("example-my-class.lua").wait(); }());

            poesie::VmHandle::ArgsType args(1);
            args[0] = 3;

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.call("transform", "my_obj", args); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            auto content = result.dump();
            auto expected = std::string{
                R"({"t":false,"x":13.0,"y":"hello world","z":[1.0,2.0,3.0]})"};
            REQUIRE(content == expected);
        }

        SECTION("Use MemoryView") {

            auto code = R"(
            function use_memory_view(view)
                assert(memory.type(view) == "other", "invalid memory type")
                assert(memory.tostring(view) == "ABCDEFGHIJKLMNOP", "invalid memory content")
                memory.fill(view, "abcdefghijklmnop")
            end
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
