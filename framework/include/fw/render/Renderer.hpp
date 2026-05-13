#pragma once

#include "fw/event/Event.hpp"
#include "fw/render/RenderTree.hpp"

#include <vector>

namespace fw::render {

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool initialize(int width, int height, const char* title) = 0;
    virtual void beginFrame(const Color& clear) = 0;
    virtual void draw(const RenderObject& obj) = 0;
    virtual void endFrame() = 0;
    virtual std::vector<event::InputEvent> pollEvents() = 0;
    virtual bool shouldClose() const = 0;
};

} // namespace fw::render

