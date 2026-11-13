#pragma once
#include "VehicleDynamics.h"
#include "VehicleInteraction.h"

namespace cars {
enum RoadRejection { RoadClear, RoadTerrainMissing, RoadWater, RoadTooSteep, RoadObstacle, RoadRejectionCount };
inline const char* roadRejectionName(RoadRejection r) {
    const char* names[]={"clear","terrain unavailable","water","slope","obstacle"};
    return r>=RoadClear && r<RoadRejectionCount?names[r]:"unknown";
}
struct RoadReport {
    unsigned samples,counts[RoadRejectionCount];RoadRejection first;RigPoint firstPoint;
    RigPoint obstacleFrom,obstacleTo;SurfaceHit obstacleHit;bool hasObstacle;
    std::string obstacleOwner;unsigned obstacleGroup;
    RoadReport():samples(0),first(RoadClear),hasObstacle(false),obstacleGroup(0){for(unsigned n=0;n<RoadRejectionCount;++n)counts[n]=0;}
    bool record(RoadRejection reason,const RigPoint& p) {
        ++counts[reason];if(reason!=RoadClear && first==RoadClear){first=reason;firstPoint=p;}
        return reason==RoadClear;
    }
};
// Preserve the elevation of a native character route across raised decks.
// Renderer terrain alone is below bridges and gate floors. Probe locally above
// the route's layer, not from the sky, so overhead roofs are not selected.
template<class Queries>
double vehicleRoadSurface(Queries& w,const RigPoint& p,double terrain) {
    if(!validPoint(p)||!boardingTerrainAvailable(terrain))return -99;
    const double top=std::max(terrain+2,p.y+16);
    SurfaceHit hit;
    if(w.ray(RigPoint(p.x,top,p.z),RigPoint(p.x,terrain-1,p.z),hit)
        && validPoint(hit.point) && validPoint(hit.normal) && hit.normal.y>.55
        && hit.point.y>=terrain && hit.point.y<=top)
        return hit.point.y;
    return terrain;
}
// Vehicle A* uses loaded terrain and physical clearance. A pedestrian navmesh
// face under the chassis is not required: the live 0.6.4 query rejected the
// parked car's origin before examining any road or destination.
// The radius is horizontal clearance; samples follow the ground on slopes.
template<class Queries>
bool surveyVehicleGround(Queries& w,const RigPoint& p,double radius,double& ground,RoadReport& report,double maxGrade=.85) {
    ++report.samples;
    ground=-99;
    if(!validPoint(p)||!_finite(radius)||radius<=0)return report.record(RoadTerrainMissing,p);
    ground=w.terrainHeight(p);
    if(!boardingTerrainAvailable(ground))return report.record(RoadTerrainMissing,p);
    if(ground<=100)return report.record(RoadWater,p);
    for(unsigned n=0;n<8;++n) {
        double angle=n*3.141592653589793/4;
        RigPoint edge(p.x+cos(angle)*radius,ground,p.z+sin(angle)*radius);
        const double h=w.terrainHeight(edge);
        if(!boardingTerrainAvailable(h))return report.record(RoadTerrainMissing,edge);
        if(h<=100)return report.record(RoadWater,edge);
        if(fabs(h-ground)>radius*maxGrade)return report.record(RoadTooSteep,edge);
        edge.y=h;
        SurfaceHit hit;
        RigPoint from(p.x,ground+5,p.z),to=edge+RigPoint(0,5,0);
        if(w.ray(from,to,hit)) {
            if(!report.hasObstacle){report.hasObstacle=true;report.obstacleFrom=from;report.obstacleTo=to;report.obstacleHit=hit;}
            return report.record(RoadObstacle,edge);
        }
    }
    return report.record(RoadClear,p);
}
// A native pedestrian corridor may cross constructed ramps around 44 degrees.
// Allow that modest extra grade only while validating its vehicle-sized strip;
// this does not relax walls, width, water, or the fallback terrain search.
template<class Queries>
bool vehicleNativePassage(Queries& w,const RigPoint& a,const RigPoint& b,double radius,RoadReport& report) {
    if(!validPoint(a)||!validPoint(b)||!_finite(radius)||radius<=0)return false;
    const double length=(b-a).length(),spacing=std::max(2.0,radius*.5);
    if(length>128*spacing)return false;
    const unsigned count=static_cast<unsigned>(ceil(length/spacing));
    const RigPoint horizontal(b.x-a.x,0,b.z-a.z);
    const double step=count?horizontal.length()/count:0;
    double prior=a.y;
    for(unsigned n=0;n<=count;++n){
        double h;const RigPoint p=count?a+(b-a)*(double(n)/count):a;
        if(!surveyVehicleGround(w,p,radius,h,report,1.05)||(n && fabs(h-prior)>step*1.05+1))return false;
        prior=h;
    }
    return true;
}
}
