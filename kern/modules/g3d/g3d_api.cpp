/* *
 * kern/modules/g3d/g3d_api.cpp - Implementation of Ursina-Style 3D API
 */
#include "g3d_api.hpp"
#include <cmath>
#include <algorithm>

namespace kern {
namespace g3d {

// ============================================================================
// Vec3 Implementation
// ============================================================================

// Vec3 methods are inline in header for performance

// ============================================================================
// ColorRGB Implementation
// ============================================================================

// Static methods inline in header

// ============================================================================
// Entity Implementation
// ============================================================================

Entity::Entity() {
    id_ = internal::ECSBridge::instance().createEntity();
    
    // Add default components
    internal::ECSBridge::instance().addComponent(id_, internal::TransformComponent{});
    internal::ECSBridge::instance().addComponent(id_, internal::MeshComponent{});
    internal::ECSBridge::instance().addComponent(id_, internal::MaterialComponent{});
    
    // Default to cube mesh
    auto* mesh = internal::ECSBridge::instance().getComponent<internal::MeshComponent>(id_);
    if (mesh) mesh->loadCube();
}

Entity::Entity(const std::string& model) : Entity() {
    setModel(model);
}

Entity::Entity(const std::string& model, const Vec3& position) : Entity(model) {
    setPosition(position);
}

Entity::Entity(const std::string& model, const Vec3& position, const ColorRGB& color) 
    : Entity(model, position) {
    setColor(color);
}

Entity::Entity(const Params& params) {
    id_ = internal::ECSBridge::instance().createEntity();
    
    // Add transform
    internal::TransformComponent transform;
    transform.position = params.position;
    transform.rotation = params.rotation;
    transform.scale = params.scale;
    internal::ECSBridge::instance().addComponent(id_, transform);
    
    // Add mesh
    internal::MeshComponent mesh;
    mesh.modelPath = params.model;
    if (params.model == "cube") mesh.loadCube();
    else if (params.model == "sphere") mesh.loadSphere();
    else if (params.model == "plane") mesh.loadPlane();
    else mesh.loadFromFile(params.model);
    internal::ECSBridge::instance().addComponent(id_, mesh);
    
    // Add material
    internal::MaterialComponent material;
    material.color = params.color;
    material.wireframe = params.wireframe;
    material.visible = params.visible;
    internal::ECSBridge::instance().addComponent(id_, material);
    
    // Name
    if (!params.name.empty()) {
        setName(params.name);
    }
}

Entity::~Entity() {
    if (id_ != 0) {
        internal::ECSBridge::instance().destroyEntity(id_);
    }
}

Entity::Entity(Entity&& other) noexcept 
    : id_(other.id_),
      cachedTransform_(other.cachedTransform_),
      cachedMesh_(other.cachedMesh_),
      cachedMaterial_(other.cachedMaterial_) {
    other.id_ = 0;
    other.cachedTransform_ = nullptr;
    other.cachedMesh_ = nullptr;
    other.cachedMaterial_ = nullptr;
}

Entity& Entity::operator=(Entity&& other) noexcept {
    if (this != &other) {
        if (id_ != 0) {
            internal::ECSBridge::instance().destroyEntity(id_);
        }
        id_ = other.id_;
        cachedTransform_ = other.cachedTransform_;
        cachedMesh_ = other.cachedMesh_;
        cachedMaterial_ = other.cachedMaterial_;
        other.id_ = 0;
        other.cachedTransform_ = nullptr;
        other.cachedMesh_ = nullptr;
        other.cachedMaterial_ = nullptr;
    }
    return *this;
}

void Entity::ensureCached() const {
    if (!cachedTransform_) {
        cachedTransform_ = internal::ECSBridge::instance()
            .getComponent<internal::TransformComponent>(id_);
    }
    if (!cachedMesh_) {
        cachedMesh_ = internal::ECSBridge::instance()
            .getComponent<internal::MeshComponent>(id_);
    }
    if (!cachedMaterial_) {
        cachedMaterial_ = internal::ECSBridge::instance()
            .getComponent<internal::MaterialComponent>(id_);
    }
}

void Entity::invalidateCache() {
    cachedTransform_ = nullptr;
    cachedMesh_ = nullptr;
    cachedMaterial_ = nullptr;
}

// Position
Vec3 Entity::getPosition() const {
    ensureCached();
    if (cachedTransform_) return cachedTransform_->position;
    return Vec3::zero();
}

void Entity::setPosition(const Vec3& pos) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->position = pos;
}

float Entity::getX() const { return getPosition().x; }
float Entity::getY() const { return getPosition().y; }
float Entity::getZ() const { return getPosition().z; }

void Entity::setX(float x) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->position.x = x;
}

