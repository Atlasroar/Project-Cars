#pragma once
#include <float.h>
namespace cars {
static const char* const dieselEngineItemId="175-Project Cars.mod";
static const char* const vanillaFuelItemId="1514-gamedata.base";
inline double fuelItemLitres(double charges) {return _finite(charges) && charges>0 && charges<=1?5*charges:0;}
}
