#include "Vehicle.h"
#include <sstream>
#include <locale>
#include <cfloat>

namespace cars {
bool finiteNumber(double x) { return x == x && x <= DBL_MAX && x >= -DBL_MAX; }
std::string seatSlot(unsigned index) {
    if (index == 0) return "seat_driver";
    std::ostringstream name; name << "seat_passenger_" << index; return name.str();
}
bool validToken(const std::string& s) {
    if (s.empty() || s.size() > 128) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
    }
    return s != "-" && s != "." && s != "..";
}
static bool positive(double x, double max) { return finiteNumber(x) && x > 0 && x <= max; }
static bool validBuildingId(const std::string& id) {
    // FCS IDs include spaces (e.g. 10-Project Cars.mod). Preserve them exactly.
    if (id.empty() || id.size() > 256 || id[0] == ' ' || id[id.size()-1] == ' ') return false;
    for (size_t i = 0; i < id.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(id[i]);
        if (c < 32 || c == 127 || c == '"' || c == '#') return false;
    }
    return true;
}
const FrameDef* Catalog::frameForBuilding(const std::string& id) const {
    std::map<std::string, std::string>::const_iterator binding = buildingBindings.find(id);
    if (binding == buildingBindings.end()) return 0;
    std::map<std::string, FrameDef>::const_iterator frame = frames.find(binding->second);
    return frame == frames.end() ? 0 : &frame->second;
}
const FrameDef* Catalog::frameForCompletedBuilding(const std::string& id,
        ConstructionObservation construction) const {
    if (construction != ConstructionComplete) return 0;
    return frameForBuilding(id);
}
static Kind slotKind(const std::string& slot) {
    if (slot == "engine") return Engine;
    if (slot == "tank") return Tank;
    if (slot.compare(0, 5, "seat_") == 0) return Seat;
    if (slot == "wheel_fl" || slot == "wheel_fr" || slot == "wheel_rl" || slot == "wheel_rr") return Wheel;
    return Upgrade;
}
bool Catalog::validate(std::string& error) const {
    error.clear();
    if (frames.empty() || frames.size() > 256 || parts.empty() || parts.size() > 4096) {
        error = "Catalog must contain 1..256 frames and 1..4096 parts"; return false;
    }
    for (std::map<std::string, PartDef>::const_iterator i = parts.begin(); i != parts.end(); ++i) {
        const PartDef& p = i->second;
        if (!validToken(p.id) || i->first != p.id || p.kind < Engine || p.kind > Seat ||
            !finiteNumber(p.capacity) || !finiteNumber(p.burnRate) || p.capacity < 0 || p.burnRate < 0 ||
            (p.kind == Engine && ((p.fuel != Gasoline && p.fuel != Diesel) || !positive(p.burnRate, 10) || p.capacity != 0)) ||
            (p.kind == Tank && (p.fuel != NoFuel || !positive(p.capacity, 10000) || p.burnRate != 0)) ||
            ((p.kind == Wheel || p.kind == Upgrade || p.kind == Seat) && (p.fuel != NoFuel || p.capacity != 0 || p.burnRate != 0))) {
            error = "Invalid part: " + i->first; return false;
        }
    }
    const char* required[] = { "engine", "tank", "wheel_fl", "wheel_rl", "wheel_fr", "wheel_rr" };
    for (std::map<std::string, FrameDef>::const_iterator i = frames.begin(); i != frames.end(); ++i) {
        const FrameDef& f = i->second;
        if (!validToken(f.id) || i->first != f.id || !positive(f.buildWork, 1e7) ||
            !positive(f.disassembleWork, 1e7) || !positive(f.maxSpeed, 100) ||
            !positive(f.acceleration, 50) || !positive(f.turnRate, 5) ||
            f.seats < 1 || f.seats > 16 || f.slots.size() > 64) {
            error = "Invalid frame: " + i->first; return false;
        }
        for (unsigned r = 0; r < (f.id=="motorcycle_frame"?4u:6u); ++r) if (f.slots.find(required[r]) == f.slots.end()) {
            error = "Missing required slot: " + f.id + "/" + required[r]; return false;
        }
        for (unsigned seat = 0; seat < f.seats; ++seat) if (f.slots.find(seatSlot(seat)) == f.slots.end()) {
            error = "Missing seat mount: " + f.id + "/" + seatSlot(seat); return false;
        }
        for (std::map<std::string, std::vector<std::string> >::const_iterator s = f.slots.begin(); s != f.slots.end(); ++s) {
            if (slotKind(s->first) == Seat) {
                bool found = false;
                for (unsigned seat = 0; seat < f.seats; ++seat) if (s->first == seatSlot(seat)) found = true;
                if (!found) { error = "Unknown seat mount: " + f.id + "/" + s->first; return false; }
            }
            if (!validToken(s->first) || s->second.empty() || s->second.size() > 256) {
                error = "Invalid slot: " + f.id + "/" + s->first; return false;
            }
            std::map<std::string, bool> seen;
            for (size_t n = 0; n < s->second.size(); ++n) {
                std::map<std::string, PartDef>::const_iterator p = parts.find(s->second[n]);
                if (p == parts.end() || p->second.kind != slotKind(s->first) || seen[p->first]) {
                    error = "Invalid/duplicate compatible part: " + f.id + "/" + s->first; return false;
                }
                seen[p->first] = true;
            }
        }
    }
    if (buildingBindings.size() > 256) { error = "Too many building bindings"; return false; }
    for (std::map<std::string, std::string>::const_iterator b = buildingBindings.begin(); b != buildingBindings.end(); ++b) {
        if (!validBuildingId(b->first) || frames.find(b->second) == frames.end()) {
            error = "Invalid building binding: " + b->first; return false;
        }
    }
    return true;
}
bool Catalog::load(std::istream& input, std::string& error) {
    error.clear();
    Catalog parsed;
    std::string line;
    unsigned lineNumber = 0;
    bool version = false;
    while (std::getline(input, line)) {
        if (++lineNumber > 20000 || line.size() > 8192) { error = "Catalog size limit exceeded"; return false; }
        line = line.substr(0, line.find('#'));
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string tag, extra;
        if (!(row >> tag)) continue;
        bool good = true;
        if (!version) {
            int v = 0;
            good = tag == "catalog" && (row >> v) && v == 1;
            version = good;
        } else if (tag == "part") {
            PartDef p;
            std::string kind, fuel;
            good = !!(row >> p.id >> kind >> fuel >> p.capacity >> p.burnRate);
            p.kind = kind == "engine" ? Engine : kind == "wheel" ? Wheel : kind == "tank" ? Tank : kind == "seat" ? Seat : Upgrade;
            p.fuel = fuel == "gasoline" ? Gasoline : fuel == "diesel" ? Diesel : NoFuel;
            good = good && (kind == "engine" || kind == "wheel" || kind == "tank" || kind == "upgrade" || kind == "seat") &&
                (fuel == "gasoline" || fuel == "diesel" || fuel == "none") && parsed.parts.find(p.id) == parsed.parts.end();
            if (good) parsed.parts[p.id] = p;
        } else if (tag == "frame") {
            FrameDef f;
            good = !!(row >> f.id >> f.buildWork >> f.disassembleWork >> f.seats >> f.maxSpeed >> f.acceleration >> f.turnRate);
            good = good && parsed.frames.find(f.id) == parsed.frames.end();
            if (good) parsed.frames[f.id] = f;
        } else if (tag == "building") {
            std::string frame, id;
            char quote = 0;
            good = !!(row >> frame >> quote) && quote == '"';
            if (good) {
                std::getline(row, id, '"');
                // getline reaching EOF means the closing quote was absent.
                good = !row.fail() && !row.eof() && validBuildingId(id) &&
                    parsed.buildingBindings.find(id) == parsed.buildingBindings.end();
            }
            if (good) parsed.buildingBindings[id] = frame;
        } else if (tag == "slot") {
            std::string frame, slot, allowed;
            good = !!(row >> frame >> slot >> allowed);
            good = good && parsed.frames.find(frame) != parsed.frames.end() && parsed.frames[frame].slots.find(slot) == parsed.frames[frame].slots.end();
            if (good) {
                std::vector<std::string> ids;
                size_t start = 0, end;
                do {
                    end = allowed.find(',', start);
                    ids.push_back(allowed.substr(start, end == std::string::npos ? end : end - start));
                    start = end + 1;
                } while (end != std::string::npos);
                parsed.frames[frame].slots[slot] = ids;
            }
        } else good = false;
        if (!good || (row >> extra)) {
            std::ostringstream out; out << "Invalid catalog record at line " << lineNumber;
            error = out.str(); return false;
        }
    }
    if (input.bad() || !version || !parsed.validate(error)) {
        if (error.empty()) error = "Catalog read failed or version missing";
        return false;
    }
    *this = parsed;
    error.clear();
    return true;
}
}
