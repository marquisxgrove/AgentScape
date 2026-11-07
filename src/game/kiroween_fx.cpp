#include "game/kiroween_fx.hpp"

#include <cmath>

#include "core/rng.hpp"

namespace as::game {

namespace {
float wrap(float v, float lo, float hi) {
  const float span = hi - lo;
  if (span <= 0) return lo;
  float r = std::fmod(v - lo, span);
  if (r < 0) r += span;
  return lo + r;
}
}  // namespace

void stepDrifter(Drifter& d, double dt, float wrapW, float wrapH, bool animate) {
  if (!animate) return;
  d.x = wrap(d.x + d.vx * static_cast<float>(dt), 0.0F, wrapW);
  d.y = wrap(d.y + d.vy * static_cast<float>(dt), 0.0F, wrapH);
}

void stepBat(Bat& b, double dt, float width, float margin, bool animate) {
  if (!animate) return;
  b.x = wrap(b.x + b.speed * static_cast<float>(dt) + margin, -0.0F, width + 2 * margin) - margin;
  b.phase += static_cast<float>(dt) * 6.0F;
  b.y = b.baseY + std::sin(b.phase) * b.bob;
  b.frame = (std::fmod(b.phase, 2.0F) < 1.0F) ? 0 : 1;
}

void stepPumpkin(Pumpkin& p, double dt, bool animate) {
  if (!animate) {
    p.flicker = 1.0F;
    return;
  }
  p.phase += static_cast<float>(dt) * 5.0F;
  p.flicker = 0.8F + 0.2F * std::sin(p.phase) * std::cos(p.phase * 0.37F);
  if (p.flicker < 0.6F) p.flicker = 0.6F;
  if (p.flicker > 1.0F) p.flicker = 1.0F;
}

std::vector<Bat> makeBats(std::size_t count, float width, float height, std::uint64_t seed) {
  core::SplitMix64 rng(seed);
  std::vector<Bat> bats;
  bats.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    Bat b;
    b.x = static_cast<float>(rng.below(static_cast<std::uint64_t>(width)));
    b.baseY = 20.0F + static_cast<float>(rng.below(static_cast<std::uint64_t>(height) / 2));
    b.y = b.baseY;
    b.phase = static_cast<float>(rng.below(628)) / 100.0F;  // 0..2pi
    b.speed = 18.0F + static_cast<float>(rng.below(24));
    b.bob = 4.0F + static_cast<float>(rng.below(6));
    bats.push_back(b);
  }
  return bats;
}

}  // namespace as::game
