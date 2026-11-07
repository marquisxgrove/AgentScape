#pragma once

#include <cstdint>
#include <vector>

namespace as::game {

// Pure, deterministic state for the XP drop and the level-up fireworks (quest-logs Requirements 5.5 and 5.6).
// Positions are in frame pixels. With reduceMotion set, nothing moves: a drop stays put and a firework is one
// frozen burst, each shown for a short time and then removed (Reduce motion means "no animation", not "no feedback").

// A floating "+N XP". It rises and fades; the screen draws it near the XP counter.
struct XpDropState {
  int amount = 0;
  float age = 0;        // seconds since it appeared
  float rise = 0;       // pixels risen so far
  float alpha = 1;      // 1 = opaque, 0 = gone
};

inline constexpr float XP_DROP_SECONDS = 1.6F;
inline constexpr float XP_DROP_RISE_PX = 24.0F;

// One spark of a firework burst.
struct Spark {
  float x = 0;
  float y = 0;
  float vx = 0;
  float vy = 0;
  float life = 0;       // seconds left
  float maxLife = 1;
  int color = 0;        // index into the palette the screen picks from
};

struct FireworkBurst {
  float age = 0;
  std::vector<Spark> sparks;
};

inline constexpr float FIREWORK_SECONDS = 1.4F;
inline constexpr std::size_t SPARKS_PER_BURST = 28;
inline constexpr std::size_t BURSTS_PER_LEVEL_UP = 3;

// The set of live effects. step() advances them by dt seconds and drops the finished ones.
class Effects {
 public:
  void addXpDrop(int amount);
  // A level-up: a few bursts over the region [0,width) x [0,height), laid out from `seed` so renders and tests agree.
  void addFireworks(float width, float height, std::uint64_t seed);

  void step(double dt, bool reduceMotion);

  const std::vector<XpDropState>& drops() const { return drops_; }
  const std::vector<FireworkBurst>& bursts() const { return bursts_; }
  bool active() const { return !drops_.empty() || !bursts_.empty(); }
  void clear() {
    drops_.clear();
    bursts_.clear();
  }

 private:
  std::vector<XpDropState> drops_;
  std::vector<FireworkBurst> bursts_;
};

}  // namespace as::game
