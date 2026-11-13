#pragma once
#include "VehicleRig.h"
#include <algorithm>
namespace cars {
const double trackPi=3.14159265358979323846;
const double lightTrackLength=32.4642,lightTrackRadius=4.85;
const unsigned trackLinkCount=60,trackRollerCount=6,trackBoneCount=73;
struct TrackPose {RigPoint position;double angle,scale;TrackPose():angle(0),scale(1){}};
inline double trackPerimeter(){return 2*lightTrackLength+2*trackPi*lightTrackRadius;}
inline TrackPose trackBindLink(unsigned n) {
    TrackPose p;double t=(n+.5)*trackPerimeter()/trackLinkCount,L=lightTrackLength,r=lightTrackRadius;
    if(t<L)p.position=RigPoint(-L/2+t,r,0);
    else if(t<L+trackPi*r){double a=(t-L)/r;p.position=RigPoint(L/2+r*sin(a),r*cos(a),0);p.angle=-a;}
    else if(t<2*L+trackPi*r){p.position=RigPoint(L/2-(t-L-trackPi*r),-r,0);p.angle=-trackPi;}
    else {double a=(t-2*L-trackPi*r)/r;p.position=RigPoint(-L/2-r*sin(a),-r*cos(a),0);p.angle=-trackPi-a;}
    return p;
}
inline RigPoint trackBindRoller(unsigned n){return RigPoint(-lightTrackLength/2+n*lightTrackLength/5,n==0||n==5?0:-1.05,0);}
inline RigPoint trackArmRoot(unsigned n){RigPoint p=trackBindRoller(n);return RigPoint(p.x-.8,.3,0);}
inline RigPoint trackArmCenter(unsigned n){return (trackArmRoot(n)+trackBindRoller(n))*.5;}
// Sampled closed belt path. Shoes remain rigid; only their poses change.
struct TrackLoop {
    enum {Quarter=64,Count=Quarter*4};
    RigPoint points[Count+1];double distance[Count+1],length;
    explicit TrackLoop(const double* displacement) {
        double h[6];for(unsigned n=0;n<6;++n)h[n]=trackBindRoller(n).y+std::max(-4.5,std::min(4.5,displacement[n]));
        for(unsigned i=0;i<Count;++i) {
            const unsigned section=i/Quarter;const double u=double(i%Quarter)/Quarter,L=lightTrackLength,r=lightTrackRadius;
            RigPoint p;
            if(section==1)p=RigPoint(L/2+r*sin(trackPi*u),h[5]+r*cos(trackPi*u),0);
            else if(section==3)p=RigPoint(-L/2-r*sin(trackPi*u),h[0]-r*cos(trackPi*u),0);
            else {
                p.x=section==0?-L/2+L*u:L/2-L*u;
                const double t=(p.x+L/2)/L;
                if(section==0)p.y=h[0]*(1-t)+h[5]*t+r-.35*16*t*t*(1-t)*(1-t);
                else {double index=t*5;unsigned n=std::min(4u,static_cast<unsigned>(index));double f=index-n;f=f*f*(3-2*f);
                    p.y=(h[n]-(n==0?r:3.65))*(1-f)+(h[n+1]-(n+1==5?r:3.65))*f;}
                for(unsigned n=0;n<6;++n){double radius=n==0||n==5?r:3.65,dx=p.x-trackBindRoller(n).x;
                    if(fabs(dx)<radius){double edge=sqrt(radius*radius-dx*dx);p.y=section==0?std::max(p.y,h[n]+edge):std::min(p.y,h[n]-edge);}}
            }
            points[i]=p;
        }
        points[Count]=points[0];distance[0]=0;
        for(unsigned n=1;n<=Count;++n)distance[n]=distance[n-1]+(points[n]-points[n-1]).length();
        length=distance[Count];
    }
    TrackPose sample(double at)const {
        at=fmod(at,length);if(at<0)at+=length;unsigned lo=0,hi=Count;
        while(hi-lo>1){unsigned mid=(lo+hi)/2;if(distance[mid]<=at)lo=mid;else hi=mid;}
        double f=(at-distance[lo])/(distance[hi]-distance[lo]);TrackPose p;p.position=points[lo]*(1-f)+points[hi]*f;
        RigPoint a=(points[(lo+1)%Count]-points[(lo+Count-1)%Count]).unit();
        RigPoint b=(points[(hi+1)%Count]-points[(hi+Count-1)%Count]).unit();
        RigPoint tangent=a*(1-f)+b*f;p.angle=atan2(tangent.y,tangent.x);return p;
    }
};
inline void solveTrackPose(double travel,const double* displacement,TrackPose* poses) {
    TrackLoop loop(displacement);double phase=fmod(travel/trackPerimeter(),1.0);if(phase<0)phase+=1;
    poses[0]=TrackPose();
    for(unsigned n=0;n<60;++n){poses[1+n]=loop.sample(((n+.5)/60+phase)*loop.length);poses[1+n].angle-=trackBindLink(n).angle;}
    for(unsigned n=0;n<6;++n){
        TrackPose& wheel=poses[61+n];wheel=TrackPose();wheel.position=trackBindRoller(n);
        wheel.position.y+=std::max(-4.5,std::min(4.5,displacement[n]));wheel.angle=-fmod(travel/(n==0||n==5?4.25:3.05),2*trackPi);
        TrackPose& arm=poses[67+n];arm=TrackPose();RigPoint root=trackArmRoot(n),before=trackBindRoller(n)-root,after=wheel.position-root;
        arm.position=(root+wheel.position)*.5;arm.angle=atan2(after.y,after.x);arm.scale=after.length()/before.length();
    }
}
}