void Entity::setY(float y) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->position.y = y;
}

void Entity::setZ(float z) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->position.z = z;
}

// Rotation
Vec3 Entity::getRotation() const {
    ensureCached();
    if (cachedTransform_) return cachedTransform_->rotation;
    return Vec3::zero();
}

void Entity::setRotation(const Vec3& rot) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->rotation = rot;
}

float Entity::getRotationX() const { return getRotation().x; }
float Entity::getRotationY() const { return getRotation().y; }
float Entity::getRotationZ() const { return getRotation().z; }

void Entity::setRotationX(float x) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->rotation.x = x;
}

void Entity::setRotationY(float y) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->rotation.y = y;
}

void Entity::setRotationZ(float z) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->rotation.z = z;
}

// Scale
Vec3 Entity::getScale() const {
    ensureCached();
    if (cachedTransform_) return cachedTransform_->scale;
    return Vec3::one();
}

void Entity::setScale(const Vec3& scale) {
    ensureCached();
    if (cachedTransform_) cachedTransform_->scale = scale;
}

void Entity::setScale(float uniformScale) {
    setScale(Vec3(uniformScale));
}

// Color
ColorRGB Entity::getColor() const {
    ensureCached();
    if (cachedMaterial_) return cachedMaterial_->color;
    return ColorRGB::white();
}

void Entity::setColor(const ColorRGB& color) {
    ensureCached();
    if (cachedMaterial_) cachedMaterial_->color = color;
}

// Visibility
bool Entity::isVisible() const {
    ensureCached();
    if (cachedMaterial_) return cachedMaterial_->visible;
    return true;
}

void Entity::setVisible(bool visible) {
    ensureCached();
    if (cachedMaterial_) cachedMaterial_->visible = visible;
}

// Wireframe
bool Entity::isWireframe() const {
    ensureCached();
    if (cachedMaterial_) return cachedMaterial_->wireframe;
    return false;
}

void Entity::setWireframe(bool wireframe) {
    ensureCached();
    if (cachedMaterial_) cachedMaterial_->wireframe = wireframe;
}

// Model
std::string Entity::getModel() const {
    ensureCached();
    if (cachedMesh_) return cachedMesh_->modelPath;
    return "";
}

void Entity::setModel(const std::string& model) {
    ensureCached();
    if (!cachedMesh_) return;
    
    cachedMesh_->modelPath = model;
    
    if (model == "cube") cachedMesh_->loadCube();
    else if (model == "sphere") cachedMesh_->loadSphere();
    else if (model == "plane") cachedMesh_->loadPlane();
    else cachedMesh_->loadFromFile(model);
}

// Name
std::string Entity::getName() const {
    // Would retrieve from ECS name component
    return "";
}

void Entity::setName(const std::string& name) {
    // Would store in ECS name component
    (void)name;
}

// Transform helpers
void Entity::lookAt(const Vec3& target) {
    Vec3 pos = getPosition();
    Vec3 direction = {target.x - pos.x, target.y - pos.y, target.z - pos.z};
    
    // Calculate rotation to face target
    float yaw = atan2f(direction.x, direction.z) * (180.0f / PI);
    float pitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z)) * (180.0f / PI);
    
    setRotation({-pitch, yaw, 0});
}

void Entity::lookAt(const Entity& target) {
    lookAt(target.getPosition());
}

void Entity::moveForward(float distance) {
    Vec3 rot = getRotation();
    float yaw = rot.y * (PI / 180.0f);
    
    Vec3 pos = getPosition();
    pos.x += sinf(yaw) * distance;
    pos.z += cosf(yaw) * distance;
    setPosition(pos);
}

void Entity::moveRight(float distance) {
    Vec3 rot = getRotation();
    float yaw = rot.y * (PI / 180.0f);
    
    Vec3 pos = getPosition();
    pos.x += cosf(yaw) * distance;
    pos.z -= sinf(yaw) * distance;
    setPosition(pos);
}

void Entity::moveUp(float distance) {
    Vec3 pos = getPosition();
    pos.y += distance;
    setPosition(pos);
}

// ============================================================================
// Camera Implementation
// ============================================================================

Camera::Camera() : Camera(Vec3(0, 2, -5)) {}

