namespace Mouse {
enum Kind { Category, Model, Style, Color, Theme, Look, Option, Fov, History, Back, Page, Hands, Spin, Close, Custom };
struct Hit { int x,y,w,h;Kind kind;int value; };
static std::vector<Hit> hits;
static void (*extraAction)(int);
static int x,y,dragX,dragW;
static bool down,pressed,active,dragging,flushing;
static void (*activate)(),(*deactivate)(),(*clearStates)();
static void (*origActivate)();
static void (*origMouseEvent)(int);
static void activateHook(){CallbackScope callbackScope;if(!opened&&!Hud::chatOpened)origActivate();}
static void eventHook(int state){CallbackScope callbackScope;origMouseEvent(opened||Hud::chatOpened?0:state);}
// True while the game shows its own main menu (no HUD_Redraw); the engine
// mouse must not be captured or released there.
static bool menuScreen;
static void sync() {
    bool desired=opened||Hud::chatOpened;if(active==desired)return;
    active=desired;dragging=false;
    if(menuScreen)return;
    if(desired && origMouseEvent){flushing=true;origMouseEvent(0);flushing=false;}
    if(clearStates)clearStates();
    if(desired){if(deactivate)deactivate();ClipCursor(nullptr);}
    else if(activate)activate();
    down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
}
static void begin(const SCREENINFO &screen) {
    sync();hits.clear();DWORD pid=0;HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);
    POINT point={};RECT rect={};bool focused=pid==GetCurrentProcessId();
    if(focused&&GetCursorPos(&point)&&ScreenToClient(window,&point)&&GetClientRect(window,&rect)&&rect.right>0&&rect.bottom>0){
        x=point.x*screen.iWidth/rect.right;y=point.y*screen.iHeight/rect.bottom;
    }else x=y=-100;
    bool next=focused&&(GetAsyncKeyState(VK_LBUTTON)&0x8000);pressed=next&&!down;down=next;
    if(dragging){if(down){int value=std::max(60,std::min(120,60+(x-dragX)*60/std::max(1,dragW)));if(value!=fov){fov=value;save();}}else dragging=false;}
}
static bool inside(const Hit &h){return x>=h.x&&y>=h.y&&x<h.x+h.w&&y<h.y+h.h;}
static void target(int tx,int ty,int w,int h,Kind kind,int value=0){
    hits.push_back({tx,ty,w,h,kind,value});
    if(inside(hits.back())){const auto &t=Hud::inMenu?Hud::legacyTheme():Hud::hudTheme();eng->pfnFillRGBA(tx,ty,w,1,t.rgb[0],t.rgb[1],t.rgb[2],255);eng->pfnFillRGBA(tx,ty+h-1,w,1,t.rgb[0],t.rgb[1],t.rgb[2],255);}
}
static void choose(const Hit &hit) {
    switch(hit.kind){
    case Custom:if(extraAction)extraAction(hit.value);break;
    case Category:row=hit.value;Hud::historyDetails=false;break;
    case Model:choice[row]=hit.value;break;
    case Style:crossStyle=hit.value;crossPending=true;break;
    case Color:crossColor=hit.value;crossPending=true;break;
    case Theme:Hud::legacyPalette=hit.value;Hud::customLegacy=false;break;
    case Look:Hud::look=hit.value;break;
    case Option:Hud::option=hit.value;Hud::changeOption(x<hit.x+hit.w/2?-1:1);break;
    case Fov:dragging=true;dragX=hit.x;dragW=hit.w;fov=std::max(60,std::min(120,60+(x-dragX)*60/std::max(1,dragW)));break;
    case History:Hud::historySelected=hit.value;Hud::historyDetails=true;Hud::detailScroll=0;break;
    case Back:Hud::historyDetails=false;break;
    case Page:if(Hud::historyDetails)Hud::detailScroll=std::max(0,Hud::detailScroll+hit.value*3);else Hud::navigateHistory(hit.value);break;
    case Hands:Preview::showHands=!Preview::showHands;break;
    case Spin:Preview::spin=!Preview::spin;break;
    case Close:opened=false;sync();break;
    }save();
}
static void finish(const SCREENINFO &screen) {
    if(pressed)for(auto i=hits.rbegin();i!=hits.rend();++i)if(inside(*i)){choose(*i);break;}
    if((!opened&&!Hud::chatOpened)||x<0||y<0)return;
    // Draw our cursor last, independent of the engine's hardware cursor visibility.
    if(wglGetCurrentContext()){
        Hud::GL2D state(screen.iWidth,screen.iHeight);
        glColor4ub(8,12,18,255);glBegin(GL_TRIANGLES);glVertex2f(x-1,y-1);glVertex2f(x-1,y+23);glVertex2f(x+16,y+15);glEnd();
        glColor4ub(245,250,255,255);glBegin(GL_TRIANGLES);glVertex2f(x,y);glVertex2f(x,y+19);glVertex2f(x+12,y+13);glEnd();
    }else{eng->pfnFillRGBA(x,y,2,18,255,255,255,255);eng->pfnFillRGBA(x,y,12,2,255,255,255,255);}
}
}
