/* *
 * Cross-platform TCP/UDP sockets for builtins (tcp_connect, udp_recv, ...).
 * Default blocking I/O; use kernSocketSetNonBlocking + kernSocketSelectRead for polling.
 */
#ifndef KERN_KERN_SOCKET_HPP
#define KERN_KERN_SOCKET_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace kern {

/** Initialize Winsock once on Windows; no-op elsewhere. */
void kernSocketInit();

/** All functions return human-readable English errors in *err when returning false. */

bool kernTcpConnect(const std::string& host, int port, int64_t& outId, std::string& err);
/** Non-blocking connect: socket is created non-blocking; may complete immediately (`instant_connected`) or enter `TcpConnecting`. */
bool kernTcpConnectStart(const std::string& host, int port, int64_t& outId, bool& instantConnected, std::string& err);
/** Poll a pending connect: sets `connected` when handshake done, `pending` if still in progress. On failure, socket is removed. */
bool kernTcpConnectCheck(int64_t id, bool& connected, bool& pending, std::string& err);
bool kernTcpListen(const std::string& bindHost, int port, int backlog, int64_t& outId, std::string& err);
bool kernTcpAccept(int64_t listenId, int64_t& outClientId, bool& wouldBlock, std::string& err);
bool kernTcpSend(int64_t id, const std::string& data, std::string& err);
bool kernTcpRecv(int64_t id, size_t maxBytes, std::string& outData, bool& eof, bool& wouldBlock, std::string& err);
bool kernSocketClose(int64_t id, std::string& err);

bool kernUdpOpen(int64_t& outId, std::string& err);
bool kernUdpBind(int64_t id, const std::string& host, int port, std::string& err);
bool kernUdpSend(int64_t id, const std::string& host, int port, const std::string& data, std::string& err);
bool kernUdpRecv(int64_t id, size_t maxBytes, std::string& outData, std::string& outHost, int& outPort, bool& wouldBlock,
                 std::string& err);

/** Non-blocking mode (per socket). */
bool kernSocketSetNonBlocking(int64_t id, bool nonBlocking, std::string& err);

/** Wait until one or more sockets are readable. `timeout_ms` < 0 = block forever; 0 = poll.
 *  At most FD_SETSIZE sockets (typically 64 on Windows). */
bool kernSocketSelectRead(const std::vector<int64_t>& ids, int timeoutMs, std::vector<int64_t>& ready, std::string& err);

/** Wait until one or more sockets are writable (e.g. async `tcp_connect_start` completion). */
bool kernSocketSelectWrite(const std::vector<int64_t>& ids, int timeoutMs, std::vector<int64_t>& ready, std::string& err);

} // namespace kern

#endif
