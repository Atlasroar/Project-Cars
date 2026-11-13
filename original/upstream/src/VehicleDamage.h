#pragma once
#include "VehicleDynamics.h"
namespace cars {
inline double impactPartDamage(double speed,double mass) {
    if(!_finite(speed+mass)||speed<=25||mass<=0)return 0;
    return bounded((speed-25)*(speed-25)*mass/50000,0,1000);
}
struct RamDamage {
    double blunt,pierce,penetration;bool knockdown;
    RamDamage(double speed,double mass,bool runningGear,bool spikes):blunt(0),pierce(0),penetration(0),knockdown(false) {
        if(!_finite(speed+mass)||speed<12||mass<=0)return;
        const double energy=bounded(mass/600,.4,3)*speed*speed/180;
        blunt=bounded(energy*(runningGear?1.6:.6),0,240);
        if(spikes){pierce=bounded(energy*.9,0,180);penetration=bounded((speed-30)/220,0,.65);}
        knockdown=speed>=25;
    }
};
inline bool vehicleThreat(const RigPoint& relative,const RigPoint& velocity,double halfLength,double halfWidth,double& side) {
    RigPoint v=velocity;v.y=0;const double speed=v.length();if(speed<25||fabs(relative.y)>35)return false;
    RigPoint f=v*(1/speed),r(-f.z,0,f.x);const double ahead=relative.dot(f);side=relative.dot(r);
    return ahead>halfLength && ahead<halfLength+speed*1.2 && fabs(side)<halfWidth+6;
}
// Swept point vs convex component, expanded by a small contact radius.
inline bool sweptPartContact(const RigPoint& a,const RigPoint& b,const PickPlane* planes,unsigned count,double radius,double& time) {
    if(count<4||!validPoint(a)||!validPoint(b)||!_finite(radius)||radius<0)return false;
    double enter=0,exit=1;RigPoint d=b-a;
    for(unsigned n=0;n<count;++n) {
        const PickPlane& p=planes[n];double s=p.x*a.x+p.y*a.y+p.z*a.z+p.d-radius,v=p.x*d.x+p.y*d.y+p.z*d.z;
        if(fabs(v)<1e-9){if(s>0)return false;continue;}
        double t=-s/v;if(v<0)enter=std::max(enter,t);else exit=std::min(exit,t);
        if(enter>exit)return false;
    }
    if(enter>time)return false;time=enter;return true;
}
}
