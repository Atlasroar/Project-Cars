// SPDX-License-Identifier: GPL-3.0-only
#include "vehicle_io.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>

namespace
{
    using namespace projectcars;

    unsigned int assertions = 0;
    unsigned int rejectedTransactions = 0;
    unsigned int rejectedSaves = 0;
    unsigned int roundTrips = 0;

    void Check(bool condition, const char* expression, int line)
    {
        ++assertions;
        if (!condition)
        {
            std::ostringstream message;
            message << "vehicle test line " << line << ": " << expression;
            throw std::runtime_error(message.str());
        }
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)
#define EXPECT_FAILURE(expected, expression) \
    { \
        bool rejected = false; \
        try { expression; } \
        catch (const VehicleError& error) { \
            rejected = true; \
            CHECK(error.Failure() == (expected)); \
            CHECK(std::string(error.what()).find("vehicle: ") == 0); \
        } \
        CHECK(rejected); \
    }

    bool Near(double actual, double expected)
    {
        return std::fabs(actual - expected) <= 1e-12 * (1 + std::fabs(expected));
    }

    Catalog ParseCatalogText(const std::string& value)
    {
        std::istringstream input(value);
        return ParseCatalog(input, "vehicle-fixture.catalog");
    }

    std::string Save(const Vehicle& vehicle)
    {
        std::ostringstream output;
        WriteVehicle(output, vehicle, "test.vehicle");
        return output.str();
    }

    Vehicle Load(const Catalog& catalog, const std::string& value)
    {
        std::istringstream input(value);
        return ParseVehicle(input, "test.vehicle", catalog);
    }

    PartStock Stock(const Catalog& catalog)
    {
        PartCounts counts;
        for (size_t i = 0; i < catalog.parts.size(); ++i)
            counts[catalog.parts[i].id] = 8;
        return PartStock(catalog, 256, counts);
    }

    void FitRequired(Vehicle& vehicle, PartStock& stock)
    {
        vehicle.Fit("seat_driver", "basic_seat", stock);
        vehicle.Fit("engine", vehicle.FrameId() == "hauler" ?
                    "small_diesel_engine" : "small_gas_engine", stock);
        vehicle.Fit("tank", "tank_40", stock);
        const char* wheels[] = {"wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr"};
        for (size_t i = 0; i < 4; ++i)
            vehicle.Fit(wheels[i], "offroad_wheel", stock);
    }

    enum Action { FIT_PART, REMOVE_PART, REPLACE_PART };

    void RejectExchange(Vehicle& vehicle, PartStock& stock, Action action,
                        const char* slot, const char* part, VehicleFailure expected)
    {
        const std::string before = Save(vehicle);
        const PartCounts counts = stock.Counts();
        const unsigned int total = stock.Total();
        const unsigned int capacity = stock.Capacity();
        bool rejected = false;
        try
        {
            if (action == FIT_PART)
                vehicle.Fit(slot, part, stock);
            else if (action == REMOVE_PART)
                vehicle.Remove(slot, stock);
            else
                vehicle.Replace(slot, part, stock);
        }
        catch (const VehicleError& error)
        {
            ++rejectedTransactions;
            rejected = true;
            CHECK(error.Failure() == expected);
            CHECK(std::string(error.what()).find("vehicle: ") == 0);
            CHECK(std::strlen(error.what()) > 10);
        }
        CHECK(rejected);
        CHECK(Save(vehicle) == before);
        CHECK(stock.Counts() == counts);
        CHECK(stock.Total() == total);
        CHECK(stock.Capacity() == capacity);
    }

    void RejectSave(const Catalog& catalog, const std::string& value,
                    size_t line, const char* detail)
    {
        PartStock stock = Stock(catalog);
        Vehicle existing(catalog, "small_frame");
        FitRequired(existing, stock);
        existing.Refuel(FUEL_GASOLINE, 7);
        const std::string before = Save(existing);
        const PartCounts counts = stock.Counts();
        bool rejected = false;
        try
        {
            existing = Load(catalog, value);
        }
        catch (const VehicleSaveError& error)
        {
            ++rejectedSaves;
            rejected = true;
            CHECK(error.Source() == "test.vehicle");
            CHECK(error.Line() == line);
            CHECK(std::string(error.what()).find(detail) != std::string::npos);
            CHECK(std::string(error.what()).find("[context: ") != std::string::npos);
            std::istringstream lines(value);
            std::string original;
            for (size_t i = 0; i < line; ++i)
                if (!std::getline(lines, original))
                    original = "<end of file>";
            CHECK(error.Context() == original);
        }
        CHECK(rejected);
        CHECK(Save(existing) == before);
        CHECK(stock.Counts() == counts);
    }

