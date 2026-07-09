#pragma once
// Small, dependency-free math helpers used by the simulation.
#include <cmath>

namespace sim {

inline constexpr float PI = 3.14159265358979323846f;

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Smoothly approach target by factor (frame-rate independent enough for fixed dt).
inline float approach(float cur, float target, float rate, float dt) {
    float k = 1.0f - std::exp(-rate * dt);
    return cur + (target - cur) * k;
}

// Deterministic 32-bit xorshift PRNG so the world is reproducible per seed.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 0x9e3779b9u) : s(seed ? seed : 1u) {}

    uint32_t next_u32() {
        uint32_t x = s;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s = x;
        return x;
    }

    // Float in [0,1)
    float next_f() { return (next_u32() & 0x00ffffffu) / 16777216.0f; }

    // Float in [lo,hi)
    float range(float lo, float hi) { return lo + (hi - lo) * next_f(); }

    int range_i(int lo, int hi) { return lo + (int)(next_f() * (hi - lo)); }
};

}  // namespace sim
