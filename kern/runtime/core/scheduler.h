/* *
 * kern/runtime/core/scheduler.h - Update Scheduler
 * 
 * Manages the frame/tick loop for modules.
 * Handles variable timestep and fixed timestep updates.
 */

#pragma once

#include <functional>
#include <chrono>
#include <vector>

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// SCHEDULER CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

struct SchedulerConfig {
    // Frame rate limiting
    float targetFPS = 60.0f;
    bool limitFPS = true;
    
    // Fixed timestep
    bool useFixedUpdate = false;
    float fixedTimestep = 1.0f / 60.0f;  // 60Hz
    int maxFixedUpdatesPerFrame = 5;      // Prevent spiral of death
    
    // Profiling
    bool enableProfiling = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// SCHEDULER
// ═══════════════════════════════════════════════════════════════════════════════
//
// Manages the update loop timing.
// Calls registered callbacks at appropriate intervals.

class Scheduler {
public:
    explicit Scheduler(const SchedulerConfig& config = {});
    ~Scheduler() = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UPDATE REGISTRATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Register update callback (variable timestep)
    /// Called every frame
    void onUpdate(std::function<void(float)> callback, int priority = 100);
    
    /// Register fixed update callback
    /// Called at fixed timestep (for physics, etc.)
    void onFixedUpdate(std::function<void(float)> callback, int priority = 100);
    
    /// Register render callback
    /// Called after all updates
    void onRender(std::function<void()> callback, int priority = 100);
    
    /// Register callback for frame begin
    void onFrameBegin(std::function<void(float)> callback);
    
    /// Register callback for frame end
    void onFrameEnd(std::function<void(float)> callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MAIN LOOP
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Run the scheduler loop
    /// Blocks until stop() is called
    void run();
    
    /// Stop the scheduler
    void stop();
    
    /// Check if running
    bool isRunning() const { return running_; }
    
    /// Process single tick
    /// For custom main loops
    void tick();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // TIME QUERY
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get time since scheduler started
    float getTime() const;
    
    /// Get delta time for current frame
    float getDeltaTime() const { return deltaTime_; }
    
    /// Get current FPS
    float getFPS() const { return currentFPS_; }
    
    /// Get frame count
    uint64_t getFrameCount() const { return frameCount_; }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // PROFILING
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get average frame time
    float getAverageFrameTime() const { return avgFrameTime_; }
    
    /// Get average update time
    float getAverageUpdateTime() const { return avgUpdateTime_; }
    
    /// Get average render time
    float getAverageRenderTime() const { return avgRenderTime_; }

private:
    SchedulerConfig config_;
    
    // Timing
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    
    TimePoint startTime_;
    TimePoint lastFrameTime_;
    float deltaTime_ = 0.0f;
    float accumulatedFixedTime_ = 0.0f;
    uint64_t frameCount_ = 0;
    float currentFPS_ = 0.0f;
    
    // Running averages (for profiling)
    float avgFrameTime_ = 0.0f;
    float avgUpdateTime_ = 0.0f;
    float avgRenderTime_ = 0.0f;
    
    // State
    bool running_ = false;
    
    // Callbacks (sorted by priority)
    struct UpdateCallback {
        std::function<void(float)> func;
        int priority;
    };
    std::vector<UpdateCallback> updateCallbacks_;
    std::vector<UpdateCallback> fixedUpdateCallbacks_;
    std::vector<UpdateCallback> renderCallbacks_;
    
    std::function<void(float)> frameBeginCallback_;
    std::function<void(float)> frameEndCallback_;
    
    // Internal
    void sortCallbacks();
    void update(float dt);
    void fixedUpdate(float dt);
    void render();
    void limitFPS(float targetFrameTime);
    
    static float durationToSeconds(Clock::duration dur);
};

} // namespace kern::runtime
