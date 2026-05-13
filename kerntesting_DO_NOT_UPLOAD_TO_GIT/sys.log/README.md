# sys.log

Structured logging for Kern services and CLIs.

Features:
- log levels: debug/info/warn/error
- text or JSON output
- sensitive field redaction
- child loggers with inherited context

## API

- `create_logger(options)`
- `log(logger, level, message, meta)`
- `debug/info/warn/error(logger, message, meta)`
- `child(logger, extraContext)`

## Quick use

```kn
let slog = import("sys.log")
let lg = slog["create_logger"]({ "name": "billing", "format": "json", "level": "info" })
slog["info"](lg, "charge accepted", { "order_id": "o-1", "token": "secret-token" })
```
