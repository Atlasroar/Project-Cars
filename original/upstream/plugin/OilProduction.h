#pragma once
#include <string>
class Building;class Character;class GameData;
namespace cars {
bool installOilProduction(); void resetOilDeposits();
bool isOilPool(Building*);
std::string buildPoolOilJack(Building*,Character*);
void saveOilPool(Building*,GameData*);
void loadOilPool(Building*,GameData*);
void forgetOilBuilding(Building*);
}
