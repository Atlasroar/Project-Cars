// No cached entity/skeleton/bone pointers. Each module owns its skeleton instance.
Ogre::OldSkeletonInstance* (*trackSkeleton)(const Ogre::Entity*)=0;
void (*setTrackSkinning)(Ogre::GpuProgram*,bool)=0;
const char* trackGroup="ProjectCarsTrackAnimation";
bool trackProgramReady=false;
void prepareTrackProgram() {
    if(trackProgramReady)return;
    if(!trackSkeleton)trackSkeleton=reinterpret_cast<Ogre::OldSkeletonInstance*(*)(const Ogre::Entity*)>(GetProcAddress(GetModuleHandleW(L"OgreMain_x64.dll"),"?getSkeleton@Entity@Ogre@@QEBAPEAVOldSkeletonInstance@2@XZ"));
    if(!trackSkeleton)throw std::runtime_error("Native track skeleton accessor unavailable");
    if(!setTrackSkinning)setTrackSkinning=reinterpret_cast<void(*)(Ogre::GpuProgram*,bool)>(GetProcAddress(GetModuleHandleW(L"OgreMain_x64.dll"),"?setSkeletalAnimationIncluded@GpuProgram@Ogre@@UEAAX_N@Z"));
    if(!setTrackSkinning)throw std::runtime_error("Native skeletal shader flag unavailable");
    Ogre::ResourceGroupManager& resources=Ogre::ResourceGroupManager::getSingleton();
    if(!resources.resourceGroupExists(trackGroup))resources.createResourceGroup(trackGroup);
    const std::string directory="./mods/Project Cars/shaders";
    if(!resources.resourceLocationExists(directory,trackGroup))resources.addResourceLocation(directory,"FileSystem",trackGroup);
    Ogre::HighLevelGpuProgramManager& manager=Ogre::HighLevelGpuProgramManager::getSingleton();
    Ogre::HighLevelGpuProgramPtr program=manager.getByName("ProjectCars_Track_VP",trackGroup);
    if(program.isNull()) {
        program=manager.createProgram("ProjectCars_Track_VP",trackGroup,"hlsl",Ogre::GPT_VERTEX_PROGRAM);
        program->setSourceFile("vehicle_tracks.hlsl");program->setParameter("entry_point","track_vs");program->setParameter("target","vs_4_0");
        program->setParameter("column_major_matrices","false");
    }
    setTrackSkinning(program.getPointer(),true);
    program->Ogre::Resource::load();
    if(program->hasCompileError() || !program->isSupported())throw std::runtime_error("Track skinning shader is unsupported");
    trackProgramReady=true;
}
void poseTrack(Ogre::Entity* entity,double travel,const double* displacement,bool refresh=true) {
    if(!trackSkeleton)return;
    Ogre::OldSkeletonInstance* skeleton=trackSkeleton(entity);if(!skeleton || skeleton->Ogre::Skeleton::getNumBones()!=cars::trackBoneCount)return;
    cars::TrackPose poses[cars::trackBoneCount];cars::solveTrackPose(travel,displacement,poses);
    for(unsigned n=0;n<cars::trackBoneCount;++n) {
        Ogre::OldBone* bone=skeleton->Ogre::Skeleton::getBone(static_cast<unsigned short>(n));const cars::TrackPose& p=poses[n];
        bone->setManuallyControlled(true);
        bone->Ogre::OldNode::setPosition(Ogre::Vector3(float(p.position.x),float(p.position.y),float(p.position.z)));
        bone->Ogre::OldNode::setOrientation(Ogre::Quaternion(Ogre::Radian(float(p.angle)),Ogre::Vector3::UNIT_Z));
        bone->Ogre::OldNode::setScale(Ogre::Vector3(float(p.scale),1,1));
    }
    if(refresh)entity->_updateAnimation();
}
void rigTrackMaterial(Ogre::Entity* entity) {
    prepareTrackProgram();Ogre::OldSkeletonInstance* skeleton=trackSkeleton(entity);
    if(!skeleton || skeleton->Ogre::Skeleton::getNumBones()!=cars::trackBoneCount)throw std::runtime_error("Animated track skeleton does not match its mesh");
    Ogre::MaterialManager& manager=Ogre::MaterialManager::getSingleton();
    for(unsigned n=0;n<entity->getNumSubEntities();++n) {
        Ogre::SubEntity* sub=entity->getSubEntity(n);Ogre::MaterialPtr source=sub->getMaterial();
        const std::string name="ProjectCars_Track_"+source->getName();Ogre::MaterialPtr material=manager.getByName(name,trackGroup);
        if(material.isNull()) {
            material=source->clone(name,true,trackGroup);
            for(unsigned t=0;t<material->getNumTechniques();++t) {
                Ogre::Technique* technique=material->getTechnique(t);technique->setShadowCasterMaterial("SkinnedShadowCaster");
                for(unsigned p=0;p<technique->getNumPasses();++p) {
                    Ogre::Pass* pass=technique->getPass(p);Ogre::GpuProgramParametersSharedPtr prior=pass->getVertexProgramParameters();
                    pass->setVertexProgram("ProjectCars_Track_VP");Ogre::GpuProgramParametersSharedPtr params=pass->getVertexProgramParameters();
                    params->setNamedConstant("tiling",Ogre::Vector2(1,1));if(!prior.isNull())params->copyMatchingNamedConstantsFrom(*prior);
                    params->setNamedAutoConstant("worldMatrix3x4Array",Ogre::GpuProgramParameters::ACT_WORLD_MATRIX_ARRAY_3x4);
                    params->setNamedAutoConstant("viewProjectionMatrix",Ogre::GpuProgramParameters::ACT_VIEWPROJ_MATRIX);
                    params->setNamedAutoConstant("worldMatrix",Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);
                    params->setNamedAutoConstant("cameraDir",Ogre::GpuProgramParameters::ACT_VIEW_DIRECTION);
                }
            }
            material->Ogre::Resource::load();if(!material->getBestTechnique())throw std::runtime_error("Animated track material has no supported technique");
        }
        sub->setMaterial(material);
    }
    const double rest[6]={0,0,0,0,0,0};poseTrack(entity,0,rest,false);
}
void animateVehicleTracks(Ogre::SceneNode* root,const cars::ChassisBody& body,const cars::ChassisSpec& spec,double scale) {
    if(!spec.trackMask || scale<=0 || !trackSkeleton)return;
    for(unsigned child=0;child<root->numChildren();++child) {
        Ogre::SceneNode* node=static_cast<Ogre::SceneNode*>(root->getChild(child));const std::string& name=node->getName();
        int side=name==std::string(nodePrefix)+"Track_L"?0:name==std::string(nodePrefix)+"Track_R"?1:-1;
        if(side<0 || !node->numAttachedObjects())continue;
        // Track module pivot is y=5.3053 under the chassis root (COM y=10).
        // Align its outer grousers to the physical roller contact radius.
        double suspension[6];for(unsigned n=0;n<6;++n)suspension[n]=(10*scale+spec.mountHeight-spec.travel+body.trackCompression[side][n]-spec.radius)/scale-5.3053+5.331972;
        poseTrack(static_cast<Ogre::Entity*>(node->getAttachedObject(static_cast<size_t>(0))),body.trackTravel[side]/scale,suspension);
    }
}
