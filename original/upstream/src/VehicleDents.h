#pragma once
#include "VehicleDynamics.h"
namespace cars {
struct FrameDents {
    RigPoint sites[4];double depth[4],damage;unsigned serial;
    FrameDents():damage(0),serial(0){for(unsigned n=0;n<4;++n)depth[n]=0;}
    void repair(double remaining) {
        remaining=std::max(0.0,remaining);if(remaining<damage){double ratio=damage>0?remaining/damage:0;for(unsigned n=0;n<4;++n)depth[n]*=ratio;++serial;}damage=remaining;
    }
    void hit(const RigPoint& local,double severity,double totalDamage) {
        if(!validPoint(local)||!_finite(severity+totalDamage)||severity<=45)return;
        repair(totalDamage);unsigned slot=(local.x<0?2:0)+(local.z<0?1:0);
        double added=bounded((severity-45)*.022,0,1.4);
        if(added>=depth[slot]*.25)sites[slot]=local;
        depth[slot]=std::min(2.5,depth[slot]+added);++serial;
    }
    RigPoint deform(const RigPoint& p)const {
        RigPoint offset;
        for(unsigned n=0;n<4;++n){RigPoint delta=p-sites[n];double weight=std::max(0.0,1-delta.dot(delta)/144);RigPoint inward(-sites[n].x,-sites[n].y*.15,-sites[n].z);offset=offset+inward.unit()*(depth[n]*weight*weight);}
        double length=offset.length();return p+offset*(length>2.5?2.5/length:1);
    }
    std::string save()const{std::ostringstream o;o.precision(17);o<<"PCDT1 "<<damage;for(unsigned n=0;n<4;++n)o<<' '<<sites[n].x<<' '<<sites[n].y<<' '<<sites[n].z<<' '<<depth[n];return o.str();}
    bool load(const std::string& text){if(text.size()>2048)return false;FrameDents d;std::istringstream in(text);std::string tag,extra;in>>tag>>d.damage;if(tag!="PCDT1"||!_finite(d.damage)||d.damage<0)return false;for(unsigned n=0;n<4;++n){in>>d.sites[n].x>>d.sites[n].y>>d.sites[n].z>>d.depth[n];if(!validPoint(d.sites[n])||d.sites[n].length()>1000||!_finite(d.depth[n])||d.depth[n]<0||d.depth[n]>2.5)return false;}if(!in||in>>extra)return false;d.serial=serial+1;*this=d;return true;}
};
}
