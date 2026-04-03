# Networking and multiplayer (TCP / UDP)

Kern exposes TCP and UDP sockets as **builtins** guarded by permissions (`network.tcp`, `network.udp`). They are meant for **custom protocols**, **small servers**, and **multiplayer prototypes** — not a full game engine stack.

## Permissions

```kn
require("network.tcp")
require("network.udp")
```

Or `kern --allow=network.tcp` / `--allow=network.udp`, or `kern --unsafe` for development only.

## TCP

| Builtin | Returns (map) |
|---------|----------------|
| `tcp_connect(host, port)` | `{ ok, id, error }` — `id` is an opaque handle (blocking connect) |
| `tcp_connect_start(host, port)` | `{ ok, id, instant_connected, error }` — non-blocking connect; socket starts in **`TcpConnecting`** unless `instant_connected` is true (already connected) |
| `tcp_connect_check(socket_id)` | `{ ok, connected, pending, error }` — poll handshake: when `connected` is true, use `tcp_send`/`tcp_recv`; on failure the socket is closed and removed. Prefer **`socket_select_write`** with a positive `timeout_ms` between checks instead of a tight VM loop (avoids step-limit issues on some hosts). |
| `tcp_listen(bind_host, port [, backlog])` | `{ ok, id, error }` — use `"0.0.0.0"` for all IPv4 interfaces (recommended for local clients using `127.0.0.1`) |
| `tcp_accept(listen_id)` | `{ ok, id, would_block, error }` — on success, `id` is the new client, or `-1` when `would_block` is true (non-blocking listen socket) |
| `tcp_send(socket_id, data_string)` | `{ ok, error }` |
| `tcp_recv(socket_id [, max_bytes])` | `{ ok, data, eof, would_block, error }` — `would_block` true when the socket is non-blocking and no data is ready |
| `tcp_close(socket_id)` | `{ ok, error }` |

By default, recv/accept **block** the VM thread. For **g3d** or other tight loops, use **`socket_set_nonblocking`** + **`socket_select_read`** / **`socket_select_write`** (async **`tcp_connect_start`** + **`tcp_connect_check`**, or run networking in a **second process**).

## UDP

| Builtin | Returns (map) |
|---------|----------------|
| `udp_open()` | `{ ok, id, error }` |
| `udp_bind(socket_id, host, port)` | `{ ok, error }` — `host` `""` binds all interfaces |
| `udp_send(socket_id, host, port, data)` | `{ ok, error }` |
| `udp_recv(socket_id [, max_bytes])` | `{ ok, data, host, port, would_block, error }` |
| `udp_close(socket_id)` | `{ ok, error }` |

## Non-blocking and `select`

| Builtin | Permission | Returns (map) |
|---------|------------|----------------|
| `socket_set_nonblocking(socket_id, bool)` | `network.tcp` **or** `network.udp` | `{ ok, error }` |
| `socket_select_read(socket_ids_array, timeout_ms)` | `network.tcp` **or** `network.udp` | `{ ok, ready, error }` — `ready` is an array of socket ids that are readable |
| `socket_select_write(socket_ids_array, timeout_ms)` | `network.tcp` **or** `network.udp` | `{ ok, ready, error }` — `ready` is an array of socket ids that are writable (e.g. pending **`tcp_connect_start`**) |

- **`timeout_ms`:** `0` = poll; **`-1`** = wait until one socket becomes readable (no timeout).
- **Limit:** at most **`FD_SETSIZE`** sockets per call (often **64** on Windows, larger on many Unix systems).

## Examples (repo)

- `examples/network/tcp_echo_server.kn` — blocking echo (pairs with `tcp_echo_client.kn`).
- `examples/network/tcp_select_accept.kn` — non-blocking listen + `socket_select_read` + accept.
- See `examples/network/README.md` for two-terminal commands.

## 3D + multiplayer architecture

- **Rendering:** `import("g3d")` (Raylib) for local drawing; combine **`socket_select_read`** with short timeouts (or a **second `kern` process** for the server) so the frame callback does not stall on blocking recv.
- **Typical pattern:** a **dedicated server** process using `tcp_listen` / `tcp_accept`, and **clients** using `tcp_connect` + your own message framing (JSON lines, length-prefixed binary, etc.).
- **Same machine:** two terminals, or `spawn` another `kern` with the server script.

## Limitations (honest)

- **No** built-in replication, prediction, or lobby — you design messages and state.
- **No** async/await over sockets — use **`select`**, non-blocking recv, or **multiple processes**.
- **One OS thread** for the main VM; parallel work is **separate processes** or native extensions.

## See also

- [INTERNALS_MODULES_AND_SECURITY.md](INTERNALS_MODULES_AND_SECURITY.md) — permission model
- [TRUST_MODEL.md](TRUST_MODEL.md)
- [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md) — registry map
