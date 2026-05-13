#ifndef KERN_RUNTIME_SERVICES_HPP
#define KERN_RUNTIME_SERVICES_HPP

#include "system/event_bus.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace kern {

struct RenderSurfaceState {
    int width = 0;
    int height = 0;
    bool transparent = false;
    std::string title;
    uint64_t frameCounter = 0;
    std::vector<uint32_t> front;
    std::vector<uint32_t> back;
};

struct RuntimeServices {
    EventBus bus;
    std::mutex renderMutex;
    RenderSurfaceState surface;
};

} // namespace kern

#endif

