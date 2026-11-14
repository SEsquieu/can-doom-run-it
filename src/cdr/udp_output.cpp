// SPDX-License-Identifier: GPL-3.0-or-later
#include "udp_output.h"
#include <array>
#include <cstring>

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
LoopbackUdpOutput::LoopbackUdpOutput(std::uint16_t port, bool subscriptions) : port_(port)
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
    if (!subscriptions) return;
#ifdef _WIN32
    const SOCKET sub=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (sub==INVALID_SOCKET) return;
    u_long mode=1;
    if (ioctlsocket(sub,FIONBIO,&mode)!=0) { closesocket(sub); return; }
#else
    const int sub=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (sub<0) return;
    if (fcntl(sub,F_SETFL,O_NONBLOCK)<0 || fcntl(sub,F_SETFD,FD_CLOEXEC)<0) { close(sub); return; }
#endif
    sockaddr_in address{}; address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(SubscriptionPort);
    if (bind(sub,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0)
    {
#ifdef _WIN32
        closesocket(sub);
#else
        close(sub);
#endif
        return;
    }
    subscriptionSocket_=static_cast<std::uintptr_t>(sub); subscriptionReady_=true;
}

LoopbackUdpOutput::~LoopbackUdpOutput()
{
#ifdef _WIN32
    if (subscriptionReady_) closesocket(static_cast<SOCKET>(subscriptionSocket_));
    if (ready_) closesocket(static_cast<SOCKET>(socket_));
    if (winsockStarted_) WSACleanup();
#else
    if (subscriptionReady_) close(static_cast<int>(subscriptionSocket_));
    if (ready_) close(static_cast<int>(socket_));
#endif
}

bool LoopbackUdpOutput::Send(const Message& message,const Definition* definition) noexcept
{
    std::array<char,MaxDatagramSize> bytes{};
    const auto size=Encode(message,definition,bytes.data(),bytes.size());
    return size && Send(std::string_view(bytes.data(),size));
}

void LoopbackUdpOutput::Tick(std::uint64_t tick) noexcept
{
    if (!subscriptionReady_) return;
    for (unsigned i=0;i<4;++i)
    {
        char bytes[32]{};
#ifdef _WIN32
        const int size=recv(static_cast<SOCKET>(subscriptionSocket_),bytes,sizeof(bytes),0);
#else
        const auto size=recv(static_cast<int>(subscriptionSocket_),bytes,sizeof(bytes),0);
#endif
        if (size<=0) break;
        if (std::string_view(bytes,std::size_t(size))=="DIO/2 SNAPSHOT" && (lastRequest_==0 || tick-lastRequest_>=7))
        { snapshotRequested_=true; lastRequest_=tick; }
    }
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
