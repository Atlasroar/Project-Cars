// SPDX-License-Identifier: GPL-3.0-only
#include "catalog.h"

#include <set>
#include <utility>

namespace projectcars
{
    namespace
    {
        using text::Location;
        using text::Token;
        using text::Fail;
        using text::Arity;
        using text::Identifier;
        using text::Number;
        using text::Nonnegative;
        using text::Positive;

        PartKind Kind(const std::string& source, const Location& location,
                      const std::string& value)
        {
            if (value == "engine") return PART_ENGINE;
            if (value == "wheel") return PART_WHEEL;
            if (value == "tank") return PART_TANK;
            if (value == "upgrade") return PART_UPGRADE;
            if (value == "seat") return PART_SEAT;
            Fail(source, location, "unsupported part KIND '" + value + "'");
        }

        Fuel FuelType(const std::string& source, const Location& location,
                      const std::string& value)
        {
            if (value == "none") return FUEL_NONE;
            if (value == "gasoline") return FUEL_GASOLINE;
            if (value == "diesel") return FUEL_DIESEL;
            Fail(source, location, "unsupported FUEL '" + value + "'");
        }

        void ValidatePart(const std::string& source, const Location& location,
                          const Part& part)
        {
            if (part.kind < PART_ENGINE || part.kind > PART_SEAT)
                Fail(source, location, "unsupported part KIND");
            if (part.fuel < FUEL_NONE || part.fuel > FUEL_DIESEL)
                Fail(source, location, "unsupported FUEL");
            Nonnegative(source, location, part.capacityLitres, "CAPACITY_LITRES");
            Nonnegative(source, location, part.burnLitresPerSimSecond,
                        "BURN_LITRES_PER_SIM_SECOND");
            if (part.kind == PART_ENGINE)
            {
                if (part.fuel == FUEL_NONE || part.capacityLitres != 0 ||
                    part.burnLitresPerSimSecond <= 0)
                    Fail(source, location, "engine requires gasoline/diesel, zero capacity "
                         "and positive burn");
            }
            else if (part.kind == PART_TANK)
            {
                if (part.fuel != FUEL_NONE || part.capacityLitres <= 0 ||
                    part.burnLitresPerSimSecond != 0)
                    Fail(source, location, "tank requires fuel none, positive capacity "
                         "and zero burn");
            }
            else if (part.fuel != FUEL_NONE || part.capacityLitres != 0 ||
                     part.burnLitresPerSimSecond != 0)
                Fail(source, location, "wheel/upgrade/seat requires fuel none, "
                     "zero capacity and zero burn");
        }

        void ValidateFrame(const std::string& source, const Location& location,
                           const Frame& frame)
        {
            Nonnegative(source, location, frame.buildWork, "BUILD_WORK");
            Nonnegative(source, location, frame.disassemblyWork, "DISASSEMBLY_WORK");
            Positive(source, location, frame.maxMps, "MAX_MPS");
            Positive(source, location, frame.accelMps2, "ACCEL_MPS2");
            Positive(source, location, frame.turnRadS, "TURN_RAD_S");
        }

