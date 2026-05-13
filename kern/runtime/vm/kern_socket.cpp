/* *
 * TCP/UDP sockets for multiplayer / networking builtins (blocking by default; optional non-blocking).
 */
#include "kern_socket.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#endif

namespace kern {

namespace {

struct KernSocketState {
#ifdef _WIN32
    SOCKET s = INVALID_SOCKET;
#else
    int fd = -1;
#endif
    enum Kind { TcpConnecting, TcpConnected, TcpListen, Udp } kind = TcpConnected;
    /** Valid when `kind == TcpConnecting` (async connect target). */
    struct sockaddr_storage connectAddr {};
    socklen_t connectAddrLen = 0;
};

static std::mutex gSockMutex;
static std::unordered_map<int64_t, KernSocketState> gSocks;
static std::atomic<int64_t> gNextId{1};
static bool gWsaStarted = false;

#ifdef _WIN32
static void closeSock(SOCKET s) {
    if (s != INVALID_SOCKET) closesocket(s);
}
#else
static void closeFd(int fd) {
    if (fd >= 0) ::close(fd);
}
#endif

static bool portOk(int p) { return p > 0 && p <= 65535; }

#ifdef _WIN32
static bool ensureWsa(std::string& err) {
    if (gWsaStarted) return true;
    WSADATA wsa{};
    int r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0) {
        err = "WSAStartup failed";
        return false;
    }
    gWsaStarted = true;
    return true;
}
#endif

static int64_t allocId(KernSocketState st) {
    std::lock_guard<std::mutex> lock(gSockMutex);
    int64_t id = gNextId++;
    gSocks[id] = std::move(st);
    return id;
}

static bool getSock(int64_t id, KernSocketState& out, std::string& err) {
    std::lock_guard<std::mutex> lock(gSockMutex);
    auto it = gSocks.find(id);
    if (it == gSocks.end()) {
        err = "invalid socket id";
        return false;
    }
    out = it->second;
    return true;
}

} // namespace

void kernSocketInit() {
#ifdef _WIN32
    std::string err;
    (void)ensureWsa(err);
#endif
}

bool kernTcpConnect(const std::string& host, int port, int64_t& outId, std::string& err) {
    outId = -1;
    if (!portOk(port)) {
        err = "invalid port";
        return false;
    }
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    int gai = getaddrinfo(host.empty() ? "127.0.0.1" : host.c_str(), portStr.c_str(), &hints, &res);
    if (gai != 0) {
        err = std::string("getaddrinfo: ") + gai_strerror(gai);
        return false;
    }
    if (!res) {
        err = "getaddrinfo returned no addresses";
        return false;
    }
#ifdef _WIN32
    SOCKET s = INVALID_SOCKET;
#else
    int s = -1;
#endif
    struct addrinfo* p = res;
    for (; p; p = p->ai_next) {
#ifdef _WIN32
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
#else
        s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        if (::connect(s, p->ai_addr, p->ai_addrlen) == 0) break;
        closeFd(s);
        s = -1;
#endif
    }
    freeaddrinfo(res);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
        err = "connect failed";
        return false;
    }
    KernSocketState st;
    st.s = s;
    st.kind = KernSocketState::TcpConnected;
#else
    if (s < 0) {
        err = "connect failed";
        return false;
    }
    KernSocketState st;
    st.fd = s;
    st.kind = KernSocketState::TcpConnected;
#endif
    outId = allocId(std::move(st));
    return true;
}

