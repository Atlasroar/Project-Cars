#pragma once
#include "../src/WorkshopModel.h"
namespace cars {
// Called only on the game/UI thread; callbacks queue owned command strings.
void showWorkshop(const std::string& target,const WorkshopSnapshot& snapshot);
void updateWorkshop(const WorkshopSnapshot& snapshot);
void closeWorkshop();
bool workshopVisible();
std::string workshopTarget();
bool takeWorkshopRequest(WorkshopRequest& request);
void drivingStatus(const std::string& caption);
}
