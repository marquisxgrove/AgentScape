#include "domain/appearance.hpp"

namespace as::domain {

int cycle(int value, int size, int dir) {
  if (size < 1) return 0;
  int v = (value + dir) % size;
  if (v < 0) v += size;
  return v;
}

namespace {
bool inRange(int v, int size) { return v >= 0 && v < size; }
int pick(core::Rng& rng, int size) { return static_cast<int>(rng.below(static_cast<std::uint64_t>(size))); }
}  // namespace

Appearance randomAppearance(core::Rng& rng) {
  const auto& L = APPEARANCE_LIMITS;
  Appearance a;
  a.bodyType = pick(rng, L.bodyType) == 0 ? BodyType::A : BodyType::B;
  a.hair = pick(rng, L.hair);
  a.facialHair = pick(rng, L.facialHair);
  a.torso = pick(rng, L.torso);
  a.arms = pick(rng, L.arms);
  a.legs = pick(rng, L.legs);
  a.colors.hair = pick(rng, L.colors.hair);
  a.colors.torso = pick(rng, L.colors.torso);
  a.colors.legs = pick(rng, L.colors.legs);
  a.colors.feet = pick(rng, L.colors.feet);
  a.colors.skin = pick(rng, L.colors.skin);
  return a;
}

Result<Appearance, ValidationError> validateAppearance(const Appearance& a) {
  const auto& L = APPEARANCE_LIMITS;
  const bool ok = inRange(a.hair, L.hair) && inRange(a.facialHair, L.facialHair) && inRange(a.torso, L.torso) &&
                  inRange(a.arms, L.arms) && inRange(a.legs, L.legs) && inRange(a.colors.hair, L.colors.hair) &&
                  inRange(a.colors.torso, L.colors.torso) && inRange(a.colors.legs, L.colors.legs) &&
                  inRange(a.colors.feet, L.colors.feet) && inRange(a.colors.skin, L.colors.skin);
  if (!ok) return tl::unexpected(ValidationError::OutOfRange);
  return a;
}

}  // namespace as::domain
