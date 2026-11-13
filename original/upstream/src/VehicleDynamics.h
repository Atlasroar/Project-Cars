#pragma once
#include "VehicleRig.h"
#include <vector>
#include <map>
#include <queue>
#include <sstream>
#include <algorithm>
namespace cars {
inline double bounded(double x,double lo,double hi){return x<lo?lo:x>hi?hi:x;}
inline RigPoint cross(const RigPoint& a,const RigPoint& b){return RigPoint(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
inline bool validPoint(const RigPoint& v){return _finite(v.x)&&_finite(v.y)&&_finite(v.z);}
struct BodyRotation {
    double w,x,y,z;BodyRotation(double W=1,double X=0,double Y=0,double Z=0):w(W),x(X),y(Y),z(Z){}
    BodyRotation operator*(const BodyRotation& b)const{return BodyRotation(w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,w*b.y+y*b.w+z*b.x-x*b.z,w*b.z+z*b.w+x*b.y-y*b.x);}
    BodyRotation inverse()const{return BodyRotation(w,-x,-y,-z);}
    void normalize(){double n=sqrt(w*w+x*x+y*y+z*z);if(n>1e-8&&_finite(n)){w/=n;x/=n;y/=n;z/=n;}else *this=BodyRotation();}
    RigPoint rotate(const RigPoint& p)const {RigPoint v(x,y,z);return p+cross(v,p)*(2*w)+cross(v,cross(v,p))*2;}
    void advance(const RigPoint& angular,double dt){BodyRotation d=BodyRotation(0,angular.x,angular.y,angular.z)*(*this);w+=d.w*dt*.5;x+=d.x*dt*.5;y+=d.y*dt*.5;z+=d.z*dt*.5;normalize();}
};
struct SurfaceHit {RigPoint point,normal;double distance;bool treadSurface;SurfaceHit():normal(0,1,0),distance(0),treadSurface(true){}};
struct WheelGroundContact {RigPoint point,normal;bool touching;WheelGroundContact():normal(0,1,0),touching(false){}};
struct VehicleEnvironment {
    virtual ~VehicleEnvironment(){}
    virtual bool ray(const RigPoint& from,const RigPoint& to,SurfaceHit& out)=0;
    virtual bool sweep(const RigPoint& from,const RigPoint& to,double radius,SurfaceHit& out){return ray(from,to,out);}
    virtual void collisionContact(const SurfaceHit&){}
    virtual bool driveable(const RigPoint& point,double radius,double& ground)=0;
    virtual bool startingGround(const RigPoint& point,double radius,double& ground){return driveable(point,radius,ground);}
    virtual bool passage(const RigPoint& a,const RigPoint& b,double radius) {
        unsigned count=static_cast<unsigned>(ceil((b-a).length()/std::max(2.0,radius*.5)));
        if(count>128)return false;
        RigPoint horizontal=b-a;horizontal.y=0;
        const double step=count?horizontal.length()/count:0;
        double prior=a.y;
        for(unsigned n=1;n<=count;++n){double h;if(!driveable(a+(b-a)*(double(n)/count),radius,h)||fabs(h-prior)>step*.85+1.0)return false;prior=h;}
        return true;
    }
};
// VC100/LTCG emits aligned SIMD stores when returning this plain value.
// Keep its return temporaries aligned as well as its named stack instances.
struct __declspec(align(16)) ChassisSpec {
    unsigned wheelMask,trackMask;
    double steeringResponse,rollingDrag;
    bool motorcycle;
    double mass,halfLength,halfWidth,halfHeight,wheelBase,track,radius,travel,mountHeight,spring,damping,grip,engineForce,brakeForce,maxSpeed,gravity,cageHeight;
    ChassisSpec():wheelMask(15),trackMask(0),steeringResponse(1),rollingDrag(140),motorcycle(false),mass(500),halfLength(22),halfWidth(11),halfHeight(8),wheelBase(40),track(24),radius(5.5),travel(4),mountHeight(-2.4),spring(10000),damping(1600),grip(.9),engineForce(18000),brakeForce(35000),maxSpeed(180),gravity(98.1),cageHeight(0){}
};
inline bool runningGearReady(unsigned wheels,unsigned tracks) {
    return (wheels==15 && tracks==0) || (wheels==0 && tracks==3);
}
inline ChassisSpec lightChassisSpec(double scale,double mass) {
    ChassisSpec s;s.mass=mass;s.rollingDrag=110;s.maxSpeed=205;
    s.wheelBase=32.464229;s.track=20.847029;s.radius=5.084398;s.mountHeight=-2.094682;
    // The old box hung below the frame's belly and met small rises before the
    // tires. Give the off-road suspension usable droop and underbody clearance.
    s.halfHeight=5.5;s.travel=5.5;s.engineForce=45000;s.cageHeight=19*scale;
    // Loaded cars need enough brake torque to use the tires' available grip
    // on a hill. Friction still limits every wheel, including while parked.
    s.brakeForce=70000;
    s.halfLength*=scale;s.halfWidth*=scale;s.halfHeight*=scale;s.wheelBase*=scale;s.track*=scale;s.radius*=scale;s.travel*=scale;s.mountHeight*=scale;
    s.spring=s.mass*s.gravity/(4*s.travel*.35);s.damping=2*sqrt(s.spring*s.mass*.25)*.65;
    return s;
}
inline ChassisSpec xsmallChassisSpec(double scale,double mass,unsigned wheels) {
    ChassisSpec s;
    s.mass=mass;
    s.halfLength=10.7*scale;
    s.halfWidth=5.35*scale;
    s.wheelBase=15.2*scale;
    s.track=8.6*scale;
    s.radius=(5.084398*.49)*scale;
    s.travel=2.4*scale;
    s.mountHeight=-1.62*scale;
    s.wheelMask=wheels==15?15:0;
    s.halfHeight=(s.wheelMask?3.4:4.0)*scale;
    s.engineForce=20500;
    s.rollingDrag=100;
    s.brakeForce=26000;
    s.maxSpeed=145;
    s.steeringResponse=2.1;
    s.cageHeight=4.6*scale;
    s.spring=s.mass*s.gravity/(4*s.travel*.35);
    s.damping=2*sqrt(s.spring*s.mass*.25)*.65;
    return s;
}
inline ChassisSpec motorcycleChassisSpec(double scale,double mass,unsigned wheels) {
    ChassisSpec s=xsmallChassisSpec(scale,mass,0);
    s.motorcycle=true;
    s.wheelMask=wheels==5?5:0;
    s.halfLength=11.8*scale;
    s.halfWidth=3.25*scale;
    s.halfHeight=3.6*scale;
    s.wheelBase=21.1*scale;
    s.track=0;
    s.radius=3.4*scale;
    s.travel=2.3*scale;
    s.mountHeight=-1.585*scale;
    s.engineForce=21000;
    s.brakeForce=18500;
    s.maxSpeed=155;
    s.spring=mass*s.gravity/(2*s.travel*.35);
    s.damping=2*sqrt(s.spring*mass*.5)*.7;
    return s;
}
inline void configureTracks(ChassisSpec& s,unsigned sides) {
    s.trackMask=sides&3;s.wheelMask=0;s.maxSpeed*=.55;s.engineForce*=1.5;
    s.grip=1.15;s.travel*=1.15;s.rollingDrag=140;
    // Twelve distributed rollers share the same total supported mass.
    s.spring=s.mass*s.gravity/(12*s.travel*.35);s.damping=2*sqrt(s.spring*s.mass/12)*.65;
}
inline void configureAssemblyGear(ChassisSpec& s,unsigned wheels,unsigned tracks,double scale) {
    s.wheelMask=wheels;s.trackMask=0;
    if(runningGearReady(wheels,tracks)){if(tracks==3)configureTracks(s,tracks);return;}
    s.wheelMask=0;s.halfHeight=10*scale;
}
inline double vehicleElapsed(double hours,double secondsPerHour) {
    const double dt=hours*secondsPerHour;
    return _finite(dt) && secondsPerHour>0 && dt>0 && dt<=.5?dt:0;
}
struct DrivingInput {double throttle,brake,steer;bool handbrake,reverse,pivot;DrivingInput():throttle(0),brake(0),steer(0),handbrake(false),reverse(false),pivot(false){}};
struct ChassisBody {
    // Transient physical contacts, never serialized or used to apply forces.
    WheelGroundContact ground[4];
    RigPoint position,velocity,angular,impactPoint;BodyRotation orientation;
    double fuel,steering,wheelSpin,clock,damage,compression[4],tireSlip,impact;bool ignition;unsigned contacts;
    double trackTravel[2],trackSpeed[2],trackCompression[2][6];
    ChassisBody():fuel(0),steering(0),wheelSpin(0),clock(0),damage(0),tireSlip(0),impact(0),ignition(false),contacts(0){for(unsigned n=0;n<4;++n)compression[n]=0;for(unsigned s=0;s<2;++s){trackTravel[s]=trackSpeed[s]=0;for(unsigned n=0;n<6;++n)trackCompression[s][n]=0;}}
    RigPoint forward()const{return orientation.rotate(RigPoint(1,0,0));}
    RigPoint up()const{return orientation.rotate(RigPoint(0,1,0));}
    double speed()const{return velocity.length();}
    bool overturned()const{return up().y<.25;}
    bool valid()const{double norm=orientation.w*orientation.w+orientation.x*orientation.x+orientation.y*orientation.y+orientation.z*orientation.z;return validPoint(position)&&validPoint(velocity)&&validPoint(angular)&&_finite(norm+fuel+damage)&&norm>.25&&norm<4&&fuel>=0&&fuel<=10000&&damage>=0;}
    RigPoint inverseInertia(const RigPoint& v,const ChassisSpec& s)const {
        RigPoint local=orientation.inverse().rotate(v);
        local.x/=s.mass*(s.halfWidth*s.halfWidth+s.halfHeight*s.halfHeight)/3;
        local.y/=s.mass*(s.halfWidth*s.halfWidth+s.halfLength*s.halfLength)/3;
        local.z/=s.mass*(s.halfLength*s.halfLength+s.halfHeight*s.halfHeight)/3;
        return orientation.rotate(local);
    }
    void forceAt(const RigPoint& force,const RigPoint& offset,double dt,const ChassisSpec& s){velocity=velocity+force*(dt/s.mass);angular=angular+inverseInertia(cross(offset,force),s)*dt;}
    void impulseAt(const RigPoint& impulse,const RigPoint& offset,const ChassisSpec& s){velocity=velocity+impulse*(1/s.mass);angular=angular+inverseInertia(cross(offset,impulse),s);}
    bool canRest(const ChassisSpec& s,VehicleEnvironment& world) {
        // Static friction/sleep for parked vehicles, including a bare frame.
        // Require support around the footprint, not just a centre ray: a car
        // hanging over a ledge must still fall. Check again on every step so
        // removing wheels, ground, or changing the load cannot leave it pinned.
        if(up().y<.7 || speed()>2 || angular.length()>.18)return false;
        unsigned supported=0;const double tolerance=std::min(s.halfWidth,s.halfLength)*.06;
        for(unsigned n=0;n<4;++n) {
            if(s.motorcycle && s.wheelMask && !(s.wheelMask&(1u<<n)))continue;
            bool gear=s.trackMask?(s.trackMask&(1u<<(n%2)))!=0:(s.wheelMask&(1u<<n))!=0;
            RigPoint local((n<2?1:-1)*(gear?s.wheelBase*.5:s.halfLength*.85),
                gear?s.mountHeight-s.radius-s.travel+compression[n]:-s.halfHeight,
                (n%2?1:-1)*(gear?s.track*.5:s.halfWidth*.75));
            RigPoint foot=position+orientation.rotate(local);SurfaceHit hit;
            if(!world.ray(foot+RigPoint(0,tolerance,0),foot-RigPoint(0,tolerance,0),hit))continue;
            // 30 degrees is the parked static-grip limit. Steep-slope gravity
            // and rollover dynamics retain the normal finite-friction solver.
            RigPoint normal=hit.normal.unit();if(normal.y<.866025403784 || normal.dot(up())<.94)return false;
            ++supported;
        }
        return supported>=(s.motorcycle && s.wheelMask?2u:3u);
    }
    bool step(double dt,const DrivingInput& input,const ChassisSpec& s,VehicleEnvironment& world) {
        if(!valid()||!_finite(dt)||dt<0||dt>1.0/30||s.mass<=0||s.radius<=0||s.travel<=0||s.halfHeight<=0||s.halfWidth<=0||s.halfLength<=0||s.cageHeight<0||!_finite(input.throttle+input.brake+input.steer+s.mass+s.radius+s.travel+s.mountHeight+s.halfLength+s.halfWidth+s.halfHeight+s.gravity+s.spring+s.damping+s.grip+s.brakeForce+s.engineForce+s.maxSpeed+s.cageHeight))return false;
        if(dt==0)return true;
        DrivingInput u=input;u.throttle=bounded(u.throttle,0,1);u.brake=bounded(u.brake,0,1);u.steer=bounded(u.steer,-1,1);
        const bool restingInput=(!ignition || u.brake>=.8 || u.handbrake) && u.throttle==0 && !u.pivot;
        const bool wasSlow=speed()<=2 && angular.length()<=.18;
        clock+=dt;
        const double steeringStep=1.2*s.steeringResponse*dt;
        steering+=bounded(u.steer*.58-steering,-steeringStep,steeringStep);
        if(overturned()||fuel<=0)ignition=false;
        if(ignition){fuel=std::max(0.0,fuel-dt*(.002+.014*u.throttle));if(fuel==0)ignition=false;}
        velocity.y-=s.gravity*dt;contacts=0;tireSlip=impact=0;
        RigPoint upVector=up();
        for(int n=0;n<4;++n){compression[n]=0;ground[n].touching=false;}
        for(unsigned n=0;n<6;++n)trackCompression[0][n]=trackCompression[1][n]=0;
        const int supportCount=s.trackMask?12:s.motorcycle?2:4;
        const int wheelSlots=s.trackMask?12:4;
        for(int wheel=0;wheel<wheelSlots;++wheel) {
            const unsigned beltSide=wheel%2;
            if(s.trackMask?!(s.trackMask&(1u<<beltSide)):!(s.wheelMask&(1u<<wheel)))continue;
            RigPoint mount(s.trackMask?s.wheelBase*(.5-(wheel/2)/5.0):(wheel<2?1:-1)*s.wheelBase*.5,s.mountHeight,(beltSide?1:-1)*s.track*.5);
            RigPoint offset=orientation.rotate(mount),start=position+offset;SurfaceHit hit;
            RigPoint forwardLocal(cos(!s.trackMask && wheel<2?steering:0),0,-sin(!s.trackMask && wheel<2?steering:0));
            RigPoint wheelForward=orientation.rotate(forwardLocal);
            double squeeze=0;
            // Sample the round tire at the center and two shoulders. A single
            // axle ray cannot climb a bump until the axle is already above it.
            for(int probe=-1;probe<=1;++probe) {
                const double along=probe*s.radius*.7,drop=sqrt(s.radius*s.radius-along*along);
                const RigPoint rayStart=start+wheelForward*along;SurfaceHit candidate;
                if(!world.ray(rayStart,rayStart-upVector*(s.travel+drop),candidate)||candidate.normal.dot(upVector)<.2)continue;
                double contact=bounded(s.travel+drop-candidate.distance,0,s.travel);
                if(contact>squeeze){squeeze=contact;hit=candidate;}
            }
            const unsigned hub=s.trackMask?(wheel<6?beltSide:beltSide+2):wheel;
            compression[hub]=std::max(compression[hub],squeeze);
            if(s.trackMask)trackCompression[beltSide][5-wheel/2]=squeeze;
            if(squeeze<=0)continue;
            if(hit.treadSurface && hit.normal.y>=.25) {
                // For a belt, keep its rearmost contact while moving forward
                // and its foremost contact while reversing. One ribbon per side.
                const unsigned mark=s.trackMask?beltSide:wheel;
                const double beltSpeed=velocity.dot(forward())+angular.dot(upVector)*(beltSide?1:-1)*s.track*.5;
                if(!s.trackMask || !ground[mark].touching || beltSpeed>=0){ground[mark].point=hit.point;ground[mark].normal=hit.normal;ground[mark].touching=true;}
            }
            ++contacts;RigPoint pv=velocity+cross(angular,offset);
            double load=bounded(s.spring*squeeze-s.damping*pv.dot(hit.normal),0,s.mass*s.gravity*2);
            RigPoint normal=hit.normal.unit();forceAt(normal*load,offset,dt,s);
            RigPoint f=orientation.rotate(forwardLocal);f=(f-normal*f.dot(normal)).unit();RigPoint lateral=cross(f,normal).unit();
            double longitudinal=pv.dot(f),side=pv.dot(lateral);
            tireSlip=std::max(tireSlip,bounded((fabs(side)-5)/35,0,1));
            double drive=ignition && speed()<s.maxSpeed?s.engineForce*u.throttle*(u.reverse?-1:1)/supportCount:0;
            // Split drive force between belts to follow the requested curvature.
            // Throttle remains a force request, as it is for wheels and the route
            // controller. Treating it as belt velocity makes that controller hunt.
            if(s.trackMask && ignition) {
                const double forwardSpeed=velocity.dot(forward());
                const double targetYaw=u.pivot && s.trackMask==3?steering/.58*.8:forwardSpeed*tan(steering)/s.wheelBase;
                const double yawInertia=s.mass*(s.halfWidth*s.halfWidth+s.halfLength*s.halfLength)/3;
                const double differential=(targetYaw-angular.dot(upVector))*yawInertia*40/(6*s.track);
                if(u.pivot)drive=0;
                if(!u.handbrake && u.brake<.5)drive+=bounded(differential,-s.engineForce/supportCount,s.engineForce/supportCount)*(beltSide?1:-1);
            }
            double brake=s.brakeForce*(u.handbrake?1:u.brake)/supportCount;
            double fx=drive-bounded(longitudinal*s.mass/(supportCount*dt),-brake,brake)-longitudinal*s.rollingDrag/supportCount;
            double fy=-side*s.mass/(supportCount*dt);double limit=load*s.grip*(u.handbrake && !s.trackMask && wheel>=2?.45:1);
            if(s.trackMask) {
                // A belt may scrub during a turn, but its whole chassis must
                // still resist sliding sideways on a hill. Capping the entire
                // lateral force at 0.22 * load made ordinary side slopes icy.
                const double translation=velocity.dot(lateral),turn=cross(angular,offset).dot(lateral);
                fy=-translation*s.mass/(supportCount*dt)-bounded(turn*s.mass/(supportCount*dt),-load*.15,load*.15);
            }
            double magnitude=sqrt(fx*fx+fy*fy);if(magnitude>limit && magnitude>0){tireSlip=std::max(tireSlip,bounded((fabs(longitudinal)-12)/70,0,1)*u.brake);fx*=limit/magnitude;fy*=limit/magnitude;}
            forceAt(f*fx+lateral*fy,offset,dt,s);
        }
        if(s.motorcycle && contacts==2 && upVector.y>.55) {
            // A rider balances a single-track vehicle. Apply roll correction
            // through tire contact, with no correction while airborne.
            const RigPoint axis=forward();
            const double roll=atan2(-orientation.rotate(RigPoint(0,0,1)).y,upVector.y);
            const double forwardSpeed=velocity.dot(axis);
            const double lean=bounded(-atan(forwardSpeed*forwardSpeed*tan(steering)/(s.wheelBase*s.gravity)),-.43,.43);
            const double correction=bounded((lean-roll)*85-angular.dot(axis)*16,-45,45);
            angular=angular+axis*(correction*dt);
        }
        velocity=velocity*(1.0/(1+.0005*speed()*dt));angular=angular*(1.0/(1+.12*dt));
        RigPoint old=position;BodyRotation oldRot=orientation;
        position=position+velocity*dt;orientation.advance(angular,dt);
        // Swept chassis corners and short support probes provide impulses at
        // contact points. No ground snap or forced upright rotation in flight.
        bool cageContact=false;double separation=0;
        for(int corner=0;corner<(s.cageHeight>0?26:11);++corner) {
            const double sweepRadius=std::min(s.halfHeight,s.halfWidth)*.85;
            RigPoint local=corner<8?RigPoint((corner&1?1:-1)*s.halfLength,(corner&2?1:-1)*s.halfHeight,(corner&4?1:-1)*s.halfWidth):RigPoint((corner-9)*(s.halfLength-sweepRadius),0,0);
            // Three crowned cage hoops. The roof is higher than the center of
            // mass and narrows toward its crown, rather than a flat box lid.
            if(s.cageHeight>0 && corner<8)local.z*=.75;
            if(corner>=11){int hoop=(corner-11)/5;double arc=((corner-11)%5)*3.141592653589793/4;local=RigPoint((hoop-1)*s.halfLength*.65,s.cageHeight*(.21+.79*sin(arc)),cos(arc)*s.halfWidth);}
            RigPoint offset=orientation.rotate(local),end=position+offset,begin=old+oldRot.rotate(local);SurfaceHit hit;
            const bool pointProbe=corner<8 || corner>=11;
            bool collision=(end-begin).length()>1e-5 && (pointProbe?world.ray(begin,end,hit):world.sweep(begin,end,sweepRadius,hit));
            if(!collision && pointProbe)collision=world.ray(end+RigPoint(0,1,0),end-RigPoint(0,.5,0),hit) && end.y<hit.point.y;
            if(!collision && corner<8 && !(corner&2) && !s.wheelMask && !s.trackMask && restingInput && up().y>.5 && speed()<12) {
                // Construction can finish with lower rails already under the
                // surface. Cast from within the upper chassis, not from the
                // buried rail; retain actual floor/terrain contact normals.
                RigPoint top=end;top.y=std::max(end.y+1,position.y+s.halfHeight);
                collision=world.ray(top,end,hit) && hit.normal.y>.35 && end.y<hit.point.y;
            }
            if(!collision)continue;
            world.collisionContact(hit);
            if(corner>=11 && hit.normal.y>.35)cageContact=true;
            if(!pointProbe) {offset=offset-hit.normal.unit()*sweepRadius;end=position+offset;}
            RigPoint normal=hit.normal.unit();double vn=(velocity+cross(angular,offset)).dot(normal);
            if(vn<0){double denom=1/s.mass+normal.dot(cross(inverseInertia(cross(offset,normal),s),offset));
                if(-vn>impact){impact=-vn;impactPoint=hit.point;}
                double impulse=-(1.05)*vn/std::max(denom,1e-8);impulseAt(normal*impulse,offset,s);
                damage+=std::max(0.0,-vn-45)*.01;
                const bool parkingContact=restingInput && normal.y>=.866025403784;
                RigPoint contactVelocity=parkingContact?velocity+cross(angular,offset):velocity;
                RigPoint tangent=contactVelocity-normal*contactVelocity.dot(normal);double speedT=tangent.length();
                if(speedT>1e-4) {
                    double frictionImpulse=std::min(impulse*.45,speedT*s.mass*.25);
                    if(parkingContact) {
                        // A frame resting on steel rails needs its own ground
                        // friction; absent tires cannot supply braking forces.
                        RigPoint direction=tangent*(1/speedT);
                        double effectiveMass=1/s.mass+direction.dot(cross(inverseInertia(cross(offset,direction),s),offset));
                        frictionImpulse=std::min(impulse*.8,speedT/std::max(effectiveMass,1e-8));
                    }
                    impulseAt(tangent*(-frictionImpulse/speedT),offset,s);
                }
            }
            double penetration=(hit.point-end).dot(normal);if(penetration>0){double correction=std::min(penetration+.02,2.0);position=position+normal*correction;separation+=correction;}
        }
        if(cageContact && up().y<.35) {
            // Only while the cage bears on the ground: retain the direction of
            // the roll through the unstable crown. No upright snap in flight.
            RigPoint axis=forward();double roll=angular.dot(axis);
            double lean=cross(up(),RigPoint(0,1,0)).dot(axis);
            double sign=fabs(roll)>.08?(roll>0?1:-1):(lean>=0?1:-1);
            double assist=s.gravity/s.cageHeight*.38;
            if(fabs(roll)<1.4)angular=angular+axis*(sign*assist*dt);
        }
        if(restingInput && wasSlow && impact<12 && canRest(s,world)) {
            // Keep a settled chassis exactly still instead of accumulating
            // tiny contact impulses into visible creep and false fitting locks.
            if(separation<.1){position=old;orientation=oldRot;}velocity=angular=RigPoint();tireSlip=0;
        }
        wheelSpin+=velocity.dot(forward())/s.radius*dt;
        for(unsigned side=0;side<2;++side){trackSpeed[side]=(s.trackMask&(1u<<side))?velocity.dot(forward())+angular.dot(up())*(side?1:-1)*s.track*.5:0;trackTravel[side]+=trackSpeed[side]*dt;}
        return valid() && speed()<5000 && angular.length()<100;
    }
    bool rightVehicle(const ChassisSpec& s,VehicleEnvironment& world) {
        if(!overturned()||speed()>2||angular.length()>.25)return false;
        double height;if(!world.driveable(position,s.halfLength,height))return false;
        RigPoint f=forward();double yaw=atan2(-f.z,f.x);orientation=BodyRotation(cos(yaw*.5),0,sin(yaw*.5),0);
        position.y=height-s.mountHeight+s.radius+s.travel*.65;velocity=angular=RigPoint();ignition=false;return true;
    }
    std::string save()const {std::ostringstream o;o.precision(17);o<<"PCD1 "<<position.x<<' '<<position.y<<' '<<position.z<<' '<<orientation.w<<' '<<orientation.x<<' '<<orientation.y<<' '<<orientation.z<<' '<<velocity.x<<' '<<velocity.y<<' '<<velocity.z<<' '<<angular.x<<' '<<angular.y<<' '<<angular.z<<' '<<fuel<<' '<<damage;return o.str();}
    bool load(const std::string& text){if(text.size()>1024)return false;ChassisBody b;std::istringstream i(text);std::string v;i>>v>>b.position.x>>b.position.y>>b.position.z>>b.orientation.w>>b.orientation.x>>b.orientation.y>>b.orientation.z>>b.velocity.x>>b.velocity.y>>b.velocity.z>>b.angular.x>>b.angular.y>>b.angular.z>>b.fuel>>b.damage;std::string extra;if(!i||v!="PCD1"||i>>extra||!b.valid()||b.speed()>5000||b.angular.length()>100)return false;b.orientation.normalize();*this=b;return true;}
};
inline bool sameDrivingDestination(const RigPoint& a,const RigPoint& b) {
    RigPoint delta=a-b;delta.y=0;return delta.dot(delta)<1;
}
struct DrivingOrderStream {
    RigPoint last;double stamp;bool seen,lastAppend,mouseHeld,mouseOrder;unsigned suppressed;
    DrivingOrderStream():stamp(0),seen(false),lastAppend(false),mouseHeld(false),mouseOrder(false),suppressed(0){}
    void reset(){seen=false;mouseOrder=false;}
    void observeMouse(bool down){if(down!=mouseHeld){mouseHeld=down;if(down)mouseOrder=false;}}
    bool acceptMouse(const RigPoint& target,bool append,double now,bool down,bool released,bool vehicleMenu) {
        observeMouse(down);
        if(vehicleMenu || ((down||released)&&mouseOrder)){++suppressed;return false;}
        if(!accept(target,append,now))return false;
        if(down||released)mouseOrder=true;
        return true;
    }
    bool accept(const RigPoint& target,bool append,double now) {
        if(!validPoint(target)||!_finite(now))return false;
        const bool repeat=seen && append==lastAppend && now>=stamp && now-stamp<.3 && sameDrivingDestination(last,target);
        stamp=now;
        if(repeat)return false;
        seen=true;last=target;lastAppend=append;return true;
    }
};
struct RouteDriver {
    std::vector<RigPoint> points;size_t next;double handling,pace;bool stopped,reversing,arriving,completed;double steeringCommand;
    bool gearChosen,maneuvering,endingManeuver,resumeReverse,steeringSettled,maneuverBlocked;double maneuverSteer,maneuverTime,clearanceTime,arrivalTime;
    bool forwardTurn;size_t forwardTurnEnd;
    RouteDriver():next(0),handling(1),pace(.6),stopped(true),reversing(false),arriving(false),completed(false),steeringCommand(0),gearChosen(false),maneuvering(false),endingManeuver(false),resumeReverse(false),steeringSettled(false),maneuverBlocked(false),maneuverSteer(0),maneuverTime(0),clearanceTime(0),arrivalTime(0),forwardTurn(false),forwardTurnEnd(0){}
    void stop(){stopped=true;points.clear();next=0;arriving=false;completed=false;gearChosen=false;reversing=resumeReverse=false;maneuvering=endingManeuver=maneuverBlocked=forwardTurn=false;clearanceTime=0;}
    void route(const std::vector<RigPoint>& p){stop();points=p;stopped=p.empty();steeringCommand=0;steeringSettled=false;arrivalTime=0;}
    void extend(const std::vector<RigPoint>& p) {
        if(p.empty())return;
        if(stopped){route(p);return;}
        // Drop already travelled points without changing the current target.
        if(next>1){if(forwardTurn)forwardTurnEnd-=next-1;points.erase(points.begin(),points.begin()+next-1);next=1;}
        size_t first=sameDrivingDestination(points.back(),p.front())?1:0;
        points.insert(points.end(),p.begin()+first,p.end());arriving=false;completed=false;arrivalTime=0;
    }
    double remaining(const RigPoint& position)const {
        if(stopped||next>=points.size())return 0;
        RigPoint gap=points[next]-position;gap.y=0;double distance=gap.length();
        for(size_t i=next+1;i<points.size();++i){gap=points[i]-points[i-1];gap.y=0;distance+=gap.length();}
        return distance;
    }
    bool maneuverPassage(const ChassisBody& body,const ChassisSpec& spec,VehicleEnvironment& world)const {
        const double radius=spec.halfWidth+3,travel=std::max(spec.wheelBase*.75,body.speed()*.8);
        const unsigned count=static_cast<unsigned>(ceil(travel/std::max(2.0,radius*.5)));
        if(count>32)return false;
        double previous;if(!world.startingGround(body.position,radius,previous))return false;
        RigPoint f=body.forward();f.y=0;f=f.unit();RigPoint r(-f.z,0,f.x);
        const double curve=-tan(maneuverSteer*.58)/spec.wheelBase;
        for(unsigned n=1;n<=count;++n) {
            const double distance=travel*n/count*(reversing?-1:1),angle=curve*distance;
            RigPoint p=body.position+f*(sin(angle)/curve)+r*((1-cos(angle))/curve);p.y=previous;double h;
            if(!world.driveable(p,radius,h)||fabs(h-previous)>travel/count*.85+1)return false;
            previous=h;
        }
        return true;
    }
    bool planForwardTurn(const ChassisBody& body,const ChassisSpec& spec,VehicleEnvironment& world) {
        // Try both forward tangent arcs before asking for a three-point turn.
        // Clearance follows the complete arc, including the side away from the
        // destination. Every sample is bounded and uses the normal road checks.
        const double pi=3.141592653589793,turnRadius=spec.wheelBase/tan(.4),width=spec.halfWidth+3;
        RigPoint f=body.forward();f.y=0;f=f.unit();RigPoint right(-f.z,0,f.x);
        RigPoint endpoint=points[next],toward=endpoint-body.position;toward.y=0;
        const bool local=toward.length()>spec.wheelBase*5;
        if(local)endpoint=body.position+toward.unit()*(spec.wheelBase*5);
        double endpointHeight;if(!world.driveable(endpoint,width,endpointHeight))return false;endpoint.y=endpointHeight;
        std::vector<RigPoint> best;double bestLength=1e30;
        for(int sign=-1;sign<=1;sign+=2) {
            RigPoint center=body.position+right*(sign*turnRadius),gap=endpoint-center;gap.y=0;
            const double distance=gap.length();if(distance<=turnRadius+.01)continue;
            const double endAngle=atan2(gap.z,gap.x)-sign*acos(turnRadius/distance);
            const RigPoint radial=body.position-center;const double startAngle=atan2(radial.z,radial.x);
            double angle=sign*(endAngle-startAngle);while(angle<0)angle+=2*pi;while(angle>=2*pi)angle-=2*pi;
            const double arcLength=angle*turnRadius,total=arcLength+sqrt(distance*distance-turnRadius*turnRadius);
            if(total>=bestLength)continue;
            const unsigned count=std::max(1u,static_cast<unsigned>(ceil(arcLength/std::max(2.0,width*.5))));
            if(count>96)continue;
            std::vector<RigPoint> path;RigPoint prior=body.position;double h;
            if(!world.startingGround(prior,width,h))continue;prior.y=h;bool clear=true;
            for(unsigned n=1;n<=count;++n) {
                const double a=startAngle+sign*angle*n/count;
                RigPoint p=center+RigPoint(cos(a)*turnRadius,0,sin(a)*turnRadius);p.y=prior.y;
                if(!world.driveable(p,width,h)){clear=false;break;}p.y=h;
                RigPoint step=p-prior;step.y=0;if(fabs(p.y-prior.y)>step.length()*.85+1){clear=false;break;}
                path.push_back(p);prior=p;
            }
            if(clear && world.passage(prior,endpoint,width)){if(local)path.push_back(endpoint);best=path;bestLength=total;}
        }
        if(best.empty())return false;
        // Keep the incoming segment, not an ever-growing history of prior arcs.
        if(next>1){points.erase(points.begin(),points.begin()+next-1);next=1;}
        forwardTurnEnd=next+best.size()-(local?1:0);points.insert(points.begin()+next,best.begin(),best.end());forwardTurn=true;return true;
    }
    DrivingInput control(const ChassisBody& body,const ChassisSpec& spec,double dt,VehicleEnvironment* world=0) {
        DrivingInput out;out.brake=1;out.reverse=reversing;out.steer=steeringCommand;
        if(stopped || points.empty() || body.overturned() || !body.ignition)return out;
        const double skill=bounded(handling/100,0,1),speed=body.speed();
        const double look=std::max(spec.wheelBase*1.5,10+speed*(.35+.45*skill));
        while(next+1<points.size()) {
            RigPoint gap=points[next]-body.position;gap.y=0;
            RigPoint outgoing=points[next+1]-points[next];outgoing.y=0;
            // The planner can finish after the car has passed its origin.
            // Passed points on the same corridor must never demand a U-turn.
            const bool passed=gap.dot(outgoing)<0 && cross(gap,outgoing.unit()).length()<spec.halfWidth;
            if(gap.length()>std::max(spec.halfWidth*.8,speed*.15) && !passed)break;
            ++next;if(maneuvering)endingManeuver=true;
        }
        if(forwardTurn && next>forwardTurnEnd)forwardTurn=false;
        RigPoint delta=points[next]-body.position;delta.y=0;double distance=delta.length();
        const RigPoint waypointDelta=delta;
        if(next+1==points.size() && (distance<10||arriving)) {
            arriving=true;maneuvering=endingManeuver=false;arrivalTime+=dt;
            // Arrival is an order state, not a physics anchor. Gravity may
            // keep moving the car on a slope even with the brakes fully applied.
            if(speed<2 || arrivalTime>=2){stop();completed=true;}return out;
        }
        // Project onto a long segment before looking ahead. A distant waypoint
        // must not dilute steering corrections by its entire distance.
        if(next && distance>look && !maneuvering) {
            RigPoint segment=points[next]-points[next-1];segment.y=0;
            RigPoint relative=body.position-points[next-1];relative.y=0;
            const double length=segment.length();
            const double along=bounded(relative.dot(segment.unit()),0,length);
            const RigPoint projection=points[next-1]+segment.unit()*along;
            RigPoint lateral=projection-body.position;lateral.y=0;
            RigPoint heading=body.forward();heading.y=0;heading=heading.unit();
            if(lateral.length()<look && fabs(heading.dot(segment.unit()))>.8) {
                delta=projection+segment.unit()*std::min(look,length-along)-body.position;delta.y=0;distance=delta.length();
            }
        } else if(forwardTurn && next<forwardTurnEnd && distance<look) {
            RigPoint aim=points[next];double available=look-distance;
            for(size_t n=next+1;n<=forwardTurnEnd && available>0;++n) {
                RigPoint segment=points[n]-aim;segment.y=0;double length=segment.length();
                aim=aim+segment.unit()*std::min(available,length);available-=length;
            }
            delta=aim-body.position;delta.y=0;distance=delta.length();
        } else if(next+1<points.size() && distance<look) {
            RigPoint outgoing=points[next+1]-points[next];outgoing.y=0;
            double ahead=std::min(std::min(look-distance,outgoing.length()),spec.halfWidth);
            delta=delta+outgoing.unit()*ahead;distance=delta.length();
        }
        const RigPoint maneuverDelta=next+1<points.size()?delta:waypointDelta;
        const double maneuverDistance=maneuverDelta.length();
        if(distance>look){delta=delta*(look/distance);distance=look;}
        RigPoint forward=body.forward();forward.y=0;forward=forward.unit();
        RigPoint right(-forward.z,0,forward.x);
        const double ahead=delta.dot(forward),side=delta.dot(right);
        const double waypointAhead=maneuverDelta.dot(forward),waypointSide=maneuverDelta.dot(right);
        if(spec.trackMask==3) {
            const double headingError=atan2(-waypointSide,waypointAhead);
            // Differential drive can align without the wheeled three-point turn.
            if(fabs(headingError)>.65 || (maneuvering && fabs(headingError)>.16)) {
                maneuvering=true;reversing=false;out.steer=bounded(headingError*1.7,-1,1);steeringCommand=out.steer;
                if(speed>3)return out;
                clearanceTime-=dt;
                if(world && clearanceTime<=0){double h;maneuverBlocked=!world->driveable(body.position,sqrt(spec.halfLength*spec.halfLength+spec.halfWidth*spec.halfWidth)+2,h);clearanceTime=.3;}
                if(maneuverBlocked)return out;
                out.pivot=true;out.brake=0;out.throttle=fabs(out.steer)*.6;out.reverse=false;return out;
            }
            maneuvering=maneuverBlocked=false;gearChosen=steeringSettled=true;reversing=false;
        }

        if(!gearChosen) {
            // Prefer forward travel, including targets behind the car. The
            // exception is retreating straight uphill when already pointing
            // steeply downhill: a forward U-turn would first descend farther.
            const bool hillRetreat=body.forward().y<-.30 && waypointAhead<-maneuverDistance*.8;
            if(body.velocity.dot(forward)*(hillRetreat?-1:1)<-2)return out;
            reversing=hillRetreat;gearChosen=true;
        }
        const double maxCurvature=tan(.58)/spec.wheelBase;
        const double travelAhead=waypointAhead*(reversing?-1:1);
        const double requiredCurvature=-2*waypointSide/std::max(36.0,maneuverDistance*maneuverDistance);
        // Reverse pursuit includes the rear-axle offset, preventing the back
        // of the car from continually missing a nearby target on a curved arc.
        const double axleAhead=ahead+(reversing?spec.wheelBase*.5:0);
        double curvature=-2*side/std::max(36.0,axleAhead*axleAhead+side*side);
        if(!spec.trackMask && !forwardTurn && !maneuvering && (travelAhead<-maneuverDistance*.35 || fabs(requiredCurvature)>maxCurvature*.95)) {
            if(!reversing && world && planForwardTurn(body,spec,*world))return control(body,spec,dt,world);
            maneuvering=true;endingManeuver=false;resumeReverse=reversing;maneuverTime=0;clearanceTime=0;
            // Countersteer while backing away to change the turning circle;
            // reversing with the same steering simply retraces the failed arc.
            maneuverSteer=waypointSide>0?1:-1;
        }
        if(maneuvering) {
            if(reversing!=resumeReverse && speed>2)maneuverTime+=dt;
            const double normalAhead=waypointAhead*(resumeReverse?-1:1);
            if((maneuverTime>1 && normalAhead>maneuverDistance*.9 && fabs(requiredCurvature)<maxCurvature*.5) || maneuverTime>3)endingManeuver=true;
            const bool wantReverse=endingManeuver?resumeReverse:!resumeReverse;
            if(wantReverse!=reversing){if(speed>=2)return out;reversing=wantReverse;steeringSettled=false;}
            if(endingManeuver){maneuvering=false;endingManeuver=false;}
        }
        out.reverse=reversing;
        const double angle=reversing?atan2(side,-ahead):atan2(-side,ahead);
        double target=maneuvering?maneuverSteer:bounded(atan(spec.wheelBase*curvature)/.58,-1,1);
        target+=sin(body.clock*(1.1+.5*skill))*.04*(1-skill)*bounded(speed/spec.maxSpeed,0,1);
        target=bounded(target,-1,1);
        steeringCommand+=bounded(target-steeringCommand,-dt*spec.steeringResponse*(.8+2.2*skill),dt*spec.steeringResponse*(.8+2.2*skill));out.steer=steeringCommand;
        // A gear change must let the wheels countersteer before applying power.
        // Otherwise low-skill drivers drive the previous arc during rack travel.
        if(!steeringSettled) {
            if(speed<2 && fabs(body.steering-target*.58)>.08)return out;
            steeringSettled=true;
        }
        if(maneuvering && world) {
            clearanceTime-=dt;
            if(clearanceTime<=0){maneuverBlocked=!maneuverPassage(body,spec,*world);clearanceTime=.25;}
            if(maneuverBlocked)return out;
        } else {maneuverBlocked=false;clearanceTime=0;}
        double desired=spec.maxSpeed*bounded(pace,.2,1);
        // Ease off while a slow steering rack catches up to a sharp bend.
        // Handling changes corner speed and response, not straight-line power.
        if(fabs(body.steering-target*.58)>.12)desired=std::min(desired,18+27*skill);
        if(reversing)desired=std::min(desired,25.0);
        if(maneuvering)desired=std::min(desired,18.0);
        desired=std::min(desired,sqrt(spec.grip*spec.gravity*(.38+.37*skill)/std::max(fabs(curvature),.0001)));
        const double deceleration=std::min(spec.brakeForce/spec.mass,spec.grip*spec.gravity*.6);
        if(!maneuvering)desired=std::min(desired,sqrt(std::max(0.0,remaining(body.position)-5)*deceleration));
        if(fabs(angle)>1.35)desired=std::min(desired,18.0);
        // Supply the force needed to hold speed against grade and rolling drag.
        // Speed error alone starves the engine near a slow uphill target.
        const double grade=body.forward().y*(reversing?-1:1);
        const double resistance=spec.mass*spec.gravity*grade+desired*spec.rollingDrag+spec.mass*.0005*desired*desired;
        const double progressSpeed=body.velocity.dot(body.forward())*(reversing?-1:1);
        // Rollback is motion against the order, not progress toward cruise
        // speed. Arrest it with the brakes before applying power uphill again.
        if(progressSpeed<-2)return out;
        const double effort=resistance/std::max(1.0,spec.engineForce)+(desired-progressSpeed)/30;
        out.brake=bounded(-effort*spec.engineForce/std::max(1.0,spec.brakeForce),0,1);
        out.throttle=bounded(effort,0,1);
        return out;
    }
};
inline bool completeVehicleDestination(RouteDriver& driver,std::vector<RigPoint>& destinations,bool planning,bool finalLeg) {
    if(!driver.completed || planning || !finalLeg || destinations.empty())return false;
    destinations.erase(destinations.begin());driver.completed=false;return true;
}
// Incremental A*: bounded work and memory. Edges require width clearance and
// slope checks from the environment; diagonal corner cutting is forbidden.
class VehicleRoute {
    struct Node {double cost;int parent;RigPoint p;bool closed;Node():cost(1e30),parent(-1),closed(false){}};
    struct Ground {RigPoint p;bool clear;Ground():clear(false){}};
    struct Pending {double cost;int id;bool operator<(const Pending& b)const{return cost>b.cost;}};
    std::map<int,Node> nodes;std::map<int,Ground> ground;
    std::map<std::pair<int,int>,bool> edges;
    std::priority_queue<Pending> open;RigPoint origin,goal,directPrior;
    double cell,radius;int endId;unsigned directStep,directCount;bool done,failed,started,goalChecked,direct,directResult;
    int key(int x,int z)const{return (x+128)*257+z+128;}
    static double horizontalDistance(const RigPoint& a,const RigPoint& b){RigPoint d=a-b;d.y=0;return d.length();}
    bool sample(int x,int z,VehicleEnvironment& w,RigPoint& p){
        if(abs(x)>127||abs(z)>127)return false;
        const int id=key(x,z);std::map<int,Ground>::iterator found=ground.find(id);
        if(found==ground.end()) {
            if(ground.size()>=16384)return false;
            Ground g;g.p=origin+RigPoint(x*cell,0,z*cell);double h;
            g.clear=w.driveable(g.p,radius,h);if(g.clear)g.p.y=h;
            found=ground.insert(std::make_pair(id,g)).first;
        }
        p=found->second.p;return found->second.clear;
    }
    bool edge(int a,int b,const RigPoint& from,const RigPoint& to,VehicleEnvironment& w){
        // Direction is part of the key: slope/step checks need not be symmetric.
        const std::pair<int,int> id(a,b);std::map<std::pair<int,int>,bool>::iterator found=edges.find(id);
        if(found!=edges.end())return found->second;
        const bool clear=w.passage(from,to,radius);edges[id]=clear;return clear;
    }
public:
    VehicleRoute():cell(24),radius(12),endId(-1),directStep(0),directCount(0),done(false),failed(false),started(false),goalChecked(false),direct(true),directResult(false){}
    void begin(const RigPoint& from,const RigPoint& target,double width){
        nodes.clear();ground.clear();edges.clear();open=std::priority_queue<Pending>();done=false;failed=true;endId=-1;
        started=false;goalChecked=false;direct=true;directResult=false;directStep=0;
        if(!validPoint(from)||!validPoint(target)||!_finite(width)||width<=0||width>10000)return;
        origin=from;goal=target;radius=width;cell=std::max(8.0,width*1.6);
        double x=(goal.x-origin.x)/cell,z=(goal.z-origin.z)/cell;
        if(!_finite(x)||!_finite(z)||fabs(x)>126||fabs(z)>126)return;
        directCount=std::max(1u,static_cast<unsigned>(ceil(horizontalDistance(from,target)/std::max(2.0,radius*.5))));failed=false;
        Node& n=nodes[key(0,0)];n.cost=0;n.p=from;Pending p={0,key(0,0)};open.push(p);
    }
    bool advance(VehicleEnvironment& w,unsigned budget=24){if(done||failed)return true;
        if(!budget)return false;
        if(!started){double h;if(!w.startingGround(origin,radius,h)){failed=true;return true;}nodes[key(0,0)].p.y=h;origin.y=h;directPrior=origin;started=true;--budget;}
        // An occupied or unloaded endpoint cannot be rescued by searching the
        // entire grid. This also removes click-height error from the heuristic.
        if(!goalChecked && budget){--budget;double h;if(!w.driveable(goal,radius,h)){failed=true;return true;}goal.y=h;goalChecked=true;}
        if(!goalChecked)return false;
        // One full-width ground survey per unit, yielded between updates. A
        // visible straight corridor does not need a grid search at all.
        while(direct && budget) {
            --budget;++directStep;RigPoint p=origin+(goal-origin)*(double(directStep)/directCount);double h;
            if(!w.driveable(p,radius,h)||fabs(h-directPrior.y)>horizontalDistance(p,directPrior)*.85+1.0){direct=false;return false;}
            p.y=h;directPrior=p;
            if(directStep==directCount){goal=p;done=true;directResult=true;return true;}
        }
        if(direct)return false;
        while(budget-- && !open.empty()) {Pending p=open.top();open.pop();Node& n=nodes[p.id];if(n.closed)continue;n.closed=true;int x=p.id/257-128,z=p.id%257-128;
            RigPoint toGoal=goal-n.p;toGoal.y=0;
            if(toGoal.length()<=cell*1.5) {
                double height;
                if(w.driveable(goal,radius,height)) {
                    RigPoint endpoint(goal.x,height,goal.z);
                    if(w.passage(n.p,endpoint,radius)){goal=endpoint;endId=p.id;done=true;return true;}
                }
            }
            if(nodes.size()>8192){failed=true;return true;}
            for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz){if(!dx&&!dz)continue;
                const int id=key(x+dx,z+dz);std::map<int,Node>::iterator seen=nodes.find(id);
                if(seen!=nodes.end() && seen->second.closed)continue;
                RigPoint q;if(!sample(x+dx,z+dz,w,q))continue;
                double c=n.cost+(q-n.p).length();if(seen!=nodes.end() && c>=seen->second.cost)continue;
                if(dx&&dz){RigPoint side;if(!sample(x+dx,z,w,side)||!sample(x,z+dz,w,side))continue;}
                if(!edge(p.id,id,n.p,q,w))continue;
                Node& nextNode=nodes[id];nextNode.cost=c;nextNode.parent=p.id;nextNode.p=q;Pending candidate={c+horizontalDistance(goal,q),id};open.push(candidate);
            }
        }if(open.empty())failed=true;return done||failed;
    }
    bool success()const{return done&&!failed;}
    bool checkingDirect()const{return direct;}
    bool usedDirect()const{return directResult;}
    std::vector<RigPoint> result()const {
        std::vector<RigPoint> r;if(!success())return r;
        if(directResult){r.push_back(origin);r.push_back(goal);return r;}int id=endId;
        for(unsigned i=0;i<8192&&id!=-1;++i){std::map<int,Node>::const_iterator n=nodes.find(id);if(n==nodes.end())return std::vector<RigPoint>();r.push_back(n->second.p);id=n->second.parent;}
        std::reverse(r.begin(),r.end());
        if(r.empty() || (r.back()-goal).length()>.01)r.push_back(goal);
        // Only collapse collinear edges that were already checked. Never cut
        // across an obstacle by shortcutting a bend after the clearance search.
        std::vector<RigPoint> compact;
        for(size_t i=0;i<r.size();++i) {
            if(compact.size()>1) {
                RigPoint a=compact.back()-compact[compact.size()-2],b=r[i]-compact.back();
                a.y=b.y=0; // Steering follows ground-plane direction; suspension follows height.
                if(a.dot(b)>0 && cross(a.unit(),b.unit()).length()<1e-6)compact.pop_back();
            }
            compact.push_back(r[i]);
        }
        return compact;
    }
};
}
