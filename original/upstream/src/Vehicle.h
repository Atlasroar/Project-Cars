#pragma once
#include <map>
#include <string>
#include <vector>
#include <iosfwd>

// Engine-independent C++03. All quantities use metres, seconds and litres.
namespace cars {
enum Fuel { NoFuel, Gasoline, Diesel };
enum Kind { Engine, Wheel, Tank, Upgrade, Seat };
enum Phase { Portable, Preview, Construction, Assembled, Disassembly };
// Supplied by a validated native observation, not a timer or guessed percentage.
enum ConstructionObservation { ConstructionUnknown, ConstructionUnfinished, ConstructionComplete };
enum Code {
    Ok, InvalidArgument, WrongPhase, InvalidGround, Occupied, Moving,
    UnknownSlot, IncompatiblePart, SlotFull, SlotEmpty, FuelMismatch,
    TankMissing, TankFull, DrainFirst, StripFirst, MissingEngine,
    MissingWheels, EmptyTank, DriverRequired, StaleAction, WorldRejected, SeatMissing
};
enum InteractionAction { NoInteraction, SitInteraction, ModifyInteraction };
struct InteractionChoice {
    InteractionAction action;
    int seat;
    InteractionChoice(InteractionAction a = NoInteraction, int s = -1) : action(a), seat(s) {}
};
std::string seatSlot(unsigned index);
class Vehicle;
// Native adapter owns GUI, movement/range checks, authorization and pending seat
// reservations. A sit request is not proof the character has reached the seat.
struct VehicleInteractionPort {
    virtual ~VehicleInteractionPort() {}
    virtual bool requestSit(const std::string& vehicleId, const std::string& actorId, unsigned seat) = 0;
    virtual bool openComponents(const Vehicle& vehicle, const std::string& actorId) = 0;
};
const char* message(Code code);
struct Pose {
    double x, y, z, yaw;
    Pose() : x(0), y(0), z(0), yaw(0) {}
};
struct PartDef {
    std::string id;
    Kind kind;
    Fuel fuel;
    double capacity, burnRate;
};
struct FrameDef {
    std::string id;
    double buildWork, disassembleWork, maxSpeed, acceleration, turnRate;
    unsigned seats;
    std::map<std::string, std::vector<std::string> > slots;
};
struct Catalog {
    std::map<std::string, FrameDef> frames;
    std::map<std::string, PartDef> parts;
    // Exact FCS building StringID -> mod-owned frame ID. Not display names.
    std::map<std::string, std::string> buildingBindings;
    const FrameDef* frameForBuilding(const std::string& stringId) const;
    const FrameDef* frameForCompletedBuilding(const std::string& stringId,
        ConstructionObservation construction) const;
    // Transactional parse: failure leaves this catalog untouched.
    bool load(std::istream& input, std::string& error);
    bool validate(std::string& error) const;
};
bool validToken(const std::string& value);
bool finiteNumber(double value);

// The engine adapter must perform a swept collision/terrain query, including
// the vehicle footprint. False means fail closed. No default pass-through.
struct MotionWorld {
    virtual ~MotionWorld() {}
    virtual bool canMove(const Pose& from, const Pose& to) = 0;
};

class Vehicle {
public:
    Vehicle(const Catalog& catalog, const std::string& frameId,
            const std::string& persistentId);
    const std::string& id() const { return id_; }
    const FrameDef& frame() const { return frame_; }
    Phase phase() const { return phase_; }
    const Pose& pose() const { return pose_; }
    double progress() const { return work_; }
    double fuelLitres() const { return fuel_; }
    Fuel fuelType() const { return fuelType_; }
    double speed() const { return speed_; }
    const std::map<std::string, std::string>& installed() const { return installed_; }
    const std::vector<std::string>& occupants() const { return occupants_; }
    bool canPickup() const { return phase_ == Portable; }
    // Native build-menu adoption path: no vehicle exists during construction.
    // Caller registers the instance only after success and enforces unique IDs.
    Code adoptCompletedBuilding(const std::string& buildingStringId,
        ConstructionObservation construction, const Pose& pose);
    Code beginPlacement();
    Code cancelPlacement();
    Code confirmPlacement(const Pose& pose, bool groundValidated);
    Code addConstructionWork(double work);
    Code install(const std::string& slot, const std::string& part);
    Code remove(const std::string& slot);
    // Exact amounts: overfill fails without consuming an inventory item.
    Code refuel(Fuel type, double litres);
    Code drain(double litres);
    Code sit(unsigned seat, const std::string& actorId);
    bool seatInstalled(unsigned seat) const;
    InteractionChoice defaultInteraction(const std::string& actorId) const;
    Code interact(const std::string& actorId, VehicleInteractionPort& port, bool explicitModify = false) const;
    Code leave(const std::string& actorId);
    Code readiness() const;
    Code beginDisassembly();
    Code addDisassemblyWork(double work);
    // Frame time must be game simulation time, not wall-clock time. A native
    // caller substeps at <= 0.25 s and imposes an overall per-frame work budget.
    Code drive(const std::string& driverId, double throttle, double steering,
               bool brake, double seconds, MotionWorld& world);
    void stop() { speed_ = 0; }
    // Save identity is an opaque, validated playthrough+snapshot key, not a name.
    // Serialization excludes occupants, speed and placement previews.
    bool save(std::ostream& output, const std::string& saveKey) const;
    bool load(std::istream& input, const std::string& expectedSaveKey, std::string& error);
private:
    Catalog catalog_;
    FrameDef frame_;
    std::string id_;
    Phase phase_;
    Pose pose_;
    double work_, fuel_, speed_;
    Fuel fuelType_;
    std::map<std::string, std::string> installed_;
    std::vector<std::string> occupants_;
    bool hasOccupants() const;
    Code serviceCheck() const;
    const PartDef* partAt(const std::string& slot) const;
};

// Placement controller routes the future inventory Build action to a ground
// preview. The port returns true ONLY after observing both the item debit and
// construction-site creation. An unsuccessful operation must be rolled back or
// reconciled by the port before another attempt; queued engine calls are not ACKs.
struct PlacementPort {
    virtual ~PlacementPort() {}
    virtual bool eligible(const std::string& actor, const std::string& item,
                          const std::string& frame) = 0;
    virtual bool groundAllowed(const FrameDef& frame, const Pose& pose) = 0;
    virtual bool commitSiteAndConsumeKit(const std::string& actor,
        const std::string& item, const Vehicle& constructionSite) = 0;
    virtual void clearPreview() = 0;
};
class Placement {
public:
    Placement() : active_(false), next_(0), ticket_(0), epoch_(0) {}
    Code begin(Vehicle& vehicle, const std::string& actor, const std::string& item,
               unsigned long epoch, PlacementPort& port, unsigned long& ticket);
    Code confirm(Vehicle& vehicle, unsigned long ticket, unsigned long epoch,
                 const Pose& pose, PlacementPort& port);
    void cancel(Vehicle& vehicle, PlacementPort& port);
    bool active() const { return active_; }
private:
    bool active_;
    unsigned long next_, ticket_, epoch_;
    std::string actor_, item_, vehicle_;
};
}
