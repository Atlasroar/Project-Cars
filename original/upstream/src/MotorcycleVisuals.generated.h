#pragma once
#include "WorkshopModel.h"
#include "VehiclePicking.generated.h"
namespace cars {
static const WorkshopVisual motorcycleVisuals[] = {
    { "Frame", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Frame.mesh", 0.00000f, 0.00000f, 0.00000f, "211-Project Cars.mod" },
    { "Engine", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Engine.mesh", 0.00000f, 3.50000f, 0.00000f, "212-Project Cars.mod" },
    { "Fuel_Tank", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Fuel_Tank.mesh", 1.00000f, 7.00000f, 0.00000f, "213-Project Cars.mod" },
    { "Seat_Driver", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Seat_Driver.mesh", -4.00000f, 7.50000f, 0.00000f, "214-Project Cars.mod" },
    { "Steering_Wheel", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Fork.mesh", 6.40000f, 6.92000f, 0.00000f, "215-Project Cars.mod" },
    { "Wheel_FL", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Wheel_FL.mesh", 10.30000f, 0.52000f, 0.00000f, "216-Project Cars.mod" },
    { "Wheel_RL", "./mods/Project Cars/meshes/xsmall_frame/XS_Bike_Wheel_RL.mesh", -10.80000f, 0.52000f, 0.00000f, "217-Project Cars.mod" },
};
static const PickPlane motorcyclePickPlanes[][6] = {
    { {-1,0,0,-11.60000f}, {1,0,0,-8.10388f}, {0,-1,0,0.00000f}, {0,1,0,-8.61873f}, {0,0,-1,-3.26000f}, {0,0,1,-3.26000f} },
    { {-1,0,0,-3.55189f}, {1,0,0,-3.55189f}, {0,-1,0,2.30763f}, {0,1,0,-4.69237f}, {0,0,-1,-1.71435f}, {0,0,1,-1.71435f} },
    { {-1,0,0,-0.19239f}, {1,0,0,-2.49336f}, {0,-1,0,6.97013f}, {0,1,0,-9.18260f}, {0,0,-1,-2.47897f}, {0,0,1,-2.87643f} },
    { {-1,0,0,-6.49925f}, {1,0,0,1.50075f}, {0,-1,0,3.88405f}, {0,1,0,-11.11595f}, {0,0,-1,-2.01378f}, {0,0,1,-2.01378f} },
    { {-1,0,0,5.13330f}, {1,0,0,-10.94000f}, {0,-1,0,-0.12000f}, {0,1,0,-8.98980f}, {0,0,-1,-3.49632f}, {0,0,1,-3.49632f} },
    { {-1,0,0,6.89854f}, {1,0,0,-13.70146f}, {0,-1,0,-2.88146f}, {0,1,0,-3.92146f}, {0,0,-1,-1.21958f}, {0,0,1,-1.21958f} },
    { {-1,0,0,-14.20146f}, {1,0,0,7.39854f}, {0,-1,0,-2.88146f}, {0,1,0,-3.92146f}, {0,0,-1,-1.21958f}, {0,0,1,-1.21958f} },
};
static const size_t motorcycleVisualCount = sizeof(motorcycleVisuals) / sizeof(motorcycleVisuals[0]);
}
