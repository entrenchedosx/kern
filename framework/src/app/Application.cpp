#include "fw/app/Application.hpp"

#include <chrono>
#include <future>
#include <thread>

namespace fw::app {

Application::Application(std::unique_ptr<render::Renderer> renderer)
    : renderer_(std::move(renderer)) {}

bool Application::initialize(int width, int height, const std::string& title) {
    width_ = width;
    height_ = height;
    if (!renderer_) return false;
    return renderer_->initialize(width_, height_, title.c_str());
}

bool Application::loadRemoteDocument(const std::string& url) {
    auto fut = resources_.loadTextAsync(url);
    if (fut.wait_for(std::chrono::milliseconds(2500)) != std::future_status::ready) {
        return false;
    }
    const std::string source = fut.get();
    if (source.empty()) return false;
    document_ = parser_.parse(source);
    rebuild();
    return true;
}

bool Application::loadStyles(const std::string& styleSource) {
    styleSheet_ = style::StyleSheet::parse(styleSource);
    rebuild();
    return true;
}

void Application::rebuild() {
    auto styles = styleResolver_.resolve(document_, styleSheet_);
    layoutTree_ = layoutEngine_.compute(document_, styles, static_cast<float>(width_), static_cast<float>(height_));
    renderTree_ = renderTreeBuilder_.build(layoutTree_);
}

void Application::run() {
    using namespace std::chrono_literals;
    while (!renderer_->shouldClose()) {
        auto events = renderer_->pollEvents();
        bool dirty = false;
        for (auto& ev : events) {
            eventDispatcher_.dispatch(layoutTree_, ev);
            scriptContext_.invoke("on_event", document_, ev);
            dirty = true;
            if (ev.type == event::EventType::Quit) return;
        }
        if (dirty) rebuild();

        renderer_->beginFrame(render::Color{18, 18, 22, 255});
        for (const auto& obj : renderTree_.objects()) renderer_->draw(obj);
        renderer_->endFrame();
        std::this_thread::sleep_for(1ms);
    }
}

} // namespace fw::app