    void CatalogOwnershipAndReadiness(const Catalog& catalog)
    {
        CHECK(catalog.FindSlot("small_frame", "engine") == &catalog.slots[1]);
        CHECK(catalog.FindSlot("hauler", "cargo") == &catalog.slots[31]);
        CHECK(catalog.FindSlot("small_frame", "cargo") == NULL);
        EXPECT_FAILURE(VEHICLE_UNKNOWN_FRAME, Vehicle(catalog, "missing"));
        EXPECT_FAILURE(VEHICLE_UNKNOWN_FRAME, Vehicle(catalog, "Small_frame"));

        const char* frames[] = {"small_frame", "scout", "hauler"};
        for (size_t i = 0; i < 3; ++i)
        {
            Vehicle vehicle(catalog, frames[i]);
            PartStock stock = Stock(catalog);
            CHECK(vehicle.FrameId() == frames[i]);
            CHECK(vehicle.Installed().empty());
            CHECK(vehicle.FuelState().type == FUEL_NONE);
            CHECK(vehicle.FuelState().litres == 0);
            CHECK(vehicle.TankCapacityLitres() == 0);
            CHECK(!vehicle.Readiness().Ready());
            CHECK(vehicle.Readiness().missingSlots.size() == 7);
            CHECK(vehicle.Readiness().unsupportedSlots.empty());
            vehicle.Fit("armor", "light_armor", stock);
            vehicle.Fit("headlights", "headlights", stock);
            if (i != 0)
                vehicle.Fit("seat_passenger_1", "basic_seat", stock);
            if (i == 2)
                vehicle.Fit("cargo", "cargo_rack", stock);
            CHECK(vehicle.Readiness().missingSlots.size() == 7);
            FitRequired(vehicle, stock);
            CHECK(vehicle.Readiness().Ready());
            CHECK(vehicle.FuelState().litres == 0);
            CHECK(vehicle.TankCapacityLitres() == 40);
            vehicle.Remove("armor", stock);
            vehicle.Remove("headlights", stock);
            CHECK(vehicle.Readiness().Ready());
            vehicle.Remove("wheel_rr", stock);
            CHECK(!vehicle.Readiness().Ready());
            CHECK(vehicle.Readiness().missingSlots.size() == 1);
            CHECK(vehicle.Readiness().missingSlots[0] == "wheel_rr");
        }

        Catalog original = catalog;
        Vehicle snapshot(original, "small_frame");
        original = Catalog();
        PartStock stock = Stock(catalog);
        FitRequired(snapshot, stock);
        CHECK(snapshot.Readiness().Ready());
        snapshot.Refuel(FUEL_GASOLINE, 1);
        CHECK(Near(snapshot.Consume(100).consumedLitres, 0.2));
        Vehicle copied(snapshot);
        copied.Drain(copied.FuelState().litres);
        CHECK(snapshot.FuelState().litres > 0);
        Vehicle assigned(catalog, "hauler");
        assigned = snapshot;
        CHECK(Save(assigned) == Save(snapshot));
        assigned = assigned;
        CHECK(Save(assigned) == Save(snapshot));
        PartStock stockCopy(stock);
        PartStock stockAssigned(catalog, 0);
        stockAssigned = stockCopy;
        stockAssigned = stockAssigned;
        CHECK(stockAssigned.Counts() == stock.Counts());
        CHECK(stockAssigned.Capacity() == stock.Capacity());

        Catalog noBindings = catalog;
        noBindings.buildings.clear();
        Vehicle declarations(noBindings, "scout");
        PartStock declarationStock = Stock(noBindings);
        FitRequired(declarations, declarationStock);
        CHECK(declarations.Readiness().Ready());
        const Catalog incomplete = ParseCatalogText(
            "catalog 1\npart e engine gasoline 0 1\n"
            "frame f 0 0 0 1 1 1\nslot f engine e\n");
        Vehicle unsupported(incomplete, "f");
        CHECK(!unsupported.Readiness().Ready());
        CHECK(unsupported.Readiness().unsupportedSlots.size() == 6);
        Catalog mixed = catalog;
        mixed.slots[0].compatiblePartIds.push_back("road_wheel");
        Vehicle mixedRoles(mixed, "small_frame");
        PartStock mixedStock = Stock(mixed);
        FitRequired(mixedRoles, mixedStock);
        CHECK(!mixedRoles.Readiness().Ready());
        CHECK(mixedRoles.Readiness().unsupportedSlots.size() == 1);
        CHECK(mixedRoles.Readiness().unsupportedSlots[0] == "seat_driver");
    }

    void RejectCatalogModel(const Catalog& catalog, const char* detail)
    {
        bool rejected = false;
        try
        {
            Vehicle vehicle(catalog, "small_frame");
        }
        catch (const CatalogError& error)
        {
            rejected = true;
            CHECK(error.Source() == "<catalog>");
            CHECK(std::string(error.what()).find(detail) != std::string::npos);
            CHECK(!error.Context().empty());
        }
        CHECK(rejected);
    }

    void MutableCatalogValidation(const Catalog& catalog)
    {
        Catalog bad = catalog;
        bad.version = 2;
        RejectCatalogModel(bad, "version");
        bad = catalog;
        bad.parts.push_back(bad.parts[0]);
        RejectCatalogModel(bad, "duplicate part");
        bad = catalog;
        bad.parts[0].kind = static_cast<PartKind>(99);
        RejectCatalogModel(bad, "KIND");
        bad = catalog;
        bad.parts[0].fuel = static_cast<Fuel>(99);
        RejectCatalogModel(bad, "FUEL");
        bad = catalog;
        bad.parts[0].burnLitresPerSimSecond = std::numeric_limits<double>::quiet_NaN();
        RejectCatalogModel(bad, "finite");
        bad = catalog;
        bad.parts[4].capacityLitres = std::numeric_limits<double>::infinity();
        RejectCatalogModel(bad, "finite");
        bad = catalog;
        bad.parts[0].burnLitresPerSimSecond = 0;
        RejectCatalogModel(bad, "engine requires");
        bad = catalog;
        bad.parts[4].capacityLitres = 0;
        RejectCatalogModel(bad, "tank requires");
        bad = catalog;
        bad.parts[2].capacityLitres = 1;
        RejectCatalogModel(bad, "wheel/upgrade/seat requires");
        bad = catalog;
        bad.parts[0].id = "bad.id";
        RejectCatalogModel(bad, "identifier");
        bad = catalog;
        bad.frames.push_back(bad.frames[0]);
        RejectCatalogModel(bad, "duplicate frame");
        bad = catalog;
        bad.frames[0].maxMps = std::numeric_limits<double>::infinity();
        RejectCatalogModel(bad, "finite");
        bad = catalog;
        bad.frames[0].buildWork = -1;
        RejectCatalogModel(bad, "nonnegative");
        bad = catalog;
        bad.slots.push_back(bad.slots[0]);
        RejectCatalogModel(bad, "duplicate slot");
        bad = catalog;
        bad.slots[0].frameId = "missing";
        RejectCatalogModel(bad, "unknown slot frame");
        bad = catalog;
        bad.slots[0].compatiblePartIds.clear();
        RejectCatalogModel(bad, "requires compatible");
        bad = catalog;
        bad.slots[0].compatiblePartIds.push_back("missing");
        RejectCatalogModel(bad, "unknown compatible part");
        bad = catalog;
        bad.slots[0].compatiblePartIds.push_back("basic_seat");
        RejectCatalogModel(bad, "duplicate compatible");
        bad = catalog;
        bad.buildings.push_back(bad.buildings[0]);
        RejectCatalogModel(bad, "duplicate building");
        bad = catalog;
        bad.buildings[0].frameId = "missing";
        RejectCatalogModel(bad, "unknown building frame");
        bad = catalog;
        bad.buildings[0].binding.clear();
        RejectCatalogModel(bad, "nonempty");
        bad = catalog;
        bad.buildings[0].binding = "bad\tbinding";
        RejectCatalogModel(bad, "control character");
        bad = catalog;
        Building duplicate = bad.buildings[0];
        duplicate.frameId = "scout";
        bad.buildings.push_back(duplicate);
        RejectCatalogModel(bad, "duplicate FCS");
    }