bool kernTcpConnectStart(const std::string& host, int port, int64_t& outId, bool& instantConnected, std::string& err) {
    outId = -1;
    instantConnected = false;
    if (!portOk(port)) {
        err = "invalid port";
        return false;
    }
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    int gai = getaddrinfo(host.empty() ? "127.0.0.1" : host.c_str(), portStr.c_str(), &hints, &res);
    if (gai != 0) {
        err = std::string("getaddrinfo: ") + gai_strerror(gai);
        return false;
    }
    if (!res) {
        err = "getaddrinfo returned no addresses";
        return false;
    }
#ifdef _WIN32
    SOCKET s = INVALID_SOCKET;
#else
    int s = -1;
#endif
    struct addrinfo* p = res;
    for (; p; p = p->ai_next) {
#ifdef _WIN32
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        u_long nb = 1UL;
        if (ioctlsocket(s, FIONBIO, &nb) != 0) {
            closesocket(s);
            s = INVALID_SOCKET;
            continue;
        }
        int cr = connect(s, p->ai_addr, static_cast<int>(p->ai_addrlen));
        if (cr == 0) {
            instantConnected = true;
            KernSocketState st;
            st.s = s;
            st.kind = KernSocketState::TcpConnected;
            outId = allocId(std::move(st));
            freeaddrinfo(res);
            return true;
        }
        int w = WSAGetLastError();
        if (w == WSAEWOULDBLOCK || w == WSAEINPROGRESS) {
            if (static_cast<size_t>(p->ai_addrlen) > sizeof(sockaddr_storage)) {
                closesocket(s);
                s = INVALID_SOCKET;
                continue;
            }
            KernSocketState st;
            st.s = s;
            st.kind = KernSocketState::TcpConnecting;
            std::memcpy(&st.connectAddr, p->ai_addr, p->ai_addrlen);
            st.connectAddrLen = static_cast<socklen_t>(p->ai_addrlen);
            outId = allocId(std::move(st));
            freeaddrinfo(res);
            return true;
        }
        closesocket(s);
        s = INVALID_SOCKET;
#else
        s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        int flags = fcntl(s, F_GETFL, 0);
        if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) != 0) {
            closeFd(s);
            s = -1;
            continue;
        }
        int cr = ::connect(s, p->ai_addr, p->ai_addrlen);
        if (cr == 0) {
            instantConnected = true;
            KernSocketState st;
            st.fd = s;
            st.kind = KernSocketState::TcpConnected;
            outId = allocId(std::move(st));
            freeaddrinfo(res);
            return true;
        }
        if (errno == EINPROGRESS) {
            if (static_cast<size_t>(p->ai_addrlen) > sizeof(sockaddr_storage)) {
                closeFd(s);
                s = -1;
                continue;
            }
            KernSocketState st;
            st.fd = s;
            st.kind = KernSocketState::TcpConnecting;
            std::memcpy(&st.connectAddr, p->ai_addr, p->ai_addrlen);
            st.connectAddrLen = static_cast<socklen_t>(p->ai_addrlen);
            outId = allocId(std::move(st));
            freeaddrinfo(res);
            return true;
        }
        closeFd(s);
        s = -1;
#endif
    }
    freeaddrinfo(res);
    err = "connect failed";
    return false;
}

