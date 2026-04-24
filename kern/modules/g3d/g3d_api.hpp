/* *
 * kern/modules/g3d/g3d_api.hpp - Ursina-Style 3D API Layer
 * 
 * High-level API that wraps ECS internals.
 * User code looks like Ursina - no ECS knowledge required.
 * 
 * ECS is INTERNAL ONLY - users never see entity IDs, components, or systems.
 */
#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <raylib.h>

// Forward declare ECS types (internal implementation detail)
namespace ecs {
    class World;
    class Entity;
    class Component;
}

namespace kern {
namespace g3d {

// ============================================================================
// Internal ECS Bridge (hidden from users)
// ============================================================================

namespace internal {
    // ECS entity ID type - never exposed to users
    using EntityId = uint32_t;
    
    // Component type IDs (internal)
    enum class ComponentType : uint8_t {
        TRANSFORM = 0,
        MESH = 1,
        MATERIAL = 2,
        CAMERA = 3,
        LIGHT = 4,
        SCRIPT = 5,
        INPUT_CONTROLLER = 6
    };
    
    // ECS bridge singleton - manages the internal ECS world
    class ECSBridge {
    public:
        static ECSBridge& instance();
        
        // Entity management
        EntityId createEntity();
        void destroyEntity(EntityId id);
        bool entityExists(EntityId id);
        
        // Component management (template-based for type safety)
        template<typename T>
        void addComponent(EntityId id, const T& component);
        
        template<typename T>
        T* getComponent(EntityId id);
        
        template<typename T>
        void removeComponent(EntityId id);
        
        bool hasComponent(EntityId id, ComponentType type);
        
        // World lifecycle
        void init();
        void update(float dt);
        void render();
        void shutdown();
        
        // Access to underlying ECS world (for internal systems only)
        ecs::World* getWorld();
        
    private:
        ECSBridge() = default;
        ~ECSBridge() = default;
        ECSBridge(const ECSBridge&) = delete;
        ECSBridge& operator=(const ECSBridge&) = delete;
        
        std::unique_ptr<ecs::World> world_;
        std::unordered_map<EntityId, std::vector<ComponentType>> entityComponents_;
        EntityId nextId_ = 1;
    };
    
    // Component structures (internal - users never see these)
    struct TransformComponent {
        Vector3 position = {0, 0, 0};
        Vector3 rotation = {0, 0, 0};  // Euler angles in degrees
        Vector3 scale = {1, 1, 1};
        
        Matrix getMatrix() const;
    };
    
    struct MeshComponent {
        Mesh mesh = {};
        bool loaded = false;
        std::string modelPath;
        
        void loadCube();
        void loadSphere();
        void loadPlane();
        void loadFromFile(const std::string& path);
        void unload();
    };
    
    struct MaterialComponent {
        Color color = WHITE;
        float metallic = 0.0f;
        float roughness = 0.5f;
        bool wireframe = false;
        bool visible = true;
        
        Material toRaylibMaterial() const;
    };
    
    struct CameraComponent {
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        int projection = CAMERA_PERSPECTIVE;
        bool isMain = false;
        
        Camera3D toRaylibCamera(const TransformComponent& transform) const;
    };
    
    struct LightComponent {
        int type = LIGHT_POINT;  // LIGHT_POINT, LIGHT_DIRECTIONAL, LIGHT_SPOT
        Color color = WHITE;
        float intensity = 1.0f;
        float range = 10.0f;
    };
    
    struct ScriptComponent {
        std::function<void(float)> updateFunc;
        std::function<void()> startFunc;
        bool hasStarted = false;
    };
    
    struct InputControllerComponent {
        bool enableWASD = true;
        bool enableMouseLook = true;
        float moveSpeed = 5.0f;
        float mouseSensitivity = 0.1f;
    };
} // namespace internal

// ============================================================================
// Public API - Ursina-Style Interface
// ============================================================================

// Forward declarations
class Entity;
class Camera;
class Light;
class App;

// Vec3 helper - simple 3D vector like Ursina
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(float v) : x(v), y(v), z(v) {}
    
    // Implicit conversion to/from Raylib Vector3
    Vec3(const Vector3& v) : x(v.x), y(v.y), z(v.z) {}
    operator Vector3() const { return {x, y, z}; }
    
