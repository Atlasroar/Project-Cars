// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "catalog.h"

#include <map>

namespace projectcars
{
    typedef std::map<std::string, std::string> InstalledParts;
    typedef std::map<std::string, unsigned int> PartCounts;

    enum VehicleFailure
    {
        VEHICLE_UNKNOWN_FRAME,
        VEHICLE_UNKNOWN_SLOT,
        VEHICLE_UNKNOWN_PART,
        VEHICLE_INCOMPATIBLE_PART,
        VEHICLE_SLOT_OCCUPIED,
        VEHICLE_SLOT_EMPTY,
        VEHICLE_SAME_PART,
        VEHICLE_INSUFFICIENT_STOCK,
        VEHICLE_INVENTORY_CAPACITY,
        VEHICLE_COUNT_OVERFLOW,
        VEHICLE_INVALID_QUANTITY,
        VEHICLE_INVALID_FUEL,
        VEHICLE_INCOMPATIBLE_FUEL,
        VEHICLE_FUEL_CAPACITY,
        VEHICLE_INSUFFICIENT_FUEL,
        VEHICLE_ENGINE_REQUIRED,
        VEHICLE_TANK_REQUIRED,
        VEHICLE_NUMERIC_RANGE
    };

    class VehicleError : public std::runtime_error
    {
    public:
        VehicleError(VehicleFailure failure, const std::string& detail);
        VehicleFailure Failure() const;

    private:
        VehicleFailure failure_;
    };

    struct FuelAmount
    {
        FuelAmount(Fuel type = FUEL_NONE, double litres = 0);
        Fuel type;
        double litres;
    };

    struct FuelConsumption
    {
        double consumedLitres;
        double usableSimSeconds;
        bool exhausted;
    };

    struct MechanicalReadiness
    {
        std::vector<std::string> missingSlots;
        std::vector<std::string> unsupportedSlots;
        bool Ready() const;
    };

    class PartStock
    {
    public:
        PartStock(const Catalog& catalog, unsigned int capacity,
                  const PartCounts& counts = PartCounts());
        PartStock& operator=(const PartStock& other);

        unsigned int Capacity() const;
        unsigned int Total() const;
        unsigned int Count(const std::string& partId) const;
        const PartCounts& Counts() const;

    private:
        friend class Vehicle;
        void Swap(PartStock& other);

        unsigned int capacity_;
        unsigned int total_;
        PartCounts counts_;
    };

    class Vehicle
    {
    public:
        Vehicle(const Catalog& catalog, const std::string& frameId);
        Vehicle& operator=(const Vehicle& other);

        const std::string& FrameId() const;
        const InstalledParts& Installed() const;
        FuelAmount FuelState() const;
        double TankCapacityLitres() const;
        MechanicalReadiness Readiness() const;

        void Fit(const std::string& slotId, const std::string& partId, PartStock& stock);
        void Remove(const std::string& slotId, PartStock& stock);
        void Replace(const std::string& slotId, const std::string& partId, PartStock& stock);
        double Refuel(Fuel type, double litres);
        FuelAmount Drain(double litres);
        FuelConsumption Consume(double simulationSeconds);

    private:
        friend Vehicle ParseVehicle(std::istream& input, const std::string& source,
                                    const Catalog& catalog);
        enum ExchangeMode { FIT, REMOVE, REPLACE };

        void Swap(Vehicle& other);
        void ValidateInstallation(const std::string& slotId, const std::string& partId) const;
        void ValidateFuel(const InstalledParts& installed, const FuelAmount& fuel) const;
        const Part* InstalledRole(const InstalledParts& installed, const char* slotId,
                                  PartKind kind) const;
        void Exchange(const std::string& slotId, const std::string& partId,
                      PartStock& stock, ExchangeMode mode);

        Catalog catalog_;
        std::string frameId_;
        InstalledParts installed_;
        FuelAmount fuel_;
    };
}
