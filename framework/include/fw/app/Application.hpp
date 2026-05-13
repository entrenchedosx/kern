#pragma once

#include "fw/document/Parser.hpp"
#include "fw/event/EventDispatcher.hpp"
#include "fw/layout/LayoutEngine.hpp"
#include "fw/render/RenderTree.hpp"
#include "fw/render/Renderer.hpp"
#include "fw/resource/ResourceManager.hpp"
#include "fw/script/ScriptContext.hpp"
#include "fw/style/StyleResolver.hpp"
#include "fw/style/StyleSheet.hpp"

#include <memory>
#include <string>

namespace fw::app {

class Application {
public:
    explicit Application(std::unique_ptr<render::Renderer> renderer);

    bool initialize(int width, int height, const std::string& title);
    bool loadRemoteDocument(const std::string& url);
    bool loadStyles(const std::string& styleSource);
    void rebuild();
    void run();

    document::Document& document() noexcept { return document_; }
    script::ScriptContext& scripts() noexcept { return scriptContext_; }

private:
    std::unique_ptr<render::Renderer> renderer_;
    resource::ResourceManager resources_;
    document::Parser parser_;
    style::StyleSheet styleSheet_;
    style::StyleResolver styleResolver_;
    layout::LayoutEngine layoutEngine_;
    render::RenderTreeBuilder renderTreeBuilder_;
    event::EventDispatcher eventDispatcher_;
    script::ScriptContext scriptContext_;

    document::Document document_;
    layout::LayoutTree layoutTree_;
    render::RenderTree renderTree_;
    int width_{1280};
    int height_{720};
};

} // namespace fw::app

