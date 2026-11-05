// SPDX-License-Identifier: GPL-3.0-only
#include "catalog.h"

#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace projectcars
{
    namespace
    {
        struct Token
        {
            std::string text;
            bool quoted;
        };

        struct Location
        {
            size_t line;
            std::string context;
        };

        std::string ErrorMessage(const std::string& source, size_t line,
                                 const std::string& context, const std::string& detail)
        {
            std::ostringstream message;
            message << source << ':' << line << ": " << detail
                    << " [context: ";
            const char* hex = "0123456789abcdef";
            for (size_t i = 0; i < context.size(); ++i)
            {
                const unsigned char ch = static_cast<unsigned char>(context[i]);
                if (ch < 32 || ch == 127)
                    message << "\\x" << hex[ch >> 4] << hex[ch & 15];
                else
                    message << context[i];
            }
            message << ']';
            return message.str();
        }

        __declspec(noreturn) void Fail(const std::string& source, const Location& location,
                                      const std::string& detail)
        {
            throw CatalogError(source, location.line, location.context, detail);
        }

        bool Space(char value)
        {
            return value == ' ' || value == '\t' || value == '\r' ||
                   value == '\v' || value == '\f';
        }

        bool Digit(char value)
        {
            return value >= '0' && value <= '9';
        }

        std::vector<Token> Tokenize(const std::string& source, const Location& location)
        {
            const std::string& line = location.context;
            std::vector<Token> tokens;
            size_t pos = 0;
            while (pos < line.size())
            {
                if (Space(line[pos]))
                {
                    ++pos;
                    continue;
                }
                if (line[pos] == '#')
                    break;

                Token token;
                token.quoted = line[pos] == '"';
                if (token.quoted)
                {
                    const size_t begin = ++pos;
                    while (pos < line.size() && line[pos] != '"')
                    {
                        if (static_cast<unsigned char>(line[pos]) < 32 || line[pos] == 127)
                            Fail(source, location, "control character in quoted binding");
                        ++pos;
                    }
                    if (pos == line.size())
                        Fail(source, location, "unterminated quoted binding");
                    token.text = line.substr(begin, pos - begin);
                    ++pos;
                    if (pos < line.size() && !Space(line[pos]) && line[pos] != '#')
                        Fail(source, location, "characters after quoted binding");
                }
                else
                {
                    const size_t begin = pos;
                    while (pos < line.size() && !Space(line[pos]) && line[pos] != '#')
                    {
                        if (line[pos] == '"' || static_cast<unsigned char>(line[pos]) < 32 ||
                            line[pos] == 127)
                            Fail(source, location, "invalid character in unquoted token");
                        ++pos;
                    }
                    token.text = line.substr(begin, pos - begin);
                }
                tokens.push_back(token);
            }
            return tokens;
        }

        void Arity(const std::string& source, const Location& location,
                   const std::vector<Token>& tokens, size_t expected)
        {
            if (tokens.size() != expected)
            {
                std::ostringstream detail;
                detail << tokens[0].text << " expects " << expected - 1
                       << " fields, got " << tokens.size() - 1;
                Fail(source, location, detail.str());
            }
        }

        void Identifier(const std::string& source, const Location& location,
                        const std::string& value)
        {
            if (value.empty())
                Fail(source, location, "empty identifier");
            for (size_t i = 0; i < value.size(); ++i)
            {
                const char ch = value[i];
                if (!Digit(ch) && !(ch >= 'A' && ch <= 'Z') &&
                    !(ch >= 'a' && ch <= 'z') && ch != '_' && ch != '-')
                    Fail(source, location, "invalid identifier '" + value +
                         "'; expected ASCII letters, digits, '_' or '-'");
            }
        }

        bool Decimal(const std::string& text)
        {
            size_t pos = 0;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
                ++pos;
            size_t digits = 0;
            while (pos < text.size() && Digit(text[pos]))
            {
                ++pos;
                ++digits;
            }
            if (pos < text.size() && text[pos] == '.')
            {
                ++pos;
                while (pos < text.size() && Digit(text[pos]))
                {
                    ++pos;
                    ++digits;
                }
            }
            if (digits == 0)
                return false;
            if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E'))
            {
                ++pos;
                if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
                    ++pos;
                const size_t begin = pos;
                while (pos < text.size() && Digit(text[pos]))
                    ++pos;
                if (pos == begin)
                    return false;
            }
            return pos == text.size();
        }

        bool NonzeroMantissa(const std::string& text)
        {
            for (size_t i = 0; i < text.size() && text[i] != 'e' && text[i] != 'E'; ++i)
                if (text[i] >= '1' && text[i] <= '9')
                    return true;
            return false;
        }

        double Number(const std::string& source, const Location& location,
                      const std::string& text, const char* field)
        {
            const std::string detail = std::string(field) +
                " must be a finite, representable decimal number (got '" + text + "')";
            if (!Decimal(text))
                Fail(source, location, detail);

            std::istringstream input(text);
            input.imbue(std::locale::classic());
            double value = 0;
            input >> value;
            if (input.fail() || !input.eof() || value != value ||
                value > (std::numeric_limits<double>::max)() ||
                value < -(std::numeric_limits<double>::max)() ||
                (value == 0 && NonzeroMantissa(text)))
                Fail(source, location, detail);
            return value;
        }

        void Nonnegative(const std::string& source, const Location& location,
                         double value, const char* field)
        {
            if (value < 0)
                Fail(source, location, std::string(field) + " must be nonnegative");
        }

        void Positive(const std::string& source, const Location& location,
                      double value, const char* field)
        {
            if (value <= 0)
                Fail(source, location, std::string(field) + " must be positive");
        }

        unsigned int Seats(const std::string& source, const Location& location,
                           const std::string& text)
        {
            unsigned int value = 0;
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (!Digit(text[i]))
                    Fail(source, location, "SEATS must be an unsigned decimal integer");
                const unsigned int digit = static_cast<unsigned int>(text[i] - '0');
                if (value > ((std::numeric_limits<unsigned int>::max)() - digit) / 10)
                    Fail(source, location, "SEATS exceeds the 32-bit unsigned integer range");
                value = value * 10 + digit;
            }
            return value;
        }

        PartKind Kind(const std::string& source, const Location& location,
                      const std::string& text)
        {
            if (text == "engine") return PART_ENGINE;
            if (text == "wheel") return PART_WHEEL;
            if (text == "tank") return PART_TANK;
            if (text == "upgrade") return PART_UPGRADE;
            if (text == "seat") return PART_SEAT;
            Fail(source, location, "unsupported part KIND '" + text + "'");
        }

        Fuel FuelType(const std::string& source, const Location& location,
                      const std::string& text)
        {
            if (text == "none") return FUEL_NONE;
            if (text == "gasoline") return FUEL_GASOLINE;
            if (text == "diesel") return FUEL_DIESEL;
            Fail(source, location, "unsupported FUEL '" + text + "'");
        }

        void ValidatePart(const std::string& source, const Location& location,
                          const Part& part)
        {
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

    CatalogError::CatalogError(const std::string& source, size_t line,
                               const std::string& context, const std::string& detail)
        : std::runtime_error(ErrorMessage(source, line, context, detail)),
          source_(source), line_(line), context_(context) {}

    const std::string& CatalogError::Source() const { return source_; }
    size_t CatalogError::Line() const { return line_; }
    const std::string& CatalogError::Context() const { return context_; }

    Catalog ParseCatalog(std::istream& input, const std::string& source)
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

        for (;;)
        {
            std::string line;
            try
            {
                if (!std::getline(input, line))
                {
                    if (input.bad() || !input.eof())
                        throw CatalogError(source, lineNumber + 1, "<input>",
                                           "input read failed");
                    break;
                }
            }
            catch (const std::ios_base::failure& error)
            {
                if (!input.eof() || input.bad())
                    throw CatalogError(source, lineNumber + 1, "<input>",
                                       std::string("input read failed: ") + error.what());
                if (line.empty())
                    break;
            }

            ++lineNumber;
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf)
                line.erase(0, 3);
            Location location;
            location.line = lineNumber;
            location.context = line;
            const std::vector<Token> tokens = Tokenize(source, location);
            if (tokens.empty())
                continue;
            for (size_t i = 0; i < tokens.size(); ++i)
                if (tokens[i].quoted && !(tokens[0].text == "building" && i == 2))
                    Fail(source, location, "quoted tokens are only allowed for a building binding");

            const std::string& directive = tokens[0].text;
            if (catalog.version == 0 && directive != "catalog")
                Fail(source, location, "first directive must be catalog 1");
            if (directive == "catalog")
            {
                Arity(source, location, tokens, 2);
                if (catalog.version != 0)
                    Fail(source, location, "duplicate catalog header");
                if (tokens[1].text != "1")
                    Fail(source, location, "unsupported catalog version '" + tokens[1].text + "'");
                catalog.version = 1;
            }
            else if (directive == "part")
            {
                Arity(source, location, tokens, 6);
                Part part;
                part.id = tokens[1].text;
                Identifier(source, location, part.id);
                if (!partIds.insert(part.id).second)
                    Fail(source, location, "duplicate part '" + part.id + "'");
                part.kind = Kind(source, location, tokens[2].text);
                part.fuel = FuelType(source, location, tokens[3].text);
                part.capacityLitres = Number(source, location, tokens[4].text, "CAPACITY_LITRES");
                part.burnLitresPerSimSecond =
                    Number(source, location, tokens[5].text, "BURN_LITRES_PER_SIM_SECOND");
                ValidatePart(source, location, part);
                catalog.parts.push_back(part);
            }
            else if (directive == "frame")
            {
                Arity(source, location, tokens, 8);
                Frame frame;
                frame.id = tokens[1].text;
                Identifier(source, location, frame.id);
                if (!frameIds.insert(frame.id).second)
                    Fail(source, location, "duplicate frame '" + frame.id + "'");
                frame.buildWork = Number(source, location, tokens[2].text, "BUILD_WORK");
                frame.disassemblyWork = Number(source, location, tokens[3].text, "DISASSEMBLY_WORK");
                frame.seats = Seats(source, location, tokens[4].text);
                frame.maxMps = Number(source, location, tokens[5].text, "MAX_MPS");
                frame.accelMps2 = Number(source, location, tokens[6].text, "ACCEL_MPS2");
                frame.turnRadS = Number(source, location, tokens[7].text, "TURN_RAD_S");
                Nonnegative(source, location, frame.buildWork, "BUILD_WORK");
                Nonnegative(source, location, frame.disassemblyWork, "DISASSEMBLY_WORK");
                Positive(source, location, frame.maxMps, "MAX_MPS");
                Positive(source, location, frame.accelMps2, "ACCEL_MPS2");
                Positive(source, location, frame.turnRadS, "TURN_RAD_S");
                catalog.frames.push_back(frame);
            }
            else if (directive == "slot")
            {
                Arity(source, location, tokens, 4);
                Slot slot;
                slot.frameId = tokens[1].text;
                slot.id = tokens[2].text;
                Identifier(source, location, slot.frameId);
                Identifier(source, location, slot.id);
                if (!slotIds.insert(std::make_pair(slot.frameId, slot.id)).second)
                    Fail(source, location, "duplicate slot '" + slot.frameId + "/" + slot.id + "'");
                std::set<std::string> compatibleIds;
                const std::string& list = tokens[3].text;
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
                building.frameId = tokens[1].text;
                Identifier(source, location, building.frameId);
                if (!tokens[2].quoted || tokens[2].text.empty())
                    Fail(source, location, "building requires a nonempty quoted exact FCS binding");
                building.binding = tokens[2].text;
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
            throw CatalogError(source, lineNumber + 1, "<end of file>", "missing catalog 1 header");
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
}
