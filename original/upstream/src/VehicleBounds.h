#pragma once
#include "VehicleDynamics.h"
#include <float.h>
namespace cars {
// Ogre's not-yet-loaded entities can report infinite bounds. Never let one
// poison the vehicle's spatial box: town activation dereferences its corners.
struct VehicleBounds {
    RigPoint minimum,maximum;bool empty;
    VehicleBounds():empty(true){}
    static bool finite(const RigPoint& p) {
        return validPoint(p) && fabs(p.x)<FLT_MAX/4 && fabs(p.y)<FLT_MAX/4 && fabs(p.z)<FLT_MAX/4;
    }
    bool add(const RigPoint& center,const RigPoint& half) {
        if(!finite(center)||!finite(half)||half.x<0||half.y<0||half.z<0)return false;
        const RigPoint lo=center-half,hi=center+half;
        if(!finite(lo)||!finite(hi))return false;
        if(empty){minimum=lo;maximum=hi;empty=false;}
        else {minimum.x=std::min(minimum.x,lo.x);minimum.y=std::min(minimum.y,lo.y);minimum.z=std::min(minimum.z,lo.z);
              maximum.x=std::max(maximum.x,hi.x);maximum.y=std::max(maximum.y,hi.y);maximum.z=std::max(maximum.z,hi.z);}
        return true;
    }
    // Rounded outwards from the exported light frame's bounds. This remains
    // available before the first mesh callback, without an Ogre cache or pointer.
    bool frame(const RigPoint& origin,const BodyRotation& rotation,double scale) {
        if(!finite(origin)||!_finite(scale)||scale<=0||scale>1000)return false;
        VehicleBounds fallback;
        for(unsigned n=0;n<8;++n) {
            const RigPoint corner((n&1)?22:-24,(n&2)?22:3,(n&4)?12:-12);
            if(!fallback.add(origin+rotation.rotate(corner*scale),RigPoint()))return false;
        }
        return add(fallback.center(),fallback.half());
    }
    RigPoint center()const{return (minimum+maximum)*.5;}
    RigPoint half()const{return (maximum-minimum)*.5;}
};
}
