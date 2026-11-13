#pragma once
#include "compat/HandleIdentity.h"
#include "../src/ScopedQueryGroups.h"
#include "../src/VehicleCollisionPose.h"
// Steam 1.0.65 SimplePhysXEntity actor array verified in simple-update.txt.
// NxActor 2.8.4 ABI: virtual destructor occupies slot 0; positions are slot 10,
// quaternion slot 12, setters 6 and 8. See docs/DRIVING.md for source/evidence.
// Calls are made only inside PhysicsActual::updateUT, where native Kenshi also
// reads these actors after simulation. Never retain an actor across callbacks.
namespace cars {
struct PhysicsVector {float x,y,z;};
struct PhysicsQuaternion {float x,y,z,w;};
inline bool executableAddress(void* address) {
    MEMORY_BASIC_INFORMATION m;if(!address || !VirtualQuery(address,&m,sizeof(m)))return false;
    return m.State==MEM_COMMIT && !(m.Protect&PAGE_GUARD) && (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
// Installed set/getActorCollisionGroup verifies these NxShape group slots.
// Work per shape so mixed groups and native owner userdata remain unchanged.
struct NativeShapeGroups {
    static bool read(NxShape* shape,unsigned short& group) {
        __try {
            if(!shape)return false;void** vt=*reinterpret_cast<void***>(shape);
            if(!executableAddress(vt[3]))return false;
            typedef unsigned short (*Read)(NxShape*);group=reinterpret_cast<Read>(vt[3])(shape);return group<=31;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    static bool write(NxShape* shape,unsigned short group) {
        __try {
            if(!shape||group>31)return false;void** vt=*reinterpret_cast<void***>(shape);
            if(!executableAddress(vt[2]))return false;
            typedef void (*Write)(NxShape*,unsigned short);reinterpret_cast<Write>(vt[2])(shape,group);return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
};
// NxShape 2.8.4: getFlag slot 6, getName slot 29. Group membership is
// insufficient: queryable trigger/disabled-response shapes are not walls.
struct NativeShapeInfo {
    unsigned flags;char name[96];
    NativeShapeInfo():flags(0){name[0]=0;}
    bool read(NxShape* shape,bool withName=false) {
        __try {
            if(!shape)return false;void** vt=*reinterpret_cast<void***>(shape);
            if(!executableAddress(vt[6]))return false;
            typedef bool (*Flag)(NxShape*,unsigned);
            const unsigned bits[]={1,2,4,0x10,0x1000};flags=0;
            for(unsigned n=0;n<5;++n)if(reinterpret_cast<Flag>(vt[6])(shape,bits[n]))flags|=bits[n];
            if(withName){
                if(!executableAddress(vt[29]))return false;
                typedef const char* (*Name)(NxShape*);const char* text=reinterpret_cast<Name>(vt[29])(shape);
                unsigned n=0;if(text)for(;n<95 && text[n];++n)name[n]=text[n];name[n]=0;
            }return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    bool solid()const{return (flags&(7|0x10|0x1000))==0;}
};
class CollisionQueryMask {
    ScopedQueryGroups<NxShape*,NativeShapeGroups> shapes;bool complete;unsigned riderCount;
    bool excludeActor(NxActor* actor,bool nonSolidOnly=false) {
        __try {
            if(!actor)return false;void** vt=*reinterpret_cast<void***>(actor);
            if(!executableAddress(vt[19])||!executableAddress(vt[20]))return false;
            typedef unsigned (*Count)(NxActor*);typedef NxShape*const* (*List)(NxActor*);
            unsigned count=reinterpret_cast<Count>(vt[19])(actor);
            NxShape*const* list=reinterpret_cast<List>(vt[20])(actor);
            if(count>1024||(count&&!list))return false;
            for(unsigned n=0;n<count;++n){
                if(nonSolidOnly){NativeShapeInfo info;if(!info.read(list[n]) || info.solid())continue;}
                if(!shapes.exclude(list[n]))return false;
            }return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    bool excludePart(SimplePhysXEntity* entity,bool nonSolidOnly=false) {
        __try {
            if(!entity)return true;char* part=reinterpret_cast<char*>(entity);
            unsigned size=*reinterpret_cast<unsigned*>(part+0x58);NxActor** list=*reinterpret_cast<NxActor***>(part+0x60);
            if(size>64 || (size&&!list))return false;
            for(unsigned k=0;k<size;++k)if(!excludeActor(list[k],nonSolidOnly))return false;
            return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
    bool acquire(Building* b,Character* rider) {
        __try {
            if(!b||!b->physical||b->physical->parts.size()>64)return false;
            for(unsigned n=0;n<b->physical->parts.size();++n) {
                char* part=reinterpret_cast<char*>(b->physical->parts[n]);if(!part)return false;
                unsigned size=*reinterpret_cast<unsigned*>(part+0x58);NxActor** list=*reinterpret_cast<NxActor***>(part+0x60);
                if(size>64||(size&&!list))return false;
                for(unsigned k=0;k<size;++k)if(!excludeActor(list[k]))return false;
            }
            if(rider&&rider->getMovement()) {
                // Steam 1.0.65 refreshClickHull/createTriggerHull and native
                // hull creation/update prove actor +0x50 and owner hand +0x58.
                char* hull=reinterpret_cast<char*>(rider->getMovement()->clickHull);
                if(hull) {
                    if(!sameHandle(*reinterpret_cast<hand*>(hull+0x58),rider->getHandle()))return false;
                    NxActor* actor=*reinterpret_cast<NxActor**>(hull+0x50);
                    if(actor){unsigned before=shapes.size();if(!excludeActor(actor))return false;riderCount=shapes.size()-before;}
                }
            }
            return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    }
public:
    CollisionQueryMask():complete(true),riderCount(0){}
    CollisionQueryMask(Building* b,Character* rider):complete(false),riderCount(0){complete=acquire(b,rider);}
    bool excludeBuilding(Building* b){if(!acquire(b,0))return complete=false;return true;}
    bool excludeNonSolid(Building* b) {
        __try {
            if(!b || !b->physical || b->physical->parts.size()>64)return false;
            for(unsigned n=0;n<b->physical->parts.size();++n)
                if(!excludePart(b->physical->parts[n],true))return complete=false;
            // This separate native interaction volume is not in physical.parts.
            if(!excludePart(b->triggerVolume))return complete=false;
            return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return complete=false;}
    }
    bool excludeShape(NxShape* shape){return shapes.exclude(shape);}
    bool excludeRider(Character* rider) {
        __try {
            if(!rider||!rider->getMovement())return true;
            char* hull=reinterpret_cast<char*>(rider->getMovement()->clickHull);if(!hull)return true;
            if(!sameHandle(*reinterpret_cast<hand*>(hull+0x58),rider->getHandle()))return complete=false;
            NxActor* actor=*reinterpret_cast<NxActor**>(hull+0x50);
            if(actor && !excludeActor(actor))return complete=false;
            return true;
        }__except(EXCEPTION_EXECUTE_HANDLER){return complete=false;}
    }
    bool ready()const{return complete;}
    unsigned riderShapes()const{return riderCount;}
    unsigned shapeCount()const{return shapes.size();}
    bool restore(){return shapes.restore();}
    ~CollisionQueryMask(){if(!shapes.restore())ErrorLog("Project Cars: could not restore a temporary collision query group");}
private:CollisionQueryMask(const CollisionQueryMask&);CollisionQueryMask& operator=(const CollisionQueryMask&);
};
struct CollisionPoseReport {
    const char* reason;unsigned part,actor,count;double distance,norm;PhysicsVector position;
    CollisionPoseReport():reason("ok"),part(0),actor(0),count(0),distance(0),norm(0){position.x=position.y=position.z=0;}
    bool fail(const char* text){reason=text;return false;}
};
inline bool moveNativeCollision(Building* b,const Ogre::Vector3& p,const Ogre::Quaternion& q,bool apply,CollisionPoseReport* detail=0) {
    CollisionPoseReport local;CollisionPoseReport& report=detail?*detail:local;
    report=CollisionPoseReport();
    __try {
        if(!b || !b->physical || !_finite(b->getScale()) || b->getScale()<=.0001f)return report.fail("physical collection or scale");
        void* actors[256];PhysicsVector positions[256];PhysicsQuaternion rotations[256];unsigned count=0;
        const Ogre::Vector3 old=b->getPosition();
        if(b->physical->parts.size()>64)return report.fail("part count");
        for(unsigned n=0;n<b->physical->parts.size();++n) {
            report.part=n;char* part=reinterpret_cast<char*>(b->physical->parts[n]);if(!part)return report.fail("null part");
            unsigned size=*reinterpret_cast<unsigned*>(part+0x58);
            void** list=*reinterpret_cast<void***>(part+0x60);
            if(size>64 || count+size>256 || (size&&!list))return report.fail("actor list");
            for(unsigned a=0;a<size;++a) {
                report.actor=a;report.count=count;void* actor=list[a];if(!actor)return report.fail("null actor");void** vt=*reinterpret_cast<void***>(actor);
                if(!executableAddress(vt[6])||!executableAddress(vt[8])||!executableAddress(vt[10])||!executableAddress(vt[12]))return report.fail("pose vtable");
                PhysicsVector position;PhysicsQuaternion rotation;
                typedef PhysicsVector* (*ReadPosition)(void*,PhysicsVector*);
                typedef PhysicsQuaternion* (*ReadRotation)(void*,PhysicsQuaternion*);
                reinterpret_cast<ReadPosition>(vt[10])(actor,&position);reinterpret_cast<ReadRotation>(vt[12])(actor,&rotation);
                Ogre::Vector3 v(position.x,position.y,position.z);
                report.position=position;report.distance=(v-old).length();
                // Fail closed on an unexpected physics origin/ABI, before changing any actor.
                if(!_finite(v.x+v.y+v.z) || v.squaredDistance(old)>40000*b->getScale()*b->getScale())return report.fail("actor position outside frame");
                const float norm=rotation.w*rotation.w+rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z;
                report.norm=norm;if(!_finite(norm)||norm<.5f||norm>1.5f)return report.fail("actor rotation length");
                RigPoint moved;BodyRotation oriented;
                if(!transformCollisionPose(RigPoint(old.x,old.y,old.z),BodyRotation(b->rot.w,b->rot.x,b->rot.y,b->rot.z),RigPoint(p.x,p.y,p.z),BodyRotation(q.w,q.x,q.y,q.z),RigPoint(v.x,v.y,v.z),BodyRotation(rotation.w,rotation.x,rotation.y,rotation.z),b->getScale(),moved,oriented))return report.fail("pose transform");
                v=Ogre::Vector3(float(moved.x),float(moved.y),float(moved.z));Ogre::Quaternion r(float(oriented.w),float(oriented.x),float(oriented.y),float(oriented.z));
                positions[count].x=v.x;positions[count].y=v.y;positions[count].z=v.z;
                rotations[count].x=r.x;rotations[count].y=r.y;rotations[count].z=r.z;rotations[count].w=r.w;actors[count++]=actor;
            }
        }
        report.count=count;if(!count)return report.fail("no actors");
        if(apply)for(unsigned n=0;n<count;++n) {
            void** vt=*reinterpret_cast<void***>(actors[n]);
            typedef void (*SetPosition)(void*,const PhysicsVector&);typedef void (*SetRotation)(void*,const PhysicsQuaternion&);
            reinterpret_cast<SetPosition>(vt[6])(actors[n],positions[n]);reinterpret_cast<SetRotation>(vt[8])(actors[n],rotations[n]);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return report.fail("native pose exception");}
}
}
