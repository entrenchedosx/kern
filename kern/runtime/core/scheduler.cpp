/* *
 * kern/runtime/core/scheduler.cpp - Scheduler Implementation
 */

#include "scheduler.h"
#include <algorithm>
#include <iostream>

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// SCHEDULER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

Scheduler::Scheduler(const SchedulerConfig& config) : config_(config) {
    startTime_ = Clock::now();
    lastFrameTime_ = startTime_;
}

void Scheduler::onUpdate(std::function<void(float)> callback, int priority) {
    updateCallbacks_.push_back({callback, priority});
    sortCallbacks();
}

void Scheduler::onFixedUpdate(std::function<void(float)> callback, int priority) {
    fixedUpdateCallbacks_.push_back({callback, priority});
    sortCallbacks();
}

void Scheduler::onRender(std::function<void()> callback, int priority) {
    renderCallbacks_.push_back({[callback](float) { callback(); }, priority});
    sortCallbacks();
}

void Scheduler::onFrameBegin(std::function<void(float)> callback) {
    frameBeginCallback_ = callback;
}

void Scheduler::onFrameEnd(std::function<void(float)> callback) {
    frameEndCallback_ = callback;
}

void Scheduler::run() {
    running_ = true;
    
    while (running_) {
        tick();
    }
}

void Scheduler::stop() {
    running_ = false;
}

void Scheduler::tick() {
    // Calculate delta time
    TimePoint currentTime = Clock::now();
    deltaTime_ = durationToSeconds(currentTime - lastFrameTime_);
    lastFrameTime_ = currentTime;
    
    // Limit delta time to prevent spiral of death
    if (deltaTime_ > 0.1f) {  // Max 10fps
        deltaTime_ = 0.1f;
    }
    
    // Frame begin callback
    if (frameBeginCallback_) {
        frameBeginCallback_(deltaTime_);
    }
    
    // Update phase
    update(deltaTime_);
    
    // Fixed update phase
    if (config_.useFixedUpdate) {
        accumulatedFixedTime_ += deltaTime_;
        
        int fixedUpdates = 0;
        while (accumulatedFixedTime_ >= config_.fixedTimestep && 
               fixedUpdates < config_.maxFixedUpdatesPerFrame) {
            fixedUpdate(config_.fixedTimestep);
            accumulatedFixedTime_ -= config_.fixedTimestep;
            fixedUpdates++;
        }
    }
    
    // Render phase
    render();
    
    // Frame end callback
    if (frameEndCallback_) {
        frameEndCallback_(deltaTime_);
    }
    
    // FPS limiting
    if (config_.limitFPS) {
        float targetFrameTime = 1.0f / config_.targetFPS;
        limitFPS(targetFrameTime);
    }
    
    // Update statistics
    frameCount_++;
    
    // Calculate FPS (every 10 frames)
    if (frameCount_ % 10 == 0) {
        currentFPS_ = 1.0f / deltaTime_;
    }
}

float Scheduler::getTime() const {
    return durationToSeconds(Clock::now() - startTime_);
}

void Scheduler::sortCallbacks() {
    // Sort update callbacks by priority (lower = earlier)
    std::stable_sort(updateCallbacks_.begin(), updateCallbacks_.end(),
        [](const UpdateCallback& a, const UpdateCallback& b) {
            return a.priority < b.priority;
        });
    
    std::stable_sort(fixedUpdateCallbacks_.begin(), fixedUpdateCallbacks_.end(),
        [](const UpdateCallback& a, const UpdateCallback& b) {
            return a.priority < b.priority;
        });
    
    std::stable_sort(renderCallbacks_.begin(), renderCallbacks_.end(),
        [](const UpdateCallback& a, const UpdateCallback& b) {
            return a.priority < b.priority;
        });
}

void Scheduler::update(float dt) {
    TimePoint updateStart = Clock::now();
    
    for (const auto& callback : updateCallbacks_) {
        if (callback.func) {
            callback.func(dt);
        }
    }
    
    // Update profiling
    if (config_.enableProfiling) {
        float updateTime = durationToSeconds(Clock::now() - updateStart);
        avgUpdateTime_ = avgUpdateTime_ * 0.9f + updateTime * 0.1f;
    }
}

void Scheduler::fixedUpdate(float dt) {
    for (const auto& callback : fixedUpdateCallbacks_) {
        if (callback.func) {
            callback.func(dt);
        }
    }
}

void Scheduler::render() {
    TimePoint renderStart = Clock::now();
    
    for (const auto& callback : renderCallbacks_) {
        if (callback.func) {
            callback.func(0.0f);  // dt not used for render
        }
    }
    
    // Update profiling
    if (config_.enableProfiling) {
        float renderTime = durationToSeconds(Clock::now() - renderStart);
        avgRenderTime_ = avgRenderTime_ * 0.9f + renderTime * 0.1f;
    }
}

void Scheduler::limitFPS(float targetFrameTime) {
    TimePoint frameEnd = Clock::now();
    float frameTime = durationToSeconds(frameEnd - lastFrameTime_);
    
    if (frameTime < targetFrameTime) {
        // Sleep for remaining time
        float sleepTime = targetFrameTime - frameTime;
        // Note: Should use platform-specific sleep
        // For now, busy wait
        TimePoint sleepEnd = lastFrameTime_ + std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<float>(targetFrameTime));
        while (Clock::now() < sleepEnd) {
            // Busy wait
        }
    }
}

float Scheduler::durationToSeconds(Clock::duration dur) {
    return std::chrono::duration_cast<std::chrono::duration<float>>(dur).count();
}

} // namespace kern::runtime
