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

TEST_CASE("Jx9 vm test", "[jx9]") {
    auto engine = thallium::engine("na+sm", THALLIUM_SERVER_MODE);
    ENSURE(engine.finalize());
    const auto provider_config = R"(
    {
        "vm": {
            "type": "jx9",
            "config": {
                "preamble_file": "example-preamble.jx9"
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
            REQUIRE_NOTHROW([&]() { future = rh.execute("return my_add(42,33);"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Execute code with argv") {

            poesie::VmHandle::ArgsType argv = {42};
            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("return $__argv__[1] + 33;", argv); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Execute foreign function") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute("return my_mult(42,33);"); }());

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

        SECTION("Load a file") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("example.jx9"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_number());
            REQUIRE(result.get<int>() == 75);
        }

        SECTION("Load a file (bad file)") {

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.load("bad.jx9"); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_THROWS_AS([&]() { result = future.wait(); }(), poesie::Exception);
        }

        SECTION("Return JSON data") {

            auto code = R"(
            $my_var = {
                x: 42,
                y: 4.2,
                z: true,
                a: [2, 4, 6, 8],
                b: {
                    c: "abcd",
                    d: "efgh"
                },
                e: null
            };
            return $my_var;
            )";

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() { future = rh.execute(code); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&]() { result = future.wait(); }());

            REQUIRE(result.is_object());
            auto content = result.dump();
            auto expected = std::string{
                R"({"a":[2,4,6,8],"b":{"c":"abcd","d":"efgh"},"e":null,"x":42,"y":4.2,"z":true})"};
            REQUIRE(content == expected);
        }

        SECTION("Use MemoryView") {

            auto code = R"(
            $view = $__argv__[1];
            if(memory_view_length($view) != 16) {
                return 1;
            }
            if(memory_view_to_string($view) != "ABCDEFGHIJKLMNOP") {
                return 2;
            }
            for($i = 0; $i < 16; $i++) {
                $b = memory_view_get($view, $i);
                memory_view_set($view, $i, $b + 32);
            }
            return 0;
            )";

            std::string data = "ABCDEFGHIJKLMNOP";

            poesie::VmHandle::ArgsType args(1);
            args[0] = poesie::MemoryView{
                engine, data.data(), data.size(),
                    poesie::MemoryView::Intent::INOUT};

            poesie::VmHandle::FutureType future;
            REQUIRE_NOTHROW([&]() {
                future = rh.execute(code, args); }());

            poesie::VmHandle::ReturnType result;
            REQUIRE_NOTHROW([&](){ result = future.wait();}());
            REQUIRE(result.get<int>() == 0);
            REQUIRE(data == "abcdefghijklmnop");
        }
    }
}
