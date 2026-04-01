# BrowserKit for Kern

BrowserKit extends Kern with a browser-engine foundation that is modular and importable.

## Platform + runtime policy

- Browser rendering path is currently validated on **Windows** (`g2d`/Raylib build).
- BrowserKit network bridges respect runtime guard policy:
  - sandbox enabled => OS/network bridge calls are denied with structured errors.
- Unsupported/incomplete production paths fail loudly using explicit codes (no fake success paths).

## Modules

- `network/http.kn`: HTTP/HTTPS client wrappers, headers/cookies metadata, DNS helper, explicit bridge requirements.
- `network/ws.kn`: WebSocket session abstraction via external bridge (`websocat`) with fail-loud behavior when unavailable.
- `network/loader.kn`: Async resource queue and response cache.
- `network/protocols.kn`: Custom protocol handler registration.
- `html/parser.kn`: HTML parser to DOM tree (elements, attrs, text, nesting).
- `dom/tree.kn`, `dom/query.kn`: DOM creation, querying, and manipulation APIs.
- `css/parser.kn`, `css/style_engine.kn`: CSS parsing + selector matching + style application.
- `js_engine/runtime.kn`: JS-like events/timers/DOM bridge (`set_timeout`, `set_interval`, event dispatch).
- `js_engine/plugins.kn`: Plugin/extension registry and hooks.
- `render/layout.kn`, `render/paint2d.kn`: box-layout and painter for g2d rendering.
- `ui/events.kn`, `ui/window_tabs.kn`: mouse/keyboard bubbling events, window/tab/history/navigation.
- `storage/local.kn`: local storage with persistence.
- `security/policy.kn`: sandbox file-access guard + same-origin checks.
- `module.kn`: convenience import facade.

## Demos

- `examples/browserkit_load_page.kn`
- `examples/browserkit_render_demo.kn`

## Diagnostics (selected)

- `BROWSERKIT-SANDBOX-DENY`
- `BROWSERKIT-NOT-IMPLEMENTED`
- `BROWSERKIT-UNSUPPORTED-PROTOCOL`
- `BROWSERKIT-INIT-FAIL`
