#pragma once
#include "WorkshopModel.h"
#include "VehiclePicking.generated.h"
namespace cars {
static const WorkshopVisual xsmallVisuals[] = {
    { "Frame", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Frame.mesh", 0.00000f, 0.00000f, 0.00000f, "201-Project Cars.mod" },
    { "Engine", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Engine.mesh", 0.00000f, 3.50000f, 0.00000f, "202-Project Cars.mod" },
    { "Fuel_Tank", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Fuel_Tank.mesh", 1.00000f, 7.00000f, 0.00000f, "203-Project Cars.mod" },
    { "Seat_Driver", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Seat_Driver.mesh", -4.00000f, 7.50000f, 0.00000f, "204-Project Cars.mod" },
    { "Steering_Wheel", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Steering_Wheel.mesh", 4.25000f, 9.50000f, 0.00000f, "205-Project Cars.mod" },
    { "Wheel_FL", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Wheel_FL.mesh", 7.30000f, 0.82000f, -4.30000f, "206-Project Cars.mod" },
    { "Wheel_FR", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Wheel_FR.mesh", 7.30000f, 0.82000f, 4.30000f, "207-Project Cars.mod" },
    { "Wheel_RL", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Wheel_RL.mesh", -7.90000f, 0.82000f, -4.30000f, "208-Project Cars.mod" },
    { "Wheel_RR", "./mods/Project Cars/meshes/xsmall_frame/XS_Quad_Wheel_RR.mesh", -7.90000f, 0.82000f, 4.30000f, "209-Project Cars.mod" },
};
static const PickPlane xsmallPickPlanes[][6] = {
    { {-1,0,0,-10.26000f}, {1,0,0,-8.20000f}, {0,-1,0,0.00000f}, {0,1,0,-8.61873f}, {0,0,-1,-4.50088f}, {0,0,1,-4.50088f} },
    { {-1,0,0,-3.55189f}, {1,0,0,-3.55189f}, {0,-1,0,2.30763f}, {0,1,0,-4.69237f}, {0,0,-1,-1.71435f}, {0,0,1,-1.71435f} },
    { {-1,0,0,-0.19239f}, {1,0,0,-2.49336f}, {0,-1,0,6.97013f}, {0,1,0,-9.18260f}, {0,0,-1,-2.47897f}, {0,0,1,-2.87643f} },
    { {-1,0,0,-6.84636f}, {1,0,0,1.15364f}, {0,-1,0,3.38183f}, {0,1,0,-11.61817f}, {0,0,-1,-2.29347f}, {0,0,1,-2.29347f} },
    { {-1,0,0,2.78532f}, {1,0,0,-6.69376f}, {0,-1,0,5.73619f}, {0,1,0,-10.46580f}, {0,0,-1,-1.70372f}, {0,0,1,-1.70372f} },
    { {-1,0,0,4.80865f}, {1,0,0,-9.79135f}, {0,-1,0,-1.67135f}, {0,1,0,-3.31135f}, {0,0,-1,-5.19327f}, {0,0,1,3.40673f} },
    { {-1,0,0,4.80865f}, {1,0,0,-9.79135f}, {0,-1,0,-1.67135f}, {0,1,0,-3.31135f}, {0,0,-1,3.40673f}, {0,0,1,-5.19327f} },
    { {-1,0,0,-10.39136f}, {1,0,0,5.40864f}, {0,-1,0,-1.67135f}, {0,1,0,-3.31136f}, {0,0,-1,-5.19327f}, {0,0,1,3.40673f} },
    { {-1,0,0,-10.39136f}, {1,0,0,5.40864f}, {0,-1,0,-1.67135f}, {0,1,0,-3.31135f}, {0,0,-1,3.40673f}, {0,0,1,-5.19327f} },
};
static const size_t xsmallVisualCount = sizeof(xsmallVisuals) / sizeof(xsmallVisuals[0]);
}
