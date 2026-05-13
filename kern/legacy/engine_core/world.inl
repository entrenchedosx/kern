/* *
 * kern/engine/core/world.inl - Template Implementations
 * 
 * Week 2 Production Implementation
 */

namespace kern::engine {

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT STORAGE ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

template<typename T>
TypedComponentStorage<T>* World::getOrCreateStorage() {
    std::type_index typeId = std::type_index(typeid(T));
    
    auto it = componentStorages_.find(typeId);
    if (it != componentStorages_.end()) {
        return static_cast<TypedComponentStorage<T>*>(it->second.get());
    }
    
    // Create new storage
    auto storage = std::make_unique<TypedComponentStorage<T>>();
    TypedComponentStorage<T>* ptr = storage.get();
    componentStorages_[typeId] = std::move(storage);
    
    return ptr;
}

template<typename T>
const TypedComponentStorage<T>* World::getStorage() const {
    std::type_index typeId = std::type_index(typeid(T));
    
    auto it = componentStorages_.find(typeId);
    if (it != componentStorages_.end()) {
        return static_cast<const TypedComponentStorage<T>*>(it->second.get());
    }
    
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

template<typename T, typename... Args>
T* World::addComponent(EntityId entity, Args&&... args) {
    KERN_CONTRACT_VALID_ENTITY(entities_, entity);
    
    auto* storage = getOrCreateStorage<T>();
    return storage->add(entity, std::forward<Args>(args)...);
}

template<typename T>
void World::removeComponent(EntityId entity) {
    KERN_CONTRACT_VALID_ENTITY(entities_, entity);
    
    auto* storage = getStorage<T>();
    if (storage) {
        storage->remove(entity);
    }
}

template<typename T>
T* World::getComponent(EntityId entity) {
    KERN_CONTRACT_VALID_ENTITY(entities_, entity);
    
    auto* storage = getStorage<T>();
    if (storage) {
        return storage->get(entity);
    }
    return nullptr;
}

template<typename T>
const T* World::getComponent(EntityId entity) const {
    return const_cast<World*>(this)->getComponent<T>(entity);
}

template<typename T>
bool World::hasComponent(EntityId entity) const {
    KERN_CONTRACT_VALID_ENTITY(entities_, entity);
    
    auto* storage = getStorage<T>();
    if (storage) {
        return storage->has(entity);
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT ITERATION
// ═══════════════════════════════════════════════════════════════════════════════

template<typename T>
typename ComponentStorage<T>::Range World::query() {
    auto* storage = getOrCreateStorage<T>();
    return storage->getStorage().all();
}

template<typename T>
typename ComponentStorage<T>::Range World::query() const {
    auto* storage = getStorage<T>();
    if (storage) {
        return storage->getStorage().all();
    }
    // Return empty range
    static ComponentStorage<T> empty;
    return empty.all();
}

template<typename T, typename Func>
void World::forEach(Func&& func) {
    static_assert(std::is_invocable_v<Func, EntityId, T&>,
                  "Func must be callable with (EntityId, T&)");
    
    auto* storage = getStorage<T>();
    if (storage) {
        storage->getStorage().forEach(std::forward<Func>(func));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// MULTI-COMPONENT QUERY
// ═══════════════════════════════════════════════════════════════════════════════

template<typename First, typename... Rest>
std::vector<EntityId> World::queryEntities() {
    std::vector<EntityId> result;
    
    // Get storage for first component type
    auto* firstStorage = getStorage<First>();
    if (!firstStorage) {
        return result;
    }
    
    // Iterate all entities with First component
    firstStorage->getStorage().forEach([this, &result](EntityId entity, First&) {
        // Check if entity has all other component types
        if ((hasComponent<Rest>(entity) && ...)) {
            result.push_back(entity);
        }
    });
    
    return result;
}

// Single component specialization
template<>
inline std::vector<EntityId> World::queryEntities<Transform>() {
    std::vector<EntityId> result;
    
    auto* storage = getStorage<Transform>();
    if (storage) {
        storage->getStorage().forEach([&result](EntityId entity, Transform&) {
            result.push_back(entity);
        });
    }
    
    return result;
}

} // namespace kern::engine
