#include <catch2/catch_test_macros.hpp>

#include "Luma/Serialization/SerialValue.h"

using Luma::SerialType;
using Luma::SerialValue;

TEST_CASE("SerialValue scalars report type and value", "[serial][value]") {
    REQUIRE(SerialValue().IsNull());

    SerialValue b{true};
    REQUIRE(b.IsBool());
    REQUIRE(b.AsBool() == true);

    SerialValue i{42};
    REQUIRE(i.IsInt());
    REQUIRE(i.IsNumber());
    REQUIRE(i.AsInt() == 42);
    REQUIRE(i.AsFloat() == 42.0);  // int reads as float

    SerialValue f{1.5};
    REQUIRE(f.IsFloat());
    REQUIRE(f.AsFloat() == 1.5);

    SerialValue s{"hello"};
    REQUIRE(s.IsString());
    REQUIRE(s.AsString() == "hello");
}

TEST_CASE("SerialValue wrong-typed reads return the default", "[serial][value]") {
    SerialValue s{"nope"};
    REQUIRE(s.AsInt(7) == 7);
    REQUIRE(s.AsBool(true) == true);

    SerialValue n;  // null
    REQUIRE(n.AsString("d") == "d");
    REQUIRE(n.AsFloat(2.5) == 2.5);
}

TEST_CASE("SerialValue arrays push and index in order", "[serial][value]") {
    SerialValue a = SerialValue::MakeArray();
    REQUIRE(a.IsArray());
    a.PushBack(10);
    a.PushBack(20);
    a.PushBack("z");

    REQUIRE(a.Size() == 3);
    REQUIRE(a.At(0).AsInt() == 10);
    REQUIRE(a.At(1).AsInt() == 20);
    REQUIRE(a.At(2).AsString() == "z");
}

TEST_CASE("SerialValue objects insert, find and keep insertion order",
          "[serial][value]") {
    SerialValue o = SerialValue::MakeObject();
    REQUIRE(o.IsObject());
    o["x"] = 1;
    o["y"] = 2;
    o["name"] = "cube";

    REQUIRE(o.Size() == 3);
    REQUIRE(o.Contains("y"));
    REQUIRE_FALSE(o.Contains("z"));
    REQUIRE(o.Find("name") != nullptr);
    REQUIRE(o.Find("name")->AsString() == "cube");
    REQUIRE(o.Find("z") == nullptr);

    // Insertion order preserved.
    const auto& members = o.Members();
    REQUIRE(members.size() == 3);
    REQUIRE(members[0].first == "x");
    REQUIRE(members[1].first == "y");
    REQUIRE(members[2].first == "name");
}

TEST_CASE("SerialValue operator[] auto-vivifies nested objects",
          "[serial][value]") {
    SerialValue root;  // null promoted to object on first key access
    root["transform"]["position"]["x"] = 3;
    REQUIRE(root.IsObject());
    REQUIRE(root["transform"]["position"]["x"].AsInt() == 3);
}
