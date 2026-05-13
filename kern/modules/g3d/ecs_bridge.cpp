/* *
 * kern/modules/g3d/ecs_bridge.cpp - ECS Bridge Implementation
 * 
 * Minimal ECS implementation that bridges to Raylib for rendering.
 * Users never see this - it's purely internal to the g3d module.
 */
#include "g3d_api.hpp"
#include <unordered_map>
#include <vector>

namespace kern {
namespace g3d {
namespace internal {

// ============================================================================
// Minimal ECS World Implementation
// ============================================================================

class ComponentPool {
public:
    virtual ~ComponentPool() = default;
    virtual void remove(EntityId entity) = 0;
};

template<typename T>
class TypedComponentPool : public ComponentPool {
public:
    std::unordered_map<EntityId, T> components;
    
    T* get(EntityId id) {
        auto it = components.find(id);
        return (it != components.end()) ? &it->second : nullptr;
    }
    
    void add(EntityId id, const T& component) {
        components[id] = component;
    }
    
    void remove(EntityId id) override {
        components.erase(id);
    }
    
    bool has(EntityId id) const {
        return components.find(id) != components.end();
    }
};

class ECSWorld {
public:
    std::unordered_map<EntityId, bool> entities;
    std::unordered_map<uint8_t, std::unique_ptr<ComponentPool>> componentPools;
    EntityId nextId = 1;
    
    template<typename T>
    TypedComponentPool<T>* getPool() {
        uint8_t typeId = static_cast<uint8_t>(getComponentType<T>());
        auto it = componentPools.find(typeId);
        if (it == componentPools.end()) {
            auto pool = std::make_unique<TypedComponentPool<T>>();
            TypedComponentPool<T>* ptr = pool.get();
            componentPools[typeId] = std::move(pool);
            return ptr;
        }
        return static_cast<TypedComponentPool<T>*>(it->second.get());
    }
    
    template<typename T>
    ComponentType getComponentType();
    
    EntityId createEntity() {
        EntityId id = nextId++;
        entities[id] = true;
        return id;
    }
    
    void destroyEntity(EntityId id) {
        entities.erase(id);
        // Remove all components
        for (auto& [type, pool] : componentPools) {
            pool->remove(id);
        }
    }
    
