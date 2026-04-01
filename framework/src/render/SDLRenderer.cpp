#include "fw/render/SDLRenderer.hpp"

#if FW_ENABLE_SDL2
#include <algorithm>
#include <thread>
#include <chrono>

namespace fw::render {

SDLRenderer::~SDLRenderer() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool SDLRenderer::initialize(int width, int height, const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) return false;
    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!window_) return false;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    return renderer_ != nullptr;
}

void SDLRenderer::beginFrame(const Color& clear) {
    SDL_SetRenderDrawColor(renderer_, clear.r, clear.g, clear.b, clear.a);
    SDL_RenderClear(renderer_);
}

void SDLRenderer::draw(const RenderObject& obj) {
    SDL_SetRenderDrawColor(renderer_, obj.color.r, obj.color.g, obj.color.b, obj.color.a);
    if (obj.type == RenderObjectType::Rect) {
        SDL_FRect r{obj.frame.x, obj.frame.y, obj.frame.width, obj.frame.height};
        SDL_RenderFillRectF(renderer_, &r);
    } else if (obj.type == RenderObjectType::Text) {
        drawText(static_cast<int>(obj.frame.x), static_cast<int>(obj.frame.y), obj.text, obj.color);
    }
}

void SDLRenderer::drawText(int x, int y, const std::string& text, const Color& color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    int cursorX = x;
    for (unsigned char ch : text) {
        // lightweight procedural glyph: 5x7 bitmap based on code bits.
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                const uint8_t bit = static_cast<uint8_t>((ch >> ((row + col) % 6)) & 1u);
                if (bit) {
                    SDL_Rect px{cursorX + col * 2, y + row * 2, 2, 2};
                    SDL_RenderFillRect(renderer_, &px);
                }
            }
        }
        cursorX += 12;
    }
}

void SDLRenderer::endFrame() {
    SDL_RenderPresent(renderer_);
}

std::vector<event::InputEvent> SDLRenderer::pollEvents() {
    std::vector<event::InputEvent> out;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        event::InputEvent e{};
        switch (ev.type) {
            case SDL_QUIT:
                e.type = event::EventType::Quit;
                closed_ = true;
                out.push_back(e);
                break;
            case SDL_KEYDOWN:
                e.type = event::EventType::KeyDown;
                e.key = static_cast<int>(ev.key.keysym.sym);
                out.push_back(e);
                break;
            case SDL_KEYUP:
                e.type = event::EventType::KeyUp;
                e.key = static_cast<int>(ev.key.keysym.sym);
                out.push_back(e);
                break;
            case SDL_MOUSEMOTION:
                e.type = event::EventType::MouseMove;
                e.x = ev.motion.x;
                e.y = ev.motion.y;
                out.push_back(e);
                break;
            case SDL_MOUSEBUTTONDOWN:
                e.type = event::EventType::MouseDown;
                e.x = ev.button.x;
                e.y = ev.button.y;
                e.button = ev.button.button;
                out.push_back(e);
                break;
            case SDL_MOUSEBUTTONUP:
                e.type = event::EventType::MouseUp;
                e.x = ev.button.x;
                e.y = ev.button.y;
                e.button = ev.button.button;
                out.push_back(e);
                break;
            default:
                break;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return out;
}

bool SDLRenderer::shouldClose() const {
    return closed_;
}

} // namespace fw::render

#else

namespace fw::render {
SDLRenderer::~SDLRenderer() = default;
bool SDLRenderer::initialize(int, int, const char*) { return false; }
void SDLRenderer::beginFrame(const Color&) {}
void SDLRenderer::draw(const RenderObject&) {}
void SDLRenderer::endFrame() {}
std::vector<event::InputEvent> SDLRenderer::pollEvents() { return {}; }
bool SDLRenderer::shouldClose() const { return true; }
void SDLRenderer::drawText(int, int, const std::string&, const Color&) {}
} // namespace fw::render

#endif

