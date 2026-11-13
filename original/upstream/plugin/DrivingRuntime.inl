unsigned passengerCount(Building*);
bool attachedOccupant(Building*,const hand&);
#include "NativeRoadNetwork.inl"
// Included in FittingRuntime's anonymous namespace. Uses the same lifetime lock
// and native fitted inventories, so fitting, saves and driving share one state.
struct DriveState {
    cars::TreadEmitter treads[4];
    cars::FrameDents dents;cars::ChassisBody body;cars::RouteDriver driver;cars::VehicleRoute planner;
    cars::RoadJourney journey;
    unsigned nativeRequest,nativeSegment;bool nativeFailed;std::vector<cars::RigPoint> nativeGuide;
    cars::RoadReport routeReport;
    cars::DrivingOrderStream orderStream;
    cars::ManualDriver manual;
    double diagnosticTime;DWORD planningStarted;double trackVisualStamp;bool trackVisualFailed;
    DWORD streamRetryAt;unsigned streamAttempts;bool headlightsOn;
    bool explicitRebuild,reportedStreamPin;bool flooded;
    bool reportedOwnerPin;bool streamNeeded;
    std::vector<cars::RigPoint> destinations;hand occupant,recovery,restoreOccupant;Ogre::Vector3 recoveryEntry;cars::DrivingInput input;
    double stamp,accumulator,recoveryTime,blockedTime,recoveryElapsed,audioImpact;DWORD physicsSeen;cars::DriveFault fault;DWORD retryAt,streamAt,faultAt;unsigned automaticRetries;double healthyPhysics;bool restartAfterRetry;bool retryPhysics,rebuildAttempted,streamPending;bool initialized,planning,navDetached,recoveryAnimated,recoveryCancelled,finalLeg,physicsReported,riderMaskReported,connecting,plannedFinal,exitRequested;
    DWORD diagnosticWallStart;
    DriveState():nativeRequest(0),nativeSegment(1),nativeFailed(false),flooded(false),diagnosticTime(0),planningStarted(0),trackVisualStamp(-1),trackVisualFailed(false),streamRetryAt(0),streamAttempts(0),headlightsOn(false),explicitRebuild(false),reportedStreamPin(false),reportedOwnerPin(false),streamNeeded(false),stamp(-1),accumulator(0),recoveryTime(0),blockedTime(0),recoveryElapsed(0),audioImpact(0),physicsSeen(GetTickCount()),fault(cars::NoDriveFault),retryAt(0),streamAt(0),faultAt(0),automaticRetries(0),healthyPhysics(0),restartAfterRetry(false),retryPhysics(false),rebuildAttempted(false),streamPending(false),initialized(false),planning(false),navDetached(false),recoveryAnimated(false),recoveryCancelled(false),finalLeg(true),physicsReported(false),riderMaskReported(false),connecting(false),plannedFinal(true),exitRequested(false),diagnosticWallStart(0){driver.pace=.75;}
};
std::map<hand,DriveState> drives;
bool passengerOrder(Character*);
bool occupiedVehicleStream(Building* b) {
    Lock lock;if(!ou || !ou->initialized || ou->gameResetting || cars::vehicleWorldChanging())return false;
    std::map<hand,DriveState>::const_iterator i=drives.find(b->getHandle());
    return passengerCount(b)>0 || (i!=drives.end() && (!i->second.occupant.isNull() || !i->second.restoreOccupant.isNull()));
}
bool protectOccupiedPhysical(Building* b) {
    if(!occupiedVehicleStream(b))return false;
    DriveState& s=drives[b->getHandle()];if(s.explicitRebuild)return false;
    if(!s.reportedStreamPin){s.reportedStreamPin=true;DebugLog("Project Cars: prevented zone teardown of occupied vehicle / "+b->getHandle().toString());}
    return true;
}
bool vehicleZoneUpdate(ZoneMap* zone) {
    // Routine streaming inlines the manager's teardown inside ZoneMap::update.
    // Renew the owner BEFORE its countdown, content and handle teardown; keep
    // calling the full native update so content and thread-state still advance.
    ZoneManager* manager=ou?ou->zoneMgr:0;
    if(manager && zone && zone->mapContent && ou->initialized && !ou->gameResetting && !cars::vehicleWorldChanging()) {
        Lock lock;
        for(std::map<hand,DriveState>::iterator i=drives.begin();i!=drives.end();++i) {
            // The handle identifies the owning save zone independently of the
            // car's new world position. Do not rewrite persistent handle IDs.
            if(manager->getZoneMapFromBuildingHandle(i->first)!=zone)continue;
            DriveState& s=i->second;Building* b=i->first.getBuilding();
            if(!b || !b->isValid())continue;
            ZoneMap* current=manager->getZoneMap(b->getPosition());
            const bool nearby=current && current!=zone && current->isLoadedMT() &&
                (current->isActivationType(ACTIVATION_CAMERA) || current->isActivationType(ACTIVATION_PLAYER_CHARACTER));
            if(s.occupant.isNull() && s.restoreOccupant.isNull() && !passengerCount(b) && !nearby)continue;
            // Refresh only this already-loaded owner, never a trail of zones.
            // Use the town retention channel, leaving real camera/player
            // activity distinguishable for parked cars (no mutual pin cycle).
            // Preserve any longer native town reservation. No render/PhysX
            // objects are mutated from this main-thread ZoneMap callback.
            if(zone->activatedCountdown[ACTIVATION_TOWN]<5.0f)zone->_activate(0,ACTIVATION_TOWN,5.0f);
            if(!s.reportedOwnerPin) {
                s.reportedOwnerPin=true;std::ostringstream detail;
                detail<<"Project Cars: retaining vehicle owner zone "<<zone->coordinates.x<<","<<zone->coordinates.y
                    <<" / "<<i->first.toString()<<" / current="<<(current?current->coordinates.x:-1)<<","<<(current?current->coordinates.y:-1);
                DebugLog(detail.str());
            }
            break;
        }
    }
    return originalZoneUpdate(zone);
}
bool vehicleObjectDestroy(GameWorld* world,RootObject* object,bool unloaded,const char* reason) {
    if(!unloaded && object && object->getDataType()==BUILDING)cars::forgetOilBuilding(static_cast<Building*>(object));
    if(!unloaded && object && object->getDataType()==BUILDING && frame(static_cast<Building*>(object)) && GetCurrentThreadId()==vehicleThreadId && !cars::vehicleWorldChanging())removeVehicleTurret(static_cast<Building*>(object));
    if(unloaded && object && object->getDataType()==BUILDING){Building* carrier=turretVehicle(static_cast<Building*>(object));if(carrier && occupiedVehicleStream(carrier))return false;}
    if(object && object->getDataType()==BUILDING && GetCurrentThreadId()==vehicleThreadId && !cars::vehicleWorldChanging()) {
        Building* carrier=turretVehicle(static_cast<Building*>(object));
        if(carrier)endVehicleGunner(carrier);
    }
    // Observe before the handle manager invalidates the object. Real removal
    // and world resets always pass through; never veto a destructor halfway.
    if(object && object->getDataType()==BUILDING && frame(static_cast<Building*>(object))) {
        Lock lock;std::map<hand,DriveState>::const_iterator i=drives.find(object->getHandle());
        std::ostringstream detail;detail<<"Project Cars: native vehicle removal / "<<object->getHandle().toString()
            <<" / unloaded="<<unloaded<<" / changingWorld="<<cars::vehicleWorldChanging()
            <<" / occupied="<<(i!=drives.end() && (!i->second.occupant.isNull() || !i->second.restoreOccupant.isNull()))
            <<" / reason="<<(reason?reason:"unspecified");DebugLog(detail.str());
    }
    if(unloaded && object && object->getDataType()==BUILDING && frame(static_cast<Building*>(object)) &&
       occupiedVehicleStream(static_cast<Building*>(object)))return false;
    if(unloaded && object && !cars::vehicleWorldChanging() && world->initialized && !world->gameResetting && passengerOrder(object->getHandle().getCharacter()))return false;
    return originalObjectDestroy(world,object,unloaded,reason);
}
hand manualVehicle,previousCameraFollow;
cars::ManualDrivingKeys manualKeys;
bool manualCameraOwned=false;
void endManual(bool restoreCamera=true);
const char* drivingKey="ProjectCars dynamics v1";
void (*originalPhysicsUpdate)(PhysicsActual*)=0;
cars::RigPoint point(const Ogre::Vector3& p){return cars::RigPoint(p.x,p.y,p.z);}
Ogre::Vector3 vector(const cars::RigPoint& p){return Ogre::Vector3(float(p.x),float(p.y),float(p.z));}
Ogre::Quaternion quaternion(const cars::BodyRotation& r){return Ogre::Quaternion(float(r.w),float(r.x),float(r.y),float(r.z));}
#include "NativeVehiclePath.inl"
cars::RigPoint massCenter(Building* b){return (motorcycleFrame(b)?cars::RigPoint(-.25,3.6,0):xsmallFrame(b)?cars::RigPoint(-.3,4,0):cars::RigPoint(-1.0007885,10,-.0329885))*b->getScale();}
DriveState& driveState(Building* b) {
    DriveState& s=drives[b->getHandle()];if(!s.initialized) {
        s.body.orientation=cars::BodyRotation(b->rot.w,b->rot.x,b->rot.y,b->rot.z);
        s.body.position=point(b->getPosition())+s.body.orientation.rotate(massCenter(b));s.initialized=true;
    }return s;
}
bool driveReady(Building* b) {
    const char* required[]={"engine","tank","seat_driver","steering"};
    for(unsigned n=0;n<4;++n)if(!fitted(b,required[n]))return false;
    unsigned wheels=(fitted(b,"wheel_fl")?1:0)|(fitted(b,"wheel_fr")?2:0)|(fitted(b,"wheel_rl")?4:0)|(fitted(b,"wheel_rr")?8:0);
    unsigned tracks=(fitted(b,"track_l")?1:0)|(fitted(b,"track_r")?2:0);
    return motorcycleFrame(b)?wheels==5 && tracks==0:cars::runningGearReady(wheels,tracks);
}
cars::ChassisSpec driveSpec(Building* b) {
    double mass=(motorcycleFrame(b)?85:xsmallFrame(b)?125:300)+(lookup(b)?lookup(b)->getTotalWeight():0)+(b->getInventory()?b->getInventory()->getTotalWeight():0)+drives[b->getHandle()].body.fuel*.75;
    if(drives[b->getHandle()].occupant.isValid())mass+=75;
    mass+=75*passengerCount(b);
    cars::ChassisSpec spec=cars::lightChassisSpec(b->getScale(),mass);spec.wheelMask=0;
    const char* wheels[]={"wheel_fl","wheel_fr","wheel_rl","wheel_rr"};
    for(unsigned n=0;n<4;++n)if(fitted(b,wheels[n]))spec.wheelMask|=1u<<n;
    if(motorcycleFrame(b))return cars::motorcycleChassisSpec(b->getScale(),mass,spec.wheelMask);
    if(xsmallFrame(b))return cars::xsmallChassisSpec(b->getScale(),mass,spec.wheelMask);
    // With no tires the lower frame rests on the ground instead of receiving
    // suspension forces from absent wheels. Local frame origin is at its base.
    unsigned tracks=(fitted(b,"track_l")?1:0)|(fitted(b,"track_r")?2:0);
    // Incomplete running gear rests on the frame. A single belt must not lift
    // one side on six powered suspension contacts while the other has none.
    // This also keeps sequential wheel installation balanced. Gravity and the
    // frame's ordinary ground contacts still apply on slopes and ledges.
    cars::configureAssemblyGear(spec,spec.wheelMask,tracks,b->getScale());
    return spec;
}
void brakeDrive(DriveState&);
#include "VehicleDamage.inl"
#include "VehicleOccupants.inl"
#include "VehicleTurret.inl"
#include "VehicleTrails.inl"
class NativeRoad : public cars::VehicleEnvironment {
    typedef cars::ScopedQueryBatch<cars::CollisionQueryMask,DoorStuff*> GateBatch;
    GateBatch* activeGateBatch;
    hand ignored,ignoredRider;cars::RoadReport& report;
    hand lastGate;NxShape* lastShape;unsigned lastGroup;bool lastSweep,surfaceProbe;
    bool skipOpenGate(const physHit& hit,GateBatch& gates,bool sweep) {
        lastGate.setNull();lastShape=hit.shape;lastGroup=hit.group();lastSweep=sweep;
        Building* object=hit.hitObject.getBuilding();if(!object || !object->isValid())return false;
        DoorStuff* door=object->doorStuff();
        if(!door && object->isGate())door=object->getDoor();
        if(!door || !door->isValid() || !door->parent || !door->parent->isValid() || !door->parent->isGate())return false;
        lastGate=object->getHandle();
        if(gates.contains(door))return false;
        cars::CollisionQueryMask& leaves=gates.exclusions;
        // Native sphereTrace supplies an owner but DOES NOT fill physHit::shape.
        // Resolve the actual leaf even when the owner is the gateway. Its live
        // state also covers gates already open on load; no transition cache.
        const bool open=door->state==DOORSTATE_OPEN || door->isBroken();
        const unsigned before=leaves.shapeCount();
        bool acquired=false;
        if(open) {
            acquired=leaves.excludeBuilding(door) && leaves.excludeNonSolid(door->parent) && leaves.excludeNonSolid(door);
            cars::NativeShapeInfo info;
            if(hit.shape && info.read(hit.shape) && !info.solid())acquired=leaves.excludeShape(hit.shape) && acquired;
        }
        if(!open || acquired)gates.remember(door);
        const bool retry=acquired && leaves.shapeCount()>before;
        // Never exclude the parent gateway: posts and overhead stay solid.
        // A repeated parent hit after masking the leaf is real structure.
        static unsigned reports[4]={0,0,0,0};const unsigned kind=(sweep?2:0)+(retry?1:0);
        if(reports[kind]++<6){std::ostringstream log;log<<"Project Cars: gate query / "<<(sweep?"sweep":"ray")<<" / owner="<<object->getName()<<" / leaf="<<door->getName()<<" / state="<<int(door->state)<<" / amount="<<door->doorOpenAmount<<" / group="<<hit.group()<<" / leafShapes="<<leaves.shapeCount()<<" / retry="<<retry;DebugLog(log.str());}
        return retry;
    }
public:
    // Hold gate exclusions across steering and one fixed physics step only.
    // Destruction also restores on exceptional exits, before game callbacks.
    class Batch {
        NativeRoad& road;GateBatch gates;GateBatch* previous;
    public:
        Batch(NativeRoad& r):road(r),previous(r.activeGateBatch){road.activeGateBatch=&gates;}
        bool restore(){road.activeGateBatch=previous;return gates.restore();}
        ~Batch(){restore();}
    private:Batch(const Batch&);Batch& operator=(const Batch&);
    };
    NativeRoad(Building* b,cars::RoadReport& diagnostics,const hand& rider=hand()):activeGateBatch(0),ignored(b->getHandle()),ignoredRider(rider),report(diagnostics),lastShape(0),lastGroup(0),lastSweep(false),surfaceProbe(false){}
    void collisionContact(const cars::SurfaceHit& hit) {
        static unsigned count=0;if(lastGate.isNull() || count>=24)return;
        ++count;cars::NativeShapeInfo info;info.read(lastShape,true);
        std::ostringstream log;log<<"Project Cars: gate chassis contact / owner="<<lastGate.toString()<<" / sweep="<<lastSweep<<" / shape="<<info.name<<" / flags="<<info.flags<<" / group="<<lastGroup<<" / hit="<<vector(hit.point)<<" / normal="<<vector(hit.normal);DebugLog(log.str());
    }
    bool ray(const cars::RigPoint& a,const cars::RigPoint& b,cars::SurfaceHit& out) {
        if(!cars::validPoint(a)||!cars::validPoint(b)||(b-a).length()<=1e-5)return false;
        Ogre::Vector3 from=vector(a),end=vector(b);if((end-from).squaredLength()<=1e-10f)return false;
        const Ogre::Vector3 dir=(end-from).normalisedCopy();
        GateBatch local;GateBatch& openLeaves=activeGateBatch?*activeGateBatch:local;
        for(unsigned n=0;n<12;++n) {
            // traceNoActors includes actor shapes but omits their owner/shape
            // from physHit. trace preserves that identity; both rays are infinite.
            physHit hit(true,true,-99);UtilityT::trace(hit,from,end-from,0x7fffffff);
            if(!hit.hit || hit.traceWasAborted())return false;
            cars::SurfaceHit segmentHit;
            if(!cars::boundedVehicleRayHit(point(from),b,point(hit.position),point(hit.normal),segmentHit))return false;
            if(skipOpenGate(hit,openLeaves,false))continue;
            if(!cars::sameHandle(hit.hitObject,ignored) && (ignoredRider.isNull() || !cars::sameHandle(hit.hitObject,ignoredRider))) {
                if(!cars::boundedVehicleRayHit(a,b,point(hit.position),point(hit.normal),out))return false;
                out.treadSurface=hit.hitObject.isNull(); // Terrain/rocks; never stamp an actor or building.
                if(!surfaceProbe && !report.hasObstacle){report.obstacleOwner=hit.hitObject.isNull()?"unowned shape":hit.hitObject.toString();report.obstacleGroup=hit.group();}
                return true;
            }
            from=hit.position+dir*.2f;if((end-from).dotProduct(dir)<=0)return false;
        }return false;
    }
    bool sweep(const cars::RigPoint& from,const cars::RigPoint& to,double radius,cars::SurfaceHit& out) {
        GateBatch local;GateBatch& openLeaves=activeGateBatch?*activeGateBatch:local;
        for(unsigned n=0;n<12;++n) {
        physHit hit(true,true,-99);UtilityT::sphereTrace(hit,vector(from),vector(to-from),float(radius),0x7fffffff,true);
        if(!hit.hit || hit.traceWasAborted() || cars::sameHandle(hit.hitObject,ignored))return false;
        if(skipOpenGate(hit,openLeaves,true))continue;
        if(!cars::vehicleSweepDistance(from,to,hit.distance,out.distance))return false;
        out.point=point(hit.position);out.normal=point(hit.normal);return cars::validPoint(out.point)&&cars::validPoint(out.normal)&&out.normal.length()>.5;
        }return false;
    }
    double terrainHeight(const cars::RigPoint& p){
        const double terrain=UtilityT::getTerrainHeightFromRenderer(vector(p),true,0);
        surfaceProbe=true;const double result=cars::vehicleRoadSurface(*this,p,terrain);surfaceProbe=false;return result;
    }
    bool startingGround(const cars::RigPoint& p,double radius,double& h) {
        // The chassis already occupies its origin. Check terrain here, then let
        // the outgoing edges test clearance instead of stranding it on a slope.
        ++report.samples;h=terrainHeight(p);
        if(!cars::boardingTerrainAvailable(h))return report.record(cars::RoadTerrainMissing,p);
        return report.record(h>100?cars::RoadClear:cars::RoadWater,p);
    }
    bool driveable(const cars::RigPoint& p,double radius,double& ground){return cars::surveyVehicleGround(*this,p,radius,ground,report);}
    bool nativePassage(const cars::RigPoint& a,const cars::RigPoint& b,double radius){return cars::vehicleNativePassage(*this,a,b,radius,report);}
};
void brakeDrive(DriveState& s){cancelNativePath(s.nativeRequest);s.nativeGuide.clear();s.driver.stop();s.planning=false;s.connecting=false;s.journey.reset();s.destinations.clear();s.orderStream.reset();s.exitRequested=false;}
void stopDrive(DriveState& s){brakeDrive(s);s.body.ignition=false;}
#include "VehicleRecovery.inl"
void disableDrive(DriveState& s,cars::DriveFault fault=cars::StateDriveFault) {
    if(drives.count(manualVehicle) && &drives[manualVehicle]==&s)endManual();
    // A rejected native update leaves the physical car stationary. Clear the
    // predicted motion too, so the driver can still exit a faulted vehicle.
    stopDrive(s);s.body.velocity=cars::RigPoint();s.body.angular=cars::RigPoint();
    s.input=cars::DrivingInput();s.input.brake=1;s.fault=fault;s.retryPhysics=false;s.restartAfterRetry=false;s.faultAt=GetTickCount();s.healthyPhysics=0;
}
bool driverOrder(Character* actor,TaskType type,const Ogre::Vector3& destination,bool append) {
    if(!actor || issuingSeatOrder)return false;
    for(std::map<hand,DriveState>::iterator i=drives.begin();i!=drives.end();++i)if(cars::sameHandle(i->second.recovery,actor->getHandle()))i->second.recoveryCancelled=true;
    for(std::map<hand,DriveState>::iterator i=drives.begin();i!=drives.end();++i)if(cars::sameHandle(i->second.occupant,actor->getHandle())) {
        DriveState& s=i->second;Building* b=i->first.getBuilding();if(!b)return false;
        if(s.streamPending || (ou && ou->isLoadingFromASaveGame()))return true;
        if(type==MOVE_CUS_ORDERED) {
            if(cars::sameHandle(manualVehicle,i->first))return true;
            if(!cars::validPoint(point(destination)))return true;
            if(!s.body.ignition){messages[i->first]="Start the engine before giving a driving destination.";return true;}
            // Native held-right-click orders repeat each frame. Preserve an
            // identical pending/active route rather than restarting its A*.
            const bool menu=ou->player->contextMenu.isVisible();
            if(!s.orderStream.acceptMouse(point(destination),append,GetTickCount()*.001,key && key->mRight,key && key->mRUp,menu))return true;
            if(!s.destinations.empty() && (append||s.destinations.size()==1) && cars::sameDrivingDestination(s.destinations.back(),point(destination)))return true;
            s.exitRequested=false;
            if(!append){cancelNativePath(s.nativeRequest);s.nativeGuide.clear();s.destinations.clear();s.driver.stop();s.planning=false;s.connecting=false;s.journey.reset();}
            if(s.destinations.size()<32)s.destinations.push_back(point(destination));
            messages[i->first]="Driving destination queued.";return true;
        }
        if(type==HOLD_POSITION){if(cars::sameHandle(manualVehicle,i->first))endManual();brakeDrive(s);messages[i->first]="Braking.";return true;}
        // A new non-driving task releases the seat through normal boarding cleanup.
        if(cars::sameHandle(manualVehicle,i->first))endManual();
        stopDrive(s);s.occupant.setNull();return false;
    }return false;
}
void releaseDriver(Building* b) {
    if(cars::sameHandle(manualVehicle,b->getHandle()))endManual();
    std::map<hand,DriveState>::iterator i=drives.find(b->getHandle());if(i==drives.end())return;
    stopDrive(i->second);i->second.occupant.setNull();i->second.restoreOccupant.setNull();i->second.streamPending=false;i->second.retryPhysics=false;i->second.restartAfterRetry=false;clearDriverGrips();
}
void destroyDrive(Building* b){forgetModuleCollisions(b);ramContacts.erase(b->getHandle());if(GetCurrentThreadId()==vehicleThreadId && ou && !ou->gameResetting && !cars::vehicleWorldChanging()){for(unsigned n=0;n<5;++n)releasePassenger(b,personMounts[n],true);}passengers.erase(b->getHandle());if(GetCurrentThreadId()==vehicleThreadId && cars::sameHandle(manualVehicle,b->getHandle()))endManual();drives.erase(b->getHandle());}
void suspendDrive(Building* b) {
    std::map<hand,DriveState>::iterator i=drives.find(b->getHandle());if(i==drives.end())return;
    DriveState& s=i->second;
    // Physical teardown is not logical destruction. No actor, camera or GUI
    // mutation here: this hook may be called by the loading thread.
    if(!s.streamPending){s.streamAt=GetTickCount();DebugLog("Project Cars: physical rebuild suspended / "+b->getHandle().toString());}
    if(!s.streamPending){s.streamAttempts=0;s.streamRetryAt=GetTickCount();s.streamNeeded=false;}
    s.streamPending=true;if(!s.occupant.isNull())s.restoreOccupant=s.occupant;
    s.navDetached=false;s.physicsReported=false;s.riderMaskReported=false;
    s.stamp=-1;s.accumulator=0;s.physicsSeen=GetTickCount();
}
void colliderFailure(Building* b,const cars::CollisionPoseReport& report,const char* phase) {
    std::ostringstream log;log<<"Project Cars: collider validation failed / "<<b->getHandle().toString()<<" / "<<phase
        <<" / reason="<<report.reason<<" / part="<<report.part<<" / actor="<<report.actor<<" / count="<<report.count
        <<" / distance="<<report.distance<<" / norm="<<report.norm<<" / actorPosition="<<report.position.x<<','<<report.position.y<<','<<report.position.z
        <<" / frame="<<b->getPosition();ErrorLog(log.str());
}
bool drivingMenu(Building* b,Character* actor,cars::VehicleClick action) {
    DriveState& s=driveState(b);
    if(action==cars::EndManualDriveVehicle) {
        if(!cars::sameHandle(manualVehicle,b->getHandle()))return false;
        endManual();messages[b->getHandle()]="Manual driving ended. Right-click terrain to drive.";return true;
    }
    if(action==cars::ManualDriveVehicle) {
        Character* driver=s.occupant.getCharacter();
        if(!driver || !interactionActor(b,driver,true) || !fitted(b,"steering") || s.fault || s.exitRequested || !ou->player->camera)return false;
        endManual();brakeDrive(s);s.manual=cars::ManualDriver();s.manual.reversing=s.body.velocity.dot(s.body.forward())<-2;
        manualVehicle=b->getHandle();manualKeys=cars::ManualDrivingKeys();
        previousCameraFollow=ou->player->camera->getFollowObject();manualCameraOwned=true;
        if(!ou->player->isObjectSelected(driver))ou->player->selectObject(driver,true);
        ou->player->camera->followObject(s.occupant);
        messages[b->getHandle()]="Manual driving. Escape returns to normal driving.";return true;
    }
    if(action==cars::RightVehicle)return beginRecovery(b,actor);

    const bool sharedSwitch=action==cars::IgnitionVehicle || action==cars::HeadlightsVehicle;
    if(!cars::sameHandle(s.occupant,actor->getHandle()) && !(sharedSwitch && attachedOccupant(b,actor->getHandle()))){messages[b->getHandle()]="Enter the driver seat first.";return false;}
    if(action==cars::HeadlightsVehicle) {
        if(!fitted(b,"headlights")){messages[b->getHandle()]="Install headlights first.";return false;}
        s.headlightsOn=!s.headlightsOn;b->notifyChange();messages[b->getHandle()]=s.headlightsOn?"Headlights on.":"Headlights off.";return true;
    }
    if(action==cars::IgnitionVehicle) {
        if(s.body.ignition){stopDrive(s);messages[b->getHandle()]="Engine stopped.";return true;}
        const char* reason=s.flooded?"Vehicle flooded. Recover it to dry ground.":!driveReady(b)?"Install the required driving components.":s.body.fuel<=0?"Fuel tank is empty.":s.body.overturned()?"Vehicle overturned. Get out and use Right Vehicle.":jobs.count(b->getHandle())||(workPlans.count(b->getHandle())&&workPlans[b->getHandle()].queue.running)?"Pause fitting work before starting the engine.":0;
        if(reason){messages[b->getHandle()]=reason;DebugLog("Project Cars: engine start blocked / "+b->getHandle().toString()+" / "+reason);return false;}
        if(s.fault) {
            if(!cars::driveFaultCanRetry(s.fault)){messages[b->getHandle()]="Vehicle simulation state is invalid. Reload the save before driving.";return false;}
            if(!s.retryPhysics){s.retryPhysics=true;s.automaticRetries=0;s.rebuildAttempted=false;s.retryAt=GetTickCount();}
            s.restartAfterRetry=true;
            messages[b->getHandle()]="Rechecking vehicle physics before restarting the engine.";return true;
        }
        s.body.ignition=true;s.physicsSeen=GetTickCount();messages[b->getHandle()]="Engine running. Right-click terrain to drive.";return true;
    }
    if(action==cars::StopVehicle){if(cars::sameHandle(manualVehicle,b->getHandle()))endManual();brakeDrive(s);messages[b->getHandle()]="Braking.";return true;}
    if(action==cars::PaceVehicle){s.driver.pace=s.driver.pace<.5?.75:s.driver.pace<.9?1:.35;messages[b->getHandle()]=s.driver.pace<.5?"Driving pace: cautious":s.driver.pace<.9?"Driving pace: cruise":"Driving pace: fast";return true;}
    return false;
}
void endManual(bool restoreCamera) {
    std::map<hand,DriveState>::iterator i=drives.find(manualVehicle);
    if(i!=drives.end()){brakeDrive(i->second);i->second.input=cars::DrivingInput();i->second.input.brake=1;}
    manualVehicle.setNull();manualKeys=cars::ManualDrivingKeys();
    if(manualCameraOwned && ou && ou->player && ou->player->camera) {
        CameraClass* camera=ou->player->camera;
        if(restoreCamera && !ou->gameResetting && !ou->isLoadingFromASaveGame() && previousCameraFollow.isValid())camera->followObject(previousCameraFollow);
        else camera->stopFollowing();
    }
    previousCameraFollow.setNull();manualCameraOwned=false;
}
std::string fuelService(Building* b,Character* actor,bool drain) {
    DriveState& s=driveState(b);
    if(!eligible(b,actor))return "Stand beside the vehicle and leave combat to service fuel.";
    if(s.body.ignition)return "Switch off the engine to service fuel.";
    if(s.body.speed()>1)return "Wait for the vehicle to settle before servicing fuel.";
    if(!fitted(b,"tank"))return "Install a fuel tank first.";
    Inventory* inv=actor->getInventory();if(!inv)return "Inventory unavailable.";
    Item* engine=fittedItem(b,"engine");
    const bool diesel=engine && engine->getGameData()->stringID==cars::dieselEngineItemId;
    if(!drain && !fitted(b,"engine"))return "Install a working engine first.";
    GameData* def=ou->gamedata.getData(diesel?cars::vanillaFuelItemId:"144-Project Cars.mod",ITEM);if(!def)return "Fuel definition unavailable.";
    // One can holds 5 litres. Never consume a partly fitting can or mint fuel on removal.
    if(drain) {
        if(s.body.fuel<5){s.body.fuel=0;return "Emptied the remaining fuel.";}
        if(!inv->hasRoomForItem(def))return "Make room for the returned fuel.";
        int before=inv->countItems(def);Item* item=ou->theFactory->createItem(def,hand(),0,0,-1,0);
        if(!item)return "Could not create fuel can.";
        if(!inv->addItem(item,1,false,true)||inv->countItems(def)!=before+1)return "Could not return fuel can.";
        s.body.fuel-=5;return diesel?"Returned one Fuel (5 litres).":"Returned 5 litres of gasoline.";
    }
    Item* can=inv->getItem(def);int before=inv->countItems(def);if(!can||before<1)return diesel?"Carry ordinary Fuel for this diesel engine.":"Carry Gasoline for this engine.";
    const double litres=cars::fuelItemLitres(can->chargesLeft);
    if(litres<=0)return "The fuel container is empty or invalid.";
    if(s.body.fuel+litres>40)return "Tank cannot hold this container's remaining fuel.";
    if(!inv->removeItemAutoDestroy(can,1)||inv->countItems(def)!=before-1)return "Fuel transfer failed.";
    s.body.fuel+=litres;std::ostringstream status;status<<"Added "<<litres<<(diesel?" litres from Fuel.":" litres of gasoline.");return status.str();
}
void saveDrive(Building* b,GameData* data) {if(data && drives.count(b->getHandle())){data->sdata[drivingKey]=drives[b->getHandle()].body.save();data->sdata["ProjectCars dents v1"]=drives[b->getHandle()].dents.save();data->idata["ProjectCars flooded v1"]=drives[b->getHandle()].flooded?1:0;data->storeHandle(drives[b->getHandle()].occupant,"ProjectCars driver v1",false);data->sdata["ProjectCars headlights v1"]=drives[b->getHandle()].headlightsOn?"1":"0";}}
void loadDrive(Building* b,GameData* data) {
    if(!data)return;
    // Older fitting saves may predate serialized dynamics. Register them for
    // bounded physical restoration using their native saved building pose.
    if(!data->sdata.count(drivingKey)){driveState(b);return;}DriveState s;
    if(s.body.load(data->sdata[drivingKey])){if(data->sdata.count("ProjectCars dents v1"))s.dents.load(data->sdata["ProjectCars dents v1"]);s.flooded=data->idata["ProjectCars flooded v1"]!=0;s.initialized=true;s.headlightsOn=data->sdata.count("ProjectCars headlights v1") && data->sdata["ProjectCars headlights v1"]=="1";data->getHandle(s.restoreOccupant,"ProjectCars driver v1");drives[b->getHandle()]=s;}else ErrorLog("Project Cars: invalid saved dynamics rejected");
}
void physicsUpdate(PhysicsActual* physics) {
    cars::VehicleFaultScope trace("vehicle physics update");
    {cars::VehicleFaultScope trace("original native physics update");originalPhysicsUpdate(physics);}
    if(GetCurrentThreadId()!=vehicleThreadId || !ou||!ou->initialized||ou->gameResetting||ou->isLoadingFromASaveGame()||cars::vehicleWorldChanging()||!ou->player)return;
    Lock lock;advanceRecoveryPhysics();applyVehicleHits();
    const double now=ou->getTimeStamp_inGameHours().getTotalHours();unsigned active=0,planningBudget=8,roadBudget=2048;
    nativeRoadNetwork.advance();
    advanceVehicleTrails(now);
    static hand cursor;
    std::map<hand,DriveState>::iterator scan=drives.upper_bound(cursor);
    const unsigned scanLimit=std::min(128u,static_cast<unsigned>(drives.size()));
    for(unsigned scanned=0;scanned<scanLimit && active<32;++scanned) {
        if(scan==drives.end())scan=drives.begin();
        std::map<hand,DriveState>::iterator i=scan++;cursor=i->first;
        DriveState& s=i->second;Building* b=i->first.getBuilding();
        s.physicsSeen=GetTickCount();
        if(!b||!b->isValid()){s.stamp=now;continue;}
        // Hold the selected, unoccupied vehicle while its recovery ghost is
        // being placed. Cancel preserves its pose and momentum; no catch-up.
        if(cars::sameHandle(recoveryPlacement.vehicle,i->first)){s.stamp=now;s.accumulator=0;continue;}
        if(ou->isPaused()){s.stamp=now;s.faultAt=GetTickCount();if(s.retryPhysics)s.retryAt=GetTickCount();continue;}
        if(!s.retryPhysics && cars::automaticPhysicsRetry(s.fault,s.automaticRetries,(GetTickCount()-s.faultAt)*.001)) {
            ++s.automaticRetries;s.retryPhysics=true;s.restartAfterRetry=false;s.rebuildAttempted=false;s.retryAt=GetTickCount();
            messages[i->first]="Restoring vehicle physics. Engine remains off.";
        }
        if(s.retryPhysics && GetTickCount()-s.retryAt>15000){s.retryPhysics=false;s.restartAfterRetry=false;s.faultAt=GetTickCount();messages[i->first]="Vehicle physics is still unavailable. Try R again after the area loads.";}
        if(s.retryPhysics && s.fault==cars::ColliderDriveFault && !s.rebuildAttempted) {
            // Explicit R recovery gets fresh native actors at the current frame
            // anchor. Never reuse or repair an actor with an invalid pose.
            s.rebuildAttempted=true;s.explicitRebuild=true;
            try {b->_NV_destroyPhysical();b->_NV_createPhysical();}catch(...){s.explicitRebuild=false;throw;}
            s.explicitRebuild=false;s.stamp=now;continue;
        }
        if(!b->isPhysical()||!b->entitiesLoaded||!b->getRootNode()||s.streamPending||(s.fault&&!s.retryPhysics)){s.stamp=now;continue;}
        double elapsed=s.stamp<0?0:cars::vehicleElapsed(now-s.stamp,ou->getLengthOfHourInRealSeconds());s.stamp=now;
        if(ou->isPaused()||elapsed<=0||elapsed>.5)continue;
        try {
            // Components enable propulsion, not gravity. Parked, incomplete and
            // stalled vehicles still get suspension/contact and falling steps.
            if(s.body.position.y<100){if(!s.flooded)messages[i->first]="Vehicle flooded. Use Correct Vehicle to recover it.";s.flooded=true;}
            if(!driveReady(b))stopDrive(s);
            if(s.flooded)stopDrive(s);
            ++active;syncModuleCollisions(b,physics);
            cars::CollisionPoseReport collision;
            if(!cars::moveNativeCollision(b,b->getPosition(),b->rot,false,&collision)){colliderFailure(b,collision,"acquire");disableDrive(s,cars::ColliderDriveFault);messages[i->first]="Vehicle collision synchronization stopped. Press R to rebuild and retry.";continue;}
            if(!s.physicsReported){s.physicsReported=true;std::ostringstream log;log<<"Project Cars: collider acquired / "<<i->first.toString()<<" / scale="<<b->getScale()<<" / world="<<b->getPosition();DebugLog(log.str());}
            Character* actor=s.occupant.getCharacter();
            if(!s.occupant.isNull() && (!actor || !actor->isValid() || !actor->getMovement())){suspendDrive(b);continue;}
            if(actor && (actor->isDead() || !actor->getMedical() || actor->getMedical()->isUnconcious())){releaseDriver(b);actor=0;}
            cars::CollisionQueryMask queryMask(b,actor);NativeRoad road(b,s.routeReport);cars::ChassisSpec spec=driveSpec(b);
            if(passengers.count(b->getHandle()))for(std::map<std::string,MountedPerson>::const_iterator p=passengers[b->getHandle()].begin();p!=passengers[b->getHandle()].end();++p)queryMask.excludeRider(p->second.person.getCharacter());
            if(!queryMask.ready()){disableDrive(s,cars::QueryDriveFault);messages[i->first]="Vehicle collision filtering could not be prepared. Driving stopped.";ErrorLog("Project Cars: collision query mask validation failed / "+i->first.toString());continue;}
            if(s.retryPhysics) {
                if(!queryMask.restore() || !cars::driveRetryValidated(s.fault,s.body.valid(),true,queryMask.ready())){disableDrive(s,cars::QueryDriveFault);messages[i->first]="Physics retry failed validation. Engine remains stopped.";continue;}
                s.fault=cars::NoDriveFault;s.retryPhysics=false;s.stamp=now;s.accumulator=0;
                const bool serviceIdle=!jobs.count(i->first) && !(workPlans.count(i->first) && workPlans[i->first].queue.running);
                s.body.ignition=cars::retryStartsEngine(s.restartAfterRetry,actor!=0,driveReady(b)&&serviceIdle,s.body.fuel,s.body.overturned());s.restartAfterRetry=false;
                messages[i->first]=s.body.ignition?"Vehicle physics restored. Engine running.":"Vehicle physics restored. Engine off.";
                DebugLog("Project Cars: physics retry validated / "+i->first.toString());continue;
            }
            if(actor&&queryMask.riderShapes()&&!s.riderMaskReported){s.riderMaskReported=true;std::ostringstream log;log<<"Project Cars: vehicle query excludes seated driver / "<<i->first.toString()<<" / rider="<<s.occupant.toString()<<" / rider shapes="<<queryMask.riderShapes()<<" / total shapes="<<queryMask.shapeCount();DebugLog(log.str());}
            if(!actor)s.riderMaskReported=false;
            if(!s.navDetached && (s.body.ignition || s.body.speed()>.5)) {ou->navmesh->destroy(b);s.navDetached=true;}
            if(!s.destinations.empty() && !s.journey.started()) {
                const bool longTrip=cars::RoadJourney::eligible(s.body.position,s.destinations.front());
                if(longTrip)nativeRoadNetwork.request();
                if(!longTrip || nativeRoadNetwork.ready())s.journey.begin(nativeRoadNetwork.graph,s.body.position,s.destinations.front());
                else messages[i->first]="Finding roads to the destination.";
            }
            if(s.journey.started() && !s.journey.ready() && roadBudget) {
                unsigned budget=std::min(512u,roadBudget);roadBudget-=budget;
                if(s.journey.advance(nativeRoadNetwork.graph,budget)) {
                    std::ostringstream log;log<<"Project Cars: journey planned / roads="<<s.journey.usingRoads()<<" / guides="<<s.journey.route().size();DebugLog(log.str());
                }
            }
            if(!s.planning && !s.destinations.empty() && s.journey.ready() && (s.driver.stopped || (!s.finalLeg && s.driver.remaining(s.body.position)<spec.maxSpeed*4))) {
                s.connecting=!s.driver.stopped;
                cars::RigPoint start=s.connecting?s.driver.points.back():s.body.position;
                double legLength=(spec.halfWidth+3)*100;
                if(s.journey.usingRoads())legLength=std::min(512.0,legLength);
                cars::RigPoint goal=s.journey.target(start,legLength,s.plannedFinal);
                s.routeReport=cars::RoadReport();s.planner.begin(start,goal,spec.halfWidth+3);
                cancelNativePath(s.nativeRequest);s.nativeGuide.clear();s.nativeSegment=1;
                // Kenshi supplies the ordinary character corridor. Our own
                // passage checks retain the car's full width and steer it.
                s.nativeRequest=requestNativePath(actor,start,goal,nativeCharacterRouteRadius(actor));s.nativeFailed=!s.nativeRequest;
                s.planning=true;s.planningStarted=GetTickCount();
                if(!s.connecting)messages[i->first]="Finding a vehicle route.";
            }
            const unsigned routeBudget=s.planning?std::min(4u,planningBudget):0;planningBudget-=routeBudget;
            bool nativeReady=false;
            if(s.planning && !s.nativeFailed && routeBudget) {
                int status=pollNativePath(s.nativeRequest,s.nativeGuide);
                if(status<0){s.nativeFailed=true;cancelNativePath(s.nativeRequest);}
                else if(status>0) {
                    for(unsigned n=0;n<routeBudget && s.nativeSegment<s.nativeGuide.size();++n,++s.nativeSegment)
                        if(!road.nativePassage(s.nativeGuide[s.nativeSegment-1],s.nativeGuide[s.nativeSegment],spec.halfWidth+3)){s.nativeFailed=true;break;}
                    nativeReady=!s.nativeFailed && s.nativeSegment==s.nativeGuide.size();
                    if(nativeReady || s.nativeFailed)cancelNativePath(s.nativeRequest);
                }
            }
            if(s.planning && routeBudget && (nativeReady || (s.nativeFailed && s.planner.advance(road,s.planner.checkingDirect()?routeBudget*4:routeBudget)))) {
                s.planning=false;if(nativeReady || s.planner.success()){
                    const std::vector<cars::RigPoint>& route=nativeReady?s.nativeGuide:s.planner.result();
                    s.journey.acceptTarget();
                    if(s.connecting)s.driver.extend(route);else s.driver.route(route);
                    s.finalLeg=s.plannedFinal;s.connecting=false;messages[i->first]=s.journey.usingRoads()?"Following roads":"Driving";
                    std::ostringstream log;log<<"Project Cars: vehicle route ready / "<<i->first.toString()<<" / native character path="<<nativeReady<<" / waypoints="<<s.driver.points.size()<<" / samples="<<s.routeReport.samples<<" / direct="<<s.planner.usedDirect()<<" / elapsed ms="<<(GetTickCount()-s.planningStarted);DebugLog(log.str());
                }
                else {
                    std::ostringstream log;log<<"Project Cars: vehicle route failed / "<<i->first.toString()<<" / start="<<vector(s.body.position)<<" / goal="<<vector(s.destinations.front())<<" / samples="<<s.routeReport.samples;
                    for(unsigned r=1;r<cars::RoadRejectionCount;++r)log<<" / "<<cars::roadRejectionName(static_cast<cars::RoadRejection>(r))<<"="<<s.routeReport.counts[r];
                    log<<" / first="<<cars::roadRejectionName(s.routeReport.first)<<" at "<<vector(s.routeReport.firstPoint);
                    if(s.routeReport.hasObstacle)log<<" / ray="<<vector(s.routeReport.obstacleFrom)<<" to "<<vector(s.routeReport.obstacleTo)<<" / actual-hit="<<vector(s.routeReport.obstacleHit.point)<<" / distance="<<s.routeReport.obstacleHit.distance<<" / normal="<<vector(s.routeReport.obstacleHit.normal)<<" / owner="<<s.routeReport.obstacleOwner<<" / group="<<s.routeReport.obstacleGroup;
                    DebugLog(log.str());
                    s.driver.stop();s.connecting=false;
                    if(s.journey.usingRoads()){s.journey.fallback();messages[i->first]="Road approach blocked; finding a terrain route.";}
                    else {s.destinations.clear();s.journey.reset();messages[i->first]="No clear vehicle route to that destination.";}
                }
            }
            if(actor&&actor->getStats())s.driver.handling=cars::vehicleSkillLevel(skills[actor->getStats()].handlingXp);
            const cars::ChassisBody previous=s.body;const bool wasManeuverBlocked=s.driver.maneuverBlocked;
            s.accumulator=std::min(.5,s.accumulator+elapsed);
            while(s.accumulator>=1.0/120) {
                NativeRoad::Batch gateBatch(road);
                s.input=cars::sameHandle(manualVehicle,i->first)?s.manual.control(s.body,spec,manualKeys,s.driver.handling,1.0/120):s.driver.control(s.body,spec,1.0/120,&road);
                const bool stepped=s.body.step(1.0/120,s.input,spec,road);
                const bool gatesRestored=gateBatch.restore();
                if(!stepped || !gatesRestored){s.body=previous;disableDrive(s,gatesRestored?cars::StateDriveFault:cars::QueryDriveFault);messages[i->first]="Vehicle simulation stopped after invalid physics state.";ErrorLog("Project Cars: dynamics or gate query restoration failed / "+i->first.toString());break;}
                s.audioImpact=std::max(s.audioImpact,s.body.impact);
                if(s.body.impact>45){cars::RigPoint local=s.body.orientation.inverse().rotate(s.body.impactPoint-s.body.position)+massCenter(b);s.dents.hit(local*(1/b->getScale()),s.body.impact,s.body.damage);}
                if(s.body.impact>25)damageMount(b,struckMount(b,s.body.impactPoint,s.body.position,3*b->getScale()),cars::impactPartDamage(s.body.impact,spec.mass));
                s.accumulator-=1.0/120;
            }
            if(s.driver.maneuverBlocked)messages[i->first]="Waiting for room to maneuver. Choose a wider approach.";
            else if(wasManeuverBlocked)messages[i->first]="Driving";
            // Only a latched arrival completes an order. Pauses, obstacles and
            // intermediate streamed route sections retain their destination.
            if(cars::completeVehicleDestination(s.driver,s.destinations,s.planning,s.finalLeg)){s.journey.reset();if(s.destinations.empty())messages[i->first]="Arrived.";DebugLog("Project Cars: vehicle destination completed / "+i->first.toString());}
            if(s.input.throttle>.3 && s.body.speed()<2 && (!s.input.pivot || fabs(s.body.angular.dot(s.body.up()))<.05) && !s.driver.stopped)s.blockedTime+=elapsed;else s.blockedTime=0;
            if(s.blockedTime>4){s.driver.stop();s.planning=false;s.connecting=false;s.journey.fallback();s.blockedTime=0;messages[i->first]="Vehicle obstructed; replanning.";}
            const bool manual=cars::sameHandle(manualVehicle,i->first);
            if(actor && s.body.ignition && (!s.destinations.empty() || (manual && (s.input.throttle>0 || s.body.speed()>2)))) {
                if(s.diagnosticTime==0)s.diagnosticWallStart=GetTickCount();
                s.diagnosticTime+=elapsed;
            } else s.diagnosticTime=0;
            if(s.diagnosticTime>=5) {
                const double simulationSeconds=s.diagnosticTime;
                const double wallSeconds=(GetTickCount()-s.diagnosticWallStart)*.001;
                s.diagnosticTime=0;std::ostringstream log;log<<"Project Cars: driving sample / "<<i->first.toString()
                    <<" / mode="<<(manual?"manual":"route")<<" / simSeconds="<<simulationSeconds<<" / realSeconds="<<wallSeconds<<" / kmh="<<s.body.speed()*.36<<" / throttle="<<s.input.throttle<<" / brake="<<s.input.brake<<" / steer="<<s.input.steer<<" / reverse="<<s.driver.reversing<<" / maneuver="<<s.driver.maneuvering<<" / forward arc="<<s.driver.forwardTurn<<" / mass="<<spec.mass<<" / grade="<<s.body.forward().y<<" / longitudinal="<<s.body.velocity.dot(s.body.forward())<<" / arriving="<<s.driver.arriving<<" / remaining="<<s.driver.remaining(s.body.position)<<" / contacts="<<s.body.contacts<<" / waypoint="<<s.driver.next<<" of "<<s.driver.points.size()<<" / planning="<<s.planning<<" / suppressed orders="<<s.orderStream.suppressed;DebugLog(log.str());
            }
            if(!s.recovery.isNull()) {
                Character* worker=s.recovery.getCharacter();
                s.recoveryElapsed+=elapsed;
                if(s.recoveryCancelled||!worker||!interactionActor(b,worker)||s.body.speed()>2||s.recoveryElapsed>60) {
                    if(worker&&s.recoveryAnimated&&worker->getAnimationClass())worker->getAnimationClass()->stopAction(workAnimation);
                    s.recovery.setNull();s.recoveryAnimated=false;messages[i->first]="Vehicle recovery stopped.";
                }else if(worker->getPosition().squaredDistance(s.recoveryEntry)<=9) {
                    if(!s.recoveryAnimated && worker->getAnimationClass()){worker->getAnimationClass()->playAction(workAnimation,1,1,false);s.recoveryAnimated=true;}
                    s.recoveryTime+=elapsed*cars::mechanicsRate(skills[worker->getStats()].mechanicsXp);if(s.recoveryTime>=12) {
                    if(worker->getAnimationClass())worker->getAnimationClass()->stopAction(workAnimation);
                    s.recoveryAnimated=false;
                    if(s.body.rightVehicle(spec,road)){messages[i->first]="Vehicle righted.";s.recovery.setNull();}
                    else {messages[i->first]="Not enough clear ground to right the vehicle.";s.recovery.setNull();}
                }}
            }
            // Finish borrowed shape mutations before native pose/notification
            // calls can update the vehicle or rebuild the rider's selection hull.
            if(!queryMask.restore()){s.body=previous;disableDrive(s,cars::QueryDriveFault);messages[i->first]="Vehicle collision filtering could not be restored. Driving stopped.";ErrorLog("Project Cars: collision query group restore failed / "+i->first.toString());continue;}
            if(s.fault)continue;
            vehicleRamming(b,previous,spec);
            Ogre::Vector3 origin=vector(s.body.position-s.body.orientation.rotate(massCenter(b)));Ogre::Quaternion rotation=quaternion(s.body.orientation);
            if(!cars::moveNativeCollision(b,origin,rotation,true,&collision)) {
                s.body=previous;disableDrive(s,cars::ColliderDriveFault);messages[i->first]="Vehicle physics could not acquire its collider. See Project Cars log.";
                colliderFailure(b,collision,"apply");continue;
            }
            recordVehicleTrails(b,s,spec);
            Ogre::SceneNode* node=b->getRootNode();const Ogre::Vector3 shift=node->_getDerivedPosition()-b->getPosition();
            b->pos=origin;b->rot=rotation;node->setPosition(node->getParentSceneNode()?node->getParentSceneNode()->convertWorldToLocalPosition(origin+shift):origin+shift);
            node->setOrientation(node->getParentSceneNode()?node->getParentSceneNode()->_getDerivedOrientation().Inverse()*rotation:rotation);
            node->_getDerivedPositionUpdated();node->_getDerivedOrientationUpdated();b->recalculateWorldAABB();b->notifyChange();b->instanceID.notifyChange();
            if(ou->zoneMgr)ou->zoneMgr->buildingGrid.update(b);
            moveVehicleTurret(b);updatePassengers(b);
            s.healthyPhysics+=elapsed;if(s.healthyPhysics>=10)s.automaticRetries=0;
            if(actor && actor->getMovement()) {
                actor->getMovement()->_setPositionDirectionAndTeleport(rigWorld(b,driverAnchor(b)),b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y));
                if(s.body.contacts && s.body.speed()>10 && actor->getStats())skills[actor->getStats()].handlingXp=cars::validVehicleXp(skills[actor->getStats()].handlingXp+elapsed*4);
            }
            if(spec.trackMask && !s.trackVisualFailed && (s.trackVisualStamp<0 || s.body.clock<s.trackVisualStamp || s.body.clock-s.trackVisualStamp>=1.0/30)){
                try {animateVehicleTracks(node,s.body,spec,b->getScale());}
                catch(const std::exception& e){s.trackVisualFailed=true;ErrorLog(std::string("Project Cars: track animation stopped for this vehicle: ")+e.what());}
                s.trackVisualStamp=s.body.clock;
            }
            for(unsigned child=0;child<node->numChildren();++child)if(node->getChild(child)->getName()==std::string(nodePrefix)+"Steering_Wheel")
                // Axis points toward the driver: positive is counterclockwise
                // as viewed from the seat, matching a left steering command.
                node->getChild(child)->setOrientation(Ogre::Quaternion(Ogre::Radian(float(s.body.steering*(motorcycleFrame(b)?1:2.5))),motorcycleFrame(b)?Ogre::Vector3::UNIT_Y:vector(cars::steeringAxis())));
            for(unsigned child=0;child<node->numChildren();++child) {
                Ogre::Node* wheel=node->getChild(child);const std::string& name=wheel->getName();
                const char* hubs[]={"FL","FR","RL","RR"};
                for(unsigned hub=0;hub<4;++hub) {
                    const std::string armor=std::string(nodePrefix)+"Armor_wheel_"+hubs[hub]+"_";
                    if(name!=std::string(nodePrefix)+"Wheel_"+hubs[hub] && name.compare(0,armor.size(),armor)!=0)continue;
                    wheel->setOrientation(Ogre::Quaternion(Ogre::Radian(float(hub<2?s.body.steering:0)),Ogre::Vector3::UNIT_Y)*Ogre::Quaternion(Ogre::Radian(float(-s.body.wheelSpin)),Ogre::Vector3::UNIT_Z));
                    // All wheel armor exports share the owning wheel's pivot.
                    Ogre::Vector3 hubPosition=wheel->getPosition();
                    if(motorcycleFrame(b) && hub==0) {
                        // Keep the front hub aligned with the fork as it turns
                        // around the headstock. Suspension owns the vertical offset.
                        hubPosition.x=6.4f+3.9f*float(cos(s.body.steering));
                        hubPosition.z=-3.9f*float(sin(s.body.steering));
                    }
                    if(!xsmallFrame(b) || spec.wheelMask)
                        hubPosition.y=float((massCenter(b).y+spec.mountHeight-spec.travel+s.body.compression[hub])/b->getScale());
                    wheel->setPosition(hubPosition);break;
                }
            }
        }catch(const std::exception& e){disableDrive(s);messages[i->first]="Vehicle physics stopped. You can leave the seat.";ErrorLog(std::string("Project Cars: driving disabled for vehicle: ")+e.what());}
    }
    if(driverPoseReady)prepareDriverGrips();
}
