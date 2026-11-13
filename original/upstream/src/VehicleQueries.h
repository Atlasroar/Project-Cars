#pragma once
#include "VehicleDynamics.h"

namespace cars {
// UtilityT::traceNoActors normalizes direction and uses FLT_MAX as its native
// ray distance. A displacement argument does NOT limit that cast's length.
// Convert its nearest hit to our finite-segment contract before any caller
// counts it as road clearance, suspension contact or a chassis collision.
inline bool boundedVehicleRayHit(const RigPoint& from,const RigPoint& to,const RigPoint& position,const RigPoint& normal,SurfaceHit& out) {
    if(!validPoint(from)||!validPoint(to)||!validPoint(position)||!validPoint(normal))return false;
    const RigPoint travel=to-from,offset=position-from;const double length=travel.length();
    if(length<=1e-5||normal.length()<.5)return false;
    const double along=offset.dot(travel)/length;
    // Account for float resolution at Kenshi world coordinates. Do not let a
    // fixed tolerance turn a tiny per-frame sweep into a long obstacle probe.
    const double epsilon=std::min(.02,length*.01);
    if(along< -epsilon||along>length+epsilon)return false;
    if((offset-travel*(along/length)).length()>.03)return false;
    out.point=position;out.normal=normal.unit();out.distance=bounded(along,0,length);return true;
}
// Unlike traceNoActors, sphereTrace uses the finite motion vector. PhysX's
// sweep result stores t (0..1) in physHit.distance, not a length in world units.
inline bool vehicleSweepDistance(const RigPoint& from,const RigPoint& to,double fraction,double& distance) {
    if(!validPoint(from)||!validPoint(to)||!_finite(fraction)||fraction<0||fraction>1)return false;
    distance=(to-from).length()*fraction;return true;
}
}
