// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard_adapter.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <xcb/xcb.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
extern char** environ;
#endif
namespace cdr
{
#ifdef _WIN32
class WindowsEndpoint final:public KeyEndpoint
{
    HWND target_=nullptr; DWORD pid_=0;
    static unsigned Code(Key key)
    {
        static const unsigned code[]={VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,VK_SPACE,VK_RETURN,VK_SHIFT,'W','S','1','2','3','B'};
        return code[unsigned(key)];
    }
public:
    bool Connect() noexcept override
    {
        target_=FindWindowA("DoomIOInputLab",nullptr);
        if (target_) GetWindowThreadProcessId(target_,&pid_);
        return Ready();
    }
    bool Ready() const noexcept override
    {
        DWORD pid=0; if (!target_ || !IsWindow(target_)) return false;
        GetWindowThreadProcessId(target_,&pid); return pid==pid_;
    }
    bool Set(Key key,bool down) noexcept override
    {
        if (!Ready()) return false;
        const auto code=Code(key); LPARAM flags=1 | (LPARAM(MapVirtualKeyA(code,MAPVK_VK_TO_VSC))<<16);
        if (key<=Key::Down) flags|=LPARAM(1)<<24;
        if (!down) flags|=(LPARAM(1)<<30) | (LPARAM(1)<<31);
        return PostMessageA(target_,down ? WM_KEYDOWN : WM_KEYUP,code,flags)!=0;
    }
    void Poll() noexcept override {}
};
std::unique_ptr<KeyEndpoint> NativeKeyboardEndpoint() { return std::make_unique<WindowsEndpoint>(); }
bool LaunchInputLab(const std::string& executable)
{
    STARTUPINFOA startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    std::string command="\""+executable+"\"";
    if (!CreateProcessA(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) return false;
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return true;
}
#else
class X11Endpoint final:public KeyEndpoint
{
    xcb_connection_t* connection_=nullptr;
    xcb_window_t target_=XCB_NONE,root_=XCB_NONE;
    std::array<xcb_keycode_t,KeyCount> codes_{};
    unsigned modifiers_=0;
    static xcb_keysym_t Symbol(Key key)
    {
        static const xcb_keysym_t sym[]={0xff51,0xff53,0xff52,0xff54,0x20,0xff0d,0xffe1,'w','s','1','2','3','b'};
        return sym[unsigned(key)];
    }
    bool Search(xcb_window_t window,unsigned depth) noexcept
    {
        if (depth>2) return false;
        auto cookie=xcb_get_property(connection_,0,window,XCB_ATOM_WM_CLASS,XCB_ATOM_STRING,0,64);
        auto* property=xcb_get_property_reply(connection_,cookie,nullptr);
        if (property)
        {
            std::string_view value(static_cast<char*>(xcb_get_property_value(property)),std::size_t(xcb_get_property_value_length(property)));
            const bool found=value.find("DoomIOInputLab")!=std::string_view::npos; std::free(property);
            if (found) { target_=window; return true; }
        }
        auto* tree=xcb_query_tree_reply(connection_,xcb_query_tree(connection_,window),nullptr);
        if (!tree) return false;
        bool found=false; const auto* children=xcb_query_tree_children(tree);
        const int count=xcb_query_tree_children_length(tree);
        for (int i=0;i<count && i<128 && !found;++i) found=Search(children[i],depth+1);
        std::free(tree); return found;
    }
public:
    ~X11Endpoint() override { if (connection_) xcb_disconnect(connection_); }
    bool Connect() noexcept override
    {
        if (connection_) xcb_disconnect(connection_);
        connection_=xcb_connect(nullptr,nullptr); target_=XCB_NONE; modifiers_=0; codes_={};
        if (!connection_ || xcb_connection_has_error(connection_)) return false;
        const auto* setup=xcb_get_setup(connection_); auto screens=xcb_setup_roots_iterator(setup);
        if (!screens.rem) return false;
        root_=screens.data->root;
        if (!Search(root_,0)) return false;
        const unsigned mask=XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        xcb_change_window_attributes(connection_,target_,XCB_CW_EVENT_MASK,&mask);
        const unsigned count=unsigned(setup->max_keycode)-setup->min_keycode+1;
        auto* keys=xcb_get_keyboard_mapping_reply(connection_,xcb_get_keyboard_mapping(connection_,setup->min_keycode,std::uint8_t(count)),nullptr);
        if (!keys) return false;
        const auto* symbols=xcb_get_keyboard_mapping_keysyms(keys);
        for (unsigned key=0;key<KeyCount;++key)
            for (unsigned code=0;code<count;++code)
                for (unsigned level=0;level<keys->keysyms_per_keycode;++level)
                    if (!codes_[key] && symbols[code*keys->keysyms_per_keycode+level]==Symbol(Key(key))) codes_[key]=xcb_keycode_t(setup->min_keycode+code);
        std::free(keys); xcb_flush(connection_);
        for (auto code:codes_) if (!code) { target_=XCB_NONE; return false; }
        return Ready();
    }
    bool Ready() const noexcept override { return connection_ && target_!=XCB_NONE && !xcb_connection_has_error(connection_); }
    bool Set(Key key,bool down) noexcept override
    {
        if (!Ready()) return false;
        xcb_key_press_event_t event{}; event.response_type=down ? XCB_KEY_PRESS : XCB_KEY_RELEASE;
        event.detail=codes_[unsigned(key)]; event.root=root_; event.event=target_; event.same_screen=1; event.state=std::uint16_t(modifiers_);
        xcb_send_event(connection_,0,target_,down ? XCB_EVENT_MASK_KEY_PRESS : XCB_EVENT_MASK_KEY_RELEASE,reinterpret_cast<const char*>(&event));
        if (key==Key::Shift) modifiers_=down ? XCB_MOD_MASK_SHIFT : 0;
        return xcb_flush(connection_)>0 && !xcb_connection_has_error(connection_);
    }
    void Poll() noexcept override
    {
        if (!connection_) return;
        while (auto* event=xcb_poll_for_event(connection_))
        {
            if ((event->response_type&127)==XCB_DESTROY_NOTIFY && reinterpret_cast<xcb_destroy_notify_event_t*>(event)->window==target_) target_=XCB_NONE;
            if ((event->response_type&127)==0) target_=XCB_NONE;
            std::free(event);
        }
    }
};
std::unique_ptr<KeyEndpoint> NativeKeyboardEndpoint() { return std::make_unique<X11Endpoint>(); }
bool LaunchInputLab(const std::string& executable)
{
    pid_t pid=0; char* args[]={const_cast<char*>(executable.c_str()),nullptr};
    const bool good=posix_spawn(&pid,executable.c_str(),nullptr,nullptr,args,environ)==0;
    if (good) { /* Child has its own visible window; the engine does not wait. */ }
    return good;
}
#endif
}
