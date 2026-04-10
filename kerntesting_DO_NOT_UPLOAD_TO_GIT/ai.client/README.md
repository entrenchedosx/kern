# ai.client

Unified AI client package for Kern with normalized responses.

## API

- `create_client(options)`
- `from_env(prefix)`
- `build_chat_request(client, messages, options)`
- `chat_complete(client, messages, options, logger)`
- `normalize_response(provider, rawText, parsedJson)`

## Dependencies

- `web.httpplus`
- `sys.config`
- `sys.log`

## Quick use

```kn
let ai = import("ai.client")
let c = ai["create_client"]({ "provider": "custom", "endpoint": "https://httpbin.org/post", "model": "mini" })
let r = ai["chat_complete"](c, [{ "role": "user", "content": "hello" }], { "temperature": 0.1 }, null)
print(json_stringify(r, 2))
```
