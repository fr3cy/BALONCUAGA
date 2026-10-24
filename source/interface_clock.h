#pragma once
namespace InterfaceClock {
static SYSTEMTIME shifted(const SYSTEMTIME &utc,int minutes){
    FILETIME file={};SYSTEMTIME result={};if(!SystemTimeToFileTime(&utc,&file))return result;
    ULARGE_INTEGER value;value.LowPart=file.dwLowDateTime;value.HighPart=file.dwHighDateTime;
    value.QuadPart+=static_cast<int64_t>(std::clamp(minutes,-720,840))*600000000LL;
    file.dwLowDateTime=value.LowPart;file.dwHighDateTime=value.HighPart;FileTimeToSystemTime(&file,&result);return result;
}
static SYSTEMTIME now(int offset,bool system){SYSTEMTIME value={};if(system)GetLocalTime(&value);else{SYSTEMTIME utc;GetSystemTime(&utc);value=shifted(utc,offset);}return value;}
}