        Catalog ParseCatalogText(std::istream& input, const std::string& source)
        {
            Catalog catalog;
            std::set<std::string> partIds;
            std::set<std::string> frameIds;
            std::set<std::pair<std::string, std::string> > slotIds;
            std::set<std::string> buildingFrames;
            std::set<std::string> buildingBindings;
            std::vector<Location> slotLocations;
            std::vector<Location> buildingLocations;
            size_t lineNumber = 0;
            Location location;

            while (text::ReadLine(input, source, lineNumber, location))
            {
                const std::vector<Token> tokens = text::Tokenize(source, location);
                if (tokens.empty())
                    continue;
                for (size_t i = 0; i < tokens.size(); ++i)
                    if (tokens[i].quoted && !(tokens[0].value == "building" && i == 2))
                        Fail(source, location, "quoted tokens are only allowed for a building binding");

                const std::string& directive = tokens[0].value;
                if (catalog.version == 0 && directive != "catalog")
                    Fail(source, location, "first directive must be catalog 1");
                if (directive == "catalog")
                {
                    Arity(source, location, tokens, 2);
                    if (catalog.version != 0)
                        Fail(source, location, "duplicate catalog header");
                    if (tokens[1].value != "1")
                        Fail(source, location, "unsupported catalog version '" + tokens[1].value + "'");
                    catalog.version = 1;
                }
                else if (directive == "part")
                {
                    Arity(source, location, tokens, 6);
                    Part part;
                    part.id = tokens[1].value;
                    Identifier(source, location, part.id);
                    if (!partIds.insert(part.id).second)
                        Fail(source, location, "duplicate part '" + part.id + "'");
                    part.kind = Kind(source, location, tokens[2].value);
                    part.fuel = FuelType(source, location, tokens[3].value);
                    part.capacityLitres = Number(source, location, tokens[4].value, "CAPACITY_LITRES");
                    part.burnLitresPerSimSecond =
                        Number(source, location, tokens[5].value, "BURN_LITRES_PER_SIM_SECOND");
                    ValidatePart(source, location, part);
                    catalog.parts.push_back(part);
                }
                else if (directive == "frame")
                {
                    Arity(source, location, tokens, 8);
                    Frame frame;
                    frame.id = tokens[1].value;
                    Identifier(source, location, frame.id);
                    if (!frameIds.insert(frame.id).second)
                        Fail(source, location, "duplicate frame '" + frame.id + "'");
                    frame.buildWork = Number(source, location, tokens[2].value, "BUILD_WORK");
                    frame.disassemblyWork = Number(source, location, tokens[3].value, "DISASSEMBLY_WORK");
                    frame.seats = text::Unsigned(source, location, tokens[4].value, "SEATS");
                    frame.maxMps = Number(source, location, tokens[5].value, "MAX_MPS");
                    frame.accelMps2 = Number(source, location, tokens[6].value, "ACCEL_MPS2");
                    frame.turnRadS = Number(source, location, tokens[7].value, "TURN_RAD_S");
                    ValidateFrame(source, location, frame);
                    catalog.frames.push_back(frame);
                }
                else if (directive == "slot")
                {
                    Arity(source, location, tokens, 4);
                    Slot slot;
                    slot.frameId = tokens[1].value;
                    slot.id = tokens[2].value;
                    Identifier(source, location, slot.frameId);
                    Identifier(source, location, slot.id);
                    if (!slotIds.insert(std::make_pair(slot.frameId, slot.id)).second)
                        Fail(source, location, "duplicate slot '" + slot.frameId + "/" + slot.id + "'");
                    std::set<std::string> compatibleIds;
                    const std::string& list = tokens[3].value;
                    size_t begin = 0;
                    for (;;)
                    {
                        const size_t comma = list.find(',', begin);
                        const std::string id = list.substr(begin, comma == std::string::npos ?
                                                          comma : comma - begin);
                        Identifier(source, location, id);
                        if (!compatibleIds.insert(id).second)
                            Fail(source, location, "duplicate compatible part '" + id + "'");
                        slot.compatiblePartIds.push_back(id);
                        if (comma == std::string::npos)
                            break;
                        begin = comma + 1;
                    }
                    catalog.slots.push_back(slot);
                    slotLocations.push_back(location);
                }
                else if (directive == "building")
                {
                    Arity(source, location, tokens, 3);
                    Building building;
                    building.frameId = tokens[1].value;
                    Identifier(source, location, building.frameId);
                    if (!tokens[2].quoted || tokens[2].value.empty())
                        Fail(source, location, "building requires a nonempty quoted exact FCS binding");
                    building.binding = tokens[2].value;
                    if (!buildingFrames.insert(building.frameId).second)
                        Fail(source, location, "duplicate building mapping for frame '" +
                             building.frameId + "'");
                    if (!buildingBindings.insert(building.binding).second)
                        Fail(source, location, "duplicate FCS building binding '" + building.binding + "'");
                    catalog.buildings.push_back(building);
                    buildingLocations.push_back(location);
                }
                else
                    Fail(source, location, "unsupported directive '" + directive + "'");
            }

            if (catalog.version == 0)
                throw text::Error(source, lineNumber + 1, "<end of file>", "missing catalog 1 header");
            for (size_t i = 0; i < catalog.slots.size(); ++i)
            {
                const Slot& slot = catalog.slots[i];
                if (frameIds.find(slot.frameId) == frameIds.end())
                    Fail(source, slotLocations[i], "unknown slot frame '" + slot.frameId + "'");
                for (size_t j = 0; j < slot.compatiblePartIds.size(); ++j)
                    if (partIds.find(slot.compatiblePartIds[j]) == partIds.end())
                        Fail(source, slotLocations[i], "unknown compatible part '" +
                             slot.compatiblePartIds[j] + "'");
            }
            for (size_t i = 0; i < catalog.buildings.size(); ++i)
                if (frameIds.find(catalog.buildings[i].frameId) == frameIds.end())
                    Fail(source, buildingLocations[i], "unknown building frame '" +
                         catalog.buildings[i].frameId + "'");
            return catalog;
        }

        Location ModelLocation(size_t index, const std::string& context)
        {
            Location location;
            location.line = index + 1;
            location.context = context;
            return location;
        }

