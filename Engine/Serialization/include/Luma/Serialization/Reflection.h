#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "Luma/Core/Types.h"
#include "Luma/Serialization/SerialValue.h"

// Lightweight compile-time-registered reflection. A type describes its fields
// once via a TypeBuilder; the engine can then generically serialize/deserialize
// instances (SerializeObject/DeserializeObject) and, later, drive a data-driven
// Inspector from the same metadata (category, tooltip, range, read-only).
//
// Registration for a type T is provided by specializing GetTypeInfo<T>() to
// return a built-once TypeInfo<T>. Field types are converted through
// SerialTraits<F>; arithmetic types, bool, enums and std::string work out of the
// box, and other types (e.g. math vectors) are supported by specializing
// SerialTraits for them near their definition.

namespace Luma {

// --- Field <-> SerialValue conversion (customization point) ---
template <class F, class = void>
struct SerialTraits {
    static SerialValue Write(const F& v) {
        if constexpr (std::is_same_v<F, bool>) {
            return SerialValue{v};
        } else if constexpr (std::is_enum_v<F>) {
            return SerialValue{static_cast<i64>(
                static_cast<std::underlying_type_t<F>>(v))};
        } else if constexpr (std::is_integral_v<F>) {
            return SerialValue{static_cast<i64>(v)};
        } else if constexpr (std::is_floating_point_v<F>) {
            return SerialValue{static_cast<f64>(v)};
        } else if constexpr (std::is_same_v<F, std::string>) {
            return SerialValue{v};
        } else {
            static_assert(sizeof(F) == 0,
                          "No SerialTraits<F> for this field type; specialize it.");
            return {};
        }
    }
    static void Read(const SerialValue& s, F& v) {
        if constexpr (std::is_same_v<F, bool>) {
            v = s.AsBool(v);
        } else if constexpr (std::is_enum_v<F>) {
            v = static_cast<F>(s.AsInt(
                static_cast<i64>(static_cast<std::underlying_type_t<F>>(v))));
        } else if constexpr (std::is_integral_v<F>) {
            v = static_cast<F>(s.AsInt(static_cast<i64>(v)));
        } else if constexpr (std::is_floating_point_v<F>) {
            v = static_cast<F>(s.AsFloat(static_cast<f64>(v)));
        } else if constexpr (std::is_same_v<F, std::string>) {
            v = s.AsString(v);
        } else {
            static_assert(sizeof(F) == 0,
                          "No SerialTraits<F> for this field type; specialize it.");
        }
    }
};

// Editor/serialization metadata attached to a property.
struct PropertyMeta {
    std::string category;
    std::string tooltip;
    bool hasRange = false;
    f64 rangeMin = 0.0;
    f64 rangeMax = 0.0;
    bool readOnly = false;
};

template <class T>
class TypeInfo {
public:
    struct Property {
        std::string name;
        PropertyMeta meta;
        std::function<SerialValue(const T&)> get;
        std::function<void(T&, const SerialValue&)> set;
    };

    const std::string& Name() const { return m_name; }
    const std::vector<Property>& Properties() const { return m_props; }

    // Populated by TypeBuilder<T>.
    std::string m_name;
    std::vector<Property> m_props;
};

template <class T>
class TypeBuilder {
public:
    explicit TypeBuilder(std::string typeName) {
        m_info.m_name = std::move(typeName);
    }

    template <class F>
    TypeBuilder& Property(std::string name, F T::*member) {
        typename TypeInfo<T>::Property p;
        p.name = std::move(name);
        p.get = [member](const T& obj) {
            return SerialTraits<F>::Write(obj.*member);
        };
        p.set = [member](T& obj, const SerialValue& s) {
            SerialTraits<F>::Read(s, obj.*member);
        };
        m_info.m_props.push_back(std::move(p));
        return *this;
    }

    // Fluent metadata applied to the most recently added property.
    TypeBuilder& Category(std::string c) {
        if (!m_info.m_props.empty()) m_info.m_props.back().meta.category = std::move(c);
        return *this;
    }
    TypeBuilder& Tooltip(std::string t) {
        if (!m_info.m_props.empty()) m_info.m_props.back().meta.tooltip = std::move(t);
        return *this;
    }
    TypeBuilder& Range(f64 lo, f64 hi) {
        if (!m_info.m_props.empty()) {
            auto& meta = m_info.m_props.back().meta;
            meta.hasRange = true;
            meta.rangeMin = lo;
            meta.rangeMax = hi;
        }
        return *this;
    }
    TypeBuilder& ReadOnly() {
        if (!m_info.m_props.empty()) m_info.m_props.back().meta.readOnly = true;
        return *this;
    }

    TypeInfo<T> Build() { return std::move(m_info); }

private:
    TypeInfo<T> m_info;
};

// Specialize this per reflected type to return a built-once descriptor.
template <class T>
const TypeInfo<T>& GetTypeInfo();

// Generic serialize: every reflected property -> an object member (by name).
template <class T>
SerialValue SerializeObject(const T& obj) {
    SerialValue out = SerialValue::MakeObject();
    for (const auto& prop : GetTypeInfo<T>().Properties()) {
        out[prop.name] = prop.get(obj);
    }
    return out;
}

// Generic deserialize: present members overwrite fields; missing ones keep the
// instance's existing (default) value, so older files load forward-compatibly.
template <class T>
void DeserializeObject(const SerialValue& in, T& obj) {
    for (const auto& prop : GetTypeInfo<T>().Properties()) {
        if (const SerialValue* field = in.Find(prop.name)) {
            prop.set(obj, *field);
        }
    }
}

}  // namespace Luma
