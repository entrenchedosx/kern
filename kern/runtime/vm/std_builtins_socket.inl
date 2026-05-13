// Socket builtins (TCP/UDP). Included from std_builtins_v1.inl after `require`.
// Uses: makeBuiltin, i, toInt, VM, Value, ValuePtr, vmRequirePermission, Perm, kernSocket*.

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_connect");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.size() < 2 || !args[0] || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_connect(host, port)"));
            return Value::fromMap(std::move(m));
        }
        std::string host = args[0]->toString();
        int port = static_cast<int>(toInt(args[1]));
        int64_t id = -1;
        std::string err;
        if (kernTcpConnect(host, port, id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_connect", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_connect_start");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        m["instant_connected"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.size() < 2 || !args[0] || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_connect_start(host, port)"));
            return Value::fromMap(std::move(m));
        }
        std::string host = args[0]->toString();
        int port = static_cast<int>(toInt(args[1]));
        int64_t id = -1;
        bool instant = false;
        std::string err;
        if (kernTcpConnectStart(host, port, id, instant, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
            m["instant_connected"] = std::make_shared<Value>(Value::fromBool(instant));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_connect_start", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_connect_check");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["connected"] = std::make_shared<Value>(Value::fromBool(false));
        m["pending"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_connect_check(socket_id)"));
            return Value::fromMap(std::move(m));
        }
        bool connected = false;
        bool pending = false;
        std::string err;
        if (kernTcpConnectCheck(toInt(args[0]), connected, pending, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["connected"] = std::make_shared<Value>(Value::fromBool(connected));
            m["pending"] = std::make_shared<Value>(Value::fromBool(pending));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_connect_check", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_listen");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.size() < 2 || !args[0] || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_listen(bind_host, port [, backlog])"));
            return Value::fromMap(std::move(m));
        }
        std::string bh = args[0]->toString();
        int port = static_cast<int>(toInt(args[1]));
        int backlog = 16;
        if (args.size() >= 3 && args[2]) backlog = static_cast<int>(toInt(args[2]));
        int64_t id = -1;
        std::string err;
        if (kernTcpListen(bh, port, backlog, id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_listen", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_accept");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        m["would_block"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_accept(listen_id)"));
            return Value::fromMap(std::move(m));
        }
        int64_t lid = toInt(args[0]);
        int64_t cid = -1;
        bool wb = false;
        std::string err;
        if (kernTcpAccept(lid, cid, wb, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["would_block"] = std::make_shared<Value>(Value::fromBool(wb));
            m["id"] = std::make_shared<Value>(Value::fromInt(wb ? -1 : cid));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_accept", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_send");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 2 || !args[0] || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_send(socket_id, data_string)"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        std::string data = args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : args[1]->toString();
        std::string err;
        if (kernTcpSend(sid, data, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_send", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_recv");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["data"] = std::make_shared<Value>(Value::fromString(""));
        m["eof"] = std::make_shared<Value>(Value::fromBool(false));
        m["would_block"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_recv(socket_id [, max_bytes])"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        size_t maxB = 65536;
        if (args.size() >= 2 && args[1]) maxB = static_cast<size_t>(std::max<int64_t>(1, toInt(args[1])));
        std::string data;
        bool eof = false;
        bool wb = false;
        std::string err;
        if (kernTcpRecv(sid, maxB, data, eof, wb, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["data"] = std::make_shared<Value>(Value::fromString(std::move(data)));
            m["eof"] = std::make_shared<Value>(Value::fromBool(eof));
            m["would_block"] = std::make_shared<Value>(Value::fromBool(wb));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_recv", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "tcp_close");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("tcp_close(socket_id)"));
            return Value::fromMap(std::move(m));
        }
        std::string err;
        if (kernSocketClose(toInt(args[0]), err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("tcp_close", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        (void)args;
        vmRequirePermission(vm, Perm::kNetworkUdp, "udp_open");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        int64_t id = -1;
        std::string err;
        if (kernUdpOpen(id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("udp_open", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkUdp, "udp_bind");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 3 || !args[0] || !args[1] || !args[2]) {
            m["error"] = std::make_shared<Value>(Value::fromString("udp_bind(socket_id, host, port) — host \"\" for all interfaces"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        std::string host = args[1]->toString();
        int port = static_cast<int>(toInt(args[2]));
        std::string err;
        if (kernUdpBind(sid, host, port, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("udp_bind", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkUdp, "udp_send");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 4 || !args[0] || !args[1] || !args[2] || !args[3]) {
            m["error"] = std::make_shared<Value>(Value::fromString("udp_send(socket_id, host, port, data)"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        std::string host = args[1]->toString();
        int port = static_cast<int>(toInt(args[2]));
        std::string data = args[3]->type == Value::Type::STRING ? std::get<std::string>(args[3]->data) : args[3]->toString();
        std::string err;
        if (kernUdpSend(sid, host, port, data, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("udp_send", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkUdp, "udp_recv");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["data"] = std::make_shared<Value>(Value::fromString(""));
        m["host"] = std::make_shared<Value>(Value::fromString(""));
        m["port"] = std::make_shared<Value>(Value::fromInt(0));
        m["would_block"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("udp_recv(socket_id [, max_bytes])"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        size_t maxB = 65536;
        if (args.size() >= 2 && args[1]) maxB = static_cast<size_t>(std::max<int64_t>(1, toInt(args[1])));
        std::string data, host;
        int port = 0;
        bool wb = false;
        std::string err;
        if (kernUdpRecv(sid, maxB, data, host, port, wb, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["data"] = std::make_shared<Value>(Value::fromString(std::move(data)));
            m["host"] = std::make_shared<Value>(Value::fromString(std::move(host)));
            m["port"] = std::make_shared<Value>(Value::fromInt(port));
            m["would_block"] = std::make_shared<Value>(Value::fromBool(wb));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("udp_recv", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkUdp, "udp_close");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.empty() || !args[0]) {
            m["error"] = std::make_shared<Value>(Value::fromString("udp_close(socket_id)"));
            return Value::fromMap(std::move(m));
        }
        std::string err;
        if (kernSocketClose(toInt(args[0]), err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("udp_close", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vmPermissionAllowed(vm, Perm::kNetworkTcp) && !vmPermissionAllowed(vm, Perm::kNetworkUdp))
            vmRequirePermission(vm, Perm::kNetworkTcp, "socket_set_nonblocking");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 2 || !args[0] || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("socket_set_nonblocking(socket_id, bool)"));
            return Value::fromMap(std::move(m));
        }
        int64_t sid = toInt(args[0]);
        bool nb = false;
        if (args[1]->type == Value::Type::BOOL)
            nb = std::get<bool>(args[1]->data);
        else
            nb = toInt(args[1]) != 0;
        std::string err;
        if (kernSocketSetNonBlocking(sid, nb, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("socket_set_nonblocking", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vmPermissionAllowed(vm, Perm::kNetworkTcp) && !vmPermissionAllowed(vm, Perm::kNetworkUdp))
            vmRequirePermission(vm, Perm::kNetworkTcp, "socket_select_read");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["ready"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("socket_select_read(socket_ids_array, timeout_ms)"));
            return Value::fromMap(std::move(m));
        }
        const auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<int64_t> ids;
        ids.reserve(arr.size());
        for (const ValuePtr& v : arr) {
            if (!v) continue;
            ids.push_back(toInt(v));
        }
        int timeoutMs = static_cast<int>(toInt(args[1]));
        std::vector<int64_t> ready;
        std::string err;
        if (kernSocketSelectRead(ids, timeoutMs, ready, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            std::vector<ValuePtr> rv;
            for (int64_t r : ready) rv.push_back(std::make_shared<Value>(Value::fromInt(r)));
            m["ready"] = std::make_shared<Value>(Value::fromArray(std::move(rv)));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("socket_select_read", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vmPermissionAllowed(vm, Perm::kNetworkTcp) && !vmPermissionAllowed(vm, Perm::kNetworkUdp))
            vmRequirePermission(vm, Perm::kNetworkTcp, "socket_select_write");
        kernSocketInit();
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        m["ready"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY || !args[1]) {
            m["error"] = std::make_shared<Value>(Value::fromString("socket_select_write(socket_ids_array, timeout_ms)"));
            return Value::fromMap(std::move(m));
        }
        const auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<int64_t> ids;
        ids.reserve(arr.size());
        for (const ValuePtr& v : arr) {
            if (!v) continue;
            ids.push_back(toInt(v));
        }
        int timeoutMs = static_cast<int>(toInt(args[1]));
        std::vector<int64_t> ready;
        std::string err;
        if (kernSocketSelectWrite(ids, timeoutMs, ready, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            std::vector<ValuePtr> rv;
            for (int64_t r : ready) rv.push_back(std::make_shared<Value>(Value::fromInt(r)));
            m["ready"] = std::make_shared<Value>(Value::fromArray(std::move(rv)));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("socket_select_write", i - 1);
