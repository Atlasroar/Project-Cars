// SPDX-License-Identifier: GPL-3.0-only
#include "catalog.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>

void RunVehicleTests(const char* catalogPath);
void RunLifecycleTests();
void RunLifecycleMissingOriginalFixture();

namespace
{
    unsigned int assertions = 0;
    unsigned int rejectedInputs = 0;

    void Check(bool condition, const char* expression, int line)
    {
        ++assertions;
        if (!condition)
        {
            std::ostringstream message;
            message << "test line " << line << ": " << expression;
            throw std::runtime_error(message.str());
        }
    }

#define CHECK(expression) Check((expression), #expression, __LINE__)

    bool Near(double actual, double expected)
    {
        return std::fabs(actual - expected) <= 1e-12 * (1 + std::fabs(expected));
    }

    projectcars::Catalog Parse(const std::string& text)
    {
        std::istringstream input(text);
        return projectcars::ParseCatalog(input, "test.catalog");
    }

    void Reject(const std::string& text, size_t line, const char* detail)
    {
        try
        {
            Parse(text);
        }
        catch (const projectcars::CatalogError& error)
        {
            ++rejectedInputs;
            CHECK(error.Source() == "test.catalog");
            CHECK(error.Line() == line);
            CHECK(std::string(error.what()).find(detail) != std::string::npos);
            CHECK(std::string(error.what()).find("test.catalog:") != std::string::npos);
            CHECK(!error.Context().empty());
            CHECK(std::string(error.what()).find("[context: ") != std::string::npos);
            std::istringstream original(text);
            std::string originalLine;
            for (size_t i = 0; i < line; ++i)
                if (!std::getline(original, originalLine))
                    originalLine = "<end of file>";
            CHECK(error.Context() == originalLine);
            if (error.Context().find('\0') != std::string::npos)
                CHECK(std::string(error.what()).find("\\x00") != std::string::npos);
            else if (error.Context().find('\t') != std::string::npos)
                CHECK(std::string(error.what()).find("\\x09") != std::string::npos);
            else if (error.Context().find('\x7f') != std::string::npos)
                CHECK(std::string(error.what()).find("\\x7f") != std::string::npos);
            else
                CHECK(std::string(error.what()).find(error.Context()) != std::string::npos);
            return;
        }
        throw std::runtime_error("accepted invalid catalog: " + text);
    }

    const char* minimal =
        "catalog 1\n"
        "part engine_a engine gasoline 0 0.002\n"
        "frame frame_a 30 20 1 20 4 0.8\n";

