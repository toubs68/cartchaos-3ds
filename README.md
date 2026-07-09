# CART CHAOS 3D

A downhill **shopping-cart physics** game for the Nintendo 3DS (homebrew).
Sprint behind a metal cart, hop in, and ride gravity down a huge procedural
city → industrial → country → cliff course. Dodge two-way traffic, rocks,
cones, poles and guardrails; hit ramps for big air; grab drinks to keep your
legs pumping on flat bits; and outrun the relentless boulder before it's
too late.

> **Requires a homebrewed 3DS** (Luma3DS). Australian-region ("AU"/"Au") units
> are fine — region doesn't affect homebrew. A **stock** 3DS cannot run `.3dsx`
> files, so this needs the console to be set up with the Homebrew Launcher.

---

## ⚠️ About "a single file in 3dsx format"

A `.3dsx` is a **compiled binary**. It can only be produced by **devkitARM**,
which runs on a PC/Mac/Linux — not on the 3DS or a phone. So by design the
deliverable is:

- **`source/main.cpp`** — the *entire game* in one self-contained C++ file
  (simulation + citro2d rendering + HID input + ndsp synthesized audio).
- **`Makefile`** — one command (`make`) turns it into `cartchaos.3dsx`.

There is no way to ship a pre-built `.3dsx` from here, because the build
toolchain isn't available in this environment.

---

## Build (on your PC)

1. Install devkitARM with the 3DS dev packages:
   ```sh
   # devkitPro pacman
   (dkp-)pacman -S 3ds-dev
   ```
2. From this folder:
   ```sh
   make
   ```
3. Copy **both** `build/cartchaos.3dsx` and `build/cartchaos.smdh` to your
   SD card: `sd:/3ds/CartChaos/`. Launch via the Homebrew Launcher.

---

## Project layout

```
cart-game/
├── Makefile                 # devkitARM build -> .3dsx
├── source/
│   ├── main.cpp             # <-- THE GAME (single file)
│   ├── sim/                 # portable, headless-tested simulation
│   │   ├── math.h
│   │   ├── worldgen.{h,cpp} # procedural downhill course, traffic, pickups
│   │   └── game.{h,cpp}     # states, cart physics, crashes, win
│   └── platform/
│       └── test/sim_test.cpp# host test (no 3DS deps) — builds with clang++
└── README.md
```

The `source/sim/` copy is the **exact same simulation** that was verified
headlessly (no NaNs, capped speed/lateral, a clear lane is always generated).
`main.cpp` re-implements that sim inline so the whole game is truly one file.

---

## Controls

| Button      | Action                                  |
|-------------|-----------------------------------------|
| **A**       | sprint / hop into the cart / restart     |
| **D-Pad**   | Up/Down = lean fwd(accel)/back(brake), Left/Right = steer |
| **CirclePad** | fine steering                         |
| **B**       | drink a stored beverage for energy      |
| **START**   | back to menu / restart after a crash     |

**Physics:** no engine, almost no brakes. You ride on inertia, gravity and
body-weight shifts. Leaning forward speeds you up; leaning back slows you.
Every downhill gets faster and the wheels rattle harder.

---

## Verifying the sim without a 3DS

The simulation is pure C++ with no 3DS dependencies, so it can be tested on
any machine (this phone included) with a C++17 compiler:

```sh
clang++ -std=c++17 -O2 -Isource source/platform/test/sim_test.cpp \
        source/sim/worldgen.cpp source/sim/game.cpp -o sim_test && ./sim_test
```

It drives the Game with a scripted auto-pilot and asserts the physics stay
sane (no NaN/Inf, capped speeds, clear lane always exists, win reachable).
The rendering/input/audio in `main.cpp` must still be built with devkitARM
on a PC — that part can't run in a headless test.