    // Arithmetic operators
    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    
    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    
    // Common values
    static Vec3 zero() { return {0, 0, 0}; }
    static Vec3 one() { return {1, 1, 1}; }
    static Vec3 up() { return {0, 1, 0}; }
    static Vec3 right() { return {1, 0, 0}; }
    static Vec3 forward() { return {0, 0, 1}; }
};

// Color helper
struct ColorRGB {
    int r, g, b, a;
    
    ColorRGB() : r(255), g(255), b(255), a(255) {}
    ColorRGB(int r, int g, int b, int a = 255) : r(r), g(g), b(b), a(a) {}
    
    // Named colors like Ursina
    static ColorRGB red() { return {255, 0, 0}; }
    static ColorRGB green() { return {0, 255, 0}; }
    static ColorRGB blue() { return {0, 0, 255}; }
    static ColorRGB white() { return {255, 255, 255}; }
    static ColorRGB black() { return {0, 0, 0}; }
    static ColorRGB yellow() { return {255, 255, 0}; }
    static ColorRGB orange() { return {255, 165, 0}; }
    static ColorRGB purple() { return {128, 0, 128}; }
    static ColorRGB cyan() { return {0, 255, 255}; }
    
    operator Color() const { return {(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
};

// ============================================================================
// Entity Class - Main 3D Object Wrapper
// ============================================================================

class Entity {
public:
    // Constructors - various ways to create like Ursina
    Entity();
    Entity(const std::string& model);
    Entity(const std::string& model, const Vec3& position);
    Entity(const std::string& model, const Vec3& position, const ColorRGB& color);
    
    // Named parameter style (like Ursina kwargs)
    struct Params {
        std::string model = "cube";
        Vec3 position = Vec3::zero();
        Vec3 rotation = Vec3::zero();
        Vec3 scale = Vec3::one();
        ColorRGB color = ColorRGB::white();
        bool wireframe = false;
        bool visible = true;
        std::string texture;
        std::string name;
    };
    
    explicit Entity(const Params& params);
    
    // Destructor - automatically cleans up ECS entity
    virtual ~Entity();
    
    // Disable copy (unique ownership of ECS entity)
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    
    // Enable move
    Entity(Entity&& other) noexcept;
    Entity& operator=(Entity&& other) noexcept;
    
    // Position property
    Vec3 getPosition() const;
    void setPosition(const Vec3& pos);
    
    // Direct property access like Ursina: entity.x += 1
    float getX() const;
    float getY() const;
    float getZ() const;
    void setX(float x);
    void setY(float y);
    void setZ(float z);
    
    // Rotation property (Euler angles in degrees)
    Vec3 getRotation() const;
    void setRotation(const Vec3& rot);
    float getRotationX() const;
    float getRotationY() const;
    float getRotationZ() const;
    void setRotationX(float x);
    void setRotationY(float y);
    void setRotationZ(float z);
    
    // Scale property
    Vec3 getScale() const;
    void setScale(const Vec3& scale);
    void setScale(float uniformScale);
    
    // Color property
    ColorRGB getColor() const;
    void setColor(const ColorRGB& color);
    
    // Visibility
    bool isVisible() const;
    void setVisible(bool visible);
    void hide() { setVisible(false); }
    void show() { setVisible(true); }
    
    // Wireframe
    bool isWireframe() const;
    void setWireframe(bool wireframe);
    
    // Model
    std::string getModel() const;
    void setModel(const std::string& model);
    
    // Name (for debugging)
    std::string getName() const;
    void setName(const std::string& name);
    
    // Transform helpers
    void lookAt(const Vec3& target);
    void lookAt(const Entity& target);
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    
    // Local vs world space
    Vec3 worldPosition() const;  // For parented entities
    void setWorldPosition(const Vec3& pos);
    
    // Parenting (for hierarchical transforms)
    void setParent(Entity* parent);
    Entity* getParent() const;
    std::vector<Entity*> getChildren() const;
    
    // Internal use only - for App to access ECS ID
    internal::EntityId getInternalId() const { return id_; }
    
protected:
    internal::EntityId id_ = 0;  // ECS entity ID - never exposed to users
    
    // Cached component pointers for fast access (no per-frame lookup)
    mutable internal::TransformComponent* cachedTransform_ = nullptr;
    mutable internal::MeshComponent* cachedMesh_ = nullptr;
    mutable internal::MaterialComponent* cachedMaterial_ = nullptr;
    
    void ensureCached() const;  // Lazy cache initialization
    void invalidateCache();
    
    // Allow App to create entities with specific IDs
    friend class App;
    explicit Entity(internal::EntityId id);
};

// ============================================================================
// Camera Class - Camera Wrapper
// ============================================================================

class Camera : public Entity {
public:
    Camera();
    explicit Camera(const Vec3& position);
    
    struct Params {
        Vec3 position = {0, 2, -5};
        Vec3 rotation = {0, 0, 0};
        float fov = 45.0f;
        bool orthographic = false;
        bool enableWASD = false;
        bool enableMouseLook = false;
    };
    
    explicit Camera(const Params& params);
    
    // Camera-specific properties
    float getFov() const;
    void setFov(float fov);
    
    bool isOrthographic() const;
    void setOrthographic(bool ortho);
    
    // Make this the main camera
    void setAsMain();
    bool isMain() const;
    
    // WASD movement
    void enableWASD(bool enable);
    bool isWASDEnabled() const;
    
    // Mouse look
    void enableMouseLook(bool enable);
    bool isMouseLookEnabled() const;
    
    // Input handling (called automatically by App)
    void processInput(float dt);
    
    // Look at target
    void lookAt(const Vec3& target);
    void lookAt(const Entity& target);
    
    // Get view matrix
    Matrix getViewMatrix() const;
    
private:
    mutable internal::CameraComponent* cachedCamera_ = nullptr;
    mutable internal::InputControllerComponent* cachedInput_ = nullptr;
};

// ============================================================================
// Light Class - Light Source Wrapper
// ============================================================================

class Light : public Entity {
public:
    enum Type {
        POINT = 0,
        DIRECTIONAL = 1,
        SPOT = 2
    };
    
    Light();
    explicit Light(Type type);
    
    struct Params {
        Type type = POINT;
        Vec3 position = {0, 5, 0};
        ColorRGB color = ColorRGB::white();
        float intensity = 1.0f;
        float range = 10.0f;
    };
    
    explicit Light(const Params& params);
    
    // Light properties
    Type getType() const;
    void setType(Type type);
    
    ColorRGB getColor() const;
    void setColor(const ColorRGB& color);
    
    float getIntensity() const;
    void setIntensity(float intensity);
    
    float getRange() const;
    void setRange(float range);
    
private:
    mutable internal::LightComponent* cachedLight_ = nullptr;
};

// ============================================================================
// Input System - Clean Abstraction
// ============================================================================

namespace input {
    // Key constants like Ursina
    namespace Key {
        constexpr int W = KEY_W;
        constexpr int A = KEY_A;
        constexpr int S = KEY_S;
        constexpr int D = KEY_D;
        constexpr int SPACE = KEY_SPACE;
        constexpr int ESCAPE = KEY_ESCAPE;
        constexpr int ENTER = KEY_ENTER;
        constexpr int UP = KEY_UP;
        constexpr int DOWN = KEY_DOWN;
        constexpr int LEFT = KEY_LEFT;
        constexpr int RIGHT = KEY_RIGHT;
        constexpr int SHIFT = KEY_LEFT_SHIFT;
        constexpr int CTRL = KEY_LEFT_CONTROL;
    }
    
    // Mouse buttons
    namespace Mouse {
        constexpr int LEFT = MOUSE_LEFT_BUTTON;
        constexpr int RIGHT = MOUSE_RIGHT_BUTTON;
        constexpr int MIDDLE = MOUSE_MIDDLE_BUTTON;
    }
    
    // Check if key is held down
    bool held(int key);
    
    // Check if key was pressed this frame
    bool pressed(int key);
    
    // Check if key was released this frame
    bool released(int key);
    
    // Mouse position
    Vec2 getMousePosition();
    Vec2 getMouseDelta();
    
    // Mouse wheel
    float getMouseWheel();
    
    // Lock/hide mouse cursor (for FPS camera)
    void lockMouse();
    void unlockMouse();
    bool isMouseLocked();
    
    // Gamepad (if available)
    bool isGamepadAvailable(int gamepad = 0);
    float getGamepadAxis(int gamepad, int axis);
    bool getGamepadButton(int gamepad, int button);
}

// Vec2 helper for input
struct Vec2 {
    float x, y;
    
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    
    operator Vector2() const { return {x, y}; }
};

// ============================================================================
// App Class - Application / Game Loop
// ============================================================================

class App {
public:
    struct Config {
        int windowWidth = 1280;
        int windowHeight = 720;
        std::string title = "Kern G3D";
        bool fullscreen = false;
        bool vsync = true;
        int targetFps = 60;
        ColorRGB backgroundColor = ColorRGB::black();
        bool showFPS = true;
    };
    
    // Singleton access
    static App& instance();
    
    // Configuration
    void configure(const Config& config);
    Config getConfig() const { return config_; }
    
    // Entity management
    Entity* createEntity(const Entity::Params& params = {});
    Camera* createCamera(const Camera::Params& params = {});
    Light* createLight(const Light::Params& params = {});
    
    // Entity queries (by name or tag)
    Entity* findEntity(const std::string& name);
    std::vector<Entity*> findEntitiesWithTag(const std::string& tag);
    
    // Update function registration
    using UpdateFunc = std::function<void(float)>;
    using StartFunc = std::function<void()>;
    
    void onUpdate(UpdateFunc func);
    void onStart(StartFunc func);
    
    // Main loop
    void run();
    void stop();
    bool isRunning() const { return running_; }
    
    // Time
    float getDeltaTime() const { return deltaTime_; }
    float getTime() const { return totalTime_; }
    int getFPS() const { return currentFps_; }
    
    // Scene management
    void clearScene();  // Destroy all entities
    void loadScene(const std::string& sceneFile);
    void saveScene(const std::string& sceneFile);
    
    // Main camera
    void setMainCamera(Camera* camera);
    Camera* getMainCamera() const { return mainCamera_; }
    
    // Rendering settings
    void setBackgroundColor(const ColorRGB& color);
    void enableShadows(bool enable);
    void setAmbientLight(float intensity);
    
private:
    App() = default;
    ~App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    
    Config config_;
    bool running_ = false;
    bool initialized_ = false;
    
    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;
    int currentFps_ = 0;
    
    Camera* mainCamera_ = nullptr;
    
    // Registered callbacks
    std::vector<UpdateFunc> updateFuncs_;
    std::vector<StartFunc> startFuncs_;
    bool hasStarted_ = false;
    
    // Entity tracking (for queries and cleanup)
    std::unordered_map<std::string, Entity*> namedEntities_;
    std::vector<std::unique_ptr<Entity>> ownedEntities_;
    
    void init();
    void shutdown();
    void update(float dt);
    void render();
    
    // Internal ECS bridge access
    internal::ECSBridge& getECS() { return internal::ECSBridge::instance(); }
};

// Global convenience function like Ursina's default
App& app();

// ============================================================================
// Utility Functions
// ============================================================================

// Distance between entities
float distance(const Entity& a, const Entity& b);
float distance(const Vec3& a, const Vec3& b);

// Lerp
Vec3 lerp(const Vec3& a, const Vec3& b, float t);
float lerp(float a, float b, float t);

// Clamp
float clamp(float value, float min, float max);
Vec3 clamp(const Vec3& value, const Vec3& min, const Vec3& max);

// Random
float randomFloat(float min = 0.0f, float max = 1.0f);
int randomInt(int min, int max);
Vec3 randomVec3(float min = -1.0f, float max = 1.0f);
ColorRGB randomColor();

// Time
float time();  // Total time since App started
float deltaTime();  // Time since last frame
void wait(float seconds);  // Blocking wait (for scripts)

// Debug drawing
void drawLine(const Vec3& start, const Vec3& end, const ColorRGB& color = ColorRGB::white());
void drawPoint(const Vec3& pos, float size = 0.1f, const ColorRGB& color = ColorRGB::red());
void drawRay(const Vec3& origin, const Vec3& direction, float length = 10.0f, 
             const ColorRGB& color = ColorRGB::yellow());

} // namespace g3d
} // namespace kern