bool kernTcpConnectCheck(int64_t id, bool& connected, bool& pending, std::string& err) {
    connected = false;
    pending = false;
    std::lock_guard<std::mutex> lock(gSockMutex);
    auto it = gSocks.find(id);
    if (it == gSocks.end()) {
        err = "invalid socket id";
        return false;
    }
    if (it->second.kind == KernSocketState::TcpConnected) {
        connected = true;
        return true;
    }
    if (it->second.kind != KernSocketState::TcpConnecting) {
        err = "not a pending tcp_connect_start socket";
        return false;
    }
#ifdef _WIN32
    SOCKET sock = it->second.s;
#else
    int fd = it->second.fd;
#endif
    auto eraseConnecting = [&]() {
#ifdef _WIN32
        closesocket(sock);
#else
        closeFd(fd);
#endif
        gSocks.erase(it);
    };
    auto finishSuccess = [&]() {
        it->second.kind = KernSocketState::TcpConnected;
        it->second.connectAddrLen = 0;
        connected = true;
        return true;
    };
    if (it->second.connectAddrLen > 0) {
#ifdef _WIN32
        int cr2 =
            connect(sock, reinterpret_cast<const sockaddr*>(&it->second.connectAddr), it->second.connectAddrLen);
        if (cr2 == 0) return finishSuccess();
        int w = WSAGetLastError();
        if (w == WSAEISCONN) return finishSuccess();
        if (w == WSAECONNREFUSED || w == WSAENETUNREACH || w == WSAETIMEDOUT || w == WSAEHOSTUNREACH ||
            w == WSAECONNRESET) {
            err = std::string("connect failed (SO_ERROR ") + std::to_string(w) + ")";
            eraseConnecting();
            return false;
        }
        if (w != WSAEWOULDBLOCK && w != WSAEINPROGRESS && w != WSAEALREADY && w != WSAEINVAL) {
            err = std::string("connect failed (SO_ERROR ") + std::to_string(w) + ")";
            eraseConnecting();
            return false;
        }
#else
        int cr2 = ::connect(fd, reinterpret_cast<const sockaddr*>(&it->second.connectAddr), it->second.connectAddrLen);
        if (cr2 == 0) return finishSuccess();
        if (errno == EISCONN) return finishSuccess();
        if (errno == ECONNREFUSED || errno == ENETUNREACH || errno == ETIMEDOUT || errno == EHOSTUNREACH ||
            errno == ECONNRESET) {
            err = std::string("connect failed: ") + std::strerror(errno);
            eraseConnecting();
            return false;
        }
        if (errno != EINPROGRESS && errno != EALREADY && errno != EAGAIN && errno != EWOULDBLOCK) {
            err = std::string("connect failed: ") + std::strerror(errno);
            eraseConnecting();
            return false;
        }
#endif
    }

    fd_set wfds;
    FD_ZERO(&wfds);
#ifdef _WIN32
    FD_SET(sock, &wfds);
#else
    FD_SET(fd, &wfds);
#endif
    struct timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 0;
#ifdef _WIN32
    int sr = select(0, nullptr, &wfds, nullptr, &tv);
#else
    int sr = select(fd + 1, nullptr, &wfds, nullptr, &tv);
#endif
    if (sr < 0) {
        err = "select failed";
        return false;
    }
    auto finishSoError = [&](int soerr) -> bool {
        if (soerr != 0) {
#ifdef _WIN32
            err = std::string("connect failed (SO_ERROR ") + std::to_string(soerr) + ")";
#else
            err = std::string("connect failed: ") + std::strerror(soerr);
#endif
#ifdef _WIN32
            closesocket(sock);
#else
            closeFd(fd);
#endif
            gSocks.erase(it);
            return false;
        }
        it->second.kind = KernSocketState::TcpConnected;
        it->second.connectAddrLen = 0;
        connected = true;
        return true;
    };
    if (sr == 0) {
        int soerrPoll = 0;
        socklen_t solenPoll = sizeof(soerrPoll);
#ifdef _WIN32
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerrPoll), &solenPoll) != 0) {
#else
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&soerrPoll), &solenPoll) != 0) {
#endif
            err = "getsockopt SO_ERROR failed";
            return false;
        }
        if (soerrPoll != 0) return finishSoError(soerrPoll);
        pending = true;
        return true;
    }
#ifdef _WIN32
    if (!FD_ISSET(sock, &wfds)) {
#else
    if (!FD_ISSET(fd, &wfds)) {
#endif
        pending = true;
        return true;
    }
    int soerr = 0;
    socklen_t solen = sizeof(soerr);
#ifdef _WIN32
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr), &solen) != 0) {
#else
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&soerr), &solen) != 0) {
#endif
        err = "getsockopt SO_ERROR failed";
        return false;
    }
    return finishSoError(soerr);
}

bool kernTcpListen(const std::string& bindHost, int port, int backlog, int64_t& outId, std::string& err) {
    outId = -1;
    if (!portOk(port)) {
        err = "invalid port";
        return false;
    }
    if (backlog < 1) backlog = 8;
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* res = nullptr;
    std::string portStr = std::to_string(port);
    const char* node = nullptr;
    if (!bindHost.empty()) node = bindHost.c_str();
    int gai = getaddrinfo(node, portStr.c_str(), &hints, &res);
    if (gai != 0 || !res) {
        err = "getaddrinfo (listen) failed";
        return false;
    }
#ifdef _WIN32
    SOCKET s = INVALID_SOCKET;
#else
    int s = -1;
#endif
    struct addrinfo* p = res;
    for (; p; p = p->ai_next) {
#ifdef _WIN32
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
        if (bind(s, p->ai_addr, static_cast<int>(p->ai_addrlen)) != 0) {
            closesocket(s);
            s = INVALID_SOCKET;
            continue;
        }
        if (listen(s, backlog) == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
#else
        s = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) continue;
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (::bind(s, p->ai_addr, p->ai_addrlen) != 0) {
            closeFd(s);
            s = -1;
            continue;
        }
        if (::listen(s, backlog) == 0) break;
        closeFd(s);
        s = -1;
#endif
    }
    freeaddrinfo(res);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
        err = "bind/listen failed";
        return false;
    }
    KernSocketState st;
    st.s = s;
    st.kind = KernSocketState::TcpListen;
