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

using json = nlohmann::json;

TEST_CASE("MemoryView test", "[memory-view]") {

    auto engine = thallium::engine("na+sm", THALLIUM_SERVER_MODE);
    std::function<void(const thallium::request&, const std::string&)> fun =
        [&](const thallium::request& req, const std::string& s) {
            auto j = nlohmann::json::parse(s);
            if(!j.is_object()) {
                req.respond((int)1);
                return;
            }
            if(!j.contains("view")) {
                req.respond((int)2);
                return;
            }
            if(!poesie::MemoryView::IsMemoryView(j["view"])) {
                req.respond((int)3);
                return;
            }
            auto view = poesie::MemoryView(engine, j["view"]);
            if(!view) {
                req.respond((int)4);
                return;
            }
            if(view.size() != 128) {
                req.respond((int)5);
                return;
            }
            auto data = view.data();
            if(data == nullptr) {
                req.respond((int)6);
                return;
            }
            if(view.intent() == poesie::MemoryView::Intent::IN
            || view.intent() == poesie::MemoryView::Intent::INOUT) {
                std::vector<char> expected(view.size());
                for(size_t i = 0; i < view.size(); ++i) {
                    expected[i] = 'A' + (i % 26);
                }
                if(std::memcmp(data, expected.data(), expected.size()) != 0) {
                    req.respond((int)7);
                    return;
                }
            }
            if(view.intent() == poesie::MemoryView::Intent::OUT
            || view.intent() == poesie::MemoryView::Intent::INOUT) {
                for(size_t i = 0; i < view.size(); ++i) {
                    data[i] = 'A' + ((i+1) % 26);;
                }
            }
            req.respond((int)0);
        };
    auto rpc = engine.define("rpc", fun);

    SECTION("Create MemoryView") {
        std::vector<char> data(128);
        poesie::MemoryView view;
        REQUIRE_NOTHROW([&](){
                view = poesie::MemoryView{
                    engine,
                    data.data(),
                    data.size(),
                    poesie::MemoryView::Intent::INOUT};}());
        REQUIRE(view.size() == 128);
        REQUIRE(view.data() == data.data());
    }

    SECTION("Serialize MemoryView") {
        std::vector<char> data(128);
        auto view1 = poesie::MemoryView{
            engine,
            data.data(),
            data.size(),
            poesie::MemoryView::Intent::INOUT};
        auto view2 = poesie::MemoryView{
            engine,
            view1.toJson()};
        REQUIRE(view1.size() == view2.size());
        REQUIRE(view1.data() == view2.data());
    }

    SECTION("Serialize MemoryView with dump") {
        std::vector<char> data(128);
        auto view1 = poesie::MemoryView{
            engine,
            data.data(),
            data.size(),
            poesie::MemoryView::Intent::INOUT};
        auto view2 = poesie::MemoryView{
            engine,
            json::parse(view1.toJson().dump())};
        REQUIRE(view1.size() == view2.size());
        REQUIRE(view1.data() == view2.data());
    }

    SECTION("Send Input MemoryView") {
        std::vector<char> data(128, 0);
        std::vector<char> expected(128, 0);
        for(size_t i = 0; i < data.size(); ++i) {
            data[i] = 'A' + (i%26);
            expected[i] = 'A' + (i%26);
        }
        auto view = poesie::MemoryView{
            engine,
            data.data(),
            data.size(),
            poesie::MemoryView::Intent::IN};
        auto args = json::object();
        args["view"] = view;
        int ret = rpc.on(engine.self())(args.dump());
        REQUIRE(ret == 0);
        REQUIRE(std::memcmp(data.data(), expected.data(), data.size()) == 0);
    }

    SECTION("Send Output MemoryView") {
        std::vector<char> data(128, 0);
        std::vector<char> expected(128, 0);
        for(size_t i = 0; i < data.size(); ++i) {
            data[i] = 'A' + (i%26);
            expected[i] = 'A' + ((i + 1)%26);
        }
        auto view = poesie::MemoryView{
            engine,
            data.data(),
            data.size(),
            poesie::MemoryView::Intent::OUT};
        auto args = json::object();
        args["view"] = view;
        int ret = rpc.on(engine.self())(args.dump());
        REQUIRE(ret == 0);
        REQUIRE(std::memcmp(data.data(), expected.data(), data.size()) == 0);
    }

    SECTION("Send InOut MemoryView") {
        std::vector<char> data(128);
        std::vector<char> expected(128);
        for(size_t i = 0; i < data.size(); ++i) {
            data[i] = 'A' + (i%26);
            expected[i] = 'A' + ((i + 1)%26);
        }
        auto view = poesie::MemoryView{
            engine,
            data.data(),
            data.size(),
            poesie::MemoryView::Intent::INOUT};
        auto args = json::object();
        args["view"] = view;
        int ret = rpc.on(engine.self())(args.dump());
        REQUIRE(ret == 0);
        REQUIRE(std::memcmp(data.data(), expected.data(), data.size()) == 0);
    }

    SECTION("Send Fragmented InOut MemoryView") {
        std::vector<char> data1(64);
        std::vector<char> data2(64);
        std::vector<char> expected(128);
        for(size_t i = 0; i < expected.size(); ++i) {
            if(i < data1.size()) {
                data1[i] = 'A' + (i%26);
            } else {
                data2[i-data1.size()] = 'A' + (i%26);
            }
            expected[i] = 'A' + ((i + 1)%26);
        }
        auto bulk = engine.expose({{data1.data(), data1.size()},
                                   {data2.data(), data2.size()}},
                                   thallium::bulk_mode::read_write);
        auto view = poesie::MemoryView{
            engine, bulk, engine.self(),
            poesie::MemoryView::Intent::INOUT};
        auto args = json::object();
        args["view"] = view;
        int ret = rpc.on(engine.self())(args.dump());
        REQUIRE(ret == 0);
        REQUIRE(std::memcmp(data1.data(), expected.data(), data1.size()) == 0);
        REQUIRE(std::memcmp(data2.data(), expected.data() + data1.size(), data2.size()) == 0);
    }
}
