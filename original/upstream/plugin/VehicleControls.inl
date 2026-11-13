// Included by NativeRuntime. Contextual bindings stay out of InputHandler's
// global one-command-per-key map so WASD/R/H retain their normal uses on foot.
enum VehicleBinding {EngineBinding,ExitBinding,ForwardBinding,BackBinding,LeftBinding,RightBinding,CameraBinding,HeadlightsBinding,VehicleBindingCount};
const char* bindingNames[VehicleBindingCount]={"Vehicle: toggle engine","Vehicle: get out","Manual driving: accelerate","Manual driving: brake / reverse","Manual driving: steer left","Manual driving: steer right","Manual driving: cycle camera","Vehicle: toggle headlights"};
const wchar_t* bindingIds[VehicleBindingCount]={L"Engine",L"Exit",L"Accelerate",L"BrakeReverse",L"SteerLeft",L"SteerRight",L"CameraView",L"Headlights"};
const int bindingDefaults[VehicleBindingCount]={OIS::KC_R,OIS::KC_H,OIS::KC_W,OIS::KC_S,OIS::KC_A,OIS::KC_D,OIS::KC_V,OIS::KC_F};
int bindingCodes[VehicleBindingCount];
cars::VehicleKeyPress vehicleKeys[256];
int captureBinding=-1;
bool bindingsLoaded=false;
DWORD bindingRefreshAt=0;
std::wstring bindingPath;
void (*originalOptionsCreate)(OptionsWindow*)=0;
void (*originalOptionsShow)(OptionsWindow*,bool)=0;
void (*originalOptionsDefaults)(OptionsWindow*,DataPanelLine*)=0;
void (*originalOptionsSave)(OptionsWindow*)=0;
bool (*originalWaitingForKey)(OptionsWindow*)=0;
void (*originalOptionsSetKey)(OptionsWindow*,const OIS::KeyCode&)=0;
void (*originalCameraUpdate)(CameraClass*,bool)=0;
void refreshVehicleBindings(OptionsWindow*);
#include "VehicleCamera.inl"
void refreshVehicleGraphics(OptionsWindow* window) {
    if(!window || !window->tabs)return;
    MyGUI::TabControl* tabs=window->tabs;
    for(size_t n=0;n<tabs->getItemCount() && n<32;++n) {
        std::string name=tabs->getItemNameAt(n).asUTF8();
        for(size_t k=0;k<name.size();++k)name[k]=static_cast<char>(tolower(static_cast<unsigned char>(name[k])));
        if(name.find("graphics")==std::string::npos)continue;
        DatapanelGUI** stored=tabs->getItemDataAt<DatapanelGUI*>(n,false);if(!stored || !*stored)return;
        DatapanelGUI* panel=*stored;const int category=panel->_NV_getCurrentCategory();
        const std::string label="First Person Vehicle FOV";
        if(!panel->lineExists(label,category)) {
            DataPanelLine_Slider* line=panel->setLineSlider(label,label,45.f,110.f,category,&firstPersonVehicleFov,1.f);
            if(line)line->setToolTip("Vertical field of view while driving in first person. Default 75 degrees. V switches view; middle mouse toggles looking around and using the cursor.",window->tooltip);
        }
        return;
    }
}
void readVehicleBindings() {
    if(bindingsLoaded)return;
    HMODULE module=0;wchar_t path[32768];
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&bindingsLoaded),&module)) {
        DWORD length=GetModuleFileNameW(module,path,32768);
        if(length && length<32768){bindingPath.assign(path,length);bindingPath=bindingPath.substr(0,bindingPath.find_last_of(L"\\/")+1)+L"ProjectCarsControls.ini";}
    }
    for(int n=0;n<VehicleBindingCount;++n) {
        int value=bindingPath.empty()?bindingDefaults[n]:GetPrivateProfileIntW(L"Controls",bindingIds[n],bindingDefaults[n],bindingPath.c_str());
        bindingCodes[n]=value>=0 && value<2048 && (value&255)!=OIS::KC_ESCAPE?value:bindingDefaults[n];
    }
    firstPersonVehicleFov=float(cars::vehicleFov(bindingPath.empty()?75:GetPrivateProfileIntW(L"Graphics",L"FirstPersonVehicleFOV",75,bindingPath.c_str())));
    bindingsLoaded=true;
}
void saveVehicleBindings() {
    if(bindingPath.empty())return;
    for(int n=0;n<VehicleBindingCount;++n){std::wostringstream value;value<<bindingCodes[n];
        if(!WritePrivateProfileStringW(L"Controls",bindingIds[n],value.str().c_str(),bindingPath.c_str()))ErrorLog("Project Cars: vehicle key binding could not be saved");}
}
void resetVehicleBindings(DataPanelLine*) {
    readVehicleBindings();for(int n=0;n<VehicleBindingCount;++n)bindingCodes[n]=bindingDefaults[n];
    captureBinding=-1;saveVehicleBindings();refreshVehicleBindings(OptionsWindow::getSingleton());
}
void chooseVehicleBinding(DataPanelLine* line) {
    if(!line)return;
    OptionsWindow* window=OptionsWindow::getSingleton();if(window && window->keyConfig)window->keyConfig->cancel();
    for(int n=0;n<VehicleBindingCount;++n)if(line->keyValue==bindingNames[n]){captureBinding=n;break;}
    refreshVehicleBindings(OptionsWindow::getSingleton());
}
void refreshVehicleBindings(OptionsWindow* window) {
    refreshVehicleGraphics(window);
    if(!window || !window->keysDatapanel || !key)return;
    readVehicleBindings();DatapanelGUI* panel=window->keysDatapanel;
    if(!panel->lineExists("Project Cars controls",0)) {
        panel->addSpace(0,1);
        panel->setLineText("Project Cars controls","Vehicles",0,false,MyGUI::Align::Left);
    }
    for(int n=0;n<VehicleBindingCount;++n) {
        std::string caption=captureBinding==n?"Press a key (Esc cancels)":bindingCodes[n]?key->keyString(bindingCodes[n],true):"Unbound";
        DataPanelLine_Button* row=panel->setLineTextButton(bindingNames[n],caption,0,.5f,"Kenshi_Button1");
        if(row)row->callback=MyGUI::newDelegate(chooseVehicleBinding);
    }
    panel->setLineText("Project Cars controls help","Engine / get out / headlights affect selected occupants. V cycles Standard / Follow / First Person in manual driving. Middle mouse releases first-person look. Escape ends manual driving. Delete clears a binding.",0,true,MyGUI::Align::Left);
    DataPanelLine_Button* reset=panel->setLineTextButton("Reset vehicle controls","Defaults",0,.5f,"Kenshi_Button1");
    if(reset)reset->callback=MyGUI::newDelegate(resetVehicleBindings);
}
void optionsCreate(OptionsWindow* window){originalOptionsCreate(window);refreshVehicleBindings(window);}
void optionsShow(OptionsWindow* window,bool on){captureBinding=-1;originalOptionsShow(window,on);if(on)refreshVehicleBindings(window);}
void optionsDefaults(OptionsWindow* window,DataPanelLine* line){originalOptionsDefaults(window,line);resetVehicleBindings(0);}
void optionsSave(OptionsWindow* window) {
    originalOptionsSave(window);readVehicleBindings();firstPersonVehicleFov=float(cars::vehicleFov(firstPersonVehicleFov));
    std::wostringstream value;value<<int(firstPersonVehicleFov);
    if(!bindingPath.empty() && !WritePrivateProfileStringW(L"Graphics",L"FirstPersonVehicleFOV",value.str().c_str(),bindingPath.c_str()))ErrorLog("Project Cars: camera FOV could not be saved");
}
bool optionsWaitingForKey(OptionsWindow* window){const bool native=originalWaitingForKey(window);if(native)captureBinding=-1;return native || (captureBinding>=0 && window->isVisible());}
void optionsSetKey(OptionsWindow* window,const OIS::KeyCode& code) {
    if(captureBinding>=0 && window->isVisible()){if(key)vehicleControlDown(key,code);return;}
    originalOptionsSetKey(window,code);
}
bool vehicleOptionsOpen(){OptionsWindow* w=OptionsWindow::getSingleton();return w && w->isVisible();}
int keyModifiers(InputHandler* input){return (input->ctrl?InputHandler::CTRL_MASK:0)|(input->shift?InputHandler::SHIFT_MASK:0)|(input->alt?InputHandler::ALT_MASK:0);}
bool modifierKey(int code){return code==OIS::KC_LSHIFT || code==OIS::KC_RSHIFT || code==OIS::KC_LCONTROL || code==OIS::KC_RCONTROL || code==OIS::KC_LMENU || code==OIS::KC_RMENU;}
bool vehicleControlDown(InputHandler* input,OIS::KeyCode code) {
    const unsigned index=static_cast<unsigned>(code);if(index>=256 || modifierKey(code))return false;
    readVehicleBindings();bool fresh=false;
    if(captureBinding>=0 && vehicleOptionsOpen()) {
        vehicleKeys[index].down(true,fresh);
        if(fresh) {
            if(code!=OIS::KC_ESCAPE) {
                const int binding=code==OIS::KC_DELETE?0:(int(code)|keyModifiers(input));
                if(binding!=(OIS::KC_F12|InputHandler::CTRL_MASK)) {
                    for(int n=0;n<VehicleBindingCount;++n)if(binding && bindingCodes[n]==binding)bindingCodes[n]=0;
                    bindingCodes[captureBinding]=binding;saveVehicleBindings();
                }
            }
            captureBinding=-1;refreshVehicleBindings(OptionsWindow::getSingleton());
        }
        return true;
    }
    const bool allowed=!faulted && readyLastFrame && input->controlEnabled && inputAllowed() && !cars::workshopVisible() && !vehicleOptionsOpen();
    int action=-1;const int binding=int(code)|keyModifiers(input);
    if(allowed)for(int n=0;n<VehicleBindingCount;++n)if(bindingCodes[n] && bindingCodes[n]==binding){action=n;break;}
    const bool motion=action>=ForwardBinding && action<=RightBinding;
    const cars::VehicleClick click=action==EngineBinding?cars::IgnitionVehicle:action==HeadlightsBinding?cars::HeadlightsVehicle:cars::LeaveVehicle;
    bool eligible=(motion || action==CameraBinding)?cars::manualDriving():action>=0?cars::selectedVehicleAction(click,false):false;
    const bool consumed=vehicleKeys[index].down(eligible,fresh);
    if(fresh && eligible && action>=0){if(action==CameraBinding)toggleVehicleView();else if(!motion)cars::selectedVehicleAction(click,true);}
    return consumed;
}
bool vehicleControlUp(OIS::KeyCode code){const unsigned index=static_cast<unsigned>(code);return index<256 && vehicleKeys[index].up();}
void updateVehicleControls() {
    readVehicleBindings();
    OptionsWindow* window=OptionsWindow::getSingleton();const bool optionsOpen=window && window->isVisible();
    if(optionsOpen && GetTickCount()-bindingRefreshAt>=250){bindingRefreshAt=GetTickCount();refreshVehicleBindings(window);}
    if(!optionsOpen)captureBinding=-1;
    const bool allowed=!faulted && readyLastFrame && key && key->controlEnabled && inputAllowed() && !cars::workshopVisible() && !optionsOpen;
    const bool manual=cars::manualDriving();
    for(unsigned n=0;n<256;++n)if(vehicleKeys[n].held) {
        if(key && key->keyboard && !key->keyboard->isKeyDown(static_cast<OIS::KeyCode>(n)))vehicleKeys[n].up();
        else if(!allowed || !manual)vehicleKeys[n].suspend();
    }
    cars::ManualDrivingKeys keys;keys.enabled=allowed;
    bool* values[]={&keys.forward,&keys.back,&keys.left,&keys.right};
    for(unsigned n=0;n<4;++n){const int binding=bindingCodes[n+ForwardBinding];*values[n]=allowed && binding && vehicleKeys[binding&255].active && key && keyModifiers(key)==(binding&InputHandler::ALL_MASK);}
    if(worldReady())cars::updateManualDriving(keys);
    if(!worldReady() || !allowed || !cars::manualDriving() || !vehicleForeground())releaseVehiclePointer();
    // A zone-loading panel must not relinquish the manual driver or camera.
    // The world-reset hook and explicit exit paths own that cleanup.
}
void cameraUpdate(CameraClass* camera,bool enabled) {
    vehicleCameraUpdate(camera,enabled);
}
bool installVehicleControls() {
    readVehicleBindings();
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::create),optionsCreate,&originalOptionsCreate)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::_NV_show),optionsShow,&originalOptionsShow)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::resetAllKeys),optionsDefaults,&originalOptionsDefaults)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::saveOptions),optionsSave,&originalOptionsSave)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::waitingForKey),optionsWaitingForKey,&originalWaitingForKey)!=KenshiLib::SUCCESS)return false;
    if(KenshiLib::AddHook(KenshiLib::GetRealAddress(&OptionsWindow::setKey),optionsSetKey,&originalOptionsSetKey)!=KenshiLib::SUCCESS)return false;
    return KenshiLib::AddHook(KenshiLib::GetRealAddress(&CameraClass::update),cameraUpdate,&originalCameraUpdate)==KenshiLib::SUCCESS;
}
