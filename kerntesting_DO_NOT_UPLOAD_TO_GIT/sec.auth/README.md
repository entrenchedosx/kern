# sec.auth

`sec.auth` provides practical authentication helpers for Kern:

- token issue/verify/refresh
- session create/validate/revoke
- password hash/verify wrappers

Depends on `sec.crypto`.

## Quick use

```kn
let auth = import("sec.auth")
let t = auth["issue_token"]({ "sub": "u1" }, "top-secret", 3600)
let ok = auth["verify_token"](t, "top-secret")
```
