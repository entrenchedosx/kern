/* *
 * kern g2d module – implementation (window, renderer, shapes, text, colors).
 * backend: game_builtins (Raylib). Single compilation unit for shared state.
 */
#include "g2d.h"
#include "game/game_builtins.hpp"
#include "vm/vm.hpp"
#include "bytecode/value.hpp"
#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace kern {

static int toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return static_cast<int>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::FLOAT) return static_cast<int>(std::get<double>(v->data));
    return 0;
}

static float toFloat(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return static_cast<float>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::FLOAT) return static_cast<float>(std::get<double>(v->data));
    return 0;
}

static std::string toString(ValuePtr v) {
    if (!v) return "";
    return v->toString();
}

static Color makeColor(int r, int g, int b, int a = 255) {
    return Color{ (unsigned char)(r & 255), (unsigned char)(g & 255), (unsigned char)(b & 255), (unsigned char)(a & 255) };
}

static std::string normalizeKeyName(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static int keyNameToRaylib(const std::string& s) {
    static const std::unordered_map<std::string, int> m = {
        {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT}, {"UP", KEY_UP}, {"DOWN", KEY_DOWN},
        {"SPACE", KEY_SPACE}, {"ENTER", KEY_ENTER}, {"ESCAPE", KEY_ESCAPE}, {"TAB", KEY_TAB},
        {"A", KEY_A}, {"B", KEY_B}, {"C", KEY_C}, {"D", KEY_D}, {"E", KEY_E}, {"F", KEY_F},
        {"G", KEY_G}, {"H", KEY_H}, {"I", KEY_I}, {"J", KEY_J}, {"K", KEY_K}, {"L", KEY_L},
        {"M", KEY_M}, {"N", KEY_N}, {"O", KEY_O}, {"P", KEY_P}, {"Q", KEY_Q}, {"R", KEY_R},
        {"S", KEY_S}, {"T", KEY_T}, {"U", KEY_U}, {"V", KEY_V}, {"W", KEY_W}, {"X", KEY_X},
        {"Y", KEY_Y}, {"Z", KEY_Z},
        {"ZERO", KEY_ZERO}, {"ONE", KEY_ONE}, {"TWO", KEY_TWO}, {"THREE", KEY_THREE},
        {"FOUR", KEY_FOUR}, {"FIVE", KEY_FIVE}, {"SIX", KEY_SIX}, {"SEVEN", KEY_SEVEN},
        {"EIGHT", KEY_EIGHT}, {"NINE", KEY_NINE},
    };
    auto it = m.find(normalizeKeyName(s));
    return it != m.end() ? it->second : 0;
}

static int mouseButtonFromArg(ValuePtr v) {
    if (!v) return MOUSE_BUTTON_LEFT;
    if (v->type == Value::Type::INT || v->type == Value::Type::FLOAT)
        return toInt(v);
    std::string s = normalizeKeyName(toString(v));
    if (s == "LEFT" || s == "LMB" || s == "0") return MOUSE_BUTTON_LEFT;
    if (s == "RIGHT" || s == "RMB" || s == "1") return MOUSE_BUTTON_RIGHT;
    if (s == "MIDDLE" || s == "MMB" || s == "2") return MOUSE_BUTTON_MIDDLE;
    return MOUSE_BUTTON_LEFT;
}

static std::vector<Texture2D> g_g2d_textures;
static std::vector<Font> g_g2d_fonts;
struct G2DSprite { int imageId; int frameW; int frameH; int currentFrame; float animTime; };
static std::vector<G2DSprite> g_g2d_sprites;
static int g_g2d_draw_r = 255, g_g2d_draw_g = 255, g_g2d_draw_b = 255, g_g2d_draw_a = 255;
static bool g_g2d_camera_enabled = false;
static bool g_g2d_camera_has_target = false;
static float g_g2d_camera_target_x = 0.0f;
static float g_g2d_camera_target_y = 0.0f;
static bool g_g2d_camera_bounds_enabled = false;
static float g_g2d_camera_min_x = 0.0f, g_g2d_camera_min_y = 0.0f, g_g2d_camera_max_x = 0.0f, g_g2d_camera_max_y = 0.0f;
static float g_g2d_shake_time_left = 0.0f;
static float g_g2d_shake_strength = 0.0f;
static bool g_g2d_scissor_active = false;
static std::unordered_map<int64_t, RenderTexture2D> g_g2d_render_targets;
static int64_t g_g2d_next_rt_id = 1;
static bool g_g2d_in_render_target = false;
// must start after game builtins (GAME_BASE 240, 46 entries -> 240..285 inclusive).
static const size_t G2D_BASE = 286;

static void unloadAllG2dRenderTargets() {
    for (auto& kv : g_g2d_render_targets)
        UnloadRenderTexture(kv.second);
    g_g2d_render_targets.clear();
    g_g2d_in_render_target = false;
}

static void getColorFromArgs(const std::vector<ValuePtr>& args, size_t start, int* r, int* g, int* b, int* a) {
    if (start >= args.size() || !args[start]) { *r = g_g2d_draw_r; *g = g_g2d_draw_g; *b = g_g2d_draw_b; *a = g_g2d_draw_a; return; }
    if (args[start]->type == Value::Type::ARRAY) {
        auto& arr = std::get<std::vector<ValuePtr>>(args[start]->data);
        *r = arr.size() > 0 ? toInt(arr[0]) : 255;
        *g = arr.size() > 1 ? toInt(arr[1]) : 255;
        *b = arr.size() > 2 ? toInt(arr[2]) : 255;
        *a = arr.size() > 3 ? toInt(arr[3]) : 255;
        return;
    }
    *r = start < args.size() ? toInt(args[start]) : 255;
    *g = start + 1 < args.size() ? toInt(args[start + 1]) : 255;
    *b = start + 2 < args.size() ? toInt(args[start + 2]) : 255;
    *a = start + 3 < args.size() ? toInt(args[start + 3]) : 255;
}

static void applyG2dCameraTarget() {
    if (!g_g2d_camera_enabled || !g_g2d_camera_has_target) return;
    float tx = g_g2d_camera_target_x;
    float ty = g_g2d_camera_target_y;
    if (g_g2d_camera_bounds_enabled) {
        tx = std::max(g_g2d_camera_min_x, std::min(tx, g_g2d_camera_max_x));
        ty = std::max(g_g2d_camera_min_y, std::min(ty, g_g2d_camera_max_y));
        g_g2d_camera_target_x = tx;
        g_g2d_camera_target_y = ty;
    }
    if (g_g2d_shake_time_left > 0.0f && g_g2d_shake_strength > 0.0f) {
        float ox = ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float oy = ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        tx += ox * g_g2d_shake_strength;
        ty += oy * g_g2d_shake_strength;
    }
    graphicsSetCameraTarget(tx, ty);
}

static void register2dGraphicsToVM(VM& vm, std::function<void(const std::string&, ValuePtr)> onFn) {
    size_t i = G2D_BASE;
    auto add = [&vm, &i, &onFn](const std::string& name, VM::BuiltinFn fn) {
        vm.registerBuiltin(i, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = i++;
        onFn(name, std::make_shared<Value>(Value::fromFunction(f)));
    };

    add("create", [](VM*, std::vector<ValuePtr> args) {
        int w = args.size() >= 1 ? toInt(args[0]) : 800;
        int h = args.size() >= 2 ? toInt(args[1]) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern 2D";
        graphicsInitWindow(w, h, title);
        return Value::nil();
    });
    add("createWindow", [](VM*, std::vector<ValuePtr> args) {
        int w = args.size() >= 1 ? toInt(args[0]) : 800;
        int h = args.size() >= 2 ? toInt(args[1]) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern Game";
        graphicsInitWindow(w, h, title);
        return Value::nil();
    });
    // back-compat alias used by older BrowserKit demos.
    add("open", [](VM*, std::vector<ValuePtr> args) {
        int w = args.size() >= 1 ? toInt(args[0]) : 800;
        int h = args.size() >= 2 ? toInt(args[1]) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern Game";
        graphicsInitWindow(w, h, title);
        return Value::nil();
    });

    add("close", [](VM*, std::vector<ValuePtr>) {
        unloadAllG2dRenderTargets();
        g_g2d_scissor_active = false;
        g_g2d_camera_enabled = false;
        g_g2d_camera_has_target = false;
        g_g2d_camera_bounds_enabled = false;
        g_g2d_shake_time_left = 0.0f;
        g_g2d_shake_strength = 0.0f;
        graphicsCloseWindow();
        return Value::nil();
    });
    add("closeWindow", [](VM*, std::vector<ValuePtr>) {
        unloadAllG2dRenderTargets();
        g_g2d_scissor_active = false;
        g_g2d_camera_enabled = false;
        g_g2d_camera_has_target = false;
        g_g2d_camera_bounds_enabled = false;
        g_g2d_shake_time_left = 0.0f;
        g_g2d_shake_strength = 0.0f;
        graphicsCloseWindow();
        return Value::nil();
    });
    add("setWindowSize", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2 && graphicsWindowOpen())
            graphicsSetWindowSize(toInt(args[0]), toInt(args[1]));
        return Value::nil();
    });
    add("toggleFullscreen", [](VM*, std::vector<ValuePtr>) {
        graphicsToggleFullscreen();
        return Value::nil();
    });

    add("should_close", [](VM*, std::vector<ValuePtr>) {
        return Value::fromBool(graphicsWindowOpen() && WindowShouldClose());
    });
    add("is_open", [](VM*, std::vector<ValuePtr>) {
        return Value::fromBool(graphicsWindowOpen() && !WindowShouldClose());
    });
    add("isOpen", [](VM*, std::vector<ValuePtr>) {
        return Value::fromBool(graphicsWindowOpen() && !WindowShouldClose());
    });

    add("beginFrame", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) return Value::nil();
        if (g_g2d_shake_time_left > 0.0f) {
            g_g2d_shake_time_left -= GetFrameTime();
            if (g_g2d_shake_time_left < 0.0f) g_g2d_shake_time_left = 0.0f;
        }
        applyG2dCameraTarget();
        graphicsBeginFrame();
        if (g_g2d_camera_enabled) graphicsBegin2D();
        return Value::nil();
    });
    add("endFrame", [](VM*, std::vector<ValuePtr>) {
        if (g_g2d_scissor_active) {
            EndScissorMode();
            g_g2d_scissor_active = false;
        }
        if (g_g2d_camera_enabled) graphicsEnd2D();
        graphicsEndFrame();
        return Value::nil();
    });
    add("setTargetFps", [](VM*, std::vector<ValuePtr> args) {
        int fps = args.empty() ? 60 : toInt(args[0]);
        SetTargetFPS(fps > 0 ? fps : 60);
        return Value::nil();
    });
    add("setVsync", [](VM*, std::vector<ValuePtr> args) {
        if (graphicsWindowOpen() && args.size() >= 1) {
            if (args[0] && args[0]->isTruthy())
                SetWindowState(FLAG_VSYNC_HINT);
            else
                ClearWindowState(FLAG_VSYNC_HINT);
        }
        return Value::nil();
    });
    add("beginScissor", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        if (w <= 0 || h <= 0) return Value::nil();
        BeginScissorMode(x, y, w, h);
        g_g2d_scissor_active = true;
        return Value::nil();
    });
    add("endScissor", [](VM*, std::vector<ValuePtr>) {
        if (!g_g2d_scissor_active) return Value::nil();
        EndScissorMode();
        g_g2d_scissor_active = false;
        return Value::nil();
    });
    add("setColor", [](VM*, std::vector<ValuePtr> args) {
        int r, g, b, a;
        getColorFromArgs(args, 0, &r, &g, &b, &a);
        g_g2d_draw_r = (r & 255); g_g2d_draw_g = (g & 255); g_g2d_draw_b = (b & 255); g_g2d_draw_a = (a & 255);
        return Value::nil();
    });

    add("clear", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::nil();
        int r = args.size() >= 1 ? (toInt(args[0]) & 255) : 0;
        int g = args.size() >= 2 ? (toInt(args[1]) & 255) : 0;
        int b = args.size() >= 3 ? (toInt(args[2]) & 255) : 0;
        int a = args.size() >= 4 ? (toInt(args[3]) & 255) : 255;
        graphicsBeginFrame();
        ClearBackground(makeColor(r, g, b, a));
        return Value::nil();
    });

    add("fill_rect", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        int r = 255, g = 255, b = 255, a = 255;
        if (args.size() >= 7) { r = toInt(args[4]); g = toInt(args[5]); b = toInt(args[6]); }
        if (args.size() >= 8) a = toInt(args[7]);
        DrawRectangle(x, y, w, h, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("stroke_rect", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        int r = 255, g = 255, b = 255, a = 255;
        if (args.size() >= 7) { r = toInt(args[4]); g = toInt(args[5]); b = toInt(args[6]); }
        if (args.size() >= 8) a = toInt(args[7]);
        DrawRectangleLines(x, y, w, h, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("fill_circle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]); float radius = toFloat(args[2]);
        int r = 255, g = 255, b = 255, a = 255;
        if (args.size() >= 6) { r = toInt(args[3]); g = toInt(args[4]); b = toInt(args[5]); }
        if (args.size() >= 7) a = toInt(args[6]);
        DrawCircle(cx, cy, radius, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("stroke_circle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]); float radius = toFloat(args[2]);
        int r = 255, g = 255, b = 255, a = 255;
        if (args.size() >= 6) { r = toInt(args[3]); g = toInt(args[4]); b = toInt(args[5]); }
        if (args.size() >= 7) a = toInt(args[6]);
        DrawCircleLines(cx, cy, radius, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("line", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x1 = toInt(args[0]), y1 = toInt(args[1]), x2 = toInt(args[2]), y2 = toInt(args[3]);
        int r = 255, g = 255, b = 255, a = 255;
        if (args.size() >= 7) { r = toInt(args[4]); g = toInt(args[5]); b = toInt(args[6]); }
        if (args.size() >= 8) a = toInt(args[7]);
        DrawLine(x1, y1, x2, y2, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("drawLineThick", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5) return Value::nil();
        graphicsBeginFrame();
        Vector2 a{toFloat(args[0]), toFloat(args[1])};
        Vector2 b{toFloat(args[2]), toFloat(args[3])};
        float thick = toFloat(args[4]);
        if (thick <= 0.0f) thick = 1.0f;
        int r, g, bl, al; getColorFromArgs(args, 5, &r, &g, &bl, &al);
        DrawLineEx(a, b, thick, makeColor(r, g, bl, al));
        return Value::nil();
    });
    add("drawLine", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x1 = toInt(args[0]), y1 = toInt(args[1]), x2 = toInt(args[2]), y2 = toInt(args[3]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawLine(x1, y1, x2, y2, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("drawRect", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawRectangleLines(x, y, w, h, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("fillRect", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawRectangle(x, y, w, h, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("drawCircle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]); float radius = toFloat(args[2]);
        int r, g, b, a; getColorFromArgs(args, 3, &r, &g, &b, &a);
        DrawCircleLines(cx, cy, radius, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("fillCircle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]); float radius = toFloat(args[2]);
        int r, g, b, a; getColorFromArgs(args, 3, &r, &g, &b, &a);
        DrawCircle(cx, cy, radius, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("drawPolygon", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 1 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& points = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (points.size() < 2) return Value::nil();
        graphicsBeginFrame();
        std::vector<Vector2> v;
        v.reserve(points.size());
        for (const auto& p : points) {
            if (p && p->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(p->data);
                float x = arr.size() > 0 ? toFloat(arr[0]) : 0, y = arr.size() > 1 ? toFloat(arr[1]) : 0;
                v.push_back(Vector2{ x, y });
            }
        }
        if (v.size() < 2) return Value::nil();
        int r, g, b, a; getColorFromArgs(args, 1, &r, &g, &b, &a);
        Color c = makeColor(r, g, b, a);
        for (size_t i = 0; i + 1 < v.size(); ++i)
            DrawLineV(v[i], v[i + 1], c);
        DrawLineV(v.back(), v[0], c);
        return Value::nil();
    });
    add("drawTriangle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 6) return Value::nil();
        graphicsBeginFrame();
        Vector2 a{toFloat(args[0]), toFloat(args[1])};
        Vector2 b{toFloat(args[2]), toFloat(args[3])};
        Vector2 c{toFloat(args[4]), toFloat(args[5])};
        int r, g, bl, al; getColorFromArgs(args, 6, &r, &g, &bl, &al);
        DrawTriangleLines(a, b, c, makeColor(r, g, bl, al));
        return Value::nil();
    });
    add("fillTriangle", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 6) return Value::nil();
        graphicsBeginFrame();
        Vector2 a{toFloat(args[0]), toFloat(args[1])};
        Vector2 b{toFloat(args[2]), toFloat(args[3])};
        Vector2 c{toFloat(args[4]), toFloat(args[5])};
        int r, g, bl, al; getColorFromArgs(args, 6, &r, &g, &bl, &al);
        DrawTriangle(a, b, c, makeColor(r, g, bl, al));
        return Value::nil();
    });
    add("drawEllipse", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]);
        float rx = toFloat(args[2]), ry = toFloat(args[3]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawEllipseLines(cx, cy, rx, ry, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("fillEllipse", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]);
        float rx = toFloat(args[2]), ry = toFloat(args[3]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawEllipse(cx, cy, rx, ry, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("fillGradientRect", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 10) return Value::nil();
        graphicsBeginFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        if (w <= 0 || h <= 0) return Value::nil();
        Color c1 = makeColor(toInt(args[4]), toInt(args[5]), toInt(args[6]));
        Color c2 = makeColor(toInt(args[7]), toInt(args[8]), toInt(args[9]));
        bool vertical = args.size() < 11 || toInt(args[10]) != 0;
        if (vertical)
            DrawRectangleGradientV(x, y, w, h, c1, c2);
        else
            DrawRectangleGradientH(x, y, w, h, c1, c2);
        return Value::nil();
    });
    add("fillGradientRect4", [](VM*, std::vector<ValuePtr> args) {
        // x y w h  r1 g1 b1 a1  r2 g2 b2 a2  r3 g3 b3 a3  r4 g4 b4 a4
        if (!graphicsWindowOpen() || args.size() < 20) return Value::nil();
        graphicsBeginFrame();
        Rectangle rec{toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        if (rec.width <= 0 || rec.height <= 0) return Value::nil();
        Color c1 = makeColor(toInt(args[4]), toInt(args[5]), toInt(args[6]), toInt(args[7]));
        Color c2 = makeColor(toInt(args[8]), toInt(args[9]), toInt(args[10]), toInt(args[11]));
        Color c3 = makeColor(toInt(args[12]), toInt(args[13]), toInt(args[14]), toInt(args[15]));
        Color c4 = makeColor(toInt(args[16]), toInt(args[17]), toInt(args[18]), toInt(args[19]));
        DrawRectangleGradientEx(rec, c1, c2, c3, c4);
        return Value::nil();
    });
    add("fillRoundedRect", [](VM*, std::vector<ValuePtr> args) {
        // x y w h roundness[0..1] segments [color...]
        if (!graphicsWindowOpen() || args.size() < 6) return Value::nil();
        graphicsBeginFrame();
        Rectangle rec{toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        float roundness = toFloat(args[4]);
        int segments = toInt(args[5]);
        if (roundness < 0.0f) roundness = 0.0f;
        if (roundness > 1.0f) roundness = 1.0f;
        if (segments < 1) segments = 1;
        int r, g, b, a; getColorFromArgs(args, 6, &r, &g, &b, &a);
        DrawRectangleRounded(rec, roundness, segments, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("strokeRoundedRect", [](VM*, std::vector<ValuePtr> args) {
        // x y w h roundness[0..1] segments thickness [color...]
        if (!graphicsWindowOpen() || args.size() < 7) return Value::nil();
        graphicsBeginFrame();
        Rectangle rec{toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        float roundness = toFloat(args[4]);
        int segments = toInt(args[5]);
        float thick = toFloat(args[6]);
        if (roundness < 0.0f) roundness = 0.0f;
        if (roundness > 1.0f) roundness = 1.0f;
        if (segments < 1) segments = 1;
        if (thick <= 0.0f) thick = 1.0f;
        int r, g, b, a; getColorFromArgs(args, 7, &r, &g, &b, &a);
        // Keep compatibility across Raylib versions where DrawRectangleRoundedLines
        // accepts only 4 args (no thickness). Approximate thickness by drawing
        // outward expanded outlines.
        int passes = std::max(1, (int)std::round(thick));
        for (int i = 0; i < passes; ++i) {
            Rectangle ri{rec.x - (float)i, rec.y - (float)i, rec.width + 2.0f * (float)i, rec.height + 2.0f * (float)i};
            DrawRectangleRoundedLines(ri, roundness, segments, makeColor(r, g, b, a));
        }
        return Value::nil();
    });
    add("fillArc", [](VM*, std::vector<ValuePtr> args) {
        // cx cy innerRadius outerRadius startDeg endDeg segments [color...]
        if (!graphicsWindowOpen() || args.size() < 7) return Value::nil();
        graphicsBeginFrame();
        Vector2 c{toFloat(args[0]), toFloat(args[1])};
        float inner = toFloat(args[2]);
        float outer = toFloat(args[3]);
        float start = toFloat(args[4]);
        float end = toFloat(args[5]);
        int segments = toInt(args[6]);
        if (inner < 0.0f) inner = 0.0f;
        if (outer < inner) std::swap(outer, inner);
        if (segments < 3) segments = 3;
        int r, g, b, a; getColorFromArgs(args, 7, &r, &g, &b, &a);
        DrawRing(c, inner, outer, start, end, segments, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("strokeArc", [](VM*, std::vector<ValuePtr> args) {
        // cx cy innerRadius outerRadius startDeg endDeg segments [color...]
        if (!graphicsWindowOpen() || args.size() < 7) return Value::nil();
        graphicsBeginFrame();
        Vector2 c{toFloat(args[0]), toFloat(args[1])};
        float inner = toFloat(args[2]);
        float outer = toFloat(args[3]);
        float start = toFloat(args[4]);
        float end = toFloat(args[5]);
        int segments = toInt(args[6]);
        if (inner < 0.0f) inner = 0.0f;
        if (outer < inner) std::swap(outer, inner);
        if (segments < 3) segments = 3;
        int r, g, b, a; getColorFromArgs(args, 7, &r, &g, &b, &a);
        DrawRingLines(c, inner, outer, start, end, segments, makeColor(r, g, b, a));
        return Value::nil();
    });

    add("createRenderTarget", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 2) return Value::fromInt(-1);
        int w = toInt(args[0]), h = toInt(args[1]);
        if (w <= 0 || h <= 0) return Value::fromInt(-1);
        RenderTexture2D rt = LoadRenderTexture(w, h);
        if (rt.id == 0) return Value::fromInt(-1);
        int64_t id = g_g2d_next_rt_id++;
        g_g2d_render_targets[id] = rt;
        return Value::fromInt(id);
    });
    add("beginRenderTarget", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.empty()) return Value::nil();
        if (g_g2d_in_render_target) {
            std::cerr << "g2d.beginRenderTarget: already inside a render target (call endRenderTarget first)." << std::endl;
            return Value::nil();
        }
        int64_t id = toInt(args[0]);
        auto it = g_g2d_render_targets.find(id);
        if (it == g_g2d_render_targets.end()) return Value::nil();
        graphicsBeginFrame();
        BeginTextureMode(it->second);
        g_g2d_in_render_target = true;
        return Value::nil();
    });
    add("endRenderTarget", [](VM*, std::vector<ValuePtr>) {
        if (!g_g2d_in_render_target) return Value::nil();
        EndTextureMode();
        g_g2d_in_render_target = false;
        return Value::nil();
    });
    add("drawRenderTarget", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        int64_t id = toInt(args[0]);
        auto it = g_g2d_render_targets.find(id);
        if (it == g_g2d_render_targets.end()) return Value::nil();
        graphicsBeginFrame();
        Texture2D tex = it->second.texture;
        float x = toFloat(args[1]), y = toFloat(args[2]);
        float dw = args.size() >= 5 ? toFloat(args[3]) : (float)tex.width;
        float dh = args.size() >= 5 ? toFloat(args[4]) : (float)tex.height;
        if (dw <= 0 || dh <= 0) return Value::nil();
        Rectangle source = { 0.0f, 0.0f, (float)tex.width, (float)-tex.height };
        Rectangle dest = { x, y, dw, dh };
        DrawTexturePro(tex, source, dest, Vector2{ 0, 0 }, 0.0f, WHITE);
        return Value::nil();
    });
    add("unloadRenderTarget", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t id = toInt(args[0]);
        auto it = g_g2d_render_targets.find(id);
        if (it == g_g2d_render_targets.end()) return Value::nil();
        if (g_g2d_in_render_target) {
            std::cerr << "g2d.unloadRenderTarget: cannot unload while rendering to a target." << std::endl;
            return Value::nil();
        }
        UnloadRenderTexture(it->second);
        g_g2d_render_targets.erase(it);
        return Value::nil();
    });

    add("text", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3 || !args[0]) return Value::nil();
        graphicsBeginFrame();
        std::string s = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : args[0]->toString();
        int x = toInt(args[1]), y = toInt(args[2]);
        int fontSize = args.size() >= 4 ? toInt(args[3]) : 20;
        int r = 255, g = 255, b = 255;
        if (args.size() >= 7) { r = toInt(args[4]); g = toInt(args[5]); b = toInt(args[6]); }
        DrawText(s.c_str(), x, y, fontSize > 0 ? fontSize : 20, makeColor(r, g, b));
        return Value::nil();
    });
    add("loadFont", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        int size = toInt(args[1]);
        Font f = LoadFontEx(path.c_str(), size > 0 ? size : 16, nullptr, 0);
        if (f.texture.id == 0) return Value::fromInt(-1);
        g_g2d_fonts.push_back(f);
        return Value::fromInt(static_cast<int64_t>(g_g2d_fonts.size() - 1));
    });
    add("drawText", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3 || !args[0]) return Value::nil();
        graphicsBeginFrame();
        std::string s = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : args[0]->toString();
        float x = toFloat(args[1]), y = toFloat(args[2]);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        if (args.size() >= 4 && args[3]) {
            int fid = toInt(args[3]);
            if (fid >= 0 && fid < (int)g_g2d_fonts.size()) {
                DrawTextEx(g_g2d_fonts[fid], s.c_str(), Vector2{ x, y }, (float)g_g2d_fonts[fid].baseSize, 1.0f, makeColor(r, g, b, a));
                return Value::nil();
            }
        }
        DrawText(s.c_str(), (int)x, (int)y, 20, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("measureText", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        std::string s = args[0] ? args[0]->toString() : "";
        int fontSize = args.size() >= 2 ? toInt(args[1]) : 20;
        if (fontSize <= 0) fontSize = 20;
        return Value::fromInt(MeasureText(s.c_str(), fontSize));
    });
    add("drawTextCentered", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        std::string s = args[0] ? args[0]->toString() : "";
        int cx = toInt(args[1]), cy = toInt(args[2]);
        int fontSize = toInt(args[3]); if (fontSize <= 0) fontSize = 20;
        int w = MeasureText(s.c_str(), fontSize);
        int r, g, b, a; getColorFromArgs(args, 4, &r, &g, &b, &a);
        DrawText(s.c_str(), cx - (w / 2), cy - (fontSize / 2), fontSize, makeColor(r, g, b, a));
        return Value::nil();
    });
    add("drawTextBox", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5 || !args[0]) return Value::nil();
        graphicsBeginFrame();
        std::string text = args[0]->toString();
        int x = toInt(args[1]), y = toInt(args[2]);
        int maxWidth = toInt(args[3]);
        int fontSize = toInt(args[4]);
        if (maxWidth <= 0) maxWidth = 1;
        if (fontSize <= 0) fontSize = 20;
        int lineHeight = args.size() >= 6 ? toInt(args[5]) : (fontSize + 4);
        if (lineHeight <= 0) lineHeight = fontSize + 4;
        int r, g, b, a; getColorFromArgs(args, 6, &r, &g, &b, &a);

        std::string current;
        int cy = y;
        auto flushLine = [&]() {
            if (!current.empty()) {
                DrawText(current.c_str(), x, cy, fontSize, makeColor(r, g, b, a));
                cy += lineHeight;
                current.clear();
            }
        };

        size_t pos = 0;
        while (pos < text.size()) {
            if (text[pos] == '\n') {
                flushLine();
                ++pos;
                continue;
            }
            size_t start = pos;
            while (pos < text.size() && text[pos] != ' ' && text[pos] != '\n' && text[pos] != '\t') ++pos;
            std::string word = text.substr(start, pos - start);
            while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
            std::string candidate = current.empty() ? word : (current + " " + word);
            if (!word.empty() && MeasureText(candidate.c_str(), fontSize) <= maxWidth) {
                current = std::move(candidate);
                continue;
            }
            flushLine();
            if (word.empty()) continue;
            if (MeasureText(word.c_str(), fontSize) <= maxWidth) {
                current = std::move(word);
                continue;
            }
            std::string chunk;
            for (char c : word) {
                std::string test = chunk + c;
                if (!chunk.empty() && MeasureText(test.c_str(), fontSize) > maxWidth) {
                    DrawText(chunk.c_str(), x, cy, fontSize, makeColor(r, g, b, a));
                    cy += lineHeight;
                    chunk.clear();
                }
                chunk.push_back(c);
            }
            current = std::move(chunk);
        }
        flushLine();
        return Value::nil();
    });

    add("load_image", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id == 0) return Value::fromInt(-1);
        g_g2d_textures.push_back(tex);
        return Value::fromInt(static_cast<int64_t>(g_g2d_textures.size() - 1));
    });
    add("loadImage", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id == 0) return Value::fromInt(-1);
        g_g2d_textures.push_back(tex);
        return Value::fromInt(static_cast<int64_t>(g_g2d_textures.size() - 1));
    });
    add("draw_image", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]);
        float x = toFloat(args[1]), y = toFloat(args[2]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        float scale = args.size() >= 4 ? toFloat(args[3]) : 1.0f;
        float rotation = args.size() >= 5 ? toFloat(args[4]) : 0.0f;
        DrawTextureEx(g_g2d_textures[id], Vector2{ x, y }, rotation, scale, WHITE);
        return Value::nil();
    });
    add("drawImage", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]);
        float x = toFloat(args[1]), y = toFloat(args[2]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        DrawTextureEx(g_g2d_textures[id], Vector2{ x, y }, 0.0f, 1.0f, WHITE);
        return Value::nil();
    });
    add("drawImageScaled", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]);
        float x = toFloat(args[1]), y = toFloat(args[2]), w = toFloat(args[3]), h = toFloat(args[4]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        Texture2D& tex = g_g2d_textures[id];
        Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
        Rectangle dst = { x, y, w, h };
        DrawTexturePro(tex, src, dst, Vector2{ 0, 0 }, 0, WHITE);
        return Value::nil();
    });
    add("drawImageRotated", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]);
        float x = toFloat(args[1]), y = toFloat(args[2]);
        float angle = args.size() >= 4 ? toFloat(args[3]) : 0.0f;
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        DrawTextureEx(g_g2d_textures[id], Vector2{ x, y }, angle, 1.0f, WHITE);
        return Value::nil();
    });
    add("spriteSheet", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromInt(-1);
        int imageId = toInt(args[0]), fw = toInt(args[1]), fh = toInt(args[2]);
        if (imageId < 0 || imageId >= (int)g_g2d_textures.size() || fw <= 0 || fh <= 0) return Value::fromInt(-1);
        g_g2d_sprites.push_back(G2DSprite{ imageId, fw, fh, 0, 0.0f });
        return Value::fromInt(static_cast<int64_t>(g_g2d_sprites.size() - 1));
    });
    add("playAnimation", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int sid = toInt(args[0]); float speed = toFloat(args[1]);
        if (sid < 0 || sid >= (int)g_g2d_sprites.size()) return Value::nil();
        G2DSprite& s = g_g2d_sprites[sid];
        int tid = s.imageId;
        if (tid < 0 || tid >= (int)g_g2d_textures.size()) return Value::nil();
        int cols = g_g2d_textures[tid].width / s.frameW;
        int rows = g_g2d_textures[tid].height / s.frameH;
        int n = cols * rows;
        if (n <= 0) return Value::nil();
        s.animTime += GetFrameTime() * speed;
        s.currentFrame = (int)(s.animTime) % n;
        if (s.currentFrame < 0) s.currentFrame += n;
        return Value::nil();
    });
    add("drawSprite", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int sid = toInt(args[0]);
        float x = toFloat(args[1]), y = toFloat(args[2]);
        if (sid < 0 || sid >= (int)g_g2d_sprites.size()) return Value::nil();
        G2DSprite& s = g_g2d_sprites[sid];
        int tid = s.imageId;
        if (tid < 0 || tid >= (int)g_g2d_textures.size()) return Value::nil();
        int cols = g_g2d_textures[tid].width / s.frameW;
        int row = s.currentFrame / cols, col = s.currentFrame % cols;
        Rectangle src = { (float)(col * s.frameW), (float)(row * s.frameH), (float)s.frameW, (float)s.frameH };
        Rectangle dst = { x, y, (float)s.frameW, (float)s.frameH };
        DrawTexturePro(g_g2d_textures[tid], src, dst, Vector2{ 0, 0 }, 0, WHITE);
        return Value::nil();
    });

    add("isKeyPressed", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        int k = keyNameToRaylib(toString(args[0]));
        return Value::fromBool(k != 0 && IsKeyPressed(k));
    });
    add("isKeyDown", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        int k = keyNameToRaylib(toString(args[0]));
        return Value::fromBool(k != 0 && IsKeyDown(k));
    });
    add("mouseX", [](VM*, std::vector<ValuePtr>) { return Value::fromInt(GetMouseX()); });
    add("mouseY", [](VM*, std::vector<ValuePtr>) { return Value::fromInt(GetMouseY()); });
    add("isMousePressed", [](VM*, std::vector<ValuePtr> args) {
        int btn = args.empty() ? MOUSE_BUTTON_LEFT : mouseButtonFromArg(args[0]);
        return Value::fromBool(IsMouseButtonPressed(btn));
    });
    add("isMouseDown", [](VM*, std::vector<ValuePtr> args) {
        int btn = args.empty() ? MOUSE_BUTTON_LEFT : mouseButtonFromArg(args[0]);
        return Value::fromBool(IsMouseButtonDown(btn));
    });
    // back-compat aliases used by older BrowserKit demos.
    add("mousePressed", [](VM*, std::vector<ValuePtr> args) {
        int btn = args.empty() ? MOUSE_BUTTON_LEFT : mouseButtonFromArg(args[0]);
        return Value::fromBool(IsMouseButtonPressed(btn));
    });
    add("mouseDown", [](VM*, std::vector<ValuePtr> args) {
        int btn = args.empty() ? MOUSE_BUTTON_LEFT : mouseButtonFromArg(args[0]);
        return Value::fromBool(IsMouseButtonDown(btn));
    });
    add("mousePos", [](VM*, std::vector<ValuePtr>) {
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        auto& out = std::get<std::vector<ValuePtr>>(arr->data);
        out.push_back(std::make_shared<Value>(Value::fromInt(GetMouseX())));
        out.push_back(std::make_shared<Value>(Value::fromInt(GetMouseY())));
        return Value(*arr);
    });
    add("mouseWheel", [](VM*, std::vector<ValuePtr>) {
        return Value::fromFloat(GetMouseWheelMove());
    });
    add("rectsOverlap", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 8) return Value::fromBool(false);
        float x1 = toFloat(args[0]), y1 = toFloat(args[1]), w1 = toFloat(args[2]), h1 = toFloat(args[3]);
        float x2 = toFloat(args[4]), y2 = toFloat(args[5]), w2 = toFloat(args[6]), h2 = toFloat(args[7]);
        bool hit = (x1 < x2 + w2) && (x1 + w1 > x2) && (y1 < y2 + h2) && (y1 + h1 > y2);
        return Value::fromBool(hit);
    });
    add("pointInRect", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::fromBool(false);
        float px = toFloat(args[0]), py = toFloat(args[1]);
        float x = toFloat(args[2]), y = toFloat(args[3]), w = toFloat(args[4]), h = toFloat(args[5]);
        bool hit = (px >= x) && (px <= x + w) && (py >= y) && (py <= y + h);
        return Value::fromBool(hit);
    });
    add("circlesOverlap", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::fromBool(false);
        float x1 = toFloat(args[0]), y1 = toFloat(args[1]), r1 = toFloat(args[2]);
        float x2 = toFloat(args[3]), y2 = toFloat(args[4]), r2 = toFloat(args[5]);
        float dx = x1 - x2, dy = y1 - y2;
        float rr = r1 + r2;
        bool hit = (dx * dx + dy * dy) <= (rr * rr);
        return Value::fromBool(hit);
    });

    add("setCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2) {
            graphicsSetCameraEnabled(true);
            g_g2d_camera_enabled = true;
            g_g2d_camera_has_target = true;
            g_g2d_camera_target_x = toFloat(args[0]);
            g_g2d_camera_target_y = toFloat(args[1]);
            applyG2dCameraTarget();
        }
        return Value::nil();
    });
    add("moveCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2) {
            if (!g_g2d_camera_has_target) {
                g_g2d_camera_has_target = true;
                g_g2d_camera_target_x = 0.0f;
                g_g2d_camera_target_y = 0.0f;
            }
            g_g2d_camera_target_x += toFloat(args[0]);
            g_g2d_camera_target_y += toFloat(args[1]);
            g_g2d_camera_enabled = true;
            graphicsSetCameraEnabled(true);
            applyG2dCameraTarget();
        }
        return Value::nil();
    });
    add("setCameraBounds", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) {
            g_g2d_camera_bounds_enabled = false;
            return Value::nil();
        }
        float minX = toFloat(args[0]), minY = toFloat(args[1]);
        float maxX = toFloat(args[2]), maxY = toFloat(args[3]);
        if (maxX < minX) std::swap(maxX, minX);
        if (maxY < minY) std::swap(maxY, minY);
        g_g2d_camera_min_x = minX;
        g_g2d_camera_min_y = minY;
        g_g2d_camera_max_x = maxX;
        g_g2d_camera_max_y = maxY;
        g_g2d_camera_bounds_enabled = true;
        applyG2dCameraTarget();
        return Value::nil();
    });
    add("clearCameraBounds", [](VM*, std::vector<ValuePtr>) {
        g_g2d_camera_bounds_enabled = false;
        return Value::nil();
    });
    add("screenShake", [](VM*, std::vector<ValuePtr> args) {
        float strength = args.size() >= 1 ? toFloat(args[0]) : 0.0f;
        float duration = args.size() >= 2 ? toFloat(args[1]) : 0.2f;
        if (strength < 0.0f) strength = -strength;
        if (duration < 0.0f) duration = 0.0f;
        g_g2d_shake_strength = strength;
        g_g2d_shake_time_left = duration;
        return Value::nil();
    });
    add("zoomCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 1) graphicsSetCameraZoom(toFloat(args[0]));
        return Value::nil();
    });

    add("rgb", [](VM*, std::vector<ValuePtr> args) {
        int r = args.size() >= 1 ? toInt(args[0]) : 255;
        int g = args.size() >= 2 ? toInt(args[1]) : 255;
        int b = args.size() >= 3 ? toInt(args[2]) : 255;
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(r)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(g)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(b)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(255)));
        return Value(*arr);
    });
    add("rgba", [](VM*, std::vector<ValuePtr> args) {
        int r = args.size() >= 1 ? toInt(args[0]) : 255;
        int g = args.size() >= 2 ? toInt(args[1]) : 255;
        int b = args.size() >= 3 ? toInt(args[2]) : 255;
        int a = args.size() >= 4 ? toInt(args[3]) : 255;
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(r)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(g)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(b)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(a)));
        return Value(*arr);
    });
    add("hexColor", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string hex = std::get<std::string>(args[0]->data);
        while (!hex.empty() && hex[0] == '#') hex.erase(0, 1);
        auto toHex = [](char c) { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : 0; };
        int r = 255, g = 255, b = 255, a = 255;
        if (hex.size() >= 6) {
            r = toHex(hex[0]) * 16 + toHex(hex[1]);
            g = toHex(hex[2]) * 16 + toHex(hex[3]);
            b = toHex(hex[4]) * 16 + toHex(hex[5]);
        }
        if (hex.size() >= 8) a = toHex(hex[6]) * 16 + toHex(hex[7]);
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(r)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(g)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(b)));
        std::get<std::vector<ValuePtr>>(arr->data).push_back(std::make_shared<Value>(Value::fromInt(a)));
        return Value(*arr);
    });

    add("width", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(graphicsWindowOpen() ? GetScreenWidth() : 0);
    });
    add("height", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(graphicsWindowOpen() ? GetScreenHeight() : 0);
    });

    add("present", [](VM*, std::vector<ValuePtr>) {
        graphicsEndFrame();
        return Value::nil();
    });

    add("run", [](VM* vm, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::nil();
        if (!vm || args.size() < 2) return Value::nil();
        ValuePtr updateFn = args[0], drawFn = args[1];
        bool updateByName = updateFn && updateFn->type == Value::Type::STRING;
        bool drawByName = drawFn && drawFn->type == Value::Type::STRING;
        std::string updateName = updateByName ? std::get<std::string>(updateFn->data) : "";
        std::string drawName = drawByName ? std::get<std::string>(drawFn->data) : "";
        if (!updateByName && (!updateFn || updateFn->type != Value::Type::FUNCTION))
            return Value::nil();
        if (!drawByName && (!drawFn || drawFn->type != Value::Type::FUNCTION))
            return Value::nil();
        uint64_t savedGuard = vm->getCallbackStepGuard();
        // g2d update/draw callbacks can be instruction-heavy (especially script-driven
        // layout/paint stacks). Lift guard while the render loop is active.
        const uint64_t kG2dMinGuard = 5000000;
        if (savedGuard < kG2dMinGuard) vm->setCallbackStepGuard(kG2dMinGuard);
        SetTargetFPS(60);
        while (graphicsWindowOpen() && !WindowShouldClose()) {
            bool began2d = false;
            try {
                ValuePtr u = updateByName ? vm->getGlobal(updateName) : updateFn;
                ValuePtr d = drawByName ? vm->getGlobal(drawName) : drawFn;
                if (!u || u->type != Value::Type::FUNCTION || !d || d->type != Value::Type::FUNCTION) {
                    std::cerr << "g2d.run: callback missing/invalid (update='" << updateName
                              << "', draw='" << drawName << "')." << std::endl;
                    break;
                }
                vm->callValue(u, {});
                graphicsBeginFrame();
                if (g_g2d_camera_enabled) {
                    graphicsBegin2D();
                    began2d = true;
                }
                vm->callValue(d, {});
                if (began2d) graphicsEnd2D();
                graphicsEndFrame();
            } catch (const std::exception& e) {
                std::cerr << "g2d.run: callback error: " << e.what() << std::endl;
                if (began2d) graphicsEnd2D();
                graphicsEndFrame();
                break;
            } catch (...) {
                std::cerr << "g2d.run: callback error: unknown exception." << std::endl;
                if (began2d) graphicsEnd2D();
                graphicsEndFrame();
                break;
            }
        }
        vm->setCallbackStepGuard(savedGuard);
        return Value::nil();
    });

    add("delta_time", [](VM*, std::vector<ValuePtr>) {
        return Value::fromFloat(GetFrameTime());
    });
    add("fps", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(GetFPS());
    });
    add("set_title", [](VM*, std::vector<ValuePtr> args) {
        if (graphicsWindowOpen() && args.size() >= 1)
            SetWindowTitle(toString(args[0]).c_str());
        return Value::nil();
    });
}

static std::unordered_map<std::string, ValuePtr> s_g2dModuleMap;

ValuePtr create2dGraphicsModule(VM& vm) {
    // The module map can be cached, but builtin registration is per-VM. Always ensure the
    // current VM has the g2d builtins registered so cached FunctionObjects don't point at
    // indices that are missing in this VM instance.
    register2dGraphicsToVM(vm, [](const std::string& name, ValuePtr v) { s_g2dModuleMap[name] = v; });
    return std::make_shared<Value>(Value::fromMap(s_g2dModuleMap));
}

} // namespace kern
