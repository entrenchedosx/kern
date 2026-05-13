#include "fw/render/NullRenderer.hpp"

namespace fw::render {

bool NullRenderer::initialize(int /* width*/, int /*height*/, const char* /*title*/) {
    return true;
}

void NullRenderer::beginFrame(const Color& /* clear*/) {}
void NullRenderer::draw(const RenderObject& /* obj*/) {}
void NullRenderer::endFrame() {
    ++frames_;
    if (frames_ > 1) closed_ = true;
}

std::vector<event::InputEvent> NullRenderer::pollEvents() {
    return {};
}

bool NullRenderer::shouldClose() const {
    return closed_;
}

} // namespace fw::render

