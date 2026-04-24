/* *
 * kern/modules/g3d/g3d_jobs.hpp - Minimal Job System for Parallel ECS
 * 
 * Design goals:
 * - Keep zero-overhead EntityHandle API unchanged
 * - Add job-based parallelism for systems
 * - No per-entity locking (batch processing)
 * - Simple dependency graph
 * 
 * This enables parallel ECS without breaking the cached pointer invariant.
 */
#pragma once

#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <string>

namespace kern {
namespace g3d {
namespace jobs {

// ============================================================================
// Job System Core
// ============================================================================

using JobFunc = std::function<void()>;

struct Job {
    std::string name;
    JobFunc func;
    std::vector<std::string> dependencies;
    std::atomic<bool> completed{false};
    
    Job(const std::string& n, JobFunc f) : name(n), func(f) {}
};

class JobGraph {
    std::unordered_map<std::string, std::unique_ptr<Job>> jobs_;
    std::unordered_map<std::string, std::vector<std::string>> dependents_;
    
public:
    void addJob(const std::string& name, JobFunc func, 
                const std::vector<std::string>& deps = {}) {
        auto job = std::make_unique<Job>(name, func);
        job->dependencies = deps;
        jobs_[name] = std::move(job);
        
        // Build reverse dependency map
        for (const auto& dep : deps) {
            dependents_[dep].push_back(name);
        }
    }
    
    Job* getJob(const std::string& name) {
        auto it = jobs_.find(name);
        return (it != jobs_.end()) ? it->second.get() : nullptr;
    }
    
    bool hasJob(const std::string& name) const {
        return jobs_.find(name) != jobs_.end();
    }
    
    // Get jobs ready to run (all deps completed)
    std::vector<Job*> getReadyJobs() const {
        std::vector<Job*> ready;
        for (const auto& [name, job] : jobs_) {
            if (job->completed.load()) continue;
            
            bool depsComplete = true;
            for (const auto& dep : job->dependencies) {
                auto* depJob = const_cast<JobGraph*>(this)->getJob(dep);
                if (!depJob || !depJob->completed.load()) {
                    depsComplete = false;
                    break;
                }
            }
            
            if (depsComplete) {
                ready.push_back(job.get());
            }
        }
        return ready;
    }
    
    void reset() {
        for (auto& [name, job] : jobs_) {
            job->completed.store(false);
        }
    }
    
    bool allCompleted() const {
        for (const auto& [name, job] : jobs_) {
            if (!job->completed.load()) return false;
        }
        return true;
    }
};

// ============================================================================
// Thread Pool (Fixed Size)
// ============================================================================

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<JobFunc> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    std::atomic<uint32_t> activeTasks_{0};
    
public:
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    JobFunc task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] {
                            return stop_.load() || !tasks_.empty();
                        });
                        
                        if (stop_.load() && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                        activeTasks_++;
                    }
                    
                    task();
                    activeTasks_--;
                }
            });
        }
    }
    
    ~ThreadPool() {
        stop_.store(true);
        condition_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
    
    void enqueue(JobFunc task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            tasks_.push(std::move(task));
        }
        condition_.notify_one();
    }
    
    bool isIdle() const {
        return activeTasks_.load() == 0;
    }
    
    size_t size() const {
        return workers_.size();
    }
};

// ============================================================================
// Parallel ECS System Scheduler
// ============================================================================

class ParallelScheduler {
    JobGraph graph_;
    ThreadPool pool_;
    std::mutex completionMutex_;
    std::condition_variable completionCondition_;
    std::atomic<uint32_t> pendingJobs_{0};
    
public:
    explicit ParallelScheduler(size_t numThreads = 0) 
        : pool_(numThreads > 0 ? numThreads : std::thread::hardware_concurrency()) {
    }
    
    // Register an ECS system as a job
    void registerSystem(const std::string& name, 
                       std::function<void(float)> systemFunc,
                       const std::vector<std::string>& reads = {},
                       const std::vector<std::string>& writes = {}) {
        // Build dependencies from read/write sets
        std::vector<std::string> deps;
        
        // If system B writes what system A reads, B depends on A
        // This is simplified - real ECS uses component type dependencies
        
        graph_.addJob(name, [this, systemFunc]() {
            // Note: dt would be passed differently in real implementation
            systemFunc(0.016f);  // Placeholder dt
        }, deps);
    }
    
