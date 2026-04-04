/* *
 * kern g3d module – minimal 3D (window, camera, basic shapes).
 * backend: Raylib via game_builtins window helpers.
 */
#include "g3d.h"
#include "game/game_builtins.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

static int toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return static_cast<int>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::FLOAT) return static_cast<int>(std::get<double>(v->data));
    return 0;
}

static float toFloat(ValuePtr v) {
    if (!v) return 0.0f;
    if (v->type == Value::Type::INT) return static_cast<float>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::FLOAT) return static_cast<float>(std::get<double>(v->data));
    return 0.0f;
}

static std::string toString(ValuePtr v) {
    if (!v) return "";
    return v->toString();
}

static Color makeColor(int r, int g, int b, int a = 255) {
    return Color{ (unsigned char)(r & 255), (unsigned char)(g & 255), (unsigned char)(b & 255), (unsigned char)(a & 255) };
}

static bool g_camera_initialized = false;
static Camera3D g_camera{};
static Color g_draw_color = WHITE;
static Color g_clear_color = BLACK;
static int g_camera_mode = 0; // 0=manual, 1=free, 2=orbital, 3=first-person
static bool g_show_stats = false;
static bool g_debug_draw = false;
static bool g_debug_draw_axes = true;
static bool g_debug_draw_grid = true;
static bool g_debug_draw_bounds = false;
static float g_orbit_yaw_deg = 45.0f;
static float g_orbit_pitch_deg = 30.0f;
static float g_orbit_radius = 6.0f;
static bool g_orbit_state_initialized = false;
// after g2d: G2D_BASE 286 + 73 g2d builtins -> indices 286..358; g3d starts at 359.
static const size_t G3D_BASE = 359;

struct G3DTexture {
    Texture2D tex{};
    std::string key;
};

struct G3DModel {
    Model model{};
    BoundingBox bounds{};
    std::string path;
};

struct G3DObject {
    std::string type;
    Vector3 position{0.0f, 0.5f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f}; // degrees
    Vector3 scale{1.0f, 1.0f, 1.0f};
    Color color{255, 255, 255, 255};
    float a = 1.0f; // size/radius/width
    float b = 1.0f; // depth/height
    bool visible = true;
    std::string material = "flat";
    int textureId = -1;
    std::string textureKey;
    int modelId = -1;
    std::string modelPath;
};

static std::vector<G3DObject> g_objects;
static std::vector<G3DTexture> g_textures;
static std::unordered_map<std::string, int> g_textureByKey;
static std::vector<G3DModel> g_models;
static std::unordered_map<std::string, int> g_modelByPath;

struct G3DGroup {
    std::string name;
    std::vector<int> objectIds;
    bool visible = true;
};
static std::vector<G3DGroup> g_groups;

struct G3DCameraPathNode {
    Vector3 position;
    Vector3 target;
};
static std::vector<G3DCameraPathNode> g_camera_path_nodes;
static bool g_camera_path_closed = false;

struct G3DLight {
    std::string type; // directional | point
    Vector3 v{0, 0, 0};
    Color color{255, 255, 255, 255};
    float intensity = 1.0f;
};
static std::vector<G3DLight> g_lights;
static Color g_ambient_light = makeColor(50, 50, 50);
static bool g_mouse_captured = false;
static constexpr int G3D_MAX_DIR_LIGHTS = 4;
static constexpr int G3D_MAX_POINT_LIGHTS = 4;
static Shader g_lit_shader{};
static bool g_lit_shader_loaded = false;
static int g_loc_model_color = -1;
static int g_loc_ambient_color = -1;
static int g_loc_dir_count = -1;
static int g_loc_point_count = -1;
static int g_loc_dir_dirs = -1;
static int g_loc_dir_colors = -1;
static int g_loc_dir_intensities = -1;
static int g_loc_point_pos = -1;
static int g_loc_point_colors = -1;
static int g_loc_point_intensities = -1;
static int g_loc_view_pos = -1;
static int g_loc_use_texture = -1;
static int g_loc_albedo_tex = -1;

static void ensureDefaultCamera() {
    if (g_camera_initialized) return;
    g_camera.position = { 4.0f, 4.0f, 4.0f };
    g_camera.target = { 0.0f, 0.0f, 0.0f };
    g_camera.up = { 0.0f, 1.0f, 0.0f };
    g_camera.fovy = 45.0f;
    g_camera.projection = CAMERA_PERSPECTIVE;
    g_camera_initialized = true;
}

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static bool validObjectId(int id) {
    return id >= 0 && id < (int)g_objects.size();
}

static bool objectIsActive(const G3DObject& o) {
    return !o.type.empty() && o.type != "__deleted";
}

struct SimpleAABB {
    Vector3 min;
    Vector3 max;
};

static bool validModelId(int id);

static SimpleAABB objectAABB(const G3DObject& o) {
    if (o.type == "sphere") {
        float rr = o.a * std::max(std::max(o.scale.x, o.scale.y), o.scale.z);
        return {{o.position.x - rr, o.position.y - rr, o.position.z - rr},
                {o.position.x + rr, o.position.y + rr, o.position.z + rr}};
    }
    if (o.type == "cube") {
        float hx = (o.a * o.scale.x) * 0.5f;
        float hy = (o.a * o.scale.y) * 0.5f;
        float hz = (o.a * o.scale.z) * 0.5f;
        return {{o.position.x - hx, o.position.y - hy, o.position.z - hz},
                {o.position.x + hx, o.position.y + hy, o.position.z + hz}};
    }
    if (o.type == "cylinder") {
        float rr = o.a * std::max(o.scale.x, o.scale.z);
        float hh = (o.b * o.scale.y) * 0.5f;
        return {{o.position.x - rr, o.position.y - hh, o.position.z - rr},
                {o.position.x + rr, o.position.y + hh, o.position.z + rr}};
    }
    if (o.type == "model" && validModelId(o.modelId)) {
        const auto& b = g_models[o.modelId].bounds;
        float minx = std::min(b.min.x * o.scale.x, b.max.x * o.scale.x);
        float maxx = std::max(b.min.x * o.scale.x, b.max.x * o.scale.x);
        float miny = std::min(b.min.y * o.scale.y, b.max.y * o.scale.y);
        float maxy = std::max(b.min.y * o.scale.y, b.max.y * o.scale.y);
        float minz = std::min(b.min.z * o.scale.z, b.max.z * o.scale.z);
        float maxz = std::max(b.min.z * o.scale.z, b.max.z * o.scale.z);
        return {{o.position.x + minx, o.position.y + miny, o.position.z + minz},
                {o.position.x + maxx, o.position.y + maxy, o.position.z + maxz}};
    }
    // plane / fallback
    float hx = (o.a * o.scale.x) * 0.5f;
    float hz = (o.b * o.scale.z) * 0.5f;
    return {{o.position.x - hx, o.position.y - 0.02f, o.position.z - hz},
            {o.position.x + hx, o.position.y + 0.02f, o.position.z + hz}};
}

