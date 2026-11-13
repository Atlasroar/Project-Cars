#pragma once
#include "WorkshopModel.h"
namespace cars {
enum DebugAction { DebugNone, DebugClose, DebugSpawnItem, DebugSpawnVehicle,
    DebugRefill, DebugRepair, DebugBrake, DebugStopEngines, DebugChest };
enum DebugPreset { DebugWheeledBuggy, DebugTrackedBuggy, DebugBareFrame, DebugXSmallQuad, DebugMotorcycle, DebugPresetCount };
enum DebugScope { DebugSelected, DebugNearby };
struct DebugRequest {
    DebugAction action; std::string item; unsigned quantity; int preset,scope;
    DebugRequest():action(DebugNone),quantity(1),preset(0),scope(DebugNearby){}
};
inline const char* debugPresetName(int preset) {
    if(preset==DebugMotorcycle)return "Motorcycle";
    if(preset==DebugXSmallQuad)return "Extra-Small Quad";
    return preset==DebugWheeledBuggy?"Normal buggy":preset==DebugTrackedBuggy?"Tracked buggy":preset==DebugBareFrame?"Bare light frame":"Unknown preset";
}
inline const char* debugPresetBuildingId(int preset) {
    if(preset==DebugMotorcycle)return "210-Project Cars.mod";
    return preset==DebugXSmallQuad?"200-Project Cars.mod":"10-Project Cars.mod";
}
inline std::string debugPresetItem(int preset,const WorkshopSlot& slot) {
    if(preset<0 || preset>=DebugPresetCount || preset==DebugBareFrame)return "";
    const std::string id=slot.id;
    if(!workshopMountSupported(preset==DebugXSmallQuad || preset==DebugMotorcycle,id,preset==DebugMotorcycle))return "";
    if(id=="seat_passenger_l" || id=="seat_passenger_r")return ""; // Rear bay carries cargo.
    if(id.find("wheel_")==0)return preset!=DebugTrackedBuggy?"138-Project Cars.mod":"";
    if(id.find("track_")==0)return preset==DebugTrackedBuggy?"154-Project Cars.mod":"";
    if(id.find("Armor_wheel_")==0 && preset==DebugTrackedBuggy)return "";
    if(id.find("Armor_")==0)return "143-Project Cars.mod";
    // The first accepted design is the ordinary module, before optional variants.
    const std::string designs=slot.itemIds;return designs.substr(0,designs.find('|'));
}
inline unsigned debugQuantity(unsigned count) {return count>=1 && count<=25?count:0;}
}
