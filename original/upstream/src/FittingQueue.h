#pragma once
#include <deque>
#include <sstream>
#include <iomanip>
#include "VehicleSkills.h"
#include "WorkshopModel.h"
namespace cars {
struct FitStep {std::string slot,item;bool remove;double done;FitStep():remove(false),done(0){}};
struct FittingQueue {
    std::deque<FitStep> steps;bool running;
    FittingQueue():running(false){}
    bool contains(const std::string& slot) const {for(size_t i=0;i<steps.size();++i)if(steps[i].slot==slot)return true;return false;}
    bool add(const FitStep& step) {if(steps.size()>=128 || contains(step.slot))return false;steps.push_back(step);return true;}
    std::map<std::string,std::string> projectedContents(const std::map<std::string,std::string>& installed) const {
        std::map<std::string,std::string> result=installed;
        for(size_t i=0;i<steps.size();++i) {
            if(steps[i].remove)removeMountAndDependents(result,steps[i].slot);else result[steps[i].slot]=steps[i].item;
        }
        return result;
    }
    // Validate each intermediate layout, not only the end of a proposed plan.
    bool layoutValid(const std::map<std::string,std::string>& installed,const WorkshopSlot* mounts,size_t count) const {
        std::map<std::string,std::string> state=installed;
        for(size_t i=0;i<steps.size();++i) {
            if(steps[i].remove){removeMountAndDependents(state,steps[i].slot);continue;}
            const WorkshopSlot* slot=0;
            for(size_t j=0;j<count;++j)if(steps[i].slot==mounts[j].id){slot=mounts+j;break;}
            if(!slot || !workshopConflict(*slot,state).empty())return false;
            state[steps[i].slot]=steps[i].item;
        }
        return true;
    }
    void pause(double progress) {running=false;if(!steps.empty())steps.front().done=_finite(progress)&&progress>0?progress:0;}
    void completed() {if(!steps.empty())steps.pop_front();if(steps.empty())running=false;}
    bool erase(const std::string& slot) {for(std::deque<FitStep>::iterator i=steps.begin();i!=steps.end();++i)if(i->slot==slot){steps.erase(i);if(steps.empty())running=false;return true;}return false;}
    std::string serialize() const {
        std::ostringstream out;out<<std::setprecision(17);
        for(size_t i=0;i<steps.size();++i)out<<steps[i].slot<<'\t'<<steps[i].item<<'\t'<<steps[i].remove<<'\t'<<steps[i].done<<'\n';
        return out.str();
    }
    bool load(const std::string& text) {
        FittingQueue read;std::istringstream rows(text);std::string line;
        while(std::getline(rows,line)) {
            if(line.empty())continue;FitStep step;int remove;std::string extra;
            size_t a=line.find('\t');if(a==std::string::npos)return false;
            size_t b=line.find('\t',a+1);if(b==std::string::npos)return false;
            step.slot=line.substr(0,a);step.item=line.substr(a+1,b-a-1);
            std::istringstream numbers(line.substr(b+1));if(!(numbers>>remove>>step.done)||(numbers>>extra)||remove<0||remove>1||!_finite(step.done)||step.done<0||step.done>600)return false;
            step.remove=remove!=0;if(step.slot.empty()||step.item.empty()||!read.add(step))return false;
        }
        steps.swap(read.steps);running=false;return true;
    }
};
}
