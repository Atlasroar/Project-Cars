#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/Character.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/CameraClass.h>
#include <kenshi/InputHandler.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Inventory.h>
#include <kenshi/Item.h>
#include <kenshi/util/UtilityT.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <ogre/OgreSceneManager.h>
#include <mygui/MyGUI.h>
#include <map>
#include <sstream>
#include <cctype>
#include "NativeRuntime.h"
#include "OilProduction.h"
#include "compat/HandleIdentity.h"
#include "../src/NativePolicy.h"
#include "../src/DebugPlacement.h"
#include "DebugItems.generated.h"
#include "VehicleWorkshop.h"
#include "VehicleDebug.h"
#include "FittingRuntime.h"
#include "../src/WorkshopData.generated.h"

namespace {
const cars::Catalog* catalog = 0;
std::map<hand, cars::Vehicle*> vehicles;
cars::DebugKeyPress debugKey;
hand pendingChest, pendingModify, workshopHandle, lastDebugChest;
hand pendingOilPool,pendingOilActor;
hand pendingActor;
cars::VehicleClick pendingClick=cars::ConfigureVehicle;
cars::WorkshopSnapshot workshopData;
unsigned long inventoryReadAt=0;
unsigned long chestStarted = 0, lastScan = 0;
unsigned long chestCheckedAt=0;
bool closeRequested = false, readyLastFrame = false, faulted = false;
volatile LONG worldEpoch=0, resetDepth=0;
LONG observedEpoch=0;
bool firstTick=true, firstReady=true;
cars::VehicleEscape escape;
bool pendingMenuAction=false;
hand vehicleMenuTarget;
cars::VehicleRightPress rightPress;
hand pressedVehicle;
void (*originalKeyDown)(InputHandler*,OIS::KeyCode)=0;
void (*originalKeyUp)(InputHandler*,OIS::KeyCode)=0;
void (*originalTick)(GameWorld*, float) = 0;
void (*originalReset)(GameWorld*) = 0;
void (*originalWorldReset)(GameWorld*) = 0;
void (*originalPlayerLoad)(PlayerInterface*,GameData*)=0;
volatile LONG debugInventoryCleanup=0;
volatile LONG debugLoadCleanup=0;
void (*originalContext)(ContextMenu*, bool, RootObject*) = 0;
void (*originalTask)(PlayerInterface*, TaskType, const hand&, Building*, const Ogre::Vector3&, bool) = 0;
bool inputAllowed();
bool debugInputAllowed();
void resetVehicleDebug();
void updateRightPress();
void context(ContextMenu*,bool,RootObject*);
bool vehicleControlDown(InputHandler*,OIS::KeyCode);
bool vehicleControlUp(OIS::KeyCode);
void updateVehicleControls();
bool vehicleOptionsOpen();

void log(const std::string& text) { DebugLog("Project Cars: " + text); }
void tell(const std::string& text) {
    log(text);
    cars::vehicleDebugStatus(text);
}
bool worldReady() { return InterlockedCompareExchange(&resetDepth,0,0)==0 && ou && ou->initialized && !ou->gameResetting && !ou->isLoadingFromASaveGame() && ou->player; }
// Narrow SEH guards keep reverse-engineered calls separate from STL unwinding.
cars::ConstructionObservation construction(Building* building) {
    __try {
        if (!building || !building->isValid()) return cars::ConstructionUnknown;
        Building::ConstructionState* state = building->getBuildState_ActualNonShared();
        return cars::observeConstruction(state != 0, state && state->isComplete, state && state->isDismantled);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return cars::ConstructionUnknown; }
}
Building* resolveBuilding(const hand& h) {
    __try { return h.isValid() ? h.getBuilding() : 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool componentDebugChest(const hand& h) {
    Building* b=resolveBuilding(h);
    return b && b->isValid() && b->getGameData() && cars::isComponentDebugChest(b->getGameData()->stringID,b->getName());
}
void playerLoad(PlayerInterface* player,GameData* data) {
    // Player load also runs on save-load paths that inline world cleanup.
    // Cancel debug commands on the UI thread even when reset() was bypassed.
    InterlockedExchange(&debugLoadCleanup,1);
    // Native PlayerInterface::loadFromSerialise restores selected_gui_object.
    // Do not restore our developer chest as the selected UI object. Preserve
    // the in-memory save record and every ordinary inventory/character choice.
    hand selected;const bool suppress=data && data->getHandle(selected,"selected_gui_object") && componentDebugChest(selected);
    log("player UI restore selected="+selected.toString()+" debugChest="+(suppress?"yes":"no"));
    if(suppress)data->storeHandle(hand(),"selected_gui_object",false);
    try {originalPlayerLoad(player,data);}catch(...) {
        if(suppress)data->storeHandle(selected,"selected_gui_object",false);throw;
    }
    if(suppress){data->storeHandle(selected,"selected_gui_object",false);log("debug chest GUI selection omitted during save restore");}
    InterlockedExchange(&debugInventoryCleanup,1);
}
void closeRestoredDebugInventory() {
    if(!::gui || !InterlockedExchange(&debugInventoryCleanup,0))return;
    // The previous world's window may still be open during an in-session load.
    // Copy handles before closing: closeInventory mutates the window registry.
    hand close[32];unsigned count=0,visited=0;
    for(ogre_unordered_map<hand,InventoryGUI*>::type::const_iterator i=::gui->inventoryWindowsOpen.begin();
        i!=::gui->inventoryWindowsOpen.end() && visited<128 && count<32;++i,++visited)
        if(componentDebugChest(i->first))close[count++]=i->first;
    for(unsigned i=0;i<count;++i){::gui->closeInventory(close[i]);log("closed restored debug chest inventory");}
    std::ostringstream detail;detail<<"debug inventory restore cleanup: "<<count<<" windows closed";log(detail.str());
}
bool ownFrame(Building* b) {
    return worldReady() && b && b->isValid() && b->getDataType() == BUILDING &&
        b->getFaction() && b->getFaction() == ou->player->getFaction() && b->getGameData() &&
        catalog->frameForBuilding(b->getGameData()->stringID);
}
cars::Vehicle* adopt(Building* b) {
    if (!ownFrame(b) || construction(b) != cars::ConstructionComplete) return 0;
    const hand h = b->getHandle();
    std::map<hand,cars::Vehicle*>::iterator old = vehicles.find(h);
    if (old != vehicles.end()) return old->second;
    if (vehicles.size() >= 1024) return 0;
    const cars::FrameDef* frame = catalog->frameForBuilding(b->getGameData()->stringID);
    std::ostringstream id; id << "native_" << h.container << '_' << h.containerSerial << '_' << h.index << '_' << h.serial;
    cars::Vehicle* vehicle = new cars::Vehicle(*catalog, frame->id, id.str());
    const Ogre::Vector3 p = b->getPosition();
    cars::Pose pose; pose.x=p.x; pose.y=p.y; pose.z=p.z;
    if (vehicle->adoptCompletedBuilding(b->getGameData()->stringID, cars::ConstructionComplete, pose) != cars::Ok) {
        delete vehicle; return 0;
    }
    try { vehicles.insert(std::make_pair(h,vehicle)); } catch (...) { delete vehicle; throw; }
    log("completed frame registered " + id.str() + " -> " + frame->id);
    return vehicle;
}
void closePanel() {
    cars::closeWorkshop(); workshopHandle.setNull();
    workshopData=cars::WorkshopSnapshot();inventoryReadAt=0;
    closeRequested = false;
}
void clearRecoveryUI();
void clearWorld() {
    resetVehicleDebug();
    clearRecoveryUI();
    cars::cancelAllFittingWork();
    for (std::map<hand,cars::Vehicle*>::iterator i=vehicles.begin();i!=vehicles.end();++i) delete i->second;
    vehicles.clear(); pendingChest.setNull(); lastDebugChest.setNull(); pendingModify.setNull();pendingActor.setNull();pendingOilPool.setNull();pendingOilActor.setNull();closePanel();
    lastScan=0;vehicleMenuTarget.setNull();pendingMenuAction=false;debugKey.reset();
    rightPress=cars::VehicleRightPress();pressedVehicle.setNull();
}
#include "VehicleRecoveryUI.inl"
void keyDown(InputHandler* input,OIS::KeyCode code) {
    if(cars::vehicleRecoveryActive()) {
        if(code==OIS::KC_ESCAPE){escape.down(true);input->escape=input->escape_msg=false;recoveryCancel=true;return;}
        if(code==OIS::KC_Q || code==OIS::KC_E){recoveryTurn=code==OIS::KC_Q?-1:1;return;}
    }
    if(vehicleControlDown(input,code))return;
    if(code==OIS::KC_F12)debugKey.press(!faulted && readyLastFrame && debugInputAllowed(),input->ctrl && !input->shift && !input->alt);
    if(code==OIS::KC_ESCAPE && escape.down(cars::vehicleDebugVisible() || cars::workshopVisible() || (worldReady() && !vehicleOptionsOpen() && cars::manualDriving()))) {
        input->escape=false;input->escape_msg=false;
        if(cars::vehicleDebugVisible())cars::requestVehicleDebugClose();
        else if(cars::workshopVisible())closeRequested=true;
        else {cars::endManualDriving();if(worldReady())ou->hideContextMenu();}
        return;
    }
    originalKeyDown(input,code);
}
void keyUp(InputHandler* input,OIS::KeyCode code) {
    if(vehicleControlUp(code))return;
    if(code==OIS::KC_F12)debugKey.release();
    if(code==OIS::KC_ESCAPE && escape.up()){input->escape=false;input->escape_msg=false;return;}
    originalKeyUp(input,code);
}
int countInventory(Inventory* inventory,GameData* def) {
    __try {return inventory && def?inventory->countItems(def):-1;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return -1;}
}
std::string itemForDefinition(const std::string& id) {
    if(id=="small_gas_engine")return "21-Project Cars.mod";
    if(id=="road_wheel")return "138-Project Cars.mod";
    if(id=="tank_40")return "135-Project Cars.mod";
    if(id=="basic_seat")return "136-Project Cars.mod";
    if(id=="light_armor")return "143-Project Cars.mod";
    if(id=="heavy_armor")return "142-Project Cars.mod";
    if(id=="headlights")return "139-Project Cars.mod";
    if(id=="cargo_rack")return "131-Project Cars.mod";
    return "";
}
cars::WorkshopSnapshot snapshot(Building* b,const cars::Vehicle& vehicle) {
    cars::WorkshopSnapshot result;
    result.vehicleName=b->getName();result.status=cars::message(vehicle.readiness());result.fuel=vehicle.fuelLitres();
    for(std::map<std::string,std::string>::const_iterator i=vehicle.installed().begin();i!=vehicle.installed().end();++i) {
        std::string item=itemForDefinition(i->second);if(!item.empty())result.installed[i->first]=item;
        if(i->first=="tank") {std::map<std::string,cars::PartDef>::const_iterator def=catalog->parts.find(i->second);
            if(def!=catalog->parts.end())result.tankCapacity=def->second.capacity;}
    }
    Character* actor=ou->player->selectedCharacter.getCharacter();
    cars::fittingSnapshot(b,actor,result);
    if(!actor || !actor->isValid() || !actor->isPlayerCharacter())return result;
    result.actorId=actor->getHandle().toString();
    result.actorName=actor->getName();Inventory* inv=actor->getInventory();if(!inv)return result;
    result.inventoryReadable=true;
    for(size_t i=0;i<cars::workshopItemCount;++i) {
        const char* id=cars::workshopItems[i].id;
        GameData* def=ou->gamedata.getData(id,ITEM);int count=countInventory(inv,def);
        result.inventory[id]=count;
    }
    return result;
}
void openModify(Building* b) {
    cars::Vehicle* vehicle = adopt(b);
    if (!vehicle) return;
    workshopHandle=b->getHandle();workshopData=snapshot(b,*vehicle);inventoryReadAt=GetTickCount();
    cars::showWorkshop(workshopHandle.toString(),workshopData);
    log("Modify opened for " + vehicle->id());
}
void updateWorkshop() {
    if(!cars::workshopVisible())return;
    if(GetTickCount()-inventoryReadAt>=250) {
        Building* b=resolveBuilding(workshopHandle);
        std::map<hand,cars::Vehicle*>::const_iterator found=vehicles.find(workshopHandle);
        if(!ownFrame(b) || construction(b)!=cars::ConstructionComplete || found==vehicles.end()) {closePanel();return;}
        workshopData=snapshot(b,*found->second);inventoryReadAt=GetTickCount();
    }
    cars::updateWorkshop(workshopData);
    cars::WorkshopRequest request;
    if(cars::takeWorkshopRequest(request)) {
        Building* b=resolveBuilding(workshopHandle);
        Character* actor=ou->player->selectedCharacter.getCharacter();
        if(ownFrame(b) && actor && actor->isValid()) {
            if(request.slot=="rename") {
                std::string name=request.item;if(name.size()>40)name.resize(40);
                if(!name.empty()){b->setName(name);b->notifyChange();b->instanceID.notifyChange();}
            } else if(cars::fitComponent(b,actor,request)=="Work started.")closePanel();
        }
        inventoryReadAt=0;
    }
}
bool inputAllowed() {
    DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(), &process);
    if (process != GetCurrentProcessId() || !worldReady() || ou->player->isLevelEditMode() || ou->player->isObjectPlacementMode()) return false;
    MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();
    if (!input || input->isModalAny()) return false;
    for (MyGUI::Widget* focus=input->getKeyFocusWidget(); focus; focus=focus->getParent())
        if (focus->isType<MyGUI::EditBox>()) return false;
    return true;
}
#include "VehicleControls.inl"
Building* createChestGuarded(GameWorld* world, GameData* data, const Ogre::Vector3& p, Character* actor, bool* failed) {
    __try {
        return world->theFactory->createBuilding(data,p,actor->getCurrentTownLocation(),actor->getFaction(),
            Ogre::Quaternion::IDENTITY,0,0,0,0,0,false,true,false,0,true);
    } __except(EXCEPTION_EXECUTE_HANDLER) { *failed=true; return 0; }
}
struct ChestVisualState {
    bool physical,visible,node,loaded,inventory;
    Ogre::Vector3 position;
    ChestVisualState():physical(false),visible(false),node(false),loaded(false),inventory(false),position(Ogre::Vector3::ZERO) {}
    bool ready() const {return physical && visible && node && loaded && inventory;}
};
bool readChestVisuals(Building* chest,ChestVisualState* state) {
    __try {
        state->position=chest->getPosition();state->physical=chest->isPhysical();
        state->visible=chest->getVisible();state->node=chest->getRootNode()!=0;
        state->loaded=chest->entitiesLoaded;state->inventory=chest->getInventory()!=0;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool prepareChestVisuals(Building* chest) {
    __try {
        // Factory construction creates logical inventory before physical/mesh
        // loading. Explicitly request the idempotent native physical path once.
        if (!chest->getTown()) return false;
        if (!chest->isPhysical()) chest->createPhysical();
        chest->setVisible(true);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {faulted=true;return false;}
}
void logChestVisuals(Building* chest,const char* phase,const ChestVisualState& state) {
    std::ostringstream detail;detail<<"debug chest "<<phase<<" handle="<<chest->getHandle().toString()
        <<" actual="<<state.position.x<<','<<state.position.y<<','<<state.position.z
        <<" physical="<<state.physical<<" visible="<<state.visible<<" root="<<state.node
        <<" meshesLoaded="<<state.loaded<<" inventory="<<state.inventory;
    log(detail.str());
}
bool insertGuarded(Inventory* inv, Item* item, bool* failed) {
    __try { return inv->addItem(item,1,false,true); }
    __except(EXCEPTION_EXECUTE_HANDLER) { *failed=true; return false; }
}
Item* createItemGuarded(RootObjectFactory* factory,GameData* def,const hand& h,bool* failed) {
    __try { return factory->createItem(def,h,0,0,-1,0); }
    __except(EXCEPTION_EXECUTE_HANDLER) { *failed=true; return 0; }
}
// Main-thread, bounded local probes. findValidSpawnPos randomizes across zones;
// it is not a validator for the requested point (Steam 1.0.65 disassembly).
int chestGroundGuarded(GameWorld* world,Ogre::Vector3& p,float actorHeight) {
    __try {
        const float corners[5][2]={{0,0},{-0.7f,-0.7f},{0.7f,-0.7f},{-0.7f,0.7f},{0.7f,0.7f}};
        float heights[5];
        for (int i=0;i<5;++i) {
            Ogre::Vector3 sample(p.x+corners[i][0],p.y,p.z+corners[i][1]);
            heights[i]=UtilityT::getTerrainHeightFromRenderer(sample,false,0);
            if (UtilityT::getPositionInWater(sample.x,sample.z)) return 2;
        }
        if (!cars::debugChestGround(actorHeight,heights)) return 1;
        p.y=heights[0];
        // Despite its name this engine method tests a sphere, ignoring length
        // and skip. Lift its center so the footprint does not overlap terrain.
        Ogre::Vector3 clearance(p.x,p.y+1.35f,p.z);
        if (world->buildingIntersectionTestCapsule(clearance,1.0f,2.0f,0)) return 3;
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 4; }
}
bool findChestGround(const Ogre::Vector3& center,Ogre::Vector3& result) {
    const float directions[8][2]={{1,0},{0,1},{-1,0},{0,-1},{0.707107f,0.707107f},{-0.707107f,0.707107f},{-0.707107f,-0.707107f},{0.707107f,-0.707107f}};
    unsigned reasons[5]={0,0,0,0,0};
    for (int ring=0;ring<2;++ring) for (int i=0;i<8;++i) {
        const float distance=ring==0?4.0f:8.0f;
        Ogre::Vector3 candidate(center.x+directions[i][0]*distance,center.y,center.z+directions[i][1]*distance);
        int reason=chestGroundGuarded(ou,candidate,center.y);
        ++reasons[reason];
        if (reason==4) {faulted=true;tell("Chest ground check failed; Project Cars disabled for this session. See the log.");return false;}
        if (reason==0) {
            result=candidate;
            std::ostringstream detail;detail<<"debug chest local ground accepted at "<<result.x<<','<<result.y<<','<<result.z<<"; radius "<<distance;
            log(detail.str());return true;
        }
    }
    std::ostringstream detail;detail<<"debug chest local probes rejected: terrain/slope="<<reasons[1]<<", water="<<reasons[2]<<", obstruction="<<reasons[3];
    log(detail.str());
    tell("No suitable chest spot within 8 metres: uneven ground, water or an obstruction. See the log for details.");
    return false;
}
void requestChest() {
    if (!pendingChest.isNull()) { tell("A component chest is still being prepared."); return; }
    Building* existing=resolveBuilding(lastDebugChest);
    if (existing) {
        ChestVisualState state;
        if (readChestVisuals(existing,&state) && state.ready()) {
            ou->player->focusCamera(state.position);ou->player->selectObject(existing,false);
            tell("Selected your existing component chest. Dismantle it before spawning a fresh one.");
        } else tell("The previous debug chest has not finished loading. No duplicate was created; see the log.");
        return;
    }
    Character* actor = ou->player->selectedCharacter.getCharacter();
    if (!actor || !actor->isValid() || !actor->isPlayerCharacter()) { tell("Select one of your characters to spawn the component chest."); return; }
    if (!actor->isIndoors().isNull()) { tell("Move outside before spawning the component chest."); return; }
    GameData* chest = ou->gamedata.getData("879-gamedata.base", BUILDING); // Wooden Chest, verified vanilla template.
    if (!chest || !ou->theFactory) { tell("Component chest unavailable: missing Wooden Chest definition."); return; }
    for (size_t i=0;i<debugItemCount;++i) {
        if (!ou->gamedata.getData(debugItems[i].id,ITEM)) { tell(std::string("Missing debug component: ")+debugItems[i].id); return; }
    }
    Ogre::Vector3 p;
    if (!findChestGround(actor->getPosition(),p)) return;
    log("debug chest creation requested");
    Building* created = createChestGuarded(ou,chest,p,actor,&faulted);
    if (faulted) { tell("Native chest creation faulted. Project Cars disabled for this session; reload before further testing."); return; }
    if (!created || !created->isValid()) { tell("The engine did not return a component chest."); return; }
    lastDebugChest=created->getHandle();
    ChestVisualState before;readChestVisuals(created,&before);logChestVisuals(created,"created",before);
    if (!prepareChestVisuals(created)) {tell("Chest object created, but its graphics could not initialize. No components were added; see the log.");return;}
    pendingChest=lastDebugChest; chestStarted=GetTickCount();chestCheckedAt=chestStarted;
    created->setName("Vehicle Components - Debug");
    tell("Preparing component chest graphics...");
}
void fillChest() {
    if (pendingChest.isNull()) return;
    if (GetTickCount()-chestCheckedAt<250) return;
    chestCheckedAt=GetTickCount();
    Building* chest=resolveBuilding(pendingChest);
    if (!chest) { pendingChest.setNull(); tell("Component chest is no longer available."); return; }
    ChestVisualState state;
    if (!readChestVisuals(chest,&state)) {pendingChest.setNull();tell("Could not verify the chest graphics. No components were added.");return;}
    if (!state.ready()) {
        if (GetTickCount()-chestStarted>10000) {
            logChestVisuals(chest,"graphics timeout",state);pendingChest.setNull();
            tell("Chest graphics did not finish loading. No components were added; see the log.");
        }
        return;
    }
    logChestVisuals(chest,"graphics ready",state);
    Inventory* inv=chest->getInventory();
    if (!inv) {
        if (GetTickCount()-chestStarted>10000) { pendingChest.setNull(); tell("Component chest inventory did not initialize; no items were spawned."); }
        return;
    }
    // Clear the request BEFORE inventory mutations. A failed operation must not be retried and duplicate earlier items.
    pendingChest.setNull();
    unsigned added=0, requested=0;
    for (size_t i=0;i<debugItemCount;++i) {
        GameData* def=ou->gamedata.getData(debugItems[i].id,ITEM);
        requested+=debugItems[i].quantity;
        if (!def) continue;
        for (unsigned n=0;n<debugItems[i].quantity;++n) {
            if (!inv->hasRoomForItem(def)) break;
            int before=inv->countItems(def);
            Item* item=createItemGuarded(ou->theFactory,def,hand(),&faulted);
            if (faulted) { tell("Native item creation faulted. Debug spawning disabled for this session."); return; }
            if (!item) break;
            // Engine owns cleanup on rejected insertion; never dereference item afterwards (stack merging may destroy it).
            bool accepted=insertGuarded(inv,item,&faulted);
            if (faulted) { tell("Native chest insertion faulted. Debug spawning disabled for this session."); return; }
            if (!accepted || inv->countItems(def)!=before+1) break;
            ++added;
        }
    }
    std::ostringstream msg; msg << "Component chest spawned: " << added << '/' << requested << " items.";
    if (added!=requested) msg << " Some items could not fit; see Project Cars debug manifest.";
    ou->player->focusCamera(state.position);
    ou->player->selectObject(chest,false);
    tell(msg.str());
}
#include "VehicleDebugRuntime.inl"
void reset(GameWorld* world) {
    // Do not touch MyGUI or the registry on a possibly non-UI loading thread.
    // Signal the main-loop owner, which performs cleanup after the native reset.
    InterlockedIncrement(&resetDepth);
    InterlockedIncrement(&worldEpoch);
    cars::cancelVehicleRecovery();
    cars::clearVehicleRouteRequests();
    cars::resetOilDeposits();
    originalReset(world);
    InterlockedIncrement(&worldEpoch);
    InterlockedDecrement(&resetDepth);
}
void resetWorld(GameWorld* world) {
    // Ordinary save loads inline cleanup in resetGame and can bypass the helper.
    // Use the same epoch signal; nested helper calls retain the depth barrier.
    InterlockedIncrement(&resetDepth);InterlockedIncrement(&worldEpoch);
    cars::cancelVehicleRecovery();
    cars::clearVehicleRouteRequests();
    cars::resetOilDeposits();
    originalWorldReset(world);
    InterlockedIncrement(&worldEpoch);InterlockedDecrement(&resetDepth);
}
void tick(GameWorld* world,float dt) {
    const bool trace=firstTick;
    firstTick=false;
    if (trace) log("tick entered; calling original chain");
    if(!faulted)try{updateVehicleControls();}catch(...){cars::endManualDriving();log("vehicle input update failed; manual driving ended");}
    if(!faulted && readyLastFrame && worldReady()) {
        try {updateRightPress();}catch(...){log("vehicle mouse capture failed");}
    }
    originalTick(world,dt);
    if (trace) log("tick original chain returned");
    if (faulted) {cars::closeVehicleDebug();return;}
    try {
        if(InterlockedExchange(&debugLoadCleanup,0)){resetVehicleDebug();debugKey.reset();}
        const LONG epoch=InterlockedCompareExchange(&worldEpoch,0,0);
        if (epoch!=observedEpoch) {
            clearWorld(); readyLastFrame=false; firstReady=true; observedEpoch=epoch;
            log("world boundary reconciled on main loop");
        }
        bool ready=world==ou && worldReady();
        // OS async state is not a keypress event. Require a native F12 down
        // after observing release in this loaded world; never spawn on load.
        debugKey.observeReleased(ready,key && key->keyboard?key->keyboard->isKeyDown(OIS::KC_F12):true);
        bool fire=debugKey.take(ready && readyLastFrame && debugInputAllowed());
        // The native predicate tests the shared loading panel's visibility.
        // Streaming terrain is not a new world. Only the reset hook's epoch
        // releases vehicle/seat ownership; freeze timers while this panel is up.
        if (!ready) { cars::closeVehicleDebug(); cars::pauseVehicleStreaming(); readyLastFrame=false; return; }
        if (firstReady) { firstReady=false; log("world ready; discovery restricted to explicit selection and interaction"); }
        if(!readyLastFrame)cars::pauseVehicleStreaming();
        readyLastFrame=true;
        updateRightPress();
        updateRecoveryUI();
        closeRestoredDebugInventory();
        cars::updateFittingWork();
        if (closeRequested) closePanel();
        if (fire){log("fresh Ctrl+F12 keypress accepted");toggleVehicleDebug();}
        updateVehicleDebug();
        if(faulted){cars::closeVehicleDebug();return;}
        fillChest();
        if(!pendingOilPool.isNull() && !(key && key->mRight)) {
            const hand pool=pendingOilPool,actor=pendingOilActor;pendingOilPool.setNull();pendingOilActor.setNull();
            world->hideContextMenu();tell(cars::buildPoolOilJack(resolveBuilding(pool),actor.getCharacter()));
        }
        if (!pendingModify.isNull() && (pendingMenuAction || !(key && key->mRight))) {
            const hand target=pendingModify,actor=pendingActor;const cars::VehicleClick action=pendingClick;
            pendingModify.setNull();pendingActor.setNull();pendingMenuAction=false;
            world->hideContextMenu();Building* b=resolveBuilding(target);
            if(ownFrame(b) && construction(b)==cars::ConstructionComplete) {
                if(action==cars::ConfigureVehicle)openModify(b);
                else {
                    closePanel();std::ostringstream detail;detail<<"dispatch vehicle action="<<action<<" actor="<<actor.toString()<<" vehicle="<<target.toString();log(detail.str());
                    if(!cars::interactWithVehicle(b,actor.getCharacter(),action)) {
                        const std::string reason=cars::vehicleInteractionStatus(b);log("vehicle action rejected: "+reason);
                        if(action==cars::RightVehicle)tell(reason);
                    }
                    else if(action==cars::RightVehicle)tell("Left click to place it.");
                }
            }
        }
        updateWorkshop();
        if (GetTickCount()-lastScan>=1000) {
            lastScan=GetTickCount();
            adopt(resolveBuilding(world->player->selectedObject));
            // Do not use GameWorld::getObjectsWithinSphere from this hook.
            // Installed 1.0.65 ignores maxNumber and enters a locked spatial query.
            // Selection/right-click already provide a bounded, engine-owned target.
            for (std::map<hand,cars::Vehicle*>::iterator i=vehicles.begin();i!=vehicles.end();) {
                if (!resolveBuilding(i->first)) { delete i->second; vehicles.erase(i++); } else ++i;
            }
        }
    } catch (const std::exception& e) { faulted=true; ErrorLog(std::string("Project Cars native update disabled: ")+e.what()); }
      catch (...) { faulted=true; ErrorLog("Project Cars native update disabled after exception"); }
}
bool cursorVehicle(Building* preferred,hand& hit,cars::VehicleClick& action) {
    if (!worldReady()) return false;
    if (!MyGUI::Gui::getInstancePtr()) return false;
    CameraClass* camera=ou->player->getCamera();
    MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();
    MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();
    if(!preferred)preferred=resolveBuilding(cars::drivenVehicle(ou->player->selectedCharacter.getCharacter()));
    if(camera && camera->camera && render && input) {
        MyGUI::IntSize size=render->getViewSize();MyGUI::IntPoint mouse=input->getMousePosition();
        if(size.width>0 && size.height>0) {
            Ogre::Ray ray=camera->camera->getCameraToViewportRay(static_cast<float>(mouse.left)/size.width,static_cast<float>(mouse.top)/size.height);
            cars::pickVehiclePart(ray,preferred,hit,action);
        }
    }
    return !hit.isNull();
}
void queueVehicleClick(const hand& hit,cars::VehicleClick action) {
    Building* picked=resolveBuilding(hit);Character* actor=ou->player->selectedCharacter.getCharacter();
    cars::VehicleActions available=cars::vehicleActions(picked,actor);
    pendingModify=hit;pendingClick=cars::defaultVehicleClick(action,available.seat,available.steering);
    pendingActor=ou->player->selectedCharacter;pendingMenuAction=false;
}
void updateRightPress() {
    if(cars::vehicleRecoveryActive()){rightPress.release();return;}
    const bool down=key && key->mRight;
    if(!down){rightPress.release();return;}
    if(!worldReady() || !inputAllowed() || cars::workshopVisible())return;
    if(!rightPress.held) {
        pressedVehicle.setNull();cars::VehicleClick action=cars::ConfigureVehicle;
        MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();
        const bool overGui=input && input->getMouseFocusWidget();
        if(!overGui && !ou->player->contextMenu.isVisible())cursorVehicle(0,pressedVehicle,action);
        rightPress.begin(!pressedVehicle.isNull(),GetTickCount()*.001);
        if(rightPress.vehicle)queueVehicleClick(pressedVehicle,action);
    }
    if(rightPress.openMenu(GetTickCount()*.001)) {
        Building* b=resolveBuilding(pressedVehicle);
        if(originalContext && ownFrame(b) && construction(b)==cars::ConstructionComplete)context(&ou->player->contextMenu,true,b);
    }
}
bool queueModify(RootObject* object) {
    if(!worldReady())return false;
    Building* b=object && object->getDataType()==BUILDING?static_cast<Building*>(object):0;
    // The seated driver's click hull may be the native target. Our part pick
    // still resolves the moving car instead of treating that hit as empty ground.
    if(object && !ownFrame(b) && !cars::sameHandle(object->getHandle(),ou->player->selectedCharacter))return false;
    hand hit;cars::VehicleClick action=cars::ConfigureVehicle;
    cursorVehicle(b,hit,action);
    if(hit.isNull()){if(!ownFrame(b)||construction(b)!=cars::ConstructionComplete)return false;hit=b->getHandle();}
    queueVehicleClick(hit,action);return true;
}
void vehicleOption(MyGUI::Widget* sender) {
    if(!sender || sender->getUserString("PC_Queued")=="1")return;
    hand target,actor;target.fromString(sender->getUserString("PC_Target"));actor.fromString(sender->getUserString("PC_Actor"));
    Building* b=resolveBuilding(target);if(!ownFrame(b) || construction(b)!=cars::ConstructionComplete)return;
    cars::VehicleClick action;if(!cars::parseVehicleAction(sender->getUserString("PC_Action"),action)){log("invalid vehicle menu action rejected");return;}
    sender->setUserString("PC_Queued","1");
    pendingModify=target;pendingActor=actor;pendingClick=action;
    log("vehicle menu selected: "+sender->getUserString("PC_Action"));
    pendingMenuAction=true;
}
void vehicleOptionReleased(MyGUI::Widget* sender,int x,int y,MyGUI::MouseButton button) {
    if(button==MyGUI::MouseButton::Right && sender && sender->getAbsoluteCoord().inside(MyGUI::IntPoint(x,y)))vehicleOption(sender);
}
void oilOption(MyGUI::Widget* sender) {
    if(!sender || sender->getUserString("PC_Queued")=="1")return;
    sender->setUserString("PC_Queued","1");
    pendingOilPool.fromString(sender->getUserString("PC_Target"));pendingOilActor.fromString(sender->getUserString("PC_Actor"));
}
void oilOptionReleased(MyGUI::Widget* sender,int x,int y,MyGUI::MouseButton button) {
    if(button==MyGUI::MouseButton::Right && sender && sender->getAbsoluteCoord().inside(MyGUI::IntPoint(x,y)))oilOption(sender);
}
void addOilPoolMenu(ContextMenu* menu,Building* b) {
    MyGUI::Widget* options=0;ContextMenuGUI* panels[2]={menu->menuGUI,menu->menuGUI2};
    for(int n=0;n<2;++n)if(panels[n]){MyGUI::Widget* p=*reinterpret_cast<MyGUI::Widget**>(reinterpret_cast<char*>(panels[n])+0xF8);if(p && p->getInheritedVisible()){options=p;break;}}
    if(!options)return;
    for(size_t n=0;n<options->getChildCount();++n)if(options->getChildAt(n)->getUserString("PC_OilOption")=="1")return;
    MyGUI::Widget* root=options;while(root->getParent())root=root->getParent();
    int height=options->getHeight(),width=options->getWidth();
    for(size_t n=0;n<options->getChildCount();++n){MyGUI::Widget* child=options->getChildAt(n);child->setPosition(child->getLeft(),child->getTop()+32);}
    MyGUI::IntCoord coord=root->getCoord();coord.height+=32;
    MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();if(render)coord.top=std::max(0,std::min(coord.top,render->getViewSize().height-coord.height));
    root->setCoord(coord);options->setSize(width,height+32);
    MyGUI::Button* button=options->createWidget<MyGUI::Button>("Kenshi_Button1",4,2,width-8,28,MyGUI::Align::Default);
    button->setCaption("Build Oil Jack");button->setUserString("PC_OilOption","1");button->setUserString("PC_Target",b->getHandle().toString());button->setUserString("PC_Actor",ou->player->selectedCharacter.toString());
    button->eventMouseButtonClick+=MyGUI::newDelegate(oilOption);button->eventMouseButtonReleased+=MyGUI::newDelegate(oilOptionReleased);
}
bool redundantVehicleRow(MyGUI::Widget* widget,unsigned depth=0) {
    if(!widget || depth>6)return false;
    MyGUI::TextBox* label=widget->castType<MyGUI::TextBox>(false);
    if(label) {
        std::string caption=label->getCaption().asUTF8();
        for(size_t n=0;n<caption.size();++n)caption[n]=static_cast<char>(toupper(static_cast<unsigned char>(caption[n])));
        if(caption=="USE" || caption=="INVENTORY")return true;
    }
    for(size_t n=0;n<widget->getChildCount();++n)if(redundantVehicleRow(widget->getChildAt(n),depth+1))return true;
    return false;
}
void addVehicleMenu(ContextMenu* menu,Building* b) {
    // ContextMenu is also declared by PlayerInterface.h. Read the published
    // ContextMenuGUI fields without including its conflicting second declaration.
    MyGUI::Widget* options=0;ContextMenuGUI* panels[2]={menu->menuGUI,menu->menuGUI2};
    for(int n=0;n<2;++n)if(panels[n]){MyGUI::Widget* p=*reinterpret_cast<MyGUI::Widget**>(reinterpret_cast<char*>(panels[n])+0xF8);if(p && p->getInheritedVisible()){options=p;break;}}
    if(!options)return;
    for(size_t n=0;n<options->getChildCount();++n)if(options->getChildAt(n)->getUserString("PC_VehicleOption")=="1")return;
    MyGUI::Widget* root=options;while(root->getParent())root=root->getParent();
    Character* actor=ou->player->selectedCharacter.getCharacter();cars::VehicleActions a=cars::vehicleActions(b,actor);
    const char* labels[18]={a.steering?"Drive":"Get in","Modify","Open trunk",a.boarding?"Cancel boarding":"Get out",a.ignition?"Stop engine":"Start engine","Brake / stop","Change driving pace","Correct Vehicle",a.manual?"End Manual Driving":"Manually Drive","Get in passenger seat","Place carried person: driver seat","Place carried person: rear left","Place carried person: rear right","Place carried person: front pole","Place carried person: rear pole","Place carried person: cage","Unload passengers / prisoners","Man turret"};
    const char* actions[18]={a.steering?"drive":"sit","modify","trunk","leave","ignition","stop","pace","right",a.manual?"end_manual":"manual","passenger","place_driver","place_rear_l","place_rear_r","place_pole_f","place_pole_r","place_cage","unload_people","man_turret"};
    const bool available[18]={a.seat && !a.occupied && !a.seated,true,a.cargo,a.seated || a.boarding,a.seated,a.seated,a.seated && !a.manual,a.overturned,a.driven && a.steering,a.passenger,a.placeDriver,a.placeLeft,a.placeRight,a.placeFrontPole,a.placeRearPole,a.placeCage,a.people,a.gunner};
    int count=0;for(int n=0;n<18;++n)if(available[n])++count;
    const int oldHeight=options->getHeight(),row=32,width=options->getWidth();int retainedHeight=0;
    for(size_t n=0;n<options->getChildCount();++n) {
        MyGUI::Widget* child=options->getChildAt(n);
        if(redundantVehicleRow(child)){child->setVisible(false);child->setEnabled(false);continue;}
        child->setPosition(child->getLeft(),count*row+retainedHeight);retainedHeight+=child->getHeight();
    }
    MyGUI::IntCoord coord=root->getCoord();coord.height+=count*row+retainedHeight-oldHeight;
    MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();if(render)coord.top=std::max(0,std::min(coord.top,render->getViewSize().height-coord.height));
    root->setCoord(coord);options->setSize(width,retainedHeight+count*row);
    int added=0;for(int n=0;n<18;++n)if(available[n]) {
        MyGUI::Button* button=options->createWidget<MyGUI::Button>("Kenshi_Button1",4,added++*row+2,width-8,row-4,MyGUI::Align::Default);
        button->setCaption(labels[n]);button->setUserString("PC_VehicleOption","1");
        button->setUserString("PC_Target",b->getHandle().toString());button->setUserString("PC_Actor",actor?actor->getHandle().toString():"");
        button->setUserString("PC_Action",actions[n]);
        button->eventMouseButtonClick+=MyGUI::newDelegate(vehicleOption);
        button->eventMouseButtonReleased+=MyGUI::newDelegate(vehicleOptionReleased);
    }
    vehicleMenuTarget=b->getHandle();log("vehicle context options added");
}
void context(ContextMenu* self,bool on,RootObject* object) {
    // Native hover loss must not dismiss the held menu when its vehicle moves
    // away from the cursor. Release, Escape and a chosen action still close it.
    if(!on && !faulted && worldReady() && inputAllowed() && rightPress.held && rightPress.vehicle && rightPress.menu &&
        key && key->mRight && !key->escape && !key->escape_msg && !pendingMenuAction && ownFrame(resolveBuilding(pressedVehicle)))return;
    if(on && !faulted && rightPress.held && rightPress.vehicle) {
        Building* captured=resolveBuilding(pressedVehicle);
        if(ownFrame(captured) && construction(captured)==cars::ConstructionComplete)object=captured;
    }
    originalContext(self,on,object);
    if(!on){vehicleMenuTarget.setNull();return;}
    try {
        Building* b=object && object->getDataType()==BUILDING?static_cast<Building*>(object):0;
        if(!faulted && worldReady() && cars::isOilPool(b))addOilPoolMenu(self,b);
        if(!faulted && ownFrame(b) && construction(b)==cars::ConstructionComplete) {
            if(rightPress.held)rightPress.menu=true;
            if(!pendingMenuAction){pendingModify.setNull();pendingActor.setNull();}
            addVehicleMenu(self,b);
        }
    }catch(...){log("vehicle context augmentation failed; native options retained");}
}
void task(PlayerInterface* self,TaskType type,const hand& target,Building* indoors,const Ogre::Vector3& point,bool append) {
    if(cars::vehicleRecoveryActive())return;
    if(!faulted && worldReady()) {
        try {updateRightPress();}catch(...){log("vehicle task mouse capture failed");}
    }
    if(pendingMenuAction && !append && (target.isNull() || cars::sameHandle(target,pendingModify)))return;
    if(!faulted && rightPress.vehicle && (rightPress.held || (key && key->mRUp)) && !pendingMenuAction)return;
    if(!vehicleMenuTarget.isNull() && self->contextMenu.isVisible()) {
        Building* b=resolveBuilding(target);
        if(ownFrame(b) && (type==OPERATE_MACHINERY || type==SIT_AROUND || type==SIT_ON_THRONE)) {
            cars::VehicleActions a=cars::vehicleActions(b,self->selectedCharacter.getCharacter());
            pendingModify=b->getHandle();pendingActor=self->selectedCharacter;pendingClick=a.steering?cars::DriveVehicle:cars::SitDriver;pendingMenuAction=true;return;
        }
        originalTask(self,type,target,indoors,point,append);return;
    }
    if(!faulted && type==MOVE_CUS_ORDERED && worldReady() && !cars::drivenVehicle(self->selectedCharacter.getCharacter()).isNull()) {
        if(!rightPress.takeOrder(key && key->mRUp))return;
        // A terrain press remains terrain even if the car moves under the
        // cursor later in this same gesture.
        originalTask(self,type,target,indoors,point,append);return;
    }
    try { if (!faulted && !append && (type==MOVE_CUS_ORDERED || type==BUILD || type==REPAIR ||
        type==OPERATE_MACHINERY || type==OPERATE_STORAGE || type==SIT_AROUND || type==SIT_ON_THRONE) &&
        (target.isNull() || resolveBuilding(target)) && queueModify(resolveBuilding(target))) return; }
    catch (...) { log("task interception failed; returning to native task"); }
    originalTask(self,type,target,indoors,point,append);
}
}
namespace cars {
bool vehicleWorldChanging() {
    return InterlockedCompareExchange(&resetDepth,0,0)!=0 || observedEpoch!=InterlockedCompareExchange(&worldEpoch,0,0);
}
bool installNativeRuntime(const Catalog& definitions) {
    catalog=&definitions;
    if(!cars::installFittingRuntime()) {
        log("fitting hooks unavailable; installation disabled");
    }
    if(!installVehicleControls()) {
        log("vehicle controls UI/camera hook unavailable");
    }
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&InputHandler::keyDownEvent),keyDown,&originalKeyDown)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&InputHandler::keyUpEvent),keyUp,&originalKeyUp)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&PlayerInterface::loadFromSerialise),playerLoad,&originalPlayerLoad)!=KenshiLib::SUCCESS)return false;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_clearAndDestroyGameWorldStuff),reset,&originalReset)!=KenshiLib::SUCCESS) return false;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::resetGame),resetWorld,&originalWorldReset)!=KenshiLib::SUCCESS) return false;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff),tick,&originalTick)!=KenshiLib::SUCCESS) return false;
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&ContextMenu::showContextMenu),context,&originalContext)!=KenshiLib::SUCCESS) {
        log("context hook unavailable; default task hook will still be attempted");
    }
    if (KenshiLib::AddHook(KenshiLib::GetRealAddress(&PlayerInterface::newPlayerTaskSelectedCharacters),task,&originalTask)!=KenshiLib::SUCCESS)
        log("default task hook unavailable; use held right-click for Modify");
    log("native vehicle interactions and Ctrl+F12 debug menu enabled");
    return true;
}
}
