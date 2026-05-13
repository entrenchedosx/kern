/* *
 * kern/modules/graphics/g3d_refactored.cpp - Refactored 3D Graphics Module
 * 
 * - No direct VM state access
 * - Uses clean ModuleInterface API
 * - No global state (context-based)
 * - Raylib backend isolated
 */
#include "module_api.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <vector>
#include <unordered_map>
#include <memory>

namespace kern {
namespace graphics {

// Per-context state (no globals)
struct G3DContext {
    struct Object {
        std::string type;
        Vector3 position{0.0f, 0.0f, 0.0f};
        Vector3 rotation{0.0f, 0.0f, 0.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Color color{255, 255, 255, 255};
        float a{1.0f};  // size/radius/width
        float b{1.0f};  // depth/height
        bool visible{true};
        std::string material{"flat"};
    };
    
    std::vector<Object> objects;
    std::unordered_map<int, int> freeList;  // Reuse IDs
    Camera3D camera{};
    Color clearColor{BLACK};
    Color drawColor{WHITE};
    bool cameraInitialized{false};
    bool windowOpen{false};
    int nextId{0};
    
    // Ensure camera is set up
    void initCamera() {
        if (cameraInitialized) return;
        camera.position = {4.0f, 4.0f, 4.0f};
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        cameraInitialized = true;
    }
    
    // Allocate object ID
    int allocateObject() {
        if (!freeList.empty()) {
            int id = freeList.begin()->first;
            freeList.erase(id);
            return id;
        }
        int id = nextId++;
        if (id >= (int)objects.size()) {
            objects.resize(id + 1);
        }
        return id;
    }
    
    // Free object ID
    void freeObject(int id) {
        if (id >= 0 && id < (int)objects.size()) {
            objects[id].visible = false;
            freeList[id] = 1;
        }
    }
    
    // Validate object ID
    bool isValid(int id) const {
        return id >= 0 && id < (int)objects.size() && objects[id].visible;
    }
};

// Thread-local context (no global state)
static thread_local std::unique_ptr<G3DContext> g_tlsContext;

static G3DContext& getContext() {
    if (!g_tlsContext) {
        g_tlsContext = std::make_unique<G3DContext>();
    }
    return *g_tlsContext;
}

// GraphicsContext implementation
class G3DGraphicsContext : public GraphicsContext {
    ModuleInterface* iface;
    
public:
    explicit G3DGraphicsContext(ModuleInterface* i) : iface(i) {}
    
    Result<void> createWindow(int width, int height, const std::string& title) override {
        if (getContext().windowOpen) {
            CloseWindow();
        }
        InitWindow(width, height, title.c_str());
        getContext().windowOpen = true;
        getContext().initCamera();
        
        // Set up shader
        rlglInit(0);
        
        return Result<void>(true);
    }
    
    void closeWindow() override {
        if (getContext().windowOpen) {
            CloseWindow();
            getContext().windowOpen = false;
        }
    }
    
    bool windowShouldClose() override {
        return WindowShouldClose();
    }
    
    void clear(uint8_t r, uint8_t g, uint8_t b) override {
        getContext().clearColor = {r, g, b, 255};
        ClearBackground(getContext().clearColor);
    }
    
    void drawLine(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) override {
        DrawLine(x1, y1, x2, y2, {r, g, b, 255});
    }
    
    void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool fill) override {
        Color c{r, g, b, 255};
        if (fill) {
            DrawRectangle(x, y, w, h, c);
        } else {
            DrawRectangleLines(x, y, w, h, c);
        }
    }
    
    void drawCircle(int x, int y, int radius, uint8_t r, uint8_t g, uint8_t b, bool fill) override {
        Color c{r, g, b, 255};
        if (fill) {
            DrawCircle(x, y, radius, c);
        } else {
            DrawCircleLines(x, y, radius, c);
        }
    }
    
    void drawText(int x, int y, const std::string& text, uint8_t r, uint8_t g, uint8_t b) override {
        DrawText(text.c_str(), x, y, 20, {r, g, b, 255});
    }
    
    void begin3D() override {
        BeginMode3D(getContext().camera);
    }
    
    void end3D() override {
        EndMode3D();
    }
    
    int createCube(float size) override {
        int id = getContext().allocateObject();
        auto& obj = getContext().objects[id];
        obj.type = "cube";
        obj.a = size;
        obj.visible = true;
        return id;
    }
    
    int createSphere(float radius) override {
        int id = getContext().allocateObject();
        auto& obj = getContext().objects[id];
        obj.type = "sphere";
        obj.a = radius;
        obj.visible = true;
        return id;
    }
    
    void setObjectPosition(int id, float x, float y, float z) override {
        if (!getContext().isValid(id)) return;
        getContext().objects[id].position = {x, y, z};
    }
    