#else
    if (s < 0) {
        err = "bind/listen failed";
        return false;
    }
    KernSocketState st;
    st.fd = s;
    st.kind = KernSocketState::TcpListen;
#endif
    outId = allocId(std::move(st));
    return true;
}

bool kernTcpAccept(int64_t listenId, int64_t& outClientId, bool& wouldBlock, std::string& err) {
    outClientId = -1;
    wouldBlock = false;
    KernSocketState ls;
    if (!getSock(listenId, ls, err)) return false;
    if (ls.kind != KernSocketState::TcpListen) {
        err = "not a listening socket";
        return false;
    }
#ifdef _WIN32
    SOCKET c = accept(ls.s, nullptr, nullptr);
    if (c == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
        err = "accept failed";
        return false;
    }
    KernSocketState st;
    st.s = c;
    st.kind = KernSocketState::TcpConnected;
#else
    int c = ::accept(ls.fd, nullptr, nullptr);
    if (c < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
        err = "accept failed";
        return false;
    }
    KernSocketState st;
    st.fd = c;
    st.kind = KernSocketState::TcpConnected;
#endif
    outClientId = allocId(std::move(st));
    return true;
}

bool kernTcpSend(int64_t id, const std::string& data, std::string& err) {
    KernSocketState st;
    if (!getSock(id, st, err)) return false;
    if (st.kind == KernSocketState::TcpConnecting) {
        err = "tcp connection not established (use tcp_connect_check or socket_select_write)";
        return false;
    }
    if (st.kind != KernSocketState::TcpConnected) {
        err = "not a connected TCP socket";
        return false;
    }
    size_t off = 0;
    while (off < data.size()) {
#ifdef _WIN32
        int n = send(st.s, data.data() + off, static_cast<int>(data.size() - off), 0);
#else
        ssize_t n = ::send(st.fd, data.data() + off, data.size() - off, 0);
#endif
        if (n <= 0) {
            err = "send failed";
            return false;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

bool kernTcpRecv(int64_t id, size_t maxBytes, std::string& outData, bool& eof, bool& wouldBlock, std::string& err) {
    eof = false;
    wouldBlock = false;
    outData.clear();
    KernSocketState st;
    if (!getSock(id, st, err)) return false;
    if (st.kind == KernSocketState::TcpConnecting) {
        err = "tcp connection not established (use tcp_connect_check or socket_select_write)";
        return false;
    }
    if (st.kind != KernSocketState::TcpConnected) {
        err = "not a connected TCP socket";
        return false;
    }
    if (maxBytes == 0) return true;
    std::vector<char> buf(std::min(maxBytes, size_t(1024 * 1024)));
#ifdef _WIN32
    int n = recv(st.s, buf.data(), static_cast<int>(buf.size()), 0);
#else
    ssize_t n = ::recv(st.fd, buf.data(), buf.size(), 0);
#endif
    if (n < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
#endif
        err = "recv failed";
        return false;
    }
    if (n == 0) {
        eof = true;
        return true;
    }
    outData.assign(buf.data(), static_cast<size_t>(n));
    return true;
}

bool kernSocketClose(int64_t id, std::string& err) {
    KernSocketState st;
    {
        std::lock_guard<std::mutex> lock(gSockMutex);
        auto it = gSocks.find(id);
        if (it == gSocks.end()) {
            err = "invalid socket id";
            return false;
        }
        st = std::move(it->second);
        gSocks.erase(it);
    }
#ifdef _WIN32
    closeSock(st.s);
#else
    closeFd(st.fd);
#endif
    return true;
}

bool kernUdpOpen(int64_t& outId, std::string& err) {
    outId = -1;
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        err = "udp socket() failed";
        return false;
    }
    KernSocketState st;
    st.s = s;
    st.kind = KernSocketState::Udp;
#else
    int s = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        err = "udp socket() failed";
        return false;
    }
    KernSocketState st;
    st.fd = s;
    st.kind = KernSocketState::Udp;
#endif
    outId = allocId(std::move(st));
    return true;
}

bool kernUdpBind(int64_t id, const std::string& host, int port, std::string& err) {
    if (!portOk(port)) {
        err = "invalid port";
        return false;
    }
    KernSocketState st;
    if (!getSock(id, st, err)) return false;
    if (st.kind != KernSocketState::Udp) {
        err = "not a UDP socket";
        return false;
    }
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* res = nullptr;
    std::string ps = std::to_string(port);
    const char* node = host.empty() ? nullptr : host.c_str();
    if (getaddrinfo(node, ps.c_str(), &hints, &res) != 0 || !res) {
        err = "getaddrinfo (udp bind) failed";
        return false;
    }
#ifdef _WIN32
    int br = bind(st.s, res->ai_addr, static_cast<int>(res->ai_addrlen));
#else
    int br = ::bind(st.fd, res->ai_addr, res->ai_addrlen);
#endif
    freeaddrinfo(res);
    if (br != 0) {
        err = "udp bind failed";
        return false;
    }
    return true;
}

bool kernUdpSend(int64_t id, const std::string& host, int port, const std::string& data, std::string& err) {
    if (!portOk(port)) {
        err = "invalid port";
        return false;
    }
    KernSocketState st;
    if (!getSock(id, st, err)) return false;
    if (st.kind != KernSocketState::Udp) {
        err = "not a UDP socket";
        return false;
    }
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* res = nullptr;
    std::string ps = std::to_string(port);
    if (getaddrinfo(host.empty() ? "127.0.0.1" : host.c_str(), ps.c_str(), &hints, &res) != 0 || !res) {
        err = "getaddrinfo (udp send) failed";
        return false;
    }
#ifdef _WIN32
    int n = sendto(st.s, data.data(), static_cast<int>(data.size()), 0, res->ai_addr, static_cast<int>(res->ai_addrlen));
#else
    ssize_t n = ::sendto(st.fd, data.data(), data.size(), 0, res->ai_addr, res->ai_addrlen);
#endif
    freeaddrinfo(res);
    if (n < 0 || static_cast<size_t>(n) != data.size()) {
        err = "udp sendto failed";
        return false;
    }
    return true;
}

bool kernUdpRecv(int64_t id, size_t maxBytes, std::string& outData, std::string& outHost, int& outPort, bool& wouldBlock,
                 std::string& err) {
    outData.clear();
    outHost.clear();
    outPort = 0;
    wouldBlock = false;
    KernSocketState st;
    if (!getSock(id, st, err)) return false;
    if (st.kind != KernSocketState::Udp) {
        err = "not a UDP socket";
        return false;
    }
    if (maxBytes == 0) return true;
    std::vector<char> buf(std::min(maxBytes, size_t(1024 * 1024)));
    sockaddr_storage peer{};
#ifdef _WIN32
    int peerLen = static_cast<int>(sizeof(peer));
    int n = recvfrom(st.s, buf.data(), static_cast<int>(buf.size()), 0, reinterpret_cast<sockaddr*>(&peer), &peerLen);
#else
    socklen_t peerLen = sizeof(peer);
    ssize_t n = ::recvfrom(st.fd, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr*>(&peer), &peerLen);
#endif
    if (n < 0) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wouldBlock = true;
            return true;
        }
#endif
        err = "udp recvfrom failed";
        return false;
    }
    outData.assign(buf.data(), static_cast<size_t>(n));
    char hostbuf[NI_MAXHOST] = {0};
    char servbuf[NI_MAXSERV] = {0};
