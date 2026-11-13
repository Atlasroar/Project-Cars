struct DentStream {Ogre::HardwareVertexBufferSharedPtr buffer;std::vector<unsigned char> bind;size_t stride,offset,start,count;};
struct DentMesh {Ogre::MeshPtr mesh;std::vector<DentStream> streams;unsigned revision;DentMesh():revision(~0u){}};
std::map<hand,DentMesh> dentMeshes;
void forgetDentedMesh(Building* b) {
    std::map<hand,DentMesh>::iterator i=dentMeshes.find(b->getHandle());if(i==dentMeshes.end())return;
    std::string name=i->second.mesh.isNull()?"":i->second.mesh->getName();dentMeshes.erase(i);
    if(!name.empty())Ogre::MeshManager::getSingleton().remove(name);
}
std::string prepareDentedMesh(Building* b,const std::string& source) {
    std::map<hand,DentMesh>::iterator existing=dentMeshes.find(b->getHandle());if(existing!=dentMeshes.end())return existing->second.mesh->getName();
    Ogre::MeshPtr original=Ogre::MeshManager::getSingleton().load(source,"ProjectCarsVehicleParts");
    static unsigned serial=0;std::ostringstream name;name<<"ProjectCars_DentedFrame_"<<++serial;
    DentMesh& d=dentMeshes[b->getHandle()];
    try {
        d.mesh=original->clone(name.str(),"ProjectCarsVehicleParts");std::set<Ogre::VertexData*> seen;
        size_t total=0;
        for(unsigned n=0;n<d.mesh->getNumSubMeshes();++n){Ogre::SubMesh* sub=d.mesh->getSubMesh(n);Ogre::VertexData* v=sub->useSharedVertices?d.mesh->sharedVertexData:sub->vertexData;if(!v||!seen.insert(v).second)continue;
            const Ogre::VertexElement* element=v->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);if(!element || element->getType()!=Ogre::VET_FLOAT3)throw std::runtime_error("Unsupported frame vertex layout");
            Ogre::HardwareVertexBufferSharedPtr originalBuffer=v->vertexBufferBinding->getBuffer(element->getSource());DentStream stream;
            stream.stride=originalBuffer->getVertexSize();stream.offset=element->getOffset();stream.start=v->vertexStart;stream.count=v->vertexCount;
            size_t bytes=originalBuffer->getSizeInBytes();total+=bytes;if(!bytes || total>32*1024*1024 || stream.offset+12>stream.stride || stream.start+stream.count>originalBuffer->getNumVertices())throw std::runtime_error("Invalid frame vertex bounds");
            stream.bind.resize(bytes);originalBuffer->readData(0,bytes,&stream.bind[0]);
            stream.buffer=originalBuffer->getManager()->createVertexBuffer(stream.stride,originalBuffer->getNumVertices(),Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY_DISCARDABLE,false);
            stream.buffer->writeData(0,bytes,&stream.bind[0],true);v->vertexBufferBinding->setBinding(element->getSource(),stream.buffer);d.streams.push_back(stream);
        }
        return name.str();
    }catch(...){forgetDentedMesh(b);throw;}
}
void updateFrameDents(Building* b) {
    std::map<hand,DentMesh>::iterator i=dentMeshes.find(b->getHandle());if(i==dentMeshes.end())return;
    DriveState& drive=driveState(b);drive.dents.repair(drive.body.damage);DentMesh& d=i->second;if(d.revision==drive.dents.serial)return;
    for(unsigned n=0;n<d.streams.size();++n){DentStream& stream=d.streams[n];std::vector<unsigned char> bytes=stream.bind;
        for(size_t v=stream.start;v<stream.start+stream.count;++v){float* p=reinterpret_cast<float*>(&bytes[v*stream.stride+stream.offset]);cars::RigPoint displaced=drive.dents.deform(cars::RigPoint(p[0],p[1],p[2]));p[0]=float(displaced.x);p[1]=float(displaced.y);p[2]=float(displaced.z);}
        stream.buffer->writeData(0,bytes.size(),&bytes[0],true);
    }
    d.revision=drive.dents.serial;
}
