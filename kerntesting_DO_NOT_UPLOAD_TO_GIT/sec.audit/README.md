# sec.audit

Security audit package with signed events and append-only audit lines.

## API

- `create_event(actor, action, resource, result, meta)`
- `sign_event(event, secret)`
- `verify_event(event, signature, secret)`
- `append_file(path, event, signature)`
- `audit(logger, actor, action, resource, result, meta, secret, filePath)`
- `permissions_snapshot()`

## Dependencies

- `sec.crypto`
- `sec.perm`
- `sys.log`

## Quick use

```kn
let audit = import("sec.audit")
let r = audit["audit"](null, "user:1", "login", "auth", "ok", { "ip": "127.0.0.1" }, "audit-secret", "./audit.log")
print(json_stringify(r, 2))
```
