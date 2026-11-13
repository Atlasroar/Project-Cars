#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mygui/MyGUI.h>
#include <Debug.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "VehicleWorkshop.h"
#include "../src/WorkshopData.generated.h"

namespace {
const char* rootName="ProjectCars_Workshop";
const char* systemNames[]={"Drivetrain","Cockpit","Armor","Utility","Fuel","Work queue"};
const char* regionNames[]={"Body","Nose","Suspension","FL wheel","FR wheel","RL wheel","RR wheel"};
std::string target,command;
cars::WorkshopRequest fittingRequest;
cars::WorkshopSnapshot data;
int systemIndex=0,regionIndex=0,page=0;
const unsigned queuePageSize=6;
size_t selectedMount=0,selectedItem=0;
bool renameEditing=false;
MyGUI::EditBox* renameEdit=0;
MyGUI::IntSize viewport;
cars::WorkshopLayout layout(1064,684);
const MyGUI::Colour ivory(0.87f,0.84f,0.76f),muted(0.60f,0.59f,0.54f),amber(0.89f,0.64f,0.30f),green(0.57f,0.72f,0.47f);
std::string number(int n) {std::ostringstream s;s<<n;return s.str();}
std::string decimal(double n) {std::ostringstream s;s<<std::fixed<<std::setprecision(1)<<n;return s.str();}
MyGUI::Widget* root() {
    MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();return g?g->findWidget<MyGUI::Widget>(rootName,false):0;
}
MyGUI::TextBox* text(MyGUI::Widget* parent,int x,int y,int w,int h,const std::string& caption,
                    const MyGUI::Colour& colour=ivory,bool large=false) {
    MyGUI::TextBox* t=parent->createWidget<MyGUI::TextBox>(large?"Kenshi_TextboxPaintedText_Large":"Kenshi_TextboxPaintedText",
        layout.px(x),layout.px(y),layout.px(w),layout.px(h),MyGUI::Align::Default);
    t->setCaption(caption);t->setTextColour(colour);t->setTextAlign(MyGUI::Align::Left|MyGUI::Align::Top);
    t->setFontHeight(std::max(12,layout.px(large?22:16)));
    t->setNeedMouseFocus(false);return t;
}
MyGUI::Widget* slab(MyGUI::Widget* parent,int x,int y,int w,int h) {
    MyGUI::Widget* p=parent->createWidget<MyGUI::Widget>("Kenshi_GenericWindowSkin",layout.px(x),layout.px(y),layout.px(w),layout.px(h),MyGUI::Align::Default);
    return p;
}
void clicked(MyGUI::Widget* sender) {
    if(sender && sender->getUserString("PC_Target")==target) command=sender->getUserString("PC_Action");
}
MyGUI::Button* button(MyGUI::Widget* parent,int x,int y,int w,int h,const std::string& label,const std::string& action,bool selected=false) {
    MyGUI::Button* b=parent->createWidget<MyGUI::Button>("Kenshi_Button1",layout.px(x),layout.px(y),layout.px(w),layout.px(h),MyGUI::Align::Default);
    b->setCaption(label);b->setFontHeight(std::max(12,layout.px(16)));b->setTextColour(selected?amber:ivory);
    b->setStateSelected(selected);b->setUserString("PC_Target",target);b->setUserString("PC_Action",action);
    b->eventMouseButtonClick+=MyGUI::newDelegate(clicked);return b;
}
void image(MyGUI::Widget* parent,int x,int y,int w,int h,const cars::WorkshopItem& item) {
    double scale=std::min(w/static_cast<double>(item.width),h/static_cast<double>(item.height));
    int iw=static_cast<int>(item.width*scale),ih=static_cast<int>(item.height*scale);
    MyGUI::ImageBox* picture=parent->createWidget<MyGUI::ImageBox>("ImageBox",layout.px(x+(w-iw)/2),layout.px(y+(h-ih)/2),layout.px(iw),layout.px(ih),MyGUI::Align::Default);
    picture->setNeedMouseFocus(false);
    try {picture->setImageTexture(item.icon);} catch (...) {picture->setVisible(false);}
}
std::vector<size_t> visibleMounts() {
    std::vector<size_t> mounts=cars::workshopSlots(cars::workshopMounts,cars::workshopMountCount,systemIndex,regionIndex);
    for(std::vector<size_t>::iterator i=mounts.begin();i!=mounts.end();)
        if(!cars::workshopMountSupported(data.extraSmall,cars::workshopMounts[*i].id,data.motorcycle))i=mounts.erase(i);
        else ++i;
    return mounts;
}
std::vector<size_t> compatible(size_t mount) {
    std::vector<size_t> result;
    if(mount>=cars::workshopMountCount) return result;
    for(size_t i=0;i<cars::workshopItemCount;++i) if(cars::workshopAccepts(cars::workshopMounts[mount],cars::workshopItems[i].id)) result.push_back(i);
    return result;
}
void choose(size_t mount) {
    if(mount>=cars::workshopMountCount) return;
    selectedMount=mount;
    const std::vector<size_t> choices=compatible(mount);
    selectedItem=choices.empty()?0:choices[0];
    std::map<std::string,std::string>::const_iterator installed=data.installed.find(cars::workshopMounts[mount].id);
    if(installed!=data.installed.end())for(size_t i=0;i<choices.size();++i)
        if(installed->second==cars::workshopItems[choices[i]].id)selectedItem=choices[i];
}
const char* mountName(const cars::WorkshopSlot& slot) {
    if(data.motorcycle) {
        const std::string id=slot.id;
        if(id=="wheel_fl")return "Front wheel";
        if(id=="wheel_rl")return "Rear wheel";
        if(id=="steering")return "Fork and handlebars";
    }
    return slot.name;
}
bool occupied(const char* slot) {return data.installed.find(slot)!=data.installed.end();}
bool dieselInstalled(){return occupied("engine") && data.installed.find("engine")->second==cars::dieselEngineItemId;}
std::string countText(int count) {return count<0?"--":number(count);}
std::string wrap(const std::string& input,size_t columns,unsigned maxLines) {
    std::istringstream in(input);std::string word,result;size_t used=0;unsigned lines=1;
    while(in>>word) {
        if(used && used+1+word.size()>columns) {if(lines==maxLines){result+="...";break;}result+='\n';used=0;++lines;}
        if(used) {result+=' ';++used;}result+=word;used+=word.size();
    }
    return result;
}
void metric(MyGUI::Widget* p,int x,const std::string& title,const std::string& value,bool good) {
    MyGUI::Widget* m=slab(p,x,88,244,64);text(m,14,8,216,20,title,muted);text(m,14,31,216,27,value,good?green:amber,true);
}
void inspector(MyGUI::Widget* p) {
    MyGUI::Widget* panel=slab(p,686,170,334,410);
    if(systemIndex==5) {
        text(panel,18,14,296,23,"WORK QUEUE",amber);
        text(panel,18,61,296,52,number(static_cast<int>(data.queueSlots.size()))+" operations remaining",ivory);
        text(panel,18,134,296,110,"Parts are fitted one at a time.\nPause retains progress.\nResume after bringing supplies.",muted);
        text(panel,18,280,296,96,wrap(data.status,30,5),data.queueRunning?green:amber);return;
    }
    if(systemIndex==4) {
        text(panel,18,15,296,22,"FUEL SERVICE",amber);
        text(panel,18,57,296,54,data.tankCapacity>0?(dieselInstalled()?"Fill with ordinary Fuel.\nEach barrel adds 5 L.":"Fill with gasoline cans.\nEach can contains 5 L."):"Install a fuel tank first.");
        text(panel,18,133,296,75,"Stop the engine before servicing.\nDrained fuel returns in full cans.\nDiscarding a remainder loses it.",muted);
        button(panel,18,280,143,34,"Refuel 5 L","fuel_refuel")->setEnabled(data.fittingAllowed && data.tankCapacity-data.fuel>=5);
        button(panel,173,280,143,34,data.fuel>=5?"Drain 5 L":"Discard remainder","fuel_drain")->setEnabled(data.fittingAllowed && data.fuel>0);
        text(panel,18,336,296,56,wrap(data.status,30,3),muted);return;
    }
    const cars::WorkshopSlot& slot=cars::workshopMounts[selectedMount];
    const cars::WorkshopItem& item=cars::workshopItems[selectedItem];
    text(panel,18,14,296,23,"COMPONENT DETAILS",amber);
    image(panel,18,45,100,86,item);
    text(panel,134,49,180,44,wrap(item.name,19,2));
    int have=cars::workshopCount(data,item.id);
    text(panel,134,102,180,23,"Carried: "+countText(have),have>0?green:muted);
    text(panel,18,143,296,23,decimal(item.mass)+" kg  |  "+number(int(cars::workshopMaxHp(item.id)*(data.condition.count(slot.id)?data.condition[slot.id]:1)))+" / "+number(int(cars::workshopMaxHp(item.id)))+" HP",ivory);
    const std::string description=item.description;
    text(panel,18,171,296,69,wrap(description,30,3),muted);
    const std::vector<size_t> choices=compatible(selectedMount);
    if(choices.size()>1) {
        text(panel,18,250,296,22,"COMPATIBLE OPTIONS",amber);
        int w=(296-static_cast<int>(choices.size()-1)*6)/static_cast<int>(choices.size());
        for(size_t i=0;i<choices.size();++i) {
            const std::string id=cars::workshopItems[choices[i]].id;
            std::string label=slot.system==2?(i==0?"Light":i==1?"Medium":"Heavy"):(id==cars::dieselEngineItemId?"Diesel":id=="21-Project Cars.mod"?"Gasoline":id=="155-Project Cars.mod"?"Pole":id=="156-Project Cars.mod"?"Cage":id=="131-Project Cars.mod"?"Cargo":id=="172-Project Cars.mod"?"Turret":i==0?"Rack":"Spikes");
            button(panel,18+static_cast<int>(i)*(w+6),276,w,31,label,"item:"+number(static_cast<int>(choices[i])),selectedItem==choices[i])->setEnabled(!occupied(slot.id));
        }
    } else text(panel,18,260,296,40,"Fits: "+wrap(mountName(slot),28,2),muted);
    const bool queued=data.queued.count(slot.id)!=0;
    button(panel,18,321,143,35,"Queue install","install")->setEnabled(data.fittingAllowed && !queued && !occupied(slot.id));
    button(panel,173,321,143,35,queued?"Unqueue":"Queue remove",queued?"unqueue":"remove")->setEnabled(queued || (data.fittingAllowed && occupied(slot.id)));
    text(panel,18,368,296,38,wrap(data.status,32,2),muted);
}
void fuel(MyGUI::Widget* p) {
    text(p,204,188,452,28,"FUEL",ivory,true);
    MyGUI::Widget* tank=slab(p,204,245,450,140);
    text(tank,20,18,410,28,data.tankCapacity>0?"FUEL LEVEL":"NO TANK INSTALLED",amber);
    text(tank,20,61,410,36,decimal(data.fuel)+" / "+decimal(data.tankCapacity)+" L",ivory,true);
    text(p,204,410,450,80,occupied("engine")?(dieselInstalled()?"Diesel engine - uses Fuel\nFuel use rises with throttle.":"Gasoline engine\nConsumption: 0.12 - 0.96 L/min\nFuel use rises with throttle."):"No engine installed.",muted);
    text(p,204,520,450,44,"Drain the tank before removing it.",muted);
}
void mountGrid(MyGUI::Widget* p,const std::vector<size_t>& mounts) {
    text(p,204,188,450,28,systemIndex==0?"POWER & RUNNING GEAR":systemIndex==1?"COCKPIT":systemIndex==2?"ARMOR MOUNTS":"UTILITY MOUNTS",ivory,true);
    int top=243;
    if(systemIndex==2) {
        for(int i=0;i<7;++i) button(p,204+(i%4)*113,229+(i/4)*34,107,29,i==2?"Susp.":regionNames[i],"region:"+number(i),regionIndex==i);
        top=312;
    }
    const int rowHeight=systemIndex==2?64:92;
    for(size_t j=0;j<6 && static_cast<size_t>(page*6)+j<mounts.size();++j) {
        size_t index=mounts[page*6+j];const cars::WorkshopSlot& slot=cars::workshopMounts[index];
        int x=204+static_cast<int>(j%2)*230,y=top+static_cast<int>(j/2)*rowHeight;
        bool chosen=index==selectedMount;
        MyGUI::Button* tile=button(p,x,y,220,rowHeight-9,"","mount:"+number(static_cast<int>(index)),chosen);
        const std::vector<size_t> choices=compatible(index);
        if(systemIndex!=2 && !choices.empty()) image(tile,8,9,47,47,cars::workshopItems[choices[0]]);
        int inset=systemIndex==2?12:63;
        text(tile,inset,9,220-inset-8,37,wrap(mountName(slot),systemIndex==2?21:15,2),chosen?amber:ivory);
        const bool queued=data.queued.count(slot.id)!=0;
        const bool broken=data.condition.count(slot.id) && data.condition[slot.id]<=0;
        const bool wheelAlternative=std::string(slot.id).find("wheel_")==0 && (occupied("track_l")||occupied("track_r"));
        const bool required=slot.required || (data.extraSmall && std::string(slot.id)=="steering");
        text(tile,inset,rowHeight-34,220-inset-8,19,queued?data.queued.find(slot.id)->second:broken?"DESTROYED":occupied(slot.id)?"INSTALLED":wheelAlternative?"TRACKS FITTED":required?"EMPTY / REQUIRED":"EMPTY",queued||broken?amber:occupied(slot.id)?green:muted);
    }
    if(mounts.size()>6) {
        button(p,204,535,68,29,"<","prev")->setEnabled(page>0);
        text(p,284,540,270,24,"Page "+number(page+1)+" / "+number(static_cast<int>((mounts.size()+5)/6)),muted);
        button(p,586,535,68,29,">","next")->setEnabled(static_cast<size_t>((page+1)*6)<mounts.size());
    }
}
void queueView(MyGUI::Widget* p) {
    text(p,204,188,450,28,data.queueRunning?"WORK IN PROGRESS":"PLANNED WORK",ivory,true);
    if(data.queueSlots.empty()){text(p,204,250,448,70,"Select mounts and queue the parts\nyou want to install or remove.",muted);return;}
    const size_t start=static_cast<size_t>(page*queuePageSize);
    for(size_t i=start;i<data.queueSlots.size() && i<start+queuePageSize;++i) {
        int y=240+static_cast<int>(i-start)*47;
        std::string label=number(static_cast<int>(i+1))+". "+data.queueLabels[i];
        if(i==0 && data.working)label+=" "+number(static_cast<int>(data.workProgress*100))+"%";
        text(p,204,y,395,44,label,i==0&&data.queueRunning?green:ivory);
        button(p,612,y+8,40,27,"X","unqueue:"+data.queueSlots[i]);
    }
    button(p,204,535,68,29,"<","prev")->setEnabled(page>0);
    text(p,284,540,260,24,"Page "+number(page+1)+" / "+number(static_cast<int>((data.queueSlots.size()+queuePageSize-1)/queuePageSize)),muted);
    button(p,586,535,68,29,">","next")->setEnabled(start+queuePageSize<data.queueSlots.size());
}
void destroyRoot() {
    MyGUI::Widget* p=root();if(!p)return;
    MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();
    if(input) {input->removeWidgetModal(p);input->resetKeyFocusWidget();}
    MyGUI::Gui::getInstance().destroyWidget(p);
}
void draw() {
    destroyRoot();
    MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();
    if(!g || !render)return;
    viewport=render->getViewSize();layout=cars::WorkshopLayout(viewport.width,viewport.height);
    // The HUD uses Middle, which renders above Overlapped. Native inventory and
    // building windows use Window; modal input alone does not change draw order.
    MyGUI::Widget* p=g->createWidget<MyGUI::Widget>("Kenshi_GenericWindowSkin",layout.x,layout.y,layout.width,layout.height,MyGUI::Align::Default,"Window",rootName);
    text(p,24,18,720,33,wrap(data.vehicleName,45,1),ivory,true);
    button(p,750,20,38,34,"\xE2\x9C\x8E","rename");
    if(renameEditing) {
        MyGUI::EditBox* edit=p->createWidget<MyGUI::EditBox>("Kenshi_EditBox",layout.px(24),layout.px(57),layout.px(430),layout.px(34),MyGUI::Align::Default,"Window");
        renameEdit=edit;
        edit->setCaption(data.vehicleName);edit->setNeedMouseFocus(true);edit->setNeedKeyFocus(true);
        button(p,462,57,92,34,"SAVE","rename_save");
    }
    text(p,24,55,760,23,data.motorcycle?"MOTORCYCLE":data.extraSmall?"EXTRA-SMALL FRAME":"LIGHT FRAME",muted);
    text(p,801,29,161,25,data.fuel>0 && occupied("engine") && occupied("seat_driver")?"ASSEMBLING":"UNPOWERED",amber);
    button(p,975,21,43,35,"X","close");
    unsigned wheels=0;const char* wheelSlots[]={"wheel_fl","wheel_fr","wheel_rl","wheel_rr"};
    for(unsigned i=0;i<4;++i) if(occupied(wheelSlots[i]))++wheels;
    metric(p,20,"ENGINE",occupied("engine")?"FITTED":"MISSING",occupied("engine"));
    unsigned tracks=(occupied("track_l")?1:0)+(occupied("track_r")?1:0);
    metric(p,272,tracks?"TRACKS":"WHEELS",number(tracks?tracks:wheels)+(tracks || data.motorcycle?" / 2 FITTED":" / 4 FITTED"),tracks?tracks==2:wheels==(data.motorcycle?2:4));
    metric(p,524,"DRIVER SEAT",occupied("seat_driver")?"FITTED":"EMPTY",occupied("seat_driver"));
    metric(p,776,"FUEL",data.tankCapacity>0?decimal(data.fuel)+" L":"NO TANK",data.fuel>0);
    MyGUI::Widget* nav=slab(p,20,170,160,410);
    text(nav,14,15,134,24,"SYSTEMS",muted);
    for(int i=0;i<6;++i) button(nav,10,51+i*49,140,40,systemNames[i],"system:"+number(i),systemIndex==i)->setEnabled(!data.extraSmall || (i!=2 && i!=3));
    text(nav,12,354,140,52,"MECHANICS "+decimal(data.mechanics)+"\nHANDLING "+decimal(data.handling),muted);
    slab(p,188,170,482,410);
    const std::vector<size_t> mounts=visibleMounts();
    if(systemIndex==4)fuel(p);else if(systemIndex==5)queueView(p);else mountGrid(p,mounts);
    inspector(p);
    text(p,24,597,986,23,data.actorName.empty()?"SELECT A CHARACTER TO FIT PARTS":"PARTS CARRIED BY: "+wrap(data.actorName,60,1),muted);
    button(p,24,623,164,29,"Start / Resume","queue_start")->setEnabled(!data.queueSlots.empty() && !data.queueRunning && data.fittingAllowed);
    button(p,198,623,100,29,"Pause","queue_pause")->setEnabled(data.queueRunning);
    button(p,308,623,126,29,"Clear queue","queue_clear")->setEnabled(!data.queueSlots.empty());
    text(p,456,628,410,22,number(static_cast<int>(data.queueSlots.size()))+" queued",data.queueRunning?green:muted);
    text(p,887,628,123,22,"Esc: close",muted);
    MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();if(input)input->addWidgetModal(p);
}
void selectFirst() {
    page=0;std::vector<size_t> mounts=visibleMounts();
    if(!mounts.empty())choose(mounts[0]);
}
bool action(const std::string& value) {
    if(value=="close") {cars::closeWorkshop();return false;}
    if(value=="rename") {renameEditing=true;return true;}
    if(value=="rename_save") {
        MyGUI::EditBox* edit=renameEdit;
        if(edit) {std::string name=edit->getCaption().asUTF8();if(name.size()>40)name.resize(40);if(!name.empty()){fittingRequest.target=target;fittingRequest.actor=data.actorId;fittingRequest.slot="rename";fittingRequest.item=name;renameEditing=false;renameEdit=0;}}
        return true;
    }
    if(value=="fuel_refuel"||value=="fuel_drain") {
        if(fittingRequest.target.empty()){fittingRequest.target=target;fittingRequest.actor=data.actorId;fittingRequest.slot=value;}return true;
    }
    if(value.substr(0,6)=="queue_" || value=="unqueue" || value.substr(0,8)=="unqueue:") {
        if(fittingRequest.target.empty()) {
            fittingRequest.target=target;fittingRequest.actor=data.actorId;
            fittingRequest.slot=value.substr(0,6)=="queue_"?value:"queue_drop";
            fittingRequest.item=value=="unqueue"?cars::workshopMounts[selectedMount].id:value.substr(0,8)=="unqueue:"?value.substr(8):"";
        }return true;
    }
    if(value=="install" || value=="remove" || value=="cancel") {
        if((data.fittingAllowed || (value=="cancel" && data.working)) && fittingRequest.target.empty()) {
            fittingRequest.target=target;fittingRequest.actor=data.actorId;
            fittingRequest.slot=value=="cancel"?"cancel":cars::workshopMounts[selectedMount].id;
            fittingRequest.item=cars::workshopItems[selectedItem].id;fittingRequest.remove=value=="remove";
        }
        return true;
    }
    if(value.substr(0,7)=="system:") {int n=atoi(value.substr(7).c_str());if(n>=0 && n<6 && (!data.extraSmall || (n!=2 && n!=3))){systemIndex=n;selectFirst();}}
    else if(value.substr(0,7)=="region:") {int n=atoi(value.substr(7).c_str());if(n>=0 && n<7){regionIndex=n;selectFirst();}}
    else if(value.substr(0,6)=="mount:") {size_t n=static_cast<size_t>(atoi(value.substr(6).c_str()));
        if(n<cars::workshopMountCount && cars::workshopMountSupported(data.extraSmall,cars::workshopMounts[n].id,data.motorcycle) && cars::workshopMounts[n].system==systemIndex && (systemIndex!=2 || cars::workshopMounts[n].region==regionIndex))choose(n);}
    else if(value.substr(0,5)=="item:") {size_t n=static_cast<size_t>(atoi(value.substr(5).c_str()));
        if(n<cars::workshopItemCount && cars::workshopAccepts(cars::workshopMounts[selectedMount],cars::workshopItems[n].id))selectedItem=n;}
    else if(value=="prev" && page>0)--page;
    else if(value=="next") {std::vector<size_t> m=visibleMounts();if(systemIndex==5?static_cast<size_t>((page+1)*queuePageSize)<data.queueSlots.size():static_cast<size_t>((page+1)*6)<m.size())++page;}
    return true;
}
}
namespace cars {
void drivingStatus(const std::string& caption) {
    MyGUI::Gui* g=MyGUI::Gui::getInstancePtr();MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();if(!g||!render)return;
    MyGUI::TextBox* label=g->findWidget<MyGUI::TextBox>("ProjectCars_DrivingStatus",false);
    if(caption.empty()){if(label)label->setVisible(false);return;}
    if(!label){label=g->createWidget<MyGUI::TextBox>("Kenshi_TextboxPaintedText",0,0,420,76,MyGUI::Align::Default,"Overlapped","ProjectCars_DrivingStatus");label->setNeedMouseFocus(false);label->setNeedKeyFocus(false);label->setFontHeight(17);label->setTextColour(amber);}
    MyGUI::IntSize size=render->getViewSize();label->setPosition(std::max(10,size.width-440),16);label->setCaption(caption);label->setVisible(!workshopVisible());
}
void showWorkshop(const std::string& id,const WorkshopSnapshot& snapshot) {
    data=snapshot;
    if(id!=target){systemIndex=0;regionIndex=0;selectFirst();}
    target=id;command.clear();data=snapshot;draw();
}
void updateWorkshop(const WorkshopSnapshot& snapshot) {
    if(target.empty() || !root())return;
    bool changed=snapshot.inventory!=data.inventory || snapshot.installed!=data.installed || snapshot.actorName!=data.actorName || snapshot.vehicleName!=data.vehicleName ||
        snapshot.inventoryReadable!=data.inventoryReadable || snapshot.condition!=data.condition || snapshot.fuel!=data.fuel || snapshot.tankCapacity!=data.tankCapacity ||
        snapshot.status!=data.status || snapshot.actorId!=data.actorId || snapshot.fittingAllowed!=data.fittingAllowed ||
        snapshot.working!=data.working || snapshot.mechanics!=data.mechanics || snapshot.handling!=data.handling || snapshot.queueSlots!=data.queueSlots || snapshot.queueRunning!=data.queueRunning;
    const bool installedChanged=snapshot.installed!=data.installed;
    data=snapshot;if(installedChanged)choose(selectedMount);
    if(systemIndex==5)page=std::min(page,data.queueSlots.empty()?0:static_cast<int>((data.queueSlots.size()-1)/queuePageSize));
    if(!command.empty()) {std::string pending=command;command.clear();if(!action(pending))return;changed=true;}
    MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();
    if(render && render->getViewSize()!=viewport)changed=true;
    if(changed)draw();
}
void closeWorkshop() {command.clear();target.clear();renameEditing=false;renameEdit=0;fittingRequest=WorkshopRequest();destroyRoot();}
bool takeWorkshopRequest(WorkshopRequest& request) {if(fittingRequest.target.empty())return false;request=fittingRequest;fittingRequest=WorkshopRequest();return true;}
bool workshopVisible() {return !target.empty() && root()!=0;}
std::string workshopTarget() {return target;}
}
