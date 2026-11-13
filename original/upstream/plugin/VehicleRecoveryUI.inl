bool recoveryConfirm=false,recoveryCancel=false;int recoveryTurn=0;
void recoveryMouse(MyGUI::Widget*,int,int,MyGUI::MouseButton b){if(b==MyGUI::MouseButton::Left)recoveryConfirm=true;else if(b==MyGUI::MouseButton::Right)recoveryCancel=true;}
void clearRecoveryUI(){MyGUI::Gui* gui=MyGUI::Gui::getInstancePtr();if(gui){MyGUI::Widget* w=gui->findWidget<MyGUI::Widget>("ProjectCars_RecoveryInput",false);if(w)gui->destroyWidget(w);}recoveryConfirm=recoveryCancel=false;recoveryTurn=0;}
void updateRecoveryUI() {
    if(recoveryCancel){cars::cancelVehicleRecovery();recoveryCancel=false;}
    if(!cars::vehicleRecoveryActive()){clearRecoveryUI();return;}
    MyGUI::Gui* gui=MyGUI::Gui::getInstancePtr();MyGUI::RenderManager* render=MyGUI::RenderManager::getInstancePtr();MyGUI::InputManager* input=MyGUI::InputManager::getInstancePtr();
    if(!gui || !render || !input || !ou->player->camera || !ou->player->camera->camera)return;
    MyGUI::IntSize size=render->getViewSize();if(size.width<=0||size.height<=0)return;
    MyGUI::Widget* w=gui->findWidget<MyGUI::Widget>("ProjectCars_RecoveryInput",false);
    if(!w){
        // Empty ImageBox has no visible surface; it captures placement clicks
        // without selecting people, issuing orders or starting construction.
        w=gui->createWidget<MyGUI::ImageBox>("ImageBox",0,0,size.width,size.height,MyGUI::Align::Stretch,"Overlapped","ProjectCars_RecoveryInput");
        w->setNeedMouseFocus(true);w->setNeedKeyFocus(false);w->eventMouseButtonReleased+=MyGUI::newDelegate(recoveryMouse);
        MyGUI::TextBox* label=w->createWidget<MyGUI::TextBox>("Kenshi_TextboxPaintedText",20,size.height-105,size.width-40,65,MyGUI::Align::Default,"ProjectCars_RecoveryHelp");label->setNeedMouseFocus(false);label->setNeedKeyFocus(false);label->setFontHeight(20);
    }
    w->setSize(size.width,size.height);
    MyGUI::IntPoint mouse=input->getMousePosition();Ogre::Ray ray=ou->player->camera->camera->getCameraToViewportRay(float(mouse.left)/size.width,float(mouse.top)/size.height);
    cars::updateVehicleRecovery(ray,recoveryConfirm,recoveryTurn);recoveryConfirm=false;recoveryTurn=0;
    MyGUI::TextBox* label=gui->findWidget<MyGUI::TextBox>("ProjectCars_RecoveryHelp",false);if(label){label->setPosition(20,size.height-105);label->setCaption(cars::vehicleRecoveryStatus());}
    if(!cars::vehicleRecoveryActive())clearRecoveryUI();
}
