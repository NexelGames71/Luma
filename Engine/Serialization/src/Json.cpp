#include "Luma/Serialization/Json.h"

#include <array>
#include <charconv>
#include <cstdio>

namespace Luma {
namespace {

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

void AppendEscaped(std::string& out, const std::string& s) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    std::array<char, 8> buf{};
                    std::snprintf(buf.data(), buf.size(), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf.data();
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
}

void AppendFloat(std::string& out, f64 v) {
    std::array<char, 32> buf{};
    auto [ptr, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (ec == std::errc{}) {
        out.append(buf.data(), ptr);
    } else {
        out += "0";
    }
}

void WriteValue(std::string& out, const SerialValue& v, bool pretty, int depth) {
    const auto indent = [&](int d) {
        if (pretty) {
            out.push_back('\n');
            out.append(static_cast<usize>(d) * 2, ' ');
        }
    };

    switch (v.Type()) {
        case SerialType::Null:
            out += "null";
            break;
        case SerialType::Bool:
            out += v.AsBool() ? "true" : "false";
            break;
        case SerialType::Int:
            out += std::to_string(v.AsInt());
            break;
        case SerialType::Float:
            AppendFloat(out, v.AsFloat());
            break;
        case SerialType::String:
            AppendEscaped(out, v.AsString());
            break;
        case SerialType::Array: {
            if (v.Size() == 0) {
                out += "[]";
                break;
            }
            out.push_back('[');
            const auto& elems = v.Elements();
            for (usize i = 0; i < elems.size(); ++i) {
                if (i) out.push_back(',');
                indent(depth + 1);
                WriteValue(out, elems[i], pretty, depth + 1);
            }
            indent(depth);
            out.push_back(']');
            break;
        }
        case SerialType::Object: {
            if (v.Size() == 0) {
                out += "{}";
                break;
            }
            out.push_back('{');
            const auto& members = v.Members();
            for (usize i = 0; i < members.size(); ++i) {
                if (i) out.push_back(',');
                indent(depth + 1);
                AppendEscaped(out, members[i].first);
                out += pretty ? ": " : ":";
                WriteValue(out, members[i].second, pretty, depth + 1);
            }
            indent(depth);
            out.push_back('}');
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Parser (recursive descent)
// ---------------------------------------------------------------------------

class Parser {
public:
    Parser(std::string_view text, std::string* err) : m_text(text), m_err(err) {}

    bool Parse(SerialValue& out) {
        SkipTrivia();
        if (!ParseValue(out)) return false;
        SkipTrivia();
        if (m_pos != m_text.size()) {
            return Fail("trailing characters after top-level value");
        }
        return true;
    }

private:
    bool AtEnd() const { return m_pos >= m_text.size(); }
    char Peek() const { return m_text[m_pos]; }

    void Advance() {
        if (m_text[m_pos] == '\n') {
            ++m_line;
            m_col = 1;
        } else {
            ++m_col;
        }
        ++m_pos;
    }

    void SkipTrivia() {
        while (!AtEnd()) {
            char c = Peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                Advance();
            } else if (c == '/' && m_pos + 1 < m_text.size() &&
                       m_text[m_pos + 1] == '/') {
                while (!AtEnd() && Peek() != '\n') Advance();
            } else if (c == '/' && m_pos + 1 < m_text.size() &&
                       m_text[m_pos + 1] == '*') {
                Advance();
                Advance();
                while (!AtEnd() &&
                       !(Peek() == '*' && m_pos + 1 < m_text.size() &&
                         m_text[m_pos + 1] == '/')) {
                    Advance();
                }
                if (!AtEnd()) {
                    Advance();  // '*'
                    Advance();  // '/'
                }
            } else {
                break;
            }
        }
    }

    bool ParseValue(SerialValue& out) {
        if (AtEnd()) return Fail("unexpected end of input");
        char c = Peek();
        switch (c) {
            case '{': return ParseObject(out);
            case '[': return ParseArray(out);
            case '"': return ParseString(out);
            case 't':
            case 'f': return ParseBool(out);
            case 'n': return ParseNull(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
                return Fail("unexpected character");
        }
    }

    bool ParseObject(SerialValue& out) {
        out = SerialValue::MakeObject();
        Advance();  // '{'
        SkipTrivia();
        if (!AtEnd() && Peek() == '}') {
            Advance();
            return true;
        }
        while (true) {
            SkipTrivia();
            if (AtEnd() || Peek() != '"') return Fail("expected object key string");
            SerialValue key;
            if (!ParseString(key)) return false;
            SkipTrivia();
            if (AtEnd() || Peek() != ':') return Fail("expected ':' after key");
            Advance();  // ':'
            SkipTrivia();
            SerialValue value;
            if (!ParseValue(value)) return false;
            out[key.AsString()] = std::move(value);
            SkipTrivia();
            if (AtEnd()) return Fail("unterminated object");
            if (Peek() == ',') {
                Advance();
                continue;
            }
            if (Peek() == '}') {
                Advance();
                return true;
            }
            return Fail("expected ',' or '}' in object");
        }
    }

    bool ParseArray(SerialValue& out) {
        out = SerialValue::MakeArray();
        Advance();  // '['
        SkipTrivia();
        if (!AtEnd() && Peek() == ']') {
            Advance();
            return true;
        }
        while (true) {
            SkipTrivia();
            SerialValue value;
            if (!ParseValue(value)) return false;
            out.PushBack(std::move(value));
            SkipTrivia();
            if (AtEnd()) return Fail("unterminated array");
            if (Peek() == ',') {
                Advance();
                continue;
            }
            if (Peek() == ']') {
                Advance();
                return true;
            }
            return Fail("expected ',' or ']' in array");
        }
    }

    bool ParseString(SerialValue& out) {
        Advance();  // opening quote
        std::string s;
        while (!AtEnd()) {
            char c = Peek();
            if (c == '"') {
                Advance();
                out = SerialValue{std::move(s)};
                return true;
            }
            if (c == '\\') {
                Advance();
                if (AtEnd()) break;
                char e = Peek();
                switch (e) {
                    case '"': s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/'); break;
                    case 'n': s.push_back('\n'); break;
                    case 'r': s.push_back('\r'); break;
                    case 't': s.push_back('\t'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'u': {
                        if (m_pos + 4 >= m_text.size()) return Fail("bad \\u escape");
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            Advance();
                            char h = Peek();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                            else return Fail("bad \\u hex digit");
                        }
                        AppendUtf8(s, code);
                        break;
                    }
                    default: return Fail("invalid escape sequence");
                }
                Advance();
            } else {
                s.push_back(c);
                Advance();
            }
        }
        return Fail("unterminated string");
    }

    static void AppendUtf8(std::string& s, unsigned code) {
        if (code < 0x80) {
            s.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (code >> 6)));
            s.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xE0 | (code >> 12)));
            s.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool ParseNumber(SerialValue& out) {
        usize start = m_pos;
        bool isFloat = false;
        if (!AtEnd() && Peek() == '-') Advance();
        while (!AtEnd()) {
            char c = Peek();
            if (c >= '0' && c <= '9') {
                Advance();
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                isFloat = isFloat || (c == '.' || c == 'e' || c == 'E');
                Advance();
            } else {
                break;
            }
        }
        std::string_view tok = m_text.substr(start, m_pos - start);
        if (tok.empty() || tok == "-") return Fail("malformed number");
        if (isFloat) {
            f64 value = 0.0;
            auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), value);
            if (ec != std::errc{} || ptr != tok.data() + tok.size()) {
                return Fail("malformed float");
            }
            out = SerialValue{value};
        } else {
            i64 value = 0;
            auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), value);
            if (ec != std::errc{} || ptr != tok.data() + tok.size()) {
                return Fail("malformed integer");
            }
            out = SerialValue{value};
        }
        return true;
    }

