#include "WorkshopModel.h"
#include <algorithm>
namespace cars {
std::string workshopConflict(const WorkshopSlot& slot,const std::map<std::string,std::string>& installed) {
    const std::string exclusive=std::string("|")+(slot.exclusiveSlots?slot.exclusiveSlots:"")+"|";
    for(std::map<std::string,std::string>::const_iterator i=installed.begin();i!=installed.end();++i)
        if(!i->second.empty() && exclusive.find("|"+i->first+"|")!=std::string::npos)return i->first;
    return "";
}
bool workshopAccepts(const WorkshopSlot& slot,const std::string& id) {
    if(id.empty()) return false;
    const std::string allowed=std::string("|")+slot.itemIds+"|";
    return allowed.find(std::string("|")+id+"|")!=std::string::npos;
}
std::vector<size_t> workshopSlots(const WorkshopSlot* slots,size_t count,int system,int region) {
    std::vector<size_t> result;
    for(size_t i=0;i<count;++i) if(slots[i].system==system && (system!=2 || slots[i].region==region)) result.push_back(i);
    return result;
}
int workshopCount(const WorkshopSnapshot& snapshot,const std::string& id) {
    std::map<std::string,int>::const_iterator i=snapshot.inventory.find(id);
    return !snapshot.inventoryReadable || i==snapshot.inventory.end() ? -1 : i->second;
}
unsigned workshopInstalled(const WorkshopSnapshot& snapshot,const WorkshopSlot* slots,size_t count,int system) {
    unsigned n=0;
    for(size_t i=0;i<count;++i) {
        std::map<std::string,std::string>::const_iterator found=snapshot.installed.find(slots[i].id);
        if(slots[i].system==system && found!=snapshot.installed.end() && workshopAccepts(slots[i],found->second)) ++n;
    }
    return n;
}
WorkshopLayout::WorkshopLayout(int w,int h) {
    scale=std::min(1.15,std::min(std::max(1,w-24)/1040.0,std::max(1,h-24)/660.0));
    width=px(1040); height=px(660); x=(w-width)/2; y=(h-height)/2;
}
int WorkshopLayout::px(int logical) const { return static_cast<int>(logical*scale+0.5); }
}
