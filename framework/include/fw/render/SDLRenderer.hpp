#pragma once

#include "fw/render/Renderer.hpp"

#if FW_ENABLE_SDL2
#include <SDL.h>
#endif

namespace fw::render {

class SDLRenderer final : public Renderer {
public:
    SDLRenderer() = default;
    ~SDLRenderer() override;

    bool initialize(int width, int height, const char* title) override;
    void beginFrame(const Color& clear) override;
    void draw(const RenderObject& obj) override;
    void endFrame() override;
    std::vector<event::InputEvent> pollEvents() override;
    bool shouldClose() const override;

private:
#if FW_ENABLE_SDL2
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
#endif
    bool closed_{false};

    void drawText(int x, int y, const std::string& text, const Color& color);
};

} // namespace fw::render

