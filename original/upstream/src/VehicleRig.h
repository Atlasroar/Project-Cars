#pragma once
#include <cmath>
#include <float.h>
namespace cars {
struct RigPoint {
    double x,y,z;RigPoint(double X=0,double Y=0,double Z=0):x(X),y(Y),z(Z){}
    RigPoint operator+(const RigPoint& b)const{return RigPoint(x+b.x,y+b.y,z+b.z);}
    RigPoint operator-(const RigPoint& b)const{return RigPoint(x-b.x,y-b.y,z-b.z);}
    RigPoint operator*(double s)const{return RigPoint(x*s,y*s,z*s);}
    double dot(const RigPoint& b)const{return x*b.x+y*b.y+z*b.z;}
    double length()const{return sqrt(dot(*this));}
    RigPoint unit()const{double l=length();return l>1e-8?*this*(1/l):RigPoint();}
};
struct RigMarker {const char* name;RigPoint position;};
static const RigMarker lightDoors[2]={
    {"Entrance_Driver_Left",RigPoint(-8.467561,0,-17)},
    {"Entrance_Driver_Right",RigPoint(-8.467561,0,17)}
};
static const RigMarker lightSeat={"Seat_Driver",RigPoint(-8.467561,5.25,-.099404)};
// Cushion top is y=8.45185 in the exported frame. Place the pelvis just above
// it, independently of character height and the native chair clip's root offset.
static const RigMarker lightSeatPelvis={"Seat_Driver_Pelvis",RigPoint(-8.467561,9.5,-.099404)};
static const RigMarker xsmallDoors[2]={
    {"Entrance_Driver_Left",RigPoint(-4,0,-12)},
    {"Entrance_Driver_Right",RigPoint(-4,0,12)}
};
static const RigPoint xsmallSeat(-4,2.477844,0);
static const RigPoint xsmallPelvis(-4,6.727844,0);
static const RigPoint xsmallSteering(4.25,9.5,0);
static const RigMarker lightWheel={"Steering_Pivot",RigPoint(-1.25086,11.1988,-.03299)};
// Column points toward the driver: Blender (-.28,0,.45), converted to Y-up.
inline RigPoint steeringAxis(){return RigPoint(-.28,.45,0).unit();}
inline RigPoint rotateRigPoint(const RigPoint& p,const RigPoint& axis,double angle) {
    RigPoint cross(axis.y*p.z-axis.z*p.y,axis.z*p.x-axis.x*p.z,axis.x*p.y-axis.y*p.x);
    return p*cos(angle)+cross*sin(angle)+axis*(axis.dot(p)*(1-cos(angle)));
}
inline RigPoint steeringGrip(bool left,double radians,bool extraSmall=false) {
    const double radius=extraSmall?2.37*.82:2.37;
    const RigPoint& pivot=extraSmall?xsmallSteering:lightWheel.position;
    return pivot+rotateRigPoint(RigPoint(0,0,left?-radius:radius),steeringAxis(),radians);
}
// Lean from the lower spine while leaving the pelvis on the cushion. Choose
// the least lean that reaches both grips, capped at a natural 35 degrees.
inline double driverTorsoLean(const RigPoint& pivot,const RigPoint& leftShoulder,const RigPoint& rightShoulder,double leftReach,double rightReach,const RigPoint& leftGrip,const RigPoint& rightGrip,RigPoint& axis) {
    RigPoint up=(leftShoulder+rightShoulder)*.5-pivot,toward=(leftGrip+rightGrip)*.5-pivot;
    axis=RigPoint(up.y*toward.z-up.z*toward.y,up.z*toward.x-up.x*toward.z,up.x*toward.y-up.y*toward.x).unit();
    if(axis.length()<.5||leftReach<1e-5||rightReach<1e-5)return 0;
    double best=0,bestScore=1e30;
    for(unsigned step=0;step<=14;++step) {
        double angle=step*(3.141592653589793/72);
        RigPoint l=pivot+rotateRigPoint(leftShoulder-pivot,axis,angle),r=pivot+rotateRigPoint(rightShoulder-pivot,axis,angle);
        double ls=(leftGrip-l).length()/leftReach,rs=(rightGrip-r).length()/rightReach,score=ls>rs?ls:rs;
        if(score<bestScore){bestScore=score;best=angle;}
        if(score<=.985)return angle;
    }
    return best;
}
// Two-bone IK. Clamp unreachable targets and keep the elbow on its current side.
inline bool solveArm(const RigPoint& shoulder,const RigPoint& elbow,const RigPoint& hand,const RigPoint& target,RigPoint& solvedElbow,RigPoint& solvedHand) {
    double a=(elbow-shoulder).length(),b=(hand-elbow).length(),d=(target-shoulder).length();
    if(!_finite(a+b+d)||a<1e-5||b<1e-5||d<1e-5)return false;
    RigPoint axis=(target-shoulder).unit();
    double low=fabs(a-b)+1e-4,high=a+b-1e-4;
    if(low>=high)return false;if(d<low)d=low;if(d>high)d=high;
    RigPoint pole=elbow-shoulder;pole=pole-axis*pole.dot(axis);
    if(pole.length()<1e-5){pole=RigPoint(0,-1,0);pole=pole-axis*pole.dot(axis);}
    if(pole.length()<1e-5){pole=RigPoint(0,0,1);pole=pole-axis*pole.dot(axis);}
    double along=(a*a-b*b+d*d)/(2*d),h2=a*a-along*along;
    solvedElbow=shoulder+axis*along+pole.unit()*sqrt(h2>0?h2:0);
    solvedHand=shoulder+axis*d;return true;
}
}
