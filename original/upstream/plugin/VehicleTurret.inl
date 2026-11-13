// Native mounted crossbow attached to the rear bay. The native gun owns target
// selection, reloads, ammunition, skill use and projectile damage.
struct VehicleTurretState {hand weapon;double stamp;DWORD retry;VehicleTurretState():stamp(-1),retry(0){}};
std::map<hand,VehicleTurretState> vehicleTurrets;
std::set<hand> vehicleGunners;
void (*originalGunnerUpdateT)(RangedCombatClass*,float,const hand&,const hand&)=0;
void (*originalGunnerUpdateMT)(RangedCombatClass*,float)=0;
bool mountedRangedActor(Character* actor) {
    if(!actor)return false;
    if(passengerOrder(actor))return true;
    for(std::map<hand,DriveState>::const_iterator i=drives.begin();i!=drives.end();++i)
        if(cars::sameHandle(i->second.occupant,actor->getHandle()))return true;
    return false;
}
void gunnerUpdateT(RangedCombatClass* ranged,float dt,const hand& designated,const hand& suggested) {
    {Lock lock;if(mountedRangedActor(ranged->me))return;}
    originalGunnerUpdateT(ranged,dt,designated,suggested);
}
void gunnerUpdateMT(RangedCombatClass* ranged,float dt) {
    {Lock lock;if(mountedRangedActor(ranged->me))return;}
    originalGunnerUpdateMT(ranged,dt);
}
bool installVehicleTurret() {
    return KenshiLib::AddHook(KenshiLib::GetRealAddress(&RangedCombatClass::updateT),gunnerUpdateT,&originalGunnerUpdateT)==KenshiLib::SUCCESS &&
        KenshiLib::AddHook(KenshiLib::GetRealAddress(&RangedCombatClass::updateMT),gunnerUpdateMT,&originalGunnerUpdateMT)==KenshiLib::SUCCESS;
}
void pauseVehicleTurrets(){for(std::map<hand,VehicleTurretState>::iterator i=vehicleTurrets.begin();i!=vehicleTurrets.end();++i)i->second.stamp=-1;}
void forgetVehicleTurret(Building* b) {
    if(turretPart(b))for(std::map<hand,VehicleTurretState>::iterator i=vehicleTurrets.begin();i!=vehicleTurrets.end();++i)if(cars::sameHandle(i->second.weapon,b->getHandle())) {
        // This base destructor runs after the derived gun may be freed.
        // Combat cleanup belongs to the earlier GameWorld::destroy hook.
        i->second.weapon.setNull();i->second.stamp=-1;i->second.retry=GetTickCount();
    }
    if(frame(b)) {
        if(passengers.count(b->getHandle()))for(std::map<std::string,MountedPerson>::const_iterator p=passengers[b->getHandle()].begin();p!=passengers[b->getHandle()].end();++p)vehicleGunners.erase(p->second.person);
        vehicleTurrets.erase(b->getHandle());
    }
}
bool turretPart(const Building* b){return b && b->getGameData() && b->getGameData()->stringID==cars::turretBuildingId;}
bool fittedTurret(Building* b){return fittedDesign(b,"cargo",cars::turretItemId);}
bool assignedGunner(const Building* weapon,const hand& actor) {
    Building* b=turretVehicle(const_cast<Building*>(weapon));
    return b && fittedTurret(b) && passengerSlotOccupied(b,"cargo") && cars::sameHandle(passengers[b->getHandle()]["cargo"].person,actor);
}
Building* turretVehicle(Building* weapon) {
    if(!turretPart(weapon))return 0;
    Building* b=static_cast<TurretBuilding*>(weapon)->getMountedBuilding().getBuilding();
    return b && b->isValid() && frame(b)?b:0;
}
TurretBuilding* turretWeapon(Building* b) {
    std::map<hand,VehicleTurretState>::iterator i=vehicleTurrets.find(b->getHandle());
    Building* w=i==vehicleTurrets.end()?0:i->second.weapon.getBuilding();
    return w && w->isValid() && turretPart(w)?static_cast<TurretBuilding*>(w):0;
}
Ogre::Quaternion turretFacing(Building* b) {
    TurretBuilding* w=turretWeapon(b);
    if(w && w->hingePart && w->hingePart->ent && w->hingePart->ent->getParentSceneNode())return w->hingePart->ent->getParentSceneNode()->_getDerivedOrientation();
    return b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y);
}
Ogre::Vector3 gunnerAnchor(Building* b) {
    // Stand behind the rotating hinge, on the rear deck.
    return rigWorld(b,cars::RigPoint(-17,11,0))+turretFacing(b)*Ogre::Vector3(0,0,-6*b->getScale());
}
void saveVehicleTurret(Building* b,GameData* state) {
    hand weapon;std::map<hand,VehicleTurretState>::iterator i=vehicleTurrets.find(b->getHandle());
    if(i!=vehicleTurrets.end())weapon=i->second.weapon;
    state->storeHandle(weapon,"ProjectCars turret v1",false);
}
void loadVehicleTurret(Building* b,GameData* state) {
    VehicleTurretState s;state->getHandle(s.weapon,"ProjectCars turret v1");vehicleTurrets[b->getHandle()]=s;
}
void endGunner(Building* b,Character* who) {
    if(!who)return;
    vehicleGunners.erase(who->getHandle());
    TurretBuilding* w=turretWeapon(b);
    if(w)w->stopOperating(who->getHandle());
    if(who->rangedCombat && w && cars::sameHandle(who->rangedCombat->turret,w->getHandle()))who->rangedCombat->end();
    else if(who->rangedCombat && !who->rangedCombat->turret.isNull() && !who->rangedCombat->turret.getBuilding()) {
        // A streamed-away native gun has no valid pointer to call end() on.
        who->rangedCombat->gun=0;who->rangedCombat->turret.setNull();who->rangedCombat->combatMode=false;
        who->rangedCombat->takeAShotMsg=false;who->rangedCombat->updateNeededMsg=false;
    }
}
void removeVehicleTurret(Building* b) {
    std::map<hand,VehicleTurretState>::iterator i=vehicleTurrets.find(b->getHandle());if(i==vehicleTurrets.end())return;
    TurretBuilding* w=turretWeapon(b);
    if(passengerSlotOccupied(b,"cargo"))endGunner(b,passengers[b->getHandle()]["cargo"].person.getCharacter());
    vehicleTurrets.erase(i);
    if(w && ou && !ou->gameResetting && !cars::vehicleWorldChanging()) {
        w->setMountedBuilding(0);ou->destroy(w,false,"Project Cars rear turret removed");
    }
}
void endVehicleGunner(Building* b){if(passengerSlotOccupied(b,"cargo"))endGunner(b,passengers[b->getHandle()]["cargo"].person.getCharacter());}
void moveVehicleTurret(Building* b) {
    TurretBuilding* w=turretWeapon(b);if(!w || !b->getRootNode())return;
    const Ogre::Vector3 position=rigWorld(b,cars::RigPoint(-17,11,0));
    const Ogre::Quaternion rotation=b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y);
    if(!w->isPhysical() || !w->entitiesLoaded || !w->getRootNode()){w->pos=position;w->rot=rotation;return;}
    Ogre::SceneNode* node=w->getRootNode();Ogre::SceneNode* parent=node->getParentSceneNode();
    // Physics actors retain their own world transforms. Move them before
    // changing the building anchor used by the collision delta calculation.
    cars::CollisionPoseReport collision;
    if(!cars::moveNativeCollision(w,position,rotation,true,&collision))return;
    // Native rotating parts can have separate nodes outside the building root.
    // Move only those owned part nodes, once per distinct ancestor, preserving
    // hinge/gun articulation. Never move their shared scene parent.
    std::set<Ogre::SceneNode*> parts;
    if(w->physical)for(unsigned n=0;n<w->physical->staticEnts.size() && n<128;++n) {
        PhysicalEntity* part=w->physical->staticEnts[n];
        Ogre::SceneNode* p=part && part->ent?part->ent->getParentSceneNode():0;
        if(p && p!=node && p!=node->getCreator()->getRootSceneNode())parts.insert(p);
    }
    const Ogre::Vector3 shift=node->getCreator()->getRelativeOrigin();
    const Ogre::Quaternion delta=rotation*w->rot.Inverse();
    for(std::set<Ogre::SceneNode*>::iterator i=parts.begin();i!=parts.end();++i) {
        bool inherited=false;for(Ogre::SceneNode* p=(*i)->getParentSceneNode();p;p=p->getParentSceneNode())if(p==node || parts.count(p)){inherited=true;break;}
        if(inherited)continue;
        Ogre::SceneNode* p=(*i)->getParentSceneNode();
        Ogre::Vector3 moved=position+delta*((*i)->_getDerivedPosition()+shift-w->pos)-shift;
        Ogre::Quaternion facing=delta*(*i)->_getDerivedOrientation();
        (*i)->setPosition(p?p->convertWorldToLocalPosition(moved):moved);
        (*i)->setOrientation(p?p->_getDerivedOrientation().Inverse()*facing:facing);
    }
    w->pos=position;w->rot=rotation;
    const Ogre::Vector3 render=position-node->getCreator()->getRelativeOrigin();
    node->setPosition(parent?parent->convertWorldToLocalPosition(render):render);
    node->setOrientation(parent?parent->_getDerivedOrientation().Inverse()*rotation:rotation);
    node->setScale(b->getScale(),b->getScale(),b->getScale());node->_getDerivedPositionUpdated();node->_getDerivedOrientationUpdated();
    w->maxUseRange=10*std::max(1.f,b->getScale());
    w->recalculateWorldAABB();w->notifyChange();w->instanceID.notifyChange();
    if(ou->zoneMgr)ou->zoneMgr->buildingGrid.update(w);
    if(w->gunClass)w->gunClass->GunClass::_NV_update(position);
}
void updateVehicleTurret(Building* b) {
    if(!fittedTurret(b)) {removeVehicleTurret(b);return;}
    if(!b->isPhysical() || !b->entitiesLoaded || !b->getRootNode())return;
    VehicleTurretState& state=vehicleTurrets[b->getHandle()];TurretBuilding* w=turretWeapon(b);
    if(!w) {
        // A saved handle can be unresolved while its zone restores. Preserve
        // the link instead of spawning a duplicate during that interval.
        if(!state.weapon.isNull() || GetTickCount()-state.retry<2000)return;
        state.retry=GetTickCount();GameData* data=ou->gamedata.getData(cars::turretBuildingId,BUILDING);if(!data)return;
        // Spawn directly on the rear attachment anchor.  Using the world
        // origin/ground here leaves a streamed turret behind at its creation
        // location until the next physics tick and can make it claim the
        // wrong interaction target.
        Ogre::Vector3 spawn=rigWorld(b,cars::RigPoint(-17,11,0));
        Building* made=ou->theFactory->createBuilding(data,spawn,b->getCurrentTownLocation(),b->getFaction(),b->rot,0,0,0,0,0,false,true,false,0,true);
        if(!made || !turretPart(made))return;
        state.weapon=made->getHandle();w=static_cast<TurretBuilding*>(made);w->setMountedBuilding(b);
        w->setVisible(true);w->notifyChange();w->instanceID.notifyChange();
        DebugLog("Project Cars: rear crossbow created / "+state.weapon.toString());
    }
    // Pose and collision synchronization runs only after native physics.
    if(!w->isPhysical() && GetTickCount()-state.retry>=2000){state.retry=GetTickCount();w->TurretBuilding::_NV_createPhysical();}
    if(!w->isPhysical() || !w->entitiesLoaded || !w->gunClass || !w->gunPart || !w->hingePart)return;
    if(!w->getVisible())w->setVisible(true);
    double now=ou->getTimeStamp_inGameHours().getTotalHours();
    double dt=state.stamp<0?0:cars::vehicleElapsed(now-state.stamp,ou->getLengthOfHourInRealSeconds());state.stamp=now;
    if(ou->isPaused() || dt<=0 || !passengerSlotOccupied(b,"cargo"))return;
    Character* gunner=passengers[b->getHandle()]["cargo"].person.getCharacter();
    if(!gunner || !gunner->isValid() || !gunner->rangedCombat || !gunner->getMedical())return;
    if(gunner->isDead() || gunner->getMedical()->isUnconcious() || drives[b->getHandle()].body.overturned() || drives[b->getHandle()].flooded) {endGunner(b,gunner);return;}
    if(!w->getOccupant().isNull() && !cars::sameHandle(w->getOccupant(),gunner->getHandle()))return;
    if(!w->tryOperate(gunner->getHandle()))return;
    vehicleGunners.insert(gunner->getHandle());
    if(!cars::sameHandle(gunner->rangedCombat->turret,w->getHandle()) || !gunner->rangedCombat->isInRangedCombatMode())gunner->rangedCombat->setup(w,STAT_TURRETS);
    w->TurretBuilding::_NV_operate(gunner,static_cast<float>(dt));
    // Invoke each native combat stage once on the game thread. The hooks above
    // suppress ordinary character updates for this gunner, preventing double
    // reload/shot timers and races with moving the weapon.
    hand none;originalGunnerUpdateT(gunner->rangedCombat,static_cast<float>(dt),none,none);
    originalGunnerUpdateMT(gunner->rangedCombat,static_cast<float>(dt));
}
