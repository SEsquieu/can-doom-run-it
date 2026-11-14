// SPDX-License-Identifier: GPL-3.0-or-later
#include "cdr/keyboard_adapter.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
using namespace cdr;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(false)
struct Observed { std::atomic<unsigned> held{0},pulses{0},presses{0}; std::atomic<bool> ready{true},blocked{false}; };
class Endpoint:public KeyEndpoint
{
    Observed& value_;
public:
    explicit Endpoint(Observed& value):value_(value) {}
    bool Connect() noexcept override { return value_.ready; }
    bool Ready() const noexcept override { return value_.ready; }
    bool Set(Key key,bool down) noexcept override
    {
        const unsigned bit=1u<<unsigned(key);
        if (down) { value_.held.fetch_or(bit); if (key==Key::Enter) ++value_.pulses; if (key==Key::Space) ++value_.presses; }
        else value_.held.fetch_and(~bit);
        return value_.ready;
    }
    void Poll() noexcept override { while (value_.blocked) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
};
template<class F> bool Wait(F predicate)
{
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (!predicate() && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return predicate();
}
void DefineProfile(Bus& bus)
{
    bus.Define({"trigger.pulse","Pulse","",Primitive::Trigger});
    bus.Define({"button.a","A","",Primitive::Button,0,1,{},0,Update::OnChange,14});
    bus.Define({"button.b","B","",Primitive::Button,0,1,{},0,Update::OnChange,14});
    bus.Define({"toggle.power","Power","",Primitive::Toggle});
    bus.Define({"scalar.throttle","Throttle","",Primitive::Scalar,0,1,{0.5,0},0,Update::Continuous,14});
    bus.Define({"vector.pad","XY","",Primitive::Vector2,-1,1,{},0,Update::Continuous,14});
    bus.Define({"selector.mode","Mode","",Primitive::Selector,0,2,{},3});
}
int main()
{
    Observed observed;
    KeyboardAdapter adapter(std::make_unique<Endpoint>(observed));
    CHECK(Wait([&]{return adapter.IsReady();}));
    Bus bus; DefineProfile(bus); CHECK(KeyboardProfileCompatible(bus)); CHECK(bus.Attach(adapter));
    bus.Publish("button.a",{1,0},1); std::this_thread::sleep_for(std::chrono::milliseconds(20)); CHECK(observed.held==0);
    CHECK(adapter.PrepareArm()); CHECK(bus.Grant(SoftwareKeyboard,true,2));
    CHECK(bus.Publish("button.a",{1,0},3)); CHECK(Wait([&]{return (observed.held.load()&(1u<<unsigned(Key::Space)))!=0;}));
    CHECK(bus.Publish("button.a",{1,0},4)); CHECK(observed.presses==1);
    CHECK(bus.Publish("trigger.pulse",{},5)); CHECK(Wait([&]{return observed.pulses==1;}));
    bus.Snapshot(6); std::this_thread::sleep_for(std::chrono::milliseconds(20)); CHECK(observed.pulses==1);
    bus.SetFocused(false,7); CHECK(Wait([&]{return observed.held==0;})); CHECK(bus.Grants()==0);
    bus.SetFocused(true,8); CHECK(adapter.PrepareArm()); CHECK(bus.Grant(SoftwareKeyboard,true,8));
    bus.Publish("toggle.power",{1,0},9); CHECK(Wait([&]{return observed.held!=0;}));
    CHECK(Wait([&]{return !adapter.IsReady();})); CHECK(observed.held==0); // Wall-clock watchdog, no game heartbeat.
    CHECK(adapter.PrepareArm()); CHECK(bus.Grant(SoftwareKeyboard,true,10));
    observed.blocked=true; std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (unsigned i=0;i<200;++i) bus.Publish("trigger.pulse",{},11);
    CHECK(adapter.Dropped()>0); CHECK(!adapter.IsReady()); observed.blocked=false;
    CHECK(Wait([&]{return observed.held==0;}));
    adapter.Reset(ResetReason::EndpointLost); observed.ready=false;
    CHECK(Wait([&]{return !adapter.Available();}));
    adapter.Stop(); CHECK(observed.held==0);
    std::puts("PASS: authorization, press/hold, snapshots without replay, focus release, watchdog, bounded overflow, endpoint loss, shutdown");
}
