#include "Luma/Serialization/SerialValue.h"

namespace Luma {

SerialValue SerialValue::MakeArray() {
    SerialValue v;
    v.m_type = SerialType::Array;
    return v;
}

SerialValue SerialValue::MakeObject() {
    SerialValue v;
    v.m_type = SerialType::Object;
    return v;
}

bool SerialValue::AsBool(bool def) const {
    return m_type == SerialType::Bool ? m_bool : def;
}

i64 SerialValue::AsInt(i64 def) const {
    if (m_type == SerialType::Int) return m_int;
    if (m_type == SerialType::Float) return static_cast<i64>(m_float);
    return def;
}

f64 SerialValue::AsFloat(f64 def) const {
    if (m_type == SerialType::Float) return m_float;
    if (m_type == SerialType::Int) return static_cast<f64>(m_int);
    return def;
}

std::string SerialValue::AsString(std::string def) const {
    return m_type == SerialType::String ? m_str : def;
}

usize SerialValue::Size() const {
    if (m_type == SerialType::Array) return m_array.size();
    if (m_type == SerialType::Object) return m_object.size();
    return 0;
}

SerialValue& SerialValue::PushBack(SerialValue value) {
    if (m_type != SerialType::Array) {
        *this = MakeArray();
    }
    m_array.push_back(std::move(value));
    return m_array.back();
}

const SerialValue& SerialValue::At(usize index) const {
    static const SerialValue kNull;
    if (m_type == SerialType::Array && index < m_array.size()) {
        return m_array[index];
    }
    return kNull;
}

SerialValue& SerialValue::At(usize index) {
    // Callers index within [0, Size()); out-of-range returns element 0-ish guard.
    if (m_type == SerialType::Array && index < m_array.size()) {
        return m_array[index];
    }
    static SerialValue kNull;
    kNull = SerialValue{};
    return kNull;
}

bool SerialValue::Contains(std::string_view key) const {
    return Find(key) != nullptr;
}

SerialValue& SerialValue::operator[](std::string_view key) {
    if (m_type != SerialType::Object) {
        *this = MakeObject();
    }
    for (auto& member : m_object) {
        if (member.first == key) return member.second;
    }
    m_object.emplace_back(std::string(key), SerialValue{});
    return m_object.back().second;
}

const SerialValue* SerialValue::Find(std::string_view key) const {
    if (m_type != SerialType::Object) return nullptr;
    for (const auto& member : m_object) {
        if (member.first == key) return &member.second;
    }
    return nullptr;
}

}  // namespace Luma
