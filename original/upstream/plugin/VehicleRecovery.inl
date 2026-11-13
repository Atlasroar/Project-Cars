struct RecoveryPlacement {
    hand vehicle,actor;Ogre::IdType node;cars::RigPoint target,workerStart;
    cars::BodyRotation rotation;unsigned request;DWORD changed;bool ground,reachable;
    std::string status;Ogre::Ray cursor;bool inputPending,confirm;int turn;
    RecoveryPlacement():node(0),request(0),changed(0),ground(false),reachable(false),inputPending(false),confirm(false),turn(0){}
} recoveryPlacement;
Ogre::SceneNode* recoveryNode() {
    Ogre::SceneManager* scene=trailScene();if(!scene || !recoveryPlacement.node)return 0;
    Ogre::SceneNode* n=scene->Ogre::SceneManager::getSceneNode(recoveryPlacement.node);
    return n && n->getName()=="ProjectCars_RecoveryGhost"?n:0;
}
void cancelRecovery() {
    cancelNativePath(recoveryPlacement.request);
    Ogre::SceneNode* node=recoveryNode();
    if(node){Ogre::SceneManager* scene=node->getCreator();
        while(node->numChildren()){Ogre::SceneNode* c=static_cast<Ogre::SceneNode*>(node->getChild(size_t(0)));while(c->numAttachedObjects()){Ogre::MovableObject* e=c->getAttachedObject(size_t(0));c->detachObject(e);scene->destroyEntity(static_cast<Ogre::Entity*>(e));}scene->destroySceneNode(c);}scene->destroySceneNode(node);}
    recoveryPlacement=RecoveryPlacement();
}
Ogre::MaterialPtr recoveryMaterial(bool valid) {
    const char* name=valid?"ProjectCars_RecoveryValid":"ProjectCars_RecoveryInvalid";
    Ogre::MaterialManager& mm=Ogre::MaterialManager::getSingleton();Ogre::MaterialPtr mat=mm.getByName(name);
    if(!mat.isNull())return mat;
    Ogre::HighLevelGpuProgramManager& pm=Ogre::HighLevelGpuProgramManager::getSingleton();
    if(pm.getByName("ProjectCars_GhostVP").isNull()) {
        Ogre::HighLevelGpuProgramPtr vp=pm.createProgram("ProjectCars_GhostVP","General","hlsl",Ogre::GPT_VERTEX_PROGRAM);
        vp->setSource("float4x4 wvp; float4 main(float4 p:POSITION):SV_POSITION{return mul(wvp,p);}");vp->setParameter("entry_point","main");vp->setParameter("target","vs_4_0");vp->Ogre::Resource::load();
        Ogre::HighLevelGpuProgramPtr fp=pm.createProgram("ProjectCars_GhostFP","General","hlsl",Ogre::GPT_FRAGMENT_PROGRAM);
        fp->setSource("float4 tint;float4 main():SV_TARGET{return tint;}");fp->setParameter("entry_point","main");fp->setParameter("target","ps_4_0");fp->Ogre::Resource::load();
        if(vp->hasCompileError()||fp->hasCompileError())throw std::runtime_error("Recovery ghost shader unavailable");
    }
    mat=mm.create(name,"General");Ogre::Pass* pass=mat->getTechnique(0)->getPass(0);
    pass->setVertexProgram("ProjectCars_GhostVP");pass->setFragmentProgram("ProjectCars_GhostFP");
    pass->getVertexProgramParameters()->setNamedAutoConstant("wvp",Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
    pass->getFragmentProgramParameters()->setNamedConstant("tint",valid?Ogre::Vector4(.25f,1,.4f,.35f):Ogre::Vector4(1,.15f,.1f,.35f));
    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);pass->setDepthWriteEnabled(false);pass->setLightingEnabled(false);return mat;
}
bool beginRecovery(Building* b,Character* actor) {
    DriveState& s=driveState(b);
    if(!s.occupant.isNull() || passengerCount(b)){messages[b->getHandle()]="Remove occupants before recovery.";return false;}
    if(actor->getPosition().squaredDistance(b->getPosition())>250000){messages[b->getHandle()]="Move within 50 metres to recover the vehicle.";return false;}
    cancelRecovery();recoveryPlacement.vehicle=b->getHandle();recoveryPlacement.actor=actor->getHandle();
    const double yaw=atan2(-s.body.forward().z,s.body.forward().x);recoveryPlacement.rotation=cars::BodyRotation(cos(yaw*.5),0,sin(yaw*.5),0);
    try {
        Ogre::SceneManager* scene=b->getRootNode()->getCreator();Ogre::SceneNode* root=scene->getRootSceneNode()->createChildSceneNode(Ogre::SCENE_DYNAMIC);
        root->setName("ProjectCars_RecoveryGhost");recoveryPlacement.node=root->getId();root->setScale(b->getScale(),b->getScale(),b->getScale());root->setVisible(false,true);
        std::set<std::string> components;components.insert("Frame");
        for(unsigned n=0;n<cars::workshopMountCount;++n){const cars::WorkshopSlot& slot=cars::workshopMounts[n];Item* item=fittedItem(b,slot.id);if(item && fitted(b,slot.id))components.insert(cars::workshopComponent(slot,item->getGameData()->stringID));}
        size_t visualCount=0;
        const cars::WorkshopVisual* visuals=frameVisuals(b,visualCount);
        for(size_t n=0;n<visualCount;++n){const cars::WorkshopVisual& v=visuals[n];if(!components.count(v.component))continue;
            Ogre::Entity* e=scene->createEntity(registerMesh(v.mesh),"ProjectCarsVehicleParts");e->setMaterialName(recoveryMaterial(false)->getName());e->setCastShadows(false);e->setQueryFlags(0);e->setRenderQueueGroup(83);
            Ogre::SceneNode* c=root->createChildSceneNode(Ogre::SCENE_DYNAMIC,Ogre::Vector3(v.x,v.y,v.z));c->attachObject(e);}
    }catch(const std::exception& e){ErrorLog(std::string("Project Cars: could not create recovery preview: ")+e.what());cancelRecovery();messages[b->getHandle()]="Could not create recovery preview. See the Project Cars log.";return false;}
    catch(...){ErrorLog("Project Cars: could not create recovery preview");cancelRecovery();messages[b->getHandle()]="Could not create recovery preview. See the Project Cars log.";return false;}
    recoveryPlacement.status="Choose reachable dry ground. Left click: place. Q/E: rotate. Escape/right click: cancel.";
    DebugLog("Project Cars: recovery placement opened / "+b->getHandle().toString());
    return true;
}
bool recoveryGround(Building* b,Character* worker,NativeRoad& road,const cars::RigPoint& p,double& height) {
    cars::ChassisSpec spec=driveSpec(b);double radius=sqrt(spec.halfLength*spec.halfLength+spec.halfWidth*spec.halfWidth);
    if((p-point(worker->getPosition())).length()>500 || !road.driveable(p,radius,height) || height<=105)return false;
    // Recovery needs stable footing, stricter than ordinary off-road driving.
    for(unsigned n=0;n<8;++n){double a=n*3.141592653589793/4;cars::RigPoint edge=p+cars::RigPoint(cos(a)*radius,0,sin(a)*radius);double h=road.terrainHeight(edge);if(!cars::boardingTerrainAvailable(h)||fabs(h-height)>radius*.3)return false;
        cars::SurfaceHit hit;if(road.ray(edge+cars::RigPoint(0,h-edge.y+1,0),edge+cars::RigPoint(0,h-edge.y+spec.cageHeight+12,0),hit))return false;
        if(road.ray(cars::RigPoint(p.x,height+spec.cageHeight+10,p.z),cars::RigPoint(edge.x,h+spec.cageHeight+10,edge.z),hit))return false;}
    return true;
}
void updateRecovery(const Ogre::Ray& cursor,bool confirm,int turn) {
    Building* b=recoveryPlacement.vehicle.getBuilding();Character* worker=recoveryPlacement.actor.getCharacter();Ogre::SceneNode* ghost=recoveryNode();
    if(!b || !worker || !b->isValid() || !worker->isValid() || !b->getRootNode() || !ghost){cancelRecovery();return;}
    DriveState& s=driveState(b);if(!s.occupant.isNull() || passengerCount(b)){cancelRecovery();return;}
    // Called after native physics completes, just like driving queries. Never
    // change live PhysX shape groups from the render/UI update.
    cars::CollisionQueryMask mask(b,worker);if(!mask.ready()){recoveryPlacement.status="Waiting for vehicle collision data. Escape cancels recovery.";return;}
    NativeRoad road(b,s.routeReport);cars::SurfaceHit hit;
    Ogre::Vector3 shift=b->getRootNode()->_getDerivedPosition()-b->getPosition();
    cars::RigPoint from=point(cursor.getOrigin()-shift),to=from+point(cursor.getDirection())*50000;
    if(!road.ray(from,to,hit)){ghost->setVisible(false,true);recoveryPlacement.reachable=false;recoveryPlacement.status="Point at nearby ground.";return;}
    double height=0;bool good=recoveryGround(b,worker,road,hit.point,height);
    cars::RigPoint target(hit.point.x,height,hit.point.z);
    bool changed=(target-recoveryPlacement.target).length()>1 || turn || (point(worker->getPosition())-recoveryPlacement.workerStart).length()>5;
    if(changed){cancelNativePath(recoveryPlacement.request);recoveryPlacement.changed=GetTickCount();recoveryPlacement.reachable=false;recoveryPlacement.target=target;recoveryPlacement.workerStart=point(worker->getPosition());}
    if(turn){double a=turn*.130899694;recoveryPlacement.rotation=cars::BodyRotation(cos(a),0,sin(a),0)*recoveryPlacement.rotation;}
    recoveryPlacement.ground=good;
    if(good && !recoveryPlacement.request && !recoveryPlacement.reachable && GetTickCount()-recoveryPlacement.changed>=200) {
        // Validate a pedestrian entrance beside the footprint, not beneath it.
        cars::RigPoint door=target+recoveryPlacement.rotation.rotate(cars::RigPoint(0,0,driveSpec(b).halfWidth+8));door.y=road.terrainHeight(door);
        recoveryPlacement.request=requestNativePath(worker,point(worker->getPosition()),door,1);
        if(!recoveryPlacement.request)recoveryPlacement.changed=GetTickCount();
    }
    if(recoveryPlacement.request){std::vector<cars::RigPoint> path;int result=pollNativePath(recoveryPlacement.request,path);if(result){recoveryPlacement.reachable=result>0;if(result<0){cancelNativePath(recoveryPlacement.request);recoveryPlacement.changed=GetTickCount();}}}
    bool valid=good && recoveryPlacement.reachable;
    recoveryPlacement.status=valid?"Left click: place vehicle. Q/E: rotate. Escape/right click: cancel.":good?"Checking a walking route to the vehicle...":"Choose clear, dry, gentle ground within 50 metres.";
    cars::ChassisSpec spec=driveSpec(b);cars::RigPoint center=target;center.y=height+(xsmallFrame(b) && !spec.wheelMask?spec.halfHeight:-spec.mountHeight+spec.radius+spec.travel*.65);
    Ogre::Vector3 origin=vector(center-recoveryPlacement.rotation.rotate(massCenter(b)));
    ghost->setPosition(origin+shift);ghost->setOrientation(quaternion(recoveryPlacement.rotation));ghost->setVisible(true,true);
    for(unsigned n=0;n<ghost->numChildren();++n){Ogre::SceneNode* c=static_cast<Ogre::SceneNode*>(ghost->getChild(n));for(unsigned j=0;j<c->numAttachedObjects();++j)static_cast<Ogre::Entity*>(c->getAttachedObject(j))->setMaterialName(recoveryMaterial(valid)->getName());}
    if(!confirm || !valid || !mask.restore())return;
    stopDrive(s);s.body.position=center;s.body.orientation=recoveryPlacement.rotation;s.body.velocity=s.body.angular=cars::RigPoint();s.flooded=false;
    s.fault=cars::NoDriveFault;s.retryPhysics=false;s.recovery.setNull();s.stamp=-1;s.accumulator=0;
    s.explicitRebuild=true;try{b->_NV_destroyPhysical();b->pos=origin;b->rot=quaternion(s.body.orientation);b->_NV_createPhysical();}catch(...){s.explicitRebuild=false;throw;}s.explicitRebuild=false;
    b->setVisible(true);b->notifyChange();b->instanceID.notifyChange();ou->zoneMgr->buildingGrid.update(b);
    messages[b->getHandle()]="Vehicle recovered. Engine off.";DebugLog("Project Cars: recovery placement completed / "+b->getHandle().toString());cancelRecovery();
}
void advanceRecoveryPhysics() {
    if(recoveryPlacement.vehicle.isNull() || !recoveryPlacement.inputPending)return;
    const Ogre::Ray cursor=recoveryPlacement.cursor;const bool confirm=recoveryPlacement.confirm;const int turn=recoveryPlacement.turn;
    recoveryPlacement.inputPending=recoveryPlacement.confirm=false;recoveryPlacement.turn=0;
    try {updateRecovery(cursor,confirm,turn);}
    catch(const std::exception& e){recoveryPlacement.status="Recovery preview failed. Escape cancels recovery.";ErrorLog(std::string("Project Cars: recovery preview failed: ")+e.what());}
    catch(...){recoveryPlacement.status="Recovery preview failed. Escape cancels recovery.";ErrorLog("Project Cars: recovery preview failed");}
}
