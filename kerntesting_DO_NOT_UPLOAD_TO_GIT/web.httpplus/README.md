# web.httpplus

Resilient HTTP helper package for Kern.

## API

- `get(url)`
- `get_json(url)`
- `post_json(url, bodyObject)`
- `request_with_retry(method, url, bodyObject, retries, baseBackoffMs, logger)`

## Permissions

- `network.http`

## Quick use

```kn
let httpx = import("web.httpplus")
let r = httpx["request_with_retry"]("GET", "https://httpbin.org/get", null, 3, 200, null)
print(json_stringify(r, 2))
```
