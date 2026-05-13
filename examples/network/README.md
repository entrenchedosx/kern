# TCP examples

Requires `kern --unsafe` (or `require("network.tcp")` plus `--allow=network.tcp`).

## Echo (two terminals)

**Terminal 1 — server**

```bash
kern --unsafe examples/network/tcp_echo_server.kn
```

**Terminal 2 — client**

```bash
kern --unsafe examples/network/tcp_echo_client.kn
```

Optional: `kern --unsafe tcp_echo_server.kn 9999` and `kern --unsafe tcp_echo_client.kn 127.0.0.1 9999 "ping\n"` (see script comments for `cli_args()` layout).

## Async connect (non-blocking handshake)

**Terminal 1 — server** (same as echo)

```bash
kern --unsafe examples/network/tcp_echo_server.kn
```

**Terminal 2**

```bash
kern --unsafe examples/network/tcp_async_client.kn
```

Uses `tcp_connect_start`, `socket_select_write`, and `tcp_connect_check` instead of blocking `tcp_connect`.

## Non-blocking accept

**Terminal 1**

```bash
kern --unsafe examples/network/tcp_select_accept.kn
```

**Terminal 2** (within ~60s)

```bash
kern --unsafe examples/network/tcp_echo_client.kn
```

See [NETWORKING_MULTIPLAYER.md](../../docs/NETWORKING_MULTIPLAYER.md) for API details.
