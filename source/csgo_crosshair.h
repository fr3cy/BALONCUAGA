namespace GoCross {
static bool zoomed;
static float shotStrength,predictedSpeed;
static ULONGLONG shotTick,predictionTick;
static double predictionTime=-1;
static bool predictionReady;
static float shooting(ULONGLONG tick){return tick>=shotTick?shotStrength*std::exp(-static_cast<float>(tick-shotTick)/160.f):0.f;}
static void observe(local_state_t *from,local_state_t *to,usercmd_t *cmd,int runfuncs,double time,ULONGLONG tick){
    if(!runfuncs||!from||!to||!cmd)return;
    predictedSpeed=std::sqrt(to->client.velocity[0]*to->client.velocity[0]+to->client.velocity[1]*to->client.velocity[1]);predictionTick=tick;predictionReady=true;
    if(time==predictionTime)return;if(time<predictionTime){shotStrength=0;shotTick=tick;}predictionTime=time;
    int id=to->client.m_iId;if(id<=0||id>=64||from->client.m_iId!=id)return;
    const auto &before=from->weapondata[id];const auto &after=to->weapondata[id];
    if(before.m_iClip>after.m_iClip&&before.m_iClip>0&&after.m_iClip>=0&&!after.m_fInReload){shotStrength=std::min(14.f,shooting(tick)+3.f*(before.m_iClip-after.m_iClip));shotTick=tick;}
}
static void reset(){shotStrength=predictedSpeed=0;shotTick=predictionTick=0;predictionTime=-1;predictionReady=false;}
static void sample(int x,int y,float speed,bool preview=false){
    if(crossStyle<7)return;
    if(crossStyle==12){
        SCREENINFO screen={};screen.iSize=sizeof(screen);if(!eng->pfnGetScreenInfo||!eng->pfnGetScreenInfo(&screen))return;
        float spread=CrossConfig::expansion(speed,preview?0.f:shooting(GetTickCount64()));auto rectangles=CrossConfig::geometry(x,y,spread);int alpha=CrossConfig::alpha();if(alpha<=0)return;
        Hud::GL2D state(screen.iWidth,screen.iHeight);glViewport(0,0,screen.iWidth,screen.iHeight);
        for(bool border:{true,false})for(const auto &r:rectangles)if(r.border==border){glColor4ub(border?0:CrossConfig::rgb[0],border?0:CrossConfig::rgb[1],border?0:CrossConfig::rgb[2],alpha);glBegin(GL_QUADS);glVertex2i(r.x,r.y);glVertex2i(r.x+r.w,r.y);glVertex2i(r.x+r.w,r.y+r.h);glVertex2i(r.x,r.y+r.h);glEnd();}return;
    }
    int type=crossStyle-7,length=type==1?5:8,gap=type==1?2:4,thickness=2;
    if(type==4)gap+=std::min(8,static_cast<int>(std::max(0.f,speed)/45));
    const auto &color=palette[crossColor];
    SCREENINFO screen={};screen.iSize=sizeof(screen);bool direct=eng->pfnGetScreenInfo && eng->pfnGetScreenInfo(&screen);
    auto fill=[&](int px,int py,int w,int h){
        if(direct){Hud::solidRect(screen,px-1,py-1,w+2,h+2,0,0,0);Hud::solidRect(screen,px,py,w,h,color[0],color[1],color[2]);return;}
        eng->pfnFillRGBA(px-1,py-1,w+2,h+2,0,0,0,255);
        eng->pfnFillRGBA(px,py,w,h,color[0],color[1],color[2],255);
    };
    if(type==2){fill(x-1,y-1,3,3);return;}
    fill(x-gap-length,y-1,length,thickness);fill(x+gap,y-1,length,thickness);
    if(type!=3)fill(x-1,y-gap-length,thickness,length);
    fill(x-1,y+gap,thickness,length);
}
static void draw(const SCREENINFO &screen){
    if(crossStyle<7||zoomed||Hud::chatOpened||opened)return;
    auto *player=eng->GetLocalPlayer?eng->GetLocalPlayer():nullptr;
    if(!player || player->index<1 || player->index>32 || player->curstate.spectator || player->curstate.iuser1 || Hud::dots[player->index].dead)return;
    float speed=std::sqrt(player->curstate.velocity[0]*player->curstate.velocity[0]+player->curstate.velocity[1]*player->curstate.velocity[1]);
    ULONGLONG tick=GetTickCount64();if(predictionReady&&tick>=predictionTick&&tick-predictionTick<250)speed=predictedSpeed;
    sample(screen.iWidth/2,screen.iHeight/2,speed);
}
}
