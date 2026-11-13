#pragma once
#include "VehicleDynamics.h"
namespace cars {
// A consumed physical press stays ours through release, even if H/Escape ends
// manual mode first. A key held before entry cannot acquire vehicle control.
struct VehicleKeyPress {
    bool held,owned,active;
    VehicleKeyPress():held(false),owned(false),active(false){}
    bool down(bool eligible,bool& fresh) {
        fresh=!held && eligible;
        if(!held){held=true;owned=eligible;active=eligible;}
        return owned;
    }
    bool up(){bool result=owned;held=owned=active=false;return result;}
    void suspend(){active=false;}
};
struct ManualDrivingKeys {
    bool forward,back,left,right,enabled;
    ManualDrivingKeys():forward(false),back(false),left(false),right(false),enabled(false){}
};
struct ManualDriver {
    bool reversing;double steeringCommand;
    ManualDriver():reversing(false),steeringCommand(0){}
    DrivingInput control(const ChassisBody& body,const ChassisSpec& spec,const ManualDrivingKeys& keys,double handling,double dt) {
        DrivingInput out;out.brake=1;out.reverse=reversing;
        if(!keys.enabled || !body.ignition || body.overturned()) {steeringCommand=0;return out;}
        const double skill=bounded(handling/100.,0,1);
        // Manual input shares the route driver's skill-dependent steering slew.
        double target=(keys.left?1.:0.)-(keys.right?1.:0.);
        if(!keys.left && !keys.right)target=sin(body.clock*(1.1+.5*skill))*.065*(1-skill)*bounded(body.speed()/spec.maxSpeed,0,1);
        steeringCommand+=bounded(target-steeringCommand,-dt*spec.steeringResponse*(.8+2.2*skill),dt*spec.steeringResponse*(.8+2.2*skill));out.steer=steeringCommand;
        if(keys.forward && keys.back)return out;
        if(!keys.forward && !keys.back){out.brake=0;if(spec.trackMask==3 && keys.left!=keys.right){out.pivot=true;out.throttle=fabs(out.steer)*.6;}return out;} // belts counter-rotate with A/D; release pedals to coast
        const double longitudinal=body.velocity.dot(body.forward());
        const bool wantReverse=keys.back;
        // S brakes forward motion before reverse; W does the same in reverse.
        if((wantReverse && longitudinal>2) || (!wantReverse && longitudinal<-2))return out;
        reversing=wantReverse;out.reverse=reversing;out.brake=0;
        const double limit=reversing?25.:spec.maxSpeed;
        const double progress=wantReverse?-longitudinal:longitudinal;
        const double speedError=limit-progress;
        // Keep supplying the force needed to hold the speed. Releasing the
        // pedal still coasts, and a direction change still applies the brakes.
        const double grade=body.forward().y*(wantReverse?-1:1);
        const double drag=spec.rollingDrag*limit+spec.mass*.0005*limit*limit;
        const double holdingForce=drag+spec.mass*spec.gravity*grade;
        out.throttle=speedError<0?0:bounded(holdingForce/spec.engineForce+speedError/8.,0,1);
        return out;
    }
};
}
