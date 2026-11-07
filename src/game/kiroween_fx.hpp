#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace as::game {

// Pure, deterministic animation state for the Kiroween overlays (no SDL/ImGui), so the stepping is
// unit-testable. Positions are in frame pixels. When reduceMotion is set, step() holds everything
// still (Req 6.5): fog does not drift, bats do not fly, pumpkins do not flicker.

struct Drifter {  // a tiled fog layer or a single sprite that scrolls and wraps
  float x = 0;
  float y = 0;
  float vx = 0;  // px/s
  float vy = 0;
};

struct Bat {
  float x = 0;
  float y = 0;
  float phase = 0;    // flap/bob phase, advances with time
  float speed = 0;    // px/s horizontal
  float bob = 0;      // vertical bob amplitude
  float baseY = 0;
  int frame = 0;      // 0/1 wing frame
};

struct Pumpkin {
  float x = 0;
  float y = 0;
  float flicker = 1.0F;  // 0..1 glow multiplier
  float phase = 0;
};

// Advances a drifter by dt seconds, wrapping into [0, wrapW) x [0, wrapH). No-op if !animate.
void stepDrifter(Drifter& d, double dt, float wrapW, float wrapH, bool animate);

// Advances a bat; wraps horizontally across [−margin, width+margin]; bob is sinusoidal. Updates
// frame (wing flap) from phase. No-op if !animate (keeps a static pose).
void stepBat(Bat& b, double dt, float width, float margin, bool animate);

// Advances a pumpkin flicker (bounded in [0.6, 1.0]). Holds steady at 1.0 if !animate.
void stepPumpkin(Pumpkin& p, double dt, bool animate);

// Deterministic initial layouts from a seed (so tests and renders agree).
std::vector<Bat> makeBats(std::size_t count, float width, float height, std::uint64_t seed);

}  // namespace as::game
