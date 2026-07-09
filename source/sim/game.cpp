#include "game.h"
#include <cmath>

namespace sim {

// lean forward (>0) accelerates, lean back (<0) brakes slightly.
static inline float leanToAccel(float lean) { return lean * 3.0f; }

Game::Game(uint32_t seed) { reset(seed); }

void Game::reset(uint32_t seed) {
    seed_ = seed;
    world_.generate(seed);
    cart_ = Cart{};
    state_ = State::MENU;
    energy_ = 100;
    drinks_ = 0;
    topSpeed_ = 0;
    runTime_ = 0;
    crashTimer = 0;
    shake = 0;
    prev_ = Input{};
    boulderActive_ = false;
    boulderX_ = 0;
    ev_ = EventLog{};
}

void Game::spawnBoulder() {
    boulderActive_ = true;
    // Boulder starts behind the player and relentlessly chases.
    boulderX_ = cart_.x - 90.0f;
}

void Game::doCrash(const char* reason) {
    state_ = State::CRASH;
    ev_.crashed = true;
    crashTimer = 0;
    shake = 1.0f;
    crashReason = reason;
}

void Game::land() {
    // Land from air -> ride. A clean landing at speed is fine; only a steeply
    // mis-rotated cart (nose pitched way off) crashes. Players control pitch
    // in the air via lean, so bad landings are recoverable mistakes.
    float ang = std::fabs(cart_.pitch);
    float sp = cart_.vx;
    if (ang > 0.9f || (ang > 0.6f && sp > 30.0f)) {
        doCrash("bad landing");
        return;
    }
    // settle
    cart_.y = 0;
    cart_.vy = 0;
    cart_.pitch = approach(cart_.pitch, world_.roadGrade(cart_.x), 8.0f, 0.016f);
    state_ = State::RIDE;
}

void Game::updateSprint(float dt, const Input& inp) {
    // Player sprints behind the cart building vx. No lateral control yet.
    if (inp.push) {
        cart_.vx += 7.0f * dt;       // sprint acceleration
    }
    cart_.vx = clamp(cart_.vx, 0.0f, 9.0f * 1.4f);
    cart_.x += cart_.vx * dt;
    cart_.wheelSpin += cart_.vx * dt * 1.2f;
    cart_.rattle = clamp(cart_.vx / 12.0f, 0.0f, 1.0f);
    cart_.pitch = world_.roadGrade(cart_.x);

    // Cross the start line: jump in.
    if (cart_.x >= world_.startSprint || inp.jump) {
        if (cart_.vx > 2.0f) {
            state_ = State::RIDE;
            spawnBoulder();
        }
    }
}

void Game::updateRide(float dt, const Input& inp) {
    runTime_ += dt;
    World& w = world_;

    // Slope acceleration from gravity along the road. roadGrade is dy/dx; it is
    // NEGATIVE on a downhill (height drops as we advance). The component of
    // gravity along our direction of travel is therefore -g*grade (positive ->
    // we speed up going downhill). Leaning forward (positive) adds accel; lean
    // back (negative) brakes a little.
    float grade = w.roadGrade(cart_.x);      // negative = downhill
    float g = 9.81f;
    float a = -g * grade;
    a += leanToAccel(inp.lean);              // helper below
    // rolling resistance / drag
    a -= 0.9f * cart_.vx * 0.02f;            // light drag
    a -= 0.6f;                               // bearing friction
    if (inp.push) {
        // pedal-push when energy available (push the cart on gentle slopes)
        if (energy_ > 0) {
            a += 4.2f;
            energy_ -= (int)(18.0f * dt);
            if (energy_ < 0) energy_ = 0;
        }
    }

    cart_.vx += a * dt;
    if (cart_.vx < 0) cart_.vx = 0;
    if (cart_.vx > 40.0f) cart_.vx = 40.0f;

    // Lateral position controller (body-weight shift). We drive the lateral
    // position toward a target derived from input using a critically-damped
    // P-controller, so the cart settles precisely in a lane instead of
    // overshooting off the road. Enough authority to cross the road quickly.
    float targetLat = clamp(cart_.lateral + inp.steer * 0.9f, -0.95f, 0.95f);
    float err = targetLat - cart_.lateral;
    float maxLat = 7.5f;                     // max lateral speed (m/s)
    float desiredVel = clamp(err * 6.0f, -maxLat, maxLat);
    cart_.latVel = approach(cart_.latVel, desiredVel, 14.0f, dt);
    cart_.lateral += cart_.latVel * dt;
    cart_.lateral = clamp(cart_.lateral, -1.02f, 1.02f);

    // Roll visual from steer.
    cart_.roll = approach(cart_.roll, -inp.steer * 0.4f + inp.lean * 0.2f, 8.0f, dt);

    // Move forward; follow terrain height.
    cart_.x += cart_.vx * dt;
    cart_.wheelSpin += cart_.vx * dt * 1.2f;
    cart_.rattle = clamp(cart_.vx / 14.0f, 0.0f, 1.0f);
    float gh = w.roadHeight(cart_.x);
    cart_.pitch = approach(cart_.pitch, grade, 6.0f, dt);
    cart_.y = 0; // on ground relative to road

    // ---- Ramps -> launch into AIR --------------------------------------
    for (auto& r : w.ramps) {
        if (cart_.x > r.x && cart_.x < r.x + r.length && cart_.vx > 5.0f) {
            // ramp gives upward velocity proportional to speed on the ramp
            cart_.vy = (cart_.vx * 0.45f) * (r.rise / 1.8f);
            cart_.y = 0.05f;
            state_ = State::AIR;
            ev_.launched = true;
            shake = 0.4f;
            break;
        }
    }

    // ---- Pickups --------------------------------------------------------
    for (auto& p : w.pickups) {
        if (!p.taken && std::fabs(p.x - cart_.x) < 1.2f &&
            std::fabs(p.lateral - cart_.lateral) < 0.7f &&
            std::fabs(p.y - (cart_.y + 0.8f)) < 1.4f) {
            p.taken = true;
            drinks_++;
            energy_ = clamp(energy_ + p.energy, 0, 120);
            ev_.drank = true;
            ev_.drinksCollected++;
        }
    }
    // B key consumes a drink for instant energy.
    if (inp.useDrink && drinks_ > 0 && energy_ < 100) {
        drinks_--;
        energy_ = clamp(energy_ + 30, 0, 120);
        ev_.drank = true;
    }

    // ---- Obstacles ------------------------------------------------------
    for (auto& o : w.obstacles) {
        if (std::fabs(o.x - cart_.x) < (o.halfW + 0.5f) &&
            std::fabs(o.lateral - cart_.lateral) < (o.halfW + 0.4f)) {
            if (cart_.y < 1.4f) {  // not cleared by being airborne
                doCrash("obstacle");
                return;
            }
        }
    }

    // ---- Traffic --------------------------------------------------------
    for (auto& c : w.cars) {
        c.x += c.speed * dt;
        // wrap traffic so roads stay busy
        if (c.x > cart_.x + 400) c.x = cart_.x - 400 + ((int)(c.speed) % 80);
        if (c.x < cart_.x - 600) c.x = cart_.x + 200;
        float dx = c.x - cart_.x;
        if (std::fabs(dx) < (c.halfLen + 0.7f) &&
            std::fabs(c.lateral - cart_.lateral) < 0.34f) {
            if (cart_.y < 1.2f) {
                doCrash("car");
                return;
            }
        }
        // close pass scream
        if (cart_.y < 1.2f && std::fabs(dx) < 3.5f &&
            std::fabs(c.lateral - cart_.lateral) < 1.0f) {
            ev_.screamed = true;
        }
        if (std::fabs(dx) < 4.0f) ev_.carsPassed++;
    }

    // ---- Boulder (the "before it's too late" pressure) -----------------
    if (boulderActive_) {
        float bg = w.roadGrade(cart_.x);
        boulderX_ += (6.0f + cart_.vx * 0.25f) * dt + 2.5f * (-bg) * dt;
        if (boulderX_ >= cart_.x - 0.5f) {
            doCrash("boulder");
            return;
        }
    }

    // ---- Out of bounds sideways ----------------------------------------
    if (std::fabs(cart_.lateral) > 1.02f) {
        doCrash("off road");
        return;
    }

    if (cart_.vx > topSpeed_) topSpeed_ = cart_.vx;

    // ---- Win ------------------------------------------------------------
    if (cart_.x >= w.totalLength) {
        state_ = State::WIN;
    }
}

void Game::updateAir(float dt, const Input& inp) {
    runTime_ += dt;
    World& w = world_;
    // projectile motion
    cart_.vy -= 9.81f * dt;
    cart_.x += cart_.vx * dt;
    cart_.y += cart_.vy * dt;
    cart_.wheelSpin += cart_.vx * dt * 1.2f;

    // air control via lean
    cart_.pitch += inp.lean * 1.5f * dt;
    cart_.roll = approach(cart_.roll, -inp.steer * 0.6f, 6.0f, dt);
    cart_.lateral += inp.steer * 1.2f * dt;
    cart_.lateral = clamp(cart_.lateral, -1.05f, 1.05f);

    float gh = w.roadHeight(cart_.x);
    float groundY = 0; // relative to road
    if (cart_.y <= groundY && cart_.vy < 0) {
        land();
    }

    // traffic still scrolls while airborne (advance positions)
    for (auto& c : w.cars) c.x += c.speed * dt;

    if (cart_.vx > topSpeed_) topSpeed_ = cart_.vx;
    if (cart_.x >= w.totalLength) state_ = State::WIN;
}

void Game::step(float dt, const Input& inp) {
    ev_.clear();
    switch (state_) {
        case State::MENU:
            if (inp.confirm || inp.push) {
                state_ = State::SPRINT;
                cart_.x = 0; cart_.vx = 0;
            }
            break;
        case State::SPRINT:
            updateSprint(dt, inp);
            break;
        case State::RIDE:
            updateRide(dt, inp);
            break;
        case State::AIR:
            updateAir(dt, inp);
            break;
        case State::CRASH:
            crashTimer += dt;
            shake = approach(shake, 0.0f, 2.5f, dt);
            if ((inp.confirm || inp.start) && crashTimer > 0.8f) {
                reset(seed_);
            }
            break;
        case State::WIN:
        case State::LOSE:
            shake = approach(shake, 0.0f, 2.5f, dt);
            if ((inp.confirm || inp.start) && crashTimer > 0.8f) {
                reset(seed_);
            }
            break;
    }
    prev_ = inp;
}

}  // namespace sim
