#include <catch2/catch_test_macros.hpp>

#include "Luma/Serialization/Json.h"
#include "Luma/Serialization/SerialValue.h"

using Luma::ParseJson;
using Luma::SerialValue;
using Luma::WriteJson;

TEST_CASE("WriteJson emits compact scalars and containers", "[serial][json]") {
    SerialValue o = SerialValue::MakeObject();
    o["name"] = "cube";
    o["visible"] = true;
    o["count"] = 3;
    SerialValue arr = SerialValue::MakeArray();
    arr.PushBack(1);
    arr.PushBack(2);
    o["ids"] = arr;

    const std::string json = WriteJson(o, /*pretty=*/false);
    REQUIRE(json == R"({"name":"cube","visible":true,"count":3,"ids":[1,2]})");
}

TEST_CASE("WriteJson escapes strings", "[serial][json]") {
    SerialValue s{std::string("a\"b\\c\n")};
    REQUIRE(WriteJson(s, false) == R"("a\"b\\c\n")");
}

TEST_CASE("ParseJson reads scalars, arrays and objects", "[serial][json]") {
    auto v = ParseJson(R"({"a":1,"b":2.5,"c":"hi","d":true,"e":null,"f":[1,2,3]})");
    REQUIRE(v.has_value());
    REQUIRE(v->IsObject());
    REQUIRE(v->Find("a")->AsInt() == 1);
    REQUIRE(v->Find("b")->AsFloat() == 2.5);
    REQUIRE(v->Find("c")->AsString() == "hi");
    REQUIRE(v->Find("d")->AsBool() == true);
    REQUIRE(v->Find("e")->IsNull());
    REQUIRE(v->Find("f")->Size() == 3);
    REQUIRE(v->Find("f")->At(2).AsInt() == 3);
}

TEST_CASE("ParseJson handles negative and exponent numbers", "[serial][json]") {
    auto v = ParseJson("[-5, 1.5e3, -2.0e-1]");
    REQUIRE(v.has_value());
    REQUIRE(v->At(0).AsInt() == -5);
    REQUIRE(v->At(1).AsFloat() == 1500.0);
    REQUIRE(v->At(2).AsFloat() == -0.2);
}

TEST_CASE("ParseJson tolerates whitespace and comments", "[serial][json]") {
    auto v = ParseJson(R"(
        {
            // the entity name
            "name": "hero",
            /* block */ "hp": 100
        }
    )");
    REQUIRE(v.has_value());
    REQUIRE(v->Find("name")->AsString() == "hero");
    REQUIRE(v->Find("hp")->AsInt() == 100);
}

TEST_CASE("ParseJson reports an error on malformed input", "[serial][json]") {
    std::string err;
    auto v = ParseJson(R"({"a": })", &err);
    REQUIRE_FALSE(v.has_value());
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("JSON round-trips a nested structure", "[serial][json]") {
    SerialValue root = SerialValue::MakeObject();
    root["scene"] = "Main";
    SerialValue entities = SerialValue::MakeArray();
    SerialValue e = SerialValue::MakeObject();
    e["name"] = "Camera";
    SerialValue pos = SerialValue::MakeArray();
    pos.PushBack(0.0);
    pos.PushBack(1.5);
    pos.PushBack(-3.0);
    e["position"] = pos;
    entities.PushBack(e);
    root["entities"] = entities;

    const std::string text = WriteJson(root);
    auto reparsed = ParseJson(text);
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed->Find("scene")->AsString() == "Main");
    REQUIRE(reparsed->Find("entities")->Size() == 1);
    const SerialValue& e2 = reparsed->Find("entities")->At(0);
    REQUIRE(e2.Find("name")->AsString() == "Camera");
    REQUIRE(e2.Find("position")->At(1).AsFloat() == 1.5);
}
