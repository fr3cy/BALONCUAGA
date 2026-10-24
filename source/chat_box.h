#pragma once
namespace ChatBox {
enum Theme {Css,Plain,ThemeCount};
static int theme=Css;
static int fontPercent=100,widthPercent=100,lineCount=5;
static float textScale(){return std::clamp(fontPercent,70,150)/100.f;}
static float panelWidth(float base){return base*std::clamp(widthPercent,50,160)/100.f;}
static const char *themeNames[]={"CSS",u8"Обычный с фоном"};
static void loadTheme(){auto get=[](const char *key,int fallback,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath)),lo,hi);};theme=get("chat_theme",Css,0,ThemeCount-1);fontPercent=get("chat_font_percent",100,70,150);widthPercent=get("chat_width_percent",100,50,160);lineCount=get("chat_lines",5,2,12);}
static void saveTheme(){auto put=[](const char *key,int v){char value[12];snprintf(value,sizeof(value),"%d",v);WritePrivateProfileStringA("interface",key,value,iniPath);};put("chat_theme",theme);put("chat_font_percent",fontPercent);put("chat_width_percent",widthPercent);put("chat_lines",lineCount);}
static bool timeOn=true,serverOn=true,deadOn=true,collapsed=false;
static uint64_t cleared=0;
static bool server(const Hud::ChatLine &line){return line.serverMessage||line.nick=="Server"||line.nick.empty();}
static bool visible(const Hud::ChatLine &line){return line.serial>cleared&&(serverOn||!server(line))&&(deadOn||(!line.deadMessage&&line.body.find("[Dead]")==std::string::npos&&line.body.find("*DEAD*")==std::string::npos));}
static void rounded(int x,int y,int w,int h,int radius,int r,int g,int b,int alpha,bool outline=false){
    glColor4ub(r,g,b,alpha);glBegin(outline?GL_LINE_LOOP:GL_TRIANGLE_FAN);if(!outline)glVertex2f(x+w/2.f,y+h/2.f);
    for(int corner=0;corner<4;++corner){float cx=corner==0||corner==3?x+radius:x+w-radius,cy=corner<2?y+radius:y+h-radius;
        for(int step=0;step<=12;++step){float angle=(180+corner*90+step*90.f/12)*.017453293f;glVertex2f(cx+std::cos(angle)*radius,cy+std::sin(angle)*radius);}}
    if(!outline)glVertex2i(x,y+radius);glEnd();
}
static void action(int value){
    if(value==0)timeOn=!timeOn;else if(value==1)serverOn=!serverOn;else if(value==2)deadOn=!deadOn;
    else if(value==3)collapsed=!collapsed;else if(value==4)cleared=Hud::chatSerial;
    else if(value==5)ChatEditor::team=!ChatEditor::team;
    else if(value==6)PostMessage(ChatScroll::window,WM_INPUTLANGCHANGEREQUEST,0,static_cast<LPARAM>(HKL_NEXT));
    else if(value==7)ChatEditor::send();Hud::chatScroll=0;Weave::savePreferences();
}
static void drawPanel(const SCREENINFO &screen,int x,int y,int w,int h,bool interactive,bool fullHistory){
    Hud::ChatFontScope font;
    struct Row {const Hud::ChatLine *message;std::vector<Hud::ChatSpan> spans;bool first;};std::vector<Row> rows;ULONGLONG tick=GetTickCount64();
    for(const auto &m:Hud::history){if(!visible(m))continue;if(!fullHistory&&(!m.tick||tick<m.tick||tick-m.tick>=12000))continue;
        auto lines=Hud::wrapChatSpans(m,static_cast<float>(w-30-(timeOn?58:0)),[](const std::string &s){return static_cast<float>(Hud::width(s));});
        for(size_t i=0;i<lines.size();++i)rows.push_back({&m,std::move(lines[i]),i==0});}
    if(rows.empty()&&!interactive)return;
    if(!interactive){if(!collapsed){int end=static_cast<int>(rows.size()),begin=std::max(0,end-5),ty=y+8;
        for(int i=begin;i<end;++i){const auto &line=rows[i];int tx=x+16;if(timeOn){if(line.first){Hud::TimeFontScope small;Hud::colorText(tx,ty+3,line.message->date.size()>=16?line.message->date.substr(11,5):"",146,151,147);}tx+=58;}
            for(const auto &span:line.spans){int r,g,b;Hud::nativeChatRgb(span.code,line.message->side,r,g,b);Hud::chatText(tx,ty,span.text,r,g,b);tx+=Hud::width(span.text);}ty+=27;}}
        return;}
    {Hud::GL2D state(screen.iWidth,screen.iHeight);glViewport(0,0,screen.iWidth,screen.iHeight);rounded(x,y,w,h,10,24,26,24,125);
        glColor4ub(163,173,160,65);glBegin(GL_LINES);glVertex2i(x+1,y+44);glVertex2i(x+w-1,y+44);glEnd();}
    const char *labels[]={"Time","Server","Dead"};int bx=x+10;
    for(int i=0;i<3;++i){int bw=i==1?68:60;bool active=i==0?timeOn:i==1?serverOn:deadOn;
        if(active){Hud::GL2D state(screen.iWidth,screen.iHeight);rounded(bx,y+5,bw,36,3,118,176,46,24);eng->pfnFillRGBA(bx+8,y+39,bw-16,2,158,209,77,255);}
        {Hud::TimeFontScope small;Hud::colorText(bx+12,y+16,labels[i],active?205:204,active?229:207,active?169:204);}
        if(interactive)Mouse::target(bx,y+5,bw,36,Mouse::Custom,i);bx+=bw+4;}
    {Hud::TimeFontScope small;Hud::colorText(x+w-146,y+16,collapsed?"Show chat":"Hide chat",204,207,204);Hud::colorText(x+w-54,y+16,"Clear",204,207,204);}
    if(interactive){Mouse::target(x+w-150,y+5,88,36,Mouse::Custom,3);Mouse::target(x+w-62,y+5,54,36,Mouse::Custom,4);}
    int capacity=std::min(5,std::max(1,(h-106)/27));Hud::chatScrollLimit=std::max(0,static_cast<int>(rows.size())-capacity);Hud::chatScroll=std::clamp(Hud::chatScroll,0,Hud::chatScrollLimit);
    if(!collapsed){int end=static_cast<int>(rows.size())-Hud::chatScroll,begin=std::max(0,end-capacity),ty=y+57;
        for(int i=begin;i<end;++i){const auto &line=rows[i];int tx=x+16;
            if(timeOn){if(line.first){Hud::TimeFontScope small;Hud::colorText(tx,ty+3,line.message->date.size()>=16?line.message->date.substr(11,5):"",146,151,147);}tx+=58;}
            for(const auto &span:line.spans){int r,g,b;Hud::nativeChatRgb(span.code,line.message->side,r,g,b);Hud::chatText(tx,ty,span.text,r,g,b);tx+=Hud::width(span.text);}ty+=27;}}
    int fy=y+h-47,channel=ChatEditor::team?96:66;
    {Hud::GL2D state(screen.iWidth,screen.iHeight);rounded(x+12,fy,w-24,35,6,30,32,30,65);rounded(x+12,fy,w-24,35,6,150,158,146,170,true);}
    {Hud::TimeFontScope small;Hud::colorText(x+23,fy+10,ChatEditor::team?u8"КОМАНДА":u8"ВСЕ",204,207,204);Hud::colorText(x+w-65,fy+11,std::to_string(ChatEditor::input.size())+"/100",148,155,145);}
    int ix=x+26+channel,inputWidth=w-112-channel;auto value=ChatEditor::input;
    while(!value.empty()&&Hud::width(Hud::utf8(value))>inputWidth)value.erase(0,1);
    std::string text=value.empty()?(ChatEditor::team?u8"Сказать команде…":u8"Сказать всем…"):Hud::utf8(value);Hud::colorText(ix,fy+7,Hud::fitPixels(text,inputWidth),value.empty()?139:218,value.empty()?146:220,value.empty()?138:215);
    if(interactive){Mouse::target(x+14,fy+1,channel,32,Mouse::Custom,5);Mouse::target(ix,fy+1,inputWidth,32,Mouse::Custom,8);}
    {Hud::GL2D state(screen.iWidth,screen.iHeight);rounded(x+1,y+1,w-2,h-2,10,Hud::chatBorderRgb[0],Hud::chatBorderRgb[1],Hud::chatBorderRgb[2],255,true);}
}
static void draw(const SCREENINFO &screen){if(!Hud::chatOn||!Hud::oldSay||!Hud::chatPresentationReady)return;int w=std::min(760,screen.iWidth-24),h=std::min(Hud::chatOpened?258:151,screen.iHeight-80);drawPanel(screen,12,std::max(8,screen.iHeight-h-48),w,h,Hud::chatOpened,Hud::chatOpened);}
}
