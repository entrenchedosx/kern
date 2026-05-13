# sec.secrets

Secret management helpers for Kern apps using `sec.crypto`.

## API

- `encrypt_value(plainText, masterKey)`
- `decrypt_value(cipherText, masterKey)`
- `save_store(path, secretMap, masterKey)`
- `load_store(path, masterKey)`
- `rotate_master_key(path, oldKey, newKey)`
- `from_env(prefix, keyNames)`

## Permissions

- `filesystem.read` and `filesystem.write`
- `env.access` when reading from environment

## Quick use

```kn
let sec = import("sec.secrets")
let saved = sec["save_store"]("./secrets.json", { "DB_URL": "postgres://..." }, "master-1")
print(json_stringify(saved, 2))
```
