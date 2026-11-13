#pragma once
// Included once by FittingRuntime. Record native fault context without catching,
// retrying, or suppressing the exception. No engine calls or C++ allocation in
// the handler: the interrupted code may hold an engine/allocator lock.
namespace cars {
__declspec(thread) const char* vehicleFaultStage=0;
struct VehicleFaultScope {
    const char* previous;
    explicit VehicleFaultScope(const char* stage):previous(vehicleFaultStage){vehicleFaultStage=stage;}
    ~VehicleFaultScope(){vehicleFaultStage=previous;}
};
static wchar_t vehicleFaultFile[32768]={0};
static volatile LONG vehicleFaultWriting=0;
static void faultWrite(HANDLE file,const char* line){DWORD done=0;WriteFile(file,line,lstrlenA(line),&done,0);}
static LONG CALLBACK vehicleFaultTrace(EXCEPTION_POINTERS* e) {
    if(!vehicleFaultStage || !e || !e->ExceptionRecord || !e->ContextRecord)return EXCEPTION_CONTINUE_SEARCH;
    DWORD code=e->ExceptionRecord->ExceptionCode;
    if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_IN_PAGE_ERROR && code!=EXCEPTION_ILLEGAL_INSTRUCTION)return EXCEPTION_CONTINUE_SEARCH;
    if(!vehicleFaultFile[0] || InterlockedCompareExchange(&vehicleFaultWriting,1,0))return EXCEPTION_CONTINUE_SEARCH;
    HANDLE file=CreateFileW(vehicleFaultFile,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(file!=INVALID_HANDLE_VALUE) {
        char text[1024];SYSTEMTIME time;GetLocalTime(&time);
        wsprintfA(text,"\r\nProject Cars 0.8.4 native exception (first chance, not proof of a fatal crash)\r\n%04u-%02u-%02u %02u:%02u:%02u.%03u thread=%lu stage=%s\r\ncode=%08lx instruction=%p address=%p operation=%p\r\n",
            time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,GetCurrentThreadId(),vehicleFaultStage,
            code,e->ExceptionRecord->ExceptionAddress,reinterpret_cast<void*>(e->ExceptionRecord->NumberParameters>1?e->ExceptionRecord->ExceptionInformation[1]:0),
            reinterpret_cast<void*>(e->ExceptionRecord->NumberParameters?e->ExceptionRecord->ExceptionInformation[0]:0));
        faultWrite(file,text);
        wsprintfA(text,"RIP=%p RSP=%p RCX=%p RDX=%p R8=%p R9=%p\r\n",reinterpret_cast<void*>(e->ContextRecord->Rip),reinterpret_cast<void*>(e->ContextRecord->Rsp),reinterpret_cast<void*>(e->ContextRecord->Rcx),reinterpret_cast<void*>(e->ContextRecord->Rdx),reinterpret_cast<void*>(e->ContextRecord->R8),reinterpret_cast<void*>(e->ContextRecord->R9));faultWrite(file,text);
        void* stack[48];USHORT count=CaptureStackBackTrace(0,48,stack,0);
        for(unsigned n=0;n<=count;++n) {
            void* pc=n?stack[n-1]:e->ExceptionRecord->ExceptionAddress;MEMORY_BASIC_INFORMATION region;
            wchar_t path[MAX_PATH]={0};void* base=0;
            if(VirtualQuery(pc,&region,sizeof(region)) && region.Type==MEM_IMAGE){base=region.AllocationBase;GetModuleFileNameW(static_cast<HMODULE>(base),path,MAX_PATH);}
            char utf8[768]={0};WideCharToMultiByte(CP_UTF8,0,path,-1,utf8,sizeof(utf8),0,0);
            wsprintfA(text,"%02u %p base=%p rva=%p %s\r\n",n,pc,base,reinterpret_cast<void*>(reinterpret_cast<ULONG_PTR>(pc)-reinterpret_cast<ULONG_PTR>(base)),utf8);faultWrite(file,text);
        }
        FlushFileBuffers(file);CloseHandle(file);
    }
    InterlockedExchange(&vehicleFaultWriting,0);
    return EXCEPTION_CONTINUE_SEARCH;
}
inline void installVehicleFaultTrace() {
    HMODULE module=0;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&vehicleFaultTrace),&module))return;
    DWORD size=GetModuleFileNameW(module,vehicleFaultFile,32768);
    if(!size || size>=32720){vehicleFaultFile[0]=0;return;}
    while(size && vehicleFaultFile[size-1]!=L'\\' && vehicleFaultFile[size-1]!=L'/')--size;
    if(!size){vehicleFaultFile[0]=0;return;}
    lstrcpyW(vehicleFaultFile+size,L"ProjectCars-native-exceptions.log");
    if(!AddVectoredExceptionHandler(1,vehicleFaultTrace))vehicleFaultFile[0]=0;
}
}