static bool aabbOverlap(const SimpleAABB& a, const SimpleAABB& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

static bool sphereAabbOverlap(Vector3 c, float r, const SimpleAABB& b) {
    float qx = std::max(b.min.x, std::min(c.x, b.max.x));
    float qy = std::max(b.min.y, std::min(c.y, b.max.y));
    float qz = std::max(b.min.z, std::min(c.z, b.max.z));
    float dx = c.x - qx, dy = c.y - qy, dz = c.z - qz;
    return (dx * dx + dy * dy + dz * dz) <= (r * r);
}

static bool objectIntersectsInternal(const G3DObject& a, const G3DObject& b) {
    if (!objectIsActive(a) || !objectIsActive(b)) return false;
    if (a.type == "sphere" && b.type == "sphere") {
        float ar = a.a * std::max(std::max(a.scale.x, a.scale.y), a.scale.z);
        float br = b.a * std::max(std::max(b.scale.x, b.scale.y), b.scale.z);
        float dx = a.position.x - b.position.x;
        float dy = a.position.y - b.position.y;
        float dz = a.position.z - b.position.z;
        float rr = ar + br;
        return (dx * dx + dy * dy + dz * dz) <= (rr * rr);
    }
    if (a.type == "sphere") {
        float ar = a.a * std::max(std::max(a.scale.x, a.scale.y), a.scale.z);
        return sphereAabbOverlap(a.position, ar, objectAABB(b));
    }
    if (b.type == "sphere") {
        float br = b.a * std::max(std::max(b.scale.x, b.scale.y), b.scale.z);
        return sphereAabbOverlap(b.position, br, objectAABB(a));
    }
    return aabbOverlap(objectAABB(a), objectAABB(b));
}

static bool validTextureId(int id) {
    return id >= 0 && id < (int)g_textures.size() && g_textures[id].tex.id != 0;
}

static bool validModelId(int id) {
    return id >= 0 && id < (int)g_models.size() && g_models[id].model.meshCount > 0;
}

static bool validGroupId(int id) {
    return id >= 0 && id < (int)g_groups.size();
}

static bool isNumber(ValuePtr v) {
    return v && (v->type == Value::Type::INT || v->type == Value::Type::FLOAT);
}

static int keyFromName(std::string key) {
    key = toLower(key);
    if (key == "space") return KEY_SPACE;
    if (key == "enter" || key == "return") return KEY_ENTER;
    if (key == "escape" || key == "esc") return KEY_ESCAPE;
    if (key == "tab") return KEY_TAB;
    if (key == "backspace") return KEY_BACKSPACE;
    if (key == "left") return KEY_LEFT;
    if (key == "right") return KEY_RIGHT;
    if (key == "up") return KEY_UP;
    if (key == "down") return KEY_DOWN;
    if (key == "shift") return KEY_LEFT_SHIFT;
    if (key == "ctrl" || key == "control") return KEY_LEFT_CONTROL;
    if (key == "alt") return KEY_LEFT_ALT;
    if (key.size() == 1) {
        char c = key[0];
        if (c >= 'a' && c <= 'z') return KEY_A + (int)(c - 'a');
        if (c >= '0' && c <= '9') return KEY_ZERO + (int)(c - '0');
    }
    return 0;
}

static int mouseButtonFromName(std::string btn) {
    btn = toLower(btn);
    if (btn == "left") return MOUSE_BUTTON_LEFT;
    if (btn == "right") return MOUSE_BUTTON_RIGHT;
    if (btn == "middle") return MOUSE_BUTTON_MIDDLE;
    return MOUSE_BUTTON_LEFT;
}

static Value makeRayMap(const Ray& ray) {
    std::unordered_map<std::string, ValuePtr> m;
    m["ox"] = std::make_shared<Value>(Value::fromFloat(ray.position.x));
    m["oy"] = std::make_shared<Value>(Value::fromFloat(ray.position.y));
    m["oz"] = std::make_shared<Value>(Value::fromFloat(ray.position.z));
    m["dx"] = std::make_shared<Value>(Value::fromFloat(ray.direction.x));
    m["dy"] = std::make_shared<Value>(Value::fromFloat(ray.direction.y));
    m["dz"] = std::make_shared<Value>(Value::fromFloat(ray.direction.z));
    return Value::fromMap(std::move(m));
}

static bool tryReadRay(ValuePtr rayValue, Ray& out) {
    if (!rayValue || rayValue->type != Value::Type::MAP) return false;
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(rayValue->data);
    auto getNum = [&m](const char* k, float& dst) -> bool {
        auto it = m.find(k);
        if (it == m.end() || !isNumber(it->second)) return false;
        dst = toFloat(it->second);
        return true;
    };
    return getNum("ox", out.position.x) &&
           getNum("oy", out.position.y) &&
           getNum("oz", out.position.z) &&
           getNum("dx", out.direction.x) &&
           getNum("dy", out.direction.y) &&
           getNum("dz", out.direction.z);
}

static Value makeHitMap(bool hit, float distance, Vector3 p) {
    std::unordered_map<std::string, ValuePtr> m;
    m["hit"] = std::make_shared<Value>(Value::fromBool(hit));
    m["distance"] = std::make_shared<Value>(Value::fromFloat(distance));
    m["x"] = std::make_shared<Value>(Value::fromFloat(p.x));
    m["y"] = std::make_shared<Value>(Value::fromFloat(p.y));
    m["z"] = std::make_shared<Value>(Value::fromFloat(p.z));
    return Value::fromMap(std::move(m));
}

static Value makePickMap(bool hit, int objectId, float distance, Vector3 p) {
    std::unordered_map<std::string, ValuePtr> m;
    m["hit"] = std::make_shared<Value>(Value::fromBool(hit));
    m["id"] = std::make_shared<Value>(Value::fromInt(objectId));
    m["distance"] = std::make_shared<Value>(Value::fromFloat(distance));
    m["x"] = std::make_shared<Value>(Value::fromFloat(p.x));
    m["y"] = std::make_shared<Value>(Value::fromFloat(p.y));
    m["z"] = std::make_shared<Value>(Value::fromFloat(p.z));
    return Value::fromMap(std::move(m));
}

static Vector3 pickNormalFromAabb(const SimpleAABB& aabb, Vector3 p) {
    const float eps = 0.02f;
    if (std::fabs(p.x - aabb.min.x) <= eps) return {-1.0f, 0.0f, 0.0f};
    if (std::fabs(p.x - aabb.max.x) <= eps) return { 1.0f, 0.0f, 0.0f};
    if (std::fabs(p.y - aabb.min.y) <= eps) return { 0.0f,-1.0f, 0.0f};
    if (std::fabs(p.y - aabb.max.y) <= eps) return { 0.0f, 1.0f, 0.0f};
    if (std::fabs(p.z - aabb.min.z) <= eps) return { 0.0f, 0.0f,-1.0f};
    if (std::fabs(p.z - aabb.max.z) <= eps) return { 0.0f, 0.0f, 1.0f};
    return {0.0f, 1.0f, 0.0f};
}

static Value makePickInfoMap(bool hit, int objectId, float distance, Vector3 p, Vector3 n) {
    std::unordered_map<std::string, ValuePtr> m;
    m["hit"] = std::make_shared<Value>(Value::fromBool(hit));
    m["id"] = std::make_shared<Value>(Value::fromInt(objectId));
    m["distance"] = std::make_shared<Value>(Value::fromFloat(distance));
    m["x"] = std::make_shared<Value>(Value::fromFloat(p.x));
    m["y"] = std::make_shared<Value>(Value::fromFloat(p.y));
    m["z"] = std::make_shared<Value>(Value::fromFloat(p.z));
    m["nx"] = std::make_shared<Value>(Value::fromFloat(n.x));
    m["ny"] = std::make_shared<Value>(Value::fromFloat(n.y));
    m["nz"] = std::make_shared<Value>(Value::fromFloat(n.z));
    if (hit && validObjectId(objectId)) {
        const auto& o = g_objects[objectId];
        m["type"] = std::make_shared<Value>(Value::fromString(o.type));
        m["material"] = std::make_shared<Value>(Value::fromString(o.material));
    } else {
        m["type"] = std::make_shared<Value>(Value::fromString(""));
        m["material"] = std::make_shared<Value>(Value::fromString(""));
    }
    return Value::fromMap(std::move(m));
}

static Vector3 cameraForwardDir() {
    Vector3 f{
        g_camera.target.x - g_camera.position.x,
        g_camera.target.y - g_camera.position.y,
        g_camera.target.z - g_camera.position.z
    };
    float len = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    if (len <= 0.0001f) return {0.0f, 0.0f, 1.0f};
    return {f.x / len, f.y / len, f.z / len};
}

static bool objectLikelyVisibleByCamera(const G3DObject& o, float marginPx) {
    if (!graphicsWindowOpen()) return false;
    if (!objectIsActive(o) || !o.visible) return false;
    Vector3 toObj{
        o.position.x - g_camera.position.x,
        o.position.y - g_camera.position.y,
        o.position.z - g_camera.position.z
    };
    Vector3 fwd = cameraForwardDir();
    float dot = toObj.x * fwd.x + toObj.y * fwd.y + toObj.z * fwd.z;
    if (dot <= 0.0f) return false;
    Vector2 s = GetWorldToScreen(o.position, g_camera);
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    return (s.x >= -marginPx && s.x <= w + marginPx && s.y >= -marginPx && s.y <= h + marginPx);
}

static void drawObjectBoundsInternal(const G3DObject& o, Color c) {
    if (!objectIsActive(o) || !o.visible) return;
    SimpleAABB a = objectAABB(o);
    BoundingBox box{a.min, a.max};
    DrawBoundingBox(box, c);
}

static bool valueIsNil(ValuePtr v) {
    return !v || v->type == Value::Type::NIL;
}

static bool objectInGroup(int objectId, int groupId) {
    if (!validGroupId(groupId)) return false;
    const auto& ids = g_groups[groupId].objectIds;
    for (int id : ids) if (id == objectId) return true;
    return false;
}

static bool objectMatchesFilters(int objectId, const std::string& typeFilter, const std::string& materialFilter, int groupIdFilter) {
    if (!validObjectId(objectId)) return false;
    const auto& o = g_objects[objectId];
    if (!typeFilter.empty() && toLower(o.type) != toLower(typeFilter)) return false;
    if (!materialFilter.empty() && toLower(o.material) != toLower(materialFilter)) return false;
    if (groupIdFilter >= 0 && !objectInGroup(objectId, groupIdFilter)) return false;
    return true;
}

static bool pickBestObjectWithFilters(const Ray& ray, const std::string& typeFilter, const std::string& materialFilter, int groupIdFilter, int& bestId, float& bestDist, Vector3& bestPoint, Vector3& bestNormal) {
    bestId = -1;
    bestDist = 0.0f;
    bestPoint = {0.0f, 0.0f, 0.0f};
    bestNormal = {0.0f, 1.0f, 0.0f};
    for (int i = 0; i < (int)g_objects.size(); ++i) {
        const auto& o = g_objects[i];
        if (!objectIsActive(o) || !o.visible) continue;
        if (!objectMatchesFilters(i, typeFilter, materialFilter, groupIdFilter)) continue;
        const SimpleAABB aabb = objectAABB(o);
        BoundingBox box{aabb.min, aabb.max};
        RayCollision hit = GetRayCollisionBox(ray, box);
        if (!hit.hit) continue;
        if (bestId < 0 || hit.distance < bestDist) {
            bestId = i;
            bestDist = hit.distance;
            bestPoint = hit.point;
            bestNormal = pickNormalFromAabb(aabb, hit.point);
        }
    }
    return bestId >= 0;
}

static Vector3 vec3Lerp(Vector3 a, Vector3 b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

static Vector3 vec3CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return {
        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
        0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3)
    };
}

static bool sampleCameraPath(float t, Camera3D& outCam) {
    if (g_camera_path_nodes.size() < 2) return false;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    int n = (int)g_camera_path_nodes.size();
    int segCount = g_camera_path_closed ? n : (n - 1);
    if (segCount <= 0) return false;
    float scaled = t * (float)segCount;
    int seg = (int)std::floor(scaled);
    if (seg >= segCount) seg = segCount - 1;
    float u = scaled - (float)seg;
    auto wrap = [n](int i) {
        int r = i % n;
        return r < 0 ? r + n : r;
    };
    int i1 = seg;
    int i2 = g_camera_path_closed ? wrap(seg + 1) : std::min(seg + 1, n - 1);
    int i0 = g_camera_path_closed ? wrap(i1 - 1) : std::max(0, i1 - 1);
    int i3 = g_camera_path_closed ? wrap(i2 + 1) : std::min(n - 1, i2 + 1);
    const auto& p0 = g_camera_path_nodes[i0];
    const auto& p1 = g_camera_path_nodes[i1];
    const auto& p2 = g_camera_path_nodes[i2];
    const auto& p3 = g_camera_path_nodes[i3];

    outCam = g_camera;
    if (n >= 4) {
        outCam.position = vec3CatmullRom(p0.position, p1.position, p2.position, p3.position, u);
        outCam.target = vec3CatmullRom(p0.target, p1.target, p2.target, p3.target, u);
    } else {
        outCam.position = vec3Lerp(p1.position, p2.position, u);
        outCam.target = vec3Lerp(p1.target, p2.target, u);
    }
    return true;
}

