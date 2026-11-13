#pragma once
#include "VehicleDynamics.h"
#include <deque>
namespace cars {
// World units are ten per metre. Fresh coating wears off over 35 metres or
// 15 unpaused seconds, whichever happens first. This is visual state only.
const double treadBloodDistance=350, treadBloodSeconds=15;
const unsigned maxTreadBloodPools=2048;
struct TreadBloodPool {
    double x,z,radius,born;
    TreadBloodPool(double px,double pz,double r,double time):x(px),z(pz),radius(r),born(time){}
};
class TreadBloodGround {
    std::deque<TreadBloodPool> pools;
public:
    void clear(){pools.clear();}
    size_t size()const{return pools.size();}
    void advance(double now){while(!pools.empty() && now-pools.front().born>=600)pools.pop_front();}
    void add(double x,double z,double radius,double now){
        if(!_finite(x)||!_finite(z)||!_finite(radius)||!_finite(now)||radius<=0||radius>200)return;
        // Growing native pools emit updates at the same centre; replace their
        // old footprint instead of filling the cache with duplicate circles.
        for(size_t n=pools.size(),end=n>32?n-32:0;n>end;--n)
            if(fabs(pools[n-1].x-x)<.1 && fabs(pools[n-1].z-z)<.1){pools.erase(pools.begin()+n-1);break;}
        if(pools.size()==maxTreadBloodPools)pools.pop_front();
        pools.push_back(TreadBloodPool(x,z,radius,now));
    }
    bool touches(const RigPoint& a,const RigPoint& b,double halfWidth)const{
        if(!validPoint(a)||!validPoint(b)||!_finite(halfWidth)||halfWidth<0)return false;
        const double dx=b.x-a.x,dz=b.z-a.z,length=dx*dx+dz*dz;
        for(size_t n=0;n<pools.size();++n){
            const TreadBloodPool& p=pools[n];
            const double t=length>1e-8?bounded(((p.x-a.x)*dx+(p.z-a.z)*dz)/length,0,1):0;
            const double x=a.x+t*dx-p.x,z=a.z+t*dz-p.z,r=p.radius+halfWidth;
            if(x*x+z*z<=r*r)return true;
        }
        return false;
    }
};
struct TreadBloodCoating {
    RigPoint last;double amount,stamp;bool located;
    TreadBloodCoating():amount(0),stamp(0),located(false){}
    void clear(){amount=0;located=false;stamp=0;}
    void update(double now,const WheelGroundContact& c,double width,bool continuous,const TreadBloodGround* ground){
        if(now<stamp){clear();}
        amount=std::max(0.0,amount-std::max(0.0,now-stamp)/treadBloodSeconds);stamp=now;
        if(!c.touching || !validPoint(c.point) || !validPoint(c.normal) || c.normal.y<.25){located=false;return;}
        // No pickup along teleports, airborne gaps or discontinuous strips.
        const bool sweep=located && continuous && (c.point-last).length()<=width*8;
        if(sweep)amount=std::max(0.0,amount-(c.point-last).length()/treadBloodDistance);
        if(ground && ground->touches(sweep?last:c.point,c.point,width*.5))amount=1;
        last=c.point;located=true;
    }
};
}
