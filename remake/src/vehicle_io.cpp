// SPDX-License-Identifier: GPL-3.0-only
#include "vehicle_io.h"

#include <cstring>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace projectcars
{
    namespace
    {
        const char* format = "projectcars-remake-vehicle";
        typedef char Binary64Required[(sizeof(double) == 8 &&
            std::numeric_limits<double>::is_iec559) ? 1 : -1];

        std::string EncodeLitres(double litres)
        {
            unsigned __int64 bits = 0;
            std::memcpy(&bits, &litres, sizeof(bits));
            std::string value = "binary64:";
            const char* hex = "0123456789abcdef";
            for (int shift = 60; shift >= 0; shift -= 4)
                value += hex[static_cast<size_t>((bits >> shift) & 15)];
            return value;
        }

        double DecodeLitres(const std::string& source, const text::Location& location,
                            const std::string& value)
        {
            const char* detail =
                "FUEL_LITRES must be binary64: followed by 16 lowercase hexadecimal digits";
            if (value.size() != 25 || value.compare(0, 9, "binary64:") != 0)
                text::Fail(source, location, detail);
            unsigned __int64 bits = 0;
            for (size_t i = 9; i < value.size(); ++i)
            {
                unsigned int digit = 0;
                if (value[i] >= '0' && value[i] <= '9')
                    digit = static_cast<unsigned int>(value[i] - '0');
                else if (value[i] >= 'a' && value[i] <= 'f')
                    digit = static_cast<unsigned int>(value[i] - 'a' + 10);
                else
                    text::Fail(source, location, detail);
                bits = (bits << 4) | digit;
            }
            double litres = 0;
            std::memcpy(&litres, &bits, sizeof(litres));
            if (litres == 0 && bits != 0)
                text::Fail(source, location, "negative zero is not canonical fuel volume");
            return litres;
        }

        Fuel ParseFuel(const std::string& source, const text::Location& location,
                       const std::string& value)
        {
            if (value == "none") return FUEL_NONE;
            if (value == "gasoline") return FUEL_GASOLINE;
            if (value == "diesel") return FUEL_DIESEL;
            text::Fail(source, location, "unsupported fuel type '" + value + "'");
        }

        const char* FuelName(Fuel type)
        {
            if (type == FUEL_NONE) return "none";
            if (type == FUEL_GASOLINE) return "gasoline";
            if (type == FUEL_DIESEL) return "diesel";
            throw VehicleError(VEHICLE_INVALID_FUEL, "unsupported fuel type during serialization");
        }
    }

    VehicleSaveError::VehicleSaveError(const std::string& source, size_t line,
                                     const std::string& context, const std::string& detail)
        : text::Error(source, line, context, detail) {}

    Vehicle ParseVehicle(std::istream& input, const std::string& source, const Catalog& catalog)
    {
        ValidateCatalog(catalog);
        try
        {
            struct Installation
            {
                std::string slot;
                std::string part;
                text::Location location;
            };
            bool header = false;
            bool frame = false;
            bool fuel = false;
            bool end = false;
            std::string frameId;
            FuelAmount fuelState;
            text::Location frameLocation;
            text::Location fuelLocation;
            std::vector<Installation> installations;
            std::set<std::string> slots;
            size_t lineNumber = 0;
            text::Location location;
            while (text::ReadLine(input, source, lineNumber, location))
            {
                const std::vector<text::Token> tokens = text::Tokenize(source, location);
                if (tokens.empty())
                    continue;
                if (end)
                    text::Fail(source, location, "content after end of vehicle state");
                for (size_t i = 0; i < tokens.size(); ++i)
                    if (tokens[i].quoted)
                        text::Fail(source, location, "quoted tokens are not allowed in vehicle state");
                const std::string& directive = tokens[0].value;
                if (!header && directive != format)
                    text::Fail(source, location, "first directive must be projectcars-remake-vehicle 1");
                if (directive == format)
                {
                    text::Arity(source, location, tokens, 2);
                    if (header)
                        text::Fail(source, location, "duplicate vehicle header");
                    if (tokens[1].value != "1")
                        text::Fail(source, location, "unsupported vehicle version '" + tokens[1].value + "'");
                    header = true;
                }
                else if (directive == "frame")
                {
                    text::Arity(source, location, tokens, 2);
                    if (frame)
                        text::Fail(source, location, "duplicate vehicle frame");
                    text::Identifier(source, location, tokens[1].value);
                    frameId = tokens[1].value;
                    frameLocation = location;
                    frame = true;
                }
                else if (directive == "installed")
                {
                    text::Arity(source, location, tokens, 3);
                    Installation installation;
                    installation.slot = tokens[1].value;
                    installation.part = tokens[2].value;
                    installation.location = location;
                    text::Identifier(source, location, installation.slot);
                    text::Identifier(source, location, installation.part);
                    if (!slots.insert(installation.slot).second)
                        text::Fail(source, location, "duplicate installed slot '" + installation.slot + "'");
                    installations.push_back(installation);
                }
                else if (directive == "fuel")
                {
                    text::Arity(source, location, tokens, 3);
                    if (fuel)
                        text::Fail(source, location, "duplicate vehicle fuel");
                    fuelState.type = ParseFuel(source, location, tokens[1].value);
                    fuelState.litres = DecodeLitres(source, location, tokens[2].value);
                    fuelLocation = location;
                    fuel = true;
                }
                else if (directive == "end")
                {
                    text::Arity(source, location, tokens, 1);
                    end = true;
                }
                else
                    text::Fail(source, location, "unsupported vehicle directive '" + directive + "'");
            }
            const text::Location eof = {lineNumber + 1, "<end of file>"};
            if (!header)
                text::Fail(source, eof, "missing projectcars-remake-vehicle 1 header");
            if (!frame)
                text::Fail(source, eof, "missing vehicle frame");
            if (!fuel)
                text::Fail(source, eof, "missing vehicle fuel");
            if (!end)
                text::Fail(source, eof, "missing end of vehicle state");
            if (!catalog.FindFrame(frameId))
                text::Fail(source, frameLocation, "unknown frame '" + frameId + "'");

            Vehicle result(catalog, frameId);
            for (size_t i = 0; i < installations.size(); ++i)
            {
                const Installation& installation = installations[i];
                try
                {
                    result.ValidateInstallation(installation.slot, installation.part);
                }
                catch (const VehicleError& error)
                {
                    text::Fail(source, installation.location, error.what());
                }
                result.installed_.insert(std::make_pair(installation.slot, installation.part));
            }
            try
            {
                result.ValidateFuel(result.installed_, fuelState);
            }
            catch (const VehicleError& error)
            {
                text::Fail(source, fuelLocation, error.what());
            }
            result.fuel_ = fuelState.litres > 0 ? fuelState : FuelAmount();
            return result;
        }
        catch (const text::Error& error)
        {
            throw VehicleSaveError(error.Source(), error.Line(), error.Context(), error.Detail());
        }
    }

    void WriteVehicle(std::ostream& output, const Vehicle& vehicle, const std::string& destination)
    {
        std::ostringstream buffer;
        buffer.imbue(std::locale::classic());
        buffer << format << " 1\nframe " << vehicle.FrameId() << '\n';
        const InstalledParts& installed = vehicle.Installed();
        for (InstalledParts::const_iterator it = installed.begin(); it != installed.end(); ++it)
            buffer << "installed " << it->first << ' ' << it->second << '\n';
        const FuelAmount fuel = vehicle.FuelState();
        buffer << "fuel " << FuelName(fuel.type) << ' ' << EncodeLitres(fuel.litres) << "\nend\n";
        if (!buffer)
            throw VehicleSaveError(destination, 0, "<output>", "vehicle serialization failed");
        const std::string bytes = buffer.str();
        if (bytes.size() > static_cast<size_t>((std::numeric_limits<std::streamsize>::max)()))
            throw VehicleSaveError(destination, 0, "<output>", "vehicle serialization exceeds stream size");
        try
        {
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.flush();
        }
        catch (const std::ios_base::failure& error)
        {
            throw VehicleSaveError(destination, 0, "<output>",
                                   std::string("output write failed: ") + error.what());
        }
        if (!output)
            throw VehicleSaveError(destination, 0, "<output>", "output write failed");
    }
}