#ifdef _WIN32
    socklen_t gniLen = static_cast<socklen_t>(peerLen);
#else
    socklen_t gniLen = peerLen;
#endif
    if (getnameinfo(reinterpret_cast<sockaddr*>(&peer), gniLen, hostbuf, sizeof(hostbuf), servbuf, sizeof(servbuf),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        outHost = "?";
        outPort = 0;
    } else {
        outHost = hostbuf;
        outPort = static_cast<int>(std::strtol(servbuf, nullptr, 10));
    }
    return true;
}

bool kernSocketSetNonBlocking(int64_t id, bool nonBlocking, std::string& err) {
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    std::lock_guard<std::mutex> lock(gSockMutex);
    auto it = gSocks.find(id);
    if (it == gSocks.end()) {
        err = "invalid socket id";
        return false;
    }
#ifdef _WIN32
    u_long mode = nonBlocking ? 1UL : 0UL;
    if (ioctlsocket(it->second.s, FIONBIO, &mode) != 0) {
        err = "ioctlsocket FIONBIO failed";
        return false;
    }
#else
    int fd = it->second.fd;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        err = "fcntl F_GETFL failed";
        return false;
    }
    int nflags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(fd, F_SETFL, nflags) != 0) {
        err = "fcntl O_NONBLOCK failed";
        return false;
    }
