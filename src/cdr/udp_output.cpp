// SPDX-License-Identifier: GPL-3.0-or-later
#include "udp_output.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cdr
{
LoopbackUdpOutput::LoopbackUdpOutput(std::uint16_t port) : port_(port)
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return;
    winsockStarted_ = true;
    const SOCKET descriptor = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor == INVALID_SOCKET) return;
    u_long nonblocking = 1;
    if (ioctlsocket(descriptor, FIONBIO, &nonblocking) != 0)
    {
        closesocket(descriptor);
        return;
    }
#else
    const int descriptor = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor < 0) return;
    const int flags = fcntl(descriptor, F_GETFL, 0);
    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0 ||
        fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0)
    {
        close(descriptor);
        return;
    }
#endif
    socket_ = static_cast<std::uintptr_t>(descriptor);
    ready_ = true;
}

LoopbackUdpOutput::~LoopbackUdpOutput()
{
#ifdef _WIN32
    if (ready_) closesocket(static_cast<SOCKET>(socket_));
    if (winsockStarted_) WSACleanup();
#else
    if (ready_) close(static_cast<int>(socket_));
#endif
}

bool LoopbackUdpOutput::Send(std::string_view event) const noexcept
{
    if (!ready_ || event.empty() || event.size() > MaxDatagramSize) return false;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    endpoint.sin_port = htons(port_);
#ifdef _WIN32
    const auto sent = sendto(static_cast<SOCKET>(socket_), event.data(),
        static_cast<int>(event.size()), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
#else
    const auto sent = sendto(static_cast<int>(socket_), event.data(), event.size(), 0,
        reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
#endif
    return sent >= 0 && static_cast<std::size_t>(sent) == event.size();
}
}
