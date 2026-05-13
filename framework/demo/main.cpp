#include "fw/app/Application.hpp"
#include "fw/document/Node.hpp"
#include "fw/event/Event.hpp"
#include "fw/render/NullRenderer.hpp"
#include "fw/render/SDLRenderer.hpp"

#include <iostream>
#include <memory>

namespace {

void attachDefaultHandlers(const fw::document::Node::Ptr& node) {
    if (!node) return;
    if (node->type() == fw::document::NodeType::Element) {
        node->addEventListener("mousedown", [](fw::event::InputEvent& ev, fw::document::Node& n) {
            n.setAttribute("background", "#335577");
            ev.handled = true;
        });
    }
    for (const auto& ch : node->children()) attachDefaultHandlers(ch);
}

} // namespace

int main() {
    std::unique_ptr<fw::render::Renderer> renderer;
#if FW_ENABLE_SDL2
    renderer = std::make_unique<fw::render::SDLRenderer>();
#else
    renderer = std::make_unique<fw::render::NullRenderer>();
#endif

    fw::app::Application app(std::move(renderer));
    if (!app.initialize(1200, 760, "Document Runtime Framework")) {
        std::cerr << "renderer initialization failed\n";
        return 1;
    }

    const std::string styles = R"(
root { background: #1a1a24; color: #f2f2f2; padding: 8; }
head { display: block; background: #2d2d44; height: 36; color: #ffffff; }
body { display: block; background: #202030; padding: 8; }
title { display: block; background: #34455d; color: #e6f0ff; padding: 6; margin: 4; }
p { display: block; background: #2b2b3b; color: #dcdcdc; padding: 6; margin: 4; }
.inline { display: inline; background: #3a2f5b; color: #ffffff; width: 120; height: 24; margin: 2; }
#hero { background: #425f87; height: 52; }
)";
    app.loadStyles(styles);

    if (!app.loadRemoteDocument("http://example.com/")) {
        const std::string fallback = R"(
<root>
  <head text="Runtime Header" />
  <body>
    <title id="hero" text="Fetched content unavailable. Using local content." />
    <p text="This runtime composes networking, parsing, styling, layout, rendering, events, resources, and scripting." />
    <p class="inline" text="inline-a" />
    <p class="inline" text="inline-b" />
    <p class="inline" text="inline-c" />
  </body>
</root>
)";
        app.document() = fw::document::Parser().parse(fallback);
        app.rebuild();
    }

    attachDefaultHandlers(app.document().root());
    app.scripts().bind("on_event", [](fw::document::Document& doc, fw::event::InputEvent& ev) {
        if (ev.type == fw::event::EventType::KeyDown && ev.key != 0 && doc.root()) {
            doc.root()->setAttribute("last_key", std::to_string(ev.key));
            ev.handled = true;
        }
    });

    app.run();
    return 0;
}