#endif
    return true;
}

bool kernSocketSelectRead(const std::vector<int64_t>& ids, int timeoutMs, std::vector<int64_t>& ready, std::string& err) {
    ready.clear();
    if (ids.empty()) return true;
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    if (ids.size() > FD_SETSIZE) {
        err = "socket_select_read: too many sockets (max FD_SETSIZE)";
        return false;
    }

    std::vector<std::pair<int64_t, KernSocketState>> snaps;
    snaps.reserve(ids.size());
    {
        std::lock_guard<std::mutex> lock(gSockMutex);
        for (int64_t id : ids) {
            auto it = gSocks.find(id);
            if (it == gSocks.end()) {
                err = "invalid socket id";
                return false;
            }
            snaps.push_back({id, it->second});
        }
    }

    fd_set readfds;
    FD_ZERO(&readfds);
#ifndef _WIN32
    int maxfd = 0;
#endif
    for (const auto& p : snaps) {
#ifdef _WIN32
        FD_SET(p.second.s, &readfds);
#else
        FD_SET(p.second.fd, &readfds);
        if (p.second.fd > maxfd) maxfd = p.second.fd;
#endif
    }

    struct timeval tv {};
    struct timeval* ptv = nullptr;
    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        ptv = &tv;
    }

#ifdef _WIN32
    int nsel = select(0, &readfds, nullptr, nullptr, ptv);
#else
    int nsel = select(maxfd + 1, &readfds, nullptr, nullptr, ptv);
#endif
    if (nsel < 0) {
        err = "select failed";
        return false;
    }
    if (nsel == 0) return true;

    for (const auto& p : snaps) {
#ifdef _WIN32
        if (FD_ISSET(p.second.s, &readfds)) ready.push_back(p.first);
#else
        if (FD_ISSET(p.second.fd, &readfds)) ready.push_back(p.first);
#endif
    }
    return true;
}

bool kernSocketSelectWrite(const std::vector<int64_t>& ids, int timeoutMs, std::vector<int64_t>& ready, std::string& err) {
    ready.clear();
    if (ids.empty()) return true;
#ifdef _WIN32
    if (!ensureWsa(err)) return false;
#endif
    if (ids.size() > FD_SETSIZE) {
        err = "socket_select_write: too many sockets (max FD_SETSIZE)";
        return false;
    }

    std::vector<std::pair<int64_t, KernSocketState>> snaps;
    snaps.reserve(ids.size());
    {
        std::lock_guard<std::mutex> lock(gSockMutex);
        for (int64_t id : ids) {
            auto it = gSocks.find(id);
            if (it == gSocks.end()) {
                err = "invalid socket id";
                return false;
            }
            snaps.push_back({id, it->second});
        }
    }

    fd_set writefds;
    FD_ZERO(&writefds);
#ifndef _WIN32
    int maxfd = 0;
#endif
    for (const auto& p : snaps) {
#ifdef _WIN32
        FD_SET(p.second.s, &writefds);
#else
        FD_SET(p.second.fd, &writefds);
        if (p.second.fd > maxfd) maxfd = p.second.fd;
#endif
    }

    struct timeval tv {};
    struct timeval* ptv = nullptr;
    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        ptv = &tv;
    }

#ifdef _WIN32
    int nsel = select(0, nullptr, &writefds, nullptr, ptv);
#else
    int nsel = select(maxfd + 1, nullptr, &writefds, nullptr, ptv);
#endif
    if (nsel < 0) {
        err = "select failed";
        return false;
    }
    if (nsel == 0) return true;

    for (const auto& p : snaps) {
#ifdef _WIN32
        if (FD_ISSET(p.second.s, &writefds)) ready.push_back(p.first);
#else
        if (FD_ISSET(p.second.fd, &writefds)) ready.push_back(p.first);
#endif
    }
    return true;
}

} // namespace kern
