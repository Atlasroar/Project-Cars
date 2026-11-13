// Own one transient batch in the current scene; retain IDs and plain geometry,
// never scene/vehicle pointers. Trail points are absolute physics coordinates.
#include "VehicleBlood.inl"
cars::TreadTrail vehicleTrail;
Ogre::IdType trailNodeId=0;
cars::RigPoint trailAnchor,trailOriginShift;
double trailStamp=-1;
bool trailFailed=false,trailCleanupPending=false,trailMaterialReady=false;
const char* trailName="ProjectCars_GroundTreads";
const char* trailGroup="ProjectCarsVehicleTrails";
void (*setManualDynamic)(Ogre::ManualObject*,bool)=0;

Ogre::SceneManager* trailScene() {
    return ou && ou->initialized && !ou->gameResetting && ou->player && ou->player->camera && ou->player->camera->camera?ou->player->camera->camera->getSceneManager():0;
}
Ogre::SceneNode* findTrailNode(Ogre::SceneManager* scene) {
    if(!scene || !trailNodeId)return 0;
    Ogre::SceneNode* node=scene->Ogre::SceneManager::getSceneNode(trailNodeId);
    return node && node->getName()==trailName?node:0;
}
void destroyTrailBatch(Ogre::SceneManager* scene) {
    Ogre::SceneNode* node=findTrailNode(scene);
    if(node) {
        while(node->numAttachedObjects()) {
            Ogre::MovableObject* object=node->getAttachedObject(static_cast<size_t>(0));node->detachObject(object);
            scene->Ogre::SceneManager::destroyManualObject(static_cast<Ogre::ManualObject*>(object));
        }
        scene->destroySceneNode(node);
    }
    trailNodeId=0;trailCleanupPending=false;
}
void clearVehicleTrails() {
    cars::VehicleFaultScope trace("tread trail cleanup");
    vehicleTrail.clear();trailStamp=-1;trailFailed=false;
    clearBloodPickups();
    for(std::map<hand,DriveState>::iterator d=drives.begin();d!=drives.end();++d)for(unsigned n=0;n<4;++n)d->second.treads[n].reset();
    Ogre::SceneManager* scene=trailScene();
    if(scene)destroyTrailBatch(scene);else trailCleanupPending=true;
}
void advanceVehicleTrails(double now) {
    double elapsed=trailStamp<0?0:cars::vehicleElapsed(now-trailStamp,ou->getLengthOfHourInRealSeconds());trailStamp=now;
    if(!ou->isPaused())vehicleTrail.advance(elapsed);
    drainBloodPickups(vehicleTrail.clock);
    if(options && !options->blood){bloodGround.clear();for(std::map<hand,DriveState>::iterator d=drives.begin();d!=drives.end();++d)for(unsigned n=0;n<4;++n)d->second.treads[n].coating.clear();}
}
void recordVehicleTrails(Building* b,DriveState& state,const cars::ChassisSpec& spec) {
    if(trailFailed)return;
    const bool tracked=spec.trackMask!=0;
    const double width=(tracked?5.8:2.6)*b->getScale();
    if(vehicleTrail.segments.empty())trailAnchor=state.body.position;
    for(unsigned n=0;n<4;++n) {
        if((tracked && n>=2) || (tracked?!(spec.trackMask&(1u<<n)):!(spec.wheelMask&(1u<<n)))){state.treads[n].reset();continue;}
        const double steer=!tracked && n<2?state.body.steering:0;
        const cars::RigPoint forward=state.body.orientation.rotate(cars::RigPoint(cos(steer),0,-sin(steer)));
        state.treads[n].sample(vehicleTrail,state.body.ground[n],forward,width,tracked,.7+state.body.tireSlip*.3,&bloodGround);
    }
}
Ogre::MaterialPtr treadMaterial() {
    Ogre::ResourceGroupManager& resources=Ogre::ResourceGroupManager::getSingleton();
    if(!resources.resourceGroupExists(trailGroup))resources.createResourceGroup(trailGroup);
    const std::string directory="./mods/Project Cars/shaders";
    if(!resources.resourceLocationExists(directory,trailGroup))resources.addResourceLocation(directory,"FileSystem",trailGroup);
    if(!setManualDynamic) {
        HMODULE ogre=GetModuleHandleW(L"OgreMain_x64.dll");
        setManualDynamic=ogre?reinterpret_cast<void(*)(Ogre::ManualObject*,bool)>(GetProcAddress(ogre,"?setDynamic@ManualObject@Ogre@@UEAAX_N@Z")):0;
        if(!setManualDynamic)throw std::runtime_error("Native dynamic tread buffer entry unavailable");
    }
    Ogre::MaterialManager& materials=Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr material=materials.getByName(trailName,trailGroup);
    if(!material.isNull() && trailMaterialReady)return material;
    trailMaterialReady=false;
    Ogre::HighLevelGpuProgramManager& programs=Ogre::HighLevelGpuProgramManager::getSingleton();
    Ogre::HighLevelGpuProgramPtr fragment=programs.getByName("ProjectCars_Tread_FP",trailGroup);
    if(fragment.isNull()) {
        fragment=programs.createProgram("ProjectCars_Tread_FP",trailGroup,"hlsl",Ogre::GPT_FRAGMENT_PROGRAM);
        fragment->setSourceFile("vehicle_trails.hlsl");fragment->setParameter("entry_point","tread_fp");fragment->setParameter("target","ps_4_0");
    }
    fragment->Ogre::Resource::load();
    if(fragment->hasCompileError())throw std::runtime_error("Tread shader compilation failed");
    if(material.isNull())material=materials.create(trailName,trailGroup);
    Ogre::Pass* pass=material->getTechnique(0)->getPass(0);
    pass->setVertexProgram("Basic_Coloured_Texture_VP");pass->setFragmentProgram("ProjectCars_Tread_FP");
    pass->setLightingEnabled(false);pass->setDepthCheckEnabled(true);pass->setDepthWriteEnabled(false);
    pass->setDepthBias(1,1);pass->setCullingMode(Ogre::CULL_NONE);
    // Darken the receiving surface. Treads cannot glow on night-time terrain.
    pass->setSceneBlending(Ogre::SBF_ZERO,Ogre::SBF_ONE_MINUS_SOURCE_COLOUR);
    pass->getFragmentProgramParameters()->setNamedConstant("trailTime",float(vehicleTrail.clock));
    material->Ogre::Resource::load();
    if(!material->getBestTechnique())throw std::runtime_error("Tread shader is unsupported");
    trailMaterialReady=true;
    return material;
}
void renderVehicleTrails() {
    cars::VehicleFaultScope trace("tread trail rendering");
    static DWORD refreshed=0;
    if(GetTickCount()-refreshed<50 || trailFailed)return;refreshed=GetTickCount();
    Ogre::SceneManager* scene=trailScene();if(!scene)return;
    try {
        if(trailCleanupPending)destroyTrailBatch(scene);
        Ogre::SceneNode* node=findTrailNode(scene);
        if(vehicleTrail.segments.empty()){if(node)node->setVisible(false,true);return;}
        // Native camera-relative rendering remains available after a car unloads.
        trailOriginShift=point(scene->Ogre::SceneManager::getRelativeOrigin())*-1;
        Ogre::MaterialPtr material=treadMaterial();
        material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant("trailTime",float(vehicleTrail.clock));
        Ogre::ManualObject* mesh=0;
        if(!node) {
            mesh=scene->Ogre::SceneManager::createManualObject(Ogre::SCENE_DYNAMIC);
            try {
                mesh->setName(trailName);mesh->setCastShadows(false);mesh->setQueryFlags(0);
                // The native compositor draws queues 20-80 into the G-buffer.
                // This colour/alpha shader belongs to the later forward pass.
                mesh->Ogre::MovableObject::setRenderQueueGroup(83);
                setManualDynamic(mesh,true);mesh->Ogre::ManualObject::estimateVertexCount(cars::maxTreadSegments*4);mesh->Ogre::ManualObject::estimateIndexCount(cars::maxTreadSegments*6);
                node=scene->getRootSceneNode()->createChildSceneNode(Ogre::SCENE_DYNAMIC);node->setName(trailName);node->attachObject(mesh);trailNodeId=node->getId();
            }catch(...){if(node)scene->destroySceneNode(node);scene->Ogre::SceneManager::destroyManualObject(mesh);throw;}
        }else if(node->numAttachedObjects()==1)mesh=static_cast<Ogre::ManualObject*>(node->getAttachedObject(static_cast<size_t>(0)));
        if(!mesh)throw std::runtime_error("Tread batch lost its render object");
        const cars::RigPoint camera=point(ou->player->camera->camera->getDerivedPosition())-trailOriginShift;
        const double range=options?cars::bounded(options->decalRange,0,2000):1200;
        const Ogre::Vector3 world=vector(trailAnchor+trailOriginShift);
        Ogre::SceneNode* parent=node->getParentSceneNode();node->setPosition(parent?parent->convertWorldToLocalPosition(world):world);
        node->setOrientation(parent?parent->_getDerivedOrientation().Inverse():Ogre::Quaternion::IDENTITY);
        if(mesh->Ogre::ManualObject::getNumSections())mesh->Ogre::ManualObject::beginUpdate(0);
        else mesh->Ogre::ManualObject::begin(trailName,Ogre::RenderOperation::OT_TRIANGLE_LIST,trailGroup);
        unsigned vertex=0;
        for(size_t n=0;n<vehicleTrail.segments.size();++n) {
            const cars::TreadSegment& s=vehicleTrail.segments[n];
            const double distance=((s.corners[0]+s.corners[2])*.5-camera).length();
            if(range<=0 || distance>=range)continue;
            const float strength=float(s.strength*vehicleTrail.pressureFade(n)*cars::bounded((range-distance)/std::max(1.0,range*.2),0,1));
            for(unsigned k=0;k<4;++k) {
                mesh->Ogre::ManualObject::position(vector(s.corners[k]-trailAnchor));mesh->Ogre::ManualObject::colour(options && !options->blood?0.f:float(s.blood),0,0,strength);
                mesh->Ogre::ManualObject::textureCoord(k==1||k==2?1.f:0.f,float(k<2?s.v0:s.v1),float(s.born),s.belt?1.f:0.f);
            }
            mesh->Ogre::ManualObject::triangle(vertex,vertex+1,vertex+2);mesh->Ogre::ManualObject::triangle(vertex,vertex+2,vertex+3);vertex+=4;
        }
        mesh->Ogre::ManualObject::end();node->setVisible(vertex>0,true);
    }catch(const std::exception& e) {
        trailFailed=true;Ogre::SceneNode* node=findTrailNode(scene);if(node)node->setVisible(false,true);
        ErrorLog(std::string("Project Cars: ground treads disabled: ")+e.what());
    }
}
