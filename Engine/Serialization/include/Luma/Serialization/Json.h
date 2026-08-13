#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Luma/Serialization/SerialValue.h"

// JSON text <-> SerialValue. A compact, dependency-free reader/writer covering
// the JSON data model Luma needs (objects, arrays, strings, numbers, bool, null).
// Writing is deterministic (object key order preserved). Reading is lenient about
// surrounding whitespace and accepts // and /* */ comments so hand-edited engine
// files stay friendly.

namespace Luma {

// Serializes `value` to JSON text. `pretty` uses 2-space indentation and
// newlines; otherwise the output is a single compact line.
std::string WriteJson(const SerialValue& value, bool pretty = true);

// Parses JSON text. On success returns the value; on failure returns nullopt and,
// if `outError` is non-null, sets it to a human-readable message with a line:col.
std::optional<SerialValue> ParseJson(std::string_view text,
                                     std::string* outError = nullptr);

}  // namespace Luma
