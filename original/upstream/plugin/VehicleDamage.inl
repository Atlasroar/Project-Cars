// Included after DriveState; all physical mutations use the post-simulation loop.
struct ModuleCollision {
    std::vector<SimplePhysXEntity*> parts;std::vector<PhysicsCollection::StaticEnt*> visuals;
};
std::map<hand,std::map<std::string,ModuleCollision> > moduleCollisions;
void forgetModuleCollisions(Building* b){moduleCollisions.erase(b->getHandle());}
template<class T> void forgetPart(lektor<T*>& list,T* part) {
    for(unsigned n=0;n<list.size();++n)if(list[n]==part){for(unsigned k=n+1;k<list.size();++k)list[k-1]=list[k];--list.count;return;}
}
void syncModuleCollisions(Building* b,PhysicsActual* physics) {
    cars::VehicleFaultScope trace("component collider reconciliation");
    detachUnsupportedPlates(b);
    if(!b->physical||!b->getRootNode())return;
    std::map<std::string,ModuleCollision>& current=moduleCollisions[b->getHandle()];
    std::set<std::string> wanted;
    for(size_t n=0;n<cars::workshopMountCount;++n) {
        const cars::WorkshopSlot& slot=cars::workshopMounts[n];Item* item=fittedItem(b,slot.id);
        if(item && fitted(b,slot.id))wanted.insert(cars::workshopComponent(slot,item->getGameData()->stringID));
    }
    for(std::map<std::string,ModuleCollision>::iterator i=current.begin();i!=current.end();) {
        if(wanted.count(i->first)){++i;continue;}
        for(unsigned n=0;n<i->second.parts.size();++n){
            cars::VehicleFaultScope trace("component collider native teardown");
            SimplePhysXEntity* part=i->second.parts[n];void** vt=*reinterpret_cast<void***>(part);
            if(!cars::executableAddress(vt[1]))throw std::runtime_error("Invalid module teardown entry");
            // PhysicsCollection's destructor uses slot 1. It releases the hidden
            // render entity before queuing physics deletion; destroy alone does not.
            forgetPart(b->physical->parts,part);reinterpret_cast<void(*)(void*)>(vt[1])(part);
        }
        for(unsigned n=0;n<i->second.visuals.size();++n){forgetPart(b->physical->staticEnts,i->second.visuals[n]);delete i->second.visuals[n];}
        current.erase(i++);
    }
    size_t visualCount=0;
    const cars::WorkshopVisual* visuals=frameVisuals(b,visualCount);
    for(size_t n=0;n<visualCount;++n) {
        const cars::WorkshopVisual& v=visuals[n];if(!wanted.count(v.component)||current.count(v.component))continue;
        GameData* part=ou->gamedata.getData(v.partId,BUILDING_PART);if(!part)throw std::runtime_error("Missing vehicle collision definition");
        const unsigned first=b->physical->parts.size(),visual=b->physical->staticEnts.size();
        // Native construction preserves owner userdata, offsets, scale, material
        // and asynchronous actor lifetime. It owns the returned entities.
        {cars::VehicleFaultScope trace("component collider native creation");b->loadPartEntity(b->physical,part,b->getRootNode(),50,1,b->getGameData());}
        ModuleCollision& added=current[v.component];
        for(unsigned k=first;k<b->physical->parts.size();++k){
            SimplePhysXEntity* piece=b->physical->parts[k];added.parts.push_back(piece);
            // loadPartEntity seeds PhysFileParams from the building root. Exported
            // modules use local vertices, so add their mount offset before the
            // queued native creation runs. Frame rotation and scale apply once.
            char* params=*reinterpret_cast<char**>(reinterpret_cast<char*>(piece)+8);
            if(!params)throw std::runtime_error("Missing module physics parameters");
            Ogre::Vector3& position=*reinterpret_cast<Ogre::Vector3*>(params+0x28);
            position+=b->rot*(Ogre::Vector3(v.x,v.y,v.z)*b->getScale());
        }
        for(unsigned k=visual;k<b->physical->staticEnts.size();++k)added.visuals.push_back(b->physical->staticEnts[k]);
        if(added.parts.empty())ErrorLog("Project Cars: component collider did not load / "+std::string(v.component));
    }
    hideBatchedFrame(b);
}
std::string struckMount(Building* b,const cars::RigPoint& worldFrom,const cars::RigPoint& worldTo,double radius) {
    const double scale=b->getScale();if(scale<=0)return "";
    cars::BodyRotation q(b->rot.w,b->rot.x,b->rot.y,b->rot.z);
    cars::RigPoint a=q.inverse().rotate(worldFrom-point(b->getPosition()))*(1/scale),z=q.inverse().rotate(worldTo-point(b->getPosition()))*(1/scale);
    double time=1;std::string result;
    for(size_t n=0;n<cars::workshopMountCount;++n) {
        const cars::WorkshopSlot& slot=cars::workshopMounts[n];Item* item=fittedItem(b,slot.id);if(!item||!fitted(b,slot.id))continue;
        const std::string component=cars::workshopComponent(slot,item->getGameData()->stringID);
        if(xsmallFrame(b)) {
            size_t count=0;
            const cars::WorkshopVisual* visuals=frameVisuals(b,count);
            const cars::PickPlane (*planes)[6]=compactPickPlanes(b);
            for(size_t h=1;h<count;++h)
                if(component==visuals[h].component &&
                   cars::sweptPartContact(a,z,planes[h],6,radius/scale,time))result=slot.id;
            continue;
        }
        for(unsigned h=0;h<cars::vehiclePickHullCount;++h) {
            const cars::PickHull& hull=cars::vehiclePickHulls[h];if(component!=hull.component)continue;
            if(cars::sweptPartContact(a,z,cars::vehiclePickPlanes+hull.first,hull.count,radius/scale,time))result=slot.id;
        }
    }
    if(xsmallFrame(b)) {
        if(result.empty() && cars::sweptPartContact(a,z,compactPickPlanes(b)[0],6,radius/scale,time))return "frame";
        return result;
    }
    if(result.empty())for(unsigned h=0;h<cars::vehiclePickHullCount;++h){
        const cars::PickHull& hull=cars::vehiclePickHulls[h];if(std::string(hull.component)!="Frame")continue;
        if(cars::sweptPartContact(a,z,cars::vehiclePickPlanes+hull.first,hull.count,radius/scale,time)){result="frame";break;}
    }
    return result;
}
void damageMount(Building* b,const std::string& slot,double amount) {
    Item* item=fittedItem(b,slot);if(!item||!_finite(amount)||amount<=0)return;
    const double maximum=cars::workshopMaxHp(item->getGameData()->stringID);
    const double old=item->chargesLeft;
    item->chargesLeft=float(std::max(0.0,cars::bounded(old,0,1)-amount/maximum));b->notifyChange();
    if(old>0 && item->chargesLeft==0){detachUnsupportedPlates(b);messages[b->getHandle()]="Destroyed: "+std::string(mount(slot)->name);cars::updateFittedVisuals(b);}
}
struct QueuedVehicleHit {hand vehicle;cars::RigPoint from,to;double damage;};
std::vector<QueuedVehicleHit> vehicleHits;
HitMaterialType (*originalVehicleMelee)(UseableStuff*,CutDirection,Damages&,Character*,CombatTechniqueData*,int)=0;
HitMaterialType vehicleMelee(UseableStuff* object,CutDirection dir,Damages& d,Character* attacker,CombatTechniqueData* attack,int combo) {
    if(frame(object) && attacker && _finite(d.cut+d.blunt+d.pierce)) {
        Lock lock;if(vehicleHits.size()<256){QueuedVehicleHit h;h.vehicle=object->getHandle();h.from=point(attacker->getPosition())+cars::RigPoint(0,10,0);h.to=point(object->getPosition())+cars::RigPoint(0,10,0);h.damage=std::max(0.f,d.cut)+std::max(0.f,d.blunt)+std::max(0.f,d.pierce);vehicleHits.push_back(h);}
    }
    return originalVehicleMelee(object,dir,d,attacker,attack,combo);
}
void applyVehicleHits() {
    std::vector<QueuedVehicleHit> hits;hits.swap(vehicleHits);
    for(unsigned n=0;n<hits.size();++n){Building* b=hits[n].vehicle.getBuilding();if(b&&b->isValid())damageMount(b,struckMount(b,hits[n].from,hits[n].to,2),hits[n].damage);}
}
struct RamPartWeights {
    MedicalSystem* med;float saved[64];unsigned count;
    RamPartWeights(MedicalSystem* m,MedicalSystem::HealthPartStatus* chosen):med(m),count(0) {
        if(m->anatomy.size()>64)return;
        for(unsigned n=0;n<m->anatomy.size();++n)if(!m->anatomy[n])return;
        count=m->anatomy.size();for(unsigned n=0;n<count;++n){saved[n]=m->anatomy[n]->hitChance;m->anatomy[n]->hitChance=m->anatomy[n]==chosen?100.f:0.f;}
    }
    ~RamPartWeights(){for(unsigned n=0;n<count;++n)med->anatomy[n]->hitChance=saved[n];}
};
struct RamContact {double at;RamContact():at(-10){}};
std::map<hand,std::map<hand,std::map<std::string,RamContact> > > ramContacts;
cars::NativeQueryRelease nativeNearbyRelease=0;
bool attachedOccupant(Building*,const hand&);
bool ramActorReady(Character* actor) {
    if(!actor || !actor->isValid() || !actor->isPhysical())return false;
    AnimationClass* animation=actor->getAnimationClass();
    return animation && animation->getIsActivated() && animation->node && animation->skeleton &&
        animation->appearance && animation->appearance->body && !animation->appearance->isCreatingBody;
}
void deliverRamWound(MedicalSystem* medical,Character* driver,bool spikes,Damages& damage) {
    // Although the SDK takes RootObject*, native addWound reads the attacker's
    // Character::animation (+0x448). A Building or null attacker is invalid.
    int material=1;AttackDirection::Enum direction=AttackDirection::FRONT;
    cars::VehicleFaultScope trace("ramming native wound delivery");
    medical->addWound(false,spikes?CUT_PIERCED:CUT_DEFAULT,damage,material,driver,direction,0);
}
void vehicleRamming(Building* b,const cars::ChassisBody& before,const cars::ChassisSpec& spec) {
    cars::VehicleFaultScope trace("vehicle ramming");
    DriveState& s=drives[b->getHandle()];const double speed=std::max(before.speed(),s.body.speed());
    if(speed<12||!ou->zoneMgr||!nativeNearbyRelease)return;
    Character* driver=s.occupant.getCharacter();
    // Reacquire the real driver on this frame. Do not fabricate an animated
    // attacker for an unoccupied or still-streaming vehicle.
    if(!ramActorReady(driver))return;
    typedef std::map<std::string,RamContact> Contacts;
    std::map<hand,Contacts>& cooldown=ramContacts[b->getHandle()];
    for(std::map<hand,Contacts>::iterator i=cooldown.begin();i!=cooldown.end();) {
        for(Contacts::iterator p=i->second.begin();p!=i->second.end();)if(s.body.clock-p->second.at>2)i->second.erase(p++);else ++p;
        if(i->second.empty())cooldown.erase(i++);else ++i;
    }
    cars::NativeQueryBuffer<RootObject*> candidates(nativeNearbyRelease);lektor<RootObject*>& nearby=candidates.values;
    {cars::VehicleFaultScope trace("ramming character grid query");ou->zoneMgr->characterGrid.getObjects(vector((before.position+s.body.position)*.5),float(spec.halfLength+spec.halfWidth+(s.body.position-before.position).length()+20+speed*1.2),nearby,128);}
    for(unsigned n=0;n<nearby.size();++n) {
        Character* who=nearby[n]?nearby[n]->getHandle().getCharacter():0;
        if(!who||!who->isValid()||!who->isPhysical()||attachedOccupant(b,who->getHandle())||who->isBeingCarried())continue;
        MedicalSystem* med=who->getMedical();AnimationClass* anim=who->getAnimationClass();
        if(!med||med->anatomy.size()>64||!ramActorReady(who)||med->animation!=anim)continue;
        double threatSide=0;
        if(!who->isPlayerCharacter() && !who->isDead() && !med->isUnconcious() && who->getOrdersReciever() &&
           cars::vehicleThreat(point(who->getPosition())-before.position,s.body.velocity,spec.halfLength,spec.halfWidth,threatSide) &&
           !cooldown[who->getHandle()].count("evade")) {
            const cars::RigPoint forward=s.body.velocity.unit(),right(-forward.z,0,forward.x);
            Ogre::Vector3 escape=who->getPosition()+vector(right*(threatSide<0?-1:1)*(spec.halfWidth+16));
            const float ground=UtilityT::getTerrainHeightFromRenderer(escape,true,0);
            if(cars::boardingGroundAllowed(ground,who->getPosition().y) && ou->navmesh->getPositionValid(escape)) {
                escape.y=ground;who->getOrdersReciever()->addOrder(MOVE_CUS_ORDERED,hand(),escape,true,false);
                cooldown[who->getHandle()]["evade"].at=s.body.clock;
            }
        }
        const cars::RigPoint movement=s.body.position-before.position;
        bool hit=false,knock=false;unsigned injuries=0;
        for(unsigned p=0;p<med->anatomy.size() && injuries<4;++p) {
            cars::VehicleFaultScope trace("ramming anatomical contact");
            MedicalSystem::HealthPartStatus* part=med->anatomy[p];if(!part||!part->data||part->flesh<-part->_maxHealth)continue;
            const std::string& boneName=MedicalSystem::getBoneNameForBodypart(part->data);
            if(!anim->skeleton->Ogre::Skeleton::hasBone(boneName))continue;
            const Ogre::Vector3 local=anim->skeleton->Ogre::Skeleton::getBone(boneName)->Ogre::OldNode::_getDerivedPosition();
            const Ogre::Vector3 shift=b->getRootNode()->_getDerivedPosition()-b->getPosition();
            const cars::RigPoint contact=point(anim->node->convertLocalToWorldPosition(local)-shift);
            // Relative sweep catches a fast car crossing a limb between frames.
            // b still has the previous pose here. In its coordinates the limb
            // travels opposite to the vehicle's displacement.
            std::string slot=struckMount(b,contact,contact-movement,1.3*b->getScale());
            bool running=slot.find("wheel_")==0||slot.find("track_")==0;
            bool spikes=slot=="front_attachment" && fittedDesign(b,slot,"134-Project Cars.mod");
            if(slot.empty())continue;
            const std::string contactKey=boneName+(running?" / running gear":" / body");
            if(cooldown[who->getHandle()].count(contactKey))continue;
            cars::RamDamage injury(speed,spec.mass,running,spikes);if(injury.blunt<=0)continue;
            RamPartWeights weights(med,part);if(!weights.count)continue;
            Damages damage(0,float(injury.blunt),float(injury.pierce),1,float(injury.penetration));
            if(!cooldown[who->getHandle()].count("driver attack")) {
                // addWound alone bypasses hostility. Use the same AI response
                // as a landed native hit, before wounds can knock the victim
                // unconscious, once per contact episode rather than per limb.
                cars::VehicleFaultScope attackTrace("vehicle impact attacker notification");
                // Register the offense before hostility changes isEnemy(). The
                // native bounty manager handles faction exemptions and VIPs;
                // witnesses/guards assess the offense through their normal AI.
                if(!who->isDead() && who->getFaction() && !driver->isEnemy(who,true)) {
                    if(driver->crimes.setCrime(CRIME_ASSAULT,who->getFaction(),who->getHandle()) && driver->stateBroadcast)
                        driver->stateBroadcast->isAggressor=true;
                }
                who->attackingYou(driver,true,false);
                cooldown[who->getHandle()]["driver attack"].at=s.body.clock;
            }
            deliverRamWound(med,driver,spikes,damage);
            knock|=injury.knockdown;hit=true;++injuries;
            cooldown[who->getHandle()][contactKey].at=s.body.clock;
            damageMount(b,slot,injury.blunt*.03);
        }
        if(hit && knock){med->knockoutForceTimer(2.f);who->ragdollMode(true,RagdollPart::WHOLE);}
    }
}
