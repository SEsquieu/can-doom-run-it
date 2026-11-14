// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace cdr
{
constexpr std::size_t MaxDatagramSize = 1536;
constexpr std::size_t MaxControls = 32;
constexpr std::size_t MaxAdapters = 4;
constexpr std::uint16_t DefaultOutputPort = 19793;
constexpr std::uint16_t SubscriptionPort = 19794;
enum class Control { PlayerUse }; // Original gameplay hook, now on schema 2.
enum class Primitive { Trigger, Button, Toggle, Scalar, Vector2, Selector };
enum class Update { OnChange, Continuous };
enum class Kind { Definition, Event, State, SnapshotBegin, SnapshotEnd, Lifecycle };
enum class Phase { None, Trigger, Press, Release, Change, Reset };
enum class ResetReason { FocusLost, Disabled, Shutdown, Stale, EndpointLost, Rearm, MapChange };
enum Capability : unsigned { Diagnostic = 0, SoftwareKeyboard = 1, PhysicalDevice = 2 };
struct Value { double x = 0, y = 0; };
struct Definition
{
    std::string id, label, units;
    Primitive type = Primitive::Trigger;
    double minimum = 0, maximum = 1;
    Value neutral{};
    unsigned states = 0;
    Update update = Update::OnChange;
    std::uint64_t staleTicks = 0;
};
struct State { Value value{}; std::uint64_t revision = 0, refreshed = 0; };
struct Message
{
    Kind kind = Kind::Event;
    Phase phase = Phase::None;
    std::string_view session, reason;
    std::uint64_t sequence = 0, tick = 0, snapshot = 0, revision = 0;
    unsigned control = 0, player = 0, count = 0;
    Value value{};
};
struct Counters { std::uint64_t emitted = 0, submitted = 0, dropped = 0, messages = 0; };
const char* Name(Primitive type);
const char* Name(Kind kind);
const char* Name(Phase phase);
const char* Name(ResetReason reason);
std::string NewSession();
// Bounded, locale-independent, allocation-free wire serialization.
std::size_t Encode(const Message&, const Definition*, char* bytes, std::size_t capacity) noexcept;
class Adapter
{
public:
    virtual ~Adapter() = default;
    virtual const char* Id() const noexcept = 0;
    virtual unsigned RequiredCapabilities() const noexcept { return Diagnostic; }
    virtual bool IsReady() const noexcept = 0;
    virtual bool Send(const Message&, const Definition*) noexcept = 0;
    virtual void Tick(std::uint64_t) noexcept {}
    virtual void Reset(ResetReason) noexcept {}
};
// Simulation-thread owned. Registration happens during map load. Adapters may
// borrow message references only during Send; async adapters copy into bounded
// mailboxes without waiting. No protocol or endpoint details enter gameplay.
class Bus
{
public:
    using Sink = std::function<bool(std::string_view)>;
    explicit Bus(Sink sink = {}, std::string session = NewSession());
    bool Define(const Definition&);
    int Find(std::string_view id) const noexcept;
    const Definition* GetDefinition(unsigned slot) const noexcept;
    const State* GetState(unsigned slot) const noexcept;
    unsigned Count() const noexcept { return count_; }
    const std::string& Session() const noexcept { return session_; }
    bool Publish(std::string_view id, Value value, std::uint64_t tick, unsigned player = 0);
    bool Trigger(Control control, std::uint64_t tick, std::uint32_t player);
    bool Attach(Adapter& adapter);
    void Snapshot(std::uint64_t tick);
    void Tick(std::uint64_t tick);
    void Reset(ResetReason reason, std::uint64_t tick);
    void SetEnabled(bool enabled, std::uint64_t tick);
    void SetFocused(bool focused, std::uint64_t tick);
    // Map bindings cannot grant capabilities. Physical grants are rejected;
    // only the native user-gesture path grants software keyboard authority.
    bool Grant(unsigned capability, bool granted, std::uint64_t tick);
    bool Enabled() const noexcept { return enabled_; }
    bool Focused() const noexcept { return focused_; }
    unsigned Grants() const noexcept { return grants_; }
    const Counters& GetCounters() const noexcept { return counters_; }
private:
    bool Emit(Message message, const Definition* definition);
    bool Change(unsigned slot, Value value, std::uint64_t tick, unsigned player, Phase phase, std::string_view reason = {});
    bool Allowed(const Adapter&) const noexcept;
    Sink sink_;
    std::string session_;
    std::array<Definition, MaxControls> definitions_{};
    std::array<State, MaxControls> states_{};
    std::array<Adapter*, MaxAdapters> adapters_{};
    unsigned count_ = 0, adapterCount_ = 0, grants_ = 0;
    std::uint64_t sequence_ = 0, snapshot_ = 0, currentSnapshot_ = 0, lastSnapshot_ = 0;
    bool enabled_ = true, focused_ = true;
    Counters counters_;
};
}
