// SPDX-License-Identifier: GPL-3.0-only
#include "vehicle.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace projectcars
{
    namespace
    {
        __declspec(noreturn) void Fail(VehicleFailure failure, const std::string& detail)
        {
            throw VehicleError(failure, detail);
        }

        void Quantity(double value, const char* field)
        {
            if (!text::Finite(value) || value < 0)
                Fail(VEHICLE_INVALID_QUANTITY, std::string(field) + " must be finite and nonnegative");
        }

        unsigned int ValidateCounts(const Catalog& catalog, unsigned int capacity,
                                    const PartCounts& counts)
        {
            unsigned int total = 0;
            const unsigned int limit = (std::numeric_limits<unsigned int>::max)();
            for (PartCounts::const_iterator it = counts.begin(); it != counts.end(); ++it)
            {
                if (!catalog.FindPart(it->first))
                    Fail(VEHICLE_UNKNOWN_PART, "stock references unknown part '" + it->first + "'");
                if (it->second > limit - total)
                    Fail(VEHICLE_COUNT_OVERFLOW, "total stock count exceeds the 32-bit unsigned range");
                total += it->second;
            }
            if (total > capacity)
                Fail(VEHICLE_INVENTORY_CAPACITY, "standalone stock count exceeds its part-count capacity");
            return total;
        }

        bool RoleSupported(const Catalog& catalog, const Slot* slot, PartKind kind)
        {
            if (!slot)
                return false;
            for (size_t i = 0; i < slot->compatiblePartIds.size(); ++i)
                if (catalog.FindPart(slot->compatiblePartIds[i])->kind != kind)
                    return false;
            return true;
        }
    }

    VehicleError::VehicleError(VehicleFailure failure, const std::string& detail)
        : std::runtime_error("vehicle: " + detail), failure_(failure) {}

    VehicleFailure VehicleError::Failure() const { return failure_; }

    FuelAmount::FuelAmount(Fuel fuelType, double volume) : type(fuelType), litres(volume) {}

    bool MechanicalReadiness::Ready() const
    {
        return missingSlots.empty() && unsupportedSlots.empty();
    }

    PartStock::PartStock(const Catalog& catalog, unsigned int capacity, const PartCounts& counts)
        : capacity_(capacity), total_(0), counts_(counts)
    {
        ValidateCatalog(catalog);
        total_ = ValidateCounts(catalog, capacity_, counts_);
        for (PartCounts::iterator it = counts_.begin(); it != counts_.end();)
        {
            if (it->second == 0)
                counts_.erase(it++);
            else
                ++it;
        }
    }

    PartStock& PartStock::operator=(const PartStock& other)
    {
        if (this != &other)
        {
            PartStock copy(other);
            Swap(copy);
        }
        return *this;
    }

    void PartStock::Swap(PartStock& other)
    {
        std::swap(capacity_, other.capacity_);
        std::swap(total_, other.total_);
        counts_.swap(other.counts_);
    }

    unsigned int PartStock::Capacity() const { return capacity_; }
    unsigned int PartStock::Total() const { return total_; }
    const PartCounts& PartStock::Counts() const { return counts_; }

    unsigned int PartStock::Count(const std::string& partId) const
    {
        const PartCounts::const_iterator it = counts_.find(partId);
        return it == counts_.end() ? 0 : it->second;
    }

    Vehicle::Vehicle(const Catalog& catalog, const std::string& frameId)
        : catalog_(catalog), frameId_(frameId)
    {
        ValidateCatalog(catalog_);
        if (!catalog_.FindFrame(frameId_))
            Fail(VEHICLE_UNKNOWN_FRAME, "unknown frame '" + frameId_ + "'");
    }

    Vehicle& Vehicle::operator=(const Vehicle& other)
    {
        if (this != &other)
        {
            Vehicle copy(other);
            Swap(copy);
        }
        return *this;
    }

    void Vehicle::Swap(Vehicle& other)
    {
        std::swap(catalog_.version, other.catalog_.version);
        catalog_.parts.swap(other.catalog_.parts);
        catalog_.frames.swap(other.catalog_.frames);
        catalog_.slots.swap(other.catalog_.slots);
        catalog_.buildings.swap(other.catalog_.buildings);
        frameId_.swap(other.frameId_);
        installed_.swap(other.installed_);
        std::swap(fuel_.type, other.fuel_.type);
        std::swap(fuel_.litres, other.fuel_.litres);
    }

    const std::string& Vehicle::FrameId() const { return frameId_; }
    const InstalledParts& Vehicle::Installed() const { return installed_; }
    FuelAmount Vehicle::FuelState() const { return fuel_; }

    const Part* Vehicle::InstalledRole(const InstalledParts& installed, const char* slotId,
                                       PartKind kind) const
    {
        const InstalledParts::const_iterator it = installed.find(slotId);
        if (it == installed.end())
            return NULL;
        const Part* part = catalog_.FindPart(it->second);
        return part->kind == kind ? part : NULL;
    }

    double Vehicle::TankCapacityLitres() const
    {
        const Part* tank = InstalledRole(installed_, "tank", PART_TANK);
        return tank ? tank->capacityLitres : 0;
    }

    MechanicalReadiness Vehicle::Readiness() const
    {
        struct Role
        {
            const char* slot;
            PartKind kind;
        };
        const Role roles[] = {
            {"seat_driver", PART_SEAT}, {"engine", PART_ENGINE}, {"tank", PART_TANK},
            {"wheel_fl", PART_WHEEL}, {"wheel_fr", PART_WHEEL},
            {"wheel_rl", PART_WHEEL}, {"wheel_rr", PART_WHEEL}
        };
        MechanicalReadiness result;
        for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); ++i)
        {
            const Slot* slot = catalog_.FindSlot(frameId_, roles[i].slot);
            if (!RoleSupported(catalog_, slot, roles[i].kind))
                result.unsupportedSlots.push_back(roles[i].slot);
            else if (installed_.find(roles[i].slot) == installed_.end())
                result.missingSlots.push_back(roles[i].slot);
        }
        return result;
    }

    void Vehicle::ValidateInstallation(const std::string& slotId, const std::string& partId) const
    {
        const Slot* slot = catalog_.FindSlot(frameId_, slotId);
        if (!slot)
            Fail(VEHICLE_UNKNOWN_SLOT, "unknown slot '" + frameId_ + "/" + slotId + "'");
        if (!catalog_.FindPart(partId))
            Fail(VEHICLE_UNKNOWN_PART, "unknown part '" + partId + "'");
        if (std::find(slot->compatiblePartIds.begin(), slot->compatiblePartIds.end(), partId) ==
            slot->compatiblePartIds.end())
            Fail(VEHICLE_INCOMPATIBLE_PART, "part '" + partId + "' is incompatible with slot '" +
                 frameId_ + "/" + slotId + "'");
    }

    void Vehicle::ValidateFuel(const InstalledParts& installed, const FuelAmount& fuel) const
    {
        Quantity(fuel.litres, "fuel litres");
        if (fuel.type != FUEL_NONE && fuel.type != FUEL_GASOLINE && fuel.type != FUEL_DIESEL)
            Fail(VEHICLE_INVALID_FUEL, "unsupported fuel type");
        if ((fuel.type == FUEL_NONE) != (fuel.litres == 0))
            Fail(VEHICLE_INVALID_FUEL, "empty fuel must be none; positive litres require gasoline/diesel");
        const Part* tank = InstalledRole(installed, "tank", PART_TANK);
        if (fuel.litres > 0 && !tank)
            Fail(VEHICLE_TANK_REQUIRED, "fuel requires an installed tank; drain explicitly before removal");
        if (tank && fuel.litres > tank->capacityLitres)
            Fail(VEHICLE_FUEL_CAPACITY, "fuel exceeds installed tank capacity; drain explicitly before downsizing");
        const Part* engine = InstalledRole(installed, "engine", PART_ENGINE);
        if (fuel.litres > 0 && engine && engine->fuel != fuel.type)
            Fail(VEHICLE_INCOMPATIBLE_FUEL, "stored fuel is incompatible with the installed engine");
    }

    void Vehicle::Fit(const std::string& slotId, const std::string& partId, PartStock& stock)
    {
        Exchange(slotId, partId, stock, FIT);
    }

    void Vehicle::Remove(const std::string& slotId, PartStock& stock)
    {
        Exchange(slotId, "", stock, REMOVE);
    }

    void Vehicle::Replace(const std::string& slotId, const std::string& partId, PartStock& stock)
    {
        Exchange(slotId, partId, stock, REPLACE);
    }

    void Vehicle::Exchange(const std::string& slotId, const std::string& partId,
                           PartStock& stock, ExchangeMode mode)
    {
        if (!catalog_.FindSlot(frameId_, slotId))
            Fail(VEHICLE_UNKNOWN_SLOT, "unknown slot '" + frameId_ + "/" + slotId + "'");
        const InstalledParts::const_iterator old = installed_.find(slotId);
        if (mode == FIT && old != installed_.end())
            Fail(VEHICLE_SLOT_OCCUPIED, "slot '" + slotId + "' is occupied; use Replace");
        if (mode != FIT && old == installed_.end())
            Fail(VEHICLE_SLOT_EMPTY, "slot '" + slotId + "' is empty");
        if (mode != REMOVE)
        {
            ValidateInstallation(slotId, partId);
            if (old != installed_.end() && old->second == partId)
                Fail(VEHICLE_SAME_PART, "replacement is the already installed part");
        }
        ValidateCounts(catalog_, stock.capacity_, stock.counts_);

        InstalledParts nextInstalled(installed_);
        if (mode == REMOVE)
            nextInstalled.erase(slotId);
        else
            nextInstalled[slotId] = partId;
        ValidateFuel(nextInstalled, fuel_);

        PartCounts nextCounts(stock.counts_);
        if (mode != REMOVE)
        {
            PartCounts::iterator incoming = nextCounts.find(partId);
            if (incoming == nextCounts.end() || incoming->second == 0)
                Fail(VEHICLE_INSUFFICIENT_STOCK, "no stock of part '" + partId + "'");
            if (--incoming->second == 0)
                nextCounts.erase(incoming);
        }
        if (old != installed_.end())
        {
            unsigned int& returned = nextCounts[old->second];
            if (returned == (std::numeric_limits<unsigned int>::max)())
                Fail(VEHICLE_COUNT_OVERFLOW, "returned part '" + old->second + "' would overflow its count");
            ++returned;
        }
        const unsigned int nextTotal = ValidateCounts(catalog_, stock.capacity_, nextCounts);

        // Both maps are staged; default-allocator swaps cannot allocate during this commit.
        installed_.swap(nextInstalled);
        stock.counts_.swap(nextCounts);
        stock.total_ = nextTotal;
    }

    double Vehicle::Refuel(Fuel type, double litres)
    {
        Quantity(litres, "refuel litres");
        if (type != FUEL_GASOLINE && type != FUEL_DIESEL)
            Fail(VEHICLE_INVALID_FUEL, "refueling requires gasoline or diesel");
        const Part* tank = InstalledRole(installed_, "tank", PART_TANK);
        if (!tank)
            Fail(VEHICLE_TANK_REQUIRED, "refueling requires an installed tank");
        const Part* engine = InstalledRole(installed_, "engine", PART_ENGINE);
        if ((fuel_.litres > 0 && fuel_.type != type) || (engine && engine->fuel != type))
            Fail(VEHICLE_INCOMPATIBLE_FUEL, "refueling would mix fuel or conflict with the installed engine");
        if (litres > tank->capacityLitres - fuel_.litres)
            Fail(VEHICLE_FUEL_CAPACITY, "refuel litres exceed available tank capacity");
        const double next = fuel_.litres + litres;
        if (!text::Finite(next) || next > tank->capacityLitres)
            Fail(VEHICLE_FUEL_CAPACITY, "refuel litres exceed installed tank capacity");
        const double actual = next - fuel_.litres;
        if (litres > 0 && actual == 0)
            Fail(VEHICLE_NUMERIC_RANGE, "refuel quantity is too small to change stored double-precision litres");
        if (litres > 0)
            fuel_ = FuelAmount(type, next);
        return actual;
    }

    FuelAmount Vehicle::Drain(double litres)
    {
        Quantity(litres, "drain litres");
        if (litres > fuel_.litres)
            Fail(VEHICLE_INSUFFICIENT_FUEL, "drain litres exceed stored fuel");
        const double next = fuel_.litres - litres;
        const double actual = fuel_.litres - next;
        if (litres > 0 && actual == 0)
            Fail(VEHICLE_NUMERIC_RANGE, "drain quantity is too small to change stored double-precision litres");
        const FuelAmount removed(actual > 0 ? fuel_.type : FUEL_NONE, actual);
        fuel_ = next > 0 ? FuelAmount(fuel_.type, next) : FuelAmount();
        return removed;
    }

    FuelConsumption Vehicle::Consume(double simulationSeconds)
    {
        Quantity(simulationSeconds, "simulation seconds");
        const Part* engine = InstalledRole(installed_, "engine", PART_ENGINE);
        if (!engine)
            Fail(VEHICLE_ENGINE_REQUIRED, "fuel consumption requires an installed engine");
        FuelConsumption result;
        result.consumedLitres = 0;
        result.usableSimSeconds = 0;
        result.exhausted = fuel_.litres == 0;
        if (simulationSeconds == 0 || fuel_.litres == 0)
            return result;

        const double availableSeconds = fuel_.litres / engine->burnLitresPerSimSecond;
        if (availableSeconds == 0)
            Fail(VEHICLE_NUMERIC_RANGE, "positive fuel has unrepresentable usable simulation time");
        const double requested = simulationSeconds * engine->burnLitresPerSimSecond;
        if (simulationSeconds >= availableSeconds || !text::Finite(requested) ||
            requested >= fuel_.litres)
        {
            result.consumedLitres = fuel_.litres;
            result.usableSimSeconds = (std::min)(simulationSeconds, availableSeconds);
            result.exhausted = true;
            fuel_ = FuelAmount();
            return result;
        }

        const double next = fuel_.litres - requested;
        result.consumedLitres = fuel_.litres - next;
        if (requested == 0 || result.consumedLitres == 0)
            Fail(VEHICLE_NUMERIC_RANGE, "simulation step is too small to change stored double-precision litres");
        result.usableSimSeconds = simulationSeconds;
        result.exhausted = next == 0;
        fuel_ = next > 0 ? FuelAmount(fuel_.type, next) : FuelAmount();
        return result;
    }
}
