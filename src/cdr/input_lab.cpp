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
#include <filesystem>
#include <vector>
#include <cstdlib>
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
#include <unistd.h>
#endif
using cdr::Key;
namespace
{
struct Lab
{
    struct Stroke { double x0,y0,x1,y1; unsigned color; };
    struct Mark { double x,y; unsigned color; };
    unsigned held=0,fires=0,pulses=0,brakes=0,mode=0;
    double x=0,y=0;
    std::string report,exportPath;
    std::vector<Stroke> strokes;
    std::vector<Mark> marks;
    bool Down(Key key) const { return (held&(1u<<unsigned(key)))!=0; }
    void Input(Key key,bool down)
    {
        unsigned bit=1u<<unsigned(key);
        if (down && !(held&bit))
        {
            if (key==Key::Space) { ++fires; if (marks.size()<1024) marks.push_back({x,y,mode}); }
            if (key==Key::Enter) { ++pulses; Export(); }
            if (key==Key::B) ++brakes;
            if (key>=Key::One && key<=Key::Three) mode=unsigned(key)-unsigned(Key::One);
        }
        if (down) held|=bit; else held&=~bit;
        Save();
    }
    bool Step(double dt)
    {
        const double beforeX=x,beforeY=y;
        double speed=Down(Key::Shift) ? 0.25 : 1.0;
        if (Down(Key::W)) speed*=2;
        if (Down(Key::S)) speed*=0.3;
        if (Down(Key::B)) speed=0;
        x=std::clamp(x+(double(Down(Key::Right))-double(Down(Key::Left)))*speed*dt,-1.0,1.0);
        y=std::clamp(y+(double(Down(Key::Up))-double(Down(Key::Down)))*speed*dt,-1.0,1.0);
        if (Down(Key::Shift) && strokes.size()<4096 && (beforeX!=x || beforeY!=y))
            strokes.push_back({beforeX,beforeY,x,y,mode});
        return beforeX!=x || beforeY!=y;
    }
    void Export() const
    {
        if (exportPath.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(std::filesystem::path(exportPath).parent_path(),error);
        std::ofstream file(exportPath,std::ios::trunc);
        const char* colors[]={"#41dcb8","#ffbd59","#9c91ff"};
        file << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 500 500\">\n"
             << "<title>DOOM I/O sketch</title><rect width=\"500\" height=\"500\" fill=\"#0c141d\"/>\n";
        for (const auto& s:strokes) file << "<path fill=\"none\" stroke=\"" << colors[s.color%3]
            << "\" stroke-width=\"3\" d=\"M" << 250+s.x0*240 << ',' << 250-s.y0*240 << " L" << 250+s.x1*240 << ',' << 250-s.y1*240 << "\"/>\n";
        for (const auto& m:marks) file << "<circle cx=\"" << 250+m.x*240 << "\" cy=\"" << 250-m.y*240 << "\" r=\"6\" fill=\"" << colors[m.color%3] << "\"/>\n";
        file << "</svg>\n";
    }
    void Save() const
    {
        if (report.empty()) return;
        std::ofstream file(report,std::ios::trunc);
        file << "{\"held\":" << held << ",\"fires\":" << fires << ",\"pulses\":" << pulses
             << ",\"brakes\":" << brakes << ",\"mode\":" << mode << ",\"x\":" << x << ",\"y\":" << y
             << ",\"strokes\":" << strokes.size() << ",\"marks\":" << marks.size()
#ifdef _WIN32
             << ",\"pid\":" << GetCurrentProcessId()
#else
             << ",\"pid\":" << getpid()
#endif
             << "}\n";
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
// These bitmaps are private drawing surfaces. Only a completed frame is copied
// to the window; the retained ink surface never contains the moving cursor.
class BitmapSurface
{
    HDC dc_=nullptr;
    HBITMAP bitmap_=nullptr,previous_=nullptr;
    int width_=0,height_=0;
public:
    ~BitmapSurface() { Clear(); }
    BitmapSurface()=default;
    BitmapSurface(const BitmapSurface&)=delete;
    BitmapSurface& operator=(const BitmapSurface&)=delete;
    void Clear()
    {
        if (dc_ && previous_) SelectObject(dc_,previous_);
        if (bitmap_) DeleteObject(bitmap_);
        if (dc_) DeleteDC(dc_);
        dc_=nullptr; bitmap_=nullptr; previous_=nullptr; width_=height_=0;
    }
    bool Ensure(HDC reference,int width,int height)
    {
        if (dc_ && width==width_ && height==height_) return true;
        Clear();
        dc_=CreateCompatibleDC(reference);
        bitmap_=CreateCompatibleBitmap(reference,width,height);
        if (!dc_ || !bitmap_) { Clear(); return false; }
        previous_=static_cast<HBITMAP>(SelectObject(dc_,bitmap_));
        width_=width; height_=height; return true;
    }
    HDC DC() const { return dc_; }
};
class WindowPainter
{
    BitmapSurface frame_,ink_;
    HBRUSH background_=CreateSolidBrush(RGB(12,20,29));
    HBRUSH cursor_=CreateSolidBrush(RGB(65,220,184));
    HPEN grid_=CreatePen(PS_SOLID,1,RGB(37,62,72));
    HPEN pens_[3]={CreatePen(PS_SOLID,2,RGB(65,220,184)),CreatePen(PS_SOLID,2,RGB(255,189,89)),CreatePen(PS_SOLID,2,RGB(156,145,255))};
    HFONT font_=CreateFontA(19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,"Consolas");
    std::size_t drawn_=0;
    std::uint64_t rasterized_=0;
    bool inkReady_=false;
public:
    ~WindowPainter()
    {
        // Release surfaces before resources selected into them.
        frame_.Clear(); ink_.Clear();
        DeleteObject(font_); DeleteObject(grid_); DeleteObject(background_); DeleteObject(cursor_);
        for (auto pen:pens_) DeleteObject(pen);
    }
    std::uint64_t Rasterized() const { return rasterized_; }
    bool Paint(HDC destination,RECT rect,const Lab& state)
    {
        const int width=rect.right-rect.left,height=rect.bottom-rect.top;
        if (width<=0 || height<=0) return true;
        if (!frame_.Ensure(destination,width,height) || !ink_.Ensure(destination,500,480)) return false;
        const auto ink=ink_.DC();
        if (!inkReady_ || state.strokes.size()<drawn_)
        {
            RECT area{0,0,500,480}; FillRect(ink,&area,background_); SelectObject(ink,grid_);
            for (int i=0;i<=10;++i) { MoveToEx(ink,24+i*45,100,nullptr); LineTo(ink,24+i*45,460); MoveToEx(ink,24,100+i*36,nullptr); LineTo(ink,474,100+i*36); }
            drawn_=0; inkReady_=true;
        }
        for (;drawn_<state.strokes.size();++drawn_)
        {
            const auto& stroke=state.strokes[drawn_]; SelectObject(ink,pens_[stroke.color%3]);
            MoveToEx(ink,249+int(stroke.x0*210),280-int(stroke.y0*165),nullptr);
            LineTo(ink,249+int(stroke.x1*210),280-int(stroke.y1*165)); ++rasterized_;
        }
        const auto dc=frame_.DC();
        FillRect(dc,&rect,background_);
        BitBlt(dc,0,0,std::min(width,500),std::min(height,480),ink,0,0,SRCCOPY);
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(160,230,225)); SelectObject(dc,font_);
        auto text=[&](int x,int y,const char* value) { TextOutA(dc,x,y,value,int(std::strlen(value))); };
        text(24,20,"DOOM I/O  |  INPUT LAB / SKETCHPAD");
        text(24,50,"XY PAD = VELOCITY / JOYSTICK. CENTER OR LEAVE TO STOP.");
        // Markers form a separate layer above ink, including newer strokes.
        SelectObject(dc,grid_); SelectObject(dc,cursor_);
        for (const auto& mark:state.marks) { int x=249+int(mark.x*210),y=280-int(mark.y*165); Rectangle(dc,x-4,y-4,x+4,y+4); }
        const int x=249+int(state.x*210),y=280-int(state.y*165);
        Ellipse(dc,x-10,y-10,x+10,y+10);
        char value[160]; std::snprintf(value,sizeof(value),"STAMPS   %u    EXPORTS %u",state.fires,state.pulses); text(510,115,value);
        std::snprintf(value,sizeof(value),"MODE  %u   BRAKES  %u",state.mode+1,state.brakes); text(510,150,value);
        std::snprintf(value,sizeof(value),"POSITION  %+.2f / %+.2f",state.x,state.y); text(510,185,value);
        text(510,240,"ARROWS  pen velocity"); text(510,270,"SPACE   stamp marker"); text(510,300,"ENTER   export SVG");
        text(510,330,"SHIFT   pen down / precision"); text(510,360,"W / S   throttle"); text(510,390,"B       brake"); text(510,420,"1 2 3   ink color");
        text(24,470,state.exportPath.c_str());
        text(24,495,"Keep DOOM focused. Focus loss releases controls and disarms output.");
        return BitBlt(destination,0,0,width,height,dc,0,0,SRCCOPY)!=0;
    }
};
// Exercise the actual GDI renderer on a DIB, including a full-history scene,
// incremental ink, cursor movement, repeated paints and bitmap resizing.
int TestPainting(const char* report)
{
    auto screen=GetDC(nullptr); auto target=CreateCompatibleDC(screen);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=1200; info.bmiHeader.biHeight=-800;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!screen || !target || !bitmap || !bits) return 2;
    auto previous=SelectObject(target,bitmap);
    auto hash=[&] {
        GdiFlush();
        std::uint64_t value=14695981039346656037ull;
        auto* pixels=static_cast<unsigned char*>(bits);
        for (unsigned y=0;y<600;++y) for (unsigned x=0;x<1000;++x) for (unsigned c=0;c<3;++c)
            value=(value^pixels[(y*1200+x)*4+c])*1099511628211ull;
        return value;
    };
    bool good=true; DWORD growth=0;
    std::uint64_t rasterized=0;
    const auto start=std::chrono::steady_clock::now();
    {
        Lab state; state.x=0.1; state.y=0.2;
        auto populate=[&](unsigned first,unsigned last) {
            for (unsigned i=first;i<last;++i)
                state.strokes.push_back({0.9*std::sin(i*0.017),0.9*std::cos(i*0.023),0.9*std::sin((i+1)*0.017),0.9*std::cos((i+1)*0.023),i%3});
        };
        for (unsigned i=0;i<1024;++i) state.marks.push_back({std::sin(i*0.03)*0.9,std::cos(i*0.03)*0.9,i%3});
        WindowPainter cached; RECT area{0,0,1000,600};
        populate(0,2048); good=cached.Paint(target,area,state) && good;
        populate(2048,4096); good=cached.Paint(target,area,state) && good;
        const auto expected=hash();
        {
            WindowPainter fresh; good=fresh.Paint(target,area,state) && good;
            good=hash()==expected && good;
        }
        const DWORD handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for (unsigned i=0;i<120;++i)
        {
            RECT resized{0,0,i%2 ? 1200 : 800,i%2 ? 700 : 560};
            good=cached.Paint(target,resized,state) && good;
            good=cached.Paint(target,area,state) && good;
            good=hash()==expected && good;
        }
        // The cursor must not be baked into retained ink, even after resize.
        state.x=-0.6; state.y=-0.4; good=cached.Paint(target,area,state) && good;
        const auto moved=hash(); good=moved!=expected && good;
        { WindowPainter fresh; good=fresh.Paint(target,area,state) && good; good=hash()==moved && good; }
        rasterized=cached.Rasterized(); good=rasterized==4096 && good;
        const DWORD finalHandles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        growth=finalHandles>handles ? finalHandles-handles : 0;
        good=growth<=2 && good;
    }
    SelectObject(target,previous); DeleteObject(bitmap); DeleteDC(target); ReleaseDC(nullptr,screen);
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    std::ofstream output(report,std::ios::trunc);
    output << "{\"passed\":" << (good ? "true" : "false") << ",\"strokes\":4096,\"markers\":1024,\"repaints\":240,\"rasterized\":" << rasterized
           << ",\"gdi_handle_growth\":" << growth << ",\"elapsed_ms\":" << elapsed << "}\n";
    std::printf("%s: GDI retained ink, complete frame presentation, resize, cursor and resource checks\n",good ? "PASS" : "FAIL");
    return good ? 0 : 3;
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    if (message==WM_KEYDOWN || message==WM_KEYUP)
    {
        int key=KeyIndex(unsigned(wParam)); if (key>=0) lab.Input(Key(unsigned(key)),message==WM_KEYDOWN);
        InvalidateRect(window,nullptr,FALSE); return 0;
    }
    if (message==WM_ERASEBKGND) return 1; // Paint supplies a complete opaque frame.
    if (message==WM_SIZE) { InvalidateRect(window,nullptr,FALSE); return 0; }
    if (message==WM_TIMER)
    {
        static auto previous=std::chrono::steady_clock::now();
        const auto now=std::chrono::steady_clock::now();
        const bool changed=lab.Step(std::clamp(std::chrono::duration<double>(now-previous).count(),0.,0.1)); previous=now;
        lab.Save(); if (changed) InvalidateRect(window,nullptr,FALSE); return 0;
    }
    if (message==WM_PAINT || message==WM_PRINTCLIENT)
    {
        static WindowPainter painter;
        PAINTSTRUCT ps{}; HDC dc=message==WM_PAINT ? BeginPaint(window,&ps) : reinterpret_cast<HDC>(wParam);
        RECT rect; GetClientRect(window,&rect); painter.Paint(dc,rect,lab);
        if (message==WM_PAINT) EndPaint(window,&ps);
        return 0;
    }
    if (message==WM_DESTROY) { lab.held=0; lab.Save(); PostQuitMessage(0); return 0; }
    return DefWindowProcA(window,message,wParam,lParam);
}
#else
xcb_keysym_t symbols[cdr::KeyCount]={0xff51,0xff53,0xff52,0xff54,0x20,0xff0d,0xffe1,'w','s','1','2','3','b'};
#endif
}
int main(int argc,char** argv)
{
#ifdef _WIN32
    if (argc==3 && std::string(argv[1])=="--test-render") return TestPainting(argv[2]);
#endif
#ifdef _WIN32
    const char* data=std::getenv("LOCALAPPDATA");
#else
    const char* data=std::getenv("XDG_DATA_HOME");
    if (!data || !*data) data=std::getenv("HOME");
#endif
    if (data && *data) lab.exportPath=(std::filesystem::path(data)/"can-doom-run-it"/"sketch.svg").string();
    // Trusted process environment allows test harnesses to retain independent
    // endpoint evidence when launched by the engine. This is not a map API.
    if (const char* report=std::getenv("CDR_LAB_REPORT")) lab.report=report;
    if (const char* output=std::getenv("CDR_LAB_EXPORT")) lab.exportPath=output;
    for (int i=1;i+1<argc;++i)
    {
        if (std::string(argv[i])=="--report") lab.report=argv[++i];
        else if (std::string(argv[i])=="--export") lab.exportPath=argv[++i];
    }
    lab.Save();
#ifdef _WIN32
    WNDCLASSA cls{}; cls.lpfnWndProc=WindowProc; cls.hInstance=GetModuleHandleA(nullptr); cls.lpszClassName="DoomIOInputLab"; cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if (!RegisterClassA(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return 2;
    HWND window=CreateWindowExA(WS_EX_APPWINDOW,cls.lpszClassName,"DOOM I/O - Input Lab",WS_OVERLAPPEDWINDOW,40,40,1000,600,nullptr,nullptr,cls.hInstance,nullptr);
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
    // Do not steal DOOM's focus on first show; allow ordinary later activation.
    const char userTimeName[]="_NET_WM_USER_TIME";
    auto* userTime=xcb_intern_atom_reply(connection,xcb_intern_atom(connection,0,sizeof(userTimeName)-1,userTimeName),nullptr);
    if (userTime) { const unsigned zero=0; xcb_change_property(connection,XCB_PROP_MODE_REPLACE,window,userTime->atom,XCB_ATOM_CARDINAL,32,1,&zero); std::free(userTime); }
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
    unsigned width=960,height=560;
    xcb_pixmap_t frame=XCB_NONE,ink=xcb_generate_id(connection);
    xcb_create_pixmap(connection,screen->root_depth,ink,window,500,480);
    auto color=[&](unsigned value) { xcb_change_gc(connection,gc,XCB_GC_FOREGROUND,&value); };
    auto fill=[&](xcb_drawable_t target,unsigned w,unsigned h) { xcb_rectangle_t rect{0,0,std::uint16_t(w),std::uint16_t(h)}; xcb_poly_fill_rectangle(connection,target,gc,1,&rect); };
    color(0x0c141d); fill(ink,500,480); color(0x253e48);
    for (int i=0;i<=10;++i)
    {
        xcb_point_t line[]={{std::int16_t(24+i*45),100},{std::int16_t(24+i*45),460}}; xcb_poly_line(connection,XCB_COORD_MODE_ORIGIN,ink,gc,2,line);
        xcb_point_t row[]={{24,std::int16_t(100+i*36)},{474,std::int16_t(100+i*36)}}; xcb_poly_line(connection,XCB_COORD_MODE_ORIGIN,ink,gc,2,row);
    }
    auto text=[&](int x,int y,const char* value) { auto length=std::min<std::size_t>(std::char_traits<char>::length(value),255); xcb_image_text_8(connection,std::uint8_t(length),frame,gc,std::int16_t(x),std::int16_t(y),value); };
    std::size_t drawn=0;
    bool dirty=true;
    auto last=std::chrono::steady_clock::now();
    while (!xcb_connection_has_error(connection))
    {
        bool running=true;
        while (auto* event=xcb_poll_for_event(connection))
        {
            auto type=event->response_type&127;
            if (type==XCB_KEY_PRESS || type==XCB_KEY_RELEASE)
            {
                auto code=reinterpret_cast<xcb_key_press_event_t*>(event)->detail;
                for (unsigned k=0;k<cdr::KeyCount;++k) if (codes[k]==code) { lab.Input(Key(k),type==XCB_KEY_PRESS); dirty=true; }
            }
            if (type==XCB_EXPOSE) dirty=true;
            if (type==XCB_CONFIGURE_NOTIFY)
            {
                const auto* size=reinterpret_cast<xcb_configure_notify_event_t*>(event);
                if (size->width!=width || size->height!=height)
                {
                    width=size->width; height=size->height;
                    if (frame!=XCB_NONE) xcb_free_pixmap(connection,frame);
                    frame=XCB_NONE; dirty=true;
                }
            }
            if (type==XCB_DESTROY_NOTIFY) running=false;
            std::free(event);
        }
        if (!running) break;
        const auto now=std::chrono::steady_clock::now();
        dirty=lab.Step(std::clamp(std::chrono::duration<double>(now-last).count(),0.,0.1)) || dirty; last=now;
        if (dirty && width && height)
        {
            if (frame==XCB_NONE) { frame=xcb_generate_id(connection); xcb_create_pixmap(connection,screen->root_depth,frame,window,std::uint16_t(width),std::uint16_t(height)); }
            const unsigned colors[]={0x41dcb8,0xffbd59,0x9c91ff};
            // Append new ink once. No clear or progressive drawing reaches the window.
            for (;drawn<lab.strokes.size();++drawn)
            {
                const auto& stroke=lab.strokes[drawn]; color(colors[stroke.color%3]);
                xcb_point_t path[]={{std::int16_t(249+stroke.x0*210),std::int16_t(280-stroke.y0*165)},{std::int16_t(249+stroke.x1*210),std::int16_t(280-stroke.y1*165)}};
                xcb_poly_line(connection,XCB_COORD_MODE_ORIGIN,ink,gc,2,path);
            }
            color(0x0c141d); fill(frame,width,height);
            xcb_copy_area(connection,ink,frame,gc,0,0,0,0,std::uint16_t(std::min(width,500u)),std::uint16_t(std::min(height,480u)));
            color(0xa0e6e1);
            text(24,30,"DOOM I/O | INPUT LAB / SKETCHPAD"); text(24,55,"XY PAD = VELOCITY / JOYSTICK. CENTER OR LEAVE TO STOP.");
            color(0x41dcb8);
            for (const auto& mark:lab.marks) { xcb_rectangle_t stamp{std::int16_t(245+mark.x*210),std::int16_t(276-mark.y*165),8,8}; xcb_poly_rectangle(connection,frame,gc,1,&stamp); }
            xcb_rectangle_t rover{std::int16_t(240+lab.x*210),std::int16_t(271-lab.y*165),18,18}; xcb_poly_fill_rectangle(connection,frame,gc,1,&rover);
            char value[128]; std::snprintf(value,sizeof(value),"STAMPS %u   EXPORTS %u",lab.fires,lab.pulses); text(510,120,value);
            std::snprintf(value,sizeof(value),"MODE %u   BRAKES %u",lab.mode+1,lab.brakes); text(510,155,value);
            std::snprintf(value,sizeof(value),"POSITION %+.2f / %+.2f",lab.x,lab.y); text(510,190,value);
            text(510,245,"ARROWS  pen velocity"); text(510,275,"SPACE   stamp marker"); text(510,305,"ENTER   export SVG");
            text(510,335,"SHIFT   pen down / precision"); text(510,365,"W / S   throttle"); text(510,395,"B       brake"); text(510,425,"1 2 3   ink color");
            text(24,480,lab.exportPath.c_str());
            text(24,510,"Keep DOOM focused. Focus loss releases controls and disarms output.");
            xcb_copy_area(connection,frame,window,gc,0,0,0,0,std::uint16_t(width),std::uint16_t(height));
            xcb_flush(connection); lab.Save(); dirty=false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    lab.held=0; lab.Save();
    if (frame!=XCB_NONE) xcb_free_pixmap(connection,frame);
    xcb_free_pixmap(connection,ink); xcb_free_gc(connection,gc); xcb_disconnect(connection);
#endif
    return 0;
}
