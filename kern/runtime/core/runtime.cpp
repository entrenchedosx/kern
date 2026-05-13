/* *
 * kern/runtime/core/runtime.cpp - Kern Runtime Implementation
 */

#include "runtime.h"
#include <iostream>

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// KERN RUNTIME IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

KernRuntime::KernRuntime(const RuntimeConfig& config) 
    : config_(config), scheduler_(SchedulerConfig{
        .targetFPS = config.targetFrameRate,
        .limitFPS = true,
        .useFixedUpdate = config.useFixedTimestep,
        .fixedTimestep = config.fixedTimestep,
        .maxFixedUpdatesPerFrame = 5,
        .enableProfiling = config.enableProfiling
    }) {
    
    // Create VM instance
    vm_ = std::make_unique<VM>();
    
    // Initialize
    initialize();
}

KernRuntime::~KernRuntime() {
    if (initialized_) {
        shutdown();
    }
}

void KernRuntime::initialize() {
    if (initialized_) return;
    
    // Initialize VM
    // vm_->initialize(config_.vmStackSize, config_.vmHeapSize);
    
    // Register core bindings
    bindings_.registerFunction("print", [](VM& vm) {
        // Extract string argument
        // Print to console
        std::cout << "Kern print called\n";
    });
    
    // Set up scheduler callbacks
    scheduler_.onFrameBegin([this](float dt) {
        if (onFrameBegin_) {
            onFrameBegin_(dt);
        }
    });
    
    scheduler_.onFrameEnd([this](float dt) {
        if (onFrameEnd_) {
            onFrameEnd_(dt);
        }
    });
    
    scheduler_.onUpdate([this](float dt) {
        modules_.updateAll(dt);
    });
    
    scheduler_.onRender([this]() {
        modules_.renderAll();
    });
    
    initialized_ = true;
}

void KernRuntime::shutdown() {
    if (!initialized_) return;
    
    // Unload all modules
    modules_.unloadAll();
    
    // Shutdown VM
    if (vm_) {
        // vm_->shutdown();
        vm_.reset();
    }
    
    initialized_ = false;
}

VM::Result KernRuntime::execute(const CodeObject& code) {
    if (!initialized_) {
        return VM::Result::Error("Runtime not initialized");
    }
    
    return vm_->execute(code);
}

VM::Result KernRuntime::executeScript(const std::string& source) {
    if (!initialized_) {
        return VM::Result::Error("Runtime not initialized");
    }
    
    // TODO: Compile source to bytecode
    // For now, just return success
    return VM::Result::Success();
}

void KernRuntime::runREPL() {
    std::cout << "Kern REPL\n";
    std::cout << "Type 'exit' to quit\n\n";
    
    std::string line;
    while (running_) {
        std::cout << "kern> ";
        std::getline(std::cin, line);
        
        if (line == "exit") {
            break;
        }
        
        // Execute line
        executeScript(line);
    }
}

void KernRuntime::run() {
    if (!initialized_) {
        std::cerr << "Runtime not initialized\n";
        return;
    }
    
    running_ = true;
    scheduler_.run();
    running_ = false;
}

void KernRuntime::stop() {
    running_ = false;
    scheduler_.stop();
}

void KernRuntime::tick(float deltaTime) {
    if (!initialized_) return;
    
    // Update scheduler
    scheduler_.tick();
}

void KernRuntime::onFrameBegin(std::function<void(float)> callback) {
    onFrameBegin_ = callback;
}

void KernRuntime::onFrameEnd(std::function<void(float)> callback) {
    onFrameEnd_ = callback;
}

} // namespace kern::runtime
