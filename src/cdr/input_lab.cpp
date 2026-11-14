// SPDX-License-Identifier: GPL-3.0-or-later
// A separate, keyboard-controlled desktop rover. It knows no control-bus IDs,
// UDP messages, GZDoom state, or script API. This is the observable OS endpoint.
#include "keyboard_adapter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <xcb/xcb.h>
#include <cstdlib>
#endif
using cdr::Key;
namespace
{
struct Lab
{
    unsigned held=0,fires=0,pulses=0,brakes=0,mode=0;
    double x=0,y=0;
    std::string report;
    bool Down(Key key) const { return (held&(1u<<unsigned(key)))!=0; }
    void Input(Key key,bool down)
    {
        unsigned bit=1u<<unsigned(key);
        if (down && !(held&bit))
        {
            if (key==Key::Space) ++fires;
            if (key==Key::Enter) ++pulses;
            if (key==Key::B) ++brakes;
            if (key>=Key::One && key<=Key::Three) mode=unsigned(key)-unsigned(Key::One);
        }
        if (down) held|=bit; else held&=~bit;
        Save();
    }
    void Step(double dt)
    {
        double speed=Down(Key::Shift) ? 0.25 : 1.0;
        if (Down(Key::W)) speed*=2;
        if (Down(Key::S)) speed*=0.3;
        if (Down(Key::B)) speed=0;
        x=std::clamp(x+(double(Down(Key::Right))-double(Down(Key::Left)))*speed*dt,-1.0,1.0);
        y=std::clamp(y+(double(Down(Key::Up))-double(Down(Key::Down)))*speed*dt,-1.0,1.0);
    }
    void Save() const
    {
        if (report.empty()) return;
        std::ofstream file(report,std::ios::trunc);
        file << "{\"held\":" << held << ",\"fires\":" << fires << ",\"pulses\":" << pulses
             << ",\"brakes\":" << brakes << ",\"mode\":" << mode << ",\"x\":" << x << ",\"y\":" << y << "}\n";
    }
};
Lab lab;
#ifdef _WIN32
int KeyIndex(unsigned code)
{
    switch(code) { case VK_LEFT:return 0; case VK_RIGHT:return 1; case VK_UP:return 2; case VK_DOWN:return 3;
    case VK_SPACE:return 4; case VK_RETURN:return 5; case VK_SHIFT:return 6; case 'W':return 7; case 'S':return 8;
    case '1':return 9; case '2':return 10; case '3':return 11; case 'B':return 12; default:return -1; }
}
void Paint(HDC dc,RECT rect)
{
    HBRUSH background=CreateSolidBrush(RGB(12,20,29)); FillRect(dc,&rect,background); DeleteObject(background);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(160,230,225));
    HFONT font=CreateFontA(19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,"Consolas");
    auto old=SelectObject(dc,font);
    auto text=[&](int x,int y,const char* s) { TextOutA(dc,x,y,s,int(std::strlen(s))); };
    text(24,20,"DOOM I/O  |  INPUT LAB"); text(24,50,"Independent desktop rover - routed keyboard input");
    HPEN pen=CreatePen(PS_SOLID,1,RGB(37,62,72)); auto oldPen=SelectObject(dc,pen);
    for (int i=0;i<=10;++i) { MoveToEx(dc,24+i*45,100,nullptr); LineTo(dc,24+i*45,460); MoveToEx(dc,24,100+i*36,nullptr); LineTo(dc,474,100+i*36); }
    const int x=249+int(lab.x*210),y=280-int(lab.y*165);
    HBRUSH cursor=CreateSolidBrush(RGB(65,220,184)); auto oldBrush=SelectObject(dc,cursor); Ellipse(dc,x-10,y-10,x+10,y+10);
    SelectObject(dc,oldBrush); DeleteObject(cursor); SelectObject(dc,oldPen); DeleteObject(pen);
    char value[160]; std::snprintf(value,sizeof(value),"BEACONS  %u    PULSES  %u",lab.fires,lab.pulses); text(510,115,value);
    std::snprintf(value,sizeof(value),"MODE  %u   BRAKES  %u",lab.mode+1,lab.brakes); text(510,150,value);
    std::snprintf(value,sizeof(value),"POSITION  %+.2f / %+.2f",lab.x,lab.y); text(510,185,value);
    text(510,240,"ARROWS  drive rover"); text(510,270,"SPACE   launch beacon"); text(510,300,"ENTER   pulse");
    text(510,330,"SHIFT   precision"); text(510,360,"W / S   throttle"); text(510,390,"B       brake"); text(510,420,"1 2 3   select mode");
    text(24,495,"Keep DOOM focused. Focus loss releases controls and disarms output.");
    SelectObject(dc,old); DeleteObject(font);
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    if (message==WM_KEYDOWN || message==WM_KEYUP)
    {
        int key=KeyIndex(unsigned(wParam)); if (key>=0) lab.Input(Key(unsigned(key)),message==WM_KEYDOWN);
        InvalidateRect(window,nullptr,FALSE); return 0;
    }
    if (message==WM_TIMER) { lab.Step(0.016); lab.Save(); InvalidateRect(window,nullptr,FALSE); return 0; }
    if (message==WM_PAINT) { PAINTSTRUCT ps; HDC dc=BeginPaint(window,&ps); RECT rect; GetClientRect(window,&rect); Paint(dc,rect); EndPaint(window,&ps); return 0; }
    if (message==WM_DESTROY) { lab.held=0; lab.Save(); PostQuitMessage(0); return 0; }
    return DefWindowProcA(window,message,wParam,lParam);
}
#else
xcb_keysym_t symbols[cdr::KeyCount]={0xff51,0xff53,0xff52,0xff54,0x20,0xff0d,0xffe1,'w','s','1','2','3','b'};
#endif
}
int main(int argc,char** argv)
{
    for (int i=1;i+1<argc;++i) if (std::string(argv[i])=="--report") lab.report=argv[++i];
    lab.Save();
#ifdef _WIN32
    WNDCLASSA cls{}; cls.lpfnWndProc=WindowProc; cls.hInstance=GetModuleHandleA(nullptr); cls.lpszClassName="DoomIOInputLab"; cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if (!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 2;
    HWND window=CreateWindowExA(WS_EX_NOACTIVATE,cls.lpszClassName,"DOOM I/O - Input Lab",WS_OVERLAPPEDWINDOW,40,40,1000,600,nullptr,nullptr,cls.hInstance,nullptr);
    if (!window) return 3;
    ShowWindow(window,SW_SHOWNOACTIVATE); SetTimer(window,1,16,nullptr);
    MSG message; while (GetMessageA(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageA(&message); }
#else
    auto* connection=xcb_connect(nullptr,nullptr);
    if (!connection || xcb_connection_has_error(connection)) return 2;
    auto screen=xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    const auto window=xcb_generate_id(connection);
    unsigned values[]={0x0c141d,XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_KEY_PRESS|XCB_EVENT_MASK_KEY_RELEASE|XCB_EVENT_MASK_STRUCTURE_NOTIFY};
    xcb_create_window(connection,XCB_COPY_FROM_PARENT,window,screen->root,30,30,960,560,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,screen->root_visual,XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK,values);
    const char wmclass[]="doom-io-input-lab\0DoomIOInputLab\0";
    xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_CLASS,XCB_ATOM_STRING,8,sizeof(wmclass)-1,wmclass);
    const char title[]="DOOM I/O - Input Lab";
    xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,sizeof(title)-1,title);
    auto gc=xcb_generate_id(connection); unsigned gcvalues[]={0xa0e6e1,0x0c141d};
    xcb_create_gc(connection,gc,window,XCB_GC_FOREGROUND|XCB_GC_BACKGROUND,gcvalues);
    xcb_map_window(connection,window); xcb_flush(connection);
    std::array<xcb_keycode_t,cdr::KeyCount> codes{};
    const auto* setup=xcb_get_setup(connection); unsigned count=unsigned(setup->max_keycode)-setup->min_keycode+1;
    auto* mapping=xcb_get_keyboard_mapping_reply(connection,xcb_get_keyboard_mapping(connection,setup->min_keycode,std::uint8_t(count)),nullptr);
    if (!mapping) return 3;
    const auto* map=xcb_get_keyboard_mapping_keysyms(mapping);
    for (unsigned k=0;k<cdr::KeyCount;++k) for (unsigned c=0;c<count;++c) for (unsigned l=0;l<mapping->keysyms_per_keycode;++l)
        if (!codes[k] && map[c*mapping->keysyms_per_keycode+l]==symbols[k]) codes[k]=xcb_keycode_t(setup->min_keycode+c);
    std::free(mapping);
    auto text=[&](int x,int y,const char* value) { auto length=std::min<std::size_t>(std::char_traits<char>::length(value),255); xcb_image_text_8(connection,std::uint8_t(length),window,gc,std::int16_t(x),std::int16_t(y),value); };
    auto last=std::chrono::steady_clock::now(); unsigned frame=0;
    while (!xcb_connection_has_error(connection))
    {
        bool running=true;
        while (auto* event=xcb_poll_for_event(connection))
        {
            auto type=event->response_type&127;
            if (type==XCB_KEY_PRESS || type==XCB_KEY_RELEASE)
            {
                auto code=reinterpret_cast<xcb_key_press_event_t*>(event)->detail;
                for (unsigned k=0;k<cdr::KeyCount;++k) if (codes[k]==code) lab.Input(Key(k),type==XCB_KEY_PRESS);
            }
            if (type==XCB_DESTROY_NOTIFY) running=false;
            std::free(event);
        }
        if (!running) break;
        auto now=std::chrono::steady_clock::now(); lab.Step(std::chrono::duration<double>(now-last).count()); last=now;
        if (frame++%3==0)
        {
            xcb_clear_area(connection,0,window,0,0,0,0);
            text(24,30,"DOOM I/O | INPUT LAB"); text(24,55,"Independent desktop rover - routed keyboard input");
            unsigned color=0x253e48; xcb_change_gc(connection,gc,XCB_GC_FOREGROUND,&color);
            for (int i=0;i<=10;++i)
            {
                xcb_point_t line[]={{std::int16_t(24+i*45),100},{std::int16_t(24+i*45),460}}; xcb_poly_line(connection,XCB_COORD_MODE_ORIGIN,window,gc,2,line);
                xcb_point_t row[]={{24,std::int16_t(100+i*36)},{474,std::int16_t(100+i*36)}}; xcb_poly_line(connection,XCB_COORD_MODE_ORIGIN,window,gc,2,row);
            }
            color=0x41dcb8; xcb_change_gc(connection,gc,XCB_GC_FOREGROUND,&color);
            xcb_rectangle_t rover{std::int16_t(240+lab.x*210),std::int16_t(271-lab.y*165),18,18}; xcb_poly_fill_rectangle(connection,window,gc,1,&rover);
            char value[128]; std::snprintf(value,sizeof(value),"BEACONS %u   PULSES %u",lab.fires,lab.pulses); text(510,120,value);
            std::snprintf(value,sizeof(value),"MODE %u   BRAKES %u",lab.mode+1,lab.brakes); text(510,155,value);
            std::snprintf(value,sizeof(value),"POSITION %+.2f / %+.2f",lab.x,lab.y); text(510,190,value);
            text(510,245,"ARROWS  drive rover"); text(510,275,"SPACE   launch beacon"); text(510,305,"ENTER   pulse");
            text(510,335,"SHIFT   precision"); text(510,365,"W / S   throttle"); text(510,395,"B       brake"); text(510,425,"1 2 3   select mode");
            text(24,510,"Keep DOOM focused. Focus loss releases controls and disarms output.");
            xcb_flush(connection); lab.Save();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    lab.held=0; lab.Save(); xcb_disconnect(connection);
#endif
    return 0;
}
