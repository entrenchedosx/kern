# sec.crypto

`sec.crypto` is a Kern package for practical crypto/security helpers:

- SHA-256, SHA-1, FNV hashing wrappers
- password hashing helpers
- message signing/verification helpers
- lightweight encrypt/decrypt (`encrypt_xor_base64`, `decrypt_xor_base64`)
- random token + nonce generation

## Quick use

```kn
let c = import("sec.crypto")
let h = c["hash_sha256"]("hello")
let cipher = c["encrypt_xor_base64"]("secret", "my-key")
let plain = c["decrypt_xor_base64"](cipher, "my-key")
```
