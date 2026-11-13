#pragma once
#include <kenshi/util/hand.h>
namespace cars {
// VC10 groups the SDK's overloaded virtual operator== declarations in a
// different order from Kenshi 1.0.65's hand vtable. a == b dispatches to the
// boolean overload (slot 0), not handle identity (slot 1). Use the mapped
// non-virtual twin, preserving Kenshi's exact identity and null semantics.
inline bool sameHandle(const hand& a,const hand& b) {return a._NV_operator_equal(b);}
}
