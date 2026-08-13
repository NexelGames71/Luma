#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Luma/Core/Types.h"

// SerialValue - a small, ordered, self-describing value tree used as the
// intermediate representation for all Luma serialization. Any object can be
// converted to/from a SerialValue, and a SerialValue can be written to (or read
// from) a concrete format such as JSON (see Json.h) or, later, a binary archive.
//
// Objects preserve key insertion order so output is deterministic (clean diffs,
// stable files). Numbers are stored as either integer (i64) or float (f64); JSON
// output picks the right token, and readers can request either.

namespace Luma {

enum class SerialType { Null, Bool, Int, Float, String, Array, Object };

class SerialValue {
public:
    using Member = std::pair<std::string, SerialValue>;

    SerialValue() = default;  // Null
    SerialValue(bool b) : m_type(SerialType::Bool), m_bool(b) {}
    SerialValue(i32 v) : m_type(SerialType::Int), m_int(v) {}
    SerialValue(i64 v) : m_type(SerialType::Int), m_int(v) {}
    SerialValue(u32 v) : m_type(SerialType::Int), m_int(static_cast<i64>(v)) {}
    SerialValue(f32 v) : m_type(SerialType::Float), m_float(v) {}
    SerialValue(f64 v) : m_type(SerialType::Float), m_float(v) {}
    SerialValue(const char* s) : m_type(SerialType::String), m_str(s) {}
    SerialValue(std::string s) : m_type(SerialType::String), m_str(std::move(s)) {}

    // Named constructors for the container kinds.
    static SerialValue MakeArray();
    static SerialValue MakeObject();

    SerialType Type() const { return m_type; }
    bool IsNull() const { return m_type == SerialType::Null; }
    bool IsBool() const { return m_type == SerialType::Bool; }
    bool IsInt() const { return m_type == SerialType::Int; }
    bool IsFloat() const { return m_type == SerialType::Float; }
    bool IsNumber() const { return IsInt() || IsFloat(); }
    bool IsString() const { return m_type == SerialType::String; }
    bool IsArray() const { return m_type == SerialType::Array; }
    bool IsObject() const { return m_type == SerialType::Object; }

    // Scalar reads. A wrong-typed value yields the supplied default. Numbers
    // convert between int/float freely.
    bool AsBool(bool def = false) const;
    i64 AsInt(i64 def = 0) const;
    f64 AsFloat(f64 def = 0.0) const;
    std::string AsString(std::string def = {}) const;

    // --- Array ---
    // Number of elements (array) or members (object); 0 for scalars/null.
    usize Size() const;
    SerialValue& PushBack(SerialValue value);  // array; converts Null -> Array
    const SerialValue& At(usize index) const;  // array element (bounds-checked-ish)
    SerialValue& At(usize index);
    const std::vector<SerialValue>& Elements() const { return m_array; }

    // --- Object ---
    bool Contains(std::string_view key) const;
    // Insert-or-access by key; converts Null -> Object. Missing keys are created
    // as Null so `v["a"]["b"] = 1` works.
    SerialValue& operator[](std::string_view key);
    const SerialValue* Find(std::string_view key) const;
    const std::vector<Member>& Members() const { return m_object; }

private:
    SerialType m_type = SerialType::Null;
    bool m_bool = false;
    i64 m_int = 0;
    f64 m_float = 0.0;
    std::string m_str;
    std::vector<SerialValue> m_array;
    std::vector<Member> m_object;
};

}  // namespace Luma