    void setObjectColor(int id, uint8_t r, uint8_t g, uint8_t b) override {
        if (!getContext().isValid(id)) return;
        getContext().objects[id].color = {r, g, b, 255};
    }
    
    void drawObject(int id) override {
        if (!getContext().isValid(id)) return;
        const auto& obj = getContext().objects[id];
        if (!obj.visible) return;
        
        Color c = obj.color;
        
        if (obj.type == "cube") {
            DrawCube(obj.position, obj.a, obj.a, obj.a, c);
        } else if (obj.type == "sphere") {
            DrawSphere(obj.position, obj.a, c);
        }
    }
    
    void beginFrame() override {
        BeginDrawing();
        ClearBackground(getContext().clearColor);
    }
    
    void endFrame() override {
        EndDrawing();
    }
};

// Module initialization
void initG3D(ModuleInterface* iface, std::unordered_map<std::string, Value>& exports) {
    // Provide graphics context to interface
    auto* graphics = new G3DGraphicsContext(iface);
    
    // Window functions
    exports["createWindow"] = Value::fromNative([iface, graphics](const std::vector<Value>& args) -> Value {
        if (args.size() < 3) {
            iface->raiseError(1, "createWindow requires (width, height, title)");
            return iface->makeNil();
        }
        int w = (int)args[0].asInt();
        int h = (int)args[1].asInt();
        std::string title = args[2].asString();
        graphics->createWindow(w, h, title);
        return iface->makeNil();
    });
    
    exports["closeWindow"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        graphics->closeWindow();
        return Value::nil();
    });
    
    exports["windowShouldClose"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        return Value::fromBool(graphics->windowShouldClose());
    });
    
    // Object creation
    exports["createObject"] = Value::fromNative([iface, graphics](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            iface->raiseError(1, "createObject requires (type, size)");
            return iface->makeNil();
        }
        std::string type = args[0].asString();
        float size = args.size() > 1 ? (float)args[1].asFloat() : 1.0f;
        
        int id;
        if (type == "cube") {
            id = graphics->createCube(size);
        } else if (type == "sphere") {
            id = graphics->createSphere(size);
        } else {
            id = graphics->createCube(size);  // Default
        }
        return iface->makeInt(id);
    });
    
    exports["setPosition"] = Value::fromNative([iface, graphics](const std::vector<Value>& args) -> Value {
        if (args.size() < 4) {
            iface->raiseError(1, "setPosition requires (id, x, y, z)");
            return iface->makeNil();
        }
        int id = (int)args[0].asInt();
        float x = (float)args[1].asFloat();
        float y = (float)args[2].asFloat();
        float z = (float)args[3].asFloat();
        graphics->setObjectPosition(id, x, y, z);
        return iface->makeNil();
    });
    
    exports["setObjectColor"] = Value::fromNative([iface, graphics](const std::vector<Value>& args) -> Value {
        if (args.size() < 4) {
            iface->raiseError(1, "setObjectColor requires (id, r, g, b)");
            return iface->makeNil();
        }
        int id = (int)args[0].asInt();
        int r = (int)args[1].asInt();
        int g = (int)args[2].asInt();
        int b = (int)args[3].asInt();
        graphics->setObjectColor(id, r, g, b);
        return iface->makeNil();
    });
    
    exports["drawObject"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::nil();
        int id = (int)args[0].asInt();
        graphics->drawObject(id);
        return Value::nil();
    });
    
    // 2D drawing
    exports["clear"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        int r = args.size() > 0 ? (int)args[0].asInt() : 0;
        int g = args.size() > 1 ? (int)args[1].asInt() : 0;
        int b = args.size() > 2 ? (int)args[2].asInt() : 0;
        graphics->clear(r, g, b);
        return Value::nil();
    });
    
    exports["drawRect"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        if (args.size() < 5) return Value::nil();
        int x = (int)args[0].asInt();
        int y = (int)args[1].asInt();
        int w = (int)args[2].asInt();
        int h = (int)args[3].asInt();
        int r = (int)args[4].asInt();
        int g = args.size() > 5 ? (int)args[5].asInt() : r;
        int b = args.size() > 6 ? (int)args[6].asInt() : r;
        bool fill = args.size() > 7 ? args[7].asBool() : true;
        graphics->drawRect(x, y, w, h, r, g, b, fill);
        return Value::nil();
    });
    
    // Render loop helpers
    exports["beginDrawing"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        graphics->beginFrame();
        return Value::nil();
    });
    
    exports["endDrawing"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        graphics->endFrame();
        return Value::nil();
    });
    
    exports["begin3D"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        graphics->begin3D();
        return Value::nil();
    });
    
    exports["end3D"] = Value::fromNative([graphics](const std::vector<Value>& args) -> Value {
        graphics->end3D();
        return Value::nil();
    });
}

} // namespace graphics
} // namespace kern
