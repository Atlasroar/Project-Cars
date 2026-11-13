// Included in NativeRuntime after the guarded item/building factory helpers.
hand debugSpawnedVehicle,debugSpawnActor;
int debugSpawnPreset=0;
DWORD debugSpawnStarted=0,debugSpawnChecked=0;
void resetVehicleDebug(){cars::closeVehicleDebug();debugSpawnedVehicle.setNull();debugSpawnActor.setNull();}
bool debugInputAllowed() {
    if(inputAllowed())return true;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    return (cars::vehicleDebugVisible() || cars::workshopVisible()) && process==GetCurrentProcessId() && worldReady() && !vehicleOptionsOpen() && !ou->player->isLevelEditMode() && !ou->player->isObjectPlacementMode();
}
void toggleVehicleDebug() {
    if(cars::vehicleDebugVisible()){cars::closeVehicleDebug();return;}
    closePanel();ou->hideContextMenu();pendingModify.setNull();pendingActor.setNull();pendingMenuAction=false;
    rightPress=cars::VehicleRightPress();pressedVehicle.setNull();
    cars::DebugItemChoices choices;
    for(size_t n=0;n<debugItemCount;++n){GameData* data=ou->gamedata.getData(debugItems[n].id,ITEM);if(data)choices.push_back(std::make_pair(std::string(debugItems[n].id),data->name));}
    cars::showVehicleDebug(choices);
}
void debugSpawnItem(Character* actor,const cars::DebugRequest& request) {
    if(!actor || !actor->isValid() || !actor->isPlayerCharacter() || !actor->getInventory()){tell("Select a character with an inventory first.");return;}
    bool known=false;for(size_t n=0;n<debugItemCount;++n)known|=request.item==debugItems[n].id;
    GameData* data=known?ou->gamedata.getData(request.item,ITEM):0;
    const unsigned quantity=cars::debugQuantity(request.quantity);
    if(!data || !quantity || !ou->theFactory){tell("Select an available item and quantity.");return;}
    Inventory* inv=actor->getInventory();unsigned added=0;
    for(unsigned n=0;n<quantity;++n) {
        if(!inv->hasRoomForItem(data))break;
        const int before=countInventory(inv,data);if(before<0)break;
        Item* item=createItemGuarded(ou->theFactory,data,hand(),&faulted);if(faulted || !item)break;
        if(!insertGuarded(inv,item,&faulted) || faulted || countInventory(inv,data)!=before+1)break;
        ++added;
    }
    std::ostringstream out;out<<"Spawned "<<added<<"/"<<quantity<<" "<<data->name<<" for "<<actor->getName()<<".";
    if(faulted)out<<" Native operation failed; see the log.";
    else if(added<quantity)out<<" Make more inventory space.";
    tell(out.str());
}
bool debugVehicleGround(Ogre::Vector3& candidate,float actorY) {
    // A full car footprint, outside the selected character; unlike the chest,
    // a car needs roughly fifty native units of length and clear overhead space.
    const float corners[5][2]={{0,0},{-30,-16},{30,-16},{-30,16},{30,16}};
    float low=100000,high=-100000,center=0;
    for(unsigned n=0;n<5;++n) {
        Ogre::Vector3 p(candidate.x+corners[n][0],candidate.y,candidate.z+corners[n][1]);
        const float h=UtilityT::getTerrainHeightFromRenderer(p,false,0);
        if(!cars::boardingTerrainAvailable(h) || fabs(h-actorY)>80 || UtilityT::getPositionInWater(p.x,p.z))return false;
        low=std::min(low,h);high=std::max(high,h);if(n==0)center=h;
    }
    if(high-low>15)return false;
    Ogre::Vector3 clearance(candidate.x,high+34,candidate.z);
    if(ou->buildingIntersectionTestCapsule(clearance,32,60,0))return false;
    candidate.y=center;return true;
}
void debugSpawnVehicle(Character* actor,int preset) {
    if(!debugSpawnedVehicle.isNull()){tell("A debug vehicle is still loading.");return;}
    if(preset<0 || preset>=cars::DebugPresetCount || !actor || !actor->isValid() || !actor->isPlayerCharacter() || !actor->getInventory()){tell("Select a character and vehicle preset first.");return;}
    if(!actor->isIndoors().isNull()){tell("Move outside before spawning a vehicle.");return;}
    GameData* def=ou->gamedata.getData(cars::debugPresetBuildingId(preset),BUILDING);
    if(!def || !ou->theFactory){tell("Frame definition is unavailable for this preset.");return;}
    // Resolve every preset definition before creating anything.
    for(size_t n=0;n<cars::workshopMountCount;++n){const std::string id=cars::debugPresetItem(preset,cars::workshopMounts[n]);if(!id.empty()&&!ou->gamedata.getData(id,ITEM)){tell("Missing preset item: "+id);return;}}
    const float directions[8][2]={{1,0},{0,1},{-1,0},{0,-1},{.707107f,.707107f},{-.707107f,.707107f},{-.707107f,-.707107f},{.707107f,-.707107f}};
    const Ogre::Vector3 center=actor->getPosition();Ogre::Vector3 point;bool found=false;
    for(unsigned n=0;n<16 && !found;++n){const float distance=n<8?80.f:130.f;point=Ogre::Vector3(center.x+directions[n%8][0]*distance,center.y,center.z+directions[n%8][1]*distance);found=debugVehicleGround(point,center.y);}
    if(!found){tell("No vehicle-sized space nearby. Move to flatter open ground.");return;}
    // Outside-furniture creation adds the terrain elevation to its Y offset.
    Ogre::Vector3 factoryPoint(point.x,0,point.z);
    Building* b=createChestGuarded(ou,def,factoryPoint,actor,&faulted);
    if(faulted || !b || !b->isValid()){tell("Vehicle creation failed; see the log.");return;}
    if(preset==cars::DebugXSmallQuad || preset==cars::DebugMotorcycle)b->setName(cars::debugPresetName(preset));
    else b->setName(preset==cars::DebugTrackedBuggy?"Tracked Buggy":preset==cars::DebugBareFrame?"Light Vehicle":"Buggy");
    if(!prepareChestVisuals(b)){tell("Frame created, but its graphics did not initialize.");return;}
    debugSpawnedVehicle=b->getHandle();debugSpawnActor=actor->getHandle();debugSpawnPreset=preset;debugSpawnStarted=debugSpawnChecked=GetTickCount();
    tell(std::string("Preparing ")+cars::debugPresetName(preset)+"...");
}
void finishDebugVehicle() {
    if(debugSpawnedVehicle.isNull() || GetTickCount()-debugSpawnChecked<250)return;
    debugSpawnChecked=GetTickCount();Building* b=resolveBuilding(debugSpawnedVehicle);
    if(!b){debugSpawnedVehicle.setNull();tell("Spawned frame is no longer available.");return;}
    // A frame need not have a trunk; logical inventory is not a graphics gate.
    ChestVisualState state;
    if(!readChestVisuals(b,&state) || !state.physical || !state.visible || !state.node || !state.loaded || construction(b)!=cars::ConstructionComplete) {
        if(GetTickCount()-debugSpawnStarted>15000){debugSpawnedVehicle.setNull();tell("Frame loading timed out. Frame retained without a preset.");}return;
    }
    const hand target=debugSpawnedVehicle,actor=debugSpawnActor;const int preset=debugSpawnPreset;
    debugSpawnedVehicle.setNull();debugSpawnActor.setNull(); // Never retry a partially committed loadout.
    if(!adopt(b)){tell("Spawned frame could not be registered.");return;}
    tell(cars::debugFitVehicle(b,actor.getCharacter(),preset));
    ou->player->focusCamera(b->getPosition());
    log("debug preset completed / "+target.toString());
}
void updateVehicleDebug() {
    cars::updateVehicleDebug();cars::DebugRequest request;
    if(cars::takeVehicleDebugRequest(request)) {
        Character* actor=ou->player->selectedCharacter.getCharacter();
        if(request.action==cars::DebugClose)cars::closeVehicleDebug();
        else if(request.action==cars::DebugSpawnItem)debugSpawnItem(actor,request);
        else if(request.action==cars::DebugSpawnVehicle)debugSpawnVehicle(actor,request.preset);
        else if(request.action==cars::DebugChest)requestChest();
        else tell(cars::debugServiceVehicles(actor,request.action,request.scope));
    }
    if(!faulted)finishDebugVehicle();
}
