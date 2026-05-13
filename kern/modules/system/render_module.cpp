#include "system/render_module.hpp"

#include "system/runtime_services.hpp"
#include "vm/vm.hpp"
#include "bytecode/value.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <memory>
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

static uint32_t parseColor(ValuePtr v) {
    if (!v) return 0xFFFFFFFFu;
    if (v->type == Value::Type::INT) return static_cast<uint32_t>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::STRING) {
        std::string s = std::get<std::string>(v->data);
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        try {
            if (s.size() == 6) return static_cast<uint32_t>(std::stoul(s, nullptr, 16)) | 0xFF000000u;
            if (s.size() == 8) return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
        } catch (...) {
            return 0xFFFFFFFFu;
        }
    }
    return 0xFFFFFFFFu;
}

static void putPixel(RenderSurfaceState& s, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height) return;
    size_t idx = static_cast<size_t>(y) * static_cast<size_t>(s.width) + static_cast<size_t>(x);
    if (idx < s.back.size()) s.back[idx] = color;
}

static void drawLine(RenderSurfaceState& s, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        putPixel(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

} // namespace

ValuePtr createRenderModule(VM& vm, const std::shared_ptr<RuntimeServices>& services) {
    std::unordered_map<std::string, ValuePtr> mod;
    static size_t s_builtinBase = 860;

    auto add = [&](const std::string& name, VM::BuiltinFn fn) {
        const size_t idx = s_builtinBase++;
        vm.registerBuiltin(idx, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = idx;
        mod[name] = std::make_shared<Value>(Value::fromFunction(f));
    };

    add("createWindow", [services](VM*, std::vector<ValuePtr> args) {
        int width = args.size() >= 1 ? static_cast<int>(std::max<int64_t>(1, toInt(args[0]))) : 800;
        int height = args.size() >= 2 ? static_cast<int>(std::max<int64_t>(1, toInt(args[1]))) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern Render";
        bool transparent = args.size() >= 4 ? (args[3] && args[3]->isTruthy()) : false;
        std::lock_guard<std::mutex> lock(services->renderMutex);
        services->surface.width = width;
        services->surface.height = height;
        services->surface.title = std::move(title);
        services->surface.transparent = transparent;
        const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
        services->surface.front.assign(pixels, transparent ? 0x00000000u : 0xFF101010u);
        services->surface.back.assign(pixels, transparent ? 0x00000000u : 0xFF101010u);
        services->surface.frameCounter = 0;
        return Value::fromBool(true);
    });

    add("clear", [services](VM*, std::vector<ValuePtr> args) {
        uint32_t color = args.empty() ? 0xFF101010u : parseColor(args[0]);
        std::lock_guard<std::mutex> lock(services->renderMutex);
        std::fill(services->surface.back.begin(), services->surface.back.end(), color);
        return Value::nil();
    });

    add("drawRect", [services](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int x = static_cast<int>(toInt(args[0]));
        int y = static_cast<int>(toInt(args[1]));
        int w = static_cast<int>(toInt(args[2]));
        int h = static_cast<int>(toInt(args[3]));
        uint32_t color = args.size() >= 5 ? parseColor(args[4]) : 0xFFFFFFFFu;
        std::lock_guard<std::mutex> lock(services->renderMutex);
        for (int yy = 0; yy < h; ++yy) {
            int py = y + yy;
            if (py < 0 || py >= services->surface.height) continue;
            for (int xx = 0; xx < w; ++xx) {
                putPixel(services->surface, x + xx, py, color);
            }
        }
        return Value::nil();
    });

    add("drawLine", [services](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int x0 = static_cast<int>(toInt(args[0]));
        int y0 = static_cast<int>(toInt(args[1]));
        int x1 = static_cast<int>(toInt(args[2]));
        int y1 = static_cast<int>(toInt(args[3]));
        uint32_t color = args.size() >= 5 ? parseColor(args[4]) : 0xFFFFFFFFu;
        std::lock_guard<std::mutex> lock(services->renderMutex);
        drawLine(services->surface, x0, y0, x1, y1, color);
        return Value::nil();
    });

    add("drawText", [services](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        int x = static_cast<int>(toInt(args[0]));
        int y = static_cast<int>(toInt(args[1]));
        std::string text = toString(args[2]);
        uint32_t color = args.size() >= 4 ? parseColor(args[3]) : 0xFFFFFFFFu;
        // lightweight text marker rendering: each glyph is a 6x8 block edge.
        std::lock_guard<std::mutex> lock(services->renderMutex);
        int cx = x;
        for (char ch : text) {
            (void)ch;
            for (int yy = 0; yy < 8; ++yy) {
                for (int xx = 0; xx < 6; ++xx) {
                    bool edge = (yy == 0 || yy == 7 || xx == 0 || xx == 5);
                    if (edge) putPixel(services->surface, cx + xx, y + yy, color);
                }
            }
            cx += 7;
        }
        return Value::nil();
    });

    add("present", [services](VM*, std::vector<ValuePtr>) {
        std::lock_guard<std::mutex> lock(services->renderMutex);
        services->surface.front.swap(services->surface.back);
        std::fill(services->surface.back.begin(), services->surface.back.end(),
                  services->surface.transparent ? 0x00000000u : 0xFF101010u);
        ++services->surface.frameCounter;
        SystemEvent ev;
        ev.type = "render_present";
        services->bus.push(std::move(ev));
        return Value::fromInt(static_cast<int64_t>(services->surface.frameCounter));
    });

    return std::make_shared<Value>(Value::fromMap(std::move(mod)));
}

} // namespace kern