Camera::Camera(const Vec3& position) {
    id_ = internal::ECSBridge::instance().createEntity();
    
    // Transform
    internal::TransformComponent transform;
    transform.position = position;
    internal::ECSBridge::instance().addComponent(id_, transform);
    
    // Camera
    internal::CameraComponent camera;
    camera.isMain = true;
    internal::ECSBridge::instance().addComponent(id_, camera);
    
    // Input controller
    internal::InputControllerComponent input;
    internal::ECSBridge::instance().addComponent(id_, input);
}

Camera::Camera(const Params& params) {
    id_ = internal::ECSBridge::instance().createEntity();
    
    internal::TransformComponent transform;
    transform.position = params.position;
    transform.rotation = params.rotation;
    internal::ECSBridge::instance().addComponent(id_, transform);
    
    internal::CameraComponent camera;
    camera.fov = params.fov;
    camera.projection = params.orthographic ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
    camera.isMain = false;
    internal::ECSBridge::instance().addComponent(id_, camera);
    
    internal::InputControllerComponent input;
    input.enableWASD = params.enableWASD;
    input.enableMouseLook = params.enableMouseLook;
    internal::ECSBridge::instance().addComponent(id_, input);
}

float Camera::getFov() const {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    return cam ? cam->fov : 45.0f;
}

void Camera::setFov(float fov) {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    if (cam) cam->fov = fov;
}

bool Camera::isOrthographic() const {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    return cam ? cam->projection == CAMERA_ORTHOGRAPHIC : false;
}

void Camera::setOrthographic(bool ortho) {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    if (cam) cam->projection = ortho ? CAMERA_ORTHOGRAPHIC : CAMERA_PERSPECTIVE;
}

void Camera::setAsMain() {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    if (cam) cam->isMain = true;
    
    // Set in App
    App::instance().setMainCamera(this);
}

bool Camera::isMain() const {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    return cam ? cam->isMain : false;
}

void Camera::enableWASD(bool enable) {
    auto* input = internal::ECSBridge::instance()
        .getComponent<internal::InputControllerComponent>(id_);
    if (input) input->enableWASD = enable;
}

bool Camera::isWASDEnabled() const {
    auto* input = internal::ECSBridge::instance()
        .getComponent<internal::InputControllerComponent>(id_);
    return input ? input->enableWASD : false;
}

void Camera::enableMouseLook(bool enable) {
    auto* input = internal::ECSBridge::instance()
        .getComponent<internal::InputControllerComponent>(id_);
    if (input) input->enableMouseLook = enable;
}

bool Camera::isMouseLookEnabled() const {
    auto* input = internal::ECSBridge::instance()
        .getComponent<internal::InputControllerComponent>(id_);
    return input ? input->enableMouseLook : false;
}

void Camera::processInput(float dt) {
    auto* input = internal::ECSBridge::instance()
        .getComponent<internal::InputControllerComponent>(id_);
    auto* transform = internal::ECSBridge::instance()
        .getComponent<internal::TransformComponent>(id_);
    
    if (!input || !transform) return;
    
    // WASD movement
    if (input->enableWASD) {
        float speed = input->moveSpeed * dt;
        
        if (input::held(input::Key::W)) moveForward(speed);
        if (input::held(input::Key::S)) moveForward(-speed);
        if (input::held(input::Key::A)) moveRight(-speed);
        if (input::held(input::Key::D)) moveRight(speed);
        if (input::held(input::Key::SPACE)) moveUp(speed);
    }
    
    // Mouse look
    if (input->enableMouseLook && input::isMouseLocked()) {
        Vec2 delta = input::getMouseDelta();
        Vec3 rot = getRotation();
        rot.y += delta.x * input->mouseSensitivity;
        rot.x -= delta.y * input->mouseSensitivity;
        
        // Clamp pitch
        if (rot.x > 89.0f) rot.x = 89.0f;
        if (rot.x < -89.0f) rot.x = -89.0f;
        
        setRotation(rot);
    }
}

void Camera::lookAt(const Vec3& target) {
    Entity::lookAt(target);
}

void Camera::lookAt(const Entity& target) {
    Entity::lookAt(target);
}

Matrix Camera::getViewMatrix() const {
    auto* cam = internal::ECSBridge::instance()
        .getComponent<internal::CameraComponent>(id_);
    auto* transform = internal::ECSBridge::instance()
        .getComponent<internal::TransformComponent>(id_);
    
    if (!cam || !transform) return MatrixIdentity();
    
    return cam->toRaylibCamera(*transform);
}

