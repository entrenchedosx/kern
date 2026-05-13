# sec.perm

`sec.perm` wraps Kern capability/permission behavior with helper functions:

- grant one or many permissions/groups
- inspect active granted permissions and groups
- check any/all granted
- generate summary for audit logs
- guidance string for pairing with `unsafe {}` blocks

## Quick use

```kn
let perm = import("sec.perm")
perm["ensure"]("fs.readwrite")
let meta = perm["describe"]()
print(meta)
```
