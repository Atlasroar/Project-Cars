// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "vehicle.h"

#include <ostream>

namespace projectcars
{
    class VehicleSaveError : public text::Error
    {
    public:
        VehicleSaveError(const std::string& source, size_t line,
                         const std::string& context, const std::string& detail);
    };

    Vehicle ParseVehicle(std::istream& input, const std::string& source, const Catalog& catalog);
    void WriteVehicle(std::ostream& output, const Vehicle& vehicle, const std::string& destination);
}
