const char* lightPrefix="ProjectCars_Headlight_";
void updateVehicleLights(Building* b) {
    if(!b || !b->getRootNode() || !b->isPhysical() || !b->entitiesLoaded)return;
    Ogre::SceneNode* root=b->getRootNode();Ogre::SceneManager* scene=root->getCreator();
    DriveState& state=driveState(b);const bool present=fitted(b,"headlights");
    if(!present)state.headlightsOn=false;
    const bool on=present && state.headlightsOn && b->getVisible() && !state.streamPending;
    for(unsigned n=0;n<2;++n) {
        const std::string name=std::string(lightPrefix)+b->getHandle().toString()+(n?"_R":"_L");
        Ogre::SceneNode* node=0;
        for(unsigned k=0;k<root->numChildren();++k)if(root->getChild(k)->getName()==name){node=static_cast<Ogre::SceneNode*>(root->getChild(k));break;}
        if(node) {node->setVisible(on,true);continue;}
        if(!on)continue;
        Ogre::Light* light=0;
        try {
            light=scene->Ogre::SceneManager::createLight();light->setName(name);
            light->setType(Ogre::Light::LT_SPOTLIGHT);
            light->setSpotlightRange(Ogre::Radian(.38f),Ogre::Radian(.75f),1.f);
            light->setAttenuation(350.f,1.f,.007f,.00012f);light->setPowerScale(2.f);
            light->setCastShadows(false);light->setQueryFlags(0);
            // Lens positions from the exported Headlights hulls and pivot.
            node=root->createChildSceneNode(Ogre::SCENE_DYNAMIC,Ogre::Vector3(18.25f,9.9f,n?5.97f:-6.03f));
            node->setName(name);node->attachObject(light);light->setDirection(Ogre::Vector3(1,-.07f,0).normalisedCopy());
            node->setVisible(true,true);
        } catch(...) {
            if(node){node->detachAllObjects();scene->destroySceneNode(node);}
            if(light)scene->Ogre::SceneManager::destroyLight(light);throw;
        }
    }
}
void clearOrphanVehicleLights() {
    // A physical teardown can occur on the loading thread. It detaches these
    // objects; dispose of orphaned lights on the game thread using the current
    // scene, without storing a light or scene pointer across frames.
    static DWORD checked=0;if(GetTickCount()-checked<1000)return;checked=GetTickCount();
    if(!ou || !ou->player || !ou->player->camera || !ou->player->camera->camera)return;
    Ogre::SceneManager* scene=ou->player->camera->camera->getSceneManager();if(!scene)return;
    std::vector<Ogre::Light*> remove;
    Ogre::SceneManager::MovableObjectIterator it=scene->Ogre::SceneManager::getMovableObjectIterator("Light");
    static size_t cursor=0;const size_t count=it.end()-it.begin();
    if(!count){cursor=0;return;}cursor%=count;it.current()=it.begin()+cursor;
    const size_t limit=std::min<size_t>(256,count);
    for(size_t n=0;n<limit;++n) {
        if(!it.hasMoreElements())it.current()=it.begin();
        Ogre::MovableObject* object=it.getNext();
        if(object && object->getName().compare(0,strlen(lightPrefix),lightPrefix)==0 && !object->isAttached())remove.push_back(static_cast<Ogre::Light*>(object));
    }
    cursor=(cursor+limit)%count;
    for(size_t n=0;n<remove.size();++n)scene->Ogre::SceneManager::destroyLight(remove[n]);
}
