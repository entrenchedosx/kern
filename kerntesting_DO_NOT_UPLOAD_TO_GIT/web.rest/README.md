# web.rest

REST-client scaffolding for Kern services.

## API

- `create_client(options)`
- `get(client, path)`
- `post(client, path, body)`
- `verify_access_token(token, secret)`
- `paginate(items, page, pageSize)`

## Dependencies

- `web.httpplus`
- `sec.auth`

## Quick use

```kn
let rest = import("web.rest")
let c = rest["create_client"]({ "base_url": "https://httpbin.org", "retries": 3 })
print(json_stringify(rest["get"](c, "/get"), 2))
```
