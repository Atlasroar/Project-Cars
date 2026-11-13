#pragma once
#include "VehicleDynamics.h"
namespace cars {
inline double vehicleFov(double degrees) {return _finite(degrees)?floor(bounded(degrees,45,110)+.5):75;}
inline BodyRotation cameraAxis(double angle,const RigPoint& axis) {return BodyRotation(cos(angle*.5),axis.x*sin(angle*.5),axis.y*sin(angle*.5),axis.z*sin(angle*.5));}
struct VehicleCameraPose {RigPoint eye,focus;BodyRotation rotation;};
// A loading pause can change the renderer's relative origin. Keep values in
// world space so resuming rendering does not jump to an obsolete local origin.
struct VehicleCameraHold {
    bool valid;VehicleCameraPose world;
    VehicleCameraHold():valid(false){}
    void remember(const VehicleCameraPose& local,const RigPoint& origin) {
        world=local;world.eye=world.eye+origin;world.focus=world.focus+origin;valid=true;
    }
    VehicleCameraPose relativeTo(const RigPoint& origin)const {
        VehicleCameraPose local=world;local.eye=local.eye-origin;local.focus=local.focus-origin;return local;
    }
};
struct VehicleCameraState {
    enum Mode {Standard,Follow,FirstPerson};
    Mode mode;bool mouseLook,followReady;double yaw,pitch,distance,followYaw;
    VehicleCameraState():mode(Standard),mouseLook(true),followReady(false),yaw(0),pitch(0),distance(80),followYaw(0){}
    bool firstPerson()const{return mode==FirstPerson;}
    void toggleView(){mode=static_cast<Mode>((int(mode)+1)%3);yaw=pitch=0;mouseLook=true;followReady=false;}
    void toggleMouse(){if(firstPerson())mouseLook=!mouseLook;}
    void smoothFollow(const BodyRotation& body,double dt) {
        const RigPoint f=body.rotate(RigPoint(1,0,0));const double target=atan2(-f.z,f.x);
        if(!followReady){followYaw=target;followReady=true;return;}
        if(!_finite(dt)||dt<=0)return;
        double delta=target-followYaw;const double pi=3.141592653589793;
        while(delta>pi)delta-=2*pi;while(delta<-pi)delta+=2*pi;
        followYaw+=delta*(1-exp(-4*std::min(dt,.25)));
        while(followYaw>pi)followYaw-=2*pi;while(followYaw<-pi)followYaw+=2*pi;
    }
    void look(double dx,double dy) {
        if(!firstPerson()||!mouseLook||!_finite(dx+dy))return;
        yaw=bounded(yaw-dx*.0025,-2.7,2.7);pitch=bounded(pitch-dy*.0025,-1.2,1.2);
    }
    void zoom(double wheel){if(mode==Follow && _finite(wheel))distance=bounded(distance-wheel*.08,40,200);}
    VehicleCameraPose pose(const RigPoint& anchor,const BodyRotation& body,double scale,bool extraSmall=false)const {
        VehicleCameraPose p;
        // Follow eases around chassis yaw and keeps a level horizon. First
        // person stays attached to chassis pitch/roll with its own look offset.
        const BodyRotation base=firstPerson()?body:cameraAxis(followReady?followYaw:atan2(-body.rotate(RigPoint(1,0,0)).z,body.rotate(RigPoint(1,0,0)).x),RigPoint(0,1,0));
        p.rotation=base*cameraAxis(-1.5707963267948966+(firstPerson()?yaw:0),RigPoint(0,1,0))*cameraAxis(firstPerson()?pitch:-.32,RigPoint(1,0,0));
        if(firstPerson()) {
            RigPoint eye(-3.5,22,-.099404);
            if(extraSmall)eye=eye+xsmallPelvis-lightSeatPelvis.position;
            p.eye=anchor+body.rotate(eye*scale);
            p.focus=p.eye+p.rotation.rotate(RigPoint(0,0,-20*scale));
        } else {
            p.focus=anchor+body.rotate((extraSmall?RigPoint(-1,10,0):RigPoint(-2,14,0))*scale);
            p.eye=p.focus-p.rotation.rotate(RigPoint(0,0,-distance*scale));
        }
        return p;
    }
};
// Loading panels pause this deadline. A missing physical representation must
// not skip the deadline or keep a ghost driver/audio source alive forever.
inline bool vehicleStreamExpired(double readySeconds){return !_finite(readySeconds)||readySeconds>=15;}
inline bool vehicleStreamRetry(bool zoneLoaded,unsigned attempts,double sinceAttempt,double elapsed) {
    return zoneLoaded && attempts<3 && sinceAttempt>=1 && !vehicleStreamExpired(elapsed);
}
inline bool vehicleNeedsPhysicalRestore(bool occupied,bool cameraActive,bool playerActive) {
    return occupied || cameraActive || playerActive;
}
}
