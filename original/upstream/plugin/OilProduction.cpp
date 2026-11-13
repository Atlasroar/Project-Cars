#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/GameData.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/Character.h>
#include <map>
#include <kenshi/RootObjectFactory.h>
#include <mygui/MyGUI_Widget.h>
#include <kenshi/gui/InventoryGUI.h>
#include <kenshi/Building/ProductionBuilding.h>
#include "OilProduction.h"
#include "OilContent.generated.h"
#include "../src/OilDeposits.h"
namespace {
struct Registry {
    CRITICAL_SECTION lock;cars::OilDeposits deposits;unsigned generation;bool ready;
    std::map<hand,Ogre::Vector3> jacks;
    std::map<hand,hand> pumps;
    Registry():generation(0),ready(false){InitializeCriticalSection(&lock);}
    ~Registry(){DeleteCriticalSection(&lock);}
} registry;
struct Guard { Guard(){EnterCriticalSection(&registry.lock);}~Guard(){LeaveCriticalSection(&registry.lock);} };
typedef Building* (*CreateBuilding)(RootObjectFactory*,GameData*,Ogre::Vector3,TownBase*,Faction*,Ogre::Quaternion,FactoryCallbackInterface*,Layout*,Building*,GameSaveState*,Building*,bool,bool,bool,int,bool);
CreateBuilding originalCreate=0;
float (*originalMining)(const ProductionBuilding*)=0;
// Factory input is a live GameData supplied by the native caller. Register only
// successful Oil Pool creations; foliage spawn and save restoration use this path.
Building* create(RootObjectFactory* self,GameData* data,Ogre::Vector3 position,TownBase* town,Faction* owner,Ogre::Quaternion rotation,FactoryCallbackInterface* callback,Layout* furniture,Building* door,GameSaveState* save,Building* indoors,bool invisible,bool complete,bool foliage,int floor,bool outside){
    const bool pool=data && data->stringID==cars::oilPoolRecord;
    unsigned generation=0;if(pool){Guard guard;generation=registry.generation;}
    Building* result=originalCreate(self,data,position,town,owner,rotation,callback,furniture,door,save,indoors,invisible,complete,foliage,floor,outside);
    if(pool && result){try{const Ogre::Vector3 actual=result->getPosition();Guard guard;if(generation==registry.generation)registry.deposits.add(actual.x,actual.y,actual.z);}catch(...){ErrorLog("Project Cars: oil deposit registration failed");}}
    if(result && data && data->stringID==cars::oilJackRecord){Guard guard;registry.jacks[result->getHandle()]=result->getPosition();}
    return result;
}
float mining(const ProductionBuilding* self){
    // Let every other mine retain its native resource, fertility and quality.
    if(!self || !self->getGameData() || self->getGameData()->stringID!=cars::oilJackRecord)return originalMining(self);
    Ogre::Vector3 p=const_cast<ProductionBuilding*>(self)->getPosition();
    Guard guard;return registry.ready && registry.deposits.canPump(p.x,p.y,p.z)?1.0f:0.0f;
}
}
namespace cars {
void resetOilDeposits(){Guard guard;++registry.generation;registry.deposits.clear();registry.jacks.clear();registry.pumps.clear();}
bool isOilPool(Building* b){return b && b->isValid() && b->getGameData() && b->getGameData()->stringID==oilPoolRecord;}
void saveOilPool(Building* b,GameData* state){if(!state || !isOilPool(b))return;Guard guard;std::map<hand,hand>::const_iterator i=registry.pumps.find(b->getHandle());if(i!=registry.pumps.end())state->storeHandle(i->second,"ProjectCars oil pump v1",false);}
void loadOilPool(Building* b,GameData* state){if(!state || !isOilPool(b))return;hand pump;if(state->getHandle(pump,"ProjectCars oil pump v1") && !pump.isNull()){Guard guard;registry.pumps[b->getHandle()]=pump;}}
void forgetOilBuilding(Building* b){if(!b)return;Guard guard;registry.jacks.erase(b->getHandle());registry.pumps.erase(b->getHandle());for(std::map<hand,hand>::iterator i=registry.pumps.begin();i!=registry.pumps.end();)if(i->second==b->getHandle())registry.pumps.erase(i++);else ++i;}
std::string buildPoolOilJack(Building* pool,Character* actor) {
    if(!ou || !ou->theFactory || !isOilPool(pool) || !actor || !actor->isValid() || !actor->isPlayerCharacter())return "Select a character beside an Oil Pool.";
    const Ogre::Vector3 position=pool->getPosition();
    if(actor->getPosition().squaredDistance(position)>120*120)return "Move beside the Oil Pool first.";
    {
        Guard guard;
        if(registry.pumps.count(pool->getHandle()))return "This pool already has an Oil Jack.";
        for(std::map<hand,Ogre::Vector3>::const_iterator i=registry.jacks.begin();i!=registry.jacks.end();++i)
            if(i->second.squaredDistance(position)<40*40)return "This pool already has an Oil Jack.";
    }
    GameData* data=ou->gamedata.getData(oilJackRecord,BUILDING);if(!data)return "Oil Jack definition unavailable.";
    // Native outside construction adds terrain height. Use a zero Y offset,
    // keep normal construction costs/work, and let the native automatic
    // production loop handle power, output capacity and save persistence.
    Building* jack=ou->theFactory->createBuilding(data,Ogre::Vector3(position.x,0,position.z),actor->getCurrentTownLocation(),actor->getFaction(),pool->rot,0,0,0,0,0,false,false,false,0,true);
    if(!jack || !jack->isValid())return "Oil Jack placement failed.";
    {Guard guard;registry.deposits.add(position.x,position.y,position.z);registry.pumps[pool->getHandle()]=jack->getHandle();}
    pool->notifyChange();jack->notifyChange();
    return "Oil Jack placed. Build it and supply power to extract crude oil automatically.";
}
bool installOilProduction(){
    // Verified native chain: ProductionBuilding::operate -> virtual
    // getProductionMult -> virtual getMiningResourceLevel. GUI mult uses the
    // same resource getter. Native power/staffing/inventory logic is preserved.
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&ProductionBuilding::_NV_getMiningResourceLevel),mining,&originalMining)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&RootObjectFactory::createBuilding),create,&originalCreate)!=KenshiLib::SUCCESS)return false;
    {Guard guard;registry.ready=true;}
    DebugLog("Project Cars: Oil Pools and 120-unit Oil Jack deposit gating enabled without datapanel patching");return true;
}
}
