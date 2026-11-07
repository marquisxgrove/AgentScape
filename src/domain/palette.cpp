#include "domain/palette.hpp"

#include <algorithm>
#include <cmath>

namespace as::domain {

namespace {
// A small fixed set of base hues per region; the Appearance index selects one, and we generate a
// shade ramp from it (darkest to lightest) across the reserved range. Pure and deterministic.
Rgba hueFor(int region, int choice) {
  // region: 0 skin, 1 hair, 2 torso, 3 legs, 4 feet. Spread hues deterministically.
  const auto h = static_cast<float>((choice * 47 + region * 90) % 360);
  const float s = region == 0 ? 0.35F : 0.6F;  // skin is less saturated
  const float v = 0.85F;
  const float c = v * s;
  const float x = c * (1 - std::fabs(std::fmod(h / 60.0F, 2.0F) - 1));
  const float m = v - c;
  float r = 0;
  float g = 0;
  float b = 0;
  if (h < 60) { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else { r = c; b = x; }
  const auto to8 = [](float f) { return static_cast<std::uint8_t>(std::clamp(f, 0.0F, 1.0F) * 255.0F); };
  return {to8(r + m), to8(g + m), to8(b + m), 255};
}

void fillRamp(Palette256& pal, const ReservedRange& range, int region, int choice) {
  const Rgba hue = hueFor(region, choice);
  for (int i = 0; i < range.count; ++i) {
    // shade from ~45% to ~110% brightness across the ramp
    const float t = range.count > 1 ? static_cast<float>(i) / static_cast<float>(range.count - 1) : 1.0F;
    const float mul = 0.45F + 0.65F * t;
    const auto scale = [mul](std::uint8_t v) {
      return static_cast<std::uint8_t>(std::clamp(static_cast<float>(v) * mul, 0.0F, 255.0F));
    };
    pal[static_cast<std::size_t>(range.first + i)] = {scale(hue.r), scale(hue.g), scale(hue.b), 255};
  }
}
}  // namespace

Palette256 buildPalette(const Appearance& a, const Palette256& base) {
  Palette256 pal = base;
  fillRamp(pal, RESERVED_SKIN, 0, a.colors.skin);
  fillRamp(pal, RESERVED_HAIR, 1, a.colors.hair);
  fillRamp(pal, RESERVED_TORSO, 2, a.colors.torso);
  fillRamp(pal, RESERVED_LEGS, 3, a.colors.legs);
  fillRamp(pal, RESERVED_FEET, 4, a.colors.feet);
  return pal;
}

}  // namespace as::domain
