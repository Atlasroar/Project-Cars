// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#if !defined(_MSC_VER) || _MSC_VER != 1600 || !defined(_M_X64)
#error ProjectCarsRemake requires the VC100 x64 compiler.
#endif
#if !defined(_DLL) || defined(_DEBUG)
#error ProjectCarsRemake requires the Release DLL runtime (/MD).
#endif

#include "text.h"

namespace projectcars
{
    enum PartKind
    {
        PART_ENGINE,
        PART_WHEEL,
        PART_TANK,
        PART_UPGRADE,
        PART_SEAT
    };

    enum Fuel
    {
        FUEL_NONE,
        FUEL_GASOLINE,
        FUEL_DIESEL
    };

    struct Part
    {
        std::string id;
        PartKind kind;
        Fuel fuel;
        double capacityLitres;
        double burnLitresPerSimSecond;
    };

    struct Frame
    {
        std::string id;
        double buildWork;
        double disassemblyWork;
        unsigned int seats;
        double maxMps;
        double accelMps2;
        double turnRadS;
    };

    struct Slot
    {
        std::string frameId;
        std::string id;
        std::vector<std::string> compatiblePartIds;
    };

    struct Building
    {
        std::string frameId;
        std::string binding;
    };

    struct Catalog
    {
        Catalog();

        unsigned int version;
        std::vector<Part> parts;
        std::vector<Frame> frames;
        std::vector<Slot> slots;
        std::vector<Building> buildings;

        const Part* FindPart(const std::string& id) const;
        const Frame* FindFrame(const std::string& id) const;
        const Slot* FindSlot(const std::string& frameId, const std::string& id) const;
    };

    class CatalogError : public text::Error
    {
    public:
        CatalogError(const std::string& source, size_t line,
                     const std::string& context, const std::string& detail);
    };

    Catalog ParseCatalog(std::istream& input, const std::string& source);
    void ValidateCatalog(const Catalog& catalog);
}
