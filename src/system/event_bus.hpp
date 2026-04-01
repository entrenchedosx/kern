#ifndef KERN_SYSTEM_EVENT_BUS_HPP
#define KERN_SYSTEM_EVENT_BUS_HPP

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

namespace kern {

struct SystemEvent {
    std::string type;
    int64_t key = 0;
    int64_t x = 0;
    int64_t y = 0;
    int64_t button = 0;
};

class EventBus {
public:
    void push(SystemEvent ev);
    std::optional<SystemEvent> tryPop();
    size_t drainTo(std::queue<SystemEvent>& out, size_t maxCount);

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<SystemEvent> queue_;
};

} // namespace kern

#endif

