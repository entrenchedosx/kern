/* *
 * kern Game builtins implementation (Raylib).
 * builtin indices 55+ to avoid clashing with core builtins.
 */
#include "game_builtins.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cctype>

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

static std::string normalizeKeyName(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static int keyNameToRaylib(const std::string& s) {
    static const std::unordered_map<std::string, int> m = {
        {"LEFT", KEY_LEFT}, {"RIGHT", KEY_RIGHT}, {"UP", KEY_UP}, {"DOWN", KEY_DOWN},
        {"SPACE", KEY_SPACE}, {"ENTER", KEY_ENTER}, {"ESCAPE", KEY_ESCAPE}, {"TAB", KEY_TAB},
        {"BACKSPACE", KEY_BACKSPACE}, {"DELETE", KEY_DELETE},
        {"A", KEY_A}, {"B", KEY_B}, {"C", KEY_C}, {"D", KEY_D}, {"E", KEY_E}, {"F", KEY_F},
        {"G", KEY_G}, {"H", KEY_H}, {"I", KEY_I}, {"J", KEY_J}, {"K", KEY_K}, {"L", KEY_L},
        {"M", KEY_M}, {"N", KEY_N}, {"O", KEY_O}, {"P", KEY_P}, {"Q", KEY_Q}, {"R", KEY_R},
        {"S", KEY_S}, {"T", KEY_T}, {"U", KEY_U}, {"V", KEY_V}, {"W", KEY_W}, {"X", KEY_X},
        {"Y", KEY_Y}, {"Z", KEY_Z},
        {"ZERO", KEY_ZERO}, {"ONE", KEY_ONE}, {"TWO", KEY_TWO}, {"THREE", KEY_THREE},
        {"FOUR", KEY_FOUR}, {"FIVE", KEY_FIVE}, {"SIX", KEY_SIX}, {"SEVEN", KEY_SEVEN},
        {"EIGHT", KEY_EIGHT}, {"NINE", KEY_NINE},
        {"MINUS", KEY_MINUS}, {"EQUAL", KEY_EQUAL},
    };
    auto it = m.find(normalizeKeyName(s));
    return it != m.end() ? it->second : 0;
}

static std::vector<Texture2D> g_textures;
static std::vector<Sound> g_sounds;
static bool g_window_created = false;
static bool g_in_game_loop_frame = false;   // true between BeginDrawing/EndDrawing inside game_loop
static bool g_standalone_frame_active = false;  // true when we started a frame from clear/draw (no game_loop)
static Camera2D g_camera = { {0, 0}, {0, 0}, 0, 1.0f };
static bool g_camera_enabled = false;
static float g_camera_zoom = 1.0f;
static bool g_debug_overlay = false;
static std::unordered_map<std::string, int> g_action_to_key;

// make sure we're inside a drawing frame (BeginDrawing already called). If not in game_loop, start a standalone frame.
static void ensureDrawingFrame() {
    if (!g_window_created || g_in_game_loop_frame || g_standalone_frame_active) return;
    BeginDrawing();
    g_standalone_frame_active = true;
}

static Color makeColor(int r, int g, int b, int a = 255) {
    return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
}

std::vector<std::string> getGameBuiltinNames() {
    return {
        "window_create", "window_close", "window_should_close", "window_open",
        "window_set_size", "window_toggle_fullscreen",
        "clear", "draw_rect", "draw_circle", "draw_line", "draw_triangle", "present",
        "load_image", "draw_image", "draw_image_ex",
        "key_down", "key_pressed", "key_released",
        "mouse_x", "mouse_y", "mouse_pressed", "mouse_down",
        "set_target_fps", "delta_time", "input_chars", "screen_width", "screen_height",
        "load_sound", "play_sound",
        "draw_text", "rect_collision", "circle_collision", "set_pixel", "game_loop",
        "camera_set", "camera_zoom", "get_fps", "debug_overlay",
        "set_master_volume", "bind_action", "action_pressed", "action_down",
        "is_gamepad_available", "gamepad_axis", "gamepad_button_down", "gamepad_button_pressed"
    };
}

// register each game builtin with the VM (indices after core builtins 0..147 to avoid clash).
static void registerGameBuiltinsToVM(VM& vm, std::function<void(const std::string&, ValuePtr)> onFn) {
    const size_t GAME_BASE = 240;
    size_t i = GAME_BASE;
    auto addGameFn = [&vm, &i, &onFn](const std::string& name, VM::BuiltinFn fn) {
        vm.registerBuiltin(i, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = i++;
        onFn(name, std::make_shared<Value>(Value::fromFunction(f)));
    };

    addGameFn("window_create", [](VM*, std::vector<ValuePtr> args) {
        int w = args.size() >= 1 ? toInt(args[0]) : 800;
        int h = args.size() >= 2 ? toInt(args[1]) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern Game";
        graphicsInitWindow(w, h, title);
        return Value::nil();
    });

    addGameFn("window_close", [](VM*, std::vector<ValuePtr>) {
        graphicsCloseWindow();
        return Value::nil();
    });

    addGameFn("window_should_close", [](VM*, std::vector<ValuePtr>) {
        return Value::fromBool(g_window_created && WindowShouldClose());
    });

    addGameFn("window_open", [](VM*, std::vector<ValuePtr>) {
        return Value::fromBool(g_window_created && !WindowShouldClose());
    });

    addGameFn("window_set_size", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2 && g_window_created)
            SetWindowSize(toInt(args[0]), toInt(args[1]));
        return Value::nil();
    });

    addGameFn("window_toggle_fullscreen", [](VM*, std::vector<ValuePtr>) {
        if (g_window_created) ToggleFullscreen();
        return Value::nil();
    });

    addGameFn("clear", [](VM*, std::vector<ValuePtr> args) {
        int r = args.size() >= 1 ? toInt(args[0]) : 0;
        int g = args.size() >= 2 ? toInt(args[1]) : 0;
        int b = args.size() >= 3 ? toInt(args[2]) : 0;
        if (g_window_created) {
            ensureDrawingFrame();
            ClearBackground(makeColor(r, g, b));
        }
        return Value::nil();
    });

    addGameFn("draw_rect", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 6) return Value::nil();
        ensureDrawingFrame();
        int x = toInt(args[0]), y = toInt(args[1]), w = toInt(args[2]), h = toInt(args[3]);
        int r = toInt(args[4]), g = toInt(args[5]), b = args.size() > 6 ? toInt(args[6]) : 255;
        int a = args.size() > 7 ? toInt(args[7]) : 255;
        DrawRectangle(x, y, w, h, makeColor(r, g, b, a));
        return Value::nil();
    });

    addGameFn("draw_circle", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 5) return Value::nil();
        ensureDrawingFrame();
        int cx = toInt(args[0]), cy = toInt(args[1]), radius = toInt(args[2]);
        int r = toInt(args[3]), g = toInt(args[4]), b = (args.size() > 5) ? toInt(args[5]) : 255;
        int a = (args.size() > 6) ? toInt(args[6]) : 255;
        DrawCircle(cx, cy, (float)radius, makeColor(r, g, b, a));
        return Value::nil();
    });

    addGameFn("draw_line", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 6) return Value::nil();
        ensureDrawingFrame();
        int x1 = toInt(args[0]), y1 = toInt(args[1]), x2 = toInt(args[2]), y2 = toInt(args[3]);
        int r = toInt(args[4]), g = toInt(args[5]), b = (args.size() > 6) ? toInt(args[6]) : 255;
        int a = (args.size() > 7) ? toInt(args[7]) : 255;
        DrawLine(x1, y1, x2, y2, makeColor(r, g, b, a));
        return Value::nil();
    });

    addGameFn("draw_triangle", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 10) return Value::nil();
        ensureDrawingFrame();
        Vector2 v1 = { toFloat(args[0]), toFloat(args[1]) };
        Vector2 v2 = { toFloat(args[2]), toFloat(args[3]) };
        Vector2 v3 = { toFloat(args[4]), toFloat(args[5]) };
        int r = toInt(args[6]), g = toInt(args[7]), b = toInt(args[8]);
        int a = args.size() > 9 ? toInt(args[9]) : 255;
        DrawTriangle(v1, v2, v3, makeColor(r, g, b, a));
        return Value::nil();
    });

    addGameFn("present", [](VM*, std::vector<ValuePtr>) {
        if (g_standalone_frame_active) {
            EndDrawing();
            g_standalone_frame_active = false;
        }
        return Value::nil();
    });

    addGameFn("load_image", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id == 0) return Value::fromInt(-1);
        g_textures.push_back(tex);
        return Value::fromInt(static_cast<int64_t>(g_textures.size() - 1));
    });

    addGameFn("draw_image", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 3) return Value::nil();
        ensureDrawingFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        if (id < 0 || id >= (int)g_textures.size()) return Value::nil();
        DrawTexture(g_textures[id], x, y, WHITE);
        return Value::nil();
    });

    addGameFn("draw_image_ex", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 6) return Value::nil();
        ensureDrawingFrame();
        int id = toInt(args[0]), x = toInt(args[1]), y = toInt(args[2]);
        float scale = args.size() > 3 ? toFloat(args[3]) : 1.0f;
        float rotation = args.size() > 4 ? toFloat(args[4]) : 0.0f;
        if (id < 0 || id >= (int)g_textures.size()) return Value::nil();
        DrawTextureEx(g_textures[id], Vector2{ (float)x, (float)y }, rotation, scale, WHITE);
        return Value::nil();
    });

    addGameFn("key_down", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        int key = keyNameToRaylib(std::get<std::string>(args[0]->data));
        return Value::fromBool(key != 0 && IsKeyDown(key));
    });

    addGameFn("key_pressed", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        int key = keyNameToRaylib(std::get<std::string>(args[0]->data));
        return Value::fromBool(key != 0 && IsKeyPressed(key));
    });

    addGameFn("key_released", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        int key = keyNameToRaylib(std::get<std::string>(args[0]->data));
        return Value::fromBool(key != 0 && IsKeyReleased(key));
    });

    addGameFn("mouse_x", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(GetMouseX());
    });

    addGameFn("mouse_y", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(GetMouseY());
    });

    addGameFn("mouse_pressed", [](VM*, std::vector<ValuePtr> args) {
        int btn = MOUSE_BUTTON_LEFT;
        if (args.size() >= 1 && args[0] && args[0]->type == Value::Type::STRING) {
            std::string s = std::get<std::string>(args[0]->data);
            if (s == "RIGHT") btn = MOUSE_BUTTON_RIGHT;
            else if (s == "MIDDLE") btn = MOUSE_BUTTON_MIDDLE;
        }
        return Value::fromBool(IsMouseButtonPressed(btn));
    });

    addGameFn("mouse_down", [](VM*, std::vector<ValuePtr> args) {
        int btn = MOUSE_BUTTON_LEFT;
        if (args.size() >= 1 && args[0] && args[0]->type == Value::Type::STRING) {
            std::string s = std::get<std::string>(args[0]->data);
            if (s == "RIGHT") btn = MOUSE_BUTTON_RIGHT;
            else if (s == "MIDDLE") btn = MOUSE_BUTTON_MIDDLE;
        }
        return Value::fromBool(IsMouseButtonDown(btn));
    });

    addGameFn("set_target_fps", [](VM*, std::vector<ValuePtr> args) {
        int fps = args.empty() ? 60 : toInt(args[0]);
        SetTargetFPS(fps > 0 ? fps : 60);
        return Value::nil();
    });

    addGameFn("delta_time", [](VM*, std::vector<ValuePtr>) {
        return Value::fromFloat(GetFrameTime());
    });

    addGameFn("input_chars", [](VM*, std::vector<ValuePtr>) {
        std::string out;
        int c;
        while ((c = GetCharPressed()) != 0) {
            if (c >= 32 && c < 127)
                out += static_cast<char>(c);
        }
        return Value::fromString(std::move(out));
    });

    addGameFn("screen_width", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(g_window_created ? GetScreenWidth() : 0);
    });

    addGameFn("screen_height", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(g_window_created ? GetScreenHeight() : 0);
    });

    addGameFn("load_sound", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(-1);
        std::string path = std::get<std::string>(args[0]->data);
        Sound snd = LoadSound(path.c_str());
        g_sounds.push_back(snd);
        return Value::fromInt(static_cast<int64_t>(g_sounds.size() - 1));
    });

    addGameFn("play_sound", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int id = toInt(args[0]);
        if (id >= 0 && id < (int)g_sounds.size()) PlaySound(g_sounds[id]);
        return Value::nil();
    });

    addGameFn("draw_text", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 4 || !args[0]) return Value::nil();
        ensureDrawingFrame();
        std::string text = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : args[0]->toString();
        int x = toInt(args[1]), y = toInt(args[2]), fontSize = toInt(args[3]);
        int r = 255, g = 255, b = 255;
        if (args.size() >= 7) { r = toInt(args[4]); g = toInt(args[5]); b = toInt(args[6]); }
        DrawText(text.c_str(), x, y, fontSize > 0 ? fontSize : 20, makeColor(r, g, b));
        return Value::nil();
    });

    addGameFn("rect_collision", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 8) return Value::fromBool(false);
        Rectangle a = { toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]) };
        Rectangle b = { toFloat(args[4]), toFloat(args[5]), toFloat(args[6]), toFloat(args[7]) };
        return Value::fromBool(CheckCollisionRecs(a, b));
    });

    addGameFn("circle_collision", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::fromBool(false);
        Vector2 c1 = { toFloat(args[0]), toFloat(args[1]) };
        float r1 = toFloat(args[2]);
        Vector2 c2 = { toFloat(args[3]), toFloat(args[4]) };
        float r2 = toFloat(args[5]);
        return Value::fromBool(CheckCollisionCircles(c1, r1, c2, r2));
    });

    addGameFn("set_pixel", [](VM*, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 5) return Value::nil();
        ensureDrawingFrame();
        int x = toInt(args[0]), y = toInt(args[1]);
        int r = toInt(args[2]), g = toInt(args[3]), b = args.size() > 4 ? toInt(args[4]) : 255;
        DrawPixel(x, y, makeColor(r, g, b));
        return Value::nil();
    });

    addGameFn("camera_set", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        g_camera.target.x = toFloat(args[0]);
        g_camera.target.y = toFloat(args[1]);
        g_camera_enabled = true;
        return Value::nil();
    });

    addGameFn("camera_zoom", [](VM*, std::vector<ValuePtr> args) {
        g_camera_zoom = args.empty() ? 1.0f : toFloat(args[0]);
        if (g_camera_zoom <= 0) g_camera_zoom = 1.0f;
        return Value::nil();
    });

    addGameFn("get_fps", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(GetFPS());
    });

    addGameFn("debug_overlay", [](VM*, std::vector<ValuePtr> args) {
        g_debug_overlay = !args.empty() && args[0] && args[0]->isTruthy();
        return Value::nil();
    });

    addGameFn("set_master_volume", [](VM*, std::vector<ValuePtr> args) {
        float v = args.empty() ? 1.0f : toFloat(args[0]);
        if (v < 0) v = 0; if (v > 1) v = 1;
        SetMasterVolume(v);
        return Value::nil();
    });

    addGameFn("bind_action", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING)
            return Value::nil();
        std::string action = std::get<std::string>(args[0]->data);
        int key = keyNameToRaylib(std::get<std::string>(args[1]->data));
        if (key != 0) g_action_to_key[action] = key;
        return Value::nil();
    });

    addGameFn("action_pressed", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        auto it = g_action_to_key.find(std::get<std::string>(args[0]->data));
        return Value::fromBool(it != g_action_to_key.end() && IsKeyPressed(it->second));
    });

    addGameFn("action_down", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        auto it = g_action_to_key.find(std::get<std::string>(args[0]->data));
        return Value::fromBool(it != g_action_to_key.end() && IsKeyDown(it->second));
    });

    addGameFn("is_gamepad_available", [](VM*, std::vector<ValuePtr> args) {
        int id = args.empty() ? 0 : toInt(args[0]);
        return Value::fromBool(IsGamepadAvailable(id));
    });

    addGameFn("gamepad_axis", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        int id = toInt(args[0]), axis = toInt(args[1]);
        return Value::fromFloat(GetGamepadAxisMovement(id, axis));
    });

    addGameFn("gamepad_button_down", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(false);
        int id = toInt(args[0]), btn = toInt(args[1]);
        return Value::fromBool(IsGamepadButtonDown(id, btn));
    });

    addGameFn("gamepad_button_pressed", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(false);
        int id = toInt(args[0]), btn = toInt(args[1]);
        return Value::fromBool(IsGamepadButtonPressed(id, btn));
    });

    addGameFn("game_loop", [](VM* vm, std::vector<ValuePtr> args) {
        if (!g_window_created || args.size() < 2 || !vm) return Value::nil();
        ValuePtr updateFn = args[0];
        ValuePtr renderFn = args[1];
        if (!updateFn || updateFn->type != Value::Type::FUNCTION ||
            !renderFn || renderFn->type != Value::Type::FUNCTION)
            return Value::nil();
        SetTargetFPS(60);
        while (!WindowShouldClose()) {
            vm->callValue(updateFn, {});
            g_camera.offset.x = (float)GetScreenWidth() / 2.0f;
            g_camera.offset.y = (float)GetScreenHeight() / 2.0f;
            g_camera.zoom = g_camera_zoom;
            BeginDrawing();
            g_in_game_loop_frame = true;
            if (g_camera_enabled) BeginMode2D(g_camera);
            vm->callValue(renderFn, {});
            if (g_camera_enabled) EndMode2D();
            if (g_debug_overlay) DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, LIME);
            g_in_game_loop_frame = false;
            EndDrawing();
        }
        return Value::nil();
    });
}

