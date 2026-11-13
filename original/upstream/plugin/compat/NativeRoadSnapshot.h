#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
namespace cars {
struct NativeRoadCopy {
    unsigned total,count;unsigned __int64 manager,list,ends[2];
    float points[4096][4];
};
// Copy one inspected Steam 1.0.65 road record. Reacquire the manager/list/road
// on every call. This POD-only boundary never takes ownership of native data.
inline bool copyRoadRecord(const unsigned char* manager,unsigned index,NativeRoadCopy& out) {
    __try {
        if(!manager)return false;
        out.manager=reinterpret_cast<unsigned __int64>(manager);
        out.total=*reinterpret_cast<const unsigned*>(manager+0x18);
        const unsigned char* const* roads=*reinterpret_cast<const unsigned char* const* const*>(manager+0x20);
        out.list=reinterpret_cast<unsigned __int64>(roads);out.count=0;
        if(out.total>32768 || (out.total && !roads))return false;
        if(index>=out.total)return true;
        const unsigned char* road=roads[index];if(!road)return true;
        out.count=*reinterpret_cast<const unsigned*>(road+8);
        if(out.count<2){out.count=0;return true;}if(out.count>4096)return false;
        const void* vertices=*reinterpret_cast<const void* const*>(road+0x10);if(!vertices)return false;
        out.ends[0]=*reinterpret_cast<const unsigned __int64*>(road+0x18);
        out.ends[1]=*reinterpret_cast<const unsigned __int64*>(road+0x20);
        memcpy(out.points,vertices,out.count*16);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
}