    std::string Join(const std::vector<std::string>& ids)
    {
        std::string joined;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (i != 0)
                joined += ',';
            joined += ids[i];
        }
        return joined;
    }

    void RealCatalog(const char* path)
    {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input)
            throw std::runtime_error(std::string("cannot open required real catalog: ") + path);
        const projectcars::Catalog catalog = projectcars::ParseCatalog(input, path);
        CHECK(catalog.version == 1);
        CHECK(catalog.parts.size() == 11);
        CHECK(catalog.frames.size() == 3);
        CHECK(catalog.slots.size() == 32);
        CHECK(catalog.buildings.size() == 1);

        struct ExpectedPart
        {
            const char* id;
            projectcars::PartKind kind;
            projectcars::Fuel fuel;
            double capacity;
            double burn;
        };
        const ExpectedPart parts[] = {
            {"small_gas_engine", projectcars::PART_ENGINE, projectcars::FUEL_GASOLINE, 0, 0.002},
            {"small_diesel_engine", projectcars::PART_ENGINE, projectcars::FUEL_DIESEL, 0, 0.0015},
            {"road_wheel", projectcars::PART_WHEEL, projectcars::FUEL_NONE, 0, 0},
            {"offroad_wheel", projectcars::PART_WHEEL, projectcars::FUEL_NONE, 0, 0},
            {"tank_40", projectcars::PART_TANK, projectcars::FUEL_NONE, 40, 0},
            {"tank_80", projectcars::PART_TANK, projectcars::FUEL_NONE, 80, 0},
            {"light_armor", projectcars::PART_UPGRADE, projectcars::FUEL_NONE, 0, 0},
            {"heavy_armor", projectcars::PART_UPGRADE, projectcars::FUEL_NONE, 0, 0},
            {"headlights", projectcars::PART_UPGRADE, projectcars::FUEL_NONE, 0, 0},
            {"cargo_rack", projectcars::PART_UPGRADE, projectcars::FUEL_NONE, 0, 0},
            {"basic_seat", projectcars::PART_SEAT, projectcars::FUEL_NONE, 0, 0}
        };
        for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        {
            const projectcars::Part& actual = catalog.parts[i];
            CHECK(actual.id == parts[i].id);
            CHECK(actual.kind == parts[i].kind);
            CHECK(actual.fuel == parts[i].fuel);
            CHECK(Near(actual.capacityLitres, parts[i].capacity));
            CHECK(Near(actual.burnLitresPerSimSecond, parts[i].burn));
            CHECK(catalog.FindPart(parts[i].id) == &actual);
        }

        const char* frameIds[] = {"small_frame", "scout", "hauler"};
        const unsigned int seats[] = {1, 2, 4};
        for (size_t i = 0; i < 3; ++i)
        {
            const projectcars::Frame& frame = catalog.frames[i];
            CHECK(frame.id == frameIds[i]);
            CHECK(Near(frame.buildWork, i == 2 ? 60 : 30));
            CHECK(Near(frame.disassemblyWork, i == 2 ? 40 : 20));
            CHECK(frame.seats == seats[i]);
            CHECK(Near(frame.maxMps, i == 2 ? 14 : 20));
            CHECK(Near(frame.accelMps2, i == 2 ? 2 : 4));
            CHECK(Near(frame.turnRadS, i == 2 ? 0.5 : 0.8));
            CHECK(catalog.FindFrame(frameIds[i]) == &frame);
        }

        struct ExpectedSlot
        {
            const char* frame;
            const char* slot;
            const char* parts;
        };
        const ExpectedSlot slots[] = {
            {"small_frame", "seat_driver", "basic_seat"},
            {"small_frame", "engine", "small_gas_engine,small_diesel_engine"},
            {"small_frame", "tank", "tank_40"},
            {"small_frame", "wheel_fl", "road_wheel,offroad_wheel"},
            {"small_frame", "wheel_fr", "road_wheel,offroad_wheel"},
            {"small_frame", "wheel_rl", "road_wheel,offroad_wheel"},
            {"small_frame", "wheel_rr", "road_wheel,offroad_wheel"},
            {"small_frame", "armor", "light_armor"},
            {"small_frame", "headlights", "headlights"},
            {"scout", "seat_driver", "basic_seat"},
            {"scout", "seat_passenger_1", "basic_seat"},
            {"scout", "engine", "small_gas_engine,small_diesel_engine"},
            {"scout", "tank", "tank_40"},
            {"scout", "wheel_fl", "road_wheel,offroad_wheel"},
            {"scout", "wheel_fr", "road_wheel,offroad_wheel"},
            {"scout", "wheel_rl", "road_wheel,offroad_wheel"},
            {"scout", "wheel_rr", "road_wheel,offroad_wheel"},
            {"scout", "armor", "light_armor"},
            {"scout", "headlights", "headlights"},
            {"hauler", "seat_driver", "basic_seat"},
            {"hauler", "seat_passenger_1", "basic_seat"},
            {"hauler", "seat_passenger_2", "basic_seat"},
            {"hauler", "seat_passenger_3", "basic_seat"},
            {"hauler", "engine", "small_diesel_engine"},
            {"hauler", "tank", "tank_40,tank_80"},
            {"hauler", "wheel_fl", "offroad_wheel"},
            {"hauler", "wheel_fr", "offroad_wheel"},
            {"hauler", "wheel_rl", "offroad_wheel"},
            {"hauler", "wheel_rr", "offroad_wheel"},
            {"hauler", "armor", "light_armor,heavy_armor"},
            {"hauler", "headlights", "headlights"},
            {"hauler", "cargo", "cargo_rack"}
        };
        for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i)
        {
            CHECK(catalog.slots[i].frameId == slots[i].frame);
            CHECK(catalog.slots[i].id == slots[i].slot);
            CHECK(Join(catalog.slots[i].compatiblePartIds) == slots[i].parts);
        }
        CHECK(catalog.buildings[0].frameId == "small_frame");
        CHECK(catalog.buildings[0].binding == "10-Project Cars.mod");
        CHECK(catalog.FindPart("missing") == NULL);
        CHECK(catalog.FindFrame("missing") == NULL);
        std::cout << "Real catalog: 11 parts, 3 frames, 32 slots, 1 exact FCS binding\n";
    }

    void ValidGrammar()
    {
        const projectcars::Catalog catalog = Parse(
            "\xef\xbb\xbf# UTF-8 BOM and CRLF\r\n"
            "\tcatalog 1 # version\r\n"
            "slot Frame-1 driver engine_a\n"
            "building Frame-1 \"10-Exact  # binding.mod\" # literal quoted content\n"
            "frame Frame-1 0 +0. 0 2e1 .5 8E-1\n"
            "part engine_a engine gasoline -0 2e-3\n");
        CHECK(catalog.parts.size() == 1);
        CHECK(catalog.frames.size() == 1);
        CHECK(catalog.slots.size() == 1);
        CHECK(catalog.buildings[0].binding == "10-Exact  # binding.mod");
        CHECK(catalog.frames[0].seats == 0);
        CHECK(catalog.frames[0].buildWork == 0);
        CHECK(Near(catalog.frames[0].maxMps, 20));
        CHECK(Near(catalog.frames[0].accelMps2, 0.5));
        CHECK(Parse("catalog 1").version == 1);
        CHECK(Parse(minimal).frames.size() == 1);
        CHECK(Parse(std::string(minimal) +
                    "frame frame_b 1 1 0 1 1 1\n"
                    "slot frame_a engine engine_a\n"
                    "slot frame_b engine engine_a\n").slots.size() == 2);
        CHECK(Parse(std::string(minimal) +
                    "building frame_a \"literal\\binding.mod\"\n").buildings[0].binding ==
              "literal\\binding.mod");
        CHECK(Parse("catalog 1\npart shared wheel none 0 0\n"
                    "frame shared 0 0 0 1 1 1\nslot shared wheel shared\n").slots.size() == 1);
    }

    void SyntaxErrors()
    {
        Reject("", 1, "missing catalog");
        Reject("# comment\n\n", 3, "missing catalog");
        Reject("part x wheel none 0 0\n", 1, "first directive");
        Reject("catalog 2\n", 1, "unsupported catalog version");
        Reject("catalog 01\n", 1, "unsupported catalog version");
        Reject("catalog\n", 1, "expects 1 fields");
        Reject("catalog 1 extra\n", 1, "expects 1 fields");
        Reject("catalog 1\ncatalog 1\n", 2, "duplicate catalog");
        Reject("catalog 1\nvehicle x\n", 2, "unsupported directive");
        Reject("catalog 1\npart x engine gasoline 0\n", 2, "expects 5 fields");
        Reject("catalog 1\npart x engine gasoline 0 1 extra\n", 2, "expects 5 fields");
        Reject("catalog 1\nframe f 1 1 1 1 1\n", 2, "expects 7 fields");
        Reject(std::string(minimal) + "slot frame_a engine\n", 4, "expects 3 fields");
        Reject(std::string(minimal) + "building frame_a\n", 4, "expects 2 fields");
        Reject(std::string(minimal) + "building frame_a unquoted\n", 4, "quoted exact FCS");
        Reject(std::string(minimal) + "building frame_a \"\"\n", 4, "nonempty quoted");
        Reject(std::string(minimal) + "building frame_a \"unfinished\n", 4, "unterminated");
        Reject(std::string(minimal) + "building frame_a \"id\"suffix\n", 4, "characters after");
        Reject("catalog 1\npart \"x\" wheel none 0 0\n", 2, "quoted tokens");
        Reject("catalog 1\npart x\"y wheel none 0 0\n", 2, "unquoted token");
        Reject("catalog 1\npart bad.id wheel none 0 0\n", 2, "invalid identifier");
        Reject("catalog 1\npart x gearbox none 0 0\n", 2, "unsupported part KIND");
        Reject("catalog 1\npart x engine electric 0 1\n", 2, "unsupported FUEL");
        Reject("catalog 1\npart x Engine gasoline 0 1\n", 2, "unsupported part KIND");
        Reject(std::string(minimal) + "slot frame_a x engine_a,\n", 4, "empty identifier");
        Reject(std::string(minimal) + "slot frame_a x ,engine_a\n", 4, "empty identifier");
        Reject(std::string(minimal) + "slot frame_a x engine_a,,engine_a\n", 4, "empty identifier");
        Reject(std::string(minimal) + "slot frame_a x engine_a, engine_a\n", 4, "expects 3 fields");
        Reject(std::string(minimal) + "building frame_a \"bad\tbinding\"\n", 4, "control character");
        Reject(std::string(minimal) + "building frame_a \"bad\x7f\"\n", 4, "control character");
        std::string nul = "catalog 1\npart x wheel none 0";
        nul.push_back('\0');
        nul += " 0\n";
        Reject(nul, 2, "unquoted token");
    }

    void DuplicateAndReferenceErrors()
    {
        Reject(std::string(minimal) + "part engine_a engine gasoline 0 1\n", 4, "duplicate part");
        Reject(std::string(minimal) + "frame frame_a 1 1 1 1 1 1\n", 4, "duplicate frame");
        Reject(std::string(minimal) +
               "slot frame_a engine engine_a\nslot frame_a engine engine_a\n", 5, "duplicate slot");
        Reject(std::string(minimal) +
               "slot frame_a engine engine_a,engine_a\n", 4, "duplicate compatible part");
        Reject(std::string(minimal) +
               "building frame_a \"a\"\nbuilding frame_a \"b\"\n", 5, "duplicate building mapping");
        Reject(std::string(minimal) +
               "frame frame_b 1 1 1 1 1 1\nbuilding frame_a \"a\"\nbuilding frame_b \"a\"\n",
               6, "duplicate FCS building binding");
        Reject(std::string(minimal) + "slot unknown engine engine_a\n", 4, "unknown slot frame");
        Reject(std::string(minimal) + "slot frame_a engine missing\n", 4, "unknown compatible part");
        Reject(std::string(minimal) + "slot frame_a engine engine_a,missing\n", 4, "unknown compatible part");
        Reject(std::string(minimal) + "building missing \"10-Project Cars.mod\"\n", 4, "unknown building frame");
        Reject("catalog 1\nslot later x missing\nframe later 1 1 1 1 1 1\n",
               2, "unknown compatible part");
    }

    void NumericErrors()
    {
        const char* invalid[] = {
            "nan", "NaN", "inf", "-inf", "Infinity", "1e309", "-1e309", "1e-9999",
            "1.2.3", "1f", "0x10", "1,5", "--1", ".", "+", "1e", "1e+", "1e-", "1_0"
        };
        for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        {
            Reject(std::string("catalog 1\npart x engine gasoline 0 ") + invalid[i] + "\n",
                   2, "finite, representable decimal");
            Reject(std::string("catalog 1\nframe f 1 1 1 ") + invalid[i] + " 1 1\n",
                   2, "finite, representable decimal");
        }
        Reject("catalog 1\npart x tank none -1 0\n", 2, "CAPACITY_LITRES must be nonnegative");
        Reject("catalog 1\npart x engine gasoline 0 -1\n", 2, "BURN_LITRES_PER_SIM_SECOND must be nonnegative");
        Reject("catalog 1\npart x engine none 0 1\n", 2, "engine requires");
        Reject("catalog 1\npart x engine gasoline 1 1\n", 2, "engine requires");
        Reject("catalog 1\npart x engine gasoline 0 0\n", 2, "engine requires");
        Reject("catalog 1\npart x tank none 0 0\n", 2, "tank requires");
        Reject("catalog 1\npart x tank diesel 1 0\n", 2, "tank requires");
        Reject("catalog 1\npart x tank none 1 1\n", 2, "tank requires");
        Reject("catalog 1\npart x wheel gasoline 0 0\n", 2, "wheel/upgrade/seat requires");
        Reject("catalog 1\npart x upgrade none 1 0\n", 2, "wheel/upgrade/seat requires");
        Reject("catalog 1\npart x seat none 0 1\n", 2, "wheel/upgrade/seat requires");
        Reject("catalog 1\nframe f -1 1 1 1 1 1\n", 2, "BUILD_WORK must be nonnegative");
        Reject("catalog 1\nframe f 1 -1 1 1 1 1\n", 2, "DISASSEMBLY_WORK must be nonnegative");
        Reject("catalog 1\nframe f 1 1 -1 1 1 1\n", 2, "SEATS must be");
        Reject("catalog 1\nframe f 1 1 +1 1 1 1\n", 2, "SEATS must be");
        Reject("catalog 1\nframe f 1 1 1.5 1 1 1\n", 2, "SEATS must be");
        Reject("catalog 1\nframe f 1 1 1e1 1 1 1\n", 2, "SEATS must be");
        Reject("catalog 1\nframe f 1 1 4294967296 1 1 1\n", 2, "SEATS exceeds");
        Reject("catalog 1\nframe f 1 1 99999999999999999999 1 1 1\n", 2, "SEATS exceeds");
        Reject("catalog 1\nframe f 1 1 1 0 1 1\n", 2, "MAX_MPS must be positive");
        Reject("catalog 1\nframe f 1 1 1 -1 1 1\n", 2, "MAX_MPS must be positive");
        Reject("catalog 1\nframe f 1 1 1 1 0 1\n", 2, "ACCEL_MPS2 must be positive");
        Reject("catalog 1\nframe f 1 1 1 1 -1 1\n", 2, "ACCEL_MPS2 must be positive");
        Reject("catalog 1\nframe f 1 1 1 1 1 0\n", 2, "TURN_RAD_S must be positive");
        Reject("catalog 1\nframe f 1 1 1 1 1 -1\n", 2, "TURN_RAD_S must be positive");
        Reject("catalog 1\nframe f 1 1 1 1 nan 1\n", 2, "ACCEL_MPS2 must be a finite");
        Reject("catalog 1\nframe f 1 1 1 1 1 inf\n", 2, "TURN_RAD_S must be a finite");
        Reject("catalog 1\nframe f nan 1 1 1 1 1\n", 2, "BUILD_WORK must be a finite");
        Reject("catalog 1\nframe f 1 inf 1 1 1 1\n", 2, "DISASSEMBLY_WORK must be a finite");
        Reject("catalog 1\npart x tank none inf 0\n", 2, "CAPACITY_LITRES must be a finite");
    }

    void NumericBoundariesAndLocale()
    {
        const projectcars::Catalog catalog = Parse(
            "catalog 1\n"
            "part x engine diesel 0 2.2250738585072014e-308\n"
            "frame f 0 0 4294967295 1.7976931348623157e308 .5 1.\n");
        CHECK(catalog.frames[0].seats == (std::numeric_limits<unsigned int>::max)());
        CHECK(catalog.frames[0].maxMps == (std::numeric_limits<double>::max)());
        CHECK(catalog.parts[0].burnLitresPerSimSecond > 0);

        class CommaDecimal : public std::numpunct<char>
        {
        protected:
            virtual char do_decimal_point() const { return ','; }
        };
        const std::locale old = std::locale();
        std::locale::global(std::locale(std::locale::classic(), new CommaDecimal));
        try
        {
            CHECK(Near(Parse(minimal).parts[0].burnLitresPerSimSecond, 0.002));
            Reject("catalog 1\npart x engine gasoline 0 0,002\n", 2, "decimal number");
        }
        catch (...)
        {
            std::locale::global(old);
            throw;
        }
        std::locale::global(old);
    }

    void ReadFailures()
    {
        std::istringstream finalLine("catalog 1");
        finalLine.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
        CHECK(projectcars::ParseCatalog(finalLine, "final-line.catalog").version == 1);
        std::istringstream finalNewline(minimal);
        finalNewline.exceptions(std::ios::failbit | std::ios::badbit);
        CHECK(projectcars::ParseCatalog(finalNewline, "final-newline.catalog").parts.size() == 1);

        std::istringstream input(minimal);
        input.setstate(std::ios::badbit);
        try
        {
            projectcars::ParseCatalog(input, "broken.catalog");
            CHECK(false);
        }
        catch (const projectcars::CatalogError& error)
        {
            CHECK(error.Line() == 1);
            CHECK(std::string(error.what()).find("input read failed") != std::string::npos);
        }

        std::istringstream throwingInput(minimal);
        throwingInput.exceptions(std::ios::badbit);
        try
        {
            throwingInput.setstate(std::ios::badbit);
        }
        catch (const std::ios_base::failure&) {}
        try
        {
            projectcars::ParseCatalog(throwingInput, "throwing.catalog");
            CHECK(false);
        }
        catch (const projectcars::CatalogError& error)
        {
            CHECK(error.Source() == "throwing.catalog");
            CHECK(std::string(error.what()).find("input read failed") != std::string::npos);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--lifecycle-missing-original")
    {
        RunLifecycleMissingOriginalFixture();
        return 1;
    }
    if (argc != 2)
    {
        std::cerr << "Usage: CatalogTests.exe <required-real-vehicles.catalog>\n";
        return 2;
    }
    try
    {
        RealCatalog(argv[1]);
        ValidGrammar();
        SyntaxErrors();
        DuplicateAndReferenceErrors();
        NumericErrors();
        NumericBoundariesAndLocale();
        ReadFailures();
        std::cout << "PASS: " << assertions << " assertions; " << rejectedInputs
                  << " malformed catalogs rejected; VC100 x64 Release\n";
        RunVehicleTests(argv[1]);
        RunLifecycleTests();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