    // Execute all systems for a frame
    void executeFrame(float dt) {
        graph_.reset();
        pendingJobs_.store(0);
        
        // Simple work stealing: submit ready jobs until done
        while (!graph_.allCompleted()) {
            auto ready = graph_.getReadyJobs();
            
            if (ready.empty()) {
                // Deadlock or waiting - should not happen with proper graph
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            
            // Submit all ready jobs
            for (auto* job : ready) {
                if (job->completed.load()) continue;
                
                pendingJobs_++;
                pool_.enqueue([this, job, dt]() {
                    job->func();
                    job->completed.store(true);
                    pendingJobs_--;
                    completionCondition_.notify_one();
                });
            }
        }
        
        // Wait for all jobs
        std::unique_lock<std::mutex> lock(completionMutex_);
        completionCondition_.wait(lock, [this]() {
            return pendingJobs_.load() == 0;
        });
    }
    
    // Single-threaded fallback (for debugging or simple scenes)
    void executeSequential(float dt) {
        // Simple topological execution
        while (!graph_.allCompleted()) {
            auto ready = graph_.getReadyJobs();
            for (auto* job : ready) {
                if (!job->completed.load()) {
                    job->func();
                    job->completed.store(true);
                }
            }
        }
    }
    
    size_t getThreadCount() const {
        return pool_.size();
    }
};

// ============================================================================
// Batch Component Processing (Cache-Friendly)
// ============================================================================

template<typename Component>
class ComponentBatch {
    std::vector<Component*> components_;
    
public:
    void add(Component* c) {
        components_.push_back(c);
    }
    
    // Process in cache-friendly batches
    template<typename Func>
    void forEachParallel(ThreadPool& pool, Func&& func) {
        const size_t batchSize = 64;  // Cache line friendly
        const size_t numBatches = (components_.size() + batchSize - 1) / batchSize;
        
        std::atomic<size_t> completedBatches{0};
        
        for (size_t b = 0; b < numBatches; b++) {
            pool.enqueue([this, b, batchSize, &func, &completedBatches]() {
                size_t start = b * batchSize;
                size_t end = std::min(start + batchSize, components_.size());
                
                for (size_t i = start; i < end; i++) {
                    func(components_[i]);
                }
                
                completedBatches++;
            });
        }
        
        // Wait for completion
        while (completedBatches.load() < numBatches) {
            std::this_thread::yield();
        }
    }
    
    // Sequential fallback
    template<typename Func>
    void forEachSequential(Func&& func) {
        for (auto* c : components_) {
            func(c);
        }
    }
};

// ============================================================================
// EntityHandle Safety in Parallel Context
// ============================================================================

// Critical: EntityHandle pointers remain valid, but CONCURRENT ACCESS is unsafe
// Solution: Batch processing - no per-entity locks, process entities in isolation

class ParallelEntityProcessor {
    ThreadPool& pool_;
    
public:
    explicit ParallelEntityProcessor(ThreadPool& pool) : pool_(pool) {}
    
    // Process entities in parallel - NO individual locks
    // Safe because: each entity processed by exactly one thread
    template<typename Func>
    void processEntities(const std::vector<Entity*>& entities, Func&& func) {
        const size_t batchSize = 256;
        const size_t numBatches = (entities.size() + batchSize - 1) / batchSize;
        
        std::atomic<size_t> completed{0};
        
        for (size_t b = 0; b < numBatches; b++) {
            pool_.enqueue([this, b, batchSize, &entities, &func, &completed]() {
                size_t start = b * batchSize;
                size_t end = std::min(start + batchSize, entities.size());
                
                for (size_t i = start; i < end; i++) {
                    // Safe: entity is only accessed by this thread
                    func(entities[i]);
                }
                
                completed++;
            });
        }
        
        while (completed.load() < numBatches) {
            std::this_thread::yield();
        }
    }
};

// ============================================================================
// Performance Metrics for Parallel Execution
// ============================================================================

struct ParallelStats {
    std::atomic<uint64_t> jobsExecuted{0};
    std::atomic<uint64_t> totalJobTimeUs{0};
    std::atomic<uint64_t> parallelFrames{0};
    std::atomic<uint64_t> sequentialFrames{0};
    
    void recordJob(uint64_t timeUs) {
        jobsExecuted++;
        totalJobTimeUs += timeUs;
    }
    
    void print() const {
        uint64_t jobs = jobsExecuted.load();
        uint64_t time = totalJobTimeUs.load();
        
        std::cout << "\n=== Parallel ECS Stats ===\n";
        std::cout << "Jobs executed: " << jobs << "\n";
        if (jobs > 0) {
            std::cout << "Avg job time: " << (time / jobs) << " μs\n";
        }
        std::cout << "Parallel frames: " << parallelFrames.load() << "\n";
        std::cout << "Sequential frames: " << sequentialFrames.load() << "\n";
    }
};

inline ParallelStats& parallelStats() {
    static ParallelStats stats;
    return stats;
}

} // namespace jobs
} // namespace g3d
} // namespace kern
