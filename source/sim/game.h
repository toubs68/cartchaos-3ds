#pragma once
// Core game simulation. Pure C++ with no 3DS dependencies so it can be
// unit-tested headlessly on a PC/phone.
#include "worldgen.h"
#include <cstdint>

namespace sim {

enum class State { MENU, SPRINT, RIDE, AIR, CRASH, WIN, LOSE };

// Input snapshot, filled by the platform each frame.
struct Input {
    bool push = false;       // sprint / pedal-push (A on 3DS)
    bool jump = false;       // jump / hop into ramp (also A, edge-triggered)
    float lean = 0.0f;       // -1 lean back .. +1 lean forward (D-pad up/down)
    float steer = 0.0f;      // -1 left .. +1 right (D-pad left/right / CPad)
    bool useDrink = false;   // B: consume a stored drink for energy
    bool start = false;      // START: open menu / restart
    bool confirm = false;    // A edge from menu also
};

struct Cart {
    float x = 0;          // distance along track
    float y = 0;          // height above road surface
    float vy = 0;         // vertical velocity
    float vx = 0;         // forward speed along track (m/s)
    float lateral = 0;    // -1..1 across the road
    float latVel = 0;     // lateral velocity
    float pitch = 0;      // nose up/down (radians)
    float roll = 0;       // lean visual
    float wheelSpin = 0;
    float rattle = 0;     // 0..1 wheel rattle intensity
};

struct EventLog {
    bool crashed = false;
    bool launched = false;     // big air off a ramp
    bool screamed = false;     // passed a car closely
    bool drank = false;
    int drinksCollected = 0;
    int carsPassed = 0;
    void clear() { crashed = launched = screamed = drank = false; }
};

class Game {
public:
    Game(uint32_t seed = 12345);

    void reset(uint32_t seed);
    // Advance one fixed timestep (dt seconds). `inp` is the input for this step.
    void step(float dt, const Input& inp);

    State state() const { return state_; }
    const World& world() const { return world_; }
    const Cart& cart() const { return cart_; }
    int energy() const { return energy_; }
    int drinks() const { return drinks_; }
    float topSpeed() const { return topSpeed_; }
    uint32_t seed() const { return seed_; }
    const EventLog& events() const { return ev_; }

    // For HUD/3DS bottom screen: distance to finish & boulder distance.
    float finishDistance() const { return world_.totalLength - cart_.x; }
    float boulderDistance() const { return boulderX_ - cart_.x; }

    // Transient crash info for fx.
    float crashTimer = 0;       // counts up after a crash
    float shake = 0;            // requested camera shake
    const char* crashReason = "";  // why the last run ended

private:
    void updateSprint(float dt, const Input& inp);
    void updateRide(float dt, const Input& inp);
    void updateAir(float dt, const Input& inp);
    void land();
    void doCrash(const char* reason);
    void spawnBoulder();

    World world_;
    Cart cart_;
    State state_ = State::MENU;
    Input prev_;

    uint32_t seed_ = 0;
    int energy_ = 100;
    int drinks_ = 0;
    float topSpeed_ = 0;
    float boulderX_ = 0;
    bool boulderActive_ = false;
    float runTime_ = 0;
    EventLog ev_;
};

}  // namespace sim
