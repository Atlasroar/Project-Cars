#define NOMINMAX
#include <mygui/MyGUI.h>
#include <algorithm>
#include <sstream>
#include "VehicleDebug.h"
namespace {
const char* debugRootName="ProjectCars_Debug";
cars::DebugItemChoices items;
cars::DebugRequest pending;
std::string status;
size_t itemIndex=0,quantityIndex=0,presetIndex=0,scopeIndex=1;
const unsigned quantities[]={1,4,10,25};
MyGUI::IntSize viewport;
double scale=1;
std::string wrapStatus(const std::string& text){
    std::istringstream words(text);std::string word,out;unsigned line=0;
    while(words>>word){if(line && line+word.size()+1>72){out+='\n';line=0;}if(line){out+=' ';++line;}out+=word;line+=static_cast<unsigned>(word.size());}return out;
}
int px(int n){return static_cast<int>(n*scale+.5);}
MyGUI::Widget* root(){MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();return g?g->findWidget<MyGUI::Widget>(debugRootName,false):0;}
MyGUI::ComboBox* combo(const char* name){MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();return g?g->findWidget<MyGUI::ComboBox>(name,false):0;}
void destroy(){MyGUI::Widget* w=root();if(!w)return;MyGUI::InputManager* i=MyGUI::InputManager::getInstancePtr();if(i){i->removeWidgetModal(w);i->resetKeyFocusWidget();}MyGUI::Gui::getInstance().destroyWidget(w);}
void remember(){MyGUI::ComboBox* c=combo("PC_DebugItems");if(c)itemIndex=c->getIndexSelected();c=combo("PC_DebugQuantity");if(c)quantityIndex=c->getIndexSelected();c=combo("PC_DebugPreset");if(c)presetIndex=c->getIndexSelected();c=combo("PC_DebugScope");if(c)scopeIndex=c->getIndexSelected();}
void clicked(MyGUI::Widget* w){
    if(pending.action!=cars::DebugNone)return;
    remember();std::istringstream code(w->getUserString("PC_DebugAction"));int action=0;code>>action;
    pending=cars::DebugRequest();pending.action=static_cast<cars::DebugAction>(action);
    if(itemIndex<items.size())pending.item=items[itemIndex].first;
    pending.quantity=quantityIndex<4?quantities[quantityIndex]:0;
    pending.preset=static_cast<int>(presetIndex);pending.scope=static_cast<int>(scopeIndex);
}
MyGUI::TextBox* label(int x,int y,int w,int h,const std::string& caption,bool heading=false,const char* name=""){
    MyGUI::TextBox* t=root()->createWidget<MyGUI::TextBox>(heading?"Kenshi_TextboxPaintedText_Large":"Kenshi_TextboxPaintedText",px(x),px(y),px(w),px(h),MyGUI::Align::Default,name);
    t->setCaption(caption);t->setFontHeight(std::max(12,px(heading?22:16)));t->setTextColour(heading?MyGUI::Colour(.89f,.64f,.30f):MyGUI::Colour(.87f,.84f,.76f));
    t->setTextAlign(MyGUI::Align::Left|MyGUI::Align::Top);t->setNeedMouseFocus(false);t->setNeedKeyFocus(false);return t;
}
void button(int x,int y,int w,const char* text,cars::DebugAction action){
    MyGUI::Button* b=root()->createWidget<MyGUI::Button>("Kenshi_Button1",px(x),px(y),px(w),px(36),MyGUI::Align::Default);
    b->setCaption(text);b->setFontHeight(std::max(12,px(16)));std::ostringstream code;code<<action;b->setUserString("PC_DebugAction",code.str());b->eventMouseButtonClick+=MyGUI::newDelegate(clicked);
}
MyGUI::ComboBox* makeCombo(int x,int y,int w,const char* name){
    MyGUI::ComboBox* c=root()->createWidget<MyGUI::ComboBox>("Kenshi_ComboBox",px(x),px(y),px(w),px(36),MyGUI::Align::Default,name);
    c->setComboModeDrop(true);c->setMaxListLength(px(240));c->setFontHeight(std::max(12,px(16)));return c;
}
void draw(){
    MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();MyGUI::RenderManager* r=MyGUI::RenderManager::getInstancePtr();if(!g||!r)return;
    destroy();viewport=r->getViewSize();scale=std::min(1.0,std::min((viewport.width-24)/680.0,(viewport.height-24)/590.0));scale=std::max(.25,scale);
    MyGUI::Widget* w=g->createWidget<MyGUI::Widget>("Kenshi_GenericWindowSkin",(viewport.width-px(680))/2,(viewport.height-px(590))/2,px(680),px(590),MyGUI::Align::Default,"Window",debugRootName);
    label(24,22,535,32,"VEHICLE DEBUG",true);button(612,18,44,"X",cars::DebugClose);
    label(24,72,610,24,"SPAWN ITEM");
    MyGUI::ComboBox* c=makeCombo(24,104,366,"PC_DebugItems");for(size_t i=0;i<items.size();++i)c->addItem(items[i].second);if(!items.empty())c->setIndexSelected(itemIndex<items.size()?itemIndex:0);
    c=makeCombo(402,104,106,"PC_DebugQuantity");for(unsigned i=0;i<4;++i){std::ostringstream s;s<<quantities[i];c->addItem(s.str());}c->setIndexSelected(quantityIndex<4?quantityIndex:0);button(520,104,136,"Spawn",cars::DebugSpawnItem);
    label(24,146,630,26,"Items go into the selected character's inventory.");
    label(24,188,610,24,"SPAWN VEHICLE");c=makeCombo(24,220,484,"PC_DebugPreset");for(int i=0;i<cars::DebugPresetCount;++i)c->addItem(cars::debugPresetName(i));c->setIndexSelected(presetIndex<cars::DebugPresetCount?presetIndex:0);button(520,220,136,"Spawn",cars::DebugSpawnVehicle);
    label(24,262,630,36,"Ready-to-drive presets spawn fueled with the engine off.");
    label(24,310,240,26,"VEHICLE ACTIONS");c=makeCombo(278,302,378,"PC_DebugScope");c->addItem("Selected vehicles / occupants");c->addItem("Nearby vehicles (50 m)");c->setIndexSelected(scopeIndex<2?scopeIndex:1);
    button(24,350,306,"Refill vehicles",cars::DebugRefill);button(342,350,314,"Repair vehicles",cars::DebugRepair);
    button(24,398,306,"Brake vehicles",cars::DebugBrake);button(342,398,314,"Stop engines",cars::DebugStopEngines);
    button(24,452,306,"Component chest",cars::DebugChest);label(348,458,308,30,"Ctrl+F12 / Escape: close");
    label(24,512,632,62,wrapStatus(status),false,"PC_DebugStatus");MyGUI::InputManager::getInstance().addWidgetModal(w);
}
}
namespace cars {
void showVehicleDebug(const DebugItemChoices& choices){items=choices;pending=DebugRequest();status="Select a character as the spawn point. Actions affect your vehicles.";draw();}
void closeVehicleDebug(){pending=DebugRequest();destroy();}
void updateVehicleDebug(){MyGUI::RenderManager* r=MyGUI::RenderManager::getInstancePtr();if(root()&&r&&r->getViewSize()!=viewport){remember();draw();}}
bool vehicleDebugVisible(){return root()!=0;}
void vehicleDebugStatus(const std::string& text){status=text;MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();MyGUI::TextBox* t=g?g->findWidget<MyGUI::TextBox>("PC_DebugStatus",false):0;if(t)t->setCaption(wrapStatus(text));}
bool takeVehicleDebugRequest(DebugRequest& request){if(pending.action==DebugNone)return false;request=pending;pending=DebugRequest();return true;}
void requestVehicleDebugClose(){pending=DebugRequest();pending.action=DebugClose;}
}