    void Transactions(const Catalog& catalog)
    {
        Vehicle vehicle(catalog, "small_frame");
        PartStock stock = Stock(catalog);
        RejectExchange(vehicle, stock, FIT_PART, "missing", "road_wheel", VEHICLE_UNKNOWN_SLOT);
        RejectExchange(vehicle, stock, FIT_PART, "wheel_fl", "missing", VEHICLE_UNKNOWN_PART);
        RejectExchange(vehicle, stock, FIT_PART, "wheel_fl", "Road_wheel", VEHICLE_UNKNOWN_PART);
        RejectExchange(vehicle, stock, FIT_PART, "engine", "road_wheel", VEHICLE_INCOMPATIBLE_PART);
        RejectExchange(vehicle, stock, FIT_PART, "tank", "tank_80", VEHICLE_INCOMPATIBLE_PART);
        RejectExchange(vehicle, stock, FIT_PART, "armor", "heavy_armor", VEHICLE_INCOMPATIBLE_PART);
        RejectExchange(vehicle, stock, REMOVE_PART, "wheel_fl", "", VEHICLE_SLOT_EMPTY);
        RejectExchange(vehicle, stock, REPLACE_PART, "wheel_fl", "road_wheel", VEHICLE_SLOT_EMPTY);
        RejectExchange(vehicle, stock, REMOVE_PART, "missing", "", VEHICLE_UNKNOWN_SLOT);
        RejectExchange(vehicle, stock, REPLACE_PART, "missing", "road_wheel", VEHICLE_UNKNOWN_SLOT);
        const unsigned int initial = stock.Total();
        vehicle.Fit("wheel_fl", "road_wheel", stock);
        CHECK(stock.Count("road_wheel") == 7);
        CHECK(stock.Total() == initial - 1);
        CHECK(vehicle.Installed().find("wheel_fl")->second == "road_wheel");
        RejectExchange(vehicle, stock, FIT_PART, "wheel_fl", "offroad_wheel", VEHICLE_SLOT_OCCUPIED);
        RejectExchange(vehicle, stock, REPLACE_PART, "wheel_fl", "road_wheel", VEHICLE_SAME_PART);
        RejectExchange(vehicle, stock, REPLACE_PART, "wheel_fl", "tank_40", VEHICLE_INCOMPATIBLE_PART);
        RejectExchange(vehicle, stock, REPLACE_PART, "wheel_fl", "missing", VEHICLE_UNKNOWN_PART);
        vehicle.Replace("wheel_fl", "offroad_wheel", stock);
        CHECK(stock.Count("road_wheel") == 8);
        CHECK(stock.Count("offroad_wheel") == 7);
        CHECK(stock.Total() == initial - 1);
        vehicle.Remove("wheel_fl", stock);
        CHECK(stock.Count("offroad_wheel") == 8);
        CHECK(stock.Total() == initial);
        CHECK(vehicle.Installed().empty());
        for (size_t i = 0; i < 20; ++i)
        {
            vehicle.Fit("wheel_fl", "road_wheel", stock);
            vehicle.Replace("wheel_fl", "offroad_wheel", stock);
            vehicle.Remove("wheel_fl", stock);
        }
        CHECK(stock.Count("road_wheel") == 8);
        CHECK(stock.Count("offroad_wheel") == 8);
        CHECK(stock.Total() == initial);
        Vehicle hauler(catalog, "hauler");
        RejectExchange(hauler, stock, FIT_PART, "engine", "small_gas_engine", VEHICLE_INCOMPATIBLE_PART);
        RejectExchange(hauler, stock, FIT_PART, "wheel_fl", "road_wheel", VEHICLE_INCOMPATIBLE_PART);

        PartStock empty(catalog, 0);
        RejectExchange(vehicle, empty, FIT_PART, "wheel_fl", "road_wheel", VEHICLE_INSUFFICIENT_STOCK);
        vehicle.Fit("wheel_fl", "road_wheel", stock);
        RejectExchange(vehicle, empty, REMOVE_PART, "wheel_fl", "", VEHICLE_INVENTORY_CAPACITY);
        RejectExchange(vehicle, empty, REPLACE_PART, "wheel_fl", "offroad_wheel", VEHICLE_INSUFFICIENT_STOCK);
        PartCounts single;
        single["offroad_wheel"] = 1;
        PartStock full(catalog, 1, single);
        vehicle.Replace("wheel_fl", "offroad_wheel", full);
        CHECK(full.Total() == full.Capacity());
        CHECK(full.Count("road_wheel") == 1);
        CHECK(full.Count("offroad_wheel") == 0);
        RejectExchange(vehicle, full, REMOVE_PART, "wheel_fl", "", VEHICLE_INVENTORY_CAPACITY);
        vehicle.Replace("wheel_fl", "road_wheel", full);
        CHECK(full.Count("offroad_wheel") == 1);
        CHECK(full.Count("road_wheel") == 0);
        PartCounts maximum;
        maximum["road_wheel"] = (std::numeric_limits<unsigned int>::max)();
        PartStock overflow(catalog, (std::numeric_limits<unsigned int>::max)(), maximum);
        RejectExchange(vehicle, overflow, REMOVE_PART, "wheel_fl", "", VEHICLE_COUNT_OVERFLOW);
        maximum.clear();
        maximum["offroad_wheel"] = (std::numeric_limits<unsigned int>::max)();
        PartStock totalOverflow(catalog, (std::numeric_limits<unsigned int>::max)(), maximum);
        RejectExchange(vehicle, totalOverflow, REMOVE_PART, "wheel_fl", "", VEHICLE_COUNT_OVERFLOW);
        maximum.clear();
        maximum["road_wheel"] = (std::numeric_limits<unsigned int>::max)();
        maximum["offroad_wheel"] = 1;
        EXPECT_FAILURE(VEHICLE_COUNT_OVERFLOW,
                       PartStock(catalog, (std::numeric_limits<unsigned int>::max)(), maximum));
        single["offroad_wheel"] = 2;
        EXPECT_FAILURE(VEHICLE_INVENTORY_CAPACITY, PartStock(catalog, 1, single));
        single.clear();
        single["missing"] = 0;
        EXPECT_FAILURE(VEHICLE_UNKNOWN_PART, PartStock(catalog, 0, single));
        single.clear();
        single["road_wheel"] = 0;
        PartStock zero(catalog, 0, single);
        CHECK(zero.Counts().empty());

        Catalog extended = catalog;
        Part extra = catalog.parts[2];
        extra.id = "fixture_only_part";
        extended.parts.push_back(extra);
        single.clear();
        single["fixture_only_part"] = 1;
        single["road_wheel"] = 1;
        PartStock foreign(extended, 2, single);
        RejectExchange(vehicle, foreign, FIT_PART, "wheel_fr", "road_wheel", VEHICLE_UNKNOWN_PART);
    }

