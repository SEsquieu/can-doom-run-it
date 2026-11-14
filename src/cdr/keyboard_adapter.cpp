// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard_adapter.h"
#include <chrono>
#include <algorithm>
#include <utility>
namespace cdr
{
namespace
{
constexpr const char* ids[]={"trigger.pulse","button.a","button.b","toggle.power","scalar.throttle","vector.pad","selector.mode"};
constexpr Primitive types[]={Primitive::Trigger,Primitive::Button,Primitive::Button,Primitive::Toggle,Primitive::Scalar,Primitive::Vector2,Primitive::Selector};
std::uint64_t Now() { return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
unsigned Bit(Key key) { return 1u<<unsigned(key); }
double Normalized(double value,double neutral,double minimum,double maximum)
{
    if (value>neutral && maximum>neutral) return std::clamp((value-neutral)/(maximum-neutral),0.,1.);
    if (value<neutral && minimum<neutral) return std::clamp((value-neutral)/(neutral-minimum),-1.,0.);
    return 0;
}
}
const char* KeyboardProfileId() noexcept { return "keyboard.lab"; }
bool KeyboardProfileCompatible(const Bus& bus) noexcept
{
    for (unsigned i=0;i<7;++i) { auto d=bus.GetDefinition(unsigned(bus.Find(ids[i]))); if (!d || d->type!=types[i] || (i==6 && d->states!=3)) return false; }
    return true;
}
KeyboardAdapter::KeyboardAdapter(std::unique_ptr<KeyEndpoint> endpoint):endpoint_(std::move(endpoint))
{
    values_[4]={0.5,0}; pending_=values_; heartbeat_=Now();
    worker_=std::thread([this]{Work();});
}
KeyboardAdapter::~KeyboardAdapter() { Stop(); }
void KeyboardAdapter::Stop() noexcept { stop_=true; epoch_.fetch_add(1); if (worker_.joinable()) worker_.join(); }
const char* KeyboardAdapter::Status() const noexcept
{
    if (stop_) return "STOPPED";
    if (fault_) return "RELEASED / REARM REQUIRED";
    return connected_ ? "ENDPOINT READY" : "WAITING FOR INPUT LAB";
}
bool KeyboardAdapter::PrepareArm() noexcept
{
    if (!Available()) return false;
    Reset(ResetReason::Rearm); fault_=false; heartbeat_=Now(); return true;
}
void KeyboardAdapter::Reset(ResetReason) noexcept
{
    values_={}; values_[4]={0.5,0}; pending_=values_; desired_=0; selector_=-1; snapshotting_=false;
    epoch_.fetch_add(1,std::memory_order_acq_rel);
}
void KeyboardAdapter::Tick(std::uint64_t) noexcept { heartbeat_.store(Now(),std::memory_order_release); }
bool KeyboardAdapter::Push(Action action) noexcept
{
    if (!IsReady()) return false;
    const unsigned head=head_.load(std::memory_order_relaxed),next=(head+1)%QueueSize;
    if (next==tail_.load(std::memory_order_acquire))
    {
        ++dropped_; fault_=true; epoch_.fetch_add(1); return false;
    }
    action.epoch=epoch_.load(std::memory_order_acquire); queue_[head]=action;
    head_.store(next,std::memory_order_release); return true;
}
void KeyboardAdapter::ApplyValues() noexcept
{
    unsigned mask=0;
    if (values_[1].x) mask|=Bit(Key::Space);
    if (values_[2].x) mask|=Bit(Key::B);
    if (values_[3].x) mask|=Bit(Key::Shift);
    if (values_[4].x>0.6) mask|=Bit(Key::W); else if (values_[4].x<0.4) mask|=Bit(Key::S);
    if (values_[5].x>0.25) mask|=Bit(Key::Right); else if (values_[5].x< -0.25) mask|=Bit(Key::Left);
    if (values_[5].y>0.25) mask|=Bit(Key::Up); else if (values_[5].y< -0.25) mask|=Bit(Key::Down);
    if (mask!=desired_) { desired_=mask; Push({0,mask,Key::Enter,false}); }
    int selection=int(values_[6].x);
    if (selection>=0 && selection<3 && selection!=selector_)
    { selector_=selection; Push({0,0,Key(unsigned(Key::One)+unsigned(selection)),true}); }
}
bool KeyboardAdapter::Send(const Message& m,const Definition* d) noexcept
{
    if (!IsReady()) return false;
    if (m.kind==Kind::SnapshotBegin) { snapshotting_=true; pending_=values_; return true; }
    if (m.kind==Kind::SnapshotEnd) { if (snapshotting_) { values_=pending_; snapshotting_=false; ApplyValues(); } return true; }
    if (!d || (m.kind!=Kind::Event && m.kind!=Kind::State)) return true;
    for (unsigned i=0;i<7;++i)
    {
        if (d->id!=ids[i] || d->type!=types[i]) continue;
        if (i==0) { if (m.kind==Kind::Event && m.phase==Phase::Trigger) return Push({0,0,Key::Enter,true}); return true; }
        Value value=m.value;
        if (i==4) value.x=0.5+0.5*Normalized(value.x,d->neutral.x,d->minimum,d->maximum);
        if (i==5) { value.x=Normalized(value.x,d->neutral.x,d->minimum,d->maximum); value.y=Normalized(value.y,d->neutral.y,d->minimum,d->maximum); }
        if (snapshotting_ && m.kind==Kind::State) pending_[i]=value;
        else { values_[i]=value; ApplyValues(); }
        return !fault_;
    }
    return true;
}
void KeyboardAdapter::Work() noexcept
{
    unsigned seenEpoch=epoch_.load(),held=0;
    bool selectorOwned=false;
    auto release=[&] {
        if (endpoint_->Ready())
        {
            for (unsigned i=0;i<KeyCount;++i) if (held&(1u<<i)) endpoint_->Set(Key(i),false);
            if (selectorOwned) { endpoint_->Set(Key::One,true); endpoint_->Set(Key::One,false); }
        }
        held=0; selectorOwned=false;
    };
    auto tap=[&](Key key) {
        if (!endpoint_->Set(key,true)) return false;
        held|=Bit(key);
        if (!endpoint_->Set(key,false)) return false;
        held&=~Bit(key);
        if (key>=Key::One && key<=Key::Three) selectorOwned=true;
        return true;
    };
    std::uint64_t lastConnect=0;
    while (!stop_.load())
    {
        const auto now=Now();
        endpoint_->Poll();
        if (!connected_.load() || !endpoint_->Ready())
        {
            if (connected_.exchange(false)) { release(); fault_=true; }
            if (now-lastConnect>=250) { lastConnect=now; connected_=endpoint_->Connect(); }
        }
        unsigned currentEpoch=epoch_.load(std::memory_order_acquire);
        if (currentEpoch!=seenEpoch) { release(); seenEpoch=currentEpoch; }
        if ((held || selectorOwned) && now-heartbeat_.load(std::memory_order_acquire)>350)
        { release(); fault_=true; epoch_.fetch_add(1); }
        unsigned tail=tail_.load(std::memory_order_relaxed);
        while (tail!=head_.load(std::memory_order_acquire))
        {
            const Action action=queue_[tail]; tail=(tail+1)%QueueSize; tail_.store(tail,std::memory_order_release);
            if (action.epoch!=epoch_.load(std::memory_order_acquire) || fault_ || !endpoint_->Ready()) continue;
            bool good=true;
            if (action.pulse) good=tap(action.key);
            else for (unsigned i=0;i<KeyCount;++i)
            {
                const unsigned bit=1u<<i;
                if ((held&bit)!=(action.mask&bit))
                {
                    if (!endpoint_->Set(Key(i),(action.mask&bit)!=0)) { good=false; break; }
                    if (action.mask&bit) held|=bit; else held&=~bit;
                }
            }
            if (!good) { release(); connected_=false; fault_=true; epoch_.fetch_add(1); }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
    release(); connected_=false;
}
}
