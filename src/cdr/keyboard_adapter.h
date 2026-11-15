// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "control_bus.h"
#include <atomic>
#include <memory>
#include <thread>
namespace cdr
{
enum class Key : unsigned { Left,Right,Up,Down,Space,Enter,Shift,W,S,One,Two,Three,B,Count };
constexpr unsigned KeyCount=unsigned(Key::Count);
class KeyEndpoint
{
public:
    virtual ~KeyEndpoint()=default;
    virtual bool Connect() noexcept=0;
    virtual bool Ready() const noexcept=0;
    virtual bool Set(Key key,bool down) noexcept=0;
    virtual void Poll() noexcept=0;
};
std::unique_ptr<KeyEndpoint> NativeKeyboardEndpoint();
bool LaunchInputLab(const std::string& executable);
const char* KeyboardProfileId() noexcept;
bool KeyboardProfileCompatible(const Bus& bus) noexcept;

// A single-producer/single-consumer queue. The game thread never opens a display,
// waits for a window, or calls external input APIs. Reset bypasses the queue via
// an epoch so saturation cannot hide release/disable/shutdown.
class KeyboardAdapter final : public Adapter
{
public:
    explicit KeyboardAdapter(std::unique_ptr<KeyEndpoint> endpoint);
    ~KeyboardAdapter();
    const char* Id() const noexcept override { return "keyboard.targeted"; }
    unsigned RequiredCapabilities() const noexcept override { return SoftwareKeyboard; }
    bool IsReady() const noexcept override { return connected_.load() && !fault_.load() && !stop_.load(); }
    bool Available() const noexcept { return connected_.load() && !stop_.load(); }
    bool PrepareArm() noexcept;
    bool Send(const Message&,const Definition*) noexcept override;
    void Tick(std::uint64_t) noexcept override;
    void Reset(ResetReason) noexcept override;
    void Stop() noexcept;
    std::uint64_t Dropped() const noexcept { return dropped_.load(); }
    const char* Status() const noexcept;
private:
    struct Action { unsigned epoch=0, mask=0; Key key=Key::Enter; bool pulse=false; };
    static constexpr unsigned QueueSize=64;
    bool Push(Action action) noexcept;
    void ApplyValues() noexcept;
    void Work() noexcept;
    std::unique_ptr<KeyEndpoint> endpoint_;
    std::thread worker_;
    std::array<Action,QueueSize> queue_{};
    std::atomic<unsigned> head_{0},tail_{0},epoch_{1};
    std::atomic<bool> stop_{false},connected_{false},fault_{false};
    std::atomic<std::uint64_t> heartbeat_{0},dropped_{0};
    std::array<Value,7> values_{},pending_{};
    unsigned desired_=0;
    int selector_=-1;
    bool snapshotting_=false;
};
}
