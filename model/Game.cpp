#include "Game.hpp"
#include "assert.hpp"
#include <ranges>

namespace model {

Game::Game(int myIndex, int currentTick, int maxTickCount, std::vector<model::Player> players, std::unordered_map<int, model::Planet> planets, std::vector<model::FlyingWorkerGroup> flyingWorkerGroups, int maxFlyingWorkerGroups, int maxTravelDistance, int logisticsUpgrade, int productionUpgrade, int combatUpgrade, int maxBuilders, std::unordered_map<model::BuildingType, model::BuildingProperties> buildingProperties, bool specialtiesAllowed, std::optional<int> viewDistance) : myIndex(myIndex), currentTick(currentTick), maxTickCount(maxTickCount), players(players), planets(planets), flyingWorkerGroups(flyingWorkerGroups), maxFlyingWorkerGroups(maxFlyingWorkerGroups), maxTravelDistance(maxTravelDistance), logisticsUpgrade(logisticsUpgrade), productionUpgrade(productionUpgrade), combatUpgrade(combatUpgrade), maxBuilders(maxBuilders), buildingProperties(buildingProperties), specialtiesAllowed(specialtiesAllowed), viewDistance(viewDistance) { }

// Read Game from input stream
Game Game::readFrom(InputStream& stream) {
    int myIndex = stream.readInt();
    int currentTick = stream.readInt();
    int maxTickCount = stream.readInt();
    std::vector<model::Player> players = std::vector<model::Player>();
    size_t playersSize = stream.readInt();
    players.reserve(playersSize);
    for (size_t playersIndex = 0; playersIndex < playersSize; playersIndex++) {
        model::Player playersElement = model::Player::readFrom(stream);
        players.emplace_back(playersElement);
    }
    std::unordered_map<int, model::Planet> planets;
    size_t planetsSize = stream.readInt();
    planets.reserve(planetsSize);
    for (size_t planetsIndex = 0; planetsIndex < planetsSize; planetsIndex++) {
        model::Planet planetsElement = model::Planet::readFrom(stream);
        planets[planetsElement.id] = planetsElement;
    }
    std::vector<model::FlyingWorkerGroup> flyingWorkerGroups = std::vector<model::FlyingWorkerGroup>();
    size_t flyingWorkerGroupsSize = stream.readInt();
    flyingWorkerGroups.reserve(flyingWorkerGroupsSize);
    for (size_t flyingWorkerGroupsIndex = 0; flyingWorkerGroupsIndex < flyingWorkerGroupsSize; flyingWorkerGroupsIndex++) {
        model::FlyingWorkerGroup flyingWorkerGroupsElement = model::FlyingWorkerGroup::readFrom(stream);
        flyingWorkerGroups.emplace_back(flyingWorkerGroupsElement);
    }
    int maxFlyingWorkerGroups = stream.readInt();
    int maxTravelDistance = stream.readInt();
    int logisticsUpgrade = stream.readInt();
    int productionUpgrade = stream.readInt();
    int combatUpgrade = stream.readInt();
    int maxBuilders = stream.readInt();
    size_t buildingPropertiesSize = stream.readInt();
    std::unordered_map<model::BuildingType, model::BuildingProperties> buildingProperties = std::unordered_map<model::BuildingType, model::BuildingProperties>();
    buildingProperties.reserve(buildingPropertiesSize);
    for (size_t buildingPropertiesIndex = 0; buildingPropertiesIndex < buildingPropertiesSize; buildingPropertiesIndex++) {
        model::BuildingType buildingPropertiesKey = readBuildingType(stream);
        model::BuildingProperties buildingPropertiesValue = model::BuildingProperties::readFrom(stream);
        buildingProperties.emplace(std::make_pair(buildingPropertiesKey, buildingPropertiesValue));
    }
    bool specialtiesAllowed = stream.readBool();
    std::optional<int> viewDistance = std::optional<int>();
    if (stream.readBool()) {
        int viewDistanceValue = stream.readInt();
        viewDistance.emplace(viewDistanceValue);
    }
    return Game(myIndex, currentTick, maxTickCount, players, planets, flyingWorkerGroups, maxFlyingWorkerGroups, maxTravelDistance, logisticsUpgrade, productionUpgrade, combatUpgrade, maxBuilders, buildingProperties, specialtiesAllowed, viewDistance);
}

// Write Game to output stream
void Game::writeTo(OutputStream& stream) const {
    stream.write(myIndex);
    stream.write(currentTick);
    stream.write(maxTickCount);
    stream.write((int)(players.size()));
    for (const model::Player& playersElement : players) {
        playersElement.writeTo(stream);
    }
    stream.write((int)(planets.size()));
    for (const model::Planet& planetsElement : std::views::values(planets)) {
        planetsElement.writeTo(stream);
    }
    stream.write((int)(flyingWorkerGroups.size()));
    for (const model::FlyingWorkerGroup& flyingWorkerGroupsElement : flyingWorkerGroups) {
        flyingWorkerGroupsElement.writeTo(stream);
    }
    stream.write(maxFlyingWorkerGroups);
    stream.write(maxTravelDistance);
    stream.write(logisticsUpgrade);
    stream.write(productionUpgrade);
    stream.write(combatUpgrade);
    stream.write(maxBuilders);
    stream.write((int)(buildingProperties.size()));
    for (const auto& buildingPropertiesEntry : buildingProperties) {
        const model::BuildingType& buildingPropertiesKey = buildingPropertiesEntry.first;
        const model::BuildingProperties& buildingPropertiesValue = buildingPropertiesEntry.second;
        stream.write((int)(buildingPropertiesKey));
        buildingPropertiesValue.writeTo(stream);
    }
    stream.write(specialtiesAllowed);
    if (viewDistance) {
        stream.write(true);
        const int& viewDistanceValue = *viewDistance;
        stream.write(viewDistanceValue);
    } else {
        stream.write(false);
    }
}

// Get string representation of Game
std::string Game::toString() const {
    std::stringstream ss;
    ss << "Game { ";
    ss << "myIndex: ";
    ss << myIndex;
    ss << ", ";
    ss << "currentTick: ";
    ss << currentTick;
    ss << ", ";
    ss << "maxTickCount: ";
    ss << maxTickCount;
    ss << ", ";
    ss << "players: ";
    ss << "[ ";
    for (size_t playersIndex = 0; playersIndex < players.size(); playersIndex++) {
        const model::Player& playersElement = players[playersIndex];
        if (playersIndex != 0) {
            ss << ", ";
        }
        ss << playersElement.toString();
    }
    ss << " ]";
    ss << ", ";
    ss << "planets: ";
    ss << "[ ";
    for (int planetsIndex = 0; auto &planetsElement : std::views::values(planets)) {
        if (planetsIndex++ != 0) {
            ss << ", ";
        }
        ss << planetsElement.toString();
    }
    ss << " ]";
    ss << ", ";
    ss << "flyingWorkerGroups: ";
    ss << "[ ";
    for (size_t flyingWorkerGroupsIndex = 0; flyingWorkerGroupsIndex < flyingWorkerGroups.size(); flyingWorkerGroupsIndex++) {
        const model::FlyingWorkerGroup& flyingWorkerGroupsElement = flyingWorkerGroups[flyingWorkerGroupsIndex];
        if (flyingWorkerGroupsIndex != 0) {
            ss << ", ";
        }
        ss << flyingWorkerGroupsElement.toString();
    }
    ss << " ]";
    ss << ", ";
    ss << "maxFlyingWorkerGroups: ";
    ss << maxFlyingWorkerGroups;
    ss << ", ";
    ss << "maxTravelDistance: ";
    ss << maxTravelDistance;
    ss << ", ";
    ss << "logisticsUpgrade: ";
    ss << logisticsUpgrade;
    ss << ", ";
    ss << "productionUpgrade: ";
    ss << productionUpgrade;
    ss << ", ";
    ss << "combatUpgrade: ";
    ss << combatUpgrade;
    ss << ", ";
    ss << "maxBuilders: ";
    ss << maxBuilders;
    ss << ", ";
    ss << "buildingProperties: ";
    ss << "{ ";
    size_t buildingPropertiesIndex = 0;
    for (const auto& buildingPropertiesEntry : buildingProperties) {
        if (buildingPropertiesIndex != 0) {
            ss << ", ";
        }
        const model::BuildingType& buildingPropertiesKey = buildingPropertiesEntry.first;
        const model::BuildingProperties& buildingPropertiesValue = buildingPropertiesEntry.second;
        ss << buildingTypeToString(buildingPropertiesKey);
        ss << ": ";
        ss << buildingPropertiesValue.toString();
        buildingPropertiesIndex++;
    }
    ss << " }";
    ss << ", ";
    ss << "specialtiesAllowed: ";
    ss << specialtiesAllowed;
    ss << ", ";
    ss << "viewDistance: ";
    if (viewDistance) {
        const int& viewDistanceValue = *viewDistance;
        ss << viewDistanceValue;
    } else {
        ss << "none";
    }
    ss << " }";
    return ss.str();
}

void Game::extend(Game &&other) {
    currentTick = other.currentTick;
    players = std::move(other.players);
    flyingWorkerGroups = std::move(other.flyingWorkerGroups);
    for (auto &[id, p] : planets) {
        if (!other.planets.contains(id)) {
            p.clear();
        }
    }
    for (auto &[id, new_planet] : other.planets) {
        planets[id] = std::move(new_planet);
    }
    auto hash = [](const std::pair<int, int> &p) {
        return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
    };
    std::unordered_map<std::pair<int, int>, Planet*, decltype(hash)> coord_to_planet;
    for (auto &p : std::views::values(game.planets)) {
        coord_to_planet[{p.x, p.y}] = &p;
    }
    if (planetsCount == -1) {
        for (auto &[coords, planet] : coord_to_planet) {
            auto [x, y] = coords;
            x = max_coords - x;
            y = max_coords - y;
            if (coord_to_planet.contains({x, y})) {
                planetsCount = planet->id + coord_to_planet[{x, y}]->id + 1;
                break;
            }
        }
    }
    if (planetsCount > 0) {
        for (auto &[id, p] : planets) {
            if (!planets.contains(planetsCount - 1 - id)) {
                int mirror_id = planetsCount - 1 - id;
                planets[mirror_id] = p;
                planets[mirror_id].x = max_coords - p.x;
                planets[mirror_id].y = max_coords - p.y;
                planets[mirror_id].id = mirror_id;
                planets[mirror_id].clear();
            }
        }
    }
    for (auto &g : flyingWorkerGroups) {
        auto normalize = [this, &coord_to_planet](int &id) {
            if (planets.contains(id))
                return;
            else
                id = -1;
        };
        normalize(g.departurePlanet);
        normalize(g.nextPlanet);
        normalize(g.targetPlanet);
    }
}

Game game;

}