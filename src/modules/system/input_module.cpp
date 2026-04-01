#include "modules/system/input_module.hpp"

#include "system/runtime_services.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"

#include <cstdlib>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

namespace {

static int64_t toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
    if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
    return 0;
}

static std::string toString(ValuePtr v) {
    if (!v) return "";
    return v->toString();
}

struct InputContext {
    std::unordered_map<int64_t, std::vector<ValuePtr>> keyCallbacks;
    std::unordered_map<std::string, std::vector<ValuePtr>> mouseCallbacks;
};

static bool simulationAllowed() {
#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    _dupenv_s(&value, &len, "KERN_ALLOW_INPUT_SIM");
    std::string env = value ? value : "";
    if (value) std::free(value);
    return env == "1";
#else
    const char* env = std::getenv("KERN_ALLOW_INPUT_SIM");
    return env && std::string(env) == "1";
#endif
}

static ValuePtr makeEventMap(const SystemEvent& ev) {
    std::unordered_map<std::string, ValuePtr> m;
    m["type"] = std::make_shared<Value>(Value::fromString(ev.type));
    m["key"] = std::make_shared<Value>(Value::fromInt(ev.key));
    m["x"] = std::make_shared<Value>(Value::fromInt(ev.x));
    m["y"] = std::make_shared<Value>(Value::fromInt(ev.y));
    m["button"] = std::make_shared<Value>(Value::fromInt(ev.button));
    return std::make_shared<Value>(Value::fromMap(std::move(m)));
}

} // namespace

ValuePtr createInputModule(VM& vm, const std::shared_ptr<RuntimeServices>& services) {
    std::unordered_map<std::string, ValuePtr> mod;
    static size_t s_builtinBase = 760;
    const auto context = std::make_shared<InputContext>();

    auto add = [&](const std::string& name, VM::BuiltinFn fn) {
        const size_t idx = s_builtinBase++;
        vm.registerBuiltin(idx, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = idx;
        mod[name] = std::make_shared<Value>(Value::fromFunction(f));
    };

    add("onKey", [context](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[1] || args[1]->type != Value::Type::FUNCTION) return Value::fromBool(false);
        int64_t key = toInt(args[0]);
        context->keyCallbacks[key].push_back(args[1]);
        return Value::fromBool(true);
    });

    add("onMouse", [context](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[1] || args[1]->type != Value::Type::FUNCTION) return Value::fromBool(false);
        std::string evName = toString(args[0]);
        if (evName.empty()) evName = "move";
        context->mouseCallbacks[evName].push_back(args[1]);
        return Value::fromBool(true);
    });

    add("simulateKey", [services](VM*, std::vector<ValuePtr> args) {
        if (!simulationAllowed()) return Value::fromBool(false);
        SystemEvent ev;
        ev.type = "key";
        ev.key = args.empty() ? 0 : toInt(args[0]);
        services->bus.push(std::move(ev));
        return Value::fromBool(true);
    });

    add("simulateMouse", [services](VM*, std::vector<ValuePtr> args) {
        if (!simulationAllowed()) return Value::fromBool(false);
        SystemEvent ev;
        ev.type = "mouse_move";
        ev.x = args.empty() ? 0 : toInt(args[0]);
        ev.y = args.size() >= 2 ? toInt(args[1]) : 0;
        services->bus.push(std::move(ev));
        return Value::fromBool(true);
    });

    add("poll", [context, services](VM* v, std::vector<ValuePtr>) {
        if (!v) return Value::fromInt(0);
        std::queue<SystemEvent> events;
        const size_t count = services->bus.drainTo(events, 4096);
        while (!events.empty()) {
            const SystemEvent ev = std::move(events.front());
            events.pop();

            if (ev.type == "key") {
                auto it = context->keyCallbacks.find(ev.key);
                if (it != context->keyCallbacks.end()) {
                    auto payload = makeEventMap(ev);
                    for (const auto& cb : it->second) {
                        if (cb) (void)v->callValue(cb, {payload});
                    }
                }
            } else if (ev.type == "mouse_move" || ev.type == "mouse_down" || ev.type == "mouse_up") {
                std::string mapped = ev.type == "mouse_move" ? "move" : (ev.type == "mouse_down" ? "down" : "up");
                auto it = context->mouseCallbacks.find(mapped);
                if (it != context->mouseCallbacks.end()) {
                    auto payload = makeEventMap(ev);
                    for (const auto& cb : it->second) {
                        if (cb) (void)v->callValue(cb, {payload});
                    }
                }
            }
        }
        return Value::fromInt(static_cast<int64_t>(count));
    });

    return std::make_shared<Value>(Value::fromMap(std::move(mod)));
}

} // namespace kern

