# web.webhooks

Webhook toolkit for signing, verification, replay checks, and delivery.

## API

- `sign_payload(payload, secret)`
- `verify_payload(payload, secret, signature)`
- `make_headers(eventName, payload, secret)`
- `verify_replay(timestampText, maxAgeSeconds)`
- `send(url, eventName, payload, secret, logger)`

## Dependencies

- `web.httpplus`
- `sec.crypto`
- `sys.log`

## Quick use

```kn
let hooks = import("web.webhooks")
let sig = hooks["sign_payload"]({ "id": "evt_1" }, "whsec_123")
print(sig)
```