        void ValidateCatalogModel(const Catalog& catalog)
        {
            const std::string source = "<catalog>";
            if (catalog.version != 1)
                Fail(source, ModelLocation(0, "catalog"), "unsupported catalog version");
            std::set<std::string> partIds;
            std::set<std::string> frameIds;
            std::set<std::pair<std::string, std::string> > slotIds;
            std::set<std::string> buildingFrames;
            std::set<std::string> buildingBindings;
            for (size_t i = 0; i < catalog.parts.size(); ++i)
            {
                const Part& part = catalog.parts[i];
                const Location location = ModelLocation(i, "part " + part.id);
                Identifier(source, location, part.id);
                if (!partIds.insert(part.id).second)
                    Fail(source, location, "duplicate part '" + part.id + "'");
                ValidatePart(source, location, part);
            }
            for (size_t i = 0; i < catalog.frames.size(); ++i)
            {
                const Frame& frame = catalog.frames[i];
                const Location location = ModelLocation(i, "frame " + frame.id);
                Identifier(source, location, frame.id);
                if (!frameIds.insert(frame.id).second)
                    Fail(source, location, "duplicate frame '" + frame.id + "'");
                ValidateFrame(source, location, frame);
            }
            for (size_t i = 0; i < catalog.slots.size(); ++i)
            {
                const Slot& slot = catalog.slots[i];
                const Location location = ModelLocation(i, "slot " + slot.frameId + "/" + slot.id);
                Identifier(source, location, slot.frameId);
                Identifier(source, location, slot.id);
                if (frameIds.find(slot.frameId) == frameIds.end())
                    Fail(source, location, "unknown slot frame '" + slot.frameId + "'");
                if (!slotIds.insert(std::make_pair(slot.frameId, slot.id)).second)
                    Fail(source, location, "duplicate slot '" + slot.frameId + "/" + slot.id + "'");
                if (slot.compatiblePartIds.empty())
                    Fail(source, location, "slot requires compatible parts");
                std::set<std::string> compatibleIds;
                for (size_t j = 0; j < slot.compatiblePartIds.size(); ++j)
                {
                    const std::string& id = slot.compatiblePartIds[j];
                    Identifier(source, location, id);
                    if (!compatibleIds.insert(id).second)
                        Fail(source, location, "duplicate compatible part '" + id + "'");
                    if (partIds.find(id) == partIds.end())
                        Fail(source, location, "unknown compatible part '" + id + "'");
                }
            }
            for (size_t i = 0; i < catalog.buildings.size(); ++i)
            {
                const Building& building = catalog.buildings[i];
                const Location location = ModelLocation(i, "building " + building.frameId);
                Identifier(source, location, building.frameId);
                if (frameIds.find(building.frameId) == frameIds.end())
                    Fail(source, location, "unknown building frame '" + building.frameId + "'");
                if (!buildingFrames.insert(building.frameId).second)
                    Fail(source, location, "duplicate building mapping");
                if (!buildingBindings.insert(building.binding).second)
                    Fail(source, location, "duplicate FCS building binding");
                if (building.binding.empty())
                    Fail(source, location, "building requires a nonempty quoted exact FCS binding");
                for (size_t j = 0; j < building.binding.size(); ++j)
                    if (static_cast<unsigned char>(building.binding[j]) < 32 ||
                        building.binding[j] == 127)
                        Fail(source, location, "control character in quoted binding");
            }
        }
    }

    Catalog::Catalog() : version(0) {}

    const Part* Catalog::FindPart(const std::string& id) const
    {
        for (size_t i = 0; i < parts.size(); ++i)
            if (parts[i].id == id)
                return &parts[i];
        return NULL;
    }

    const Frame* Catalog::FindFrame(const std::string& id) const
    {
        for (size_t i = 0; i < frames.size(); ++i)
            if (frames[i].id == id)
                return &frames[i];
        return NULL;
    }

    const Slot* Catalog::FindSlot(const std::string& frameId, const std::string& id) const
    {
        for (size_t i = 0; i < slots.size(); ++i)
            if (slots[i].frameId == frameId && slots[i].id == id)
                return &slots[i];
        return NULL;
    }

    CatalogError::CatalogError(const std::string& source, size_t line,
                               const std::string& context, const std::string& detail)
        : text::Error(source, line, context, detail) {}

    Catalog ParseCatalog(std::istream& input, const std::string& source)
    {
        try
        {
            return ParseCatalogText(input, source);
        }
        catch (const text::Error& error)
        {
            throw CatalogError(error.Source(), error.Line(), error.Context(), error.Detail());
        }
    }

    void ValidateCatalog(const Catalog& catalog)
    {
        try
        {
            ValidateCatalogModel(catalog);
        }
        catch (const text::Error& error)
        {
            throw CatalogError(error.Source(), error.Line(), error.Context(), error.Detail());
        }
    }
}
