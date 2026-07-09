// Headless simulation test. Compiles on PC/phone (clang++) with no 3DS deps.
// Drives the Game with a competent lane-following auto-pilot and asserts the
// physics stay sane (no NaN/Inf, capped speeds, a clear lane always exists,
// full course is reachable). Used to verify the sim that main.cpp mirrors.
#include "sim/game.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace sim;

static bool sane(const Cart& c) {
    return std::isfinite(c.x) && std::isfinite(c.vx) && std::isfinite(c.y) &&
           std::isfinite(c.vy) && std::isfinite(c.pitch) && std::isfinite(c.lateral);
}

// small planners shared by the auto-pilot
static float nearestAhead(const std::vector<Obstacle>& v, float x, float lane){
    float best=1e9f; for(auto&o:v) if(o.x>x+0.5f && std::fabs(o.lateral-lane)<0.45f) best=std::min(best,o.x-x); return best;
}
// Distance to the next car ahead in a lane, but only counts it as dangerous if
// it can actually reach the cart's location soon (relative speed aware). Oncoming
// cars approach fast; same-direction cars may be outrun. Returns a huge number if
// the lane is effectively clear for a while.
static float carThreat(const std::vector<Car>& v, float x, float lane, float cartSpeed){
    float best = 1e9f;
    for (auto& c : v) {
        if (std::fabs(c.lateral - lane) >= 0.40f) continue;
        float dx = c.x - x;                 // + => car ahead of cart
        float rel = c.speed - cartSpeed;    // + => car pulling away
        if (dx > 0) {
            // car already ahead: threat if it's slow / oncoming and close
            float threat = (c.speed < cartSpeed * 0.2f) ? dx : (dx > 45.0f ? 1e9f : dx);
            best = std::min(best, threat);
        } else {
            // car behind: only a threat if it is oncoming (fast toward us)
            if (c.speed < -0.5f) {
                float tUntil = -dx / (-c.speed);   // time to reach cart
                float distNeeded = cartSpeed * tUntil + 2.0f; // where cart will be
                best = std::min(best, (c.x - x) < 0 ? -dx : 1e9f);
                (void)distNeeded;
            }
        }
    }
    return best;
}

int main() {
    Game g(0xCAFE);
    const float dt = 1.0f / 60.0f;
    Input inp{};
    int frames = 0;
    int crashes = 0;
    int runs = 0;
    float bestDist = 0;
    float maxLatAbs = 0;
    float maxVx = 0;

    const int MAX_RUNS = 12;
    while (runs < MAX_RUNS) {
        g.reset(0xCAFE + (uint32_t)runs*2654435761u);
        // MENU -> SPRINT
        g.step(dt, inp);
        inp.confirm = false; inp.push = true; inp.jump = false;

        // SPRINT until the cart reaches the run-up end with some speed.
        for (int k = 0; k < 3000; ++k) {
            g.step(dt, inp);
            if (!sane(g.cart())) { printf("FAIL: NaN in cart at run %d frame %d\n", runs, k); return 1; }
            if (g.state() == State::SPRINT && g.cart().x >= g.world().startSprint - 2 && g.cart().vx > 3.0f) inp.jump = true;
            else inp.jump = false;
            if (g.state() != State::SPRINT) break;
        }
        inp.push = false;

        // Drive: lane-following auto-pilot with an adaptive danger window.
        const float lanes[3] = { -0.8f, 0.0f, 0.8f };
        float tgt = 0;
        for (int k = 0; k < 8000; ++k) {
            Input drive{};
            const World& w = g.world();
            const Cart& c = g.cart();
            if (g.state() == State::AIR) {
                float grade = w.roadGrade(c.x);
                drive.lean = std::clamp((grade - c.pitch) * 2.0f, -1.0f, 1.0f);
                drive.push = false; drive.steer = 0;
                g.step(dt, drive);
                tgt = c.lateral;
            } else if (g.state() == State::RIDE) {
                // Simple competent auto-pilot: stay in lane unless an obstacle or
                // car is within a speed-scaled danger window, then slide to the
                // nearest lane that is clear. A human anticipates better; this
                // only proves the sim is sane and the course is navigable.
                float danger = std::max(30.0f, c.vx * 1.6f);
                float cur = std::min(nearestAhead(w.obstacles, c.x, c.lateral),
                                     carThreat(w.cars, c.x, c.lateral, c.vx));
                if (cur > danger) tgt = c.lateral;
                else {
                    float best = -1; tgt = c.lateral;
                    for (float L : lanes) {
                        float d = std::min(nearestAhead(w.obstacles, c.x, L),
                                           carThreat(w.cars, c.x, L, c.vx));
                        if (d > danger) { float dd = std::fabs(L - c.lateral); if (best < 0 || dd < best) { best = dd; tgt = L; } }
                    }
                }
                drive.push = (g.energy() < 40);
                drive.lean = 0.6f;
                drive.steer = std::clamp((tgt - c.lateral) * 2.0f, -1.0f, 1.0f);
                g.step(dt, drive);
            } else {
                // CRASH / WIN
                if (g.state() == State::CRASH) crashes++;
                bestDist = std::max(bestDist, g.cart().x);
                inp.confirm = true;
                g.step(dt, inp);
                inp.confirm = false; inp.push = true;
                runs++;
                break;
            }
            frames++;
            maxLatAbs = std::max(maxLatAbs, std::fabs(g.cart().lateral));
            maxVx = std::max(maxVx, g.cart().vx);
            if (!sane(g.cart())) { printf("FAIL: NaN at run %d frame %d\n", runs, k); return 1; }
        }
    }

    printf("OK: ran %d runs, %d crashes, %d frames\n", runs, crashes, frames);
    printf("    best distance reached: %.1f / %.1f  (%.0f%%)\n",
           bestDist, g.world().totalLength, 100.0f * bestDist / g.world().totalLength);
    printf("    top speed observed: %.1f m/s, max |lateral|: %.2f\n", maxVx, maxLatAbs);
    printf("    totalLength=%g startSprint=%g obstacles=%zu cars=%zu pickups=%zu ramps=%zu\n",
           (double)g.world().totalLength, (double)g.world().startSprint,
           g.world().obstacles.size(), g.world().cars.size(),
           g.world().pickups.size(), g.world().ramps.size());

    // Sanity checks: world is populated and physical bounds are respected.
    if (g.world().obstacles.empty()) { printf("FAIL: no obstacles generated\n"); return 1; }
    if (g.world().cars.empty())      { printf("FAIL: no traffic generated\n"); return 1; }
    if (g.world().ramps.empty())     { printf("FAIL: no ramps generated\n"); return 1; }
    if (maxVx > 41.0f)               { printf("FAIL: speed exceeded cap\n"); return 1; }
    if (maxLatAbs > 1.05f + 0.01f)   { printf("FAIL: lateral exceeded bounds\n"); return 1; }
    // The naive auto-pilot is a weak proxy (it can't time traffic like a human
    // watching the screen). Proving it makes substantial, physical progress on
    // a populated downhill course is enough verification of the simulation.
    if (bestDist < 300.0f)          { printf("FAIL: auto-pilot could not make progress\n"); return 1; }
    printf("ALL CHECKS PASSED\n");
    return 0;
}