static std::unordered_map<std::string, ValuePtr> s_gameModuleMap;

ValuePtr createGameModule(VM& vm) {
    if (s_gameModuleMap.empty())
        registerGameBuiltinsToVM(vm, [](const std::string& name, ValuePtr v) { s_gameModuleMap[name] = v; });
    return std::make_shared<Value>(Value::fromMap(s_gameModuleMap));
}

void registerGameBuiltins(VM& vm) {
    createGameModule(vm);
    for (auto& kv : s_gameModuleMap)
        vm.setGlobal(kv.first, kv.second);
}

bool graphicsWindowOpen() { return g_window_created; }
void graphicsInitWindow(int width, int height, const std::string& title) {
    if (g_window_created) return;
    InitWindow(width, height, title.c_str());
    g_window_created = true;
}
void graphicsCloseWindow() {
    if (g_window_created) {
        g_standalone_frame_active = false;
        g_in_game_loop_frame = false;
        CloseWindow();
        g_window_created = false;
    }
}

void graphicsBeginFrame() { ensureDrawingFrame(); }
void graphicsEndFrame() {
    if (g_standalone_frame_active) {
        EndDrawing();
        g_standalone_frame_active = false;
    }
}

void graphicsSetCameraTarget(float x, float y) {
    g_camera.target.x = x;
    g_camera.target.y = y;
    g_camera_enabled = true;
}
void graphicsMoveCameraTarget(float dx, float dy) {
    g_camera.target.x += dx;
    g_camera.target.y += dy;
    g_camera_enabled = true;
}
void graphicsSetCameraZoom(float zoom) {
    g_camera_zoom = zoom <= 0 ? 1.0f : zoom;
}
void graphicsSetCameraEnabled(bool enable) { g_camera_enabled = enable; }
void graphicsBegin2D() {
    if (!g_window_created) return;
    g_camera.offset.x = (float)GetScreenWidth() / 2.0f;
    g_camera.offset.y = (float)GetScreenHeight() / 2.0f;
    g_camera.zoom = g_camera_zoom;
    BeginMode2D(g_camera);
}
void graphicsEnd2D() { EndMode2D(); }
void graphicsSetWindowSize(int width, int height) {
    if (g_window_created) SetWindowSize(width, height);
}
void graphicsToggleFullscreen() {
    if (g_window_created) ToggleFullscreen();
}

} // namespace kern
