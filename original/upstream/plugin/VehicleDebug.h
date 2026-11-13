#pragma once
#include "../src/VehicleDebugModel.h"
#include <utility>
namespace cars {
typedef std::vector<std::pair<std::string,std::string> > DebugItemChoices;
void showVehicleDebug(const DebugItemChoices&);
void closeVehicleDebug();
void updateVehicleDebug();
bool vehicleDebugVisible();
void vehicleDebugStatus(const std::string&);
bool takeVehicleDebugRequest(DebugRequest&);
void requestVehicleDebugClose();
}