    void FuelAndPartPreservation(const Catalog& catalog)
    {
        Vehicle vehicle(catalog, "small_frame");
        PartStock stock = Stock(catalog);
        EXPECT_FAILURE(VEHICLE_TANK_REQUIRED, vehicle.Refuel(FUEL_GASOLINE, 0));
        EXPECT_FAILURE(VEHICLE_ENGINE_REQUIRED, vehicle.Consume(0));
        vehicle.Fit("tank", "tank_40", stock);
        CHECK(vehicle.Refuel(FUEL_GASOLINE, 10) == 10);
        RejectExchange(vehicle, stock, FIT_PART, "engine", "small_diesel_engine", VEHICLE_INCOMPATIBLE_FUEL);
        vehicle.Fit("engine", "small_gas_engine", stock);
        vehicle.Remove("engine", stock);
        CHECK(vehicle.FuelState().type == FUEL_GASOLINE);
        CHECK(vehicle.FuelState().litres == 10);
        EXPECT_FAILURE(VEHICLE_ENGINE_REQUIRED, vehicle.Consume(1));
        const std::string parked = Save(vehicle);
        EXPECT_FAILURE(VEHICLE_INCOMPATIBLE_FUEL, vehicle.Refuel(FUEL_DIESEL, 1));
        EXPECT_FAILURE(VEHICLE_INSUFFICIENT_FUEL, vehicle.Drain(11));
        CHECK(Save(vehicle) == parked);
        RejectExchange(vehicle, stock, REMOVE_PART, "tank", "", VEHICLE_TANK_REQUIRED);
        const FuelAmount drained = vehicle.Drain(5);
        CHECK(drained.type == FUEL_GASOLINE);
        CHECK(drained.litres == 5);
        CHECK(vehicle.FuelState().litres == 5);
        CHECK(vehicle.Drain(5).litres == 5);
        CHECK(vehicle.FuelState().type == FUEL_NONE);
        CHECK(vehicle.FuelState().litres == 0);
        CHECK(vehicle.Drain(0).type == FUEL_NONE);
        vehicle.Fit("engine", "small_diesel_engine", stock);
        vehicle.Refuel(FUEL_DIESEL, 40);
        CHECK(vehicle.FuelState().litres == vehicle.TankCapacityLitres());
        const std::string full = Save(vehicle);
        EXPECT_FAILURE(VEHICLE_FUEL_CAPACITY, vehicle.Refuel(FUEL_DIESEL, 0.001));
        EXPECT_FAILURE(VEHICLE_INCOMPATIBLE_FUEL, vehicle.Refuel(FUEL_GASOLINE, 0));
        CHECK(Save(vehicle) == full);
        RejectExchange(vehicle, stock, REPLACE_PART, "engine", "small_gas_engine", VEHICLE_INCOMPATIBLE_FUEL);
        vehicle.Drain(40);
        vehicle.Replace("engine", "small_gas_engine", stock);
        CHECK(vehicle.Refuel(FUEL_GASOLINE, 0) == 0);
        CHECK(vehicle.FuelState().type == FUEL_NONE);
        CHECK(vehicle.Refuel(FUEL_GASOLINE, 0.1) == 0.1);

        const double invalid[] = {-1, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        {
            const std::string before = Save(vehicle);
            EXPECT_FAILURE(VEHICLE_INVALID_QUANTITY, vehicle.Refuel(FUEL_GASOLINE, invalid[i]));
            EXPECT_FAILURE(VEHICLE_INVALID_QUANTITY, vehicle.Drain(invalid[i]));
            EXPECT_FAILURE(VEHICLE_INVALID_QUANTITY, vehicle.Consume(invalid[i]));
            CHECK(Save(vehicle) == before);
        }
        const std::string before = Save(vehicle);
        EXPECT_FAILURE(VEHICLE_INVALID_FUEL, vehicle.Refuel(FUEL_NONE, 1));
        EXPECT_FAILURE(VEHICLE_INVALID_FUEL, vehicle.Refuel(static_cast<Fuel>(99), 1));
        EXPECT_FAILURE(VEHICLE_FUEL_CAPACITY,
                       vehicle.Refuel(FUEL_GASOLINE, (std::numeric_limits<double>::max)()));
        CHECK(Save(vehicle) == before);

        Vehicle hauler(catalog, "hauler");
        hauler.Fit("engine", "small_diesel_engine", stock);
        hauler.Fit("tank", "tank_80", stock);
        hauler.Refuel(FUEL_DIESEL, 60);
        RejectExchange(hauler, stock, REPLACE_PART, "tank", "tank_40", VEHICLE_FUEL_CAPACITY);
        CHECK(hauler.Drain(20).litres == 20);
        hauler.Replace("tank", "tank_40", stock);
        CHECK(hauler.FuelState().litres == 40);
        CHECK(hauler.FuelState().type == FUEL_DIESEL);
        CHECK(hauler.TankCapacityLitres() == 40);
        hauler.Replace("tank", "tank_80", stock);
        CHECK(hauler.FuelState().litres == 40);
        CHECK(hauler.TankCapacityLitres() == 80);
        hauler.Remove("engine", stock);
        CHECK(hauler.FuelState().litres == 40);
        CHECK(hauler.Drain(40).type == FUEL_DIESEL);
        hauler.Remove("tank", stock);
        CHECK(hauler.TankCapacityLitres() == 0);
        CHECK(hauler.FuelState().type == FUEL_NONE);

        Catalog sameType = catalog;
        Part other = catalog.parts[0];
        other.id = "fixture_gas_engine";
        other.burnLitresPerSimSecond = 0.004;
        sameType.parts.push_back(other);
        sameType.slots[1].compatiblePartIds.push_back(other.id);
        Vehicle change(sameType, "small_frame");
        PartStock alternate = Stock(sameType);
        change.Fit("engine", "small_gas_engine", alternate);
        change.Fit("tank", "tank_40", alternate);
        change.Refuel(FUEL_GASOLINE, 1);
        change.Replace("engine", other.id, alternate);
        CHECK(change.FuelState().litres == 1);
        CHECK(Near(change.Consume(100).consumedLitres, 0.4));
    }

    void ConsumptionBoundaries(const Catalog& catalog)
    {
        PartStock stock = Stock(catalog);
        Vehicle vehicle(catalog, "small_frame");
        vehicle.Fit("engine", "small_gas_engine", stock);
        FuelConsumption consumed = vehicle.Consume(100);
        CHECK(consumed.consumedLitres == 0);
        CHECK(consumed.usableSimSeconds == 0);
        CHECK(consumed.exhausted);
        vehicle.Fit("tank", "tank_40", stock);
        vehicle.Refuel(FUEL_GASOLINE, 1);
        const std::string before = Save(vehicle);
        consumed = vehicle.Consume(0);
        CHECK(consumed.consumedLitres == 0);
        CHECK(consumed.usableSimSeconds == 0);
        CHECK(!consumed.exhausted);
        CHECK(Save(vehicle) == before);
        consumed = vehicle.Consume(100);
        CHECK(consumed.consumedLitres == 1 - vehicle.FuelState().litres);
        CHECK(Near(consumed.consumedLitres, 0.2));
        CHECK(consumed.usableSimSeconds == 100);
        CHECK(!consumed.exhausted);
        consumed = vehicle.Consume(1000);
        CHECK(Near(consumed.consumedLitres, 0.8));
        CHECK(Near(consumed.usableSimSeconds, 400));
        CHECK(consumed.exhausted);
        CHECK(vehicle.FuelState().type == FUEL_NONE);
        consumed = vehicle.Consume((std::numeric_limits<double>::max)());
        CHECK(consumed.consumedLitres == 0);
        CHECK(consumed.usableSimSeconds == 0);
        vehicle.Refuel(FUEL_GASOLINE, 1);
        consumed = vehicle.Consume((std::numeric_limits<double>::max)());
        CHECK(consumed.consumedLitres == 1);
        CHECK(consumed.usableSimSeconds == 500);
        CHECK(consumed.exhausted);
        vehicle.Replace("engine", "small_diesel_engine", stock);
        vehicle.Refuel(FUEL_DIESEL, 1.5);
        consumed = vehicle.Consume(1000);
        CHECK(consumed.consumedLitres == 1.5);
        CHECK(consumed.usableSimSeconds == 1000);
        CHECK(consumed.exhausted);

        Catalog binary = catalog;
        binary.parts[0].burnLitresPerSimSecond = 0.5;
        Vehicle exact(binary, "small_frame");
        exact.Fit("engine", "small_gas_engine", stock);
        exact.Fit("tank", "tank_40", stock);
        exact.Refuel(FUEL_GASOLINE, 1);
        const double justBelow = 2 - std::numeric_limits<double>::epsilon();
        consumed = exact.Consume(justBelow);
        CHECK(!consumed.exhausted);
        CHECK(consumed.usableSimSeconds == justBelow);
        CHECK(exact.FuelState().litres > 0);
        consumed = exact.Consume(1);
        CHECK(consumed.exhausted);
        CHECK(consumed.usableSimSeconds > 0 && consumed.usableSimSeconds < 1);
        exact.Refuel(FUEL_GASOLINE, 1);
        consumed = exact.Consume(2);
        CHECK(consumed.consumedLitres == 1);
        CHECK(consumed.usableSimSeconds == 2);
        CHECK(consumed.exhausted);

        vehicle.Replace("engine", "small_gas_engine", stock);
        vehicle.Refuel(FUEL_GASOLINE, 1);
        const std::string precision = Save(vehicle);
        const double tiny = (std::numeric_limits<double>::min)();
        EXPECT_FAILURE(VEHICLE_NUMERIC_RANGE, vehicle.Refuel(FUEL_GASOLINE, tiny));
        EXPECT_FAILURE(VEHICLE_NUMERIC_RANGE, vehicle.Drain(tiny));
        EXPECT_FAILURE(VEHICLE_NUMERIC_RANGE, vehicle.Consume(tiny));
        CHECK(Save(vehicle) == precision);

        Catalog slow = catalog;
        slow.parts[0].burnLitresPerSimSecond = tiny;
        Vehicle slowEngine(slow, "small_frame");
        slowEngine.Fit("engine", "small_gas_engine", stock);
        slowEngine.Fit("tank", "tank_40", stock);
        slowEngine.Refuel(FUEL_GASOLINE, 8);
        const std::string slowBefore = Save(slowEngine);
        EXPECT_FAILURE(VEHICLE_NUMERIC_RANGE, slowEngine.Consume(tiny));
        CHECK(Save(slowEngine) == slowBefore);
        consumed = slowEngine.Consume((std::numeric_limits<double>::max)());
        CHECK(text::Finite(consumed.consumedLitres));
        CHECK(Near(consumed.consumedLitres, 4));
        CHECK(consumed.usableSimSeconds == (std::numeric_limits<double>::max)());
        CHECK(!consumed.exhausted);

        Catalog fast = catalog;
        fast.parts[0].burnLitresPerSimSecond = (std::numeric_limits<double>::max)();
        Vehicle fastEngine(fast, "small_frame");
        fastEngine.Fit("engine", "small_gas_engine", stock);
        fastEngine.Fit("tank", "tank_40", stock);
        fastEngine.Refuel(FUEL_GASOLINE, 1);
        consumed = fastEngine.Consume(2);
        CHECK(consumed.consumedLitres == 1);
        CHECK(text::Finite(consumed.usableSimSeconds));
        CHECK(consumed.usableSimSeconds > 0);
        CHECK(consumed.exhausted);
        fastEngine.Refuel(FUEL_GASOLINE, std::numeric_limits<double>::denorm_min());
        const std::string fastBefore = Save(fastEngine);
        EXPECT_FAILURE(VEHICLE_NUMERIC_RANGE, fastEngine.Consume(1));
        CHECK(Save(fastEngine) == fastBefore);
    }

    void RoundTrip(const Catalog& catalog, const Vehicle& vehicle)
    {
        const std::string serialized = Save(vehicle);
        const Vehicle loaded = Load(catalog, serialized);
        ++roundTrips;
        CHECK(loaded.FrameId() == vehicle.FrameId());
        CHECK(loaded.Installed() == vehicle.Installed());
        CHECK(loaded.FuelState().type == vehicle.FuelState().type);
        if (loaded.FuelState().litres != vehicle.FuelState().litres)
        {
            std::ostringstream detail;
            detail << std::setprecision(17) << "round trip " << roundTrips
                   << ": original " << vehicle.FuelState().litres
                   << ", loaded " << loaded.FuelState().litres << '\n' << serialized;
            throw std::runtime_error(detail.str());
        }
        CHECK(loaded.FuelState().litres == vehicle.FuelState().litres);
        CHECK(Save(loaded) == serialized);
        CHECK(loaded.Readiness().Ready() == vehicle.Readiness().Ready());
    }

    void Serialization(const Catalog& catalog)
    {
        const std::string bare =
            "projectcars-remake-vehicle 1\nframe small_frame\n"
            "fuel none binary64:0000000000000000\nend\n";
        Vehicle vehicle(catalog, "small_frame");
        CHECK(Save(vehicle) == bare);
        RoundTrip(catalog, vehicle);
        PartStock stock = Stock(catalog);
        vehicle.Fit("tank", "tank_40", stock);
        vehicle.Refuel(FUEL_GASOLINE, 0.1);
        RoundTrip(catalog, vehicle);
        vehicle.Fit("engine", "small_gas_engine", stock);
        vehicle.Fit("wheel_fr", "road_wheel", stock);
        vehicle.Fit("wheel_fl", "road_wheel", stock);
        const std::string expected =
            "projectcars-remake-vehicle 1\nframe small_frame\n"
            "installed engine small_gas_engine\ninstalled tank tank_40\n"
            "installed wheel_fl road_wheel\ninstalled wheel_fr road_wheel\n"
            "fuel gasoline binary64:3fb999999999999a\nend\n";
        CHECK(Save(vehicle) == expected);
        const PartCounts before = stock.Counts();
        RoundTrip(catalog, vehicle);
        CHECK(stock.Counts() == before);
        const Vehicle reordered = Load(catalog,
            "projectcars-remake-vehicle 1\ninstalled wheel_fr road_wheel\n"
            "fuel gasoline binary64:3fb999999999999a\ninstalled wheel_fl road_wheel\n"
            "frame small_frame\ninstalled tank tank_40\n"
            "installed engine small_gas_engine\nend");
        CHECK(Save(reordered) == expected);
        CHECK(Save(Load(catalog, "\xef\xbb\xbf# BOM\r\n\tprojectcars-remake-vehicle 1\r\n"
             "frame small_frame # frame\r\nfuel none binary64:0000000000000000\r\n"
             "end\r\n# trailing comment\r\n")) == bare);
        Vehicle ready(catalog, "scout");
        FitRequired(ready, stock);
        RoundTrip(catalog, ready);
        CHECK(ready.Readiness().Ready());
        Vehicle hauler(catalog, "hauler");
        hauler.Fit("tank", "tank_80", stock);
        hauler.Fit("engine", "small_diesel_engine", stock);
        hauler.Refuel(FUEL_DIESEL, 80);
        RoundTrip(catalog, hauler);

        Vehicle numeric(catalog, "small_frame");
        numeric.Fit("tank", "tank_40", stock);
        const double values[] = {0, 0.1, 1.2345678901234567, 39.999999999999993,
            (std::numeric_limits<double>::min)(), std::numeric_limits<double>::denorm_min()};
        for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        {
            numeric.Drain(numeric.FuelState().litres);
            numeric.Refuel(FUEL_GASOLINE, values[i]);
            RoundTrip(catalog, numeric);
        }
        unsigned __int64 bits = 0x13579bdf2468ace0ui64;
        for (size_t i = 0; i < 512; ++i)
        {
            bits = bits * 6364136223846793005ui64 + 1442695040888963407ui64;
            const unsigned __int64 positive = bits & 0x7fffffffffffffffui64;
            double value = 0;
            std::memcpy(&value, &positive, sizeof(value));
            if (!text::Finite(value) || value <= 0 || value > 40)
                continue;
            numeric.Drain(numeric.FuelState().litres);
            numeric.Refuel(FUEL_GASOLINE, value);
            RoundTrip(catalog, numeric);
        }

        Catalog maximumTank = catalog;
        maximumTank.parts[4].capacityLitres = (std::numeric_limits<double>::max)();
        Vehicle maximumVolume(maximumTank, "small_frame");
        PartStock maximumStock = Stock(maximumTank);
        maximumVolume.Fit("tank", "tank_40", maximumStock);
        maximumVolume.Refuel(FUEL_GASOLINE, (std::numeric_limits<double>::max)());
        RoundTrip(maximumTank, maximumVolume);

        const Catalog caseSensitive = ParseCatalogText(
            "catalog 1\npart Part-1 wheel none 0 0\nframe Frame-1 0 0 0 1 1 1\n"
            "slot Frame-1 z-Slot Part-1\nslot Frame-1 A-Slot Part-1\n");
        Vehicle exactIds(caseSensitive, "Frame-1");
        PartStock exactStock = Stock(caseSensitive);
        exactIds.Fit("z-Slot", "Part-1", exactStock);
        exactIds.Fit("A-Slot", "Part-1", exactStock);
        CHECK(Save(exactIds) ==
              "projectcars-remake-vehicle 1\nframe Frame-1\n"
              "installed A-Slot Part-1\ninstalled z-Slot Part-1\n"
              "fuel none binary64:0000000000000000\nend\n");
        RoundTrip(caseSensitive, exactIds);

        class CommaDecimal : public std::numpunct<char>
        {
        protected:
            virtual char do_decimal_point() const { return ','; }
        };
        const std::locale old = std::locale();
        std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
        try
        {
            std::ostringstream output;
            output << std::scientific << std::setprecision(2);
            const std::ios::fmtflags flags = output.flags();
            WriteVehicle(output, vehicle, "locale.vehicle");
            CHECK(output.str() == expected);
            CHECK(output.flags() == flags);
            CHECK(output.precision() == 2);
            CHECK(std::use_facet<std::numpunct<char> >(output.getloc()).decimal_point() == ',');
            RoundTrip(catalog, vehicle);
            RejectSave(catalog, "projectcars-remake-vehicle 1\nframe small_frame\n"
                       "fuel gasoline 0,1\nend\n", 3, "binary64:");
        }
        catch (...)
        {
            std::locale::global(old);
            throw;
        }
        std::locale::global(old);
    }

    void InvalidSaves(const Catalog& catalog)
    {
        const std::string header = "projectcars-remake-vehicle 1\n";
        const std::string frame = header + "frame small_frame\n";
        const std::string empty = "fuel none binary64:0000000000000000\n";
        RejectSave(catalog, "", 1, "missing projectcars-remake");
        RejectSave(catalog, "vehicle 1\n", 1, "first directive");
        RejectSave(catalog, "catalog 1\n", 1, "first directive");
        RejectSave(catalog, "projectcars-remake-vehicle 2\n", 1, "unsupported vehicle version");
        RejectSave(catalog, "projectcars-remake-vehicle 01\n", 1, "unsupported vehicle version");
        RejectSave(catalog, "projectcars-remake-vehicle +1\n", 1, "unsupported vehicle version");
        RejectSave(catalog, "projectcars-remake-vehicle\n", 1, "expects 1 fields");
        RejectSave(catalog, header + header, 2, "duplicate vehicle header");
        RejectSave(catalog, header + empty + "end\n", 4, "missing vehicle frame");
        RejectSave(catalog, frame + "end\n", 4, "missing vehicle fuel");
        RejectSave(catalog, frame + empty, 4, "missing end");
        RejectSave(catalog, frame + "frame scout\n", 3, "duplicate vehicle frame");
        RejectSave(catalog, frame + empty + empty, 4, "duplicate vehicle fuel");
        RejectSave(catalog, frame + empty + "end\nend\n", 5, "content after end");
        RejectSave(catalog, frame + empty + "end\ninstalled engine small_gas_engine\n",
                   5, "content after end");
        RejectSave(catalog, frame + "native_handle 123\n", 3, "unsupported vehicle directive");
        RejectSave(catalog, header + "frame missing\n" + empty + "end\n", 2, "unknown frame");
        RejectSave(catalog, header + "frame bad.id\n", 2, "invalid identifier");
        RejectSave(catalog, frame + "installed wheel.fl road_wheel\n", 3, "invalid identifier");
        RejectSave(catalog, frame + "installed wheel_fl bad.id\n", 3, "invalid identifier");
        RejectSave(catalog, frame + "installed wheel_fl\n", 3, "expects 2 fields");
        RejectSave(catalog, frame + "installed wheel_fl road_wheel extra\n", 3, "expects 2 fields");
        RejectSave(catalog, frame + "fuel none\n", 3, "expects 2 fields");
        RejectSave(catalog, frame + "end extra\n", 3, "expects 0 fields");
        RejectSave(catalog, header + "frame \"small_frame\"\n", 2, "quoted tokens");
        RejectSave(catalog, frame + "installed engine small_gas_engine\n"
                   "installed engine small_diesel_engine\n", 4, "duplicate installed slot");
        RejectSave(catalog, frame + "installed missing road_wheel\n" + empty + "end\n",
                   3, "unknown slot");
        RejectSave(catalog, frame + "installed engine missing\n" + empty + "end\n",
                   3, "unknown part");
        RejectSave(catalog, frame + "installed engine road_wheel\n" + empty + "end\n",
                   3, "incompatible");
        RejectSave(catalog, frame + "installed tank tank_80\n" + empty + "end\n",
                   3, "incompatible");
        RejectSave(catalog, frame + "fuel none binary64:3ff0000000000000\nend\n",
                   3, "empty fuel must be none");
        RejectSave(catalog, frame + "fuel gasoline binary64:0000000000000000\nend\n",
                   3, "empty fuel must be none");
        RejectSave(catalog, frame + "fuel diesel binary64:0000000000000000\nend\n",
                   3, "empty fuel must be none");
        RejectSave(catalog, frame + "fuel gasoline binary64:3ff0000000000000\nend\n",
                   3, "installed tank");
        RejectSave(catalog, frame + "installed tank tank_40\n"
                   "fuel gasoline binary64:4044000000000001\nend\n",
                   4, "capacity");
        RejectSave(catalog, frame + "installed tank tank_40\n"
                   "installed engine small_diesel_engine\nfuel gasoline binary64:3ff0000000000000\nend\n",
                   5, "incompatible");
        RejectSave(catalog, frame + "fuel electric binary64:3ff0000000000000\nend\n",
                   3, "unsupported fuel type");
        RejectSave(catalog, frame + "fuel Gasoline binary64:3ff0000000000000\nend\n",
                   3, "unsupported fuel type");
        RejectSave(catalog, frame + "fuel none binary64:bff0000000000000\nend\n",
                   3, "finite and nonnegative");
        RejectSave(catalog, frame + "fuel none binary64:8000000000000000\nend\n",
                   3, "negative zero");
        const char* nonfinite[] = {"7ff0000000000000", "fff0000000000000",
                                  "7ff8000000000000", "7ff0000000000001"};
        for (size_t i = 0; i < sizeof(nonfinite) / sizeof(nonfinite[0]); ++i)
            RejectSave(catalog, frame + "fuel gasoline binary64:" + nonfinite[i] + "\nend\n",
                       3, "finite and nonnegative");
        const char* invalid[] = {"nan", "inf", "-inf", "12.5", "1e309", "1e-9999", "0x10",
            "binary64:", "binary64:000000000000000", "binary64:00000000000000000",
            "binary64:3FF0000000000000", "binary64:3ff000000000000g", "Binary64:3ff0000000000000"};
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
            RejectSave(catalog, frame + "fuel gasoline " + invalid[i] + "\nend\n",
                       3, "binary64:");
        std::string nul = frame + "installed engine small";
        nul.push_back('\0');
        nul += "_gas_engine\n";
        RejectSave(catalog, nul, 3, "unquoted token");
    }

    void SerializationIO(const Catalog& catalog)
    {
        const std::string bare =
            "projectcars-remake-vehicle 1\nframe small_frame\n"
            "fuel none binary64:0000000000000000\nend";
        std::istringstream finalLine(bare);
        finalLine.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        CHECK(ParseVehicle(finalLine, "final-line.vehicle", catalog).Installed().empty());
        std::istringstream finalNewline(bare + "\n");
        finalNewline.exceptions(std::ios::failbit | std::ios::badbit);
        CHECK(ParseVehicle(finalNewline, "final-newline.vehicle", catalog).FrameId() == "small_frame");
        for (size_t i = 0; i < 3; ++i)
        {
            std::istringstream input(bare);
            if (i == 2)
                input.exceptions(std::ios::badbit);
            try
            {
                input.setstate(i == 1 ? std::ios::failbit : std::ios::badbit);
            }
            catch (const std::ios_base::failure&) {}
            bool rejected = false;
            try
            {
                ParseVehicle(input, "broken.vehicle", catalog);
            }
            catch (const VehicleSaveError& error)
            {
                rejected = true;
                CHECK(error.Source() == "broken.vehicle");
                CHECK(error.Line() == 1);
                CHECK(std::string(error.what()).find("input read failed") != std::string::npos);
            }
            CHECK(rejected);
        }
        class ReadFailure : public std::streambuf
        {
        public:
            explicit ReadFailure(const std::string& prefix) : prefix_(prefix)
            {
                char* begin = &prefix_[0];
                setg(begin, begin, begin + prefix_.size());
            }

        protected:
            virtual int_type underflow()
            {
                throw std::ios_base::failure("injected read failure after complete state");
            }

        private:
            std::string prefix_;
        };
        Vehicle existing(catalog, "hauler");
        const std::string existingBefore = Save(existing);
        for (size_t i = 0; i < 2; ++i)
        {
            ReadFailure readBuffer(bare + "\n");
            std::istream input(&readBuffer);
            if (i != 0)
                input.exceptions(std::ios::badbit);
            bool rejected = false;
            try
            {
                existing = ParseVehicle(input, "late-read.vehicle", catalog);
            }
            catch (const VehicleSaveError& error)
            {
                rejected = true;
                CHECK(error.Source() == "late-read.vehicle");
                CHECK(error.Line() == 5);
                CHECK(error.Context() == "<input>");
                CHECK(std::string(error.what()).find("input read failed") != std::string::npos);
            }
            CHECK(rejected);
            CHECK(Save(existing) == existingBefore);
        }
        class WriteFailure : public std::streambuf
        {
        protected:
            virtual std::streamsize xsputn(const char*, std::streamsize) { return 0; }
        };
        class FlushFailure : public std::stringbuf
        {
        protected:
            virtual int sync() { return -1; }
        };
        Vehicle vehicle(catalog, "small_frame");
        const std::string before = Save(vehicle);
        for (size_t i = 0; i < 4; ++i)
        {
            WriteFailure writeBuffer;
            FlushFailure flushBuffer;
            std::ostream output(i < 2 ? static_cast<std::streambuf*>(&writeBuffer) :
                                       static_cast<std::streambuf*>(&flushBuffer));
            if (i % 2)
                output.exceptions(std::ios::badbit | std::ios::failbit);
            bool rejected = false;
            try
            {
                WriteVehicle(output, vehicle, "broken-output.vehicle");
            }
            catch (const VehicleSaveError& error)
            {
                rejected = true;
                CHECK(error.Source() == "broken-output.vehicle");
                CHECK(error.Line() == 0);
                CHECK(error.Context() == "<output>");
                CHECK(std::string(error.what()).find("output write failed") != std::string::npos);
            }
            CHECK(rejected);
            CHECK(Save(vehicle) == before);
        }
    }
}

void RunVehicleTests(const char* catalogPath)
{
    std::ifstream input(catalogPath, std::ios::in | std::ios::binary);
    if (!input)
        throw std::runtime_error(std::string("cannot open required vehicle test catalog: ") + catalogPath);
    const projectcars::Catalog catalog = projectcars::ParseCatalog(input, catalogPath);
    CatalogOwnershipAndReadiness(catalog);
    MutableCatalogValidation(catalog);
    Transactions(catalog);
    FuelAndPartPreservation(catalog);
    ConsumptionBoundaries(catalog);
    Serialization(catalog);
    InvalidSaves(catalog);
    SerializationIO(catalog);
    std::cout << "Vehicle core PASS: " << assertions << " assertions; " << rejectedTransactions
              << " atomic rejections; " << rejectedSaves << " malformed saves rejected; "
              << roundTrips << " exact round trips; VC100 x64 Release\n";
}
