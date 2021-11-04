#ifndef __MODEL_SPECIALTY_HPP__
#define __MODEL_SPECIALTY_HPP__

#include "Stream.hpp"
#include "utils/stringable_enum.hpp"
#include <cinttypes>

namespace model {

// Player's specialty
ENUM_CLASS(Specialty, int8_t,
    // Logistics. Increased travel distance
    LOGISTICS = 0,
    // Production. Increased work speed
    PRODUCTION = 1,
    // Combat. Increased damage
    COMBAT = 2
);

// Read Specialty from input stream
Specialty readSpecialty(InputStream& stream);

// Get string representation of Specialty
std::string specialtyToString(Specialty value);

using namespace stringable_enum;

}

#endif