#include "system/vision_module.hpp"

#include "system/runtime_services.hpp"
#include "vm/vm.hpp"
#include "bytecode/value.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace kern {

namespace {

static int64_t toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
    if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
    return 0;
}

static uint32_t parseColor(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return static_cast<uint32_t>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::STRING) {
        std::string s = std::get<std::string>(v->data);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        if (s.size() == 6) {
            try {
                return static_cast<uint32_t>(std::stoul(s, nullptr, 16)) | 0xFF000000u;
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

} // namespace

ValuePtr createVisionModule(VM& vm, const std::shared_ptr<RuntimeServices>& services) {
    std::unordered_map<std::string, ValuePtr> mod;
    static size_t s_builtinBase = 810;

    auto add = [&](const std::string& name, VM::BuiltinFn fn) {
        const size_t idx = s_builtinBase++;
        vm.registerBuiltin(idx, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = idx;
        mod[name] = std::make_shared<Value>(Value::fromFunction(f));
    };

    add("capture", [services](VM*, std::vector<ValuePtr>) {
        std::lock_guard<std::mutex> lock(services->renderMutex);
        std::unordered_map<std::string, ValuePtr> m;
        m["width"] = std::make_shared<Value>(Value::fromInt(services->surface.width));
        m["height"] = std::make_shared<Value>(Value::fromInt(services->surface.height));
        m["frame"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(services->surface.frameCounter)));
        return Value::fromMap(std::move(m));
    });

    add("getPixel", [services](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t x = toInt(args[0]);
        int64_t y = toInt(args[1]);
        std::lock_guard<std::mutex> lock(services->renderMutex);
        if (services->surface.width <= 0 || services->surface.height <= 0) return Value::nil();
        if (x < 0 || y < 0 || x >= services->surface.width || y >= services->surface.height) return Value::nil();
        size_t idx = static_cast<size_t>(y) * static_cast<size_t>(services->surface.width) + static_cast<size_t>(x);
        if (idx >= services->surface.front.size()) return Value::nil();
        return Value::fromInt(static_cast<int64_t>(services->surface.front[idx]));
    });

    add("findColor", [services](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        uint32_t target = parseColor(args[0]);
        std::lock_guard<std::mutex> lock(services->renderMutex);
        const int w = services->surface.width;
        const int h = services->surface.height;
        if (w <= 0 || h <= 0 || services->surface.front.empty()) return Value::nil();
        for (int y = 0; y < h; ++y) {
            const size_t row = static_cast<size_t>(y) * static_cast<size_t>(w);
            for (int x = 0; x < w; ++x) {
                const uint32_t px = services->surface.front[row + static_cast<size_t>(x)];
                if (px == target) {
                    std::unordered_map<std::string, ValuePtr> m;
                    m["found"] = std::make_shared<Value>(Value::fromBool(true));
                    m["x"] = std::make_shared<Value>(Value::fromInt(x));
                    m["y"] = std::make_shared<Value>(Value::fromInt(y));
                    return Value::fromMap(std::move(m));
                }
            }
        }
        std::unordered_map<std::string, ValuePtr> m;
        m["found"] = std::make_shared<Value>(Value::fromBool(false));
        m["x"] = std::make_shared<Value>(Value::fromInt(-1));
        m["y"] = std::make_shared<Value>(Value::fromInt(-1));
        return Value::fromMap(std::move(m));
    });

    return std::make_shared<Value>(Value::fromMap(std::move(mod)));
}

} // namespace kern

