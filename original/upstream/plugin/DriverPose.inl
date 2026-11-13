struct GripTargets {
    cars::RigPoint pelvis,left,right;Ogre::Quaternion rotation;Ogre::Animation* seated;bool arms,standing;
    GripTargets():rotation(Ogre::Quaternion::IDENTITY),seated(0),arms(true),standing(false){}
};
struct PoseProgress {
    bool ready,applied,failed,reportedApplied,reportedFailed;
    PoseProgress():ready(false),applied(false),failed(false),reportedApplied(false),reportedFailed(false){}
};
std::map<Ogre::Skeleton*,GripTargets> gripTargets;
std::map<Ogre::Skeleton*,PoseProgress> poseProgress;
CRITICAL_SECTION poseMutex;
void (*originalBoneMatrices)(Ogre::Skeleton*,Ogre::Matrix4*)=0;
void (*originalSkeletonDestroy)(Ogre::OldSkeletonInstance*)=0;
void skeletonDestroy(Ogre::OldSkeletonInstance* skeleton) {
    EnterCriticalSection(&poseMutex);gripTargets.erase(skeleton);poseProgress.erase(skeleton);LeaveCriticalSection(&poseMutex);originalSkeletonDestroy(skeleton);
}
void clearDriverGrips(){if(!driverPoseReady)return;EnterCriticalSection(&poseMutex);gripTargets.clear();poseProgress.clear();LeaveCriticalSection(&poseMutex);}
void orientSegment(Ogre::OldBone* bone,const Ogre::Vector3& current,const Ogre::Vector3& wanted) {
    if(current.squaredLength()<.00001f||wanted.squaredLength()<.00001f)return;
    Ogre::Quaternion correction=current.getRotationTo(wanted);
    Ogre::Quaternion parent=bone->Ogre::OldNode::getParent()?bone->Ogre::OldNode::getParent()->Ogre::OldNode::_getDerivedOrientation():Ogre::Quaternion::IDENTITY;
    bone->Ogre::OldNode::setOrientation(parent.Inverse()*correction*bone->Ogre::OldNode::_getDerivedOrientation());bone->Ogre::OldNode::_update(true,false);
}
bool gripArm(Ogre::Skeleton* skeleton,const char* side,const cars::RigPoint& target) {
    std::string prefix=std::string("Bip01 ")+side+" ";
    // Qualify native exports: the SDK's overloaded Skeleton virtual layout is
    // older than Kenshi's ID-based animation table.
    if(!skeleton->Ogre::Skeleton::hasBone(prefix+"UpperArm")||!skeleton->Ogre::Skeleton::hasBone(prefix+"Forearm")||!skeleton->Ogre::Skeleton::hasBone(prefix+"Hand"))return false;
    Ogre::OldBone* upper=skeleton->Ogre::Skeleton::getBone(prefix+"UpperArm");
    Ogre::OldBone* lower=skeleton->Ogre::Skeleton::getBone(prefix+"Forearm");
    Ogre::OldBone* handBone=skeleton->Ogre::Skeleton::getBone(prefix+"Hand");
    upper->Ogre::OldNode::_update(true,false);
    cars::RigPoint a=point(upper->Ogre::OldNode::_getDerivedPosition()),b=point(lower->Ogre::OldNode::_getDerivedPosition()),c=point(handBone->Ogre::OldNode::_getDerivedPosition()),elbow,hand;
    if(!cars::solveArm(a,b,c,target,elbow,hand))return false;
    orientSegment(upper,vector(b-a),vector(elbow-a));
    orientSegment(lower,handBone->Ogre::OldNode::_getDerivedPosition()-lower->Ogre::OldNode::_getDerivedPosition(),vector(hand)-lower->Ogre::OldNode::_getDerivedPosition());
    return (point(handBone->Ogre::OldNode::_getDerivedPosition())-target).length()<.05;
}
void leanDriverTorso(Ogre::Skeleton* skeleton,const GripTargets& target) {
    const char* names[]={"Bip01 Spine","Bip01 L UpperArm","Bip01 R UpperArm","Bip01 L Forearm","Bip01 R Forearm","Bip01 L Hand","Bip01 R Hand"};
    Ogre::OldBone* bones[7];cars::RigPoint p[7];
    for(unsigned n=0;n<7;++n) {
        if(!skeleton->Ogre::Skeleton::hasBone(names[n]))return;
        bones[n]=skeleton->Ogre::Skeleton::getBone(names[n]);p[n]=point(bones[n]->Ogre::OldNode::_getDerivedPosition());
    }
    cars::RigPoint axis;double angle=cars::driverTorsoLean(p[0],p[1],p[2],(p[3]-p[1]).length()+(p[5]-p[3]).length(),(p[4]-p[2]).length()+(p[6]-p[4]).length(),target.left,target.right,axis);
    if(angle>0) {
        cars::RigPoint up=(p[1]+p[2])*.5-p[0];
        orientSegment(bones[0],vector(up),vector(cars::rotateRigPoint(up,axis,angle)));
    }
}
void driverBoneMatrices(Ogre::Skeleton* skeleton,Ogre::Matrix4* matrices) {
    // Entity::cacheBoneMatrices inlines the old setAnimationState wrapper.
    // Apply after native animation, immediately before the final skin palette.
    // Only copied render targets/resources are used here, never game handles.
    GripTargets target;bool found=false;EnterCriticalSection(&poseMutex);
    std::map<Ogre::Skeleton*,GripTargets>::const_iterator i=gripTargets.find(skeleton);
    if(i!=gripTargets.end()){target=i->second;found=true;}LeaveCriticalSection(&poseMutex);
    if(found) {
        bool applied=false;
        try {
            if((target.seated || target.standing) && skeleton->Ogre::Skeleton::hasBone("Bip01") && skeleton->Ogre::Skeleton::hasBone("Bip01 Pelvis")) {
                skeleton->Ogre::Skeleton::reset(false);
                if(target.seated && !target.standing)target.seated->apply(skeleton,0,1,1);
                skeleton->Ogre::Skeleton::_updateTransforms();
                Ogre::OldBone* root=skeleton->Ogre::Skeleton::getBone("Bip01");
                Ogre::OldBone* pelvis=skeleton->Ogre::Skeleton::getBone("Bip01 Pelvis");
                // Native character movement retains only horizontal facing.
                // Tilt the freshly reset pose before pinning the hips and solving
                // the grips. Preserve the clip's authored root orientation.
                Ogre::Quaternion parent=root->Ogre::OldNode::getParent()?root->Ogre::OldNode::getParent()->Ogre::OldNode::_getDerivedOrientation():Ogre::Quaternion::IDENTITY;
                root->Ogre::OldNode::setOrientation(parent.Inverse()*target.rotation*root->Ogre::OldNode::_getDerivedOrientation());
                root->Ogre::OldNode::_update(true,false);
                Ogre::Vector3 offset=vector(target.pelvis)-pelvis->Ogre::OldNode::_getDerivedPosition();
                if(root->Ogre::OldNode::getParent())offset=root->Ogre::OldNode::getParent()->Ogre::OldNode::_getDerivedOrientation().Inverse()*offset/root->Ogre::OldNode::getParent()->Ogre::OldNode::_getDerivedScale();
                root->Ogre::OldNode::setPosition(root->Ogre::OldNode::getPosition()+offset);root->Ogre::OldNode::_update(true,false);
                if(target.arms){if(!target.standing)leanDriverTorso(skeleton,target);
                bool left=gripArm(skeleton,"L",target.left),right=gripArm(skeleton,"R",target.right);applied=left&&right;}
                else applied=true;
            }
        }catch(...){/* Record once on the game thread; native rendering still runs. */}
        EnterCriticalSection(&poseMutex);
        std::map<Ogre::Skeleton*,PoseProgress>::iterator status=poseProgress.find(skeleton);
        if(status!=poseProgress.end()){status->second.applied|=applied;status->second.failed|=!applied;}
        LeaveCriticalSection(&poseMutex);
    }
    originalBoneMatrices(skeleton,matrices);
}
void prepareDriverGrips() {
    std::map<Ogre::Skeleton*,GripTargets> next;
    for(std::map<hand,DriveState>::const_iterator i=drives.begin();i!=drives.end();++i) {
        Building* b=i->first.getBuilding();Character* actor=i->second.occupant.getCharacter();
        if(!b||!b->isValid()||!b->getRootNode()||!actor||!actor->isValid()||actor->isDead()||actor->getMedical()->isUnconcious())continue;
        AnimationClass* anim=actor->getAnimationClass();if(!anim||!anim->skeleton||!anim->node)continue;
        Ogre::Vector3 scale=anim->node->_getDerivedScale();if(scale.x<=0||scale.y<=0||scale.z<=0)continue;
        GripTargets targets;bool cached=false;
        EnterCriticalSection(&poseMutex);
        std::map<Ogre::Skeleton*,GripTargets>::const_iterator old=gripTargets.find(anim->skeleton);
        if(old!=gripTargets.end()){targets.seated=old->second.seated;cached=true;}
        LeaveCriticalSection(&poseMutex);
        if(!cached) {
            try{targets.seated=anim->getAnimation("sitting chair");}catch(...){targets.seated=0;}
            if(!targets.seated)ErrorLog("Project Cars: native sitting chair clip unavailable / "+actor->getHandle().toString());
        }
        // Convert with the actual rendered character node, including race/body
        // scale and world origin shifts, instead of assuming the actor's yaw.
        Ogre::Vector3 shift=b->getRootNode()->_getDerivedPosition()-b->getPosition();
        targets.rotation=anim->node->_getDerivedOrientation().Inverse()*b->rot*Ogre::Quaternion(Ogre::Degree(90),Ogre::Vector3::UNIT_Y);
        targets.pelvis=point(anim->node->convertWorldToLocalPosition(rigWorld(b,driverAnchor(b,true))+shift));
        targets.left=point(anim->node->convertWorldToLocalPosition(rigWorld(b,driverGrip(b,true,i->second.body.steering))+shift));
        targets.right=point(anim->node->convertWorldToLocalPosition(rigWorld(b,driverGrip(b,false,i->second.body.steering))+shift));
        next[anim->skeleton]=targets;
    }
    for(std::map<hand,std::map<std::string,MountedPerson> >::const_iterator v=passengers.begin();v!=passengers.end();++v) {
        Building* b=v->first.getBuilding();if(!b||!b->isValid()||!b->getRootNode())continue;
        const Ogre::Vector3 shift=b->getRootNode()->_getDerivedPosition()-b->getPosition();
        for(std::map<std::string,MountedPerson>::const_iterator p=v->second.begin();p!=v->second.end();++p) {
            Character* who=p->second.person.getCharacter();AnimationClass* anim=who&&who->isValid()?who->getAnimationClass():0;
            if(!anim||!anim->skeleton||!anim->node)continue;
            if(p->first=="cargo" && fittedTurret(b))continue; // Native turret aiming/reload animation.
            GripTargets targets;targets.standing=poleMount(b,p->first);targets.arms=targets.standing;
            const float facing=(p->first=="seat_driver"||p->first=="front_attachment")?90.0f:270.0f;
            // Some mounted render nodes already carry the full vehicle tilt;
            // their relative correction is identity, avoiding a doubled slope.
            targets.rotation=anim->node->_getDerivedOrientation().Inverse()*b->rot*Ogre::Quaternion(Ogre::Degree(facing),Ogre::Vector3::UNIT_Y);
            if(!targets.standing){try{targets.seated=anim->getAnimation("sitting chair");}catch(...){continue;}}
            cars::RigPoint anchor=personAnchor(b,p->first,true);if(targets.standing)anchor.y+=9.5;
            targets.pelvis=point(anim->node->convertWorldToLocalPosition(rigWorld(b,anchor)+shift));
            if(targets.standing) {
                const bool front=p->first=="front_attachment";double x=front?23.5:-24.5,y=(front?6.6:10.9)+13.3;
                targets.left=point(anim->node->convertWorldToLocalPosition(rigWorld(b,cars::RigPoint(x,y,front?-5:5))+shift));
                targets.right=point(anim->node->convertWorldToLocalPosition(rigWorld(b,cars::RigPoint(x,y,front?5:-5))+shift));
            }
            next[anim->skeleton]=targets;
        }
    }
    std::vector<std::string> logs;
    EnterCriticalSection(&poseMutex);gripTargets.swap(next);
    for(std::map<Ogre::Skeleton*,GripTargets>::const_iterator i=gripTargets.begin();i!=gripTargets.end();++i) {
        PoseProgress& status=poseProgress[i->first];
        if(!status.ready){status.ready=true;logs.push_back("Project Cars: driver render pose prepared");}
        if(status.applied && !status.reportedApplied){status.reportedApplied=true;logs.push_back("Project Cars: seated driver and both wheel grips applied to skin palette");}
        if(status.failed && !status.reportedFailed){status.reportedFailed=true;logs.push_back("Project Cars: driver render pose could not apply seated clip or arm bones");}
    }
    for(std::map<Ogre::Skeleton*,PoseProgress>::iterator i=poseProgress.begin();i!=poseProgress.end();) {
        if(!gripTargets.count(i->first))poseProgress.erase(i++);else ++i;
    }
    LeaveCriticalSection(&poseMutex);
    for(unsigned n=0;n<logs.size();++n)DebugLog(logs[n]);
}
bool installDriverPose() {
    InitializeCriticalSection(&poseMutex);
    HMODULE ogre=GetModuleHandleW(L"OgreMain_x64.dll");
    void* target=ogre?reinterpret_cast<void*>(GetProcAddress(ogre,"?_getBoneMatrices@Skeleton@Ogre@@UEAAXPEAVMatrix4@2@@Z")):0;
    void* destroy=ogre?reinterpret_cast<void*>(GetProcAddress(ogre,"??1OldSkeletonInstance@Ogre@@UEAA@XZ")):0;
    return target && destroy && KenshiLib::AddHook(destroy,skeletonDestroy,&originalSkeletonDestroy)==KenshiLib::SUCCESS && KenshiLib::AddHook(target,driverBoneMatrices,&originalBoneMatrices)==KenshiLib::SUCCESS;
}
