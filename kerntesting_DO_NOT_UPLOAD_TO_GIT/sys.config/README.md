# sys.config

Layered configuration package for Kern applications.

Features:
- load defaults + JSON config file + env overrides
- safe result objects (`ok/value/error/code`)
- required-key checks for startup validation

## API

- `load(defaults, filePath, envPrefix, envKeys) -> {ok,value,error,code}`
- `read_json_file(path) -> {ok,value,error,code}`
- `from_env(prefix, keys) -> object`
- `merge_shallow(base, override) -> object`
- `get(config, key, fallback) -> any`
- `require_keys(config, keys) -> {ok,value,error,code}`

## Permissions

- `filesystem.read` for reading config files
- `env.access` for environment variable overlays

## Quick use

```kn
let cfgm = import("sys.config")
let r = cfgm["load"]({ "PORT": 8080 }, "./app.config.json", "APP_", ["PORT"])
if (!r["ok"]) { panic(r["error"]) }
print(r["value"])
```
