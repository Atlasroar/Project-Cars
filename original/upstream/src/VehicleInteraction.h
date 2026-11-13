#pragma once
#include <cmath>
#include <float.h>
#include <string>
namespace cars {
enum VehicleClick { ConfigureVehicle, SitDriver, SitPassenger, OpenTrunk, DriveVehicle, LeaveVehicle, IgnitionVehicle, StopVehicle, PaceVehicle, RightVehicle, ManualDriveVehicle, EndManualDriveVehicle, HeadlightsVehicle, PlaceDriverVehicle, PlaceRearLeftVehicle, PlaceRearRightVehicle, PlaceFrontPoleVehicle, PlaceRearPoleVehicle, PlaceCageVehicle, UnloadPeopleVehicle, ManTurretVehicle };
// Steam 1.0.65 terrain queries use -99 for an unavailable sample. The native
// water predicate compares terrain against the fixed world water level (100).
inline bool boardingTerrainAvailable(double height) {return _finite(height) && height!=-99.0;}
inline bool boardingGroundAllowed(double ground,double reference) {
    return boardingTerrainAvailable(ground) && boardingTerrainAvailable(reference) && ground>100.0 && fabs(ground-reference)<=15.0;
}
inline bool isComponentDebugChest(const std::string& definition,const std::string& name) {
    return definition=="879-gamedata.base" && (name=="Vehicle Components - Debug" || name=="Vehicle Components - debug");
}
inline bool parseVehicleAction(const std::string& text,VehicleClick& action) {
    if(text=="modify")action=ConfigureVehicle;else if(text=="sit")action=SitDriver;
    else if(text=="drive")action=DriveVehicle;else if(text=="trunk")action=OpenTrunk;
    else if(text=="leave")action=LeaveVehicle;else if(text=="ignition")action=IgnitionVehicle;
    else if(text=="stop")action=StopVehicle;else if(text=="pace")action=PaceVehicle;
    else if(text=="right")action=RightVehicle;else if(text=="manual")action=ManualDriveVehicle;
    else if(text=="end_manual")action=EndManualDriveVehicle;
    else if(text=="passenger")action=SitPassenger;
    else if(text=="place_driver")action=PlaceDriverVehicle;
    else if(text=="place_rear_l")action=PlaceRearLeftVehicle;
    else if(text=="place_rear_r")action=PlaceRearRightVehicle;
    else if(text=="place_pole_f")action=PlaceFrontPoleVehicle;
    else if(text=="place_pole_r")action=PlaceRearPoleVehicle;
    else if(text=="place_cage")action=PlaceCageVehicle;
    else if(text=="unload_people")action=UnloadPeopleVehicle;
    else if(text=="man_turret")action=ManTurretVehicle;else return false;return true;
}
inline VehicleClick defaultVehicleClick(VehicleClick hit,bool seat,bool steering) {
    // The rear gun is an explicit menu assignment, never an automatic seat.
    if(hit==ManTurretVehicle)hit=seat?SitDriver:ConfigureVehicle;
    return seat && steering && (hit==ConfigureVehicle || hit==SitDriver)?DriveVehicle:hit;
}
inline bool vehicleSeatBlocked(bool occupied,bool selfOccupant,bool reserved,bool selfReservation,bool interrupted) {
    return (occupied && !selfOccupant) || (reserved && !selfReservation && !selfOccupant && !interrupted);
}
// Own the entire Escape press, including repeats after the window closes.
struct VehicleEscape {
    bool held;VehicleEscape():held(false){}
    bool down(bool visible){if(!visible && !held)return false;held=true;return true;}
    bool up(){bool consumed=held;held=false;return consumed;}
};
// One physical press owns one destination or one captured vehicle. The native
// terrain hit can change every frame while the camera and car are moving.
struct VehicleRightPress {
    bool held,vehicle,orderSent,menu;double started;
    VehicleRightPress():held(false),vehicle(false),orderSent(false),menu(false),started(0){}
    void begin(bool overVehicle,double now){held=true;vehicle=overVehicle;orderSent=false;menu=false;started=now;}
    bool takeOrder(bool released=false){if(!held)return !(released && (vehicle||orderSent));if(vehicle||menu||orderSent)return false;orderSent=true;return true;}
    bool openMenu(double now){if(!held||!vehicle||menu||now-started<.35)return false;menu=true;return true;}
    void release(){held=false;}
};
struct BoardingProgress {
    double elapsed,stalled,best;
    BoardingProgress():elapsed(0),stalled(0),best(1e20){}
    // Simulation time: pausing the game cannot expire a boarding request.
    bool advance(double dt,double distance) {
        if(!_finite(dt)||dt<0||dt>5||!_finite(distance)||distance<0)return false;
        elapsed+=dt;
        if(distance<best-.25){best=distance;stalled=0;}else stalled+=dt;
        return elapsed<30 && stalled<8;
    }
};
struct PickPlane {double x,y,z,d;};
struct PickHull {const char* component;VehicleClick action;unsigned first,count;};
struct PickVector {double x,y,z;PickVector(double a=0,double b=0,double c=0):x(a),y(b),z(c){}};
// Convex clipping in vehicle-local coordinates. Preserve the transformed ray's
// length so t remains comparable across rotated/scaled vehicles.
inline bool hitVehicleHull(const PickVector& o,const PickVector& v,const PickPlane* p,unsigned count,double& t) {
    if(!_finite(o.x)||!_finite(o.y)||!_finite(o.z)||!_finite(v.x)||!_finite(v.y)||!_finite(v.z)||count<4)return false;
    if(v.x*v.x+v.y*v.y+v.z*v.z<1e-20)return false;
    double nearT=0,farT=t;
    for(unsigned i=0;i<count;++i) {
        const double distance=p[i].x*o.x+p[i].y*o.y+p[i].z*o.z+p[i].d;
        const double direction=p[i].x*v.x+p[i].y*v.y+p[i].z*v.z;
        if(fabs(direction)<1e-10){if(distance>0.0001)return false;continue;}
        const double cross=-distance/direction;
        if(direction<0){if(cross>nearT)nearT=cross;}else if(cross<farT)farT=cross;
        if(nearT>farT+0.0001)return false;
    }
    if(farT<0 || nearT>=t)return false;t=nearT;return true;
}
}
