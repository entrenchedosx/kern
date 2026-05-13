#include "fw/network/TcpSocket.hpp"

#include <cstring>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using NativeSocket = SOCKET;
static constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
static void closeNativeSocket(NativeSocket s) { closesocket(s); }
#ifndef FIONBIO
#define FIONBIO _FIONBIO
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
using NativeSocket = int;
static constexpr NativeSocket kInvalidSocket = -1;
static void closeNativeSocket(NativeSocket s) { ::close(s); }
#endif

namespace fw::network {

namespace {
struct WsaGuard {
    WsaGuard() {
#ifdef _WIN32
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
    }
    ~WsaGuard() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
} // namespace

TcpSocket::TcpSocket() {
    static WsaGuard guard;
    (void)guard;
}

TcpSocket::~TcpSocket() {
    close();
}

bool TcpSocket::connect(const std::string& host, uint16_t port, int timeoutMs) {
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return false;
    }

    bool ok = false;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        NativeSocket sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == kInvalidSocket) continue;
        if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
            fd_ = static_cast<uintptr_t>(sock);
            ok = true;
            break;
        }
#ifdef _WIN32
        int lastErr = WSAGetLastError();
        if (lastErr == WSAEWOULDBLOCK || lastErr == WSAEINPROGRESS) {
#else
        if (errno == EINPROGRESS) {
#endif
            unsigned long nb = 1;
#ifdef _WIN32
            ioctlsocket(sock, FIONBIO, &nb);
#else
            int flags = fcntl(sock, F_GETFL, 0);
            if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(sock, &writeSet);
            timeval tv{};
            const int clamped = timeoutMs > 0 ? timeoutMs : 5000;
            tv.tv_sec = clamped / 1000;
            tv.tv_usec = (clamped % 1000) * 1000;
            int sel = select(static_cast<int>(sock + 1), nullptr, &writeSet, nullptr, &tv);
            if (sel > 0 && FD_ISSET(sock, &writeSet)) {
                int soErr = 0;
                socklen_t soErrLen = static_cast<socklen_t>(sizeof(soErr));
                getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soErr), &soErrLen);
                if (soErr == 0) {
#ifdef _WIN32
                    nb = 0;
                    ioctlsocket(sock, FIONBIO, &nb);
#else
                    if (flags >= 0) fcntl(sock, F_SETFL, flags);
#endif
                    fd_ = static_cast<uintptr_t>(sock);
                    ok = true;
                    break;
                }
            }
        }
        closeNativeSocket(sock);
    }

    ::freeaddrinfo(result);
    return ok;
}

bool TcpSocket::sendAll(const std::string& bytes) {
    if (!isValid()) return false;
    const NativeSocket sock = static_cast<NativeSocket>(fd_);
    size_t sent = 0;
    while (sent < bytes.size()) {
#ifdef _WIN32
        const int rc = ::send(sock, bytes.data() + sent, static_cast<int>(bytes.size() - sent), 0);
#else
        const int rc = static_cast<int>(::send(sock, bytes.data() + sent, bytes.size() - sent, 0));
#endif
        if (rc <= 0) return false;
        sent += static_cast<size_t>(rc);
    }
    return true;
}

std::string TcpSocket::receiveAll() {
    std::string out;
    if (!isValid()) return out;
    const NativeSocket sock = static_cast<NativeSocket>(fd_);
    char buffer[4096];
    while (true) {
#ifdef _WIN32
        const int rc = ::recv(sock, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const int rc = static_cast<int>(::recv(sock, buffer, sizeof(buffer), 0));
#endif
        if (rc <= 0) break;
        out.append(buffer, static_cast<size_t>(rc));
    }
    return out;
}

void TcpSocket::close() {
    if (!isValid()) return;
    closeNativeSocket(static_cast<NativeSocket>(fd_));
    fd_ = static_cast<uintptr_t>(-1);
}

} // namespace fw::network

