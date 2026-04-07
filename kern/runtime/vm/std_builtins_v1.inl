// Included from registerAllBuiltins() in builtins.hpp — std.v1.* module-only builtins (indices follow __safe_invoke2).
// Uses: makeBuiltin, i, toDouble, toInt, VM, Value, ValuePtr, VMError.

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        return Value::fromFloat(std::fmod(toDouble(args[0]), toDouble(args[1])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        return Value::fromFloat(std::hypot(toDouble(args[0]), toDouble(args[1])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::log10(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::log2(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(1);
        return Value::fromFloat(std::exp(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::expm1(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::log1p(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::cbrt(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::asin(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::acos(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::atan(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::sinh(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(1);
        return Value::fromFloat(std::cosh(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::tanh(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        return Value::fromFloat(std::copysign(toDouble(args[0]), toDouble(args[1])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        return Value::fromFloat(std::nextafter(toDouble(args[0]), toDouble(args[1])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::trunc(toDouble(args[0])));
    });
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        return Value::fromInt(static_cast<int64_t>(std::llround(toDouble(args[0]))));
    });

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromInt(-1);
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& sub = std::get<std::string>(args[1]->data);
        size_t start = 0;
        if (args.size() >= 3 && args[2]) start = static_cast<size_t>(std::max<int64_t>(0, args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : 0));
        if (sub.empty()) return Value::fromInt(static_cast<int64_t>(start <= s.size() ? start : -1));
        size_t pos = s.find(sub, start);
        return Value::fromInt(pos == std::string::npos ? -1 : static_cast<int64_t>(pos));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromInt(-1);
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& sub = std::get<std::string>(args[1]->data);
        if (sub.empty()) return Value::fromInt(static_cast<int64_t>(s.size()));
        size_t pos = s.rfind(sub);
        return Value::fromInt(pos == std::string::npos ? -1 : static_cast<int64_t>(pos));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromInt(0);
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& sub = std::get<std::string>(args[1]->data);
        if (sub.empty()) return Value::fromInt(static_cast<int64_t>(s.size() + 1));
        size_t n = 0;
        for (size_t p = 0;;) {
            size_t f = s.find(sub, p);
            if (f == std::string::npos) break;
            ++n;
            p = f + sub.size();
        }
        return Value::fromInt(static_cast<int64_t>(n));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        const std::string& s = std::get<std::string>(args[0]->data);
        int64_t start = args.size() > 1 && args[1] ? (args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0) : 0;
        int64_t len = -1;
        if (args.size() > 2 && args[2]) len = args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : -1;
        if (start < 0) start = 0;
        size_t st = static_cast<size_t>(start);
        if (st > s.size()) return Value::fromString("");
        if (len < 0) return Value::fromString(s.substr(st));
        size_t ln = static_cast<size_t>(len);
        return Value::fromString(s.substr(st, ln));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromBool(false);
        std::string a = std::get<std::string>(args[0]->data);
        std::string b = std::get<std::string>(args[1]->data);
        for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value::fromBool(a == b);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromInt(0);
        const std::string& a = std::get<std::string>(args[0]->data);
        const std::string& b = std::get<std::string>(args[1]->data);
        if (a < b) return Value::fromInt(-1);
        if (a > b) return Value::fromInt(1);
        return Value::fromInt(0);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        size_t i0 = 0;
        while (i0 < s.size() && std::isspace(static_cast<unsigned char>(s[i0]))) ++i0;
        return Value::fromString(s.substr(i0));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        return Value::fromString(s);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["before"] = std::make_shared<Value>(Value::fromString(""));
        m["after"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromMap(std::move(m));
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& sep = std::get<std::string>(args[1]->data);
        if (sep.empty()) return Value::fromMap(std::move(m));
        size_t pos = s.find(sep);
        if (pos == std::string::npos) {
            m["before"] = std::make_shared<Value>(Value::fromString(s));
            m["ok"] = std::make_shared<Value>(Value::fromBool(false));
            return Value::fromMap(std::move(m));
        }
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["before"] = std::make_shared<Value>(Value::fromString(s.substr(0, pos)));
        m["after"] = std::make_shared<Value>(Value::fromString(s.substr(pos + sep.size())));
        return Value::fromMap(std::move(m));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        const std::string& s = std::get<std::string>(args[0]->data);
        int64_t idx = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        if (idx < 0 || static_cast<size_t>(idx) >= s.size()) return Value::fromString("");
        return Value::fromString(s.substr(static_cast<size_t>(idx), 1));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(true);
        return Value::fromBool(std::get<std::string>(args[0]->data).empty());
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        const std::string& pre = std::get<std::string>(args[1]->data);
        if (pre.empty() || s.size() < pre.size()) return Value::fromString(s);
        if (s.compare(0, pre.size(), pre) == 0) s.erase(0, pre.size());
        return Value::fromString(std::move(s));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING || !args[1] || args[1]->type != Value::Type::STRING)
            return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        const std::string& suf = std::get<std::string>(args[1]->data);
        if (suf.empty() || s.size() < suf.size()) return Value::fromString(s);
        if (s.compare(s.size() - suf.size(), suf.size(), suf) == 0) s.resize(s.size() - suf.size());
        return Value::fromString(std::move(s));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        int64_t width = args.size() > 1 && args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        std::string fill = args.size() > 2 && args[2] && args[2]->type == Value::Type::STRING ? std::get<std::string>(args[2]->data) : std::string(" ");
        if (fill.empty()) fill = " ";
        if (width <= 0 || static_cast<int64_t>(s.size()) >= width) return Value::fromString(s);
        size_t pad = static_cast<size_t>(width) - s.size();
        size_t left = pad / 2;
        size_t right = pad - left;
        std::string out;
        out.reserve(s.size() + pad);
        for (size_t p = 0; p < left; ++p) out += fill[0];
        out += s;
        for (size_t p = 0; p < right; ++p) out += fill[0];
        return Value::fromString(std::move(out));
    });

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        uint32_t crc = 0xFFFFFFFFu;
        for (const ValuePtr& v : a) {
            int64_t b = 0;
            if (v && v->type == Value::Type::INT) b = std::get<int64_t>(v->data);
            else if (v && v->type == Value::Type::FLOAT) b = static_cast<int64_t>(std::get<double>(v->data));
            b &= 255;
            crc ^= static_cast<uint32_t>(b);
            for (int k = 0; k < 8; ++k) {
                uint32_t c = crc;
                crc = (crc >> 1) ^ ((c & 1u) ? 0xEDB88320u : 0u);
            }
        }
        return Value::fromInt(static_cast<int64_t>(~crc & 0xFFFFFFFFu));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromString("");
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::string out;
        out.reserve(a.size() * 2);
        static const char* hx = "0123456789abcdef";
        for (const ValuePtr& v : a) {
            int64_t b = 0;
            if (v && v->type == Value::Type::INT) b = std::get<int64_t>(v->data);
            else if (v && v->type == Value::Type::FLOAT) b = static_cast<int64_t>(std::get<double>(v->data));
            b &= 255;
            out += hx[(b >> 4) & 15];
            out += hx[b & 15];
        }
        return Value::fromString(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        std::string s = std::get<std::string>(args[0]->data);
        if (s.size() % 2 != 0) return Value::fromArray({});
        std::vector<ValuePtr> out;
        out.reserve(s.size() / 2);
        for (size_t p = 0; p + 1 < s.size(); p += 2) {
            char c0 = s[p];
            char c1 = s[p + 1];
            auto hexv = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return -1;
            };
            int hi = hexv(c0);
            int lo = hexv(c1);
            if (hi < 0 || lo < 0) return Value::fromArray({});
            out.push_back(std::make_shared<Value>(Value::fromInt((hi << 4) | lo)));
        }
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1] || args[1]->type != Value::Type::ARRAY)
            return Value::fromArray({});
        auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        if (a.size() != b.size()) return Value::fromArray({});
        std::vector<ValuePtr> out;
        out.reserve(a.size());
        for (size_t k = 0; k < a.size(); ++k) {
            int64_t x = 0, y = 0;
            if (a[k] && a[k]->type == Value::Type::INT) x = std::get<int64_t>(a[k]->data);
            if (b[k] && b[k]->type == Value::Type::INT) y = std::get<int64_t>(b[k]->data);
            out.push_back(std::make_shared<Value>(Value::fromInt((x ^ y) & 255)));
        }
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1] || args[1]->type != Value::Type::ARRAY)
            return Value::fromArray({});
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        const auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        std::vector<ValuePtr> out = a;
        out.insert(out.end(), b.begin(), b.end());
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t start = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        int64_t len = args.size() > 2 && args[2] && args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : static_cast<int64_t>(a.size());
        if (start < 0) start = 0;
        if (start >= static_cast<int64_t>(a.size())) return Value::fromArray({});
        size_t st = static_cast<size_t>(start);
        size_t ln = static_cast<size_t>(std::max<int64_t>(0, std::min(len, static_cast<int64_t>(a.size() - st))));
        std::vector<ValuePtr> out(a.begin() + static_cast<std::ptrdiff_t>(st), a.begin() + static_cast<std::ptrdiff_t>(st + ln));
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1] || args[1]->type != Value::Type::ARRAY)
            return Value::fromBool(false);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        const auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        if (a.size() != b.size()) return Value::fromBool(false);
        for (size_t k = 0; k < a.size(); ++k) {
            if (!deepEqual(a[k], b[k])) return Value::fromBool(false);
        }
        return Value::fromBool(true);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t off = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        if (off < 0 || static_cast<size_t>(off) + 1 >= a.size()) return Value::fromInt(0);
        int64_t lo = 0, hi = 0;
        if (a[static_cast<size_t>(off)] && a[static_cast<size_t>(off)]->type == Value::Type::INT) lo = std::get<int64_t>(a[static_cast<size_t>(off)]->data) & 255;
        if (a[static_cast<size_t>(off) + 1] && a[static_cast<size_t>(off) + 1]->type == Value::Type::INT)
            hi = std::get<int64_t>(a[static_cast<size_t>(off) + 1]->data) & 255;
        uint16_t v = static_cast<uint16_t>(lo | (hi << 8));
        return Value::fromInt(static_cast<int64_t>(v));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t off = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        if (off < 0 || static_cast<size_t>(off) + 3 >= a.size()) return Value::fromInt(0);
        uint32_t v = 0;
        for (int b = 0; b < 4; ++b) {
            int64_t x = 0;
            if (a[static_cast<size_t>(off) + static_cast<size_t>(b)] && a[static_cast<size_t>(off) + static_cast<size_t>(b)]->type == Value::Type::INT)
                x = std::get<int64_t>(a[static_cast<size_t>(off) + static_cast<size_t>(b)]->data) & 255;
            v |= static_cast<uint32_t>(x) << (8 * b);
        }
        return Value::fromInt(static_cast<int64_t>(v & 0xFFFFFFFFu));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t off = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        int64_t val = args[2] && args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : 0;
        uint32_t u = static_cast<uint32_t>(val & 0xFFFFFFFFu);
        if (off < 0 || static_cast<size_t>(off) + 3 >= arr.size()) return Value::fromArray({});
        for (int b = 0; b < 4; ++b) {
            arr[static_cast<size_t>(off) + static_cast<size_t>(b)] =
                std::make_shared<Value>(Value::fromInt(static_cast<int64_t>((u >> (8 * b)) & 0xFFu)));
        }
        return Value::fromArray(std::move(arr));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t off = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        if (off < 0 || static_cast<size_t>(off) >= a.size()) return Value::fromInt(0);
        int64_t x = 0;
        if (a[static_cast<size_t>(off)] && a[static_cast<size_t>(off)]->type == Value::Type::INT) x = std::get<int64_t>(a[static_cast<size_t>(off)]->data) & 255;
        return Value::fromInt(x);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        const std::string& s = std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> out;
        out.reserve(s.size());
        for (unsigned char c : s) out.push_back(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(c))));
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        return Value::fromInt(static_cast<int64_t>(a.size()));
    });

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr key = args[1];
        for (size_t k = 0; k < a.size(); ++k) {
            if (deepEqual(a[k], key)) return Value::fromInt(static_cast<int64_t>(k));
        }
        return Value::fromInt(-1);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr key = args[1];
        for (intptr_t k = static_cast<intptr_t>(a.size()) - 1; k >= 0; --k) {
            if (deepEqual(a[static_cast<size_t>(k)], key)) return Value::fromInt(static_cast<int64_t>(k));
        }
        return Value::fromInt(-1);
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromBool(false);
        auto arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t i0 = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        int64_t j0 = args[2] && args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : 0;
        if (i0 < 0 || j0 < 0 || static_cast<size_t>(i0) >= arr.size() || static_cast<size_t>(j0) >= arr.size()) return Value::fromBool(false);
        std::swap(arr[static_cast<size_t>(i0)], arr[static_cast<size_t>(j0)]);
        return Value::fromArray(std::move(arr));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::fromArray({});
        int64_t n = args[1] && args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0;
        size_t len = arr.size();
        n %= static_cast<int64_t>(len);
        if (n < 0) n += static_cast<int64_t>(len);
        std::vector<ValuePtr> out(len);
        for (size_t k = 0; k < len; ++k) out[(k + static_cast<size_t>(n)) % len] = arr[k];
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::MAP || !args[1] || args[1]->type != Value::Type::MAP)
            return Value::fromMap({});
        auto a = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        const auto& b = std::get<std::unordered_map<std::string, ValuePtr>>(args[1]->data);
        for (const auto& kv : b) a[kv.first] = kv.second;
        return Value::fromMap(std::move(a));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1] || args[1]->type != Value::Type::ARRAY)
            return Value::fromArray({});
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        const auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        size_t n = std::min(a.size(), b.size());
        std::vector<ValuePtr> out;
        out.reserve(n);
        for (size_t k = 0; k < n; ++k) {
            std::vector<ValuePtr> pair = {a[k], b[k]};
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(pair))));
        }
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<ValuePtr> out;
        for (const ValuePtr& v : a) {
            bool found = false;
            for (const ValuePtr& u : out) {
                if (deepEqual(u, v)) {
                    found = true;
                    break;
                }
            }
            if (!found) out.push_back(v);
        }
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[1]) return Value::fromArray({});
        int64_t n = args[0] && args[0]->type == Value::Type::INT ? std::get<int64_t>(args[0]->data) : 0;
        if (n < 0) n = 0;
        std::vector<ValuePtr> out(static_cast<size_t>(n), args[1]);
        return Value::fromArray(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromMap({});
        const auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        std::unordered_map<std::string, ValuePtr> out;
        for (const auto& kv : m) {
            if (!kv.second) continue;
            if (kv.second->type != Value::Type::STRING) continue;
            std::string nk = std::get<std::string>(kv.second->data);
            out[nk] = std::make_shared<Value>(Value::fromString(kv.first));
        }
        return Value::fromMap(std::move(out));
    });
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1] || args[1]->type != Value::Type::ARRAY)
            return Value::fromArray({});
        const auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        const auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        std::unordered_map<std::string, char> seen;
        for (const ValuePtr& v : b) {
            if (v) seen[v->toString()] = 1;
        }
        std::vector<ValuePtr> out;
        for (const ValuePtr& v : a) {
            if (!v) continue;
            if (seen.find(v->toString()) != seen.end()) out.push_back(v);
        }
        return Value::fromArray(std::move(out));
    });

    // append bytes to file (create if missing); same string coercion as write_file
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "append_file");
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        std::string path = std::get<std::string>(args[0]->data);
        std::string content = args[1] && args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data)
                                                                              : (args[1] ? args[1]->toString() : "");
        std::ofstream f(path, std::ios::out | std::ios::app);
        if (!f) return Value::fromBool(false);
        f << content;
        return Value::fromBool(true);
    });

    // require("filesystem.write") — grant a permission for the rest of the VM run (no-op if enforcement off)
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        std::string p = std::get<std::string>(args[0]->data);
        for (const auto& resolved : resolvePermissionToken(p))
            vm->mutableRuntimeGuards().grantedPermissions.insert(resolved);
        return Value::fromBool(true);
    });