static Value makeCameraMap(const Camera3D& cam) {
    std::unordered_map<std::string, ValuePtr> m;
    m["px"] = std::make_shared<Value>(Value::fromFloat(cam.position.x));
    m["py"] = std::make_shared<Value>(Value::fromFloat(cam.position.y));
    m["pz"] = std::make_shared<Value>(Value::fromFloat(cam.position.z));
    m["tx"] = std::make_shared<Value>(Value::fromFloat(cam.target.x));
    m["ty"] = std::make_shared<Value>(Value::fromFloat(cam.target.y));
    m["tz"] = std::make_shared<Value>(Value::fromFloat(cam.target.z));
    m["ux"] = std::make_shared<Value>(Value::fromFloat(cam.up.x));
    m["uy"] = std::make_shared<Value>(Value::fromFloat(cam.up.y));
    m["uz"] = std::make_shared<Value>(Value::fromFloat(cam.up.z));
    m["fov"] = std::make_shared<Value>(Value::fromFloat(cam.fovy));
    m["projection"] = std::make_shared<Value>(Value::fromInt((int64_t)cam.projection));
    return Value::fromMap(std::move(m));
}

static void unloadTextures() {
    for (auto& t : g_textures) {
        if (t.tex.id != 0) {
            UnloadTexture(t.tex);
            t.tex.id = 0;
        }
    }
    g_textures.clear();
    g_textureByKey.clear();
}

static void unloadModels() {
    for (auto& m : g_models) {
        if (m.model.meshCount > 0) {
            UnloadModel(m.model);
            m.model.meshCount = 0;
        }
    }
    g_models.clear();
    g_modelByPath.clear();
}

static int ensureTexture(const std::string& key) {
    auto it = g_textureByKey.find(key);
    if (it != g_textureByKey.end() && validTextureId(it->second)) return it->second;

    Texture2D tex{};
    if (key == "@checker") {
        Image img = GenImageChecked(256, 256, 32, 32, WHITE, LIGHTGRAY);
        tex = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        tex = LoadTexture(key.c_str());
    }
    if (tex.id == 0) return -1;

    int id = (int)g_textures.size();
    g_textures.push_back({tex, key});
    g_textureByKey[key] = id;
    return id;
}

static int ensureModel(const std::string& path) {
    auto it = g_modelByPath.find(path);
    if (it != g_modelByPath.end() && validModelId(it->second)) return it->second;
    Model model = LoadModel(path.c_str());
    if (model.meshCount <= 0) return -1;
    BoundingBox bounds = GetModelBoundingBox(model);
    int id = (int)g_models.size();
    g_models.push_back({model, bounds, path});
    g_modelByPath[path] = id;
    return id;
}

