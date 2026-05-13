#pragma once

#include "fw/render/Renderer.hpp"

namespace fw::render {

class NullRenderer final : public Renderer {
public:
    bool initialize(int width, int height, const char* title) override;
    void beginFrame(const Color& clear) override;
    void draw(const RenderObject& obj) override;
    void endFrame() override;
    std::vector<event::InputEvent> pollEvents() override;
    bool shouldClose() const override;

private:
    bool closed_{false};
    int frames_{0};
};

} // namespace fw::render

