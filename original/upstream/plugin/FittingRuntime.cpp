#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "OilProduction.h"
#include <kenshi/Building/DoorStuff.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/Building/Building.h>
#include <kenshi/gui/InventoryGUI.h>
#include <kenshi/Building/UseableStuff.h>
#include <kenshi/Building/TurretBuilding.h>
class GunClassPersonal; // Missing forward declaration in the installed SDK.
#include <kenshi/GunClass.h>
#include <kenshi/combat/RangedCombatClass.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/Character.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/InputHandler.h>
#include <kenshi/CameraClass.h>
#include <kenshi/util/UtilityT.h>
#include <kenshi/Inventory.h>
#include <kenshi/CharStats.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/Damages.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/Animation/AnimationClass.h>
#include <kenshi/Appearance.h>
#include <kenshi/CharMovement.h>
#include "compat/NavMesh.h"
#include "compat/HavokCharacter.h"
#include <kenshi/ZoneManager.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/OptionsHolder.h>
#include <stdexcept>
#include <sstream>
#include <ogre/OgreSceneManager.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreMeshManager.h>
#include <ogre/OgreMesh.h>
#include <ogre/OgreSubMesh.h>
#include <ogre/OgreHardwareBufferManager.h>
#include "../src/VehicleDents.h"
#include <ogre/OgreLight.h>
#include <ogre/OgreSubEntity.h>
#include <ogre/OgreResourceGroupManager.h>
#include <ogre/OgreOldSkeletonInstance.h>
#include <ogre/OgreOldBone.h>
#include <ogre/OgreAnimation.h>
#include <ogre/OgreManualObject.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreTechnique.h>
#include <ogre/OgrePass.h>
#include <ogre/OgreHighLevelGpuProgramManager.h>
#include <ogre/OgreHighLevelGpuProgram.h>
#include "FittingRuntime.h"
#include "NativeRuntime.h"
#include "VehicleWorkshop.h"
#include "../src/WorkshopData.generated.h"
#include "../src/XSmallVisuals.generated.h"
#include "../src/MotorcycleVisuals.generated.h"
#include "../src/VehicleSkills.h"
#include "../src/VehiclePicking.generated.h"
#include "../src/FittingQueue.h"
#include "../src/VehicleRig.h"
#include "../src/VehicleDynamics.h"
#include "../src/VehicleBounds.h"
#include "../src/TrackMotion.h"
#include "../src/RoadNetwork.h"
#include "compat/NativeRoadSnapshot.h"
#include "../src/VehicleDamage.h"
#include "../src/VehicleTrails.h"
#include "../src/VehicleRoad.h"
#include "../src/VehicleQueries.h"
#include "VehiclePhysicsBridge.h"
#include "VehicleAudio.h"
#include "compat/HandleIdentity.h"
#include "compat/NativeQueryBuffer.h"
#include "VehicleFaultTrace.h"
#include <cstddef>
// Steam 1.0.65 constructor and query ABI, checked against the installed image.
typedef char VehicleCharacterGridOffset[offsetof(ZoneManager,characterGrid)==0x10?1:-1];
typedef char VehicleAnatomyOffset[offsetof(MedicalSystem,anatomy)==0x190?1:-1];
typedef char VehicleQueryCountOffset[offsetof(lektor<RootObject*>,count)==8?1:-1];
typedef char VehicleQueryDataOffset[offsetof(lektor<RootObject*>,stuff)==0x10?1:-1];
typedef char VehicleTurretGunOffset[offsetof(TurretBuilding,gunClass)==0x440?1:-1];
typedef char VehicleGunnerGunOffset[offsetof(RangedCombatClass,gun)==0x28?1:-1];
typedef char VehicleTurretStat[STAT_TURRETS==11?1:-1];

