#pragma once
// Procedural world generation: a long downhill course built from a noisy
// height profile plus scattered obstacles, ramps, pickups, and traffic.
#include "math.h"
#include <vector>

namespace sim {

enum class Biome { CITY, INDUSTRIAL, COUNTRY, CLIFF };

// A static obstacle that ends the run on collision (unless airborne high enough).
enum class ObstacleKind {
    ROCK, CONE, POLE, BARRIER, TREE, BUILDING_EDGE, GUARDRAIL
};

struct Obstacle {
    float x;            // distance along track
    float lateral;      // -1 (left curb) .. +1 (right curb)
    float halfW;        // half width for collision
    ObstacleKind kind;
    bool onGround;      // must be hit if cart y < clearHeight
};

struct Ramp {
    float x;            // start of ramp
    float length;       // length of ramp up-slope
    float rise;         // how much it lifts the cart
};

struct Pickup {
    float x;
    float lateral;
    float y;            // height above road (drinks usually at rider height)
    bool taken;
    int energy;         // energy restored
};

struct Car {
    float x;            // position along track
    float lateral;      // lane position, -0.85..0.85
    float speed;        // signed: + same dir as player, - oncoming
    float halfLen;
};

struct World {
    float totalLength = 6000.0f;   // finish line distance
    float startSprint = 120.0f;    // flat-ish run-up before the downhill
    // Height profile sampled on a coarse grid for speed.
    static constexpr int GRID_N = 4096;
    static constexpr float GRID_DX = 1.5f; // meters per sample
    float height[GRID_N];

    Biome biomeAt(float x) const;
    float roadHeight(float x) const;      // ground height (meters above 0)
    float roadGrade(float x) const;       // slope (dy/dx); negative = downhill

    std::vector<Obstacle> obstacles;
    std::vector<Ramp> ramps;
    std::vector<Pickup> pickups;
    std::vector<Car> cars;

    // Build the whole course from a seed.
    void generate(uint32_t seed);
};

}  // namespace sim
