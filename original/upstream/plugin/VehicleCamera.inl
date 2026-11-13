// Included by VehicleControls. All native objects are resolved for this callback;
// the saved camera identity is compared only, never retained for dereferencing.
cars::VehicleCameraState vehicleView;
hand cameraVehicle;
float firstPersonVehicleFov=75;
struct VehicleCameraRestore {
    size_t identity;LONG epoch;bool saved,freeCamera;float fov,nearClip;
    Ogre::Vector3 cameraPosition,nodePosition;Ogre::Quaternion cameraRotation,nodeRotation,centerRotation;
    VehicleCameraRestore():identity(0),epoch(0),saved(false),freeCamera(false),fov(1),nearClip(1){}
} cameraRestore;
cars::VehicleCameraHold cameraHold;
DWORD vehicleCameraStamp=0;
bool pointerOwned=false,pointerWasVisible=true,middleHeld=false;
POINT pointerReturn;
bool vehicleForeground() {
    DWORD process=0;HWND window=GetForegroundWindow();if(window)GetWindowThreadProcessId(window,&process);
    return process==GetCurrentProcessId();
}
void releaseVehiclePointer() {
    if(!pointerOwned)return;
    if(MyGUI::PointerManager::getInstancePtr())MyGUI::PointerManager::getInstance().setVisible(pointerWasVisible);
    if(vehicleForeground())SetCursorPos(pointerReturn.x,pointerReturn.y);
    pointerOwned=false;
}
void updateVehiclePointer(bool allowed) {
    const bool middle=(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;
    if(allowed && middle && !middleHeld)vehicleView.toggleMouse();middleHeld=middle;
    const bool capture=allowed && vehicleView.firstPerson() && vehicleView.mouseLook && vehicleForeground();
    if(!capture){releaseVehiclePointer();return;}
    HWND window=GetForegroundWindow();RECT client;POINT center,cursor;
    if(!GetClientRect(window,&client)||!GetCursorPos(&cursor)){releaseVehiclePointer();return;}
    center.x=(client.left+client.right)/2;center.y=(client.top+client.bottom)/2;
    if(!ClientToScreen(window,&center)){releaseVehiclePointer();return;}
    if(!pointerOwned) {
        pointerReturn=cursor;pointerWasVisible=MyGUI::PointerManager::getInstancePtr()?MyGUI::PointerManager::getInstance().isVisible():true;
        pointerOwned=true;
    } else vehicleView.look(cursor.x-center.x,cursor.y-center.y);
    if(MyGUI::PointerManager::getInstancePtr())MyGUI::PointerManager::getInstance().setVisible(false);
    SetCursorPos(center.x,center.y);
}
Ogre::Camera* vehicleRenderCamera(CameraClass* camera) {
    // Native getCameraPos uses CameraClass +0x68. This is its Ogre camera,
    // not a guessed attached-object slot (Kenshi's camera has a local offset).
    return camera && camera->isInitialised()?camera->camera:0;
}
void restoreVehicleCamera(CameraClass* camera) {
    releaseVehiclePointer();
    Ogre::Camera* render=vehicleRenderCamera(camera);
    if(cameraRestore.saved && render && reinterpret_cast<size_t>(render)==cameraRestore.identity) {
        render->Ogre::Frustum::setFOVy(Ogre::Radian(cameraRestore.fov));
        render->Ogre::Frustum::setNearClipDistance(cameraRestore.nearClip);
        if(cameraRestore.epoch==InterlockedCompareExchange(&worldEpoch,0,0) && ou && !ou->gameResetting && camera->getCameraNode() && camera->getCenterNode()) {
            render->setPosition(cameraRestore.cameraPosition);render->setOrientation(cameraRestore.cameraRotation);
            camera->getCameraNode()->Ogre::Node::setPosition(cameraRestore.nodePosition);
            camera->getCameraNode()->Ogre::Node::setOrientation(cameraRestore.nodeRotation);
            camera->getCenterNode()->Ogre::Node::setOrientation(cameraRestore.centerRotation);
            camera->getCenterNode()->Ogre::Node::_getFullTransformUpdated();camera->getCameraNode()->Ogre::Node::_getFullTransformUpdated();
            // Keep the center at the current car, rather than jumping back to
            // wherever manual driving began. Native following resumes normally.
            if(cameraRestore.freeCamera)camera->setFreeCameraMode(true);
        }
    }
    cameraRestore.saved=false;cameraHold.valid=false;
}
void toggleVehicleView() {
    if(!cars::manualDriving())return;
    const cars::ManualVehicleView view=cars::manualVehicleView();
    if(!cars::sameHandle(cameraVehicle,view.vehicle)) {cameraVehicle=view.vehicle;vehicleView=cars::VehicleCameraState();}
    vehicleView.toggleView();releaseVehiclePointer();
}
struct VehicleCameraInputMask {
    bool* flags[13];bool saved[13];InputHandler* input;int wheel;Ogre::Vector3 speed;
    explicit VehicleCameraInputMask(InputHandler* p):input(p),wheel(p->mWheel),speed(p->mSpeed) {
        bool* values[]={&p->up,&p->down,&p->left,&p->right,&p->pgup,&p->pgdn,&p->zoomin,&p->zoomout,&p->rotate,&p->rotRight,&p->rotLeft,&p->tiltDown,&p->tiltUp};
        for(unsigned n=0;n<13;++n){flags[n]=values[n];saved[n]=*flags[n];*flags[n]=false;}
        input->mWheel=0;input->mSpeed=Ogre::Vector3::ZERO;
    }
    ~VehicleCameraInputMask(){for(unsigned n=0;n<13;++n)*flags[n]=saved[n];input->mWheel=wheel;input->mSpeed=speed;}
private:VehicleCameraInputMask(const VehicleCameraInputMask&);VehicleCameraInputMask& operator=(const VehicleCameraInputMask&);
};
cars::RigPoint vehicleRenderOrigin(Ogre::Camera* render) {
    const Ogre::Vector3 p=render->getSceneManager()->getRelativeOrigin();return cars::RigPoint(p.x,p.y,p.z);
}
void applyVehicleCameraPose(CameraClass* camera,Ogre::Camera* render,const cars::VehicleCameraPose& pose) {
    camera->getCenterNode()->Ogre::Node::_setDerivedPosition(Ogre::Vector3(float(pose.focus.x),float(pose.focus.y),float(pose.focus.z)));
    camera->getCenterNode()->Ogre::Node::_getFullTransformUpdated();
    camera->getCameraNode()->Ogre::Node::_setDerivedPosition(Ogre::Vector3(float(pose.eye.x),float(pose.eye.y),float(pose.eye.z)));
    camera->getCameraNode()->Ogre::Node::_setDerivedOrientation(Ogre::Quaternion(float(pose.rotation.w),float(pose.rotation.x),float(pose.rotation.y),float(pose.rotation.z)));
    camera->getCameraNode()->Ogre::Node::_getFullTransformUpdated();
    render->setPosition(Ogre::Vector3::ZERO);render->setOrientation(Ogre::Quaternion::IDENTITY);
    render->Ogre::Frustum::setFOVy(Ogre::Radian(vehicleView.firstPerson()?float(cars::vehicleFov(firstPersonVehicleFov)*.0174532925199433):cameraRestore.fov));
    render->Ogre::Frustum::setNearClipDistance(vehicleView.firstPerson()?.2f:cameraRestore.nearClip);
    camera->updateAudio();
}
void vehicleCameraUpdate(CameraClass* camera,bool enabled) {
    // Loading terrain is a pause in this world, not an exit from manual mode.
    // Do not resolve a car/actor or raycast while it is loading. Reapply the
    // last camera values after native update, adjusted for any origin shift.
    const cars::ManualVehicleView view=worldReady() && observedEpoch==InterlockedCompareExchange(&worldEpoch,0,0)?cars::manualVehicleView():cars::ManualVehicleView();
    Ogre::Camera* pausedRender=vehicleRenderCamera(camera);
    if(!view.ready && !cars::vehicleWorldChanging() && ou && ou->initialized && !ou->gameResetting &&
       (view.vehicle.isNull() || cars::sameHandle(cameraVehicle,view.vehicle)) &&
       cars::manualDriving() && cameraHold.valid && cameraRestore.saved && pausedRender &&
       reinterpret_cast<size_t>(pausedRender)==cameraRestore.identity && cameraRestore.epoch==observedEpoch &&
       key && camera->getCameraNode() && camera->getCenterNode() && pausedRender->getSceneManager()) {
        releaseVehiclePointer();middleHeld=(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;
        {VehicleCameraInputMask mask(key);originalCameraUpdate(camera,false);}
        applyVehicleCameraPose(camera,pausedRender,cameraHold.relativeTo(vehicleRenderOrigin(pausedRender)));return;
    }
    if(!view.ready || !key || !camera || !vehicleRenderCamera(camera) || !camera->getCameraNode() || !camera->getCenterNode()) {
        restoreVehicleCamera(camera);middleHeld=(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;
        if(!cars::manualDriving()){cameraVehicle.setNull();vehicleView=cars::VehicleCameraState();}
        originalCameraUpdate(camera,enabled);return;
    }
    if(!cars::sameHandle(cameraVehicle,view.vehicle)){restoreVehicleCamera(camera);cameraVehicle=view.vehicle;vehicleView=cars::VehicleCameraState();}
    if(vehicleView.mode==cars::VehicleCameraState::Standard) {
        restoreVehicleCamera(camera);vehicleCameraStamp=GetTickCount();
        originalCameraUpdate(camera,enabled);return;
    }
    Ogre::Camera* render=vehicleRenderCamera(camera);
    if(!cameraRestore.saved) {
        cameraRestore.identity=reinterpret_cast<size_t>(render);cameraRestore.epoch=observedEpoch;cameraRestore.saved=true;
        cameraRestore.fov=render->Ogre::Frustum::getFOVy().valueRadians();cameraRestore.nearClip=render->Ogre::Frustum::getNearClipDistance();
        cameraRestore.cameraPosition=render->getPosition();cameraRestore.cameraRotation=render->getOrientation();
        cameraRestore.nodePosition=camera->getCameraNode()->Ogre::Node::getPosition();cameraRestore.nodeRotation=camera->getCameraNode()->Ogre::Node::getOrientation();
        cameraRestore.centerRotation=camera->getCenterNode()->Ogre::Node::getOrientation();cameraRestore.freeCamera=camera->isFreeCameraMode();
        if(cameraRestore.freeCamera)camera->setFreeCameraMode(false);
    }
    const bool allowed=enabled && !faulted && readyLastFrame && key->controlEnabled && inputAllowed() && !vehicleOptionsOpen() && !cars::workshopVisible() && !ou->player->contextMenu.isVisible() && vehicleForeground();
    if(allowed)vehicleView.zoom(key->mWheel);
    // Native update reads rotation/zoom even when controlEnabled is false.
    // Mask only its camera inputs while preserving game/menu mouse dispatch.
    {VehicleCameraInputMask mask(key);originalCameraUpdate(camera,false);}
    updateVehiclePointer(allowed);
    const DWORD now=GetTickCount();
    vehicleView.smoothFollow(view.rotation,vehicleCameraStamp?(now-vehicleCameraStamp)*.001:0);vehicleCameraStamp=now;
    cars::VehicleCameraPose pose=vehicleView.pose(view.anchor,view.rotation,view.scale,view.extraSmall);
    if(!vehicleView.firstPerson())pose.eye=cars::vehicleCameraClearance(pose);
    applyVehicleCameraPose(camera,render,pose);
    cameraHold.remember(pose,vehicleRenderOrigin(render));
}