namespace {
CRITICAL_SECTION mutex;
struct Lock {Lock(){EnterCriticalSection(&mutex);}~Lock(){LeaveCriticalSection(&mutex);}};
// These inventories are plugin-owned. The Building destructor releases them
// while their owner is still valid. Items remain native items, retaining state.
std::map<hand,Inventory*> stores;
// Lifetime is the native CharStats object, including init before a character has
// its final world handle. Only its own hooks or a live actor access this map.
std::map<CharStats*,cars::VehicleSkills> skills;
// Access the inventory directly. The inspected native section/transfer/save
// routines retain owner handles and section names without needing to replace
// Building::getInventory's tiny null-return stub. Leaving that getter untouched
// also keeps ordinary loot UI and resource hauling out of fitted components.
struct Job {cars::WorkshopRequest request;hand actor;Ogre::Vector3 origin;double stamp;cars::FittingWork work;bool animated,orderReplaced;Job():animated(false),orderReplaced(false){}};
std::map<hand,Job> jobs;
struct WorkPlan {cars::FittingQueue queue;hand actor;};
std::map<hand,WorkPlan> workPlans;
std::map<hand,std::string> messages;
struct SeatRequest {
    hand actor;Ogre::Vector3 entry;double stamp,enterTime;cars::BoardingProgress progress;
    bool acknowledged,approaching,interrupted;
    SeatRequest():stamp(0),enterTime(0),acknowledged(false),approaching(true),interrupted(false){}
    explicit SeatRequest(const hand& h):actor(h),stamp(0),enterTime(0),acknowledged(false),approaching(true),interrupted(false){}
};
std::map<hand,SeatRequest> seatRequests;
bool issuingSeatOrder=false;
bool driverOrder(Character*,TaskType,const Ogre::Vector3&,bool);
bool passengerOrder(Character*);
bool turretPart(const Building*);
bool fittedTurret(Building*);
Building* turretVehicle(Building*);
TurretBuilding* turretWeapon(Building*);
Ogre::Quaternion turretFacing(Building*);
Ogre::Vector3 gunnerAnchor(Building*);
void endGunner(Building*,Character*);
void endVehicleGunner(Building*);
void updateVehicleTurret(Building*);
void moveVehicleTurret(Building*);
void removeVehicleTurret(Building*);
void saveVehicleTurret(Building*,GameData*);
void loadVehicleTurret(Building*,GameData*);
void forgetVehicleTurret(Building*);
void pauseVehicleTurrets();
bool assignedGunner(const Building*,const hand&);
bool passengerSlotOccupied(Building*,const std::string&);
void savePassengers(Building*,GameData*);
void loadPassengers(Building*,GameData*);
void forgetModuleCollisions(Building*);
void releaseDriver(Building*);
void saveDrive(Building*,GameData*);
void loadDrive(Building*,GameData*);
void destroyDrive(Building*);
void suspendDrive(Building*);
bool occupiedVehicleStream(Building*);
bool protectOccupiedPhysical(Building*);
bool vehicleZoneUpdate(ZoneMap*);
bool vehicleObjectDestroy(GameWorld*,RootObject*,bool,const char*);
bool (*originalZoneUpdate)(ZoneMap*)=0;
bool (*originalObjectDestroy)(GameWorld*,RootObject*,bool,const char*)=0;
DWORD vehicleThreadId=0;
bool driverPoseReady=false;
void prepareDriverGrips();
void clearDriverGrips();
cars::VehicleAudio vehicleAudio;
void (*originalSkillSave)(CharStats*,GameData*)=0;
void (*originalSkillLoad)(CharStats*,GameData*)=0;
void (*originalSkillInit)(CharStats*,GameData*,MedicalSystem*,Character*)=0;
void (*originalSkillDestroy)(CharStats*)=0;
bool enabled=false;
const char* stateKey="ProjectCars fitted inventory v1";
const char* queueKey="ProjectCars work queue v1";
const char* nodePrefix="ProjectCars_Module_";
const char* mechanicsKey="ProjectCars Mechanics XP v1";
const char* handlingKey="ProjectCars Vehicle Handling XP v1";
const char* workAnimation="generic use"; // Base-game ANIMATION 43871-rebirth.mod.
void (*originalOrder)(OrdersReceiver*,TaskType,const hand&,const Ogre::Vector3&,bool,bool)=0;
void finishActivity(Job& job) {
    Character* actor=job.actor.getCharacter();if(!actor || !actor->isValid())return;
    AnimationClass* anim=actor->getAnimationClass();
    if(job.animated && anim && anim->getIsActivated())anim->stopAction(workAnimation);
    OrdersReceiver* orders=actor->getOrdersReciever();
    // Only release our temporary order. Never clear a replacement player order.
    if(!job.orderReplaced && orders) {
        if(orders->hasPlayerOrder(HOLD_POSITION))orders->clearOrders();
        else if(orders->getCurrentGoal().key()==HOLD_POSITION)orders->clearCurrentGoal(true);
    }
}
void playerOrder(OrdersReceiver* orders,TaskType type,const hand& target,const Ogre::Vector3& pos,bool clearOld,bool shift) {
    {Lock lock;if(passengerOrder(orders->me)||driverOrder(orders->me,type,pos,shift))return;}
    // Order hooks can originate outside the UI loop. Record interruption only;
    // animation cleanup and native job mutation belong to updateFittingWork.
    {Lock lock;for(std::map<hand,Job>::iterator i=jobs.begin();i!=jobs.end();++i) {
        if(orders->me && cars::sameHandle(i->second.actor,orders->me->getHandle()))i->second.orderReplaced=true;
    }}
    {Lock lock;if(orders->me)for(std::map<hand,SeatRequest>::iterator i=seatRequests.begin();i!=seatRequests.end();) {
        if(cars::sameHandle(i->second.actor,orders->me->getHandle()) && !issuingSeatOrder)i->second.interrupted=true;
        ++i;
    }}
    originalOrder(orders,type,target,pos,clearOld,shift);
}
void (*originalSave)(Building*,GameData*)=0;
void (*originalLoad)(Building*,GameData*,const std::string&)=0;
void (*originalDestroy)(Building*)=0;
void (*originalDestroyPhysical)(Building*)=0;
bool (*originalCanDismantle)(const Building*)=0;
void (*originalConfirmDismantle)(Building*,int)=0;
bool motorcycleFrame(const Building* b) {
    return b && b->getGameData() && b->getGameData()->stringID=="210-Project Cars.mod";
}
bool xsmallFrame(const Building* b) {
    if(motorcycleFrame(b))return true;
    return b && b->getGameData() && b->getGameData()->stringID=="200-Project Cars.mod";
}
bool frame(const Building* b) {
    return xsmallFrame(b) || (b && b->getGameData() && b->getGameData()->stringID=="10-Project Cars.mod");
}
const cars::WorkshopVisual* frameVisuals(const Building* b,size_t& count) {
    if(motorcycleFrame(b)){count=cars::motorcycleVisualCount;return cars::motorcycleVisuals;}
    if(xsmallFrame(b)){count=cars::xsmallVisualCount;return cars::xsmallVisuals;}
    count=cars::workshopVisualCount;
    return cars::workshopVisuals;
}
const cars::PickPlane (*compactPickPlanes(const Building* b))[6] {
    return motorcycleFrame(b)?cars::motorcyclePickPlanes:cars::xsmallPickPlanes;
}
const cars::RigPoint& driverAnchor(const Building* b,bool pelvis=false) {
    if(xsmallFrame(b))return pelvis?cars::xsmallPelvis:cars::xsmallSeat;
    return pelvis?cars::lightSeatPelvis.position:cars::lightSeat.position;
}
ZoneMap* (*originalBuildingZone)(Building*)=0;
void (*originalBuildingStream)(Building*)=0;
void vehicleLoadUnload(Building* b) {
    Building* carrier=turretVehicle(b);
    if(carrier && occupiedVehicleStream(carrier))return;
    Building::ConstructionState* state=frame(b)?b->getBuildState_ActualNonShared():0;
    if(state && state->isComplete && !state->isDismantled) {
        // An occupied car travels with its rider, including while the next zone
        // becomes ready. A transient unloaded zone is not a reason to remove it.
        if(occupiedVehicleStream(b))return;
        // Building's furniture/parent branch streams from its former owner.
        // RootObject instead asks our current-position zone hook. Intentional
        // replacement for completed cars; ordinary buildings call their original.
        b->RootObject::_NV_loadUnloadCheck();return;
    }
    originalBuildingStream(b);
}
ZoneMap* vehicleZone(Building* b) {
    // Native Building resolves its town first. A vehicle streams by its moving
    // world anchor instead; ordinary buildings retain the original behavior.
    if((frame(b)||turretPart(b)) && ou && ou->zoneMgr && cars::validPoint(cars::RigPoint(b->getPosition().x,b->getPosition().y,b->getPosition().z)))
        return ou->zoneMgr->getZoneMap(b->getPosition());
    return originalBuildingZone(b);
}

Ogre::Vector3 rigWorld(Building* b,const cars::RigPoint& local) {
    // Root-node conversions in this game returned origin-relative X/Z. Native
    // pathfinding/terrain/teleport APIs require the Building's world anchor.
    return b->getPosition()+b->rot*(Ogre::Vector3(static_cast<float>(local.x),static_cast<float>(local.y),static_cast<float>(local.z))*b->getScale());
}
bool fitted(Building* b,const std::string& slot);
bool fittedDesign(Building*,const std::string&,const std::string&);
bool seatInUse(Building* b) {
    UseableStuff* use=b?b->getUseableStuff():0;
    return b && ((use && !use->getOccupant().isNull()) || seatRequests.count(b->getHandle())!=0);
}
bool (*originalCouldOperate)(const UseableStuff*,const hand&)=0;
bool (*originalTryOperate)(UseableStuff*,const hand&)=0;
// VC10 x64 member aggregate returns receive their return buffer in RDX.
Ogre::Vector3* (*originalPositionMarker)(Building*,Ogre::Vector3*,const Ogre::Vector3&)=0;
Ogre::Vector3* (*originalDirectionMarker)(Building*,Ogre::Vector3*,const Ogre::Vector3&)=0;
Ogre::Quaternion* (*originalDirectionQuat)(Building*,Ogre::Quaternion*,const Ogre::Vector3&)=0;
InventoryGUI* (*originalShowBuildingInventory)(ForgottenGUI*,const hand&)=0;
bool authorizedSeat(const UseableStuff* use,const hand& actor) {
    Building* b=const_cast<UseableStuff*>(use);if(actor.isNull() || !fitted(b,"seat_driver"))return false;
    const std::map<hand,SeatRequest>::const_iterator i=seatRequests.find(b->getHandle());
    return (i!=seatRequests.end() && cars::sameHandle(i->second.actor,actor)) || cars::sameHandle(use->getOccupant(),actor);
}
bool couldOperate(const UseableStuff* use,const hand& actor) {
    Lock lock;return (turretPart(use)?assignedGunner(use,actor):(!frame(use) || authorizedSeat(use,actor))) && originalCouldOperate(use,actor);
}
bool tryOperate(UseableStuff* use,const hand& actor) {
    Lock lock;const bool allowed=turretPart(use)?assignedGunner(use,actor):(!frame(use) || authorizedSeat(use,actor));
    const bool result=allowed && originalTryOperate(use,actor);
    if(frame(use) && seatRequests.count(use->getHandle()) && !result) {
        std::ostringstream detail;detail<<"Project Cars: seat operation rejected / authorized="<<(allowed?"yes":"no")
            <<" actor="<<actor.toString()<<" reserved="<<seatRequests[use->getHandle()].actor.toString()
            <<" occupant="<<use->getOccupant().toString()<<" seatFitted="<<fitted(use,"seat_driver");
        DebugLog(detail.str());
    }
    return result;
}
Ogre::Vector3* positionMarker(Building* b,Ogre::Vector3* out,const Ogre::Vector3& from) {
    if(frame(b) && b->getRootNode()) {
        *out=rigWorld(b,driverAnchor(b));return out;
    }
    return originalPositionMarker(b,out,from);
}
Ogre::Vector3* directionMarker(Building* b,Ogre::Vector3* out,const Ogre::Vector3& from) {
    if(frame(b) && b->getRootNode()){*out=b->rot*Ogre::Vector3::UNIT_X;return out;}
    return originalDirectionMarker(b,out,from);
}
Ogre::Quaternion* directionQuat(Building* b,Ogre::Quaternion* out,const Ogre::Vector3& from) {
    if(frame(b) && b->getRootNode()){*out=b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y);return out;}
    return originalDirectionQuat(b,out,from);
}
InventoryGUI* showBuildingInventory(ForgottenGUI* gui,const hand& who) {
    Building* b=who.getBuilding();if(frame(b)){Lock lock;
        if(!fittedDesign(b,"cargo","131-Project Cars.mod"))return 0;
        std::map<hand,Job>::const_iterator j=jobs.find(who);if(j!=jobs.end() && j->second.request.slot=="cargo")return 0;
    }
    return originalShowBuildingInventory(gui,who);
}
void loadSkills(CharStats* stats,GameData* data) {
    if(!data)return;
    Lock lock;cars::VehicleSkills& s=skills[stats];
    if(data->fdata.count(mechanicsKey))s.mechanicsXp=cars::validVehicleXp(data->fdata.find(mechanicsKey)->second);
    if(data->fdata.count(handlingKey))s.handlingXp=cars::validVehicleXp(data->fdata.find(handlingKey)->second);
}
void skillSave(CharStats* stats,GameData* data) {
    originalSkillSave(stats,data);if(!stats->me || !data)return;
    Lock lock;cars::VehicleSkills& s=skills[stats];
    data->fdata[mechanicsKey]=static_cast<float>(s.mechanicsXp);data->fdata[handlingKey]=static_cast<float>(s.handlingXp);
}
void skillLoad(CharStats* stats,GameData* data){originalSkillLoad(stats,data);loadSkills(stats,data);}
void skillInit(CharStats* stats,GameData* data,MedicalSystem* med,Character* actor){
    {Lock lock;skills.erase(stats);} // Native init calls updateStats on the saved stats reference.
    originalSkillInit(stats,data,med,actor);loadSkills(stats,data);
}
void skillDestroy(CharStats* stats) {{Lock lock;skills.erase(stats);}originalSkillDestroy(stats);}
const cars::WorkshopSlot* mount(const std::string& id) {
    for(size_t i=0;i<cars::workshopMountCount;++i)if(id==cars::workshopMounts[i].id)return &cars::workshopMounts[i];return 0;
}
Inventory* lookup(const Building* b) {
    std::map<hand,Inventory*>::iterator i=stores.find(b->getHandle());return i==stores.end()?0:i->second;
}
void ensureMountSections(Inventory* inv,const Building* b) {
    for(size_t i=0;i<cars::workshopMountCount;++i) {
        const cars::WorkshopSlot& s=cars::workshopMounts[i];
        if(!cars::workshopMountSupported(xsmallFrame(b),s.id,motorcycleFrame(b)))continue;
        InventorySection* section=inv->getSection(s.id);
        if(!section)section=inv->initialiseNewSection(s.id,16,16,ATTACH_NONE,false,false,true,1);
        if(!section)throw std::runtime_error("Unable to create fitting slot");
        section->setStackingBonus(1,1.0f);
        section->clearVeryLimitedSlot();
        for(size_t j=0;j<cars::workshopItemCount;++j)if(cars::workshopAccepts(s,cars::workshopItems[j].id)) {
            GameData* item=ou->gamedata.getData(cars::workshopItems[j].id,ITEM);
            if(!item)throw std::runtime_error("Missing fitting item definition");
            section->addVeryLimitedSlot(item);
        }
    }
}
Inventory* ensure(Building* b) {
    Inventory* inv=lookup(b);if(inv)return inv;
    if(!enabled || !frame(b) || stores.size()>=1024)return 0;
    inv=new Inventory(b);
    try {
        ensureMountSections(inv,b);
        stores.insert(std::make_pair(b->getHandle(),inv));
    } catch (...) {delete inv;throw;}
    return inv;
}
Item* fittedItem(Building* b,const std::string& slot) {
    Inventory* inv=b?lookup(b):0;InventorySection* section=inv?inv->getSection(slot):0;
    return section?section->getItem():0;
}
bool fitted(Building* b,const std::string& slot) {Item* item=fittedItem(b,slot);return item && _finite(item->chargesLeft) && item->chargesLeft>0;}
bool fittedDesign(Building* b,const std::string& slot,const std::string& id) {Item* item=fittedItem(b,slot);return fitted(b,slot) && item->getGameData() && item->getGameData()->stringID==id;}
void detachUnsupportedPlates(Building* b) {
    Inventory* inv=lookup(b);if(!inv)return;
    for(size_t n=0;n<cars::workshopMountCount;++n) {
        const std::string slot=cars::workshopMounts[n].id,wheel=cars::wheelForPlate(slot);
        if(wheel.empty() || fitted(b,wheel))continue;
        Item* plate=fittedItem(b,slot);if(!plate)continue;
        // Native inventory drop keeps the same item/condition and uses the
        // owning vehicle's current world position. Never destroy or mint plates.
        inv->dropItem(plate);
        if(fittedItem(b,slot))ErrorLog("Project Cars: wheel plate drop pending / "+slot);
        else b->notifyChange();
    }
}

