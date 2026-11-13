#pragma once
#include "../src/WorkshopModel.h"
#include "../src/VehicleInteraction.h"
#include "../src/VehicleControls.h"
#include "../src/VehicleCamera.h"
#include "../src/VehicleDebugModel.h"
#include <kenshi/util/hand.h>
#include <ogre/OgreRay.h>
class Building;
class Character;
namespace cars {
std::string debugFitVehicle(Building*,Character*,int preset);
std::string debugServiceVehicles(Character*,DebugAction,int scope);
bool installFittingRuntime();
bool vehicleRecoveryActive();
void cancelVehicleRecovery();
void clearVehicleRouteRequests();
void updateVehicleRecovery(const Ogre::Ray&,bool confirm,int turn);
std::string vehicleRecoveryStatus();
void fittingSnapshot(Building*,Character*,WorkshopSnapshot&);
std::string fitComponent(Building*,Character*,const WorkshopRequest&);
void updateFittedVisuals(Building*);
void updateFittingWork();
void cancelAllFittingWork();
void pauseVehicleStreaming();
bool pickVehiclePart(const Ogre::Ray&,Building* preferred,hand& target,VehicleClick& action);
bool interactWithVehicle(Building*,Character*,VehicleClick);
struct VehicleActions {
    bool gunner;bool passenger,people,placeDriver,placeLeft,placeRight,placeFrontPole,placeRearPole,placeCage;bool seat,steering,cargo,occupied,seated,boarding,ignition,overturned,driven,manual;VehicleActions():gunner(false),passenger(false),people(false),placeDriver(false),placeLeft(false),placeRight(false),placeFrontPole(false),placeRearPole(false),placeCage(false),seat(false),steering(false),cargo(false),occupied(false),seated(false),boarding(false),ignition(false),overturned(false),driven(false),manual(false){}};
VehicleActions vehicleActions(Building*,Character*);
hand drivenVehicle(Character*);
bool selectedVehicleAction(VehicleClick,bool execute);
bool manualDriving();
void endManualDriving();
void updateManualDriving(const ManualDrivingKeys&);
struct ManualVehicleView {
    hand vehicle;RigPoint anchor;BodyRotation rotation;double scale;bool ready,extraSmall;
    ManualVehicleView():scale(1),ready(false),extraSmall(false){}
};
ManualVehicleView manualVehicleView();
RigPoint vehicleCameraClearance(const VehicleCameraPose&);
std::string vehicleInteractionStatus(Building*);
}
