#include "Vehicle.h"
#include <sstream>
#include <iomanip>
#include <locale>

namespace cars {
bool Vehicle::save(std::ostream& out, const std::string& key) const {
    if (!validToken(key)) return false;
    std::ostringstream data;
    data.imbue(std::locale::classic());
    const Phase stored = phase_ == Preview ? Portable : phase_;
    data << std::setprecision(17) << "projectcars 1 " << key << ' ' << id_ << ' ' << frame_.id << '\n'
         << stored << ' ' << work_ << ' ' << pose_.x << ' ' << pose_.y << ' ' << pose_.z << ' ' << pose_.yaw << '\n'
         << fuelType_ << ' ' << fuel_ << ' ' << installed_.size() << '\n';
    for (std::map<std::string, std::string>::const_iterator i = installed_.begin(); i != installed_.end(); ++i)
        data << i->first << ' ' << i->second << '\n';
    data << "end\n";
    out << data.str();
    return !!out;
}
bool Vehicle::load(std::istream& input, const std::string& key, std::string& error) {
    // Limit before allocating/reading the rest of an untrusted snapshot.
    std::string bytes;
    char ch;
    while (input.get(ch)) {
        if (bytes.size() >= 32768) { error = "Snapshot exceeds size limit"; return false; }
        bytes += ch;
    }
    if (input.bad()) { error = "Snapshot read failed"; return false; }
    std::istringstream in(bytes); in.imbue(std::locale::classic());
    std::string tag, savedKey, id, frame;
    unsigned version = 0, phase = 0, type = 0, count = 0;
    double work = 0, fuel = 0;
    Pose pose;
    error = "Invalid, incompatible or mismatched vehicle snapshot";
    if (!validToken(key) || !(in >> tag >> version >> savedKey >> id >> frame) ||
        tag != "projectcars" || version != 1 || savedKey != key || id != id_ || frame != frame_.id ||
        !(in >> phase >> work >> pose.x >> pose.y >> pose.z >> pose.yaw >> type >> fuel >> count) ||
        phase > Disassembly || phase == Preview || type > Diesel || count > frame_.slots.size() ||
        !finiteNumber(work) || work < 0 || !finiteNumber(fuel) || fuel < 0) return false;
    Vehicle candidate(catalog_, frame_.id, id_);
    // Reconstruct through the same validation rules used during play.
    if (phase == Portable) {
        if (work != 0 || count != 0 || fuel != 0 || type != NoFuel ||
            pose.x != 0 || pose.y != 0 || pose.z != 0 || pose.yaw != 0) return false;
    } else {
        candidate.beginPlacement();
        if (candidate.confirmPlacement(pose, true) != Ok) return false;
        if (phase == Construction) {
            if (work >= frame_.buildWork || count || fuel != 0 || type != NoFuel) return false;
            if (work > 0 && candidate.addConstructionWork(work) != Ok) return false;
        } else if (phase == Assembled) {
            if (work != frame_.buildWork) return false;
            candidate.addConstructionWork(frame_.buildWork);
        } else {
            if (work >= frame_.disassembleWork || count || fuel != 0 || type != NoFuel) return false;
            candidate.beginDisassembly();
            if (work > 0 && candidate.addDisassemblyWork(work) != Ok) return false;
        }
    }
    for (unsigned i = 0; i < count; ++i) {
        std::string slot, part;
        if (!(in >> slot >> part) || candidate.install(slot, part) != Ok) return false;
    }
    if (fuel > 0) {
        if (candidate.refuel(static_cast<Fuel>(type), fuel) != Ok) return false;
    } else if (type != NoFuel) return false;
    std::string end, trailing;
    if (!(in >> end) || end != "end" || (in >> trailing)) return false;
    *this = candidate;
    error.clear();
    return true;
}
}