bool hasFittingsOrWork(const Building* b) {
    if(!frame(b))return false;
    Inventory* inv=lookup(b);Inventory* cargo=b->getInventory();
    return (inv && !inv->isEmpty()) || (cargo && !cargo->isEmpty()) || jobs.count(b->getHandle())!=0 || (workPlans.count(b->getHandle()) && !workPlans[b->getHandle()].queue.steps.empty());
}
bool canDismantle(const Building* b) {Lock lock;return !hasFittingsOrWork(b) && originalCanDismantle(b);}
void confirmDismantle(Building* b,int answer) {
    {Lock lock;if(hasFittingsOrWork(b)){messages[b->getHandle()]="Remove all fitted parts before dismantling the frame.";return;}}
    originalConfirmDismantle(b,answer);
}
void save(Building* b,GameData* state) {
    originalSave(b,state);
    cars::saveOilPool(b,state);
    Lock lock;Inventory* inv=lookup(b);
    if(!inv || !state)return;
    saveDrive(b,state);savePassengers(b,state);saveVehicleTurret(b,state);
    // Use native inventory and item serialization into this building's actual
    // save container. No external/global playthrough ledger or copied item IDs.
    GameData* saved=inv->serialise(state->sourceContainer,state);
    if(!saved)throw std::runtime_error("Project Cars fitting inventory could not be serialized");
    state->clearList(stateKey);state->addToList(stateKey,saved->stringID,1,0,0);
    std::map<hand,WorkPlan>::iterator plan=workPlans.find(b->getHandle());
    if(plan!=workPlans.end()) {
        cars::FittingQueue snapshot=plan->second.queue;
        std::map<hand,Job>::const_iterator job=jobs.find(b->getHandle());
        if(job!=jobs.end() && !snapshot.steps.empty())snapshot.steps.front().done=job->second.work.done;
        state->sdata[queueKey]=snapshot.serialize();
    }
    DebugLog("Project Cars: fitted inventory saved for "+b->getHandle().toString());
}
void load(Building* b,GameData* state,const std::string& door) {
    originalLoad(b,state,door);
    cars::loadOilPool(b,state);
    if(!frame(b) || !state || !state->listExistsAndNotEmpty(stateKey))return;
    Lock lock;Inventory* inv=ensure(b);if(!inv)return;
    GameData* saved=state->getFromListAsData(stateKey,0,state->sourceContainer,INVENTORY_STATE);
    if(!saved) {ErrorLog("Project Cars: fitted inventory reference missing from save");enabled=false;return;}
    inv->loadFrom(state->sourceContainer,saved);
    // Old saves predate new optional mounts. Fill only missing sections, keeping
    // existing installed items and their section identities intact.
    ensureMountSections(inv,b);
    loadDrive(b,state);loadPassengers(b,state);loadVehicleTurret(b,state);
    if(state->sdata.count(queueKey)) {
        cars::FittingQueue restored;
        if(restored.load(state->sdata[queueKey])) {
            bool valid=true;
            for(size_t n=0;n<restored.steps.size();++n) {const cars::FitStep& s=restored.steps[n];const cars::WorkshopSlot* slot=mount(s.slot);if(!slot || !cars::workshopMountSupported(xsmallFrame(b),s.slot,motorcycleFrame(b)) || !cars::workshopAccepts(*slot,s.item))valid=false;}
            if(valid){workPlans[b->getHandle()].queue=restored;messages[b->getHandle()]="Saved queue paused. Select a worker and Resume.";}
            else ErrorLog("Project Cars: invalid saved queue ignored");
        }else ErrorLog("Project Cars: malformed saved queue ignored");
    }
    DebugLog("Project Cars: fitted inventory restored for "+b->getHandle().toString());
}
void forgetDentedMesh(Building*);
void clearVisuals(Building* b) {
    Ogre::SceneNode* root=b->getRootNode();if(!root)return;
    Ogre::SceneManager* scene=root->getCreator();
    for(size_t n=root->numChildren();n>0;--n) {
        Ogre::SceneNode* child=static_cast<Ogre::SceneNode*>(root->getChild(n-1));
        if(child->getName().compare(0,strlen(nodePrefix),nodePrefix)!=0 && child->getName().compare(0,strlen("ProjectCars_Headlight_"),"ProjectCars_Headlight_")!=0)continue;
        while(child->numAttachedObjects()) {
            Ogre::MovableObject* obj=child->getAttachedObject(static_cast<size_t>(0));
            child->detachObject(obj);
            if(obj->getMovableType()=="Entity")scene->destroyEntity(static_cast<Ogre::Entity*>(obj));
            else if(obj->getMovableType()=="Light")scene->Ogre::SceneManager::destroyLight(static_cast<Ogre::Light*>(obj));
        }
        scene->destroySceneNode(child);
    }
    forgetDentedMesh(b);
}
void destroyPhysical(Building* b) {
    if(frame(b)) {
        Lock lock;
        // Zone/town unloading can call this directly, bypassing loadUnloadCheck.
        // Occupied cars remain physical. Actual destruction clears DriveState
        // first; world reset and our explicit collider rebuild also pass through.
        if(protectOccupiedPhysical(b))return;
        forgetModuleCollisions(b);
        suspendDrive(b);if(GetCurrentThreadId()==vehicleThreadId)clearVisuals(b);
    }
    originalDestroyPhysical(b);
}
void destroy(Building* b) {
    // Pending work resolves this handle again on the UI loop and cleans up its
    // actor there. Destruction may run on a loading thread.
    {Lock lock;forgetVehicleTurret(b);destroyDrive(b);workPlans.erase(b->getHandle());seatRequests.erase(b->getHandle());messages.erase(b->getHandle());std::map<hand,Inventory*>::iterator i=stores.find(b->getHandle());
        if(i!=stores.end()){Inventory* inv=i->second;stores.erase(i);delete inv;}}
    originalDestroy(b);
}
bool interactionActor(Building* b,Character* actor,bool allowCombat=false) {
    if(!enabled || !frame(b) || !b->isValid() || !actor || !actor->isValid() || !actor->isPlayerCharacter() || !actor->getStats() || !actor->getMedical() || actor->isDead() || actor->getMedical()->isUnconcious())return false;
    Building::ConstructionState* s=b->getBuildState_ActualNonShared();
    return s && s->isComplete && !s->isDismantled && b->getFaction()==actor->getFaction() && (allowCombat || !actor->isInCombatMode(true,true));
}
bool eligible(Building* b,Character* actor) {return interactionActor(b,actor) && b->getPosition().squaredDistance(actor->getPosition())<=900;}
bool boardingEntry(Building* b,Character* actor,Ogre::Vector3& entry) {
    const cars::RigMarker* doors=xsmallFrame(b)?cars::xsmallDoors:cars::lightDoors;
    if(!b || !b->getRootNode() || !actor)return false;
    // Enable native collision/zone terrain fallback. A renderer miss is -99,
    // not NaN; mixing that sentinel with a successful sample rejected dry land.
    const float vehicleGround=UtilityT::getTerrainHeightFromRenderer(b->getPosition(),true,0);
    double best=1e20;bool found=false;
    for(int side=0;side<2;++side) {
        Ogre::Vector3 p=rigWorld(b,doors[side].position);
        float height=UtilityT::getTerrainHeightFromRenderer(p,true,0);
        // Use the same terrain sample for height and water. getPositionInWater
        // makes a separate fast query and mistakes its own -99 miss for water.
        const bool allowed=cars::boardingGroundAllowed(height,vehicleGround);
        std::ostringstream detail;detail<<"Project Cars: boarding marker="<<doors[side].name<<" point="<<p.x<<','<<p.y<<','<<p.z
            <<" terrain="<<height<<" frameTerrain="<<vehicleGround<<" actorY="<<actor->getPosition().y<<" accepted="<<allowed;
        DebugLog(detail.str());
        if(!allowed)continue;
        p.y=height;double distance=p.squaredDistance(actor->getPosition());
        if(distance<best){best=distance;entry=p;found=true;}
    }
    return found;
}
void finishBoarding(Building* b,SeatRequest& request,bool keepNewOrder) {
    if(b)releaseDriver(b);
    UseableStuff* seat=b && b->isValid()?b->getUseableStuff():0;
    if(seat && cars::sameHandle(seat->getOccupant(),request.actor))seat->stopOperating(request.actor);
    Character* actor=request.actor.getCharacter();if(!actor || !actor->isValid())return;
    OrdersReceiver* orders=actor->getOrdersReciever();
    if(!keepNewOrder && orders){orders->clearOrders();orders->clearCurrentGoal(true);}
    if(!request.approaching && actor->getMovement()) {
        Ogre::Vector3 exit;
        // A door remembered before a drive can be kilometres away. Use a
        // current, validated door only, and never pull back a departing actor.
        if(b && actor->getPosition().squaredDistance(b->getPosition())<=6400 && boardingEntry(b,actor,exit))
            actor->getMovement()->_setPositionAndTeleport(exit,0);
    }
    if(actor->getAnimationClass())actor->getAnimationClass()->stopAction("sitting chair");
    if(!keepNewOrder && actor->getMovement())actor->getMovement()->halt();
}
std::map<std::string,std::string> contents(Inventory* inv) {
    std::map<std::string,std::string> out;if(!inv)return out;
    for(size_t i=0;i<cars::workshopMountCount;++i) {
        const cars::WorkshopSlot& s=cars::workshopMounts[i];InventorySection* section=inv->getSection(s.id);
        Item* it=section?section->getItem():0;
        if(it && it->getGameData() && cars::workshopAccepts(s,it->getGameData()->stringID))out[s.id]=it->getGameData()->stringID;
    }return out;
}
std::string registerMesh(const std::string& path) {
    const size_t slash=path.find_last_of("/\\");
    if(slash==std::string::npos)throw std::runtime_error("Fitted mesh has no resource directory");
    const std::string directory=path.substr(0,slash),group="ProjectCarsVehicleParts";
    Ogre::ResourceGroupManager& resources=Ogre::ResourceGroupManager::getSingleton();
    if(!resources.resourceGroupExists(group))resources.createResourceGroup(group);
    if(!resources.resourceLocationExists(directory,group))resources.addResourceLocation(directory,"FileSystem",group);
    return path.substr(slash+1);
}
void hideBatchedFrame(Building* b) {
    if(!b || !b->physical || !b->getRootNode())return;
    Ogre::SceneNode* root=b->getRootNode();bool replacement=false;
    for(unsigned n=0;n<root->numChildren();++n) {
        Ogre::SceneNode* child=static_cast<Ogre::SceneNode*>(root->getChild(n));
        if(child->getName()==std::string(nodePrefix)+"Frame" && child->numAttachedObjects()){replacement=true;break;}
    }
    if(!replacement)return; // Keep the native frame if mesh/material creation failed.
    // Do not call PhysicsCollection::setVisible: native code hides whole parent
    // nodes recursively, including our modules. Mask only its original visuals.
    // SimplePhysXEntity's visual at +0x20 is read by native setVisible (0x4cb49e).
    for(unsigned n=0;n<b->physical->parts.size() && n<64;++n) {
        char* part=reinterpret_cast<char*>(b->physical->parts[n]);if(!part)continue;
        Ogre::MovableObject* ent=*reinterpret_cast<Ogre::MovableObject**>(part+0x20);
        if(ent){ent->setVisibilityFlags(0);ent->setVisible(false);}
    }
    for(unsigned n=0;n<b->physical->staticEnts.size() && n<128;++n) {
        PhysicalEntity* physical=b->physical->staticEnts[n];
        if(physical && physical->ent){physical->ent->setVisibilityFlags(0);physical->ent->setVisible(false);}
    }
}
#include "VehicleBounds.inl"
#include "TrackVisuals.inl"
#include "DrivingRuntime.inl"
#include "VehicleDents.inl"
cars::RigPoint driverGrip(Building* b,bool left,double angle) {
    if(!motorcycleFrame(b))return cars::steeringGrip(left,angle*2.5,xsmallFrame(b));
    return cars::RigPoint(6.4,6.92,0)+cars::rotateRigPoint(cars::RigPoint(-.45,1.15,left?-3:3),cars::RigPoint(0,1,0),angle);
}
#include "DriverPose.inl"
#include "VehicleLights.inl"
void clearSeatClaim(Building* b) {
    UseableStuff* seat=b->getUseableStuff();
    if(seat && !seat->getOccupant().isNull()){const hand occupant=seat->getOccupant();seat->stopOperating(occupant);}
    seatRequests.erase(b->getHandle());releaseDriver(b);
}
void clearOrphanSeat(Building* b) {
    UseableStuff* seat=b->getUseableStuff();if(!seat || seat->getOccupant().isNull())return;
    const hand who=seat->getOccupant();const hand vehicle=b->getHandle();
    std::map<hand,DriveState>::const_iterator drive=drives.find(vehicle);
    if(drive!=drives.end() && (cars::sameHandle(drive->second.occupant,who)||cars::sameHandle(drive->second.restoreOccupant,who)||drive->second.streamPending))return;
    std::map<hand,SeatRequest>::const_iterator request=seatRequests.find(vehicle);
    if(request!=seatRequests.end() && cars::sameHandle(request->second.actor,who) && !request->second.interrupted)return;
    seat->stopOperating(who);DebugLog("Project Cars: orphan native seat reservation released / "+vehicle.toString());
}
bool restoreDriverSeat(Building* b,DriveState& drive,const hand& who,double now) {
    Character* actor=who.getCharacter();UseableStuff* seat=b->getUseableStuff();
    if(!actor || !interactionActor(b,actor,true) || !seat || !fitted(b,"seat_driver") || !actor->getMovement() || !actor->getOrdersReciever())return false;
    // Never reclaim a character that actually walked away or is in another car.
    if(actor->getPosition().squaredDistance(rigWorld(b,driverAnchor(b)))>6400)return false;
    for(std::map<hand,DriveState>::const_iterator d=drives.begin();d!=drives.end();++d)
        if(!cars::sameHandle(d->first,b->getHandle()) && cars::sameHandle(d->second.occupant,who))return false;
    if(!seat->getOccupant().isNull() && !cars::sameHandle(seat->getOccupant(),who))return false;
    SeatRequest request(who);request.approaching=false;request.acknowledged=true;request.stamp=now;request.entry=actor->getPosition();
    seatRequests[b->getHandle()]=request;
    if(!cars::sameHandle(seat->getOccupant(),who) && (!seat->_NV_tryOperate(who) || !cars::sameHandle(seat->getOccupant(),who))){seatRequests.erase(b->getHandle());return false;}
    drive.occupant=who;drive.restoreOccupant.setNull();drive.streamPending=false;drive.physicsSeen=GetTickCount();drive.stamp=-1;drive.accumulator=0;
    issuingSeatOrder=true;
    try {
        actor->getOrdersReciever()->addOrder(HOLD_POSITION,hand(),actor->getPosition(),true,false);
        actor->getMovement()->halt();
        actor->getMovement()->_setPositionDirectionAndTeleport(rigWorld(b,driverAnchor(b)),b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y));
    }catch(...){issuingSeatOrder=false;clearSeatClaim(b);throw;}
    issuingSeatOrder=false;
    if(actor->getAnimationClass())actor->getAnimationClass()->playAction("sitting chair",1,1,false);
    DebugLog("Project Cars: driver seat restored after loading / "+who.toString()+" / "+b->getHandle().toString());return true;
}

}
namespace cars {
#include "VehicleDebugServices.inl"
void clearVehicleRouteRequests(){Lock lock;clearNativePaths();}
bool vehicleRecoveryActive(){Lock lock;return !recoveryPlacement.vehicle.isNull();}
void cancelVehicleRecovery(){Lock lock;cancelRecovery();}
void updateVehicleRecovery(const Ogre::Ray& ray,bool confirm,int turn){Lock lock;if(recoveryPlacement.vehicle.isNull())return;recoveryPlacement.cursor=ray;recoveryPlacement.confirm|=confirm;recoveryPlacement.turn+=turn;recoveryPlacement.inputPending=true;}
std::string vehicleRecoveryStatus(){Lock lock;return recoveryPlacement.status;}
std::string vehicleInteractionStatus(Building* b){Lock lock;return b?messages[b->getHandle()]:"Vehicle unavailable.";}
VehicleActions vehicleActions(Building* b,Character* actor) {
    Lock lock;VehicleActions out;if(!frame(b) || !b->isValid())return out;
    clearOrphanSeat(b);
    out.seat=fitted(b,"seat_driver");out.steering=fitted(b,"steering");out.cargo=fittedDesign(b,"cargo","131-Project Cars.mod");
    UseableStuff* seat=b->getUseableStuff();out.seated=actor && seat && sameHandle(seat->getOccupant(),actor->getHandle());
    std::map<hand,SeatRequest>::const_iterator request=seatRequests.find(b->getHandle());
    if(drives.count(b->getHandle())) {DriveState& s=drives[b->getHandle()];out.ignition=s.body.ignition;out.overturned=s.occupant.isNull();out.seated=actor && sameHandle(s.occupant,actor->getHandle());out.driven=s.occupant.isValid();out.manual=sameHandle(manualVehicle,b->getHandle());}
    out.boarding=actor && request!=seatRequests.end() && sameHandle(request->second.actor,actor->getHandle()) && !request->second.acknowledged && !request->second.interrupted;
    out.occupied=passengerSlotOccupied(b,"seat_driver") || vehicleSeatBlocked(seat && !seat->getOccupant().isNull(),out.seated,request!=seatRequests.end(),out.boarding,request!=seatRequests.end() && request->second.interrupted);
    out.passenger=availablePersonMount(b,"seat_passenger_l")||availablePersonMount(b,"seat_passenger_r");
    out.gunner=availablePersonMount(b,"cargo",cars::turretItemId) && actor && !attachedOccupant(b,actor->getHandle());
    out.people=passengers.count(b->getHandle()) && !passengers[b->getHandle()].empty();
    bool carrying=actor && !actor->getCarryingObject().isNull();
    out.placeDriver=carrying&&availablePersonMount(b,"seat_driver");
    out.placeLeft=carrying&&availablePersonMount(b,"seat_passenger_l");out.placeRight=carrying&&availablePersonMount(b,"seat_passenger_r");
    out.placeFrontPole=carrying&&availablePersonMount(b,"front_attachment","155-Project Cars.mod");
    out.placeRearPole=carrying&&availablePersonMount(b,"cargo","155-Project Cars.mod");out.placeCage=carrying&&availablePersonMount(b,"cargo","156-Project Cars.mod");
    out.seated|=actor && attachedOccupant(b,actor->getHandle());
    return out;
}
bool pickOneVehicle(Building* b,const Ogre::Ray& ray,double& nearest,hand& target,VehicleClick& action) {
    if(!frame(b) || !b->isValid() || !b->isPhysical() || !b->entitiesLoaded || !b->getRootNode() || b->getFaction()!=ou->player->getFaction())return false;
    Building::ConstructionState* state=b->getBuildState_ActualNonShared();if(!state || !state->isComplete || state->isDismantled)return false;
    Ogre::SceneNode* root=b->getRootNode();Ogre::Vector3 o=root->convertWorldToLocalPosition(ray.getOrigin());
    Ogre::Vector3 d=root->convertWorldToLocalPosition(ray.getOrigin()+ray.getDirection())-o;
    PickVector origin(o.x,o.y,o.z),direction(d.x,d.y,d.z);
    if(xsmallFrame(b)) {
        bool hit=false;
        size_t count=0;
        const cars::WorkshopVisual* visuals=frameVisuals(b,count);
        const cars::PickPlane (*planes)[6]=compactPickPlanes(b);
        const char* quadSlots[]={"","engine","tank","seat_driver","steering","wheel_fl","wheel_fr","wheel_rl","wheel_rr"};
        const char* bikeSlots[]={"","engine","tank","seat_driver","steering","wheel_fl","wheel_rl"};
        const char** slots=motorcycleFrame(b)?bikeSlots:quadSlots;
        for(size_t n=0;n<count;++n) {
            const std::string component=visuals[n].component;
            if(n && !fitted(b,slots[n]))continue;
            if(!hitVehicleHull(origin,direction,planes[n],6,nearest))continue;
            target=b->getHandle();
            action=component=="Seat_Driver"?SitDriver:component=="Steering_Wheel"?DriveVehicle:ConfigureVehicle;
            hit=true;
        }
        return hit;
    }
    const PickPlane broad[]={{-1,0,0,-35},{1,0,0,-35},{0,-1,0,2},{0,1,0,-45},{0,0,-1,-25},{0,0,1,-25}};
    double broadDistance=nearest;if(!hitVehicleHull(origin,direction,broad,6,broadDistance))return false;
    const std::map<std::string,std::string> items=contents(lookup(b));std::set<std::string> active;active.insert("Frame");
    for(unsigned n=0;n<workshopMountCount;++n) {
        const WorkshopSlot& s=workshopMounts[n];std::map<std::string,std::string>::const_iterator i=items.find(s.id);
        if(i!=items.end() && fitted(b,s.id))active.insert(workshopComponent(s,i->second));
    }
    bool hit=false;
    if(fittedTurret(b)) {
        const PickPlane turretHull[]={{-1,0,0,-29},{1,0,0,5},{0,-1,0,11},{0,1,0,-33},{0,0,-1,-10},{0,0,1,-10}};
        // The turret is an explicit gunner assignment from the context menu.
        // A normal click on its housing must still resolve to the vehicle's
        // configure/drive action instead of treating the gunner mount as a seat.
        if(hitVehicleHull(origin,direction,turretHull,6,nearest)){target=b->getHandle();action=ConfigureVehicle;hit=true;}
    }
    for(unsigned n=0;n<vehiclePickHullCount;++n) {
        const PickHull& h=vehiclePickHulls[n];if(!active.count(h.component))continue;
        if(hitVehicleHull(origin,direction,vehiclePickPlanes+h.first,h.count,nearest)){target=b->getHandle();action=h.action;hit=true;}
    }
    return hit;
}
bool pickVehiclePart(const Ogre::Ray& ray,Building* preferred,hand& target,VehicleClick& action) {
    Lock lock;double nearest=10000;bool hit=pickOneVehicle(preferred,ray,nearest,target,action);
    for(std::map<hand,Inventory*>::const_iterator i=stores.begin();i!=stores.end();++i) {
        Building* b=i->first.getBuilding();if(b && b!=preferred && pickOneVehicle(b,ray,nearest,target,action))hit=true;
    }
    return hit;
}
hand drivenVehicle(Character* actor) {
    Lock lock;if(!actor || !actor->isValid())return hand();
    for(std::map<hand,DriveState>::const_iterator i=drives.begin();i!=drives.end();++i)
        if(!i->second.occupant.isNull() && sameHandle(i->second.occupant,actor->getHandle()))return i->first;
    return hand();
}
bool selectedVehicleAction(VehicleClick action,bool execute) {
    Lock lock;if(!enabled || !ou || !ou->player)return false;
    std::vector<std::pair<hand,hand> > selected;
    for(std::map<hand,DriveState>::const_iterator i=drives.begin();i!=drives.end();++i) {
        Character* actor=i->second.occupant.getCharacter();Building* b=i->first.getBuilding();
        if(!b || !actor || !actor->isValid() || !actor->isPlayerCharacter() || !ou->player->isObjectSelected(actor))continue;
        if(!execute)return true;
        selected.push_back(std::make_pair(i->first,actor->getHandle()));
    }
    if(action==RightVehicle || action==LeaveVehicle || action==IgnitionVehicle || action==HeadlightsVehicle)for(std::map<hand,std::map<std::string,MountedPerson> >::const_iterator b=passengers.begin();b!=passengers.end();++b)
        for(std::map<std::string,MountedPerson>::const_iterator p=b->second.begin();p!=b->second.end();++p){Character* person=p->second.person.getCharacter();if(person && person->isPlayerCharacter() && ou->player->isObjectSelected(person)){if(!execute)return true;selected.push_back(std::make_pair(b->first,p->second.person));}}
    // Snapshot before dispatch: exiting releases boarding state and orders.
    std::set<hand> switched;
    for(size_t n=0;n<selected.size();++n) {
        if(action!=LeaveVehicle && !switched.insert(selected[n].first).second)continue;
        interactWithVehicle(selected[n].first.getBuilding(),selected[n].second.getCharacter(),action);
    }
    return !selected.empty();
}
bool manualDriving(){Lock lock;return enabled && !manualVehicle.isNull();}
ManualVehicleView manualVehicleView() {
    Lock lock;ManualVehicleView view;
    if(!enabled || manualVehicle.isNull())return view;
    view.vehicle=manualVehicle;
    std::map<hand,DriveState>::iterator i=drives.find(manualVehicle);
    Building* b=manualVehicle.getBuilding();Character* actor=i==drives.end()?0:i->second.occupant.getCharacter();
    if(i==drives.end() || !b || !b->isValid() || !actor || !actor->isValid() || i->second.streamPending || !b->isPhysical() || !b->entitiesLoaded || !b->getRootNode())return view;
    UseableStuff* seat=b->getUseableStuff();if(!seat || !sameHandle(seat->getOccupant(),i->second.occupant))return view;
    Ogre::SceneNode* root=b->getRootNode();const Ogre::Quaternion q=root->_getDerivedOrientation();
    view.anchor=point(root->_getDerivedPosition());view.rotation=BodyRotation(q.w,q.x,q.y,q.z);view.scale=b->getScale();
    view.extraSmall=xsmallFrame(b);
    view.ready=validPoint(view.anchor) && _finite(q.w+q.x+q.y+q.z+view.scale) && view.scale>0;return view;
}
RigPoint vehicleCameraClearance(const VehicleCameraPose& pose) {
    Lock lock;Building* b=manualVehicle.getBuilding();if(!b||!b->isValid()||!b->isPhysical()||!b->getRootNode())return pose.eye;
    std::map<hand,DriveState>::iterator i=drives.find(manualVehicle);if(i==drives.end())return pose.eye;
    // Camera nodes use origin-relative coordinates; physics uses world space.
    const RigPoint shift=point(b->getPosition()-b->getRootNode()->_getDerivedPosition());
    // Camera callbacks only query physics. Temporary collision-group mutations
    // remain confined to the verified post-simulation physics callback.
    RoadReport report;NativeRoad road(b,report,i->second.occupant);SurfaceHit hit;
    RigPoint eye=pose.eye;
    if(road.ray(pose.focus+shift,pose.eye+shift,hit))eye=pose.focus+(pose.eye-pose.focus).unit()*std::max(1.0,hit.distance-2);
    return eye;
}
void endManualDriving(){Lock lock;endManual();}
void updateManualDriving(const ManualDrivingKeys& keys) {
    Lock lock;if(manualVehicle.isNull())return;
    std::map<hand,DriveState>::iterator i=drives.find(manualVehicle);
    Building* b=manualVehicle.getBuilding();Character* actor=i==drives.end()?0:i->second.occupant.getCharacter();
    if(i==drives.end() || !b || !b->isValid()){endManual();return;}
    if(!i->second.streamPending && (!actor || !actor->isValid() || !b->isPhysical()))suspendDrive(b);
    if(i->second.streamPending || !b->isPhysical() || !actor || !actor->isValid()) {manualKeys=ManualDrivingKeys();if(vehicleStreamExpired((GetTickCount()-i->second.streamAt)*.001))clearSeatClaim(b);return;}
    if(!b || !b->isValid() || !actor || !actor->isValid() || actor->isDead() || !actor->getMedical() || actor->getMedical()->isUnconcious() || i->second.fault || i->second.exitRequested || !fitted(b,"steering")) {endManual();return;}
    manualKeys=keys;
}
bool interactWithVehicle(Building* b,Character* actor,VehicleClick action) {
    Lock lock;if(!b)return false;
    if(actor && passengerAction(b,actor,action))return true;
    if((action==ManualDriveVehicle || action==EndManualDriveVehicle) && drives.count(b->getHandle()))actor=drives[b->getHandle()].occupant.getCharacter();
    const bool drivingControl=action==RightVehicle || action==LeaveVehicle || action==IgnitionVehicle || action==StopVehicle || action==PaceVehicle || action==ManualDriveVehicle || action==EndManualDriveVehicle || action==HeadlightsVehicle;
    if(!interactionActor(b,actor,drivingControl)){messages[b->getHandle()]="Select a conscious character from the vehicle's faction, out of combat for fitting or boarding.";return false;}
    if(action==OpenTrunk && !eligible(b,actor)){messages[b->getHandle()]="Move closer to open the trunk.";return false;}
    ensure(b);
    if(action==IgnitionVehicle||action==StopVehicle||action==PaceVehicle||action==RightVehicle||action==ManualDriveVehicle||action==EndManualDriveVehicle||action==HeadlightsVehicle)return drivingMenu(b,actor,action);
    if(action==OpenTrunk) {
        if(!fittedDesign(b,"cargo","131-Project Cars.mod")){messages[b->getHandle()]="Install a cargo crate first.";return false;}
        std::map<hand,Job>::const_iterator job=jobs.find(b->getHandle());
        if(job!=jobs.end() && job->second.request.slot=="cargo"){messages[b->getHandle()]="Wait for cargo work to finish.";return false;}
        if(!b->getInventory() || !::gui){messages[b->getHandle()]="Trunk unavailable. Restart with the updated vehicle content.";return false;}
        ::gui->showInventoryBuilding(b->getHandle());
        DebugLog("Project Cars: trunk opened / "+b->getHandle().toString());return true;
    }
    if(action==LeaveVehicle) {
        if(sameHandle(manualVehicle,b->getHandle()) && sameHandle(drives[b->getHandle()].occupant,actor->getHandle()))endManual();
        if(drives.count(b->getHandle()) && drives[b->getHandle()].streamPending && sameHandle(drives[b->getHandle()].occupant,actor->getHandle())) {
            drives[b->getHandle()].body.velocity=RigPoint();drives[b->getHandle()].body.angular=RigPoint();
        }
        if(drives.count(b->getHandle()) && drives[b->getHandle()].body.speed()>2) {
            DriveState& drive=drives[b->getHandle()];if(!sameHandle(drive.occupant,actor->getHandle()))return false;
            brakeDrive(drive);drive.exitRequested=true;messages[b->getHandle()]="Braking, then getting out.";return true;
        }
        std::map<hand,SeatRequest>::iterator pending=seatRequests.find(b->getHandle());
        if(pending!=seatRequests.end() && sameHandle(pending->second.actor,actor->getHandle())) {
            finishBoarding(b,pending->second,false);seatRequests.erase(pending);messages[b->getHandle()]="Boarding stopped / driver seat free.";return true;
        }
        UseableStuff* seat=b->getUseableStuff();if(!seat || !sameHandle(seat->getOccupant(),actor->getHandle()))return false;
        Ogre::Vector3 entry;if(!boardingEntry(b,actor,entry))return false;
        OrdersReceiver* orders=actor->getOrdersReciever();if(!orders)return false;
        releaseDriver(b);orders->clearOrders();orders->clearCurrentGoal(true);seat->stopOperating(actor->getHandle());seatRequests.erase(b->getHandle());
        if(actor->getMovement())actor->getMovement()->_setPositionAndTeleport(entry,0);
        messages[b->getHandle()]="Left the driver seat.";return true;
    }
    if(action==SitDriver || action==DriveVehicle) {
        UseableStuff* seat=b->getUseableStuff();
        if(!seat || !fitted(b,"seat_driver")){messages[b->getHandle()]="Driver seat unavailable. Restart with the updated vehicle content.";return false;}
        if(action==DriveVehicle && !fitted(b,"steering")){messages[b->getHandle()]="Install a steering wheel first.";return false;}
        clearOrphanSeat(b);
        if(sameHandle(seat->getOccupant(),actor->getHandle())) {
            if(restoreDriverSeat(b,driveState(b),actor->getHandle(),ou->getTimeStamp_inGameHours().getTotalHours())){messages[b->getHandle()]="In the driver seat.";return true;}
            clearSeatClaim(b);
        }
        std::map<hand,SeatRequest>::const_iterator existing=seatRequests.find(b->getHandle());
        if(existing!=seatRequests.end() && sameHandle(existing->second.actor,actor->getHandle())){finishBoarding(b,seatRequests[b->getHandle()],false);seatRequests.erase(b->getHandle());}
        if(seatInUse(b)){messages[b->getHandle()]="Driver seat is occupied or reserved.";return false;}
        std::map<hand,Job>::const_iterator job=jobs.find(b->getHandle());
        if(job!=jobs.end() && job->second.request.slot=="seat_driver"){messages[b->getHandle()]="Wait for seat work to finish.";return false;}
        OrdersReceiver* orders=actor->getOrdersReciever();if(!orders)return false;
        SeatRequest request(actor->getHandle());if(!boardingEntry(b,actor,request.entry)){messages[b->getHandle()]="No safe boarding point beside the car.";return false;}
        request.stamp=ou->getTimeStamp_inGameHours().getTotalHours();seatRequests[b->getHandle()]=request;
        // Navigate only to terrain beside the frame. The raised seat inside its
        // collider is a boarding destination, not a walkable path endpoint.
        issuingSeatOrder=true;
        try {orders->addOrder(MOVE_CUS_ORDERED,hand(),request.entry,true,false);}
        catch(...){issuingSeatOrder=false;seatRequests.erase(b->getHandle());throw;}
        issuingSeatOrder=false;
        messages[b->getHandle()]="Walking to boarding point.";
        std::ostringstream detail;detail<<"Project Cars: driver seat requested / "<<actor->getHandle().toString()<<" / "<<b->getHandle().toString()
            <<" / task="<<seat->getDefaultTask()<<" / operators="<<seat->numOperatorsMax<<" / range="<<seat->maxUseRange<<" / nativeEligible="<<originalCouldOperate(seat,actor->getHandle());
        DebugLog(detail.str());return true;
    }
    return false;
}
void fittingSnapshot(Building* b,Character* actor,WorkshopSnapshot& out) {
    Lock lock;out.extraSmall=xsmallFrame(b);out.motorcycle=motorcycleFrame(b);out.fittingAllowed=eligible(b,actor);out.installed=contents(ensure(b));
    for(size_t n=0;n<workshopMountCount;++n){Item* part=fittedItem(b,workshopMounts[n].id);if(part)out.condition[workshopMounts[n].id]=bounded(part->chargesLeft,0,1);}
    if(drives.count(b->getHandle()) && (drives[b->getHandle()].body.ignition||drives[b->getHandle()].body.speed()>1))out.fittingAllowed=false;
    if(out.installed.count("tank"))out.tankCapacity=40;
    if(drives.count(b->getHandle()))out.fuel=drives[b->getHandle()].body.fuel;
    out.status=out.fittingAllowed?messages[b->getHandle()]:eligible(b,actor)?
        (drives.count(b->getHandle()) && drives[b->getHandle()].body.ignition?"Switch off the engine to fit components.":"Wait for the vehicle to settle. Use Correct Vehicle if the slope is too steep."):
        "Bring a character nearby and out of combat to fit parts.";
    if(actor && actor->getStats()){const VehicleSkills& s=skills[actor->getStats()];out.mechanics=vehicleSkillLevel(s.mechanicsXp);out.handling=vehicleSkillLevel(s.handlingXp);}
    std::map<hand,WorkPlan>::const_iterator p=workPlans.find(b->getHandle());
    if(p!=workPlans.end()) {
        out.queueRunning=p->second.queue.running;
        for(size_t n=0;n<p->second.queue.steps.size();++n) {
            const FitStep& step=p->second.queue.steps[n];const WorkshopSlot* slot=mount(step.slot);
            out.queued[step.slot]=step.remove?"REMOVE QUEUED":"INSTALL QUEUED";out.queueSlots.push_back(step.slot);
            std::string itemName=step.item;
            for(size_t k=0;k<workshopItemCount;++k)if(step.item==workshopItems[k].id){itemName=workshopItems[k].name;break;}
            out.queueLabels.push_back(std::string(step.remove?"Remove ":"Install ")+(slot?slot->name:step.slot)+"\n"+itemName);
        }
    }
    std::map<hand,Job>::const_iterator j=jobs.find(b->getHandle());
    if(j!=jobs.end()) {
        out.working=true;out.workProgress=j->second.work.fraction();
        std::ostringstream status;status<<(ou->isPaused()?"Paused: ":j->second.request.remove?"Removing: ":"Installing: ")<<static_cast<int>(out.workProgress*100)<<"%";out.status=status.str();
    }
}
std::string commitFit(Building* b,Character* actor,const WorkshopRequest& req) {
    VehicleFaultScope trace("fitting inventory transfer");
    Lock lock;
    if(!eligible(b,actor) || req.target!=b->getHandle().toString() || req.actor!=actor->getHandle().toString())return "The character or vehicle changed. Try again.";
    const WorkshopSlot* slot=mount(req.slot);if(!slot || !workshopMountSupported(xsmallFrame(b),req.slot,motorcycleFrame(b)))return "Mount unavailable on this frame.";
    Inventory* inv=ensure(b);Inventory* carried=actor->getInventory();if(!inv || !carried)return "Inventory unavailable.";
    InventorySection* section=inv->getSection(req.slot);if(!section)return "Mount unavailable.";
    if(req.remove) {
        if(passengerSlotOccupied(b,req.slot))return "Unload the occupant before removing this mount.";
        if((req.slot=="tank" || req.slot=="engine") && drives.count(b->getHandle()) && drives[b->getHandle()].body.fuel>0)return "Drain the tank before removing the engine or tank.";
        if(req.slot=="seat_driver" && seatInUse(b))return "Leave the seat before removing it.";
        if(req.slot=="cargo" && b->getInventory() && !b->getInventory()->isEmpty())return "Empty the trunk before removing the cargo crate.";
        Item* fitted=section->getItem();if(!fitted)return "This mount is empty.";
        GameData* def=fitted->getGameData();if(!def || def->stringID!=req.item)return "The fitted part changed; work cancelled.";
        if(!carried->hasRoomForItem(def))return "Make room in the character's inventory first.";
        int before=carried->countItems(def);
        Item* item=inv->removeItemDontDestroy_returnsItem(fitted,1,true);if(!item)return "Could not remove the part.";
        if(!carried->addItem(item,1,false,false)) {
            if(!section->addItem(item,1)) {carried->addItem(item,1,true,false);enabled=false;return "Transfer interrupted; fitting disabled. Check the ground for the part.";}
            return "No room. The part remains fitted.";
        }
        if(!section->isEmpty() || carried->countItems(def)!=before+1){enabled=false;return "Transfer could not be verified; fitting disabled.";}
        detachUnsupportedPlates(b);b->notifyChange();return "Removed";
    }
    if(!section->isEmpty())return "Remove the fitted part first.";
    if(req.slot=="engine" && drives.count(b->getHandle()) && drives[b->getHandle()].body.fuel>0)return "Drain the tank before changing engine types.";
    if(!wheelForPlate(req.slot).empty() && !fitted(b,wheelForPlate(req.slot)))return "Install the matching wheel first.";
    if(!workshopAccepts(*slot,req.item))return "That part does not fit this mount.";
    if(!workshopConflict(*slot,contents(inv)).empty())return "Mount conflict. Remove the conflicting parts first.";
    GameData* def=ou->gamedata.getData(req.item,ITEM);
    if(!def || carried->countItems(def)<1)return "The character is not carrying that part.";
    if(!section->hasRoomForItem(def,1))return "This mount cannot accept the part.";
    const int before=carried->countItems(def);
    Item* item=carried->takeOneItemOnly(def);if(!item)return "Could not take the carried part.";
    if(!section->addItem(item,1)) {
        carried->addItem(item,1,true,false);return "Fitting failed; the part was returned or placed at your feet.";
    }
    if(carried->countItems(def)!=before-1 || !section->hasItem(def)){enabled=false;return "Transfer could not be verified; fitting disabled.";}
    b->notifyChange();return "Installed";
}
std::string startFit(Building* b,Character* actor,const WorkshopRequest& req) {
    Lock lock;
    if(req.target!=b->getHandle().toString())return "The vehicle changed. Try again.";
    if(req.slot=="cancel") {std::map<hand,Job>::iterator j=jobs.find(b->getHandle());if(j!=jobs.end()){finishActivity(j->second);jobs.erase(j);}return messages[b->getHandle()]="Work cancelled. No parts moved.";}
    if(!eligible(b,actor) || req.actor!=actor->getHandle().toString() || req.target!=b->getHandle().toString())return "Bring the character nearby and out of combat.";
    if(jobs.count(b->getHandle()))return "This vehicle already has work in progress.";
    if(req.remove && req.slot=="seat_driver" && seatInUse(b))return "Leave the seat before removing it.";
    if(req.remove && req.slot=="cargo" && b->getInventory() && !b->getInventory()->isEmpty())return "Empty the trunk before removing the cargo crate.";
    for(std::map<hand,Job>::const_iterator i=jobs.begin();i!=jobs.end();++i)if(sameHandle(i->second.actor,actor->getHandle()))return "This character is already working on a vehicle.";
    if(jobs.size()>=64)return "Too many active vehicle jobs.";
    const WorkshopSlot* slot=mount(req.slot);if(!slot || !workshopMountSupported(xsmallFrame(b),req.slot,motorcycleFrame(b)))return "Mount unavailable on this frame.";
    Inventory* inv=ensure(b);if(!inv)return "Mount inventory unavailable.";
    InventorySection* section=inv->getSection(req.slot);
    if(!section || (req.remove?section->isEmpty():!section->isEmpty()))return req.remove?"This mount is empty.":"Remove the fitted part first.";
    if(!req.remove && !workshopAccepts(*slot,req.item))return "Incompatible part.";
    if(!req.remove && !wheelForPlate(req.slot).empty() && !fitted(b,wheelForPlate(req.slot)))return "Install the matching wheel first.";
    if(!req.remove && !workshopConflict(*slot,contents(inv)).empty())return "Mount conflict. Remove the conflicting parts first.";
    GameData* def=ou->gamedata.getData(req.item,ITEM);
    if(!req.remove && (!actor->getInventory() || !def || actor->getInventory()->countItems(def)<1))return "The character must carry this part.";
    if(req.remove) {
        Item* part=section->getItem();
        if(!part || !part->getGameData())return "The fitted part is unavailable.";
        if(!actor->getInventory() || !actor->getInventory()->hasRoomForItem(part->getGameData()))return "Make room in the character's inventory for the removed part, then Resume.";
        if(passengerSlotOccupied(b,req.slot))return "Unload the occupant before removing this mount.";
        if((req.slot=="tank" || req.slot=="engine") && drives.count(b->getHandle()) && drives[b->getHandle()].body.fuel>0)return "Drain the tank before removing the engine or tank.";
    }
    Job job;job.actor=actor->getHandle();job.request=req;job.origin=actor->getPosition();job.stamp=ou->getTimeStamp_inGameHours().getTotalHours();
    if(req.remove)job.request.item=section->getItem()->getGameData()->stringID;
    job.work=FittingWork((req.slot=="engine"?60:slot->system==2?15:30)*(req.remove?0.75:1.0));
    OrdersReceiver* orders=actor->getOrdersReciever();if(!orders)return "Character orders unavailable.";
    // Replace the previous movement order once. Permanent jobs and their toggle
    // remain intact. The fitting job owns this temporary order, not the window.
    orders->addOrder(HOLD_POSITION,hand(),job.origin,true,false);
    if(!orders->hasPlayerOrder(HOLD_POSITION))return "Could not start vehicle work. Try again.";
    if(req.remove && req.slot=="cargo" && ::gui)::gui->closeInventory(b->getHandle());
    jobs.insert(std::make_pair(b->getHandle(),job));
    DebugLog("Project Cars: work started "+req.slot+" / "+job.request.item+" / "+req.actor+" / "+req.target);
    return messages[b->getHandle()]="Work started.";
}
void pausePlan(const hand& h) {
    std::map<hand,Job>::iterator job=jobs.find(h);
    if(job!=jobs.end()){workPlans[h].queue.pause(job->second.work.done);finishActivity(job->second);jobs.erase(job);}
    else workPlans[h].queue.running=false;
}
std::string startNextQueued(Building* b,WorkPlan& plan) {
    // A wheel removal/destroy can already have dropped a queued plate. That
    // completed dependency must not pause the remaining conversion to tracks.
    while(!plan.queue.steps.empty() && plan.queue.steps.front().remove &&
        !wheelForPlate(plan.queue.steps.front().slot).empty() && !fittedItem(b,plan.queue.steps.front().slot))plan.queue.completed();
    if(plan.queue.steps.empty()){plan.queue.running=false;return "Queue complete.";}
    Character* actor=plan.actor.getCharacter();if(!actor || !actor->isValid()){plan.queue.running=false;return "Select a worker and Resume.";}
    const FitStep& step=plan.queue.steps.front();WorkshopRequest request;
    request.target=b->getHandle().toString();request.actor=actor->getHandle().toString();request.slot=step.slot;request.item=step.item;request.remove=step.remove;
    const std::string result=startFit(b,actor,request);
    if(result!="Work started."){plan.queue.running=false;DebugLog("Project Cars: queue paused / "+step.slot+" / "+result);return "Queue paused: "+result;}
    Job& job=jobs[b->getHandle()];job.work.done=std::min(step.done,job.work.required-0.000001);
    return result;
}
std::string editQueue(Building* b,Character* actor,const WorkshopRequest& req) {
    if(!b || !frame(b) || !actor || !actor->isValid() || req.target!=b->getHandle().toString() || req.actor!=actor->getHandle().toString())return "Select a worker for this vehicle.";
    WorkPlan& plan=workPlans[b->getHandle()];
    if(req.slot=="queue_pause" || req.slot=="cancel"){pausePlan(b->getHandle());return "Queue paused. Progress retained.";}
    if(req.slot=="queue_clear"){pausePlan(b->getHandle());plan.queue.steps.clear();return "Queue cleared. Fitted parts unchanged.";}
    if(req.slot=="queue_start") {
        if(plan.queue.running)return "Queue already running.";
        if(!eligible(b,actor))return "Bring a worker nearby and out of combat.";
        plan.actor=actor->getHandle();plan.queue.running=true;return startNextQueued(b,plan);
    }
    if(req.slot=="queue_drop") {
        if(!plan.queue.contains(req.item))return "That mount is not queued.";
        FittingQueue proposed=plan.queue;proposed.erase(req.item);
        if(!proposed.layoutValid(contents(lookup(b)),workshopMounts,workshopMountCount))return "Unqueue the dependent installations first.";
        if(!plan.queue.steps.empty() && plan.queue.steps.front().slot==req.item)pausePlan(b->getHandle());
        plan.queue.erase(req.item);return "Removed from queue.";
    }
    if(!eligible(b,actor))return "Bring a worker nearby and out of combat.";
    const WorkshopSlot* slot=mount(req.slot);if(!slot || !workshopMountSupported(xsmallFrame(b),req.slot,motorcycleFrame(b)))return "Mount unavailable on this frame.";
    Inventory* inv=ensure(b);InventorySection* section=inv?inv->getSection(req.slot):0;
    if(!section)return "Mount unavailable.";
    if(plan.queue.contains(req.slot))return "This mount is already queued.";
    if(req.remove?section->isEmpty():!section->isEmpty())return req.remove?"This mount is empty.":"Remove the fitted part first.";
    FitStep step;step.slot=req.slot;step.remove=req.remove;
    step.item=req.remove?section->getItem()->getGameData()->stringID:req.item;
    if(!workshopAccepts(*slot,step.item))return "Incompatible component.";
    FittingQueue proposed=plan.queue;
    if(!proposed.add(step))return "Queue is full.";
    if(!proposed.layoutValid(contents(inv),workshopMounts,workshopMountCount))return "Queue removal of the conflicting parts first.";
    if(!plan.queue.add(step))return "Queue is full.";
    // Planning does not reserve or consume items. Missing supplies pause the
    // batch when that step starts, so a whole armor set can be planned at once.
    b->notifyChange();return "Added to work queue.";
}
std::string fitComponent(Building* b,Character* actor,const WorkshopRequest& req) {
    Lock lock;if(!b||!actor)return "Character or vehicle unavailable.";
    if(req.target!=b->getHandle().toString()||req.actor!=actor->getHandle().toString())return "Selection changed.";
    if(req.slot=="fuel_refuel"||req.slot=="fuel_drain")return messages[b->getHandle()]=fuelService(b,actor,req.slot=="fuel_drain");
    if(drives.count(b->getHandle())) {
        if(drives[b->getHandle()].body.ignition)return messages[b->getHandle()]="Switch off the engine before fitting parts.";
        if(drives[b->getHandle()].body.speed()>1)return messages[b->getHandle()]="Wait for the vehicle to settle. Use Correct Vehicle if the slope is too steep.";
    }
    return messages[b->getHandle()]=editQueue(b,actor,req);
}
void pauseVehicleStreaming() {
    Lock lock;manualKeys=ManualDrivingKeys();
    pauseVehicleTurrets();
    clearDriverGrips();
    const DWORD tick=GetTickCount();
    for(std::map<hand,DriveState>::iterator d=drives.begin();d!=drives.end();++d) {
        for(unsigned n=0;n<4;++n)d->second.treads[n].reset();
        d->second.stamp=-1;d->second.accumulator=0;d->second.physicsSeen=tick;d->second.streamAt=tick;
        if(d->second.retryPhysics)d->second.retryAt=tick;
    }
    if(ou && ou->initialized) {
        const double now=ou->getTimeStamp_inGameHours().getTotalHours();
        for(std::map<hand,SeatRequest>::iterator s=seatRequests.begin();s!=seatRequests.end();++s)s->second.stamp=now;
        for(std::map<hand,Job>::iterator j=jobs.begin();j!=jobs.end();++j)j->second.stamp=now;
    }
    vehicleAudio.stop();
}
void cancelAllFittingWork(){
    Lock lock;
    pauseVehicleTurrets();
    nativeRoadNetwork.reset();
    clearNativePaths();
    cancelVehicleRecovery();
    clearVehicleTrails();
    endManual(false);
    for(std::map<hand,Job>::const_iterator i=jobs.begin();i!=jobs.end();++i)if(workPlans.count(i->first))workPlans[i->first].queue.pause(i->second.work.done);
    jobs.clear();messages.clear();seatRequests.clear();vehicleHits.clear();ramContacts.clear();
    vehicleAudio.stop();
    cars::drivingStatus("");
    clearDriverGrips();
    for(std::map<hand,DriveState>::iterator d=drives.begin();d!=drives.end();++d){stopDrive(d->second);d->second.occupant.setNull();d->second.recovery.setNull();d->second.stamp=-1;}
    // Native Building destruction owns plan lifetime. Loading can restore a
    // new building before the UI reconciles its world epoch; preserve that plan.
    for(std::map<hand,WorkPlan>::iterator i=workPlans.begin();i!=workPlans.end();++i){i->second.queue.running=false;i->second.actor.setNull();}
}
void updateFittingWork() {
    VehicleFaultScope trace("vehicle fitting update");
    vehicleThreadId=GetCurrentThreadId();
    Lock lock;const double now=ou->getTimeStamp_inGameHours().getTotalHours();
    clearOrphanVehicleLights();
    for(std::map<hand,Inventory*>::const_iterator t=stores.begin();t!=stores.end();++t){Building* b=t->first.getBuilding();if(b && b->isValid()){updateVehicleTurret(b);updatePassengers(b);}}
    renderVehicleTrails();
    bool audible=false;std::string hud;
    for(std::map<hand,DriveState>::const_iterator d=drives.begin();d!=drives.end();++d)if(sameHandle(d->second.occupant,ou->player->selectedCharacter) && !d->second.occupant.isNull() && !ou->isPaused()) {
        const DriveState& s=d->second;Building* b=d->first.getBuilding();
        if(s.streamPending || !b || !b->isValid() || !b->isPhysical() || !b->entitiesLoaded || !b->getRootNode())continue;
        UseableStuff* seat=b->getUseableStuff();if(!seat || !sameHandle(seat->getOccupant(),s.occupant))continue;
        cars::VehicleSoundInput sound;sound.engine=s.body.ignition;sound.speed=s.body.speed();sound.throttle=s.input.throttle;sound.brake=s.input.brake;sound.slip=s.body.tireSlip;sound.impact=s.audioImpact;sound.overturned=s.body.overturned();
        vehicleAudio.update(d->first.toString(),sound,options?options->sfxVolume:.6);audible=true;break;
    }
    if(!audible)vehicleAudio.stop();
    for(std::map<hand,DriveState>::iterator d=drives.begin();d!=drives.end();++d) {
        DriveState& drive=d->second;Building* b=d->first.getBuilding();
        drive.orderStream.observeMouse(key && key->mRight);
        drive.audioImpact=0;
        if(!b||!b->isValid()) {if(sameHandle(manualVehicle,d->first))endManual();stopDrive(drive);drive.occupant.setNull();drive.restoreOccupant.setNull();continue;}
        Character* currentDriver=drive.occupant.getCharacter();
        if(ou->isPaused() && drive.streamPending)drive.streamAt=GetTickCount();
        if(!drive.streamPending && !drive.occupant.isNull() && (!currentDriver || !currentDriver->isValid()))suspendDrive(b);
        if(!b->isPhysical() || !b->entitiesLoaded || !b->getRootNode()) {
            suspendDrive(b);drive.physicsSeen=GetTickCount();
            const DWORD tick=GetTickCount();
            const bool occupied=!drive.occupant.isNull() || !drive.restoreOccupant.isNull();
            ZoneMap* zone=vehicleZone(b);
            const bool needed=vehicleNeedsPhysicalRestore(occupied,
                zone && zone->isActivationType(ACTIVATION_CAMERA),
                zone && zone->isActivationType(ACTIVATION_PLAYER_CHARACTER));
            if(needed && !drive.streamNeeded){drive.streamAt=drive.streamRetryAt=tick;drive.streamAttempts=0;}
            drive.streamNeeded=needed;
            const double elapsed=(tick-drive.streamAt)*.001;
            if(needed) {
                if(occupied && vehicleStreamExpired(elapsed)) {
                    clearSeatClaim(b);messages[d->first]="Vehicle reload failed. Driver released.";
                    ErrorLog("Project Cars: physical restore timed out; released driver / "+d->first.toString());
                } else {
                    if(vehicleStreamRetry(zone && zone->isLoadedMT(),drive.streamAttempts,(tick-drive.streamRetryAt)*.001,elapsed)) {
                        ++drive.streamAttempts;drive.streamRetryAt=tick;
                        DebugLog("Project Cars: physical restore at current vehicle zone / "+d->first.toString()+(occupied?" / occupied":" / parked"));
                        if(!b->isPhysical())b->_NV_createPhysical();
                    }
                }
            }
            continue;
        }
        if(drive.exitRequested && drive.body.speed()<=2) {
            drive.exitRequested=false;
            interactWithVehicle(b,drive.occupant.getCharacter(),LeaveVehicle);
        }
        if(ou->isPaused() || drive.streamPending)drive.physicsSeen=GetTickCount();
        if(drive.body.ignition && !ou->isPaused() && GetTickCount()-drive.physicsSeen>2000) {
            disableDrive(drive,CallbackDriveFault);messages[d->first]="Physics synchronization unavailable. Press R to retry.";
            ErrorLog("Project Cars: physics update is not reaching the game thread / "+d->first.toString());
        }
        if(drive.streamPending || !drive.restoreOccupant.isNull()) {
            if(!drive.streamAt)drive.streamAt=GetTickCount();
            const hand who=drive.restoreOccupant.isNull()?drive.occupant:drive.restoreOccupant;
            if(who.isNull()){drive.streamPending=false;drive.stamp=-1;}
            else if(!restoreDriverSeat(b,drive,who,now)) {
                if(!vehicleStreamExpired((GetTickCount()-drive.streamAt)*.001))continue;
                clearSeatClaim(b);messages[d->first]="Driver could not be restored. Seat released; select Drive to board again.";
            }
        }
        // The native operator set can be replaced without destroying Building.
        // Reclaim only an acknowledged driver still beside the current seat.
        if(!drive.occupant.isNull()) {
            UseableStuff* seat=b->getUseableStuff();
            if(!seat || !sameHandle(seat->getOccupant(),drive.occupant)) {
                const hand who=drive.occupant;
                if(!restoreDriverSeat(b,drive,who,now)){clearSeatClaim(b);messages[d->first]="Driver seat released. Drive is available to retry.";}
            }
        }
        if(sameHandle(drive.occupant,ou->player->selectedCharacter) && !drive.occupant.isNull()) {
            std::ostringstream text;text.precision(1);text.setf(std::ios::fixed);
            const bool manual=sameHandle(manualVehicle,d->first);
            text<<(manual?"MANUAL DRIVING":"DRIVING")<<" / "<<(manual?"WASD":drive.driver.pace<.5?"CAUTIOUS":drive.driver.pace<.9?"CRUISE":"FAST")<<((manual?drive.manual.reversing:drive.driver.reversing)?" / REVERSE":"")<<"\n"<<(drive.body.ignition?"ENGINE ON":"ENGINE OFF")<<"   "<<drive.body.speed()*.36<<" km/h   "<<drive.body.fuel<<" L\n"<<messages[d->first];hud=text.str();
        }
        try {updateVehicleLights(b);}catch(const std::exception& e){drive.headlightsOn=false;ErrorLog(std::string("Project Cars: headlights disabled: ")+e.what());}
    }
    cars::drivingStatus(hud);
    if(!manualVehicle.isNull() && !drives.count(manualVehicle))endManual();
    for(std::map<hand,SeatRequest>::iterator s=seatRequests.begin();s!=seatRequests.end();) {
        Building* b=s->first.getBuilding();Character* actor=s->second.actor.getCharacter();UseableStuff* seat=b && b->isValid()?b->getUseableStuff():0;
        if(drives.count(s->first) && drives[s->first].streamPending){s->second.stamp=now;++s;continue;}
        if(b && (!b->isPhysical() || !b->entitiesLoaded)){s->second.stamp=now;++s;continue;}
        if(!seat || !actor || !actor->isValid()){if(b)finishBoarding(b,s->second,true);seatRequests.erase(s++);continue;}
        SeatRequest& request=s->second;CharMovement* movement=actor->getMovement();OrdersReceiver* orders=actor->getOrdersReciever();
        if(request.interrupted || !interactionActor(b,actor,request.acknowledged) || !movement || !orders) {
            finishBoarding(b,request,request.interrupted);messages[s->first]="Boarding stopped. Driver seat available.";seatRequests.erase(s++);continue;
        }
        double elapsed=TimeOfDay(now-request.stamp).getRealLifeSeconds();request.stamp=now;
        if(ou->isPaused()){++s;continue;}
        if(request.approaching) {
            double distance=actor->getPosition().distance(request.entry);
            if(distance<=3.0) {
                orders->clearOrders();orders->clearCurrentGoal(true);movement->halt();
                Ogre::Vector3 position=rigWorld(b,driverAnchor(b));
                Ogre::Quaternion direction=b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y);
                movement->_setPositionDirectionAndTeleport(position,direction);request.approaching=false;request.enterTime=0;
                // Claim the native seat directly after reaching the door. The
                // walking AI must never pathfind again to a point inside the car.
                if(seat->_NV_tryOperate(request.actor) && sameHandle(seat->getOccupant(),request.actor)) {
                    driveState(b).occupant=request.actor;request.acknowledged=true;
                    issuingSeatOrder=true;try{orders->addOrder(HOLD_POSITION,hand(),position,true,false);}catch(...){issuingSeatOrder=false;throw;}issuingSeatOrder=false;
                    if(actor->getAnimationClass())actor->getAnimationClass()->playAction("sitting chair",1,1,false);
                    messages[s->first]="Driving mode. Start engine, then right-click terrain.";
                    DebugLog("Project Cars: reached boarding point; native driver seat confirmed / "+request.actor.toString());
                } else {
                    finishBoarding(b,request,false);messages[s->first]="Seat entry failed. Drive is available to retry.";
                    seatRequests.erase(s++);continue;
                }
            } else if(!request.progress.advance(elapsed,distance) || (request.progress.elapsed>1 && movement->pathFailed())) {
                finishBoarding(b,request,false);messages[s->first]="Boarding path blocked. Drive is available to retry.";
                DebugLog("Project Cars: boarding approach failed; reservation released");seatRequests.erase(s++);continue;
            }else {++s;continue;}
        }
        if(drives.count(s->first) && sameHandle(drives[s->first].occupant,s->second.actor)) {
            if(!s->second.acknowledged)DebugLog("Project Cars: driver seated / "+s->second.actor.toString()+" / "+s->first.toString());
            s->second.acknowledged=true;++s;
        }else {
            request.enterTime+=elapsed;
            if(request.acknowledged || request.enterTime>=5) {
                finishBoarding(b,request,false);messages[s->first]="Seat entry ended. Driver seat available.";
                DebugLog("Project Cars: seat entry ended; returned to boarding point and released reservation");seatRequests.erase(s++);
            }else ++s;
        }
    }
    for(std::map<hand,Job>::iterator i=jobs.begin();i!=jobs.end();) {
        Job& job=i->second;Building* b=i->first.getBuilding();Character* actor=job.actor.getCharacter();
        OrdersReceiver* orders=actor && actor->isValid()?actor->getOrdersReciever():0;
        const bool ready=eligible(b,actor) && orders;
        Ogre::Vector3 drift=ready?actor->getPosition()-job.origin:Ogre::Vector3::ZERO;
        bool partAvailable=true;
        if(ready && !job.request.remove){GameData* def=ou->gamedata.getData(job.request.item,ITEM);partAvailable=def && actor->getInventory() && actor->getInventory()->countItems(def)>0;}
        // hasPlayerOrder examines only the queued orders, not the active task.
        // A consumed HOLD_POSITION command must not cancel the fitting timer.
        if(!fittingWorkAllowed(ready,job.orderReplaced,partAvailable,drift.x,drift.z)) {
            const char* reason=job.orderReplaced?"new character order":!ready?"character or vehicle no longer eligible":!partAvailable?"carried component missing":"character moved from work position";
            std::ostringstream detail;detail<<"Project Cars: work interrupted / "<<reason<<" / "<<job.request.slot
                <<" / drift="<<drift.x<<","<<drift.y<<","<<drift.z;
            if(orders)detail<<" / holdQueued="<<orders->hasPlayerOrder(HOLD_POSITION)<<" / goal="<<orders->getCurrentGoal().key();
            DebugLog(detail.str());if(workPlans.count(i->first))workPlans[i->first].queue.pause(job.work.done);finishActivity(job);
            messages[i->first]=std::string("Queue paused: ")+reason+". Progress retained.";jobs.erase(i++);continue;
        }
        if(!ou->isPaused()) {
            if(actor->getMovement())actor->getMovement()->lookatPosition(b->getPosition());
            AnimationClass* anim=actor->getAnimationClass();
            if(anim && anim->getIsActivated() && anim->hasAnimation(workAnimation) && !anim->isAnimationPlaying(workAnimation)) {
                anim->playAction(workAnimation,1.0f,1.0f,false);job.animated=true;
            }
        }
        double elapsed=TimeOfDay(now-job.stamp).getRealLifeSeconds();job.stamp=now;
        if(!ou->isPaused() && job.work.advance(elapsed,skills[actor->getStats()].mechanicsXp,true)) {
            std::string result=commitFit(b,actor,job.request);messages[i->first]=result;
            DebugLog("Project Cars: work result "+job.request.slot+" / "+job.request.item+" / "+result);
            if(result=="Installed" || result=="Removed") {
                // A successful transfer updates the visible mount in this same
                // main-thread tick; collider reconciliation follows post physics.
                try {updateFittedVisuals(b);}catch(const std::exception& e){ErrorLog(std::string("Project Cars: fitted model refresh pending: ")+e.what());}
                skills[actor->getStats()].mechanicsXp=validVehicleXp(skills[actor->getStats()].mechanicsXp+job.work.required*2);
                if(workPlans.count(i->first)){workPlans[i->first].queue.completed();if(workPlans[i->first].queue.steps.empty())messages[i->first]="Queue complete.";}
            } else if(workPlans.count(i->first))workPlans[i->first].queue.pause(job.work.done);
            finishActivity(job);jobs.erase(i++);
        } else ++i;
    }
    for(std::map<hand,WorkPlan>::iterator p=workPlans.begin();p!=workPlans.end();++p) {
        if(!p->second.queue.running || jobs.count(p->first))continue;
        Building* b=p->first.getBuilding();if(!b || !b->isValid()){p->second.queue.running=false;continue;}
        messages[p->first]=startNextQueued(b,p->second);
    }
    // Keep the pose registered while parked or paused, including first entry
    // and save restoration before a physics step has run.
    if(driverPoseReady)prepareDriverGrips();
    // Rotate a bounded list of known loaded vehicles, including restored saves.
    static DWORD visualAt=0;static hand visualCursor;
    if(GetTickCount()-visualAt>=250) {
        visualAt=GetTickCount();std::map<hand,Inventory*>::iterator it=stores.upper_bound(visualCursor);
        if(it==stores.end())it=stores.begin();
        for(unsigned n=0;n<4 && it!=stores.end();++n,++it) {
            visualCursor=it->first;Building* b=it->first.getBuilding();
            if(b) {try {updateFittedVisuals(b);}catch(const std::exception& e){
                static DWORD errorAt=0;if(!errorAt || GetTickCount()-errorAt>10000){errorAt=GetTickCount();ErrorLog(std::string("Project Cars fitted visuals: ")+e.what());}
            }}
        }
    }
}
void updateFittedVisuals(Building* b) {
    VehicleFaultScope trace("fitted model reconciliation");
    if(!frame(b) || !b->isPhysical() || !b->entitiesLoaded)return;
    Building::ConstructionState* state=b->getBuildState_ActualNonShared();
    if(!state || !state->isComplete || state->isDismantled)return;
    Lock lock;Inventory* inv=lookup(b);if(!inv)return;
    Ogre::SceneNode* root=b->getRootNode();if(!root)return;
    const std::map<std::string,std::string> fitted=contents(inv);
    std::map<std::string,const WorkshopVisual*> wanted;
    // Completed frames travel as ordinary dynamic entities, just like fitted
    // modules. Native building instancing can retain the old batch bounds.
    size_t visualCount=0;
    const WorkshopVisual* visuals=frameVisuals(b,visualCount);
    for(size_t i=0;i<visualCount;++i)if(std::string(visuals[i].component)=="Frame")wanted[std::string(nodePrefix)+"Frame"]=&visuals[i];
    for(size_t i=0;i<workshopMountCount;++i) {
        const WorkshopSlot& s=workshopMounts[i];std::map<std::string,std::string>::const_iterator f=fitted.find(s.id);if(f==fitted.end() || !::fitted(b,s.id))continue;
        std::string component=workshopComponent(s,f->second);
        for(size_t j=0;j<visualCount;++j)if(component==visuals[j].component)wanted[std::string(nodePrefix)+component]=&visuals[j];
    }
    Ogre::SceneManager* scene=root->getCreator();
    for(size_t n=root->numChildren();n>0;--n) {
        Ogre::SceneNode* child=static_cast<Ogre::SceneNode*>(root->getChild(n-1));std::string name=child->getName();
        if(name.compare(0,strlen(nodePrefix),nodePrefix)!=0)continue;
        if(wanted.erase(name)){child->setVisible(b->getVisible(),true);continue;}
        while(child->numAttachedObjects()){Ogre::MovableObject* obj=child->getAttachedObject(static_cast<size_t>(0));child->detachObject(obj);scene->destroyEntity(static_cast<Ogre::Entity*>(obj));}
        scene->destroySceneNode(child);
    }
    updateFrameDents(b);
    if(wanted.empty()){hideBatchedFrame(b);return;}
    GameData* part=ou->gamedata.getData(motorcycleFrame(b)?"211-Project Cars.mod":xsmallFrame(b)?"201-Project Cars.mod":"13-Project Cars.mod",BUILDING_PART);
    GameData* material=part?b->selectPartMaterial(part):0;
    if(!material)throw std::runtime_error("Frame material definition unavailable for fitted meshes");
    // Buildings may render through InstancedEntity. Ask Kenshi for the ordinary
    // entity material variant instead of copying an instancing shader or relying
    // on an Entity child being present under the building root.
    for(std::map<std::string,const WorkshopVisual*>::const_iterator i=wanted.begin();i!=wanted.end();++i) {
        const WorkshopVisual& v=*i->second;
        Ogre::Entity* ent=0;Ogre::SceneNode* child=0;
        try {
            const bool track=std::string(v.component)=="Track_L" || std::string(v.component)=="Track_R";
            if(track) {
                try {
                    std::string animated=v.mesh;animated.insert(animated.find_last_of('.'),"_Animated");
                    ent=scene->createEntity(registerMesh(animated),"ProjectCarsVehicleParts");
                    Building::setBuildingPartMaterial(ent,material,false,false,b->getPosition());rigTrackMaterial(ent);
                } catch(const std::exception& e) {
                    if(ent){scene->destroyEntity(ent);ent=0;}ErrorLog(std::string("Project Cars: using static track fallback: ")+e.what());
                }
            }
            if(!ent){std::string mesh=registerMesh(v.mesh);if(std::string(v.component)=="Frame")try{mesh=prepareDentedMesh(b,mesh);}catch(const std::exception& e){ErrorLog(std::string("Project Cars: cosmetic dent fallback: ")+e.what());}ent=scene->createEntity(mesh,"ProjectCarsVehicleParts");Building::setBuildingPartMaterial(ent,material,false,false,b->getPosition());}
            ent->setQueryFlags(0);
            child=root->createChildSceneNode(Ogre::SCENE_DYNAMIC,Ogre::Vector3(v.x,v.y,v.z));
            child->setName(i->first);child->attachObject(ent);child->setVisible(b->getVisible(),true);
            if(std::string(v.component)=="Frame")hideBatchedFrame(b);
            DebugLog("Project Cars: fitted mesh attached "+std::string(v.component)+" / "+b->getHandle().toString());
        } catch (...) {
            if(child){child->detachAllObjects();scene->destroySceneNode(child);}
            if(ent)scene->destroyEntity(ent);throw;
        }
    }
}
bool installFittingRuntime() {
    InitializeCriticalSection(&mutex);
    installVehicleFaultTrace();
    if(!installBloodPickups())ErrorLog("Project Cars: native blood decal contract unavailable; ordinary tread marks retained");
    nativeNearbyRelease=reinterpret_cast<cars::NativeQueryRelease>(GetProcAddress(GetModuleHandleW(L"mfc100u.dll"),MAKEINTRESOURCEA(266)));
    if(!nativeNearbyRelease){ErrorLog("Project Cars: native query allocator unavailable; ramming scans disabled");}
    bool ok=installVehicleTurret();
    ok=installNativeVehiclePaths() && ok;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_hitByMeleeAttack),vehicleMelee,&originalVehicleMelee)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::updateWorldAABB),vehicleUpdateWorldAabb,&originalUpdateWorldAabb)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::recalculateWorldAABB),vehicleRecalculateWorldAabb,&originalRecalculateWorldAabb)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV__saveBuildingState),save,&originalSave)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV__loadBuildingState),load,&originalLoad)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_getZoneMapLocation),vehicleZone,&originalBuildingZone)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_loadUnloadCheck),vehicleLoadUnload,&originalBuildingStream)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&ZoneMap::update),vehicleZoneUpdate,&originalZoneUpdate)==KenshiLib::SUCCESS;
    bool (GameWorld::*objectDestroy)(RootObject*,bool,const char*)=&GameWorld::destroy;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(objectDestroy),vehicleObjectDestroy,&originalObjectDestroy)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_destroyPhysical),destroyPhysical,&originalDestroyPhysical)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_canDismantle),canDismantle,&originalCanDismantle)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_confirmDismantle),confirmDismantle,&originalConfirmDismantle)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_DESTRUCTOR),destroy,&originalDestroy)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&CharStats::serialise),skillSave,&originalSkillSave)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&CharStats::updateStats),skillLoad,&originalSkillLoad)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&CharStats::_NV_init),skillInit,&originalSkillInit)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&CharStats::_DESTRUCTOR),skillDestroy,&originalSkillDestroy)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&OrdersReceiver::addOrder),playerOrder,&originalOrder)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_couldIOperate),couldOperate,&originalCouldOperate)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_tryOperate),tryOperate,&originalTryOperate)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_getPositionMarker),positionMarker,&originalPositionMarker)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_getDirectionMarker),directionMarker,&originalDirectionMarker)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&Building::_NV_getDirectionMarkerQuat),directionQuat,&originalDirectionQuat)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&ForgottenGUI::showInventoryBuilding),showBuildingInventory,&originalShowBuildingInventory)==KenshiLib::SUCCESS;
    ok=ok && KenshiLib::AddHook(KenshiLib::GetRealAddress(&PhysicsActual::_NV_updateUT),physicsUpdate,&originalPhysicsUpdate)==KenshiLib::SUCCESS;
    driverPoseReady=installDriverPose();if(!driverPoseReady)ErrorLog("Project Cars: seated driver render hook unavailable");
    if(vehicleAudio.initialize())DebugLog("Project Cars: V8 sound bank loaded (25 clips)");else ErrorLog("Project Cars: V8 sound bank missing or invalid; using synthesized engine fallback");
    enabled=ok;return ok;
}
}
