#ifndef __MODEL_GAME_HPP__
#define __MODEL_GAME_HPP__

#include "Stream.hpp"
#include "model/Building.hpp"
#include "model/BuildingProperties.hpp"
#include "model/BuildingType.hpp"
#include "model/FlyingWorkerGroup.hpp"
#include "model/Planet.hpp"
#include "model/Player.hpp"
#include "model/Resource.hpp"
#include "model/Specialty.hpp"
#include "model/WorkerGroup.hpp"
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace model {

constexpr int max_coords = 29;
constexpr int max_planet_index = 500;

// Current game's state
class Game {
public:
    int planetsCount = -1;
    // Your player's index
    int myIndex;
    // Current tick number
    int currentTick;
    // Max number of ticks in the game
    int maxTickCount;
    // List of players
    std::vector<model::Player> players;
    // List of planets
    std::unordered_map<int, model::Planet> planets;
    // List of flying worker groups
    std::vector<model::FlyingWorkerGroup> flyingWorkerGroups;
    // Max number of flying worker groups for one player
    int maxFlyingWorkerGroups;
    // Max distance of direct travel between planets
    int maxTravelDistance;
    // Additional distance of direct travel between planets for player with Logistics specialty
    int logisticsUpgrade;
    // Additional work done by player with Production specialty (in percent)
    int productionUpgrade;
    // Additional strength workers for player with Combat specialty (in percent)
    int combatUpgrade;
    // Max number of workers performing building on one planet
    int maxBuilders;
    // Properties of every building type
    std::unordered_map<model::BuildingType, model::BuildingProperties> buildingProperties;
    // Whether choosing specialties is allowed
    bool specialtiesAllowed;
    // View distance
    std::optional<int> viewDistance;

    Game() = default;

    Game(int myIndex, int currentTick, int maxTickCount, std::vector<model::Player> players, std::unordered_map<int, model::Planet> planets, std::vector<model::FlyingWorkerGroup> flyingWorkerGroups, int maxFlyingWorkerGroups, int maxTravelDistance, int logisticsUpgrade, int productionUpgrade, int combatUpgrade, int maxBuilders, std::unordered_map<model::BuildingType, model::BuildingProperties> buildingProperties, bool specialtiesAllowed, std::optional<int> viewDistance);

    // Read Game from input stream
    static Game readFrom(InputStream& stream);

    // Write Game to output stream
    void writeTo(OutputStream& stream) const;

    // Get string representation of Game
    std::string toString() const;

    void extend(Game &&other);
};

extern Game game;

}

#endif