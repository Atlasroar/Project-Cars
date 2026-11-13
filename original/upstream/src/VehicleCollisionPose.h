#pragma once
#include "VehicleDynamics.h"
namespace cars {
inline double rotationNorm(const BodyRotation& q){return q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;}
inline bool unitCollisionRotation(BodyRotation& q) {
    const double norm=rotationNorm(q);
    if(!_finite(norm) || norm<.5 || norm>1.5)return false;
    q.normalize();return true;
}
// PhysX stores an orientation matrix internally. Do not feed small quaternion
// length errors back through its matrix/quaternion round trip each frame.
inline bool transformCollisionPose(const RigPoint& oldOrigin,const BodyRotation& oldRotation,
    const RigPoint& origin,const BodyRotation& rotation,const RigPoint& actorPosition,const BodyRotation& actorRotation,
    double scale,RigPoint& position,BodyRotation& orientation) {
    if(!validPoint(oldOrigin)||!validPoint(origin)||!validPoint(actorPosition)||!_finite(scale)||scale<=.0001)return false;
    if((actorPosition-oldOrigin).length()>200*scale)return false;
    BodyRotation old=oldRotation,next=rotation,actor=actorRotation;
    if(!unitCollisionRotation(old)||!unitCollisionRotation(next)||!unitCollisionRotation(actor))return false;
    BodyRotation delta=next*old.inverse();delta.normalize();
    RigPoint p=origin+delta.rotate(actorPosition-oldOrigin);BodyRotation q=delta*actor;q.normalize();
    if(!validPoint(p)||!_finite(rotationNorm(q)))return false;
    position=p;orientation=q;return true;
}
enum DriveFault {NoDriveFault,ColliderDriveFault,QueryDriveFault,StateDriveFault,CallbackDriveFault};
inline bool driveFaultCanRetry(DriveFault fault){return fault==ColliderDriveFault || fault==QueryDriveFault || fault==CallbackDriveFault;}
inline bool driveRetryValidated(DriveFault fault,bool body,bool collider,bool queries){return driveFaultCanRetry(fault)&&body&&collider&&queries;}
// A lost collider must not permanently anchor an engine-off car to a cliff.
// Retry only recoverable synchronization faults, with backoff and a hard cap.
inline bool automaticPhysicsRetry(DriveFault fault,unsigned attempts,double seconds) {
    return driveFaultCanRetry(fault) && attempts<3 && _finite(seconds) && seconds>=.5*(attempts+1)*(attempts+1);
}
inline bool retryStartsEngine(bool requested,bool driver,bool ready,double fuel,bool overturned) {
    return requested && driver && ready && _finite(fuel) && fuel>0 && !overturned;
}

}
