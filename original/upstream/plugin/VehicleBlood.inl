// The legacy KingOfRenderThread::addTerrainBloodSplat is a no-op on Steam
// 1.0.65. Follow the mapped combat splat -> decal request -> projection chain.
// See verify-vehicle-blood-native.py and Rekenshi's discovery entry.
struct NativeBloodDecal {
    unsigned tile;unsigned char kind,image;unsigned short x,z,width,height;
    short angle;unsigned colour;float life;
};
typedef char BloodDecalSize[sizeof(NativeBloodDecal)==24?1:-1];
typedef char BloodDecalWidth[offsetof(NativeBloodDecal,width)==0xa?1:-1];
typedef char BloodDecalColour[offsetof(NativeBloodDecal,colour)==0x10?1:-1];
struct BloodPickup {double x,z,radius;};
struct BloodInbox {
    CRITICAL_SECTION mutex;unsigned generation,count;BloodPickup pending[512];
    BloodInbox():generation(0),count(0){InitializeCriticalSection(&mutex);}
    ~BloodInbox(){DeleteCriticalSection(&mutex);}
} bloodInbox;
struct BloodLock {BloodLock(){EnterCriticalSection(&bloodInbox.mutex);}~BloodLock(){LeaveCriticalSection(&bloodInbox.mutex);}};
cars::TreadBloodGround bloodGround;
void (*originalBloodProjection)(void*,const Ogre::Vector3&,float,float,NativeBloodDecal*)=0;
unsigned char* bloodJump(unsigned char* p){for(unsigned n=0;p && *p==0xe9 && n<4;++n)p=p+5+*reinterpret_cast<int*>(p+1);return p;}
void* bloodProjectionEntry(){
    __try {
        unsigned char* splat=bloodJump(reinterpret_cast<unsigned char*>(KenshiLib::GetRealAddress(&MedicalSystem::splatBlood)));
        if(!splat || memcmp(splat,"\x48\x89\x5c\x24\x10\x48\x89\x6c\x24\x18\x56",11) || splat[0x20a]!=0xe8)return 0;
        unsigned char* request=bloodJump(splat+0x20f+*reinterpret_cast<int*>(splat+0x20b));
        if(memcmp(request,"\x40\x53\x55\x56\x57\x48\x81\xec\x98\x00\x00\x00",12) || request[0x10b]!=0xe8)return 0;
        unsigned char* project=bloodJump(request+0x110+*reinterpret_cast<int*>(request+0x10c));
        if(memcmp(project,"\x48\x8b\xc4\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57",15) ||
           memcmp(project+0x70,"\x0f\xb6\x46\x04",4) ||
           memcmp(project+0x10f,"\xf3\x45\x0f\x59\x96\xc0\x01\x00\x00",9))return 0;
        return project;
    } __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
bool copyBloodPickup(void* manager,const Ogre::Vector3& position,const NativeBloodDecal* decal,BloodPickup& out){
    __try {
        if(!decal || decal->kind<1 || decal->kind>3 || (decal->colour>>24)<16 || decal->life<=0)return false;
        const unsigned char* bytes=static_cast<const unsigned char*>(manager);
        if(!*reinterpret_cast<const size_t*>(bytes+0xd0+24*decal->kind))return false;
        // These are the exact X/Z half-extents used for native projection,
        // reconstructed from its tenths-encoded size and manager texel scale.
        const double x=decal->width*.1*(*reinterpret_cast<const float*>(bytes+0x1c0));
        const double z=decal->height*.1*(*reinterpret_cast<const float*>(bytes+0x1c4));
        if(!_finite(x)||!_finite(z)||x<=0||z<=0)return false;
        out.x=position.x;out.z=position.z;out.radius=std::min(x,z)*.8;
        return _finite(out.x)&&_finite(out.z)&&out.radius>0&&out.radius<=200;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void bloodProjection(void* manager,const Ogre::Vector3& position,float size,float angle,NativeBloodDecal* decal){
    unsigned generation;{BloodLock lock;generation=bloodInbox.generation;}
    // Never hold our locks while invoking the native renderer. Its own locks
    // and all effects remain unchanged. No borrowed pointer leaves this call.
    originalBloodProjection(manager,position,size,angle,decal);
    BloodPickup pickup;
    if(cars::vehicleWorldChanging() || !copyBloodPickup(manager,position,decal,pickup))return;
    BloodLock lock;
    if(generation==bloodInbox.generation && bloodInbox.count<512)bloodInbox.pending[bloodInbox.count++]=pickup;
}
void clearBloodPickups(){BloodLock lock;++bloodInbox.generation;bloodInbox.count=0;bloodGround.clear();}
void drainBloodPickups(double clock){
    // Main-thread drain. The bounded POD inbox is the only shared hook state.
    BloodPickup copy[512];unsigned count;
    {BloodLock lock;count=bloodInbox.count;memcpy(copy,bloodInbox.pending,count*sizeof(BloodPickup));bloodInbox.count=0;}
    bloodGround.advance(clock);
    for(unsigned n=0;n<count;++n)bloodGround.add(copy[n].x,copy[n].z,copy[n].radius,clock);
}
bool installBloodPickups(){
    void* target=bloodProjectionEntry();
    if(!target || KenshiLib::AddHook(target,reinterpret_cast<void*>(bloodProjection),reinterpret_cast<void**>(&originalBloodProjection))!=KenshiLib::SUCCESS)return false;
    DebugLog("Project Cars: native ground blood pickup enabled for wheel/track marks");return true;
}