// ============================================================================
// Light Implementation
// ============================================================================

Light::Light() : Light(POINT) {}

Light::Light(Type type) {
    id_ = internal::ECSBridge::instance().createEntity();
    
    internal::TransformComponent transform;
    transform.position = {0, 5, 0};
    internal::ECSBridge::instance().addComponent(id_, transform);
    
    internal::LightComponent light;
    light.type = type;
    internal::ECSBridge::instance().addComponent(id_, light);
}

Light::Light(const Params& params) {
    id_ = internal::ECSBridge::instance().createEntity();
    
    internal::TransformComponent transform;
    transform.position = params.position;
    internal::ECSBridge::instance().addComponent(id_, transform);
    
    internal::LightComponent light;
    light.type = params.type;
    light.color = params.color;
    light.intensity = params.intensity;
    light.range = params.range;
    internal::ECSBridge::instance().addComponent(id_, light);
}

Light::Type Light::getType() const {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    return light ? static_cast<Type>(light->type) : POINT;
}

void Light::setType(Type type) {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    if (light) light->type = type;
}

ColorRGB Light::getColor() const {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    return light ? light->color : ColorRGB::white();
}

void Light::setColor(const ColorRGB& color) {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    if (light) light->color = color;
}

float Light::getIntensity() const {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    return light ? light->intensity : 1.0f;
}

void Light::setIntensity(float intensity) {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    if (light) light->intensity = intensity;
}

float Light::getRange() const {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    return light ? light->range : 10.0f;
}

void Light::setRange(float range) {
    auto* light = internal::ECSBridge::instance()
        .getComponent<internal::LightComponent>(id_);
    if (light) light->range = range;
}

// ============================================================================
// Input Implementation
// ============================================================================

namespace input {

bool held(int key) {
    return IsKeyDown(key);
}

bool pressed(int key) {
    return IsKeyPressed(key);
}

bool released(int key) {
    return IsKeyReleased(key);
}

Vec2 getMousePosition() {
    Vector2 pos = GetMousePosition();
    return {pos.x, pos.y};
}

Vec2 getMouseDelta() {
    Vector2 delta = GetMouseDelta();
    return {delta.x, delta.y};
}

float getMouseWheel() {
    return GetMouseWheelMove();
}

void lockMouse() {
    DisableCursor();
}

void unlockMouse() {
    EnableCursor();
}

bool isMouseLocked() {
    return IsCursorHidden();
}

bool isGamepadAvailable(int gamepad) {
    return IsGamepadAvailable(gamepad);
}

float getGamepadAxis(int gamepad, int axis) {
    return GetGamepadAxisMovement(gamepad, axis);
}

bool getGamepadButton(int gamepad, int button) {
    return IsGamepadButtonDown(gamepad, button);
}

} // namespace input

// ============================================================================
// App Implementation
// ============================================================================

App& App::instance() {
    static App instance;
    return instance;
}

App& app() {
    return App::instance();
}

void App::configure(const Config& config) {
    config_ = config;
}

Entity* App::createEntity(const Entity::Params& params) {
    auto entity = std::make_unique<Entity>(params);
    Entity* ptr = entity.get();
    
    if (!params.name.empty()) {
        namedEntities_[params.name] = ptr;
    }
    
    ownedEntities_.push_back(std::move(entity));
    return ptr;
}

Camera* App::createCamera(const Camera::Params& params) {
    auto camera = std::make_unique<Camera>(params);
    Camera* ptr = camera.get();
    
    if (!mainCamera_) {
        mainCamera_ = ptr;
        ptr->setAsMain();
    }
    
    ownedEntities_.push_back(std::move(camera));
    return ptr;
}

Light* App::createLight(const Light::Params& params) {
    auto light = std::make_unique<Light>(params);
    Light* ptr = light.get();
    ownedEntities_.push_back(std::move(light));
    return ptr;
}

Entity* App::findEntity(const std::string& name) {
    auto it = namedEntities_.find(name);
    return (it != namedEntities_.end()) ? it->second : nullptr;
}

void App::onUpdate(UpdateFunc func) {
    updateFuncs_.push_back(func);
}

void App::onStart(StartFunc func) {
    startFuncs_.push_back(func);
}

