#pragma once
// One background thread for disk writes (settings file, chat history), so the
// render thread never waits for the disk or an antivirus scan of the file.
#include <deque>
#include <functional>
namespace BackgroundIo {
static SRWLOCK lock=SRWLOCK_INIT;
static CONDITION_VARIABLE wake=CONDITION_VARIABLE_INIT;
static std::deque<std::function<void()>> jobs;
static HANDLE thread;
static bool stopping;
static unsigned busy;
static DWORD WINAPI run(void *){
    for(;;){
        AcquireSRWLockExclusive(&lock);
        while(jobs.empty()&&!stopping)SleepConditionVariableSRW(&wake,&lock,INFINITE,0);
        if(jobs.empty()){ReleaseSRWLockExclusive(&lock);return 0;}
        auto job=std::move(jobs.front());jobs.pop_front();++busy;
        ReleaseSRWLockExclusive(&lock);
        job();
        AcquireSRWLockExclusive(&lock);--busy;WakeAllConditionVariable(&wake);ReleaseSRWLockExclusive(&lock);
    }
}
// Runs the job on the I/O thread; after stop() (plugin unloading) it runs right here.
static void post(std::function<void()> job){
    AcquireSRWLockExclusive(&lock);
    if(!thread&&!stopping)thread=CreateThread(nullptr,0,run,nullptr,0,nullptr);
    if(thread&&!stopping){jobs.push_back(std::move(job));WakeAllConditionVariable(&wake);ReleaseSRWLockExclusive(&lock);return;}
    ReleaseSRWLockExclusive(&lock);job();
}
// Waits until every queued job has finished.
static void drain(){
    AcquireSRWLockExclusive(&lock);
    while(!jobs.empty()||busy)SleepConditionVariableSRW(&wake,&lock,INFINITE,0);
    ReleaseSRWLockExclusive(&lock);
}
// Finishes the queue and ends the thread; required before the DLL is freed.
static void stop(){
    AcquireSRWLockExclusive(&lock);stopping=true;WakeAllConditionVariable(&wake);HANDLE running=thread;ReleaseSRWLockExclusive(&lock);
    if(running){WaitForSingleObject(running,INFINITE);CloseHandle(running);}
    AcquireSRWLockExclusive(&lock);thread=nullptr;ReleaseSRWLockExclusive(&lock);
}
}
