// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_bus.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <utility>

namespace cdr
{
const char* Name(Primitive t) { static const char* n[] = {"trigger","button","toggle","scalar","vector2","selector"}; return n[unsigned(t)<6 ? unsigned(t) : 0]; }
const char* Name(Kind k) { static const char* n[] = {"definition","event","state","snapshot_begin","snapshot_end","lifecycle"}; return n[unsigned(k)<6 ? unsigned(k) : 0]; }
const char* Name(Phase p) { static const char* n[] = {"none","trigger","press","release","change","reset"}; return n[unsigned(p)<6 ? unsigned(p) : 0]; }
const char* Name(ResetReason r) { static const char* n[] = {"focus_lost","disabled","shutdown","stale","endpoint_lost","rearm","map_change"}; return n[unsigned(r)<7 ? unsigned(r) : 0]; }
std::string NewSession()
{
    // Identity, not an authentication token. Initialized once outside ticks.
    static std::atomic<std::uint64_t> nonce{0};
    std::uint64_t value = std::uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^ ++nonce;
    try { std::random_device random; value ^= (std::uint64_t(random())<<32) ^ random(); } catch (...) {}
    char bytes[17]{}; auto result=std::to_chars(bytes,bytes+16,value,16);
    return std::string(bytes,result.ptr);
}
namespace
{
class Writer
{
    char* bytes_; std::size_t capacity_,size_=0; bool good_=true;
public:
    Writer(char* bytes,std::size_t capacity):bytes_(bytes),capacity_(capacity) {}
    void Add(std::string_view text) noexcept
    {
        if (!good_ || text.size()>capacity_-size_) { good_=false; return; }
        if (!text.empty()) std::memcpy(bytes_+size_,text.data(),text.size());
        size_+=text.size();
    }
    void Quote(std::string_view text) noexcept
    {
        Add("\""); for (char c:text) { if (c=='\\' || c=='"') Add("\\"); Add({&c,1}); } Add("\"");
    }
    void Number(std::uint64_t value) noexcept { char text[24]; auto r=std::to_chars(text,text+sizeof(text),value); if (r.ec!=std::errc{}) good_=false; else Add({text,std::size_t(r.ptr-text)}); }
    void Number(double value) noexcept { char text[32]; auto r=std::to_chars(text,text+sizeof(text),value,std::chars_format::general,9); if (r.ec!=std::errc{}) good_=false; else Add({text,std::size_t(r.ptr-text)}); }
    void Pair(Value value) noexcept { Add("["); Number(value.x); Add(","); Number(value.y); Add("]"); }
    std::size_t Size() const noexcept { return good_ ? size_ : 0; }
};
bool ValidText(std::string_view text,std::size_t maximum,bool identifier)
{
    if (text.size()>maximum || (identifier && text.empty())) return false;
    for (unsigned char c:text)
    {
        if (identifier) { if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return false; }
        else if (c<32 || c>126) return false;
    }
    return true;
}
bool Same(Value a,Value b) { return std::abs(a.x-b.x)<0.00001 && std::abs(a.y-b.y)<0.00001; }
}
std::size_t Encode(const Message& m,const Definition* d,char* bytes,std::size_t capacity) noexcept
{
    Writer w(bytes,capacity); w.Add("{\"schema\":2,\"session\":"); w.Quote(m.session);
    w.Add(",\"sequence\":"); w.Number(m.sequence); w.Add(",\"tick\":"); w.Number(m.tick);
    w.Add(",\"kind\":"); w.Quote(Name(m.kind)); w.Add(",\"snapshot\":"); w.Number(m.snapshot);
    if (d)
    {
        w.Add(",\"control\":"); w.Quote(d->id); w.Add(",\"type\":"); w.Quote(Name(d->type));
        w.Add(",\"player\":"); w.Number(std::uint64_t(m.player)); w.Add(",\"phase\":"); w.Quote(Name(m.phase));
        w.Add(",\"value\":"); w.Pair(m.value); w.Add(",\"revision\":"); w.Number(m.revision);
        if (m.kind==Kind::Definition)
        {
            w.Add(",\"label\":"); w.Quote(d->label); w.Add(",\"units\":"); w.Quote(d->units);
            w.Add(",\"minimum\":"); w.Number(d->minimum); w.Add(",\"maximum\":"); w.Number(d->maximum);
            w.Add(",\"neutral\":"); w.Pair(d->neutral); w.Add(",\"states\":"); w.Number(std::uint64_t(d->states));
            w.Add(",\"update\":"); w.Quote(d->update==Update::Continuous ? "continuous" : "on_change");
            w.Add(",\"stale_ticks\":"); w.Number(d->staleTicks);
        }
    }
    else { w.Add(",\"control_count\":"); w.Number(std::uint64_t(m.count)); }
    if (!m.reason.empty()) { w.Add(",\"reason\":"); w.Quote(m.reason); }
    w.Add("}"); return w.Size();
}
Bus::Bus(Sink sink,std::string session):sink_(std::move(sink)),session_(std::move(session))
{
    if (!ValidText(session_,32,true)) session_=NewSession();
    Define({"player.use","Player Use","",Primitive::Trigger});
}
int Bus::Find(std::string_view id) const noexcept { for (unsigned i=0;i<count_;++i) if (definitions_[i].id==id) return int(i); return -1; }
const Definition* Bus::GetDefinition(unsigned slot) const noexcept { return slot<count_ ? &definitions_[slot] : nullptr; }
const State* Bus::GetState(unsigned slot) const noexcept { return slot<count_ ? &states_[slot] : nullptr; }
bool Bus::Define(const Definition& d)
{
    if (!ValidText(d.id,48,true) || !ValidText(d.label,64,false) || !ValidText(d.units,24,false) || unsigned(d.type)>5 ||
        !std::isfinite(d.minimum) || !std::isfinite(d.maximum) || d.minimum>=d.maximum ||
        std::abs(d.minimum)>1e6 || std::abs(d.maximum)>1e6 || !std::isfinite(d.neutral.x) || !std::isfinite(d.neutral.y) ||
        d.neutral.x<d.minimum || d.neutral.x>d.maximum || d.staleTicks>3500 || unsigned(d.update)>1) return false;
    if (d.type!=Primitive::Vector2 && d.neutral.y!=0) return false;
    if (d.type==Primitive::Trigger && (d.minimum!=0 || d.maximum!=1 || d.neutral.x!=0)) return false;
    if ((d.type==Primitive::Button || d.type==Primitive::Toggle) && (d.minimum!=0 || d.maximum!=1 || d.neutral.x!=0)) return false;
    if (d.type==Primitive::Vector2 && (d.neutral.y<d.minimum || d.neutral.y>d.maximum)) return false;
    if (d.type==Primitive::Selector && (d.states<2 || d.states>16 || d.minimum!=0 || d.maximum!=d.states-1 || std::floor(d.neutral.x)!=d.neutral.x)) return false;
    const int existing=Find(d.id);
    if (existing>=0)
    {
        const auto& old=definitions_[unsigned(existing)];
        return old.type==d.type && old.minimum==d.minimum && old.maximum==d.maximum && Same(old.neutral,d.neutral) &&
            old.states==d.states && old.staleTicks==d.staleTicks && old.units==d.units && old.update==d.update && old.label==d.label;
    }
    if (count_==MaxControls) return false;
    definitions_[count_]=d; states_[count_].value=d.neutral; const unsigned slot=count_++;
    if (adapterCount_) { Message m; m.kind=Kind::Definition; m.control=slot; m.value=d.neutral; Emit(m,&definitions_[slot]); }
    return true;
}
bool Bus::Allowed(const Adapter& a) const noexcept
{
    const unsigned required=a.RequiredCapabilities(); return !(required&PhysicalDevice) && (required&grants_)==required;
}
bool Bus::Emit(Message m,const Definition* definition)
{
    m.session=session_; m.sequence=++sequence_; m.snapshot=currentSnapshot_; ++counters_.messages;
    bool submitted=false;
    if (sink_)
    {
        std::array<char,MaxDatagramSize> bytes{}; const auto size=Encode(m,definition,bytes.data(),bytes.size());
        submitted=size && sink_({bytes.data(),size});
    }
    for (unsigned i=0;i<adapterCount_;++i) if (Allowed(*adapters_[i]) && adapters_[i]->IsReady()) submitted=adapters_[i]->Send(m,definition) || submitted;
    if (m.kind==Kind::Event) { if (submitted) ++counters_.submitted; else ++counters_.dropped; }
    return submitted;
}
bool Bus::Change(unsigned slot,Value value,std::uint64_t tick,unsigned player,Phase phase,std::string_view reason)
{
    auto& state=states_[slot]; state.value=value; state.refreshed=tick; ++state.revision;
    Message m; m.kind=Kind::Event; m.phase=phase; m.control=slot; m.value=value; m.revision=state.revision; m.tick=tick; m.player=player; m.reason=reason;
    if (phase==Phase::Trigger) m.value={1,0}; // An event, never a latched trigger state.
    ++counters_.emitted; return Emit(m,&definitions_[slot]);
}
bool Bus::Publish(std::string_view id,Value value,std::uint64_t tick,unsigned player)
{
    const int found=Find(id);
    if (!enabled_ || !focused_ || found<0 || !std::isfinite(value.x) || !std::isfinite(value.y)) return false;
    const unsigned slot=unsigned(found); const auto& d=definitions_[slot]; auto& state=states_[slot]; Phase phase=Phase::Change;
    if (d.type==Primitive::Trigger) return Change(slot,{},tick,player,Phase::Trigger);
    if (d.type==Primitive::Button || d.type==Primitive::Toggle)
    {
        if (value.x!=0 && value.x!=1) return false;
        if (d.type==Primitive::Button) phase=value.x ? Phase::Press : Phase::Release;
    }
    if (d.type==Primitive::Selector && (value.x<0 || value.x>=d.states || std::floor(value.x)!=value.x)) return false;
    value.x=std::clamp(value.x,d.minimum,d.maximum); value.y=d.type==Primitive::Vector2 ? std::clamp(value.y,d.minimum,d.maximum) : 0;
    state.refreshed=tick;
    if (Same(value,state.value)) return true;
    return Change(slot,value,tick,player,phase);
}
bool Bus::Trigger(Control control,std::uint64_t tick,std::uint32_t player) { return control==Control::PlayerUse && Publish("player.use",{},tick,player); }
bool Bus::Attach(Adapter& adapter)
{
    for (unsigned i=0;i<adapterCount_;++i) if (adapters_[i]==&adapter) return false;
    if (adapterCount_==MaxAdapters) return false;
    adapters_[adapterCount_++]=&adapter; Snapshot(0); return true;
}
void Bus::Snapshot(std::uint64_t tick)
{
    if (!enabled_) return;
    currentSnapshot_=++snapshot_; lastSnapshot_=tick;
    Message m; m.kind=Kind::SnapshotBegin; m.tick=tick; m.count=count_; Emit(m,nullptr);
    for (unsigned i=0;i<count_;++i)
    {
        m={}; m.kind=Kind::Definition; m.tick=tick; m.control=i; m.value=states_[i].value; m.revision=states_[i].revision; Emit(m,&definitions_[i]);
        m.kind=Kind::State; Emit(m,&definitions_[i]);
    }
    m={}; m.kind=Kind::SnapshotEnd; m.tick=tick; m.count=count_; Emit(m,nullptr); currentSnapshot_=0;
}
void Bus::Tick(std::uint64_t tick)
{
    for (unsigned i=0;i<adapterCount_;++i) adapters_[i]->Tick(tick);
    if (!enabled_) return;
    for (unsigned i=0;i<count_;++i)
    {
        const auto& d=definitions_[i]; const auto& s=states_[i];
        if (d.type!=Primitive::Trigger && d.staleTicks && tick>=s.refreshed && tick-s.refreshed>d.staleTicks && !Same(s.value,d.neutral))
            Change(i,d.neutral,tick,0,d.type==Primitive::Button ? Phase::Release : Phase::Reset,Name(ResetReason::Stale));
    }
    if (focused_ && tick>=lastSnapshot_ && tick-lastSnapshot_>=35) Snapshot(tick);
}
void Bus::Reset(ResetReason reason,std::uint64_t tick)
{
    for (unsigned i=0;i<count_;++i) if (definitions_[i].type!=Primitive::Trigger && !Same(states_[i].value,definitions_[i].neutral))
        Change(i,definitions_[i].neutral,tick,0,definitions_[i].type==Primitive::Button ? Phase::Release : Phase::Reset,Name(reason));
    for (unsigned i=0;i<adapterCount_;++i) adapters_[i]->Reset(reason);
    grants_=0;
    Message m; m.kind=Kind::Lifecycle; m.tick=tick; m.reason=Name(reason); Emit(m,nullptr);
}
void Bus::SetEnabled(bool enabled,std::uint64_t tick) { if (enabled_==enabled) return; if (!enabled) Reset(ResetReason::Disabled,tick); enabled_=enabled; if (enabled) Snapshot(tick); }
void Bus::BeginMap(std::uint64_t tick)
{
    Reset(ResetReason::MapChange,tick);
    for (unsigned i=1;i<count_;++i) { definitions_[i]={}; states_[i]={}; }
    count_=1; states_[0]={}; Snapshot(tick);
}
void Bus::SetFocused(bool focused,std::uint64_t tick) { if (focused_==focused) return; if (!focused) { Reset(ResetReason::FocusLost,tick); Grant(SoftwareKeyboard,false,tick); } focused_=focused; }
bool Bus::Grant(unsigned capability,bool granted,std::uint64_t tick)
{
    if (capability!=SoftwareKeyboard || (granted && (!enabled_ || !focused_))) return false;
    if (granted) { Reset(ResetReason::Rearm,tick); grants_|=capability; Snapshot(tick); }
    else { for (unsigned i=0;i<adapterCount_;++i) if (adapters_[i]->RequiredCapabilities()&capability) adapters_[i]->Reset(ResetReason::Disabled); grants_&=~capability; }
    return true;
}
}