    bool ParseBool(SerialValue& out) {
        if (Match("true")) {
            out = SerialValue{true};
            return true;
        }
        if (Match("false")) {
            out = SerialValue{false};
            return true;
        }
        return Fail("invalid literal");
    }

    bool ParseNull(SerialValue& out) {
        if (Match("null")) {
            out = SerialValue{};
            return true;
        }
        return Fail("invalid literal");
    }

    bool Match(std::string_view literal) {
        if (m_text.substr(m_pos, literal.size()) != literal) return false;
        for (usize i = 0; i < literal.size(); ++i) Advance();
        return true;
    }

    bool Fail(const char* message) {
        if (m_err) {
            *m_err = "JSON parse error at line " + std::to_string(m_line) +
                     ", col " + std::to_string(m_col) + ": " + message;
        }
        return false;
    }

    std::string_view m_text;
    std::string* m_err = nullptr;
    usize m_pos = 0;
    usize m_line = 1;
    usize m_col = 1;
};

}  // namespace

std::string WriteJson(const SerialValue& value, bool pretty) {
    std::string out;
    WriteValue(out, value, pretty, 0);
    return out;
}

std::optional<SerialValue> ParseJson(std::string_view text, std::string* outError) {
    Parser parser(text, outError);
    SerialValue value;
    if (!parser.Parse(value)) return std::nullopt;
    return value;
}

}  // namespace Luma