    bool entityExists(EntityId id) const {
        return entities.find(id) != entities.end();
    }
};

// Component type specializations
template<> ComponentType ECSWorld::getComponentType<TransformComponent>() { return ComponentType::TRANSFORM; }
template<> ComponentType ECSWorld::getComponentType<MeshComponent>() { return ComponentType::MESH; }
template<> ComponentType ECSWorld::getComponentType<MaterialComponent>() { return ComponentType::MATERIAL; }
template<> ComponentType ECSWorld::getComponentType<CameraComponent>() { return ComponentType::CAMERA; }
template<> ComponentType ECSWorld::getComponentType<LightComponent>() { return ComponentType::LIGHT; }
template<> ComponentType ECSWorld::getComponentType<ScriptComponent>() { return ComponentType::SCRIPT; }
template<> ComponentType ECSWorld::getComponentType<InputControllerComponent>() { return ComponentType::INPUT_CONTROLLER; }

// ============================================================================
// ECS Bridge Implementation
// ============================================================================

ECSBridge& ECSBridge::instance() {
    static ECSBridge instance;
    return instance;
}

void ECSBridge::init() {
    world_ = std::make_unique<ECSWorld>();
    
    // Initialize default meshes
    // This would load/create default cube, sphere, plane meshes
}

void ECSBridge::update(float dt) {
    // Update scripts
    auto* scriptPool = getPool<ScriptComponent>();
    for (auto& [id, script] : scriptPool->components) {
        // Call start if not started
        if (!script.hasStarted && script.startFunc) {
            script.startFunc();
            script.hasStarted = true;
        }
        // Call update
        if (script.updateFunc) {
            script.updateFunc(dt);
        }
    }
}

void ECSBridge::render() {
    // Render all entities with meshes
    auto* transformPool = getPool<TransformComponent>();
    auto* meshPool = getPool<MeshComponent>();
    auto* materialPool = getPool<MaterialComponent>();
    
    for (auto& [id, transform] : transformPool->components) {
        // Skip if no mesh
        auto* mesh = meshPool->get(id);
        if (!mesh || !mesh->loaded) continue;
        
        // Get material (or use default)
        auto* material = materialPool->get(id);
        if (!material || !material->visible) continue;
        
        // Build transform matrix
        Matrix matScale = MatrixScale(transform.scale.x, transform.scale.y, transform.scale.z);
        Matrix matRotation = MatrixRotateXYZ({
            transform.rotation.x * DEG2RAD,
            transform.rotation.y * DEG2RAD,
            transform.rotation.z * DEG2RAD
        });
        Matrix matTranslation = MatrixTranslate(transform.position.x, 
                                                transform.position.y, 
                                                transform.position.z);
        
        Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
        
        // Render with Raylib
        DrawMesh(mesh->mesh, material->toRaylibMaterial(), matTransform);
    }
}

void ECSBridge::shutdown() {
    world_.reset();
}

ecs::World* ECSBridge::getWorld() {
    return reinterpret_cast<ecs::World*>(world_.get());
}

EntityId ECSBridge::createEntity() {
    return world_->createEntity();
}

void ECSBridge::destroyEntity(EntityId id) {
    world_->destroyEntity(id);
}

bool ECSBridge::entityExists(EntityId id) {
    return world_->entityExists(id);
}

bool ECSBridge::hasComponent(EntityId id, ComponentType type) {
    uint8_t typeId = static_cast<uint8_t>(type);
    auto it = world_->componentPools.find(typeId);
    if (it == world_->componentPools.end()) return false;
    
    // Check based on type
    switch (type) {
        case ComponentType::TRANSFORM:
            return world_->getPool<TransformComponent>()->has(id);
        case ComponentType::MESH:
            return world_->getPool<MeshComponent>()->has(id);
        case ComponentType::MATERIAL:
            return world_->getPool<MaterialComponent>()->has(id);
        case ComponentType::CAMERA:
            return world_->getPool<CameraComponent>()->has(id);
        case ComponentType::LIGHT:
            return world_->getPool<LightComponent>()->has(id);
        case ComponentType::SCRIPT:
            return world_->getPool<ScriptComponent>()->has(id);
        case ComponentType::INPUT_CONTROLLER:
            return world_->getPool<InputControllerComponent>()->has(id);
    }
    return false;
}

// Template specializations for add/get/remove
template<>
void ECSBridge::addComponent<TransformComponent>(EntityId id, const TransformComponent& component) {
    world_->getPool<TransformComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<MeshComponent>(EntityId id, const MeshComponent& component) {
    world_->getPool<MeshComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<MaterialComponent>(EntityId id, const MaterialComponent& component) {
    world_->getPool<MaterialComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<CameraComponent>(EntityId id, const CameraComponent& component) {
    world_->getPool<CameraComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<LightComponent>(EntityId id, const LightComponent& component) {
    world_->getPool<LightComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<ScriptComponent>(EntityId id, const ScriptComponent& component) {
    world_->getPool<ScriptComponent>()->add(id, component);
}

template<>
void ECSBridge::addComponent<InputControllerComponent>(EntityId id, const InputControllerComponent& component) {
    world_->getPool<InputControllerComponent>()->add(id, component);
}

template<>
TransformComponent* ECSBridge::getComponent<TransformComponent>(EntityId id) {
    return world_->getPool<TransformComponent>()->get(id);
}

template<>
MeshComponent* ECSBridge::getComponent<MeshComponent>(EntityId id) {
    return world_->getPool<MeshComponent>()->get(id);
}

template<>
MaterialComponent* ECSBridge::getComponent<MaterialComponent>(EntityId id) {
    return world_->getPool<MaterialComponent>()->get(id);
}

template<>
CameraComponent* ECSBridge::getComponent<CameraComponent>(EntityId id) {
    return world_->getPool<CameraComponent>()->get(id);
}

template<>
LightComponent* ECSBridge::getComponent<LightComponent>(EntityId id) {
    return world_->getPool<LightComponent>()->get(id);
}

template<>
ScriptComponent* ECSBridge::getComponent<ScriptComponent>(EntityId id) {
    return world_->getPool<ScriptComponent>()->get(id);
}

template<>
InputControllerComponent* ECSBridge::getComponent<InputControllerComponent>(EntityId id) {
    return world_->getPool<InputControllerComponent>()->get(id);
}

template<>
void ECSBridge::removeComponent<TransformComponent>(EntityId id) {
    world_->getPool<TransformComponent>()->remove(id);
}

template<>
void ECSBridge::removeComponent<MeshComponent>(EntityId id) {
    world_->getPool<MeshComponent>()->remove(id);
}

template<>
void ECSBridge::removeComponent<MaterialComponent>(EntityId id) {
    world_->getPool<MaterialComponent>()->remove(id);
}

// ============================================================================
// Component Implementation
// ============================================================================

Matrix TransformComponent::getMatrix() const {
    Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix matRotation = MatrixRotateXYZ({
        rotation.x * DEG2RAD,
        rotation.y * DEG2RAD,
        rotation.z * DEG2RAD
    });
    Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);
    
    return MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
}

void MeshComponent::loadCube() {
    if (loaded) unload();
    mesh = GenMeshCube(1.0f, 1.0f, 1.0f);
    loaded = true;
    modelPath = "cube";
}

void MeshComponent::loadSphere() {
    if (loaded) unload();
    mesh = GenMeshSphere(0.5f, 32, 32);
    loaded = true;
    modelPath = "sphere";
}

void MeshComponent::loadPlane() {
    if (loaded) unload();
    mesh = GenMeshPlane(1.0f, 1.0f, 1, 1);
    loaded = true;
    modelPath = "plane";
}

void MeshComponent::loadFromFile(const std::string& path) {
    if (loaded) unload();
    // Would load from file using Raylib's model loading
    // For now, default to cube
    loadCube();
    modelPath = path;
}

void MeshComponent::unload() {
    if (loaded) {
        UnloadMesh(mesh);
        loaded = false;
    }
}

Material MaterialComponent::toRaylibMaterial() const {
    Material mat = LoadMaterialDefault();
    // Note: Raylib's default material uses a shader that supports color
    // We'd need to set the shader uniforms appropriately
    (void)color;  // Color would be passed via shader uniforms
    return mat;
}

Camera3D CameraComponent::toRaylibCamera(const TransformComponent& transform) const {
    // Calculate forward direction from rotation
    float yaw = transform.rotation.y * DEG2RAD;
    float pitch = transform.rotation.x * DEG2RAD;
    
    Vector3 forward = {
        sinf(yaw) * cosf(pitch),
        sinf(pitch),
        cosf(yaw) * cosf(pitch)
    };
    
    Vector3 target = {
        transform.position.x + forward.x,
        transform.position.y + forward.y,
        transform.position.z + forward.z
    };
    
    Camera3D cam = {0};
    cam.position = transform.position;
    cam.target = target;
    cam.up = {0, 1, 0};
    cam.fovy = fov;
    cam.projection = projection;
    
    return cam;
}

} // namespace internal
} // namespace g3d
} // namespace kern
