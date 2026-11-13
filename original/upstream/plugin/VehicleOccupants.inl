struct MountedPerson {hand person;bool exiting,prepared;MountedPerson():exiting(false),prepared(false){}};
std::map<hand,std::map<std::string,MountedPerson> > passengers;
const char* personMounts[]={"seat_driver","seat_passenger_l","seat_passenger_r","front_attachment","cargo"};
unsigned passengerCount(Building* b){return passengers.count(b->getHandle())?static_cast<unsigned>(passengers[b->getHandle()].size()):0;}
bool passengerSlotOccupied(Building* b,const std::string& slot) {
    return passengers.count(b->getHandle()) && passengers[b->getHandle()].count(slot) && !passengers[b->getHandle()][slot].person.isNull();
}
bool attachedOccupant(Building* b,const hand& person) {
    if(drives.count(b->getHandle()) && cars::sameHandle(drives[b->getHandle()].occupant,person))return true;
    if(passengers.count(b->getHandle()))for(std::map<std::string,MountedPerson>::const_iterator i=passengers[b->getHandle()].begin();i!=passengers[b->getHandle()].end();++i)if(cars::sameHandle(i->second.person,person))return true;
    return false;
}
bool passengerOrder(Character* actor) {
    if(!actor)return false;
    for(std::map<hand,std::map<std::string,MountedPerson> >::const_iterator b=passengers.begin();b!=passengers.end();++b)
        for(std::map<std::string,MountedPerson>::const_iterator i=b->second.begin();i!=b->second.end();++i)if(cars::sameHandle(i->second.person,actor->getHandle()))return true;
    return false;
}
bool availablePersonMount(Building* b,const std::string& slot,const std::string& design="") {
    if(!fitted(b,slot)||passengerSlotOccupied(b,slot)||jobs.count(b->getHandle()))return false;
    if(slot=="seat_driver" && (seatInUse(b)||!drives[b->getHandle()].occupant.isNull()))return false;
    return design.empty()||fittedDesign(b,slot,design);
}
cars::RigPoint personAnchor(Building* b,const std::string& slot,bool pelvis=false) {
    if(slot=="seat_driver")return driverAnchor(b,pelvis);
    if(slot=="front_attachment")return cars::RigPoint(24.4,6.6,0);
    if(slot=="cargo")return fittedDesign(b,slot,"155-Project Cars.mod")?cars::RigPoint(-25.4,10.9,0):cars::RigPoint(-18.3,pelvis?16.5:11.4,0);
    const std::string component=slot=="seat_passenger_l"?"Seat_PassengerL":"Seat_PassengerR";
    // Rear seats duplicate the driver's mesh: its center is 5.022156 above
    // the standing anchor and 0.772156 above the seated pelvis anchor.
    for(size_t n=0;n<cars::workshopVisualCount;++n){const cars::WorkshopVisual& v=cars::workshopVisuals[n];if(component==v.component)return cars::RigPoint(v.x,v.y-(pelvis?.772156:5.022156),v.z);}
    return driverAnchor(b);
}
bool poleMount(Building* b,const std::string& slot){return (slot=="front_attachment"||slot=="cargo") && fittedDesign(b,slot,"155-Project Cars.mod");}
void savePassengers(Building* b,GameData* state) {
    for(unsigned n=0;n<5;++n) {
        hand person;if(passengerSlotOccupied(b,personMounts[n]))person=passengers[b->getHandle()][personMounts[n]].person;
        state->storeHandle(person,std::string("ProjectCars passenger v1 ")+personMounts[n],false);
    }
}
void loadPassengers(Building* b,GameData* state) {
    passengers.erase(b->getHandle());
    for(unsigned n=0;n<5;++n){hand person;if(state->getHandle(person,std::string("ProjectCars passenger v1 ")+personMounts[n]) && !person.isNull())passengers[b->getHandle()][personMounts[n]].person=person;}
}
void releasePassenger(Building* b,const std::string& slot,bool emergency=false,Character* carrier=0) {
    if(!passengerSlotOccupied(b,slot))return;
    Character* person=passengers[b->getHandle()][slot].person.getCharacter();
    if(!person||!person->isValid())return; // Preserve unresolved handles while loading.
    Ogre::Vector3 exit;
    if(boardingEntry(b,person,exit)){}
    else if(!emergency)return;
    else exit=b->getPosition()+b->rot*Ogre::Vector3(0,0,18*b->getScale());
    float ground=UtilityT::getTerrainHeightFromRenderer(exit,true,0);
    if(!cars::boardingTerrainAvailable(ground) && !emergency)return;
    if(cars::boardingTerrainAvailable(ground))exit.y=ground;
    passengers[b->getHandle()].erase(slot);
    if(slot=="cargo")endGunner(b,person);
    person->carryModeT(false,person->isDead() || (person->getMedical() && person->getMedical()->isUnconcious()),true);
    person->teleport(exit,b->rot);
    person->ragdollNavmeshPosition=exit;
    if(ou->zoneMgr)ou->zoneMgr->characterGrid.update(person);
    if(carrier && carrier!=person && carrier->getCarryingObject().isNull()){carrier->pickupObject(person);}
    if(person->getOrdersReciever()){person->getOrdersReciever()->clearOrders();person->getOrdersReciever()->clearCurrentGoal(true);}
    b->notifyChange();
}
bool passengerAction(Building* b,Character* carrier,cars::VehicleClick action) {
    using namespace cars;
    if(action==LeaveVehicle) {
        if(!passengers.count(b->getHandle()))return false;
        for(std::map<std::string,MountedPerson>::iterator i=passengers[b->getHandle()].begin();i!=passengers[b->getHandle()].end();++i)if(sameHandle(i->second.person,carrier->getHandle())) {
            if(driveState(b).body.speed()>2){brakeDrive(drives[b->getHandle()]);i->second.exiting=true;}else releasePassenger(b,i->first);
            return true;
        }return false;
    }
    if(action<PlaceDriverVehicle && action!=SitPassenger)return false;
    if(!eligible(b,carrier)||driveState(b).body.speed()>2){messages[b->getHandle()]="Stop beside the vehicle first.";return true;}
    if(action==UnloadPeopleVehicle) {
        for(unsigned n=0;n<5;++n)releasePassenger(b,personMounts[n],false,carrier);messages[b->getHandle()]="Passengers and prisoners unloaded.";return true;
    }
    std::string slot,design;
    if(action==PlaceDriverVehicle)slot="seat_driver";
    if(action==PlaceRearLeftVehicle)slot="seat_passenger_l";
    if(action==PlaceRearRightVehicle)slot="seat_passenger_r";
    if(action==PlaceFrontPoleVehicle){slot="front_attachment";design="155-Project Cars.mod";}
    if(action==PlaceRearPoleVehicle){slot="cargo";design="155-Project Cars.mod";}
    if(action==PlaceCageVehicle){slot="cargo";design="156-Project Cars.mod";}
    if(action==SitPassenger)slot=availablePersonMount(b,"seat_passenger_l")?"seat_passenger_l":"seat_passenger_r";
    if(action==ManTurretVehicle){slot="cargo";design=cars::turretItemId;}
    if(!availablePersonMount(b,slot,design)){messages[b->getHandle()]="This mount is unavailable or occupied.";return true;}
    Character* person=(action==SitPassenger||action==ManTurretVehicle)?carrier:carrier->getCarryingObject().getCharacter();
    if(action==ManTurretVehicle && !cars::drivenVehicle(carrier).isNull()){messages[b->getHandle()]="Get out of the driver seat first.";return true;}
    if(!person||!person->isValid()||passengerOrder(person)){messages[b->getHandle()]="Carry a person to place them into this mount.";return true;}
    const hand identity=person->getHandle();
    if(person!=carrier){carrier->dropCarriedObject(false,false);person=identity.getCharacter();if(!person||person->isBeingCarried()||!carrier->getCarryingObject().isNull()){messages[b->getHandle()]="Could not release the carried person.";return true;}}
    MountedPerson entry;entry.person=identity;passengers[b->getHandle()][slot]=entry;
    if(person->getOrdersReciever()){person->getOrdersReciever()->clearOrders();person->getOrdersReciever()->clearCurrentGoal(true);}
    person->carryModeT(true,false,false);passengers[b->getHandle()][slot].prepared=true;b->notifyChange();
    messages[b->getHandle()]=action==ManTurretVehicle?"Gunner assigned.":"Occupant secured.";return true;
}
void updatePassengers(Building* b) {
    if(!passengers.count(b->getHandle()))return;
    std::map<std::string,MountedPerson>& list=passengers[b->getHandle()];
    for(std::map<std::string,MountedPerson>::iterator i=list.begin();i!=list.end();) {
        const std::string slot=i->first;MountedPerson& p=i->second;++i;
        Character* who=p.person.getCharacter();if(!who||!who->isValid()||!who->getMovement())continue;
        if(!fitted(b,slot) || (p.exiting && drives[b->getHandle()].body.speed()<=2)){releasePassenger(b,slot,!fitted(b,slot));continue;}
        if(!p.prepared){who->carryModeT(true,false,false);p.prepared=true;}
        if(who->isRagdoll())who->ragdollMode(false,RagdollPart::WHOLE);
        Ogre::Quaternion facing=b->rot*Ogre::Quaternion(Ogre::Degree(slot=="seat_driver"||slot=="front_attachment"?90:270),Ogre::Vector3::UNIT_Y);
        const bool gunner=slot=="cargo" && fittedTurret(b);
        if(gunner)facing=turretFacing(b);
        const Ogre::Vector3 anchor=gunner?gunnerAnchor(b):rigWorld(b,personAnchor(b,slot));
        who->teleport(anchor,facing);who->ragdollNavmeshPosition=anchor;
        AnimationClass* anim=who->getAnimationClass();
        if(anim && anim->node){
            const Ogre::Vector3 render=anchor-b->getRootNode()->getCreator()->getRelativeOrigin();
            Ogre::SceneNode* parent=anim->node->getParentSceneNode();
            anim->node->setPosition(parent?parent->convertWorldToLocalPosition(render):render);
            anim->node->setOrientation(parent?parent->_getDerivedOrientation().Inverse()*facing:facing);
        }
        if(ou->zoneMgr)ou->zoneMgr->characterGrid.update(who);
    }
}
