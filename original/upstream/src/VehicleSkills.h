#pragma once
#include <cmath>
#include <float.h>
namespace cars {
// Allow small horizontal settling; terrain height correction does not cancel
// work. Replaced orders, combat and loss of range are checked by the runtime.
inline bool fittingWorkerInPlace(double dx,double dz){return _finite(dx) && _finite(dz) && dx*dx+dz*dz<=4.0;}
// Native command queue membership is deliberately not a work-lifetime signal:
// Kenshi can consume a command while our timed activity is still in progress.
inline bool fittingWorkAllowed(bool ready,bool replaced,bool partAvailable,double dx,double dz) {
    return ready && !replaced && partAvailable && fittingWorkerInPlace(dx,dz);
}
inline double validVehicleXp(double xp){return !_finite(xp)||xp<0?0:xp>980100?980100:xp;}
inline double vehicleSkillLevel(double xp){return 1+sqrt(validVehicleXp(xp)/100.0);}
inline double mechanicsRate(double xp){return 1+(vehicleSkillLevel(xp)-1)*0.04;}
struct VehicleSkills {double mechanicsXp,handlingXp;VehicleSkills():mechanicsXp(0),handlingXp(0) {}};
struct FittingWork {
    double done,required;
    FittingWork(double base=30):done(0),required(_finite(base) && base>0?base:30) {}
    bool advance(double seconds,double xp,bool eligible) {
        if(done>=required || !eligible || !_finite(seconds) || seconds<=0)return false;
        // Discard a long discontinuity rather than finishing during a load stall.
        if(seconds>5)return false;
        done+=seconds*mechanicsRate(xp);if(done>required)done=required;
        return done>=required;
    }
    double fraction() const{return required>0?done/required:0;}
};
}
