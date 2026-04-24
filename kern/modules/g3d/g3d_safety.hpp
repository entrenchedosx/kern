/* *
 * kern/modules/g3d/g3d_safety.hpp - Concurrency Safety for Parallel ECS
 * 
 * Critical safety mechanisms:
 * 1. Write conflict detection (no two systems write same component)
 * 2. Generation-safe handles (detect dangling pointers)
 * 3. Structural barriers (no mutation during parallel phase)
 * 4. Frame-phase execution pipeline
 * 
 * All safety checks are DEBUG-ONLY in release builds for zero overhead.
 */
#pragma once

#include "g3d_jobs.hpp"
#include <unordered_set>
#include <cassert>

// Debug-only safety checks
#ifndef G3D_RELEASE_BUILD
    #define G3D_SAFETY_CHECK(x) do { if (!(x)) { \
        std::cerr << "ECS SAFETY VIOLATION: " << #x << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::abort(); \
    } } while(0)
#else
    #define G3D_SAFETY_CHECK(x) ((void)0)
#endif

namespace kern {
namespace g3d {
namespace safety {

// ============================================================================
// 1. Write Conflict Detection
// ============================================================================

class WriteConflictDetector {
    std::unordered_map<std::string, std::string> componentWriters_;
    std::unordered_map<std::string, std::vector<std::string>> systemReads_;
    std::unordered_map<std::string, std::vector<std::string>> systemWrites_;
    
public:
    void registerSystem(const std::string& systemName,
                       const std::vector<std::string>& reads,
                       const std::vector<std::string>& writes) {
        systemReads_[systemName] = reads;
        systemWrites_[systemName] = writes;
        
        // Check for write-write conflicts
        for (const auto& component : writes) {
            auto it = componentWriters_.find(component);
            if (it != componentWriters_.end()) {
                // Already has a writer!
                std::cerr << "WRITE-WRITE CONFLICT DETECTED:\n";
                std::cerr << "  Component: " << component << "\n";
                std::cerr << "  Writer 1:  " << it->second << "\n";
                std::cerr << "  Writer 2:  " << systemName << "\n";
                std::cerr << "  These systems CANNOT run in parallel.\n";
                std::cerr << "  Solution: Add dependency or merge systems.\n";
                G3D_SAFETY_CHECK(false && "Write-write conflict");
            }
            componentWriters_[component] = systemName;
        }
    }
    
    // Validate a full frame graph before execution
    bool validateGraph(const JobGraph& graph) {
        // Clear state
        componentWriters_.clear();
        
        // Check all systems
        for (const auto& [name, job] : graph) {
            // Would need to extract read/write sets from job
            // For now, rely on registration-time checks
            (void)name;
            (void)job;
        }
        
        return true;
    }
    
    // Get systems that must be serialized
    std::vector<std::pair<std::string, std::string>> getConflictingPairs() {
        std::vector<std::pair<std::string, std::string>> conflicts;
        // Detect pairs that both write same component
        for (const auto& [sys1, writes1] : systemWrites_) {
            for (const auto& comp : writes1) {
                for (const auto& [sys2, writes2] : systemWrites_) {
                    if (sys1 >= sys2) continue;  // Avoid duplicates
                    if (std::find(writes2.begin(), writes2.end(), comp) != writes2.end()) {
                        conflicts.push_back({sys1, sys2});
                    }
                }
            }
        }
        return conflicts;
    }
};

// ============================================================================
// 2. Generation-Safe EntityHandle
// ============================================================================

// Generation counter for entity validation
using Generation = uint32_t;

struct SafeEntityHandle {
    internal::TransformComponent* transform_ = nullptr;
    internal::MeshComponent* mesh_ = nullptr;
    internal::MaterialComponent* material_ = nullptr;
    internal::EntityId id_ = 0;
    Generation generation_ = 0;  // Incremented on structural changes
    
    // Check if handle is still valid
    bool isValid(Generation currentGen) const {
        return generation_ == currentGen && transform_ != nullptr;
    }
    
    // Safe access (validates before dereference in debug)
    Vec3 getPosition(Generation currentGen) const {
        G3D_SAFETY_CHECK(isValid(currentGen) && "EntityHandle invalidated by structural change");
        return transform_->position;
    }
    
    void setPosition(const Vec3& pos, Generation currentGen) {
        G3D_SAFETY_CHECK(isValid(currentGen));
        transform_->position = pos;
    }
    
    // Fast access (no validation - use when certain)
    Vec3 getPositionFast() const {
        return transform_->position;
    }
    
    void invalidate() {
        transform_ = nullptr;
        mesh_ = nullptr;
        material_ = nullptr;
        generation_ = 0;
    }
};

// Global generation counter for the frame
class GenerationTracker {
    Generation currentGeneration_ = 1;
    std::unordered_map<internal::EntityId, Generation> entityGenerations_;
    
public:
    Generation current() const { return currentGeneration_; }
    