void App::init() {
    if (initialized_) return;
    
    // Initialize window
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(config_.windowWidth, config_.windowHeight, config_.title.c_str());
    
    if (config_.fullscreen) {
        ToggleFullscreen();
    }
    
    SetTargetFPS(config_.targetFps);
    
    // Initialize ECS
    internal::ECSBridge::instance().init();
    
    // Initialize 3D
    SetTraceLogLevel(LOG_WARNING);
    
    initialized_ = true;
}

void App::shutdown() {
    if (!initialized_) return;
    
    // Cleanup entities
    ownedEntities_.clear();
    namedEntities_.clear();
    
    // Shutdown ECS
    internal::ECSBridge::instance().shutdown();
    
    // Close window
    CloseWindow();
    
    initialized_ = false;
}

void App::run() {
    init();
    running_ = true;
    
    // Call start functions
    if (!hasStarted_) {
        for (auto& func : startFuncs_) {
            func();
        }
        hasStarted_ = true;
    }
    
    // Main loop
    while (running_ && !WindowShouldClose()) {
        // Time
        deltaTime_ = GetFrameTime();
        totalTime_ += deltaTime_;
        currentFps_ = GetFPS();
        
        // Update
        update(deltaTime_);
        
        // Render
        BeginDrawing();
        ClearBackground(config_.backgroundColor);
        
        render();
        
        // FPS counter
        if (config_.showFPS) {
            DrawFPS(10, 10);
        }
        
        EndDrawing();
    }
    
    shutdown();
}

void App::stop() {
    running_ = false;
}

void App::update(float dt) {
    // Process input on main camera
    if (mainCamera_) {
        mainCamera_->processInput(dt);
    }
    
    // Update ECS
    internal::ECSBridge::instance().update(dt);
    
    // Call registered update functions
    for (auto& func : updateFuncs_) {
        func(dt);
    }
}

void App::render() {
    // Begin 3D mode with main camera
    if (mainCamera_) {
        Camera3D rayCamera = {0};
        auto* cam = internal::ECSBridge::instance()
            .getComponent<internal::CameraComponent>(mainCamera_->getInternalId());
        auto* transform = internal::ECSBridge::instance()
            .getComponent<internal::TransformComponent>(mainCamera_->getInternalId());
        
        if (cam && transform) {
            rayCamera = cam->toRaylibCamera(*transform);
        }
        
        BeginMode3D(rayCamera);
        
        // Render all entities with meshes
        internal::ECSBridge::instance().render();
        
        EndMode3D();
    }
}

void App::setMainCamera(Camera* camera) {
    mainCamera_ = camera;
}

void App::setBackgroundColor(const ColorRGB& color) {
    config_.backgroundColor = color;
}

void App::clearScene() {
    ownedEntities_.clear();
    namedEntities_.clear();
    mainCamera_ = nullptr;
}

// ============================================================================
// Utility Functions
// ============================================================================

float distance(const Entity& a, const Entity& b) {
    return distance(a.getPosition(), b.getPosition());
}

float distance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

Vec3 clamp(const Vec3& value, const Vec3& min, const Vec3& max) {
    return {
        clamp(value.x, min.x, max.x),
        clamp(value.y, min.y, max.y),
        clamp(value.z, min.z, max.z)
    };
}

float randomFloat(float min, float max) {
    return min + (max - min) * ((float)GetRandomValue(0, 1000) / 1000.0f);
}

int randomInt(int min, int max) {
    return GetRandomValue(min, max);
}

Vec3 randomVec3(float min, float max) {
    return {
        randomFloat(min, max),
        randomFloat(min, max),
        randomFloat(min, max)
    };
}

ColorRGB randomColor() {
    return {
        GetRandomValue(0, 255),
        GetRandomValue(0, 255),
        GetRandomValue(0, 255)
    };
}

float time() {
    return App::instance().getTime();
}

float deltaTime() {
    return App::instance().getDeltaTime();
}

void wait(float seconds) {
    WaitTime(seconds);
}

void drawLine(const Vec3& start, const Vec3& end, const ColorRGB& color) {
    DrawLine3D(start, end, color);
}

void drawPoint(const Vec3& pos, float size, const ColorRGB& color) {
    DrawSphere(pos, size, color);
}

void drawRay(const Vec3& origin, const Vec3& direction, float length, const ColorRGB& color) {
    Vec3 end = {
        origin.x + direction.x * length,
        origin.y + direction.y * length,
        origin.z + direction.z * length
    };
    DrawLine3D(origin, end, color);
}

} // namespace g3d
} // namespace kern
