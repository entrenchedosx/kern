#include "system/event_bus.hpp"

namespace kern {

void EventBus::push(SystemEvent ev) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(ev));
    }
    cv_.notify_one();
}

std::optional<SystemEvent> EventBus::tryPop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    SystemEvent ev = std::move(queue_.front());
    queue_.pop();
    return ev;
}

size_t EventBus::drainTo(std::queue<SystemEvent>& out, size_t maxCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    while (!queue_.empty() && count < maxCount) {
        out.push(std::move(queue_.front()));
        queue_.pop();
        ++count;
    }
    return count;
}

} // namespace kern

