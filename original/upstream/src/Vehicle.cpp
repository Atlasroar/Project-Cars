#include "Vehicle.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cars {
const char* message(Code c) {
    static const char* names[] = {
        "OK", "Invalid value", "Action unavailable in this construction phase",
        "Choose valid ground", "Vacate the seats first", "Stop the car first",
        "This frame has no such slot", "This part does not fit this slot",
        "Remove the installed part first", "This slot is empty", "Wrong fuel for the engine or tank contents",
        "Install a gas tank", "Not enough tank capacity", "Drain the tank first",
        "Remove all parts and upgrades before disassembly", "Install an engine",
        "Install all four wheels", "Add fuel", "A seated driver is required",
        "This placement action has expired", "World operation did not complete", "Install a seat"
    };
    return c >= Ok && c <= SeatMissing ? names[c] : "Unknown error";
}
static bool validPose(const Pose& p) {
    return finiteNumber(p.x) && finiteNumber(p.y) && finiteNumber(p.z) && finiteNumber(p.yaw) &&
        std::fabs(p.x) <= 1e7 && std::fabs(p.y) <= 1e7 && std::fabs(p.z) <= 1e7 && std::fabs(p.yaw) <= 1e7;
}
Vehicle::Vehicle(const Catalog& catalog, const std::string& frameId, const std::string& id)
    : catalog_(catalog), id_(id), phase_(Portable), work_(0), fuel_(0), speed_(0), fuelType_(NoFuel) {
    std::string error;
    if (!catalog_.validate(error) || !validToken(id) || catalog_.frames.find(frameId) == catalog_.frames.end())
        throw std::invalid_argument("Invalid vehicle catalog, frame or identity: " + error);
    frame_ = catalog_.frames.find(frameId)->second;
    occupants_.resize(frame_.seats);
}
bool Vehicle::hasOccupants() const {
    for (size_t i = 0; i < occupants_.size(); ++i) if (!occupants_[i].empty()) return true;
    return false;
}
Code Vehicle::adoptCompletedBuilding(const std::string& buildingId,
        ConstructionObservation construction, const Pose& pose) {
    if (phase_ != Portable || construction != ConstructionComplete) return WrongPhase;
    const FrameDef* definition = catalog_.frameForCompletedBuilding(buildingId, construction);
    if (!definition || definition->id != frame_.id || !validPose(pose)) return InvalidArgument;
    // Completion is engine-authoritative. No synthetic build work or materials
    // are charged: Kenshi has already finished those. No engine parts are added.
    pose_ = pose;
    work_ = frame_.buildWork;
    phase_ = Assembled;
    return Ok;
}
Code Vehicle::serviceCheck() const {
    if (phase_ != Assembled) return WrongPhase;
    if (speed_ != 0) return Moving;
    if (hasOccupants()) return Occupied;
    return Ok;
}
const PartDef* Vehicle::partAt(const std::string& slot) const {
    std::map<std::string, std::string>::const_iterator i = installed_.find(slot);
    return i == installed_.end() ? 0 : &catalog_.parts.find(i->second)->second;
}
Code Vehicle::beginPlacement() {
    if (phase_ != Portable) return WrongPhase;
    phase_ = Preview; return Ok;
}
Code Vehicle::cancelPlacement() {
    if (phase_ != Preview) return WrongPhase;
    phase_ = Portable; return Ok;
}
Code Vehicle::confirmPlacement(const Pose& p, bool groundValidated) {
    if (phase_ != Preview) return WrongPhase;
    if (!validPose(p)) return InvalidArgument;
    if (!groundValidated) return InvalidGround;
    pose_ = p; phase_ = Construction; work_ = 0; return Ok;
}
Code Vehicle::addConstructionWork(double amount) {
    if (phase_ != Construction) return WrongPhase;
    if (!finiteNumber(amount) || amount <= 0 || amount > 1e7) return InvalidArgument;
    work_ = std::min(frame_.buildWork, work_ + amount);
    if (work_ >= frame_.buildWork) phase_ = Assembled;
    return Ok;
}
Code Vehicle::install(const std::string& slot, const std::string& part) {
    Code check = serviceCheck(); if (check != Ok) return check;
    std::map<std::string, std::vector<std::string> >::const_iterator s = frame_.slots.find(slot);
    if (s == frame_.slots.end()) return UnknownSlot;
    if (partAt(slot)) return SlotFull;
    if (std::find(s->second.begin(), s->second.end(), part) == s->second.end()) return IncompatiblePart;
    const PartDef& p = catalog_.parts.find(part)->second;
    if (p.kind == Engine && fuel_ > 0 && p.fuel != fuelType_) return FuelMismatch;
    installed_[slot] = part; return Ok;
}
Code Vehicle::remove(const std::string& slot) {
    Code check = serviceCheck(); if (check != Ok) return check;
    if (frame_.slots.find(slot) == frame_.slots.end()) return UnknownSlot;
    if (!partAt(slot)) return SlotEmpty;
    if (slot == "tank" && fuel_ > 0) return DrainFirst;
    installed_.erase(slot); return Ok;
}
Code Vehicle::refuel(Fuel type, double amount) {
    Code check = serviceCheck(); if (check != Ok) return check;
    if ((type != Gasoline && type != Diesel) || !finiteNumber(amount) || amount <= 0) return InvalidArgument;
    const PartDef* tank = partAt("tank");
    if (!tank) return TankMissing;
    const PartDef* engine = partAt("engine");
    if ((engine && engine->fuel != type) || (fuel_ > 0 && fuelType_ != type)) return FuelMismatch;
    if (amount > tank->capacity - fuel_) return TankFull;
    fuel_ += amount; fuelType_ = type; return Ok;
}
Code Vehicle::drain(double amount) {
    Code check = serviceCheck(); if (check != Ok) return check;
    if (!finiteNumber(amount) || amount <= 0 || amount > fuel_) return InvalidArgument;
    fuel_ -= amount;
    if (fuel_ == 0) fuelType_ = NoFuel;
    return Ok;
}
bool Vehicle::seatInstalled(unsigned seat) const {
    return seat < occupants_.size() && partAt(seatSlot(seat)) != 0;
}
InteractionChoice Vehicle::defaultInteraction(const std::string& actor) const {
    if (phase_ != Assembled || speed_ != 0 || !validToken(actor)) return InteractionChoice();
    // An already seated actor should not acquire a second seat.
    if (std::find(occupants_.begin(), occupants_.end(), actor) == occupants_.end()) {
        for (unsigned i = 0; i < occupants_.size(); ++i)
            if (seatInstalled(i) && occupants_[i].empty()) return InteractionChoice(SitInteraction, i);
    }
    return InteractionChoice(ModifyInteraction);
}
Code Vehicle::interact(const std::string& actor, VehicleInteractionPort& port, bool explicitModify) const {
    if (!validToken(actor)) return InvalidArgument;
    if (phase_ != Assembled) return WrongPhase;
    if (speed_ != 0) return Moving;
    // Re-evaluate at click time; never execute an old menu's seat index blindly.
    InteractionChoice choice = explicitModify ? InteractionChoice(ModifyInteraction) : defaultInteraction(actor);
    if (choice.action == SitInteraction)
        return port.requestSit(id_, actor, static_cast<unsigned>(choice.seat)) ? Ok : WorldRejected;
    return port.openComponents(*this, actor) ? Ok : WorldRejected;
}
Code Vehicle::sit(unsigned seat, const std::string& actor) {
    if (phase_ != Assembled) return WrongPhase;
    if (!validToken(actor) || seat >= occupants_.size()) return InvalidArgument;
    if (speed_ != 0) return Moving;
    if (!seatInstalled(seat)) return SeatMissing;
    if (!occupants_[seat].empty() || std::find(occupants_.begin(), occupants_.end(), actor) != occupants_.end()) return Occupied;
    occupants_[seat] = actor; return Ok;
}
Code Vehicle::leave(const std::string& actor) {
    if (speed_ != 0) return Moving;
    for (size_t i = 0; i < occupants_.size(); ++i) if (!actor.empty() && occupants_[i] == actor) {
        occupants_[i].clear(); return Ok;
    }
    return InvalidArgument;
}
Code Vehicle::readiness() const {
    if (phase_ != Assembled) return WrongPhase;
    if (!partAt("engine")) return MissingEngine;
    if (!partAt("wheel_fl") || !partAt("wheel_rl") ||
        (frame_.id!="motorcycle_frame" && (!partAt("wheel_fr") || !partAt("wheel_rr")))) return MissingWheels;
    if (!partAt("tank")) return TankMissing;
    if (fuel_ <= 0) return EmptyTank;
    if (partAt("engine")->fuel != fuelType_) return FuelMismatch;
    if (!seatInstalled(0)) return SeatMissing;
    return Ok;
}
Code Vehicle::beginDisassembly() {
    if (phase_ != Construction && phase_ != Assembled) return WrongPhase;
    if (speed_ != 0) return Moving;
    if (hasOccupants()) return Occupied;
    if (fuel_ > 0) return DrainFirst;
    if (!installed_.empty()) return StripFirst;
    phase_ = Disassembly; work_ = 0; return Ok;
}
Code Vehicle::addDisassemblyWork(double amount) {
    if (phase_ != Disassembly) return WrongPhase;
    if (!finiteNumber(amount) || amount <= 0 || amount > 1e7) return InvalidArgument;
    work_ = std::min(frame_.disassembleWork, work_ + amount);
    if (work_ >= frame_.disassembleWork) { phase_ = Portable; work_ = 0; pose_ = Pose(); }
    return Ok;
}
static double approach(double value, double target, double delta) {
    if (value < target) return std::min(value + delta, target);
    return std::max(value - delta, target);
}
Code Vehicle::drive(const std::string& actor, double throttle, double steering,
                    bool brake, double seconds, MotionWorld& world) {
    if (!finiteNumber(throttle) || !finiteNumber(steering) || !finiteNumber(seconds) ||
        std::fabs(throttle) > 1 || std::fabs(steering) > 1 || seconds < 0 || seconds > 0.25) return InvalidArgument;
    if (actor.empty() || occupants_[0] != actor) return DriverRequired;
    Code ready = readiness();
    if (ready != Ok) { speed_ = 0; return ready; }
    if (seconds == 0) return Ok;
    // No fuel consumed while stationary with the engine effectively off.
    const double rate = partAt("engine")->burnRate * (0.15 + 0.85 * std::fabs(throttle));
    const bool running = throttle != 0 || speed_ != 0;
    const double dt = running ? std::min(seconds, fuel_ / rate) : seconds;
    const double target = throttle * frame_.maxSpeed * (throttle < 0 ? 0.4 : 1.0);
    double nextSpeed = approach(speed_, brake ? 0 : target, frame_.acceleration * dt * (brake ? 3 : 1));
    Pose proposed = pose_;
    // Prototype kinematics; native collision, suspension and rider attachment
    // remain separate engine integration work. Never directly move a Building.
    proposed.yaw += steering * frame_.turnRate * dt * (nextSpeed / frame_.maxSpeed);
    proposed.yaw = std::fmod(proposed.yaw, 6.283185307179586);
    proposed.x += std::sin(proposed.yaw) * nextSpeed * dt;
    proposed.z += std::cos(proposed.yaw) * nextSpeed * dt;
    const bool moves = proposed.x != pose_.x || proposed.z != pose_.z || proposed.yaw != pose_.yaw;
    if (moves && (!validPose(proposed) || !world.canMove(pose_, proposed))) nextSpeed = 0;
    else pose_ = proposed;
    if (running) {
        fuel_ = std::max(0.0, fuel_ - rate * dt);
        if (dt < seconds || fuel_ < 1e-12) { fuel_ = 0; fuelType_ = NoFuel; nextSpeed = 0; }
    }
    speed_ = nextSpeed;
    return Ok;
}
Code Placement::begin(Vehicle& v, const std::string& actor, const std::string& item,
                      unsigned long epoch, PlacementPort& port, unsigned long& ticket) {
    if (active_) return WrongPhase;
    if (!validToken(actor) || !validToken(item) || epoch == 0) return InvalidArgument;
    if (!port.eligible(actor, item, v.frame().id)) return WorldRejected;
    Code c = v.beginPlacement(); if (c != Ok) return c;
    actor_ = actor; item_ = item; vehicle_ = v.id(); epoch_ = epoch;
    if (++next_ == 0) ++next_;
    ticket_ = next_; ticket = ticket_; active_ = true;
    return Ok;
}
Code Placement::confirm(Vehicle& v, unsigned long ticket, unsigned long epoch,
                        const Pose& pose, PlacementPort& port) {
    if (!active_ || ticket != ticket_ || epoch != epoch_ || v.id() != vehicle_ || v.phase() != Preview) return StaleAction;
    if (!validPose(pose)) return InvalidArgument;
    if (!port.eligible(actor_, item_, v.frame().id)) { cancel(v, port); return WorldRejected; }
    if (!port.groundAllowed(v.frame(), pose)) return InvalidGround;
    Vehicle proposed(v);
    Code c = proposed.confirmPlacement(pose, true); if (c != Ok) return c;
    if (!port.commitSiteAndConsumeKit(actor_, item_, proposed)) return WorldRejected;
    // The post-ACK transition touches only primitive fields; no allocating copy
    // may fail after the engine has consumed the kit.
    v.confirmPlacement(pose, true); active_ = false; port.clearPreview(); return Ok;
}
void Placement::cancel(Vehicle& v, PlacementPort& port) {
    if (!active_ || v.id() != vehicle_) return;
    if (v.phase() == Preview) v.cancelPlacement();
    active_ = false; port.clearPreview();
}
}
