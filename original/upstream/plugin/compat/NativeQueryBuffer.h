#pragma once
#include <kenshi/util/lektor.h>
namespace cars {
typedef void (*NativeQueryRelease)(void*);
// Kenshi's lektor reserve routine allocates through MFC100U ordinal 265 and
// releases through ordinal 266. The SDK's Ogre allocator base describes the
// layout, not the allocator used by the game's actual pointer buffer.
template<class T> class NativeQueryBuffer {
    NativeQueryRelease release;
    NativeQueryBuffer(const NativeQueryBuffer&);NativeQueryBuffer& operator=(const NativeQueryBuffer&);
public:
    lektor<T> values;
    explicit NativeQueryBuffer(NativeQueryRelease freeBuffer):release(freeBuffer){}
    ~NativeQueryBuffer(){if(values.stuff && release)release(values.stuff);}
};
}