    void bumpGeneration() {
        currentGeneration_++;
        G3D_SAFETY_CHECK(currentGeneration_ > 0 && "Generation overflow!");
    }
    
    Generation getEntityGeneration(internal::EntityId id) const {
        auto it = entityGenerations_.find(id);
        return (it != entityGenerations_.end()) ? it->second : 0;
    }
    
    void setEntityGeneration(internal::EntityId id, Generation gen) {
        entityGenerations_[id] = gen;
    }
    
    void removeEntity(internal::EntityId id) {
        entityGenerations_.erase(id);
    }
};

// ============================================================================
// 3. Structural Barrier
// ============================================================================

enum class FramePhase {
    IDLE,               // No frame in progress
    PARALLEL_SYSTEMS,   // Running parallel systems (NO structural changes)
    STRUCTURAL_CHANGES, // Applying deferred entity/component mutations
    COMPLETE            // Frame done
};

class StructuralBarrier {
    FramePhase currentPhase_ = FramePhase::IDLE;
    std::atomic<bool> structuralMutationPending_{false};
    
    // Deferred operations
    struct DeferredOp {
        enum Type { CREATE_ENTITY, DESTROY_ENTITY, ADD_COMPONENT, REMOVE_COMPONENT };
        Type type;
        internal::EntityId entity;
        std::function<void()> execute;
    };
    std::vector<DeferredOp> deferredOps_;
    std::mutex deferredMutex_;
    
public:
    void beginFrame() {
        G3D_SAFETY_CHECK(currentPhase_ == FramePhase::IDLE && "Must end previous frame");
        currentPhase_ = FramePhase::PARALLEL_SYSTEMS;
        deferredOps_.clear();
    }
    
    void endFrame() {
        G3D_SAFETY_CHECK(currentPhase_ == FramePhase::COMPLETE && "Must complete frame first");
        currentPhase_ = FramePhase::IDLE;
    }
    
    // Called before parallel execution
    void enterParallelPhase() {
        currentPhase_ = FramePhase::PARALLEL_SYSTEMS;
    }
    
    // Called after parallel systems complete
    void enterStructuralPhase() {
        currentPhase_ = FramePhase::STRUCTURAL_CHANGES;
        
        // Execute all deferred structural changes
        std::vector<DeferredOp> ops;
        {
            std::lock_guard<std::mutex> lock(deferredMutex_);
            ops = std::move(deferredOps_);
            deferredOps_.clear();
        }
        
        for (auto& op : ops) {
            op.execute();
        }
        
        currentPhase_ = FramePhase::COMPLETE;
    }
    
    // Try to perform structural operation (fails if in parallel phase)
    template<typename F>
    bool tryStructuralChange(F&& func) {
        if (currentPhase_ == FramePhase::PARALLEL_SYSTEMS) {
            // Defer to end of frame
            std::lock_guard<std::mutex> lock(deferredMutex_);
            deferredOps_.push_back({DeferredOp::CREATE_ENTITY, 0, std::forward<F>(func)});
            return false;  // Deferred, not immediate
        }
        
        // Safe to execute immediately
        func();
        return true;
    }
    
    // Force immediate structural change (only call from main thread, never parallel)
    void forceStructuralChange(const std::function<void()>& func) {
        G3D_SAFETY_CHECK(currentPhase_ != FramePhase::PARALLEL_SYSTEMS 
                        && "Cannot structurally mutate during parallel phase!");
        func();
    }
    
    FramePhase currentPhase() const { return currentPhase_; }
    bool isParallelPhase() const { return currentPhase_ == FramePhase::PARALLEL_SYSTEMS; }
};

// ============================================================================
// 4. Frame-Phase Execution Pipeline (Unity-style)
// ============================================================================

class FramePipeline {
    StructuralBarrier barrier_;
    GenerationTracker generation_;
    WriteConflictDetector conflictDetector_;
    
    // Phase timing
    double parallelTimeMs_ = 0;
    double structuralTimeMs_ = 0;
    
public:
    struct Config {
        bool validateWriteConflicts = true;
        bool trackGenerations = true;
        bool enableParallelism = true;
    };
    
    Config config;
    
    void executeFrame(float dt, 
                     const std::vector<std::function<void(float)>>& systems) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        
        // Phase 1: Begin frame
        barrier_.beginFrame();
        generation_.bumpGeneration();
        
        // Phase 2: Parallel systems execution
        auto parallelStart = std::chrono::high_resolution_clock::now();
        
        if (config.enableParallelism && systems.size() > 1) {
            executeParallel(dt, systems);
        } else {
            executeSequential(dt, systems);
        }
        
        auto parallelEnd = std::chrono::high_resolution_clock::now();
        parallelTimeMs_ = std::chrono::duration<double, std::milli>(parallelEnd - parallelStart).count();
        
