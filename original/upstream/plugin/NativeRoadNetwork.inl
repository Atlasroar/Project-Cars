// Included in the fitting runtime's namespace. Read only, on the simulation
// thread; copy road geometry into plugin-owned memory before routing over it.
// A tiny POD/SEH boundary isolates reconstructed native reads. Never call game
// mutations, allocate native containers, or retry partially-applied actions here.
bool copyNativeRoad(unsigned index,cars::NativeRoadCopy& out) {
    __try {
        unsigned char* entry=reinterpret_cast<unsigned char*>(KenshiLib::GetRealAddress(&AbstractMovementBase::setRoadDestination));
        HMODULE lib=GetModuleHandleW(L"KenshiLib.dll");
        void* exported=reinterpret_cast<void*>(GetProcAddress(lib,"?setupRoads@MapScreen@@QEAAXXZ"));
        if(!entry||!exported)return false;
        unsigned char* map=reinterpret_cast<unsigned char*>(KenshiLib::GetRealAddress(exported));
        // Both independently mapped native consumers must agree on the manager.
        if(!map||memcmp(entry+0xd2,"\x48\x8b\x0d",3)||memcmp(map+0x58,"\x4c\x8b\x25",3))return false;
        unsigned char** slot=reinterpret_cast<unsigned char**>(entry+0xd9+*reinterpret_cast<int*>(entry+0xd5));
        unsigned char** other=reinterpret_cast<unsigned char**>(map+0x5f+*reinterpret_cast<int*>(map+0x5b));
        if(slot!=other)return false;
        return cars::copyRoadRecord(*slot,index,out);
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
class NativeRoadNetwork {
    unsigned cursor,count,restarts;unsigned __int64 owner,list;bool requested,done,failed;
public:
    cars::RoadNetwork graph;
    NativeRoadNetwork():cursor(0),count(0),restarts(0),owner(0),list(0),requested(false),done(false),failed(false){}
    void reset(){graph.clear();cursor=count=restarts=0;owner=list=0;requested=done=failed=false;}
    void request(){requested=true;}
    bool ready()const{return done;}
    void advance() {
        if(!requested||done)return;
        unsigned copied=0;cars::NativeRoadCopy raw;
        for(unsigned n=0;n<64 && copied<4096;++n) {
            if(!copyNativeRoad(cursor,raw)){failed=true;break;}
            if(cursor && (raw.manager!=owner||raw.list!=list||raw.total!=count)) {
                graph.clear();cursor=0;if(++restarts>2){failed=true;break;}continue;
            }
            owner=raw.manager;list=raw.list;count=raw.total;
            if(cursor>=count){done=true;break;}
            if(raw.count){std::vector<cars::RigPoint> points;points.reserve(raw.count);
                for(unsigned k=0;k<raw.count;++k)points.push_back(cars::RigPoint(raw.points[k][0],raw.points[k][1],raw.points[k][2]));
                if(!graph.add(raw.ends[0],raw.ends[1],points)){failed=true;break;}
                copied+=raw.count;
            }
            ++cursor;
        }
        if(failed){graph.clear();done=true;ErrorLog("Project Cars: road network unavailable; using terrain routes.");}
        else if(done){std::ostringstream log;log<<"Project Cars: road network snapshot / roads="<<graph.roads.size()<<" / points="<<graph.pointCount;DebugLog(log.str());}
    }
} nativeRoadNetwork;
