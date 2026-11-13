#pragma once
#include "Vehicle.h"
namespace cars {
class DebugKeyPress {
public:
    DebugKeyPress():armed_(false),held_(true),pending_(false){}
    void reset(){armed_=false;held_=true;pending_=false;}
    void observeReleased(bool ready,bool down){if(!ready){reset();return;}if(!down){armed_=true;held_=false;}}
    void press(bool eligible,bool modifiers){bool fresh=armed_ && !held_;held_=true;if(fresh && eligible && modifiers)pending_=true;}
    void release(){held_=false;}
    bool take(bool eligible){bool result=pending_ && eligible;pending_=false;return result;}
private: bool armed_,held_,pending_;
};
// Also used by the native observer: an absent construction state fails closed.
inline ConstructionObservation observeConstruction(bool readable, bool complete, bool dismantled) {
    if (!readable) return ConstructionUnknown;
    return complete && !dismantled ? ConstructionComplete : ConstructionUnfinished;
}
class DebugShortcut {
public:
    DebugShortcut() : wasDown_(true), last_(0), fired_(false) {}
    bool poll(bool down, bool eligible, unsigned long now) {
        bool edge = down && !wasDown_;
        wasDown_ = down;
        if (!edge || !eligible || (fired_ && static_cast<unsigned long>(now-last_) < 2000)) return false;
        last_ = now; fired_ = true; return true;
    }
private:
    bool wasDown_; unsigned long last_; bool fired_;
};
}