        // Phase 3: Structural changes
        auto structStart = std::chrono::high_resolution_clock::now();
        barrier_.enterStructuralPhase();
        auto structEnd = std::chrono::high_resolution_clock::now();
        structuralTimeMs_ = std::chrono::duration<double, std::milli>(structEnd - structStart).count();
        
        // Phase 4: End frame
        barrier_.endFrame();
        
        auto frameEnd = std::chrono::high_resolution_clock::now();
        double totalTime = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        
        // Report
        if (totalTime > 16.0) {  // Missed 60fps
            std::cout << "Frame time: " << totalTime << "ms (parallel: " 
                      << parallelTimeMs_ << "ms, structural: " << structuralTimeMs_ << "ms)\n";
        }
    }
    
    // Safe entity creation (works from any phase, may be deferred)
    template<typename... Args>
    SafeEntityHandle createEntity(Args&&... args) {
        SafeEntityHandle handle;
        
        bool immediate = barrier_.tryStructuralChange([&]() {
            // Actually create entity
            internal::EntityId id = internal::ECSBridge::instance().createEntity();
            
            // Add components
            handle.id_ = id;
            handle.generation_ = generation_.current();
            
            // ... component creation ...
            
            generation_.setEntityGeneration(id, handle.generation_);
        });
        
        if (!immediate) {
            // Entity creation deferred to end of frame
            // Return invalid handle (will be valid after structural phase)
            handle.generation_ = generation_.current();  // Will match after bump
        }
        
        return handle;
    }
    
    // Safe entity destruction
    void destroyEntity(internal::EntityId id) {
        barrier_.tryStructuralChange([this, id]() {
            internal::ECSBridge::instance().destroyEntity(id);
            generation_.removeEntity(id);
        });
    }
    
    Generation currentGeneration() const {
        return generation_.current();
    }
    
    bool isValidHandle(const SafeEntityHandle& handle) const {
        return handle.isValid(generation_.current());
    }
    
private:
    void executeParallel(float dt, const std::vector<std::function<void(float)>>& systems) {
        // Submit all systems to thread pool
        // (Would need actual thread pool integration)
        for (const auto& system : systems) {
            system(dt);
        }
    }
    
    void executeSequential(float dt, const std::vector<std::function<void(float)>>& systems) {
        for (const auto& system : systems) {
            system(dt);
        }
    }
};

// ============================================================================
// 5. False Sharing Prevention
// ============================================================================

// Align components to cache line boundaries to prevent false sharing
template<typename T>
struct alignas(64) CacheAligned {  // 64 bytes = typical cache line
    T value;
};

// Component storage with padding to prevent false sharing between threads
class PaddedComponentStorage {
    static constexpr size_t kCacheLineSize = 64;
    
    std::vector<uint8_t> data_;
    size_t componentSize_;
    size_t paddedSize_;
    
public:
    explicit PaddedComponentStorage(size_t componentSize) 
        : componentSize_(componentSize) {
        // Round up to cache line size
        paddedSize_ = ((componentSize_ + kCacheLineSize - 1) / kCacheLineSize) * kCacheLineSize;
    }
    
    void* get(size_t index) {
        return data_.data() + (index * paddedSize_);
    }
    
    void resize(size_t count) {
        data_.resize(count * paddedSize_);
    }
};

// Entity batch aligned to cache lines
class CacheAlignedBatch {
    static constexpr size_t kBatchSize = 64;  // Cache line sized batches
    
    std::vector<std::vector<Entity*>> batches_;
    
public:
    explicit CacheAlignedBatch(const std::vector<Entity*>& entities) {
        // Split entities into cache-aligned batches
        for (size_t i = 0; i < entities.size(); i += kBatchSize) {
            size_t end = std::min(i + kBatchSize, entities.size());
            batches_.emplace_back(entities.begin() + i, entities.begin() + end);
        }
    }
    
    template<typename Func>
    void processParallel(jobs::ThreadPool& pool, Func&& func) {
        std::atomic<size_t> completed{0};
        
        for (size_t b = 0; b < batches_.size(); b++) {
            pool.enqueue([this, b, &func, &completed]() {
                for (Entity* e : batches_[b]) {
                    func(e);
                }
                completed++;
            });
        }
        
        while (completed.load() < batches_.size()) {
            std::this_thread::yield();
        }
    }
};

// ============================================================================
// Safety Macros for Development
// ============================================================================

// Use this to mark code that should only run in specific phases
#define G3D_PARALLEL_PHASE_ONLY() \
    G3D_SAFETY_CHECK(safety::currentBarrier().isParallelPhase() && \
                     "Code must run in parallel phase")

#define G3D_STRUCTURAL_PHASE_ONLY() \
    G3D_SAFETY_CHECK(!safety::currentBarrier().isParallelPhase() && \
                     "Structural changes only in structural phase")

// Global barrier access (singleton)
inline StructuralBarrier& currentBarrier() {
    static StructuralBarrier barrier;
    return barrier;
}

} // namespace safety
} // namespace g3d
} // namespace kern