static Vector3 normalize3(Vector3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.00001f) return {0.0f, -1.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

static Vector3 colorToVec3(Color c) {
    return {(float)c.r / 255.0f, (float)c.g / 255.0f, (float)c.b / 255.0f};
}

static void unloadLightingShader() {
    if (g_lit_shader_loaded) {
        UnloadShader(g_lit_shader);
        g_lit_shader_loaded = false;
    }
    g_loc_model_color = -1;
    g_loc_ambient_color = -1;
    g_loc_dir_count = -1;
    g_loc_point_count = -1;
    g_loc_dir_dirs = -1;
    g_loc_dir_colors = -1;
    g_loc_dir_intensities = -1;
    g_loc_point_pos = -1;
    g_loc_point_colors = -1;
    g_loc_point_intensities = -1;
    g_loc_view_pos = -1;
    g_loc_use_texture = -1;
    g_loc_albedo_tex = -1;
}

static void ensureLightingShader() {
    if (g_lit_shader_loaded || !graphicsWindowOpen()) return;
    const char* vs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;
void main() {
    fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    fragTexCoord = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

    const char* fs = R"(
#version 330
in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec3 modelColor;
uniform vec3 ambientColor;
uniform vec3 viewPos;
uniform int dirCount;
uniform int pointCount;
uniform vec3 dirDirs[4];
uniform vec3 dirColors[4];
uniform float dirIntensities[4];
uniform vec3 pointPos[4];
uniform vec3 pointColors[4];
uniform float pointIntensities[4];
uniform int useTexture;
uniform sampler2D albedoTex;

void main() {
    vec3 n = normalize(fragNormal);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 baseColor = modelColor;
    if (useTexture == 1) {
        baseColor *= texture(albedoTex, fragTexCoord).rgb;
    }

    vec3 diffuseAccum = vec3(0.0);
    vec3 specularAccum = vec3(0.0);

    for (int i = 0; i < dirCount; i++) {
        vec3 ldir = normalize(-dirDirs[i]);
        float diff = max(dot(n, ldir), 0.0);
        diffuseAccum += diff * dirColors[i] * dirIntensities[i];

        vec3 reflectDir = reflect(-ldir, n);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
        specularAccum += spec * dirColors[i] * dirIntensities[i] * 0.30;
    }

    for (int i = 0; i < pointCount; i++) {
        vec3 toPoint = pointPos[i] - fragPos;
        float dist = length(toPoint);
        vec3 pdir = dist > 0.0001 ? normalize(toPoint) : vec3(0.0, 1.0, 0.0);
        float diff = max(dot(n, pdir), 0.0);
        float att = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        diffuseAccum += diff * pointColors[i] * pointIntensities[i] * att;

        vec3 reflectDir = reflect(-pdir, n);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
        specularAccum += spec * pointColors[i] * pointIntensities[i] * att * 0.20;
    }

    vec3 lit = ambientColor + diffuseAccum;
    vec3 col = baseColor * lit + specularAccum;
    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)";

    g_lit_shader = LoadShaderFromMemory(vs, fs);
    if (g_lit_shader.id == 0) {
        std::cerr << "g3d: lighting shader load failed, using flat material fallback." << std::endl;
        return;
    }
    g_lit_shader_loaded = true;
    g_loc_model_color = GetShaderLocation(g_lit_shader, "modelColor");
    g_loc_ambient_color = GetShaderLocation(g_lit_shader, "ambientColor");
    g_loc_dir_count = GetShaderLocation(g_lit_shader, "dirCount");
    g_loc_point_count = GetShaderLocation(g_lit_shader, "pointCount");
    g_loc_dir_dirs = GetShaderLocation(g_lit_shader, "dirDirs");
    g_loc_dir_colors = GetShaderLocation(g_lit_shader, "dirColors");
    g_loc_dir_intensities = GetShaderLocation(g_lit_shader, "dirIntensities");
    g_loc_point_pos = GetShaderLocation(g_lit_shader, "pointPos");
    g_loc_point_colors = GetShaderLocation(g_lit_shader, "pointColors");
    g_loc_point_intensities = GetShaderLocation(g_lit_shader, "pointIntensities");
    g_loc_view_pos = GetShaderLocation(g_lit_shader, "viewPos");
    g_loc_use_texture = GetShaderLocation(g_lit_shader, "useTexture");
    g_loc_albedo_tex = GetShaderLocation(g_lit_shader, "albedoTex");
}

static void applyLightingUniforms(const G3DObject& o) {
    if (!g_lit_shader_loaded) return;
    Vector3 model = colorToVec3(o.color);
    Vector3 ambient = colorToVec3(g_ambient_light);
    float dirDirs[3 * G3D_MAX_DIR_LIGHTS] = {
        0.0f, -1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    float dirColors[3 * G3D_MAX_DIR_LIGHTS] = {
        1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    float dirIntensities[G3D_MAX_DIR_LIGHTS] = {1.0f, 0.0f, 0.0f, 0.0f};

    float pointPos[3 * G3D_MAX_POINT_LIGHTS] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    float pointColors[3 * G3D_MAX_POINT_LIGHTS] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    float pointIntensities[G3D_MAX_POINT_LIGHTS] = {0.0f, 0.0f, 0.0f, 0.0f};

    int dirCount = 0;
    int pointCount = 0;
    for (const auto& l : g_lights) {
        if (l.type == "directional" && dirCount < G3D_MAX_DIR_LIGHTS) {
            Vector3 d = normalize3(l.v);
            Vector3 c = colorToVec3(l.color);
            dirDirs[dirCount * 3 + 0] = d.x;
            dirDirs[dirCount * 3 + 1] = d.y;
            dirDirs[dirCount * 3 + 2] = d.z;
            dirColors[dirCount * 3 + 0] = c.x;
            dirColors[dirCount * 3 + 1] = c.y;
            dirColors[dirCount * 3 + 2] = c.z;
            dirIntensities[dirCount] = l.intensity;
            dirCount++;
        } else if (l.type == "point" && pointCount < G3D_MAX_POINT_LIGHTS) {
            Vector3 c = colorToVec3(l.color);
            pointPos[pointCount * 3 + 0] = l.v.x;
            pointPos[pointCount * 3 + 1] = l.v.y;
            pointPos[pointCount * 3 + 2] = l.v.z;
            pointColors[pointCount * 3 + 0] = c.x;
            pointColors[pointCount * 3 + 1] = c.y;
            pointColors[pointCount * 3 + 2] = c.z;
            pointIntensities[pointCount] = l.intensity;
            pointCount++;
        }
    }

    if (dirCount == 0) dirCount = 1; // keep a minimal default directional light
    Vector3 viewPos = g_camera.position;
    int useTexture = (o.textureId >= 0 && validTextureId(o.textureId)) ? 1 : 0;

    if (g_loc_model_color >= 0) SetShaderValue(g_lit_shader, g_loc_model_color, &model.x, SHADER_UNIFORM_VEC3);
    if (g_loc_ambient_color >= 0) SetShaderValue(g_lit_shader, g_loc_ambient_color, &ambient.x, SHADER_UNIFORM_VEC3);
    if (g_loc_dir_count >= 0) SetShaderValue(g_lit_shader, g_loc_dir_count, &dirCount, SHADER_UNIFORM_INT);
    if (g_loc_point_count >= 0) SetShaderValue(g_lit_shader, g_loc_point_count, &pointCount, SHADER_UNIFORM_INT);
    if (g_loc_dir_dirs >= 0) SetShaderValueV(g_lit_shader, g_loc_dir_dirs, dirDirs, SHADER_UNIFORM_VEC3, G3D_MAX_DIR_LIGHTS);
    if (g_loc_dir_colors >= 0) SetShaderValueV(g_lit_shader, g_loc_dir_colors, dirColors, SHADER_UNIFORM_VEC3, G3D_MAX_DIR_LIGHTS);
    if (g_loc_dir_intensities >= 0) SetShaderValueV(g_lit_shader, g_loc_dir_intensities, dirIntensities, SHADER_UNIFORM_FLOAT, G3D_MAX_DIR_LIGHTS);
    if (g_loc_point_pos >= 0) SetShaderValueV(g_lit_shader, g_loc_point_pos, pointPos, SHADER_UNIFORM_VEC3, G3D_MAX_POINT_LIGHTS);
    if (g_loc_point_colors >= 0) SetShaderValueV(g_lit_shader, g_loc_point_colors, pointColors, SHADER_UNIFORM_VEC3, G3D_MAX_POINT_LIGHTS);
    if (g_loc_point_intensities >= 0) SetShaderValueV(g_lit_shader, g_loc_point_intensities, pointIntensities, SHADER_UNIFORM_FLOAT, G3D_MAX_POINT_LIGHTS);
    if (g_loc_view_pos >= 0) SetShaderValue(g_lit_shader, g_loc_view_pos, &viewPos.x, SHADER_UNIFORM_VEC3);
    if (g_loc_use_texture >= 0) SetShaderValue(g_lit_shader, g_loc_use_texture, &useTexture, SHADER_UNIFORM_INT);
    if (useTexture == 1 && g_loc_albedo_tex >= 0) {
        SetShaderValueTexture(g_lit_shader, g_loc_albedo_tex, g_textures[o.textureId].tex);
    }
}

static void applyObjectTransform(const G3DObject& o) {
    rlPushMatrix();
    rlTranslatef(o.position.x, o.position.y, o.position.z);
    rlRotatef(o.rotation.x, 1.0f, 0.0f, 0.0f);
    rlRotatef(o.rotation.y, 0.0f, 1.0f, 0.0f);
    rlRotatef(o.rotation.z, 0.0f, 0.0f, 1.0f);
    rlScalef(o.scale.x, o.scale.y, o.scale.z);
}

static void drawObjectInternal(const G3DObject& o) {
    if (!o.visible) return;
    bool useLit = (o.material == "lambert" || o.material == "phong");
    if (useLit) {
        ensureLightingShader();
        if (g_lit_shader_loaded) {
            applyLightingUniforms(o);
            BeginShaderMode(g_lit_shader);
        }
    }
    applyObjectTransform(o);
    if (o.type == "cube") {
        // texture assignment is tracked; current fallback renders color-tinted cube.
        // (raylib helper availability differs by build, keep this path stable.)
        DrawCube({0.0f, 0.0f, 0.0f}, o.a, o.a, o.a, o.color);
        if (!useLit || !g_lit_shader_loaded) {
            DrawCubeWires({0.0f, 0.0f, 0.0f}, o.a, o.a, o.a, RAYWHITE);
        }
    } else if (o.type == "sphere") {
        DrawSphere({0.0f, 0.0f, 0.0f}, o.a, o.color);
    } else if (o.type == "plane") {
        DrawPlane({0.0f, 0.0f, 0.0f}, {o.a, o.b}, o.color);
    } else if (o.type == "cylinder") {
        DrawCylinder({0.0f, 0.0f, 0.0f}, o.a, o.a, o.b, 24, o.color);
    } else if (o.type == "model") {
        if (validModelId(o.modelId)) {
            Model& m = g_models[o.modelId].model;
            if (validTextureId(o.textureId) && m.materialCount > 0 && m.materials) {
                m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_textures[o.textureId].tex;
            }
            DrawModel(m, {0.0f, 0.0f, 0.0f}, 1.0f, o.color);
        }
    }
    rlPopMatrix();
    if (useLit && g_lit_shader_loaded) EndShaderMode();
}

static void register3dGraphicsToVM(VM& vm, std::function<void(const std::string&, ValuePtr)> onFn) {
    size_t i = G3D_BASE;
    auto add = [&vm, &i, &onFn](const std::string& name, VM::BuiltinFn fn) {
        vm.registerBuiltin(i, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = i++;
        onFn(name, std::make_shared<Value>(Value::fromFunction(f)));
    };

    add("createWindow", [](VM*, std::vector<ValuePtr> args) {
        int w = args.size() >= 1 ? toInt(args[0]) : 800;
        int h = args.size() >= 2 ? toInt(args[1]) : 600;
        std::string title = args.size() >= 3 ? toString(args[2]) : "Kern 3D";
        graphicsInitWindow(w, h, title);
        ensureDefaultCamera();
        ensureLightingShader();
        SetTargetFPS(60);
        return Value::nil();
    });

    add("closeWindow", [](VM*, std::vector<ValuePtr>) {
        unloadTextures();
        unloadModels();
        unloadLightingShader();
        g_debug_draw = false;
        g_debug_draw_axes = true;
        g_debug_draw_grid = true;
        g_debug_draw_bounds = false;
        g_orbit_state_initialized = false;
        g_camera_path_nodes.clear();
        g_camera_path_closed = false;
        graphicsCloseWindow();
        return Value::nil();
    });

    add("clear", [](VM*, std::vector<ValuePtr> args) {
        // only updates the clear color; actual clear happens in run()
        int r = args.size() >= 1 ? toInt(args[0]) : 0;
        int g = args.size() >= 2 ? toInt(args[1]) : 0;
        int b = args.size() >= 3 ? toInt(args[2]) : 0;
        g_clear_color = makeColor(r, g, b);
        return Value::nil();
    });

    add("setCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::nil();
        ensureDefaultCamera();
        g_camera.position = { toFloat(args[0]), toFloat(args[1]), toFloat(args[2]) };
        g_camera.target = { toFloat(args[3]), toFloat(args[4]), toFloat(args[5]) };
        return Value::nil();
    });

    add("setProjection", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        ensureDefaultCamera();
        std::string mode = toString(args[0]);
        for (char& c : mode) c = (char)tolower((unsigned char)c);
        if (mode == "orthographic" || mode == "ortho") {
            g_camera.projection = CAMERA_ORTHOGRAPHIC;
        } else {
            g_camera.projection = CAMERA_PERSPECTIVE;
        }
        return Value::nil();
    });

    add("setFov", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        ensureDefaultCamera();
        float fovy = toFloat(args[0]);
        if (fovy < 1.0f) fovy = 1.0f;
        if (fovy > 179.0f) fovy = 179.0f;
        g_camera.fovy = fovy;
        return Value::nil();
    });

    add("setCameraMode", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string mode = toString(args[0]);
        for (char& c : mode) c = (char)tolower((unsigned char)c);
        if (mode == "free") g_camera_mode = 1;
        else if (mode == "orbit" || mode == "orbital") g_camera_mode = 2;
        else if (mode == "firstperson" || mode == "first-person") g_camera_mode = 3;
        else g_camera_mode = 0;
        return Value::nil();
    });

    add("setColor", [](VM*, std::vector<ValuePtr> args) {
        int r = args.size() >= 1 ? toInt(args[0]) : 255;
        int g = args.size() >= 2 ? toInt(args[1]) : 255;
        int b = args.size() >= 3 ? toInt(args[2]) : 255;
        g_draw_color = makeColor(r, g, b);
        return Value::nil();
    });

    add("setDebug", [](VM*, std::vector<ValuePtr> args) {
        g_debug_draw = !args.empty() && args[0] && args[0]->isTruthy();
        if (g_debug_draw) {
            g_debug_draw_axes = true;
            g_debug_draw_grid = true;
        } else {
            g_debug_draw_axes = false;
            g_debug_draw_grid = false;
            g_debug_draw_bounds = false;
        }
        return Value::nil();
    });

    add("createObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(-1);
        std::string type = toLower(toString(args[0]));
        if (type != "cube" && type != "sphere" && type != "plane" && type != "cylinder" && type != "model") {
            std::cerr << "g3d.createObject: unsupported type '" << type << "'" << std::endl;
            return Value::fromInt(-1);
        }
        G3DObject o;
        o.type = type;
        o.color = g_draw_color;
        if (type == "cube") {
            o.a = args.size() >= 2 ? toFloat(args[1]) : 1.0f;
        } else if (type == "sphere") {
            o.a = args.size() >= 2 ? toFloat(args[1]) : 1.0f;
        } else if (type == "plane") {
            o.position = {0.0f, 0.0f, 0.0f};
            o.a = args.size() >= 2 ? toFloat(args[1]) : 2.0f;
            o.b = args.size() >= 3 ? toFloat(args[2]) : 2.0f;
        } else if (type == "cylinder") {
            o.position = {0.0f, 0.0f, 0.0f};
            o.a = args.size() >= 2 ? toFloat(args[1]) : 0.5f;
            o.b = args.size() >= 3 ? toFloat(args[2]) : 1.5f;
        } else if (type == "model") {
            if (!graphicsWindowOpen()) {
                std::cerr << "g3d.createObject(model): call after createWindow (graphics context required)." << std::endl;
                return Value::fromInt(-1);
            }
            std::string modelPath = args.size() >= 2 ? toString(args[1]) : "";
            if (modelPath.empty()) {
                std::cerr << "g3d.createObject(model): expected model file path." << std::endl;
                return Value::fromInt(-1);
            }
            int mid = ensureModel(modelPath);
            if (mid < 0) {
                std::cerr << "g3d.createObject(model): failed to load '" << modelPath << "'" << std::endl;
                return Value::fromInt(-1);
            }
            o.modelId = mid;
            o.modelPath = modelPath;
        }
        g_objects.push_back(o);
        return Value::fromInt((int64_t)g_objects.size() - 1);
    });

    add("loadModel", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(-1);
        if (!graphicsWindowOpen()) {
            std::cerr << "g3d.loadModel: call after createWindow (graphics context required)." << std::endl;
            return Value::fromInt(-1);
        }
        std::string modelPath = toString(args[0]);
        if (modelPath.empty()) return Value::fromInt(-1);
        G3DObject o;
        o.type = "model";
        o.color = g_draw_color;
        int mid = ensureModel(modelPath);
        if (mid < 0) {
            std::cerr << "g3d.loadModel: failed to load '" << modelPath << "'" << std::endl;
            return Value::fromInt(-1);
        }
        o.modelId = mid;
        o.modelPath = modelPath;
        g_objects.push_back(o);
        return Value::fromInt((int64_t)g_objects.size() - 1);
    });

    add("clearScene", [](VM*, std::vector<ValuePtr>) {
        g_objects.clear();
        g_groups.clear();
        g_lights.clear();
        return Value::nil();
    });

    add("objectCount", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt((int64_t)g_objects.size());
    });
    add("activeObjectCount", [](VM*, std::vector<ValuePtr>) {
        int n = 0;
        for (const auto& o : g_objects) if (objectIsActive(o)) ++n;
        return Value::fromInt((int64_t)n);
    });

    add("setPosition", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setPosition: invalid object id " << id << std::endl;
            return Value::nil();
        }
        g_objects[id].position = {toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        return Value::nil();
    });

    add("setRotation", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setRotation: invalid object id " << id << std::endl;
            return Value::nil();
        }
        g_objects[id].rotation = {toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        return Value::nil();
    });

    add("setScale", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setScale: invalid object id " << id << std::endl;
            return Value::nil();
        }
        g_objects[id].scale = {toFloat(args[1]), toFloat(args[2]), toFloat(args[3])};
        return Value::nil();
    });

    add("setObjectColor", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setObjectColor: invalid object id " << id << std::endl;
            return Value::nil();
        }
        g_objects[id].color = makeColor(toInt(args[1]), toInt(args[2]), toInt(args[3]));
        return Value::nil();
    });

    add("setMaterial", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setMaterial: invalid object id " << id << std::endl;
            return Value::nil();
        }
        std::string mode = toLower(toString(args[1]));
        if (mode != "flat" && mode != "lambert" && mode != "phong") {
            std::cerr << "g3d.setMaterial: unsupported mode '" << mode << "', using flat" << std::endl;
            mode = "flat";
        }
        g_objects[id].material = mode;
        return Value::nil();
    });

    add("setTexture", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        if (!graphicsWindowOpen()) {
            std::cerr << "g3d.setTexture: call after createWindow (graphics context required)." << std::endl;
            return Value::nil();
        }
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.setTexture: invalid object id " << id << std::endl;
            return Value::nil();
        }
        std::string key = toString(args[1]);
        int texId = ensureTexture(key);
        if (texId < 0) {
            std::cerr << "g3d.setTexture: failed to load '" << key << "'" << std::endl;
            return Value::nil();
        }
        g_objects[id].textureId = texId;
        g_objects[id].textureKey = key;
        if (g_objects[id].type == "model" && validModelId(g_objects[id].modelId)) {
            Model& m = g_models[g_objects[id].modelId].model;
            if (m.materialCount > 0 && m.materials) {
                m.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = g_textures[texId].tex;
            }
        }
        return Value::nil();
    });

    add("saveScene", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) {
            std::cerr << "g3d.saveScene: expected output path." << std::endl;
            return Value::fromBool(false);
        }
        std::string path = toString(args[0]);
        std::ofstream out(path, std::ios::out | std::ios::trunc);
        if (!out) {
            std::cerr << "g3d.saveScene: could not write '" << path << "'" << std::endl;
            return Value::fromBool(false);
        }
        out << "g3d_scene_v2\n";
        for (const auto& o : g_objects) {
            out << o.type << "|"
                << o.position.x << "|" << o.position.y << "|" << o.position.z << "|"
                << o.rotation.x << "|" << o.rotation.y << "|" << o.rotation.z << "|"
                << o.scale.x << "|" << o.scale.y << "|" << o.scale.z << "|"
                << (int)o.color.r << "|" << (int)o.color.g << "|" << (int)o.color.b << "|" << (int)o.color.a << "|"
                << o.a << "|" << o.b << "|"
                << (o.visible ? 1 : 0) << "|"
                << o.material << "|"
                << (o.textureKey.empty() ? "-" : o.textureKey) << "|"
                << (o.modelPath.empty() ? "-" : o.modelPath)
                << "\n";
        }
        return Value::fromBool(true);
    });

    add("loadScene", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) {
            std::cerr << "g3d.loadScene: expected input path." << std::endl;
            return Value::fromBool(false);
        }
        std::string path = toString(args[0]);
        std::ifstream in(path, std::ios::in);
        if (!in) {
            std::cerr << "g3d.loadScene: could not open '" << path << "'" << std::endl;
            return Value::fromBool(false);
        }
        std::string header;
        if (!std::getline(in, header) || (header != "g3d_scene_v1" && header != "g3d_scene_v2")) {
            std::cerr << "g3d.loadScene: invalid scene file format." << std::endl;
            return Value::fromBool(false);
        }

        std::vector<G3DObject> loaded;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::vector<std::string> parts;
            std::stringstream ss(line);
            std::string part;
            while (std::getline(ss, part, '|')) parts.push_back(part);
            if (parts.size() < 19) continue;

            try {
                G3DObject o;
                o.type = parts[0];
                o.position = {(float)std::stof(parts[1]), (float)std::stof(parts[2]), (float)std::stof(parts[3])};
                o.rotation = {(float)std::stof(parts[4]), (float)std::stof(parts[5]), (float)std::stof(parts[6])};
                o.scale = {(float)std::stof(parts[7]), (float)std::stof(parts[8]), (float)std::stof(parts[9])};
                o.color = makeColor(std::stoi(parts[10]), std::stoi(parts[11]), std::stoi(parts[12]), std::stoi(parts[13]));
                o.a = (float)std::stof(parts[14]);
                o.b = (float)std::stof(parts[15]);
                o.visible = (parts[16] == "1");
                o.material = parts[17];
                o.textureKey = parts[18] == "-" ? "" : parts[18];
                o.modelPath = (parts.size() >= 20 && parts[19] != "-") ? parts[19] : "";
                o.textureId = -1;
                o.modelId = -1;
                if (!o.modelPath.empty() && graphicsWindowOpen()) {
                    int mid = ensureModel(o.modelPath);
                    if (mid >= 0) o.modelId = mid;
                    else o.type = "__deleted";
                }
                if (!o.textureKey.empty() && graphicsWindowOpen()) {
                    int tid = ensureTexture(o.textureKey);
                    if (tid >= 0) o.textureId = tid;
                }
                loaded.push_back(o);
            } catch (...) {
                // skip malformed object rows safely.
            }
        }
        g_objects = std::move(loaded);
        return Value::fromBool(true);
    });

    add("setVisible", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) return Value::nil();
        g_objects[id].visible = args[1] && args[1]->isTruthy();
        return Value::nil();
    });

    add("destroyObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        int id = toInt(args[0]);
        if (!validObjectId(id)) return Value::fromBool(false);
        g_objects[id].type = "__deleted";
        g_objects[id].visible = false;
        for (auto& g : g_groups) {
            std::vector<int> keep;
            keep.reserve(g.objectIds.size());
            for (int oid : g.objectIds) if (oid != id) keep.push_back(oid);
            g.objectIds.swap(keep);
        }
        return Value::fromBool(true);
    });
    add("objectIntersects", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(false);
        int a = toInt(args[0]), b = toInt(args[1]);
        if (!validObjectId(a) || !validObjectId(b)) return Value::fromBool(false);
        return Value::fromBool(objectIntersectsInternal(g_objects[a], g_objects[b]));
    });
    add("objectDistance", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0.0);
        int a = toInt(args[0]), b = toInt(args[1]);
        if (!validObjectId(a) || !validObjectId(b)) return Value::fromFloat(0.0);
        float dx = g_objects[a].position.x - g_objects[b].position.x;
        float dy = g_objects[a].position.y - g_objects[b].position.y;
        float dz = g_objects[a].position.z - g_objects[b].position.z;
        return Value::fromFloat(std::sqrt(dx * dx + dy * dy + dz * dz));
    });
    add("worldToScreen", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 3) return Value::nil();
        ensureDefaultCamera();
        Vector3 w{toFloat(args[0]), toFloat(args[1]), toFloat(args[2])};
        Vector2 s = GetWorldToScreen(w, g_camera);
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        auto& out = std::get<std::vector<ValuePtr>>(arr->data);
        out.push_back(std::make_shared<Value>(Value::fromFloat(s.x)));
        out.push_back(std::make_shared<Value>(Value::fromFloat(s.y)));
        return Value(*arr);
    });
    add("isObjectVisible", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        int id = toInt(args[0]);
        if (!validObjectId(id)) return Value::fromBool(false);
        ensureDefaultCamera();
        float marginPx = args.size() >= 2 ? toFloat(args[1]) : 24.0f;
        if (marginPx < 0.0f) marginPx = 0.0f;
        return Value::fromBool(objectLikelyVisibleByCamera(g_objects[id], marginPx));
    });
    add("pickObject", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::fromInt(-1);
        ensureDefaultCamera();
        Vector2 m{
            args.size() >= 1 ? toFloat(args[0]) : static_cast<float>(GetMouseX()),
            args.size() >= 2 ? toFloat(args[1]) : static_cast<float>(GetMouseY())
        };
        Ray ray = GetMouseRay(m, g_camera);
        int bestId = -1;
        float bestDist = 0.0f;
        Vector3 bestPoint{0.0f, 0.0f, 0.0f};
        Vector3 bestNormal{0.0f, 1.0f, 0.0f};
        pickBestObjectWithFilters(ray, "", "", -1, bestId, bestDist, bestPoint, bestNormal);
        return Value::fromInt(bestId);
    });
    add("pickObjectInfo", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return makePickInfoMap(false, -1, 0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        ensureDefaultCamera();
        Vector2 m{
            args.size() >= 1 ? toFloat(args[0]) : static_cast<float>(GetMouseX()),
            args.size() >= 2 ? toFloat(args[1]) : static_cast<float>(GetMouseY())
        };
        Ray ray = GetMouseRay(m, g_camera);
        int bestId = -1;
        float bestDist = 0.0f;
        Vector3 bestPoint{0.0f, 0.0f, 0.0f};
        Vector3 bestNormal{0.0f, 1.0f, 0.0f};
        pickBestObjectWithFilters(ray, "", "", -1, bestId, bestDist, bestPoint, bestNormal);
        return makePickInfoMap(bestId >= 0, bestId, bestDist, bestPoint, bestNormal);
    });
    add("pickObjectFiltered", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::fromInt(-1);
        ensureDefaultCamera();
        Vector2 m{
            args.size() >= 1 ? toFloat(args[0]) : static_cast<float>(GetMouseX()),
            args.size() >= 2 ? toFloat(args[1]) : static_cast<float>(GetMouseY())
        };
        std::string typeFilter = (args.size() >= 3 && !valueIsNil(args[2])) ? toString(args[2]) : "";
        std::string materialFilter = (args.size() >= 4 && !valueIsNil(args[3])) ? toString(args[3]) : "";
        int groupIdFilter = (args.size() >= 5 && !valueIsNil(args[4])) ? toInt(args[4]) : -1;
        Ray ray = GetMouseRay(m, g_camera);
        int bestId = -1;
        float bestDist = 0.0f;
        Vector3 bestPoint{0.0f, 0.0f, 0.0f};
        Vector3 bestNormal{0.0f, 1.0f, 0.0f};
        pickBestObjectWithFilters(ray, typeFilter, materialFilter, groupIdFilter, bestId, bestDist, bestPoint, bestNormal);
        return Value::fromInt(bestId);
    });
    add("pickObjectInfoFiltered", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return makePickInfoMap(false, -1, 0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        ensureDefaultCamera();
        Vector2 m{
            args.size() >= 1 ? toFloat(args[0]) : static_cast<float>(GetMouseX()),
            args.size() >= 2 ? toFloat(args[1]) : static_cast<float>(GetMouseY())
        };
        std::string typeFilter = (args.size() >= 3 && !valueIsNil(args[2])) ? toString(args[2]) : "";
        std::string materialFilter = (args.size() >= 4 && !valueIsNil(args[3])) ? toString(args[3]) : "";
        int groupIdFilter = (args.size() >= 5 && !valueIsNil(args[4])) ? toInt(args[4]) : -1;
        Ray ray = GetMouseRay(m, g_camera);
        int bestId = -1;
        float bestDist = 0.0f;
        Vector3 bestPoint{0.0f, 0.0f, 0.0f};
        Vector3 bestNormal{0.0f, 1.0f, 0.0f};
        pickBestObjectWithFilters(ray, typeFilter, materialFilter, groupIdFilter, bestId, bestDist, bestPoint, bestNormal);
        return makePickInfoMap(bestId >= 0, bestId, bestDist, bestPoint, bestNormal);
    });
    add("visibleObjects", [](VM*, std::vector<ValuePtr> args) {
        ensureDefaultCamera();
        float marginPx = args.size() >= 1 ? toFloat(args[0]) : 24.0f;
        if (marginPx < 0.0f) marginPx = 0.0f;
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        auto& out = std::get<std::vector<ValuePtr>>(arr->data);
        for (int i = 0; i < (int)g_objects.size(); ++i) {
            const auto& o = g_objects[i];
            if (!objectLikelyVisibleByCamera(o, marginPx)) continue;
            out.push_back(std::make_shared<Value>(Value::fromInt(i)));
        }
        return Value(*arr);
    });
    add("visibleObjectsFiltered", [](VM*, std::vector<ValuePtr> args) {
        ensureDefaultCamera();
        float marginPx = args.size() >= 1 ? toFloat(args[0]) : 24.0f;
        if (marginPx < 0.0f) marginPx = 0.0f;
        std::string typeFilter = (args.size() >= 2 && !valueIsNil(args[1])) ? toString(args[1]) : "";
        std::string materialFilter = (args.size() >= 3 && !valueIsNil(args[2])) ? toString(args[2]) : "";
        int groupIdFilter = (args.size() >= 4 && !valueIsNil(args[3])) ? toInt(args[3]) : -1;
        auto arr = std::make_shared<Value>(Value::fromArray({}));
        auto& out = std::get<std::vector<ValuePtr>>(arr->data);
        for (int i = 0; i < (int)g_objects.size(); ++i) {
            if (!objectMatchesFilters(i, typeFilter, materialFilter, groupIdFilter)) continue;
            const auto& o = g_objects[i];
            if (!objectLikelyVisibleByCamera(o, marginPx)) continue;
            out.push_back(std::make_shared<Value>(Value::fromInt(i)));
        }
        return Value(*arr);
    });

    add("getObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) return Value::nil();
        const auto& o = g_objects[id];
        std::unordered_map<std::string, ValuePtr> m;
        m["id"] = std::make_shared<Value>(Value::fromInt(id));
        m["active"] = std::make_shared<Value>(Value::fromBool(objectIsActive(o)));
        m["type"] = std::make_shared<Value>(Value::fromString(o.type));
        m["visible"] = std::make_shared<Value>(Value::fromBool(o.visible));
        m["material"] = std::make_shared<Value>(Value::fromString(o.material));
        m["textureKey"] = std::make_shared<Value>(Value::fromString(o.textureKey));
        m["px"] = std::make_shared<Value>(Value::fromFloat(o.position.x));
        m["py"] = std::make_shared<Value>(Value::fromFloat(o.position.y));
        m["pz"] = std::make_shared<Value>(Value::fromFloat(o.position.z));
        m["rx"] = std::make_shared<Value>(Value::fromFloat(o.rotation.x));
        m["ry"] = std::make_shared<Value>(Value::fromFloat(o.rotation.y));
        m["rz"] = std::make_shared<Value>(Value::fromFloat(o.rotation.z));
        m["sx"] = std::make_shared<Value>(Value::fromFloat(o.scale.x));
        m["sy"] = std::make_shared<Value>(Value::fromFloat(o.scale.y));
        m["sz"] = std::make_shared<Value>(Value::fromFloat(o.scale.z));
        m["r"] = std::make_shared<Value>(Value::fromInt((int64_t)o.color.r));
        m["g"] = std::make_shared<Value>(Value::fromInt((int64_t)o.color.g));
        m["b"] = std::make_shared<Value>(Value::fromInt((int64_t)o.color.b));
        m["a"] = std::make_shared<Value>(Value::fromInt((int64_t)o.color.a));
        return Value::fromMap(std::move(m));
    });

    add("drawObject", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.empty()) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) {
            std::cerr << "g3d.drawObject: invalid object id " << id << std::endl;
            return Value::nil();
        }
        drawObjectInternal(g_objects[id]);
        return Value::nil();
    });

    add("drawAll", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) return Value::nil();
        std::set<int> hidden;
        for (const auto& g : g_groups) {
            if (!g.visible) {
                for (int id : g.objectIds) hidden.insert(id);
            }
        }
        for (int i = 0; i < (int)g_objects.size(); ++i) {
            if (hidden.find(i) != hidden.end()) continue;
            if (!objectIsActive(g_objects[i])) continue;
            drawObjectInternal(g_objects[i]);
        }
        return Value::nil();
    });

    add("createGroup", [](VM*, std::vector<ValuePtr> args) {
        G3DGroup g;
        g.name = args.empty() ? "group" : toString(args[0]);
        g.visible = true;
        g_groups.push_back(g);
        return Value::fromInt((int64_t)g_groups.size() - 1);
    });

    add("addToGroup", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int gid = toInt(args[0]);
        int oid = toInt(args[1]);
        if (!validGroupId(gid)) {
            std::cerr << "g3d.addToGroup: invalid group id " << gid << std::endl;
            return Value::nil();
        }
        if (!validObjectId(oid)) {
            std::cerr << "g3d.addToGroup: invalid object id " << oid << std::endl;
            return Value::nil();
        }
        auto& ids = g_groups[gid].objectIds;
        for (int x : ids) if (x == oid) return Value::nil();
        ids.push_back(oid);
        return Value::nil();
    });

    add("setGroupVisible", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int gid = toInt(args[0]);
        if (!validGroupId(gid)) {
            std::cerr << "g3d.setGroupVisible: invalid group id " << gid << std::endl;
            return Value::nil();
        }
        g_groups[gid].visible = args[1] && args[1]->isTruthy();
        return Value::nil();
    });

    add("addDirectionalLight", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::fromInt(-1);
        G3DLight l;
        l.type = "directional";
        l.v = {toFloat(args[0]), toFloat(args[1]), toFloat(args[2])};
        l.color = makeColor(toInt(args[3]), toInt(args[4]), toInt(args[5]));
        l.intensity = args.size() >= 7 ? toFloat(args[6]) : 1.0f;
        g_lights.push_back(l);
        return Value::fromInt((int64_t)g_lights.size() - 1);
    });

    add("addPointLight", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::fromInt(-1);
        G3DLight l;
        l.type = "point";
        l.v = {toFloat(args[0]), toFloat(args[1]), toFloat(args[2])};
        l.color = makeColor(toInt(args[3]), toInt(args[4]), toInt(args[5]));
        l.intensity = args.size() >= 7 ? toFloat(args[6]) : 1.0f;
        g_lights.push_back(l);
        return Value::fromInt((int64_t)g_lights.size() - 1);
    });

    add("setAmbientLight", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        g_ambient_light = makeColor(toInt(args[0]), toInt(args[1]), toInt(args[2]));
        return Value::nil();
    });

    add("isKeyDown", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.empty()) return Value::fromBool(false);
        int key = 0;
        if (isNumber(args[0])) key = toInt(args[0]);
        else key = keyFromName(toString(args[0]));
        if (key == 0) return Value::fromBool(false);
        return Value::fromBool(IsKeyDown(key));
    });

    add("isKeyPressed", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.empty()) return Value::fromBool(false);
        int key = 0;
        if (isNumber(args[0])) key = toInt(args[0]);
        else key = keyFromName(toString(args[0]));
        if (key == 0) return Value::fromBool(false);
        return Value::fromBool(IsKeyPressed(key));
    });

    add("mouseX", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) return Value::fromInt(0);
        return Value::fromInt(GetMouseX());
    });

    add("mouseY", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) return Value::fromInt(0);
        return Value::fromInt(GetMouseY());
    });

    add("isMouseDown", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::fromBool(false);
        int btn = MOUSE_BUTTON_LEFT;
        if (!args.empty()) {
            if (isNumber(args[0])) btn = toInt(args[0]);
            else btn = mouseButtonFromName(toString(args[0]));
        }
        return Value::fromBool(IsMouseButtonDown(btn));
    });

    add("rayFromMouse", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) {
            Ray r{};
            return makeRayMap(r);
        }
        ensureDefaultCamera();
        Ray r = GetMouseRay(GetMousePosition(), g_camera);
        return makeRayMap(r);
    });

    add("intersectObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return makeHitMap(false, 0.0f, {0.0f, 0.0f, 0.0f});
        int id = toInt(args[0]);
        if (!validObjectId(id)) return makeHitMap(false, 0.0f, {0.0f, 0.0f, 0.0f});
        Ray ray{};
        if (!tryReadRay(args[1], ray)) return makeHitMap(false, 0.0f, {0.0f, 0.0f, 0.0f});

        const G3DObject& o = g_objects[id];
        if (o.type == "sphere") {
            float rr = o.a * std::max(std::max(o.scale.x, o.scale.y), o.scale.z);
            RayCollision rc = GetRayCollisionSphere(ray, o.position, rr);
            return makeHitMap(rc.hit, rc.distance, rc.point);
        }
        if (o.type == "cube") {
            float hx = (o.a * o.scale.x) * 0.5f;
            float hy = (o.a * o.scale.y) * 0.5f;
            float hz = (o.a * o.scale.z) * 0.5f;
            BoundingBox box{
                {o.position.x - hx, o.position.y - hy, o.position.z - hz},
                {o.position.x + hx, o.position.y + hy, o.position.z + hz}
            };
            RayCollision rc = GetRayCollisionBox(ray, box);
            return makeHitMap(rc.hit, rc.distance, rc.point);
        }
        return makeHitMap(false, 0.0f, {0.0f, 0.0f, 0.0f});
    });

    add("captureMouse", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::nil();
        bool on = !args.empty() && args[0] && args[0]->isTruthy();
        g_mouse_captured = on;
        if (g_mouse_captured) DisableCursor();
        else EnableCursor();
        return Value::nil();
    });

    add("centerMouse", [](VM*, std::vector<ValuePtr>) {
        if (!graphicsWindowOpen()) return Value::nil();
        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
        return Value::nil();
    });

    add("drawCube", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        ensureDefaultCamera();
        float x = toFloat(args[0]), y = toFloat(args[1]), z = toFloat(args[2]);
        float size = toFloat(args[3]);
        DrawCube({x, y, z}, size, size, size, g_draw_color);
        DrawCubeWires({x, y, z}, size, size, size, RAYWHITE);
        return Value::nil();
    });

    add("drawSphere", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 4) return Value::nil();
        ensureDefaultCamera();
        float x = toFloat(args[0]), y = toFloat(args[1]), z = toFloat(args[2]);
        float radius = toFloat(args[3]);
        DrawSphere({x, y, z}, radius, g_draw_color);
        return Value::nil();
    });

    add("drawPlane", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5) return Value::nil();
        ensureDefaultCamera();
        float x = toFloat(args[0]), y = toFloat(args[1]), z = toFloat(args[2]);
        float w = toFloat(args[3]), d = toFloat(args[4]);
        DrawPlane({x, y, z}, {w, d}, g_draw_color);
        return Value::nil();
    });

    add("drawCylinder", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.size() < 5) return Value::nil();
        ensureDefaultCamera();
        float x = toFloat(args[0]), y = toFloat(args[1]), z = toFloat(args[2]);
        float radius = toFloat(args[3]), height = toFloat(args[4]);
        DrawCylinder({x, y, z}, radius, radius, height, 24, g_draw_color);
        return Value::nil();
    });

    add("drawGrid", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) return Value::nil();
        int slices = args.size() >= 1 ? toInt(args[0]) : 20;
        float spacing = args.size() >= 2 ? toFloat(args[1]) : 1.0f;
        if (slices < 1) slices = 1;
        if (spacing <= 0.0f) spacing = 1.0f;
        DrawGrid(slices, spacing);
        return Value::nil();
    });

    add("showStats", [](VM*, std::vector<ValuePtr> args) {
        g_show_stats = !args.empty() && args[0] && args[0]->isTruthy();
        return Value::nil();
    });

    add("width", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(graphicsWindowOpen() ? GetScreenWidth() : 0);
    });

    add("height", [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(graphicsWindowOpen() ? GetScreenHeight() : 0);
    });
    add("moveCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        ensureDefaultCamera();
        float dx = toFloat(args[0]), dy = toFloat(args[1]), dz = toFloat(args[2]);
        g_camera.position.x += dx; g_camera.position.y += dy; g_camera.position.z += dz;
        g_camera.target.x += dx; g_camera.target.y += dy; g_camera.target.z += dz;
        return Value::nil();
    });
    add("orbitCameraToObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id) || !objectIsActive(g_objects[id])) return Value::nil();
        ensureDefaultCamera();
        float yawDeg = toFloat(args[1]);
        float pitchDeg = toFloat(args[2]);
        float radius = toFloat(args[3]);
        if (radius < 0.05f) radius = 0.05f;
        const Vector3 c = g_objects[id].position;
        float yaw = yawDeg * (3.1415926535f / 180.0f);
        float pitch = pitchDeg * (3.1415926535f / 180.0f);
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cyaw = std::cos(yaw), syaw = std::sin(yaw);
        g_camera.target = c;
        g_camera.position = {c.x + cyaw * cp * radius, c.y + sp * radius, c.z + syaw * cp * radius};
        g_orbit_yaw_deg = yawDeg;
        g_orbit_pitch_deg = pitchDeg;
        g_orbit_radius = radius;
        g_orbit_state_initialized = true;
        return Value::nil();
    });
    add("focusCameraOnObject", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id) || !objectIsActive(g_objects[id])) return Value::nil();
        ensureDefaultCamera();
        const SimpleAABB a = objectAABB(g_objects[id]);
        Vector3 center{
            0.5f * (a.min.x + a.max.x),
            0.5f * (a.min.y + a.max.y),
            0.5f * (a.min.z + a.max.z)
        };
        float ex = a.max.x - a.min.x;
        float ey = a.max.y - a.min.y;
        float ez = a.max.z - a.min.z;
        float radius = 0.5f * std::sqrt(ex * ex + ey * ey + ez * ez);
        float padding = args.size() >= 2 ? toFloat(args[1]) : 1.3f;
        if (padding < 1.0f) padding = 1.0f;
        Vector3 fwd = cameraForwardDir();
        float dist = std::max(0.3f, radius * padding + 0.1f);
        g_camera.target = center;
        g_camera.position = {center.x - fwd.x * dist, center.y - fwd.y * dist, center.z - fwd.z * dist};
        g_orbit_state_initialized = false;
        return Value::nil();
    });
    add("lerpCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 7) return Value::nil();
        ensureDefaultCamera();
        float alpha = toFloat(args[6]);
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        Vector3 targetPos{toFloat(args[0]), toFloat(args[1]), toFloat(args[2])};
        Vector3 targetLook{toFloat(args[3]), toFloat(args[4]), toFloat(args[5])};
        g_camera.position.x += (targetPos.x - g_camera.position.x) * alpha;
        g_camera.position.y += (targetPos.y - g_camera.position.y) * alpha;
        g_camera.position.z += (targetPos.z - g_camera.position.z) * alpha;
        g_camera.target.x += (targetLook.x - g_camera.target.x) * alpha;
        g_camera.target.y += (targetLook.y - g_camera.target.y) * alpha;
        g_camera.target.z += (targetLook.z - g_camera.target.z) * alpha;
        return Value::nil();
    });
    add("orbitCamera", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 6) return Value::nil();
        ensureDefaultCamera();
        float yawDeg = toFloat(args[0]);
        float pitchDeg = toFloat(args[1]);
        float radius = toFloat(args[2]);
        if (radius < 0.05f) radius = 0.05f;
        float cx = toFloat(args[3]), cy = toFloat(args[4]), cz = toFloat(args[5]);
        float yaw = yawDeg * (3.1415926535f / 180.0f);
        float pitch = pitchDeg * (3.1415926535f / 180.0f);
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cyaw = std::cos(yaw), syaw = std::sin(yaw);
        g_camera.target = {cx, cy, cz};
        g_camera.position = {cx + cyaw * cp * radius, cy + sp * radius, cz + syaw * cp * radius};
        g_orbit_yaw_deg = yawDeg;
        g_orbit_pitch_deg = pitchDeg;
        g_orbit_radius = radius;
        g_orbit_state_initialized = true;
        return Value::nil();
    });
    add("orbitCameraStep", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        ensureDefaultCamera();
        if (!g_orbit_state_initialized) {
            Vector3 d{
                g_camera.position.x - g_camera.target.x,
                g_camera.position.y - g_camera.target.y,
                g_camera.position.z - g_camera.target.z
            };
            g_orbit_radius = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (g_orbit_radius < 0.05f) g_orbit_radius = 0.05f;
            g_orbit_yaw_deg = std::atan2(d.z, d.x) * (180.0f / 3.1415926535f);
            g_orbit_pitch_deg = std::asin(std::max(-1.0f, std::min(1.0f, d.y / g_orbit_radius))) * (180.0f / 3.1415926535f);
            g_orbit_state_initialized = true;
        }
        g_orbit_yaw_deg += toFloat(args[0]);
        g_orbit_pitch_deg += toFloat(args[1]);
        if (g_orbit_pitch_deg > 89.0f) g_orbit_pitch_deg = 89.0f;
        if (g_orbit_pitch_deg < -89.0f) g_orbit_pitch_deg = -89.0f;
        if (args.size() >= 3) {
            g_orbit_radius += toFloat(args[2]);
            if (g_orbit_radius < 0.05f) g_orbit_radius = 0.05f;
        }
        float yaw = g_orbit_yaw_deg * (3.1415926535f / 180.0f);
        float pitch = g_orbit_pitch_deg * (3.1415926535f / 180.0f);
        float cp = std::cos(pitch), sp = std::sin(pitch);
        float cyaw = std::cos(yaw), syaw = std::sin(yaw);
        Vector3 c = g_camera.target;
        g_camera.position = {c.x + cyaw * cp * g_orbit_radius, c.y + sp * g_orbit_radius, c.z + syaw * cp * g_orbit_radius};
        return Value::nil();
    });
    add("setCameraPath", [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(0);
        const auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<G3DCameraPathNode> nodes;
        nodes.reserve(arr.size() / 6);
        size_t i = 0;
        while (i + 5 < arr.size()) {
            if (!isNumber(arr[i]) || !isNumber(arr[i + 1]) || !isNumber(arr[i + 2]) ||
                !isNumber(arr[i + 3]) || !isNumber(arr[i + 4]) || !isNumber(arr[i + 5])) {
                i += 6;
                continue;
            }
            G3DCameraPathNode n;
            n.position = {toFloat(arr[i]), toFloat(arr[i + 1]), toFloat(arr[i + 2])};
            n.target = {toFloat(arr[i + 3]), toFloat(arr[i + 4]), toFloat(arr[i + 5])};
            nodes.push_back(n);
            i += 6;
        }
        if (nodes.size() < 2) {
            g_camera_path_nodes.clear();
            return Value::fromInt(0);
        }
        g_camera_path_nodes = std::move(nodes);
        g_camera_path_closed = (args.size() >= 2 && args[1] && args[1]->isTruthy());
        return Value::fromInt((int64_t)g_camera_path_nodes.size());
    });
    add("clearCameraPath", [](VM*, std::vector<ValuePtr>) {
        g_camera_path_nodes.clear();
        g_camera_path_closed = false;
        return Value::nil();
    });
    add("sampleCameraPath", [](VM*, std::vector<ValuePtr> args) {
        ensureDefaultCamera();
        if (g_camera_path_nodes.size() < 2) return Value::nil();
        float t = args.empty() ? 0.0f : toFloat(args[0]);
        Camera3D sample = g_camera;
        if (!sampleCameraPath(t, sample)) return Value::nil();
        return makeCameraMap(sample);
    });
    add("applyCameraPath", [](VM*, std::vector<ValuePtr> args) {
        ensureDefaultCamera();
        if (g_camera_path_nodes.size() < 2) return Value::fromBool(false);
        float t = args.empty() ? 0.0f : toFloat(args[0]);
        Camera3D sample = g_camera;
        if (!sampleCameraPath(t, sample)) return Value::fromBool(false);
        g_camera.position = sample.position;
        g_camera.target = sample.target;
        g_orbit_state_initialized = false;
        return Value::fromBool(true);
    });
    add("lookAt", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        ensureDefaultCamera();
        g_camera.target = {toFloat(args[0]), toFloat(args[1]), toFloat(args[2])};
        return Value::nil();
    });
    add("getCamera", [](VM*, std::vector<ValuePtr>) {
        ensureDefaultCamera();
        return makeCameraMap(g_camera);
    });
    add("setDebugOverlays", [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 1) g_debug_draw_axes = args[0] && args[0]->isTruthy();
        if (args.size() >= 2) g_debug_draw_grid = args[1] && args[1]->isTruthy();
        if (args.size() >= 3) g_debug_draw_bounds = args[2] && args[2]->isTruthy();
        g_debug_draw = g_debug_draw_axes || g_debug_draw_grid || g_debug_draw_bounds;
        return Value::nil();
    });
    add("drawObjectBounds", [](VM*, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen() || args.empty()) return Value::nil();
        int id = toInt(args[0]);
        if (!validObjectId(id)) return Value::nil();
        int r = args.size() >= 2 ? toInt(args[1]) : 255;
        int g = args.size() >= 3 ? toInt(args[2]) : 255;
        int b = args.size() >= 4 ? toInt(args[3]) : 0;
        int a = args.size() >= 5 ? toInt(args[4]) : 255;
        drawObjectBoundsInternal(g_objects[id], makeColor(r, g, b, a));
        return Value::nil();
    });

    add("status", [](VM*, std::vector<ValuePtr>) {
        std::unordered_map<std::string, ValuePtr> m;
        m["windowOpen"] = std::make_shared<Value>(Value::fromBool(graphicsWindowOpen()));
        m["width"] = std::make_shared<Value>(Value::fromInt(graphicsWindowOpen() ? GetScreenWidth() : 0));
        m["height"] = std::make_shared<Value>(Value::fromInt(graphicsWindowOpen() ? GetScreenHeight() : 0));
        m["cameraMode"] = std::make_shared<Value>(Value::fromInt(g_camera_mode));
        m["mouseCaptured"] = std::make_shared<Value>(Value::fromBool(g_mouse_captured));
        m["debugAxes"] = std::make_shared<Value>(Value::fromBool(g_debug_draw_axes));
        m["debugGrid"] = std::make_shared<Value>(Value::fromBool(g_debug_draw_grid));
        m["debugBounds"] = std::make_shared<Value>(Value::fromBool(g_debug_draw_bounds));
        m["cameraPathNodes"] = std::make_shared<Value>(Value::fromInt((int64_t)g_camera_path_nodes.size()));
        m["cameraPathClosed"] = std::make_shared<Value>(Value::fromBool(g_camera_path_closed));
        m["objectCount"] = std::make_shared<Value>(Value::fromInt((int64_t)g_objects.size()));
        m["groupCount"] = std::make_shared<Value>(Value::fromInt((int64_t)g_groups.size()));
        m["lightCount"] = std::make_shared<Value>(Value::fromInt((int64_t)g_lights.size()));
        int active = 0;
        for (const auto& o : g_objects) if (objectIsActive(o)) ++active;
        m["activeObjectCount"] = std::make_shared<Value>(Value::fromInt((int64_t)active));
        return Value::fromMap(std::move(m));
    });

    add("run", [](VM* vm, std::vector<ValuePtr> args) {
        if (!graphicsWindowOpen()) {
            std::cerr << "g3d.run: no active window (call g3d.createWindow first)." << std::endl;
            return Value::nil();
        }
        if (!vm || args.size() < 2) {
            std::cerr << "g3d.run: expected update and draw functions." << std::endl;
            return Value::nil();
        }
        ValuePtr updateFn = args[0], drawFn = args[1];
        bool updateByName = updateFn && updateFn->type == Value::Type::STRING;
        bool drawByName = drawFn && drawFn->type == Value::Type::STRING;
        std::string updateName = updateByName ? std::get<std::string>(updateFn->data) : "";
        std::string drawName = drawByName ? std::get<std::string>(drawFn->data) : "";
        if (!updateByName && (!updateFn || updateFn->type != Value::Type::FUNCTION)) {
            std::cerr << "g3d.run: update must be function or function name string." << std::endl;
            return Value::nil();
        }
        if (!drawByName && (!drawFn || drawFn->type != Value::Type::FUNCTION)) {
            std::cerr << "g3d.run: draw must be function or function name string." << std::endl;
            return Value::nil();
        }
        uint64_t savedGuard = vm->getCallbackStepGuard();
        // 3d update callbacks can be instruction-heavy; keep a sufficiently high
        // guard to avoid premature callback unwinds that leave scenes static.
        if (savedGuard < 5000000) vm->setCallbackStepGuard(5000000);
        ensureDefaultCamera();
        SetTargetFPS(60);
        // close after g3d.closeWindow(): graphicsWindowOpen() becomes false; avoid calling
        // windowShouldClose() after the window has been destroyed.
        while (graphicsWindowOpen() && !WindowShouldClose()) {
            // engine-level capture toggle so it works even if script callbacks are flaky.
            if (IsKeyPressed(KEY_TAB)) {
                g_mouse_captured = !g_mouse_captured;
                if (g_mouse_captured) {
                    DisableCursor();
                    SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
                } else {
                    EnableCursor();
                }
            }
            ValuePtr u = updateByName ? vm->getGlobal(updateName) : updateFn;
            ValuePtr d = drawByName ? vm->getGlobal(drawName) : drawFn;
            if (!u || u->type != Value::Type::FUNCTION || !d || d->type != Value::Type::FUNCTION) {
                std::cerr << "g3d.run: callback function missing/invalid." << std::endl;
                break;
            }
            if (g_camera_mode == 1) UpdateCamera(&g_camera, CAMERA_FREE);
            else if (g_camera_mode == 2) UpdateCamera(&g_camera, CAMERA_ORBITAL);
            else if (g_camera_mode == 3) UpdateCamera(&g_camera, CAMERA_FIRST_PERSON);

            try {
                vm->callValue(u, {});
            } catch (const std::exception& e) {
                std::cerr << "g3d.run: update callback error: " << e.what() << std::endl;
                break;
            } catch (...) {
                std::cerr << "g3d.run: update callback error: unknown exception." << std::endl;
                break;
            }
            // update may call closeWindow(); don't touch Raylib after the window is gone.
            if (!graphicsWindowOpen()) break;
            BeginDrawing();
            ClearBackground(g_clear_color);
            BeginMode3D(g_camera);
            if (g_debug_draw) {
                if (g_debug_draw_grid) DrawGrid(20, 1.0f);
                if (g_debug_draw_axes) {
                    DrawLine3D({0, 0, 0}, {2, 0, 0}, RED);   // +x
                    DrawLine3D({0, 0, 0}, {0, 2, 0}, GREEN); // +y
                    DrawLine3D({0, 0, 0}, {0, 0, 2}, BLUE);  // +z
                }
            }
            try {
                vm->callValue(d, {});
            } catch (const std::exception& e) {
                std::cerr << "g3d.run: draw callback error: " << e.what() << std::endl;
                // continue rendering fallback objects for this frame.
            } catch (...) {
                std::cerr << "g3d.run: draw callback error: unknown exception." << std::endl;
            }
            // fallback: always draw registered objects so scenes remain visible
            // even if callback invocation fails in certain runtime paths.
            std::set<int> hidden;
            for (const auto& g : g_groups) {
                if (!g.visible) for (int id : g.objectIds) hidden.insert(id);
            }
            for (int i = 0; i < (int)g_objects.size(); ++i) {
                if (hidden.find(i) != hidden.end()) continue;
                drawObjectInternal(g_objects[i]);
                if (g_debug_draw_bounds) {
                    drawObjectBoundsInternal(g_objects[i], makeColor(255, 220, 40));
                }
            }
            EndMode3D();
            if (g_show_stats) DrawFPS(10, 10);
            EndDrawing();
        }
        vm->setCallbackStepGuard(savedGuard);
        return Value::nil();
    });
}

static std::unordered_map<std::string, ValuePtr> s_g3dModuleMap;

ValuePtr create3dGraphicsModule(VM& vm) {
    // The module map can be cached, but builtin registration is per-VM. Always ensure the
    // current VM has the g3d builtins registered so cached FunctionObjects don't point at
    // indices that are missing in this VM instance.
    register3dGraphicsToVM(vm, [](const std::string& name, ValuePtr v) { s_g3dModuleMap[name] = v; });
    return std::make_shared<Value>(Value::fromMap(s_g3dModuleMap));
}

} // namespace kern

