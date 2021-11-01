#ifndef __MODEL_RESOURCE_HPP__
#define __MODEL_RESOURCE_HPP__

#include "Stream.hpp"
#include "utils/stringable_enum.hpp"

namespace model {

// Resource type
ENUM_CLASS(Resource, int8_t,
    // Stone
    STONE = 0,
    // Ore
    ORE = 1,
    // Sand
    SAND = 2,
    // Organics
    ORGANICS = 3,
    // Metal
    METAL = 4,
    // Silicon
    SILICON = 5,
    // Plastic
    PLASTIC = 6,
    // Chip
    CHIP = 7,
    // Accumulator
    ACCUMULATOR = 8
);

// Read Resource from input stream
Resource readResource(InputStream& stream);

// Get string representation of Resource
std::string resourceToString(Resource value);

using namespace stringable_enum;

constexpr size_t resourceTypesCount = (int) *std::max_element(EnumValues<Resource>::list.begin(), EnumValues<Resource>::list.end()) + 1;

}

#endif