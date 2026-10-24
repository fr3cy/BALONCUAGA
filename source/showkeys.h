#pragma once
// Showkeys: on-screen movement keys (WASD, Ctrl, Shift, Space, mouse buttons).
// Reads the physical key state only while the game window is in the foreground.
namespace Showkeys {
enum { Glow, Filled, Soft, Minimal, StyleCount };
static bool enabled=false,tilt=true,wheel=true; // wheel: mouse wheel cell under the keys
static int style=Glow,sizePercent=100,layout=0;
static int accent[3]={255,176,32},text[3]={255,255,255};
static float posX=50,posY=78;           // widget centre, percent of the screen
static std::vector<std::vector<int>> rows;
struct Key {int vk;const char *label;float width;};
static const Key keys[]={
    {'W',"W",1},{'A',"A",1},{'S',"S",1},{'D',"D",1},{'Q',"Q",1},{'E',"E",1},{'R',"R",1},{'F',"F",1},{'C',"C",1},{'V',"V",1},
    {'Z',"Z",1},{'X',"X",1},{'G',"G",1},{'T',"T",1},{VK_CONTROL,"CTRL",1.55f},{VK_SHIFT,"SHIFT",1.55f},{VK_SPACE,"SPACE",3.2f},
    {VK_MENU,"ALT",1.3f},{VK_TAB,"TAB",1.3f},{VK_LBUTTON,"M1",1.1f},{VK_RBUTTON,"M2",1.1f},{VK_MBUTTON,"M3",1.1f},{VK_XBUTTON1,"M4",1.1f},{VK_XBUTTON2,"M5",1.1f}};
static const Key *find(int vk){for(const auto &k:keys)if(k.vk==vk)return &k;return nullptr;}
struct Preset {const char *name;std::vector<std::vector<int>> rows;};
static const std::vector<Preset> &presets(){
    static const std::vector<Preset> list={
        {"WASD + Ctrl + Shift",{{'W'},{'A','S','D'},{VK_CONTROL,VK_SHIFT}}},
        {"WASD",{{'W'},{'A','S','D'}}},
        {"WASD + Space",{{'W'},{'A','S','D'},{VK_SPACE}}},
        {"WASD + Ctrl + Space",{{'W'},{'A','S','D'},{VK_CONTROL,VK_SPACE}}},
        {"M1 + M2",{{VK_LBUTTON,VK_RBUTTON}}}};
    return list;
}
constexpr int MaxRows=4,MaxPerRow=6;
static void applyPreset(int index){index=std::clamp(index,0,static_cast<int>(presets().size())-1);layout=index;rows=presets()[index].rows;}
static void defaults(){enabled=false;tilt=true;wheel=true;style=Glow;sizePercent=100;posX=50;posY=78;accent[0]=255;accent[1]=176;accent[2]=32;text[0]=text[1]=text[2]=255;applyPreset(0);}
// Rows are stored as "87;65,83,68;17,16".
static std::string encode(){
    std::string out;for(size_t r=0;r<rows.size();++r){if(r)out+=';';for(size_t i=0;i<rows[r].size();++i){if(i)out+=',';out+=std::to_string(rows[r][i]);}}return out;
}
static bool decode(const std::string &value,std::vector<std::vector<int>> &out){
    out.clear();std::vector<int> row;std::string number;
    auto flushNumber=[&]{if(number.empty())return true;int vk=atoi(number.c_str());number.clear();if(!find(vk)||row.size()>=MaxPerRow)return false;row.push_back(vk);return true;};
    for(char c:value){
        if(c>='0'&&c<='9'){number+=c;if(number.size()>3)return false;}
        else if(c==','){if(!flushNumber())return false;}
        else if(c==';'){if(!flushNumber())return false;if(out.size()>=MaxRows)return false;out.push_back(row);row.clear();}
        else return false;
    }
    if(!flushNumber())return false;if(!row.empty()||!out.empty()){if(out.size()>=MaxRows)return false;out.push_back(row);}
    return !out.empty();
}
static void addKey(int row,int vk){if(!find(vk))return;if(row>=static_cast<int>(rows.size())){if(rows.size()>=MaxRows)return;rows.push_back({});row=static_cast<int>(rows.size())-1;}
    if(row<0||rows[row].size()>=MaxPerRow)return;rows[row].push_back(vk);layout=-1;}
static void removeKey(int row,int index){if(row<0||row>=static_cast<int>(rows.size())||index<0||index>=static_cast<int>(rows[row].size()))return;
    rows[row].erase(rows[row].begin()+index);if(rows[row].empty())rows.erase(rows.begin()+row);layout=-1;}
static void save(){
    auto put=[](const char *key,int value){char t[16];snprintf(t,sizeof(t),"%d",value);WritePrivateProfileStringA("interface",key,t,iniPath);};
    put("showkeys",enabled);put("showkeys_style",style);put("showkeys_size",sizePercent);put("showkeys_tilt",tilt);put("showkeys_wheel",wheel);put("showkeys_layout",layout);
    put("showkeys_x",static_cast<int>(std::lround(posX*10)));put("showkeys_y",static_cast<int>(std::lround(posY*10)));
    for(int i=0;i<3;++i){char k[32];snprintf(k,sizeof(k),"showkeys_accent_%d",i);put(k,accent[i]);snprintf(k,sizeof(k),"showkeys_text_%d",i);put(k,text[i]);}
    WritePrivateProfileStringA("interface","showkeys_rows",encode().c_str(),iniPath);
}
static void load(){
    auto get=[](const char *key,int fallback,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath)),lo,hi);};
    defaults();enabled=get("showkeys",0,0,1)!=0;style=get("showkeys_style",Glow,0,StyleCount-1);sizePercent=get("showkeys_size",100,50,200);tilt=get("showkeys_tilt",1,0,1)!=0;wheel=get("showkeys_wheel",1,0,1)!=0;
    posX=get("showkeys_x",500,0,1000)/10.f;posY=get("showkeys_y",780,0,1000)/10.f;
    for(int i=0;i<3;++i){char k[32];snprintf(k,sizeof(k),"showkeys_accent_%d",i);accent[i]=get(k,accent[i],0,255);snprintf(k,sizeof(k),"showkeys_text_%d",i);text[i]=get(k,text[i],0,255);}
    int preset=get("showkeys_layout",0,-1,static_cast<int>(presets().size())-1);char value[128]={};
    GetPrivateProfileStringA("interface","showkeys_rows","",value,sizeof(value),iniPath);std::vector<std::vector<int>> parsed;
    if(*value&&decode(value,parsed)){rows=parsed;layout=preset;}else applyPreset(preset<0?0:preset);
}
// The game window's focus is looked up once per timer tick, not again for every key.
static bool focused(){
    static ULONGLONG checked=~0ULL;static bool value;ULONGLONG now=GetTickCount64();
    if(now!=checked){checked=now;DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);value=pid==GetCurrentProcessId();}
    return value;
}
static bool down(int vk){return focused()&&(GetAsyncKeyState(vk)&0x8000);}
// Smoothed press state per key, advanced once per frame.
static std::unordered_map<int,float> press;
static ULONGLONG lastPress;
static float pressed(int vk,bool demo,float time){
    bool on=down(vk);if(on)lastPress=GetTickCount64();
    if(demo&&!on&&GetTickCount64()-lastPress>1200){
        // Preview cycle when nothing is held: W, then Shift, then D.
        float t=std::fmod(time,3.f);
        on=(vk=='W'&&t<1.6f)||(vk==VK_SHIFT&&t>.6f&&t<2.2f)||(vk=='D'&&t>1.8f&&t<2.6f);
    }
    float &value=press[vk+(demo?1000:0)];value=ImLerp(value,on?1.f:0.f,std::min(1.f,18.f*ImGui::GetIO().DeltaTime));return value;
}
static ImU32 rgba(const int c[3],float a){return IM_COL32(c[0],c[1],c[2],static_cast<int>(std::clamp(a,0.f,1.f)*255));}
static ImU32 mix(ImU32 a,ImU32 b,float t){return ImGui::ColorConvertFloat4ToU32(ImLerp(ImGui::ColorConvertU32ToFloat4(a),ImGui::ColorConvertU32ToFloat4(b),t));}
// ---- Mouse wheel ------------------------------------------------------------
// A series is consecutive notches in one direction with gaps up to 250 ms.
struct Wheel {double last=0,interval=0;int count=0,direction=0;ULONGLONG tick=0;};
static Wheel wheelState;
static double nowMs(){LARGE_INTEGER c,f;QueryPerformanceCounter(&c);QueryPerformanceFrequency(&f);return 1000.*c.QuadPart/f.QuadPart;}
// Called from the game window procedure for every WM_MOUSEWHEEL.
static void wheelEvent(int delta){
    if(!delta)return;int dir=delta>0?1:-1;double t=nowMs();auto &w=wheelState;
    if(w.count>0&&dir==w.direction&&t-w.last<=250){w.interval=t-w.last;++w.count;}else{w.count=1;w.interval=0;}
    w.direction=dir;w.last=t;w.tick=GetTickCount64();
}
// Unscaled widget size for the current rows (unit = one letter key).
static float rowWidth(const std::vector<int> &row,float unit){float gap=unit*.16f,rw=0;for(size_t i=0;i<row.size();++i){auto *k=find(row[i]);rw+=(k?k->width:1)*unit+(i?gap:0);}return rw;}
static float wheelWidth(float unit){return unit*3.2f;}
static int lineCount(){return static_cast<int>(rows.size())+(wheel?1:0);}
static ImVec2 extent(float unit){
    float gap=unit*.16f,w=wheel?wheelWidth(unit):0;for(const auto &row:rows)w=std::max(w,rowWidth(row,unit));
    int lines=lineCount();float h=lines?lines*unit+(lines-1)*gap:0;return {w+unit*.5f,h+unit*.5f};
}
static float unitFor(float scale){return 34.f*std::clamp(sizePercent,50,200)/100.f*scale;}
// Key cap background/outline for the current style; t is the press amount 0..1.
static void keyBox(ImDrawList *d,ImVec2 a,ImVec2 b,float t,float unit,float alpha){
    float radius=unit*.22f;ImU32 acc=rgba(accent,alpha);
    if(style==Glow){
        d->AddRectFilled(a,b,IM_COL32(18,16,14,static_cast<int>(210*alpha)),radius);
        for(int i=1;i<=4;++i){float e=i*1.6f*unit/34;d->AddRect(a-ImVec2(e,e),b+ImVec2(e,e),rgba(accent,alpha*(.12f+.05f*t)*t*(5-i)/4),radius+e,0,2);}
        d->AddRect(a,b,rgba(accent,alpha*(.55f+.45f*t)),radius,0,std::max(1.f,1.6f*unit/34));
    }else if(style==Filled)d->AddRectFilled(a,b,mix(IM_COL32(44,42,46,static_cast<int>(235*alpha)),acc,t),radius);
    else if(style==Soft){
        d->AddRectFilled(a,b,mix(IM_COL32(22,20,24,static_cast<int>(225*alpha)),rgba(accent,alpha*.38f),t),radius);
        d->AddRect(a,b,mix(IM_COL32(74,72,80,static_cast<int>(255*alpha)),acc,t),radius,0,std::max(1.f,1.5f*unit/34));
    }
}
static ImU32 keyText(float t,float alpha){
    if(style==Glow)return mix(rgba(text,alpha*.92f),rgba(accent,alpha),t*.85f);
    if(style==Filled)return mix(IM_COL32(176,174,180,static_cast<int>(255*alpha)),IM_COL32(24,18,10,static_cast<int>(255*alpha)),t);
    if(style==Soft)return mix(IM_COL32(196,194,200,static_cast<int>(255*alpha)),rgba(text,alpha),t);
    return mix(IM_COL32(128,126,134,static_cast<int>(230*alpha)),rgba(text,alpha),t);
}
static void keyLabel(ImDrawList *d,ImFont *font,float fs,ImVec2 p,const char *label,float t,float unit,float alpha){
    if(style==Glow&&t>.02f)for(int i=0;i<8;++i){float ang=i*.785398f,r=1.1f*unit/34;d->AddText(font,fs,p+ImVec2(std::cos(ang)*r,std::sin(ang)*r),rgba(accent,alpha*.18f*t),label);}
    d->AddText(font,fs,p,keyText(t,alpha),label);
}
// Minimal style marks a pressed key with an accent bar under its label.
static void minimalBar(ImDrawList *d,ImVec2 a,ImVec2 b,float textBottom,float width,float t,float unit,float alpha){
    float lw=width*.9f*t;ImVec2 c((a.x+b.x)/2,textBottom+unit*.1f);
    if(style==Minimal&&lw>.5f)d->AddRectFilled(c-ImVec2(lw/2,0),c+ImVec2(lw/2,std::max(2.f,2.6f*unit/34)),rgba(accent,alpha),2);
}
// Wheel cell: direction arrow, notches in the series and the last interval.
static void drawWheel(ImDrawList *d,ImVec2 a,ImVec2 b,float unit,float alpha,bool demo,float time){
    ImFont *font=ImGui::GetIO().Fonts->Fonts[3];ULONGLONG now=GetTickCount64();
    int count=wheelState.count,direction=wheelState.direction;double interval=wheelState.interval;ULONGLONG age=now-wheelState.tick;
    if(demo&&(wheelState.tick==0||age>3000)){
        // Preview: a 6-notch burst downwards then upwards, 18 ms apart.
        float t=std::fmod(time,2.4f);int step=static_cast<int>(t/.12f);
        count=std::min(step,6)+1;direction=std::fmod(time,4.8f)<2.4f?-1:1;interval=18;age=static_cast<ULONGLONG>(std::max(0.f,t-std::min(step,6)*.12f)*1000);
    }
    bool recent=(demo||wheelState.tick)&&age<3000;float flash=recent?std::clamp(1.f-age/450.f,0.f,1.f):0;
    keyBox(d,a,b,flash,unit,alpha);
    float fs=unit*.32f,cy=(a.y+b.y)/2;
    if(!recent){const char *label="WHEEL";ImVec2 ts=font->CalcTextSizeA(fs,FLT_MAX,0,label);keyLabel(d,font,fs,(a+b)/2-ts/2,label,0,unit,alpha);return;}
    // Arrow
    float ax=a.x+unit*.42f,s=unit*.16f;ImU32 col=keyText(std::max(flash,.55f),alpha);
    if(direction>0)d->AddTriangleFilled({ax,cy-s},{ax-s,cy+s*.7f},{ax+s,cy+s*.7f},col);
    else d->AddTriangleFilled({ax,cy+s},{ax-s,cy-s*.7f},{ax+s,cy-s*.7f},col);
    char countText[16],msText[24];snprintf(countText,sizeof(countText),"x%d",count);
    if(interval>0)snprintf(msText,sizeof(msText),"%.0f ms",interval);else snprintf(msText,sizeof(msText),"- ms");
    ImVec2 cs=font->CalcTextSizeA(fs,FLT_MAX,0,countText),ms=font->CalcTextSizeA(fs,FLT_MAX,0,msText);
    keyLabel(d,font,fs,{a.x+unit*.78f,cy-cs.y/2},countText,flash,unit,alpha);
    keyLabel(d,font,fs,{b.x-unit*.22f-ms.x,cy-ms.y/2},msText,flash,unit,alpha);
    minimalBar(d,a,b,cy+cs.y/2,b.x-a.x-unit*.4f,flash,unit,alpha);
}
// Draws the keys centred on `center`. `alpha` fades everything (menu preview fade).
static void draw(ImDrawList *d,ImVec2 center,float unit,float alpha,bool demo,float time){
    if(lineCount()==0)return;
    float gap=unit*.16f;ImVec2 size=extent(unit)-ImVec2(unit*.5f,unit*.5f);
    int start=d->VtxBuffer.Size;float y=center.y-size.y/2;
    ImFont *font=ImGui::GetIO().Fonts->Fonts[3];
    for(const auto &row:rows){
        float x=center.x-rowWidth(row,unit)/2;
        for(int vk:row){auto *k=find(vk);if(!k)continue;float w=k->width*unit,t=pressed(vk,demo,time);
            ImVec2 a(x,y),b(x+w,y+unit);const char *label=k->label;
            float fs=(strlen(label)>2?.32f:.42f)*unit;ImVec2 ts=font->CalcTextSizeA(fs,FLT_MAX,0,label);ImVec2 tp=(a+b)/2-ts/2;
            keyBox(d,a,b,t,unit,alpha);keyLabel(d,font,fs,tp,label,t,unit,alpha);
            minimalBar(d,a,b,tp.y+ts.y,std::max(ts.x,unit*.4f),t,unit,alpha);
            x+=w+gap;
        }
        y+=unit+gap;
    }
    if(wheel){float w=wheelWidth(unit);drawWheel(d,{center.x-w/2,y},{center.x+w/2,y+unit},unit,alpha,demo,time);}
    // Italic-style lean: shear around the widget centre.
    if(tilt)for(int i=start;i<d->VtxBuffer.Size;++i){auto &v=d->VtxBuffer[i];v.pos.x+=(center.y-v.pos.y)*.16f;}
}}
