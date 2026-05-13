#pragma once

#include <cstdint>
#include <string>

namespace fw::network {

class TcpSocket {
public:
    TcpSocket();
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    bool connect(const std::string& host, uint16_t port, int timeoutMs);
    bool sendAll(const std::string& bytes);
    std::string receiveAll();
    void close();

private:
    uintptr_t fd_{static_cast<uintptr_t>(-1)};
    bool isValid() const noexcept { return fd_ != static_cast<uintptr_t>(-1); }
};

} // namespace fw::network

