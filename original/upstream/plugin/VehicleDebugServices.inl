// Included inside namespace cars, after the native fitting/driving helpers.
std::string debugFitVehicle(Building* b,Character* actor,int preset) {
    Lock lock;
    if(!enabled || !frame(b) || !b->isValid() || !actor || !actor->isValid() || !actor->isPlayerCharacter() || !actor->getInventory() || b->getFaction()!=actor->getFaction() || preset<0 || preset>=DebugPresetCount)return "Preset unavailable.";
    if(b->getGameData()->stringID!=debugPresetBuildingId(preset))return "Preset does not match this frame.";
    Building::ConstructionState* build=b->getBuildState_ActualNonShared();
    if(!build || !build->isComplete || build->isDismantled)return "Frame has not finished building.";
    Inventory* inv=ensure(b);if(!inv || !inv->isEmpty())return "Preset requires the newly spawned empty frame.";
    if(preset==DebugBareFrame)return "Bare light frame spawned.";
    std::map<std::string,std::string> layout;
    for(size_t n=0;n<workshopMountCount;++n) {
        const WorkshopSlot& slot=workshopMounts[n];const std::string id=debugPresetItem(preset,slot);if(id.empty())continue;
        GameData* def=ou->gamedata.getData(id,ITEM);InventorySection* section=inv->getSection(slot.id);
        if(!def || !section || !section->hasRoomForItem(def,1) || !workshopAccepts(slot,id))return "Preset component unavailable. Empty frame retained.";
        layout[slot.id]=id;
    }
    for(size_t n=0;n<workshopMountCount;++n)if(layout.count(workshopMounts[n].id) && !workshopConflict(workshopMounts[n],layout).empty())return "Preset mount conflict. Empty frame retained.";
    unsigned added=0;bool complete=true;
    for(size_t n=0;n<workshopMountCount;++n) {
        const WorkshopSlot& slot=workshopMounts[n];if(!layout.count(slot.id))continue;
        GameData* def=ou->gamedata.getData(layout[slot.id],ITEM);InventorySection* section=inv->getSection(slot.id);
        Item* item=ou->theFactory->createItem(def,hand(),0,0,-1,0);if(!item){complete=false;break;}
        item->chargesLeft=1;
        if(!section->addItem(item,1)) {
            // The section rejected ownership; native inventory handles cleanup.
            actor->getInventory()->addItem(item,1,true,true);
            complete=false;break;
        }
        if(!section->hasItem(def)){complete=false;break;}
        ++added;
    }
    DriveState& s=driveState(b);s.body.ignition=false;s.body.fuel=fitted(b,"tank")?40:0;
    b->notifyChange();updateFittedVisuals(b);
    std::ostringstream out;out<<debugPresetName(preset)<<" spawned: "<<added<<" parts fitted.";
    if(!complete)out<<" Partial loadout; inspect the vehicle.";
    else out<<" Full tank; engine off.";
    return out.str();
}
std::string debugServiceVehicles(Character* actor,DebugAction action,int scope) {
    Lock lock;if(!enabled || !ou || !ou->player || (scope!=DebugSelected && scope!=DebugNearby))return "Vehicle debug unavailable.";
    if(scope==DebugNearby && (!actor || !actor->isValid() || !actor->isPlayerCharacter()))return "Select a character for the nearby vehicle search.";
    if(action!=DebugRefill && action!=DebugRepair && action!=DebugBrake && action!=DebugStopEngines)return "Unknown vehicle action.";
    std::set<hand> candidates;
    for(std::map<hand,Inventory*>::const_iterator i=stores.begin();i!=stores.end();++i)candidates.insert(i->first);
    for(std::map<hand,DriveState>::const_iterator i=drives.begin();i!=drives.end();++i)candidates.insert(i->first);
    candidates.insert(ou->player->selectedObject);
    unsigned count=0,skipped=0,visited=0;
    for(std::set<hand>::const_iterator i=candidates.begin();i!=candidates.end() && visited<1024 && count<64;++i,++visited) {
        Building* b=i->getBuilding();if(!frame(b) || !b->isValid() || b->getFaction()!=ou->player->getFaction())continue;
        bool selected=ou->player->isObjectSelected(b);
        if(drives.count(*i)) {Character* driver=drives[*i].occupant.getCharacter();selected|=driver && driver->isValid() && ou->player->isObjectSelected(driver);}
        if(passengers.count(*i))for(std::map<std::string,MountedPerson>::const_iterator p=passengers[*i].begin();p!=passengers[*i].end();++p){Character* person=p->second.person.getCharacter();selected|=person && person->isValid() && ou->player->isObjectSelected(person);}
        if(scope==DebugSelected?!selected:b->getPosition().squaredDistance(actor->getPosition())>250000)continue;
        Building::ConstructionState* build=b->getBuildState_ActualNonShared();
        if(!build || !build->isComplete || build->isDismantled || !b->isPhysical() || !b->entitiesLoaded){++skipped;continue;}
        DriveState& s=driveState(b);
        if(action==DebugRefill){if(!fitted(b,"tank")){++skipped;continue;}s.body.fuel=40;}
        if(action==DebugRepair){Inventory* inv=lookup(b);for(size_t n=0;inv && n<workshopMountCount;++n){InventorySection* section=inv->getSection(workshopMounts[n].id);Item* item=section?section->getItem():0;if(item)item->chargesLeft=1;}s.body.damage=0;}
        if(action==DebugBrake || action==DebugStopEngines){if(sameHandle(manualVehicle,*i))endManual();if(action==DebugStopEngines)stopDrive(s);else brakeDrive(s);}
        b->notifyChange();++count;
    }
    std::ostringstream out;out<<(action==DebugRefill?"Refilled ":action==DebugRepair?"Repaired ":action==DebugBrake?"Braking ":"Stopped engines on ")<<count<<" vehicle(s).";
    if(skipped)out<<" Skipped "<<skipped<<" unavailable vehicle(s).";
    if(count==0)out<<" Select a vehicle or use Nearby.";
    return out.str();
}
