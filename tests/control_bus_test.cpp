// SPDX-License-Identifier: GPL-3.0-or-later
#include "cdr/control_bus.h"
#include "cdr/udp_output.h"

#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

// These checks remain active in Release builds.
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (false)

int main()
{
    std::vector<std::string> packets;
    bool accept = true;
    cdr::Bus bus([&](std::string_view packet) {
        packets.emplace_back(packet);
        return accept;
    }, "test-session");
    CHECK(bus.Trigger(cdr::Control::PlayerUse, 70, 0));
    CHECK(packets.back() == "{\"schema\":2,\"session\":\"test-session\",\"sequence\":1,\"tick\":70,"
        "\"kind\":\"event\",\"snapshot\":0,\"control\":\"player.use\",\"type\":\"trigger\",\"player\":0,"
        "\"phase\":\"trigger\",\"value\":[1,0],\"revision\":1}");
    accept = false;
    CHECK(!bus.Trigger(cdr::Control::PlayerUse, 80, 0));
    accept = true;
    CHECK(bus.Trigger(cdr::Control::PlayerUse, 90, 0));
    CHECK(packets.back().find("\"sequence\":3") != std::string::npos);
    CHECK(bus.GetCounters().emitted == 3);
    CHECK(bus.GetCounters().submitted == 2);
    CHECK(bus.GetCounters().dropped == 1);
    CHECK(!bus.Trigger(static_cast<cdr::Control>(999), 90, 0));
    CHECK(bus.GetCounters().emitted == 3);
    CHECK(bus.Trigger(cdr::Control::PlayerUse, std::numeric_limits<std::uint64_t>::max(),
        std::numeric_limits<std::uint32_t>::max()));
    CHECK(packets.back().find("18446744073709551615") != std::string::npos);
    CHECK(packets.back().size() < cdr::MaxDatagramSize);
    cdr::Bus missingSink({});
    CHECK(!missingSink.Trigger(cdr::Control::PlayerUse, 0, 0));
    CHECK(missingSink.GetCounters().dropped == 1);

    // Real transport round trip on an ephemeral loopback port, on both OSes.
    // Construct output first so Winsock is initialized before creating a receiver.
    cdr::LoopbackUdpOutput startup;
    CHECK(startup.IsReady());
#ifdef _WIN32
    const SOCKET receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(receiver != INVALID_SOCKET);
    DWORD timeout = 2000;
    CHECK(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0);
    int endpointSize = sizeof(sockaddr_in);
#else
    const int receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(receiver >= 0);
    timeval timeout{2, 0};
    CHECK(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);
    socklen_t endpointSize = sizeof(sockaddr_in);
#endif
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(receiver, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint)) == 0);
    CHECK(getsockname(receiver, reinterpret_cast<sockaddr*>(&endpoint), &endpointSize) == 0);
    cdr::LoopbackUdpOutput output(ntohs(endpoint.sin_port));
    CHECK(output.IsReady());
    CHECK(!output.Send(std::string(cdr::MaxDatagramSize + 1, 'x')));
    CHECK(!output.Send({}));
    cdr::Bus transport([&](std::string_view event) { return output.Send(event); });
    CHECK(transport.Trigger(cdr::Control::PlayerUse, 42, 0));
    char bytes[cdr::MaxDatagramSize]{};
    const auto size = recv(receiver, bytes, sizeof(bytes), 0);
    CHECK(size > 0);
    CHECK(std::string(bytes, static_cast<std::size_t>(size)).find("\"tick\":42") != std::string::npos);
#ifdef _WIN32
    closesocket(receiver);
#else
    close(receiver);
#endif
    std::puts("PASS: native serialization, bounds, sequence gaps, failure counters, UDP round trip");
    return 0;
}
