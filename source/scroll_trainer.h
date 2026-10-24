#include <atomic>
namespace Trainer {
struct Event {double ms;int direction;};
struct Pattern {std::vector<Event> events;std::vector<double> delays;double average=0,accuracy=0;};
static std::atomic<bool> allowed{false},hookReady{false};
static std::atomic<bool> enabled{false};
static CRITICAL_SECTION lock;
static std::deque<Event> pending;
static std::vector<Event> current;
static std::deque<Pattern> history;
static uint64_t totalPatterns=0,totalScrolls=0,upPatterns=0,downPatterns=0,validIntervals=0,intervals=0;
static double intervalSum=0;
static std::deque<double> recent;      // last intervals inside a series, newest last (bar chart)
static LARGE_INTEGER frequency;
static HHOOK hook;
static HANDLE workerHandle;
static HANDLE workerStartupEvent;
static DWORD workerThreadId;
static bool lockInitialized,started;
static std::atomic<unsigned> dropped{0};
static double now(){LARGE_INTEGER count;QueryPerformanceCounter(&count);LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);return 1000.*count.QuadPart/freq.QuadPart;}
static Pattern describe(const std::vector<Event> &events){
    Pattern p;p.events=events;int good=0;
    for(size_t i=1;i<events.size();++i){double delay=events[i].ms-events[i-1].ms;p.delays.push_back(delay);p.average+=delay;if(std::abs(delay-20.)<=2.)++good;}
    if(!p.delays.empty()){p.average/=p.delays.size();p.accuracy=100.*good/p.delays.size();}return p;
}
static void finish(){
    if(current.size()>=2){Pattern p=describe(current);++totalPatterns;totalScrolls+=current.size();bool up=true,down=true;
        for(const auto &event:current){up&=event.direction>0;down&=event.direction<0;}
        if(up)++upPatterns;if(down)++downPatterns;
        for(double delay:p.delays){++intervals;intervalSum+=delay;if(std::abs(delay-20.)<=2.)++validIntervals;}
        history.push_front(std::move(p));if(history.size()>64)history.pop_back();
    }current.clear();
}
static void feed(Event event){
    if(!std::isfinite(event.ms) || (event.direction!=1&&event.direction!=-1))return;
    if(!current.empty() && (event.ms<current.back().ms || event.ms-current.back().ms>100.))finish();
    if(!current.empty()){recent.push_back(event.ms-current.back().ms);if(recent.size()>36)recent.pop_front();}
    if(current.size()>=1024)finish();current.push_back(event);
}
static void reset(){allowed=false;if(hookReady){EnterCriticalSection(&lock);pending.clear();LeaveCriticalSection(&lock);}dropped=0;current.clear();history.clear();recent.clear();totalPatterns=totalScrolls=upPatterns=downPatterns=validIntervals=intervals=0;intervalSum=0;}
static void hookOn();static void hookOff();
static void toggle(){enabled=!enabled;allowed=false;if(!enabled)finish();WritePrivateProfileStringA("trainer","enabled",enabled?"1":"0",iniPath);if(enabled)hookOn();else hookOff();}
static LRESULT CALLBACK callback(int code,WPARAM wp,LPARAM lp){
    CallbackScope callbackScope;
    if(code==HC_ACTION && wp==WM_MOUSEWHEEL && allowed.load(std::memory_order_relaxed)){
        DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
        if(pid==GetCurrentProcessId()){
            auto *mouse=reinterpret_cast<MSLLHOOKSTRUCT *>(lp);int delta=static_cast<short>(HIWORD(mouse->mouseData));
            if(delta){Event event{now(),delta>0?1:-1};EnterCriticalSection(&lock);if(pending.size()<2048)pending.push_back(event);else ++dropped;LeaveCriticalSection(&lock);}
        }
    }return CallNextHookEx(hook,code,wp,lp);
}
static DWORD WINAPI worker(void *){
    MSG initMessage;PeekMessage(&initMessage,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
    hook=SetWindowsHookExW(WH_MOUSE_LL,callback,selfModule,0);if(!hook){logLine("Scroll trainer mouse hook unavailable.");SetEvent(workerStartupEvent);return 0;}
    hookReady=true;SetEvent(workerStartupEvent);MSG message;while(GetMessage(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessage(&message);}hookReady=false;UnhookWindowsHookEx(hook);hook=nullptr;return 0;
}
// The low-level mouse hook sees every mouse event of the whole system, so it is only
// installed while the trainer is on. Before, it stayed installed all game long and
// added work (and a thread switch) to each mouse event even with the trainer off.
static void hookOn(){
    if(!started||workerHandle)return;
    workerStartupEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!workerStartupEvent)return;
    workerHandle=CreateThread(nullptr,0,worker,nullptr,0,&workerThreadId);if(workerHandle)WaitForSingleObject(workerStartupEvent,INFINITE);else workerThreadId=0;
    CloseHandle(workerStartupEvent);workerStartupEvent=nullptr;
}
static void hookOff(){
    if(workerHandle){if(workerThreadId)PostThreadMessageW(workerThreadId,WM_QUIT,0,0);WaitForSingleObject(workerHandle,INFINITE);CloseHandle(workerHandle);workerHandle=nullptr;}
    workerThreadId=0;hookReady=false;hook=nullptr;
}
// The hook itself is created by frame() or toggle(), both on the render thread.
static void start(){if(started)return;InitializeCriticalSection(&lock);lockInitialized=true;started=true;enabled=GetPrivateProfileIntA("trainer","enabled",0,iniPath)!=0;}
static void stop(){
    allowed=false;enabled=false;hookOff();
    if(lockInitialized){pending.clear();current.clear();history.clear();DeleteCriticalSection(&lock);lockInitialized=false;}
    started=false;
}
static void frame(){
    if(enabled&&!workerHandle)hookOn();else if(!enabled&&workerHandle)hookOff();   // e.g. a config profile switched it
    allowed.store(enabled&&!Hud::chatOpened&&(!opened||row==11),std::memory_order_relaxed);
    if(hookReady){std::deque<Event> events;EnterCriticalSection(&lock);events.swap(pending);LeaveCriticalSection(&lock);for(auto event:events)if(enabled)feed(event);}
    if(!current.empty() && now()-current.back().ms>100.)finish();
}
static std::string sequence(const Pattern &p){std::string result;for(auto event:p.events){result+=event.direction>0?'U':'D';if(result.size()>=48){result+="...";break;}}return result;}
}
