#pragma once
#include "VehicleDynamics.h"
#include "TreadBlood.h"
#include <deque>
namespace cars {
const double treadLifetime=90.0;
const unsigned maxTreadSegments=8192;
struct TreadSegment {
    RigPoint corners[4];double born,v0,v1,strength,blood;bool belt;
};
class TreadTrail {
public:
    std::deque<TreadSegment> segments;double clock;
    TreadTrail():clock(0){}
    void clear(){segments.clear();clock=0;}
    void advance(double dt) {
        if(!_finite(dt)||dt<=0||dt>.5)return;
        clock+=dt;
        while(!segments.empty() && clock-segments.front().born>=treadLifetime)segments.pop_front();
    }
    void add(const TreadSegment& s){if(segments.size()==maxTreadSegments)segments.pop_front();segments.push_back(s);}
    // Fade the oldest end of a saturated trail before recycling its storage.
    double pressureFade(size_t index)const{
        const double pressure=bounded((double(segments.size())-(maxTreadSegments-1024))/1024,0,1);
        return 1-pressure*(1-bounded(double(index)/512,0,1));
    }
};
struct TreadEmitter {
    TreadBloodCoating coating;
    bool connected,belt;RigPoint position,normal,right,direction;double width,v,lastSeen;
    TreadEmitter():connected(false),belt(false),width(0),v(0),lastSeen(0){}
    void reset(){connected=false;v=0;coating.clear();}
    void sample(TreadTrail& trail,const WheelGroundContact& contact,const RigPoint& forward,double w,bool tracked,double strength,const TreadBloodGround* bloodGround=0) {
        const double now=trail.clock;
        if(!validPoint(forward)||!_finite(w+strength)||w<=0){reset();return;}
        if(width>0 && (belt!=tracked || fabs(width-w)>.001))coating.clear();
        coating.update(now,contact,w,connected && now-lastSeen<=.5,bloodGround);
        if(!contact.touching || !validPoint(contact.point)||!validPoint(contact.normal)||contact.normal.y<.25) {connected=false;v=0;return;}
        const RigPoint n=contact.normal.unit(),r=cross(forward,n).unit();
        if(r.length()<.5){reset();return;}
        const RigPoint p=contact.point,travel=p-position;const double distance=travel.length(),spacing=w*(tracked?.4:.75);
        const bool reconnect=!connected || belt!=tracked || fabs(width-w)>.001 || now-lastSeen>.5 || now<lastSeen || distance>w*8 || normal.dot(n)<.65 || right.dot(r)<.5;
        lastSeen=now;
        if(reconnect){connected=true;belt=tracked;position=p;normal=n;right=r;width=w;direction=RigPoint();v=0;return;}
        if(distance<spacing)return;
        const RigPoint path=travel.unit();
        if(direction.length()>.5 && direction.dot(path)<-.25){position=p;normal=n;right=r;direction=path;v=0;return;}
        // Long discontinuities reconnect above. Subdivide normal frame travel
        // to keep small ground patches and avoid one long quad across a turn.
        const unsigned pieces=std::min(12u,static_cast<unsigned>(ceil(distance/std::max(spacing,1.0))));
        const double pitch=w*(tracked?.65:1.1);
        for(unsigned k=0;k<pieces;++k) {
            const double a=double(k)/pieces,b=double(k+1)/pieces;
            const RigPoint pa=position+travel*a,pb=position+travel*b;
            const RigPoint na=(normal*(1-a)+n*a).unit(),nb=(normal*(1-b)+n*b).unit();
            const RigPoint ra=(right*(1-a)+r*a).unit()*(w*.5),rb=(right*(1-b)+r*b).unit()*(w*.5);
            TreadSegment s;s.corners[0]=pa-ra+na*.06;s.corners[1]=pa+ra+na*.06;s.corners[2]=pb+rb+nb*.06;s.corners[3]=pb-rb+nb*.06;
            s.born=now;s.v0=v+distance*a/pitch;s.v1=v+distance*b/pitch;s.belt=tracked;s.strength=bounded(strength,0,1);s.blood=coating.amount;trail.add(s);
        }
        v+=distance/pitch;if(v>4096)v-=floor(v); // integer UV wraps preserve tread phase
        position=p;normal=n;right=r;direction=path;
    }
};
}
