#include "game/effects.hpp"

#include <algorithm>
#include <cmath>

#include "core/rng.hpp"

namespace as::game {

namespace {
constexpr float PI = 3.14159265F;
constexpr float GRAVITY = 60.0F;   // px/s^2, pulls sparks down
// A frozen burst (Reduce motion) is drawn as if this much time had passed: sparks spread out, still bright.
constexpr float STATIC_BURST_AGE = 0.45F;

float unit(core::Rng& rng) { return static_cast<float>(rng.nextU64() % 10000) / 10000.0F; }   // [0, 1)
}  // namespace

void Effects::addXpDrop(int amount) {
  if (amount <= 0) return;
  drops_.push_back({amount, 0.0F, 0.0F, 1.0F});
}

void Effects::addFireworks(float width, float height, std::uint64_t seed) {
  core::SplitMix64 rng{seed};
  for (std::size_t b = 0; b < BURSTS_PER_LEVEL_UP; ++b) {
    FireworkBurst burst;
    // A burst centre in the middle band of the region, so sparks stay on screen.
    const float cx = width * (0.2F + 0.6F * unit(rng));
    const float cy = height * (0.2F + 0.4F * unit(rng));
    burst.age = -0.25F * static_cast<float>(b);   // staggered starts (a negative age waits)
    const int colorBase = static_cast<int>(rng.nextU64() % 4);
    for (std::size_t i = 0; i < SPARKS_PER_BURST; ++i) {
      const float angle = 2.0F * PI * (static_cast<float>(i) + unit(rng) * 0.5F) / static_cast<float>(SPARKS_PER_BURST);
      const float speed = 50.0F + 70.0F * unit(rng);
      Spark s;
      s.x = cx;
      s.y = cy;
      s.vx = std::cos(angle) * speed;
      s.vy = std::sin(angle) * speed;
      s.maxLife = s.life = 0.7F + 0.6F * unit(rng);
      s.color = (colorBase + static_cast<int>(i % 3)) % 4;
      burst.sparks.push_back(s);
    }
    bursts_.push_back(std::move(burst));
  }
}

void Effects::step(double dtD, bool reduceMotion) {
  const auto dt = static_cast<float>(dtD);

  for (auto& d : drops_) {
    d.age += dt;
    if (!reduceMotion) {
      const float t = std::min(d.age / XP_DROP_SECONDS, 1.0F);
      d.rise = XP_DROP_RISE_PX * t;
      d.alpha = 1.0F - std::max(0.0F, (t - 0.5F) * 2.0F);   // solid for the first half, then fades out
    } else {
      d.rise = 0.0F;
      d.alpha = 1.0F;   // stays put and solid until it expires
    }
  }
  drops_.erase(std::remove_if(drops_.begin(), drops_.end(), [](const XpDropState& d) { return d.age >= XP_DROP_SECONDS; }),
               drops_.end());

  for (auto& b : bursts_) {
    const float before = b.age;
    b.age += dt;
    if (reduceMotion) {
      // One frozen burst: put every spark where it would be a moment after the explosion, and keep it there.
      if (before <= 0.0F && b.age > 0.0F) {
        for (auto& s : b.sparks) {
          s.x += s.vx * STATIC_BURST_AGE;
          s.y += s.vy * STATIC_BURST_AGE + 0.5F * GRAVITY * STATIC_BURST_AGE * STATIC_BURST_AGE;
          s.vx = s.vy = 0.0F;
          s.life = s.maxLife * 0.8F;
        }
      }
      continue;
    }
    if (b.age <= 0.0F) continue;   // not started yet
    const float step = std::min(dt, b.age);   // the part of this step that is after the burst began
    for (auto& s : b.sparks) {
      s.vy += GRAVITY * step;
      s.x += s.vx * step;
      s.y += s.vy * step;
      s.life = std::max(0.0F, s.life - step);
    }
  }
  bursts_.erase(std::remove_if(bursts_.begin(), bursts_.end(), [](const FireworkBurst& b) { return b.age >= FIREWORK_SECONDS; }),
                bursts_.end());
}

}  // namespace as::game
