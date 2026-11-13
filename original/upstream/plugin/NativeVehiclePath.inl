// Native requests contain a character HANDLE, not a pointer to their requester.
// Keep a separate HavokCharacter for route calculation. Redirect ONLY messages
// posted by that requester, before native completion can give the seated actor
// a walking path. All allocation/copying below runs on the game thread.
struct NativePathRequest {
    HavokCharacter* probe;Ogre::Vector3 worldShift;hand actor;cars::RigPoint goal;DWORD started;
    unsigned id,sample;bool done,success,sampled;std::vector<cars::RigPoint> points;
    NativePathRequest():probe(0),started(0),id(0),sample(0),done(false),success(false),sampled(false){}
};
std::map<unsigned,NativePathRequest> nativePaths;
std::map<HavokCharacterMessage*,unsigned> nativePathMessages;
unsigned nativePathSerial=0,nativePosting=0;
CRITICAL_SECTION nativeMessageMutex;
struct NativeMessageLock{NativeMessageLock(){EnterCriticalSection(&nativeMessageMutex);}~NativeMessageLock(){LeaveCriticalSection(&nativeMessageMutex);}};
void (*originalPathPost)(NavMesh*,HavokCharacterMessage*,bool)=0;
void (*originalPathSet)(HavokCharacter*,HavokCharacterMessage*)=0;
bool nativePathHooks=false;
double nativeCharacterRouteRadius(Character* actor) {
    HavokCharacter* movement=actor && actor->movement?actor->movement->havokCharacter:0;
    const double radius=movement?movement->radius:1;
    return _finite(radius) && radius>0 && radius<=8?radius:1;
}
void vehiclePathPost(NavMesh* nav,HavokCharacterMessage* msg,bool hi) {
    // Allocator reuse also retires a tombstone whose actor vanished before
    // dispatch. The engine retains ownership of the message throughout.
    {NativeMessageLock lock;nativePathMessages.erase(msg);
    if(GetCurrentThreadId()==vehicleThreadId && nativePosting)nativePathMessages[msg]=nativePosting;}
    originalPathPost(nav,msg,hi);
}
void cancelNativePath(unsigned& id) {
    std::map<unsigned,NativePathRequest>::iterator i=nativePaths.find(id);
    if(i!=nativePaths.end()){delete i->second.probe;nativePaths.erase(i);}id=0;
    // Leave message IDs as tombstones until completion or allocator reuse.
}
void vehiclePathSet(HavokCharacter* actual,HavokCharacterMessage* msg) {
    unsigned id=0;bool ours=false;
    {NativeMessageLock lock;std::map<HavokCharacterMessage*,unsigned>::iterator m=nativePathMessages.find(msg);
    if(m!=nativePathMessages.end()){ours=true;id=m->second;nativePathMessages.erase(m);}}
    if(!ours){originalPathSet(actual,msg);return;}
    Lock lock;
    std::map<unsigned,NativePathRequest>::iterator it=nativePaths.find(id);
    if(it==nativePaths.end())return; // Cancelled request must not move its actor.
    NativePathRequest& r=it->second;r.done=true;
    if(!r.probe || !cars::sameHandle(actual->handle,r.actor) || r.worldShift.squaredDistance(ou->navmesh->getWorldShift())>.01f){r.sampled=true;return;}
    originalPathSet(r.probe,msg);
    if(r.probe->getPathState()!=HavokCharacter::COMPLETE || r.probe->currentPath.m_size<0 || r.probe->currentPath.m_size>8192){
        std::ostringstream log;log<<"Project Cars: native route unavailable / state="<<int(r.probe->getPathState());DebugLog(log.str());r.sampled=true;return;
    }
    r.points.push_back(point(r.probe->getPosition()));

}
unsigned requestNativePath(Character* actor,const cars::RigPoint& from,const cars::RigPoint& goal,double radius) {
    if(!nativePathHooks || !actor || !actor->isValid() || !ou->navmesh || nativePaths.size()>=16)return 0;
    {NativeMessageLock lock;if(nativePathMessages.size()>512)return 0;}
    Ogre::Vector3 start;unsigned face=0;
    // Exterior dry navmesh only; keep the native asynchronous planner bounded
    // to the current streamed leg, not an unbounded synchronous world search.
    if(ou->navmesh->getClosestPoint(vector(from),32,0,false,start,face)<=0 || ou->navmesh->isInterior(face))return 0;
    unsigned id=++nativePathSerial;if(!id)id=++nativePathSerial;
    NativePathRequest& r=nativePaths[id];r.id=id;r.actor=actor->getHandle();r.goal=goal;r.started=GetTickCount();r.worldShift=ou->navmesh->getWorldShift();
    try {
        r.probe=new HavokCharacter(r.actor,start,float(radius),1,0);
        r.probe->setPosition(start,face);
        nativePosting=id;r.probe->requestPath(vector(goal),1);nativePosting=0;
    }catch(...){nativePosting=0;cancelNativePath(id);return 0;}
    return id;
}
// 0 pending, 1 complete, -1 unavailable/failed. Timeouts never block movement.
int pollNativePath(unsigned id,std::vector<cars::RigPoint>& points) {
    std::map<unsigned,NativePathRequest>::iterator i=nativePaths.find(id);
    if(i==nativePaths.end())return -1;
    NativePathRequest& r=i->second;
    if(r.worldShift.squaredDistance(ou->navmesh->getWorldShift())>.01f)return -1;
    if(r.done && !r.sampled) {
        // Bound funnel sampling per frame as well as the native async request.
        // The exported sampler handles native units and floating world origin.
        for(unsigned n=0;n<12 && !r.sampled;++n){
            cars::RigPoint p=point(r.probe->calculateFuturePosition(float(++r.sample*24)));
            if(!cars::validPoint(p)){r.sampled=true;break;}
            if((p-r.points.back()).length()>.25)r.points.push_back(p);
            if(cars::roadHorizontalDistance(p,r.goal)<5 && fabs(p.y-r.goal.y)<12){r.success=true;r.sampled=true;if(r.points.size()<2)r.points.push_back(r.goal);}
            if(r.sample>=256)r.sampled=true;
        }
    }
    if(r.sampled){points=r.points;return r.success?1:-1;}
    return GetTickCount()-i->second.started>2500?-1:0;
}
void clearNativePaths(){while(!nativePaths.empty()){unsigned id=nativePaths.begin()->first;cancelNativePath(id);}}
bool installNativeVehiclePaths() {
    InitializeCriticalSection(&nativeMessageMutex);
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&NavMesh::postMessage),vehiclePathPost,&originalPathPost)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&HavokCharacter::setPath),vehiclePathSet,&originalPathSet)!=KenshiLib::SUCCESS)return false;
    nativePathHooks=true;return true;
}
