namespace ChatEditor {
static std::wstring input;
static size_t caret;
static bool team;
static std::string ansiPending;
static wchar_t highPending;
static void insert(std::wstring value){
    std::wstring clean;for(wchar_t c:value)if(c>=32&&c!=127)clean+=c;
    if(input.size()>=100)return;if(clean.size()>100-input.size())clean.resize(100-input.size());
    if(!clean.empty()&&clean.back()>=0xD800&&clean.back()<=0xDBFF)clean.pop_back();
    caret=std::min(caret,input.size());input.insert(caret,clean);caret+=clean.size();
}
static void begin(bool teamOnly){input.clear();caret=0;ansiPending.clear();highPending=0;team=teamOnly;Hud::chatOpened=true;Hud::chatScroll=0;
    if(eng->pfnClientCmd)eng->pfnClientCmd(const_cast<char *>("-forward;-back;-moveleft;-moveright;-attack;-attack2;-jump;-duck\n"));
}
static void close(){Hud::chatOpened=false;Hud::chatScroll=0;input.clear();caret=0;ansiPending.clear();highPending=0;}
static void send(){
    std::string value=Hud::utf8(input);
    for(char &c:value){if(c=='"')c='\'';if(c=='\\')c='/';if(c=='\n'||c=='\r')c=' ';}
    // Use the server's formatted echo; do not add a second optimistic row.
    if(!value.empty() && eng->pfnClientCmd){std::string command=std::string(team?"say_team \"":"say \"")+value+"\"\n";eng->pfnClientCmd(const_cast<char *>(command.c_str()));}
    close();
}
static void character(WPARAM wp,bool unicode){
    if(wp<32||wp==127)return;
    if(unicode){
        if(wp>=0xD800&&wp<=0xDBFF){highPending=static_cast<wchar_t>(wp);return;}
        if(wp>=0xDC00&&wp<=0xDFFF && highPending){std::wstring value;value+=highPending;value+=static_cast<wchar_t>(wp);highPending=0;insert(value);return;}
        highPending=0;
        if(wp>0xFFFF&&wp<=0x10FFFF){std::wstring value;unsigned code=static_cast<unsigned>(wp)-0x10000;value+=static_cast<wchar_t>(0xD800+(code>>10));value+=static_cast<wchar_t>(0xDC00+(code&1023));insert(value);return;}
        if(wp<0xD800 || (wp>0xDFFF && wp<=0xFFFF))insert(std::wstring(1,static_cast<wchar_t>(wp)));return;
    }
    ansiPending+=static_cast<char>(wp&255);wchar_t result[8];int length=MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,ansiPending.data(),static_cast<int>(ansiPending.size()),result,8);
    if(length){insert(std::wstring(result,result+length));ansiPending.clear();}else if(ansiPending.size()>4)ansiPending.clear();
}
static void key(WPARAM key){
    caret=std::min(caret,input.size());
    if(key==VK_RETURN){send();return;}if(key==VK_ESCAPE){close();return;}
    if(key==VK_BACK && caret){size_t start=caret-1;if(start && input[start]>=0xDC00&&input[start]<=0xDFFF&&input[start-1]>=0xD800&&input[start-1]<=0xDBFF)--start;input.erase(start,caret-start);caret=start;}
    else if(key==VK_DELETE && caret<input.size()){size_t count=input[caret]>=0xD800&&input[caret]<=0xDBFF&&caret+1<input.size()?2:1;input.erase(caret,count);}
    else if(key==VK_LEFT && caret){--caret;if(caret&&input[caret]>=0xDC00&&input[caret]<=0xDFFF)--caret;}
    else if(key==VK_RIGHT && caret<input.size()){if(input[caret]>=0xD800&&input[caret]<=0xDBFF&&caret+1<input.size())caret+=2;else ++caret;}
    else if(key==VK_HOME)caret=0;else if(key==VK_END)caret=input.size();
    else if(key=='V' && GetKeyState(VK_CONTROL)<0 && OpenClipboard(nullptr)){
        HANDLE data=GetClipboardData(CF_UNICODETEXT);if(data){auto *text=static_cast<const wchar_t *>(GlobalLock(data));if(text){size_t limit=std::min<size_t>(4096,GlobalSize(data)/sizeof(wchar_t));insert(std::wstring(text,wcsnlen(text,limit)));GlobalUnlock(data);}}CloseClipboard();
    }
}
}
