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
#include <poesie/VmHandle.hpp>

TEST_CASE("Client test", "[client]") {

    auto engine = thallium::engine("na+sm", THALLIUM_SERVER_MODE);
    ENSURE(engine.finalize());
    // Initialize the provider
    const auto provider_config = R"(
    {
        "vm": {
            "type": "jx9",
            "config": {}
        }
    }
    )";

    poesie::Provider provider(engine, 42, provider_config);

    SECTION("Open vm") {

        poesie::Client client(engine);
        std::string addr = engine.self();

        poesie::VmHandle my_vm = client.makeVmHandle(addr, 42);
        REQUIRE(static_cast<bool>(my_vm));

        REQUIRE_THROWS_AS(client.makeVmHandle(addr, 55), poesie::Exception);
        REQUIRE_NOTHROW(client.makeVmHandle(addr, 55, false));
    }
}
