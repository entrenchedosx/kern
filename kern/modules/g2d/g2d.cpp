/* *
 * kern g2d module – implementation (window, renderer, shapes, text, colors).
 * backend: game_builtins (Raylib). Single compilation unit for shared state.
 */
#include "g2d.h"
#include "game/game_builtins.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cctype>
#include <cstdlib>

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
    auto it = m.find(s);
    return it != m.end() ? it->second : 0;
}

static std::vector<Texture2D> g_g2d_textures;
static std::vector<Font> g_g2d_fonts;
struct G2DSprite { int imageId; int frameW; int frameH; int currentFrame; float animTime; };
static std::vector<G2DSprite> g_g2d_sprites;
static int g_g2d_draw_r = 255, g_g2d_draw_g = 255, g_g2d_draw_b = 255, g_g2d_draw_a = 255;
static bool g_g2d_camera_enabled = false;
static const size_t G2D_BASE = 284;

static void getColorFromArgs(const std::vector<ValuePtr>& args, size_t start, int* r, int* g, int* b, int* a) {
    if (start >= args.size()) { *r = g_g2d_draw_r; *g = g_g2d_draw_g; *b = g_g2d_draw_b; *a = g_g2d_draw_a; return; }
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

    add("close", [](VM*, std::vector<ValuePtr>) {
        graphicsCloseWindow();
        return Value::nil();
    });
    add("closeWindow", [](VM*, std::vector<ValuePtr>) {
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
        graphicsBeginFrame();
        if (g_g2d_camera_enabled) graphicsBegin2D();
        return Value::nil();
    });
    add("endFrame", [](VM*, std::vector<ValuePtr>) {
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

    add("text", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
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
        if (args.size() < 2 || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        int size = toInt(args[1]);
        Font f = LoadFontEx(path.c_str(), size > 0 ? size : 16, nullptr, 0);
        if (f.texture.id == 0) return Value::fromInt(-1);
        g_g2d_fonts.push_back(f);
        return Value::fromInt(static_cast<int64_t>(g_g2d_fonts.size() - 1));
    });
    add("drawText", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
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

    add("load_image", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id == 0) return Value::fromInt(-1);
        g_g2d_textures.push_back(tex);
        return Value::fromInt(static_cast<int64_t>(g_g2d_textures.size() - 1));
    });
    add("loadImage", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id == 0) return Value::fromInt(-1);
        g_g2d_textures.push_back(tex);
        return Value::fromInt(static_cast<int64_t>(g_g2d_textures.size() - 1));
    });
    add("draw_image", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        float scale = args.size() >= 4 ? toFloat(args[3]) : 1.0f;
        float rotation = args.size() >= 5 ? toFloat(args[4]) : 0.0f;
        if (scale != 1.0f || rotation != 0.0f)
            DrawTextureEx(g_g2d_textures[id], Vector2{ (float)x, (float)y }, rotation, scale, WHITE);
        else
            DrawTexture(g_g2d_textures[id], x, y, WHITE);
        return Value::nil();
    });
    add("drawImage", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        DrawTexture(g_g2d_textures[id], x, y, WHITE);
        return Value::nil();
    });
    add("drawImageScaled", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]), w = toInt(args[3]), h = toInt(args[4]);
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        Texture2D& tex = g_g2d_textures[id];
        Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
        Rectangle dst = { (float)x, (float)y, (float)w, (float)h };
        DrawTexturePro(tex, src, dst, Vector2{ 0, 0 }, 0, WHITE);
        return Value::nil();
    });
    add("drawImageRotated", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        graphicsBeginFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        float angle = args.size() >= 4 ? toFloat(args[3]) : 0.0f;
        if (id < 0 || id >= (int)g_g2d_textures.size()) return Value::nil();
        DrawTextureEx(g_g2d_textures[id], Vector2{ (float)x, (float)y }, angle, 1.0f, WHITE);
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
        int sid = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        if (sid < 0 || sid >= (int)g_g2d_sprites.size()) return Value::nil();
        G2DSprite& s = g_g2d_sprites[sid];
        int tid = s.imageId;
        if (tid < 0 || tid >= (int)g_g2d_textures.size()) return Value::nil();
        int cols = g_g2d_textures[tid].width / s.frameW;
        int row = s.currentFrame / cols, col = s.currentFrame % cols;
        Rectangle src = { (float)(col * s.frameW), (float)(row * s.frameH), (float)s.frameW, (float)s.frameH };
        Rectangle dst = { (float)x, (float)y, (float)s.frameW, (float)s.frameH };
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
        int btn = args.empty() ? 0 : toInt(args[0]);
        return Value::fromBool(IsMouseButtonPressed(btn));
    });

    add("setCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2) {
            graphicsSetCameraTarget(toFloat(args[0]), toFloat(args[1]));
            graphicsSetCameraEnabled(true);
            g_g2d_camera_enabled = true;
        }
        return Value::nil();
    });
    add("moveCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2) graphicsMoveCameraTarget(toFloat(args[0]), toFloat(args[1]));
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
        if (args.empty() || args[0]->type != Value::Type::STRING) return Value::nil();
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
        if (!updateFn || updateFn->type != Value::Type::FUNCTION ||
            !drawFn || drawFn->type != Value::Type::FUNCTION)
            return Value::nil();
        SetTargetFPS(60);
        while (!WindowShouldClose()) {
            bool began2d = false;
            try {
                vm->callValue(updateFn, {});
                graphicsBeginFrame();
                if (g_g2d_camera_enabled) {
                    graphicsBegin2D();
                    began2d = true;
                }
                vm->callValue(drawFn, {});
                if (began2d) graphicsEnd2D();
                graphicsEndFrame();
            } catch (const std::exception&) {
                if (began2d) graphicsEnd2D();
                graphicsEndFrame();
                break;
            }
        }
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
    if (s_g2dModuleMap.empty())
        register2dGraphicsToVM(vm, [](const std::string& name, ValuePtr v) { s_g2dModuleMap[name] = v; });
    return std::make_shared<Value>(Value::fromMap(s_g2dModuleMap));
}

} // namespace kern
