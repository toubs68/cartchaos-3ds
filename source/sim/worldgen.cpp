#include "worldgen.h"
#include <cstring>

namespace sim {

Biome World::biomeAt(float x) const {
    float t = x / totalLength;
    if (t < 0.30f) return Biome::CITY;
    if (t < 0.55f) return Biome::INDUSTRIAL;
    if (t < 0.80f) return Biome::COUNTRY;
    return Biome::CLIFF;
}

float World::roadHeight(float x) const {
    if (x < 0) x = 0;
    int i = (int)(x / GRID_DX);
    if (i >= GRID_N - 1) i = GRID_N - 2;
    float f = (x - i * GRID_DX) / GRID_DX;
    return lerp(height[i], height[i + 1], f);
}

float World::roadGrade(float x) const {
    float d = 0.75f;
    return (roadHeight(x + d) - roadHeight(x - d)) / (2.0f * d);
}

void World::generate(uint32_t seed) {
    Rng rng(seed);

    // ---- Height profile -------------------------------------------------
    // Flat run-up, then a big rolling downhill toward the finish.
    std::memset(height, 0, sizeof(height));
    float h = 0.0f;
    const int startIdx = (int)(startSprint / GRID_DX);
    // Smooth noise via summed random walk with restoring force toward a
    // target descent so the track reliably goes downhill overall.
    float targetSlope = -0.085f;   // average ~8.5% downhill
    float slope = 0.0f;
    for (int i = 0; i < GRID_N; ++i) {
        if (i < startIdx) {
            // gentle lead-in
            slope = approach(slope, -0.01f, 1.0f, 0.02f);
        } else {
            float tlen = (i - startIdx) * GRID_DX;
            // steeper near the end (cliffs = faster descents)
            float speedup = 0.02f * (tlen / (totalLength - startSprint));
            targetSlope = -0.085f - speedup;
            // random wobble
            slope += rng.range(-0.006f, 0.006f);
            slope = approach(slope, targetSlope, 0.6f, 1.0f);
            slope = clamp(slope, -0.28f, 0.06f);  // no crazy uphills
        }
        h += slope * GRID_DX;
        height[i] = h;
    }
    // Normalize so it starts at 0
    float base = height[startIdx];
    for (int i = 0; i < GRID_N; ++i) height[i] -= base;

    // ---- Obstacles ------------------------------------------------------
    // BLOCK-ONE-OF-THREE guarantee: at each cluster we block exactly ONE lane,
    // leaving the other two permanently clear at that x. Because a cluster is a
    // single x-slice, there is ALWAYS at least one unobstructed lane within
    // steering reach, so the course is always passable (never a 3-lane wall).
    // Spacing widens on the faster descents so the player has time to react.
    obstacles.clear();
    const float lanes[3] = { -0.7f, 0.0f, 0.7f };
    int prevLane = -1;
    for (float x = startSprint + 30.0f; x < totalLength - 40.0f; ) {
        // ~34m steps early (city), opening to ~52m on the steep descents.
        float t = (x - startSprint) / (totalLength - startSprint);
        float obsStep = 34.0f + 18.0f * t;
        int lane = rng.range_i(0, 3);
        if (lane == prevLane && rng.next_f() < 0.7f) lane = (lane + 1) % 3; // avoid monotone walls
        prevLane = lane;
        Obstacle o;
        o.x = x + rng.range(-4.0f, 4.0f);
        o.lateral = lanes[lane];
        o.halfW = rng.range(0.18f, 0.26f);
        o.kind = (ObstacleKind)rng.range_i(0, (int)ObstacleKind::GUARDRAIL + 1);
        o.onGround = true;
        obstacles.push_back(o);
        x += obsStep;
    }

    // ---- Ramps / launch jumps ------------------------------------------
    ramps.clear();
    for (float x = startSprint + 60.0f; x < totalLength - 100.0f; x += rng.range(120.0f, 220.0f)) {
        Ramp r;
        r.x = x;
        r.length = rng.range(8.0f, 16.0f);
        r.rise = rng.range(1.2f, 3.0f);
        ramps.push_back(r);
    }

    // ---- Pickups (drinks) ----------------------------------------------
    pickups.clear();
    for (float x = startSprint + 20.0f; x < totalLength - 30.0f; x += rng.range(40.0f, 75.0f)) {
        Pickup p;
        p.x = x;
        p.lateral = rng.range(-0.8f, 0.8f);
        p.y = rng.range(0.4f, 1.1f);
        p.taken = false;
        p.energy = rng.range_i(18, 34);
        pickups.push_back(p);
    }

    // ---- Traffic --------------------------------------------------------
    // Cars occupy discrete lanes to guarantee dodging room; never two cars
    // blocking the same stretch of road. Spacing also widens on the fastest
    // descents so the player can find a gap at 30+ m/s.
    cars.clear();
    const float carLanes[3] = { -0.75f, 0.0f, 0.75f };
    for (float x = startSprint + 90.0f; x < totalLength - 30.0f; ) {
        float t = (x - startSprint) / (totalLength - startSprint);
        float carStep = 42.0f + 22.0f * t;
        Car c;
        c.x = x + rng.range(-6.0f, 6.0f);
        c.lateral = carLanes[rng.range_i(0, 3)];
        bool oncoming = rng.next_f() < 0.5f;
        c.speed = (oncoming ? -1.0f : 1.0f) * rng.range(5.0f, 9.0f);
        c.halfLen = rng.range(1.6f, 2.2f);
        cars.push_back(c);
        x += carStep;
    }
}

}  // namespace sim
