#pragma once
#include <string>
#include <vector>
#include <map>
#include "VehicleTurretData.generated.h"
#include "VehicleService.h"
namespace cars {
struct WorkshopItem {
    const char* id; const char* name; const char* icon; const char* description;
    float mass; int hp; int width; int height;
};
struct WorkshopSlot {
    const char* id; const char* name; int system; int region;
    const char* component; const char* itemIds; bool required;
    const char* exclusiveSlots;
};
inline std::string wheelForPlate(const std::string& slot) {
    const char* ends[]={"FL","FR","RL","RR"};const char* wheels[]={"wheel_fl","wheel_fr","wheel_rl","wheel_rr"};
    for(unsigned n=0;n<4;++n)if(slot.find(std::string("Armor_wheel_")+ends[n]+"_")==0)return wheels[n];
    return "";
}
inline void removeMountAndDependents(std::map<std::string,std::string>& state,const std::string& slot) {
    state.erase(slot);
    for(std::map<std::string,std::string>::iterator i=state.begin();i!=state.end();)
        if(wheelForPlate(i->first)==slot)state.erase(i++);else ++i;
}
inline bool workshopMountSupported(bool extraSmall, const std::string& id, bool motorcycle=false) {
    if(motorcycle && (id=="wheel_fr" || id=="wheel_rr"))return false;
    return !extraSmall || id == "engine" || id == "tank" || id == "seat_driver" ||
        id == "steering" || id == "wheel_fl" || id == "wheel_fr" ||
        id == "wheel_rl" || id == "wheel_rr";
}
struct WorkshopSnapshot {
    std::string vehicleName, actorName, actorId, status;
    std::map<std::string,int> inventory;
    std::map<std::string,std::string> installed;
    std::map<std::string,double> condition;
    std::map<std::string,std::string> queued;
    std::vector<std::string> queueSlots,queueLabels;
    bool queueRunning;
    double fuel, tankCapacity;
    double mechanics,handling,workProgress;
    bool inventoryReadable, fittingAllowed, working, extraSmall, motorcycle;
    WorkshopSnapshot() : queueRunning(false), fuel(0), tankCapacity(0), mechanics(1), handling(1), workProgress(0), inventoryReadable(false), fittingAllowed(false), working(false), extraSmall(false), motorcycle(false) {}
};
struct WorkshopRequest {std::string target,actor,slot,item;bool remove;WorkshopRequest():remove(false) {}};
struct WorkshopVisual {const char* component;const char* mesh;float x,y,z;const char* partId;};
inline double workshopMaxHp(const std::string& item) {
    if(item==turretItemId)return 400;
    if(item=="143-Project Cars.mod")return 100;
    if(item=="141-Project Cars.mod")return 175;
    if(item=="142-Project Cars.mod")return 275;
    if(item=="154-Project Cars.mod")return 650;
    if(item=="21-Project Cars.mod" || item==dieselEngineItemId || item=="156-Project Cars.mod")return 400;
    return 250;
}
inline std::string workshopComponent(const WorkshopSlot& slot,const std::string& item) {
    if(item==turretItemId)return "Turret_Rear";
    if(item=="134-Project Cars.mod")return "Front_Spikes";
    if(item=="155-Project Cars.mod")return std::string(slot.id)=="cargo"?"Prisoner_Pole_Rear":"Prisoner_Pole_Front";
    if(item=="156-Project Cars.mod")return "Prisoner_Cage_Rear";
    return slot.component;
}
bool workshopAccepts(const WorkshopSlot& slot,const std::string& itemId);
std::string workshopConflict(const WorkshopSlot& slot,const std::map<std::string,std::string>& installed);
std::vector<size_t> workshopSlots(const WorkshopSlot* slots,size_t count,int system,int region);
int workshopCount(const WorkshopSnapshot& snapshot,const std::string& itemId);
// Never invent installed contents or count inventory items as installed parts.
unsigned workshopInstalled(const WorkshopSnapshot& snapshot,const WorkshopSlot* slots,size_t count,int system);
struct WorkshopLayout {
    int x,y,width,height; double scale;
    WorkshopLayout(int viewportWidth,int viewportHeight);
    int px(int logical) const;
};
}
