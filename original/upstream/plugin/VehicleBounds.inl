// Included after nodePrefix and frame(). These replace only the bounding-box
// calculations for our frames; ordinary buildings retain their native methods.
void (*originalUpdateWorldAabb)(Building*,Ogre::MovableObject*)=0;
void (*originalRecalculateWorldAabb)(Building*)=0;
bool rebuildVehicleBounds(Building* b) {
    const Ogre::Vector3 p=b->getPosition();
    cars::VehicleBounds bounds;
    if(!bounds.frame(cars::RigPoint(p.x,p.y,p.z),cars::BodyRotation(b->rot.w,b->rot.x,b->rot.y,b->rot.z),b->getScale()))return false;
    Ogre::SceneNode* root=b->getRootNode();
    if(root) {
        // Native Building::updateWorldAABB adds this once to render coordinates.
        const Ogre::Vector3 offset=root->getCreator()->getRelativeOrigin();
        for(unsigned n=0;n<root->numChildren();++n) {
            Ogre::SceneNode* child=static_cast<Ogre::SceneNode*>(root->getChild(n));
            if(child->getName().compare(0,strlen(nodePrefix),nodePrefix)!=0)continue;
            for(unsigned k=0;k<child->numAttachedObjects();++k) {
                const Ogre::Aabb box=child->getAttachedObject(static_cast<size_t>(k))->getWorldAabbUpdated();
                const Ogre::Vector3 center=box.mCenter+offset;
                bounds.add(cars::RigPoint(center.x,center.y,center.z),cars::RigPoint(box.mHalfSize.x,box.mHalfSize.y,box.mHalfSize.z));
            }
        }
    }
    const cars::RigPoint c=bounds.center(),h=bounds.half();
    b->setAABB(Ogre::Aabb(Ogre::Vector3(float(c.x),float(c.y),float(c.z)),Ogre::Vector3(float(h.x),float(h.y),float(h.z))));
    return true;
}
void vehicleUpdateWorldAabb(Building* b,Ogre::MovableObject* entity) {
    if(!frame(b)){originalUpdateWorldAabb(b,entity);return;}
    // loadEntityCallback calls this before its final onBuildingLoaded call.
    // Do not merge hidden, asynchronous collider meshes: unloaded ones have
    // infinite boxes, and old building instances can retain their spawn bounds.
    cars::VehicleFaultScope trace("vehicle bounds after mesh load");
    rebuildVehicleBounds(b);
}
void vehicleRecalculateWorldAabb(Building* b) {
    if(!frame(b)){originalRecalculateWorldAabb(b);return;}
    cars::VehicleFaultScope trace("vehicle bounds after movement");
    rebuildVehicleBounds(b);
}
