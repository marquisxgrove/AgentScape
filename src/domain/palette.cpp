#include "domain/palette.hpp"

#include <algorithm>
#include <array>

namespace as::domain {

namespace {
struct Rgb {
  std::uint8_t r, g, b;
  const char* name;
};

// Curated colour choices per region (the Appearance colour indices select from these). Sizes must match
// APPEARANCE_LIMITS.colors, enforced by the static_asserts below.
constexpr std::array<Rgb, 8> SKIN_TONES{{
    {0xff, 0xdf, 0xc4, "porcelain"}, {0xf0, 0xc8, 0xa0, "fair"}, {0xe0, 0xac, 0x82, "light"}, {0xc6, 0x86, 0x42, "golden"},
    {0xa8, 0x69, 0x3c, "tan"}, {0x8d, 0x55, 0x24, "brown"}, {0x6b, 0x3f, 0x1d, "deep brown"}, {0x4a, 0x2c, 0x14, "ebony"},
}};
constexpr std::array<Rgb, 12> HAIR_COLORS{{
    {0x1c, 0x1a, 0x1a, "black"},
    {0x3b, 0x23, 0x14, "dark brown"},
    {0x6a, 0x4a, 0x2a, "brown"},
    {0x8a, 0x5a, 0x2b, "chestnut"},
    {0x8b, 0x3a, 0x1e, "auburn"},
    {0xc0, 0x50, 0x1e, "ginger"},
    {0xb8, 0x98, 0x5a, "dirty blond"},
    {0xe0, 0xc8, 0x78, "blond"},
    {0xe8, 0xe4, 0xd0, "platinum"},
    {0x9a, 0x9a, 0x9a, "grey"},
    {0x2a, 0xa1, 0x98, "teal"},
    {0x7a, 0x3f, 0xb0, "violet"},
}};
constexpr std::array<Rgb, 16> CLOTH_COLORS{{
    {0xc0, 0x39, 0x2b, "red"}, {0xe6, 0x7e, 0x22, "orange"}, {0xe0, 0xb8, 0x2e, "yellow"}, {0x8b, 0xc3, 0x4a, "lime"},
    {0x2e, 0x8b, 0x57, "green"}, {0x1a, 0xbc, 0x9c, "turquoise"}, {0x3a, 0xa6, 0xd8, "sky blue"}, {0x2f, 0x5f, 0xc0, "blue"},
    {0x3f, 0x3f, 0x9a, "indigo"}, {0x8e, 0x44, 0xad, "purple"}, {0xc0, 0x39, 0x8a, "magenta"}, {0xe8, 0x8a, 0xa8, "pink"},
    {0x8a, 0x5a, 0x3a, "brown"}, {0x80, 0x80, 0x80, "grey"}, {0x2a, 0x2a, 0x2e, "black"}, {0xe8, 0xe8, 0xe8, "white"},
}};
constexpr std::array<Rgb, 6> FOOT_COLORS{{
    {0x4a, 0x2e, 0x1a, "dark brown"}, {0x7a, 0x4a, 0x2a, "brown"}, {0x1e, 0x1e, 0x22, "black"},
    {0x6a, 0x6a, 0x70, "grey"}, {0x8a, 0x2a, 0x2a, "red"}, {0xb0, 0x8a, 0x5a, "tan"},
}};

static_assert(SKIN_TONES.size() == static_cast<std::size_t>(APPEARANCE_LIMITS.colors.skin));
static_assert(HAIR_COLORS.size() == static_cast<std::size_t>(APPEARANCE_LIMITS.colors.hair));
static_assert(CLOTH_COLORS.size() == static_cast<std::size_t>(APPEARANCE_LIMITS.colors.torso));
static_assert(CLOTH_COLORS.size() == static_cast<std::size_t>(APPEARANCE_LIMITS.colors.legs));
static_assert(FOOT_COLORS.size() == static_cast<std::size_t>(APPEARANCE_LIMITS.colors.feet));

// Shade 4 of the 8-step ramp is the chosen colour itself (the sprites use it as the main fill);
// lower shades darken it for outlines/shadow and higher shades lighten it for highlights.
float shadeMultiplier(int i) { return i <= 4 ? 0.55F + 0.1125F * static_cast<float>(i) : 1.0F + 0.05F * static_cast<float>(i - 4); }

template <std::size_t N>
const Rgb& pick(const std::array<Rgb, N>& table, int choice) {
  const int clamped = std::clamp(choice, 0, static_cast<int>(N) - 1);  // validateAppearance guards callers
  return table[static_cast<std::size_t>(clamped)];
}

void fillRamp(Palette256& pal, const ReservedRange& range, const Rgb& base) {
  for (int i = 0; i < range.count; ++i) {
    const float mul = shadeMultiplier(i);
    const auto scale = [mul](std::uint8_t v) {
      return static_cast<std::uint8_t>(std::clamp(static_cast<float>(v) * mul, 0.0F, 255.0F));
    };
    pal[static_cast<std::size_t>(range.first + i)] = {scale(base.r), scale(base.g), scale(base.b), 255};
  }
}
}  // namespace

int colorCount(ColorRegion region) {
  switch (region) {
    case ColorRegion::Hair: return static_cast<int>(HAIR_COLORS.size());
    case ColorRegion::Torso:
    case ColorRegion::Legs: return static_cast<int>(CLOTH_COLORS.size());
    case ColorRegion::Feet: return static_cast<int>(FOOT_COLORS.size());
    case ColorRegion::Skin: return static_cast<int>(SKIN_TONES.size());
  }
  return 0;
}

Swatch swatchFor(ColorRegion region, int choice) {
  const Rgb* c = nullptr;
  switch (region) {
    case ColorRegion::Hair: c = &pick(HAIR_COLORS, choice); break;
    case ColorRegion::Torso:
    case ColorRegion::Legs: c = &pick(CLOTH_COLORS, choice); break;
    case ColorRegion::Feet: c = &pick(FOOT_COLORS, choice); break;
    case ColorRegion::Skin: c = &pick(SKIN_TONES, choice); break;
  }
  return {c->r, c->g, c->b, c->name};
}

Palette256 buildPalette(const Appearance& a, const Palette256& base) {
  Palette256 pal = base;
  fillRamp(pal, RESERVED_SKIN, pick(SKIN_TONES, a.colors.skin));
  fillRamp(pal, RESERVED_HAIR, pick(HAIR_COLORS, a.colors.hair));
  fillRamp(pal, RESERVED_TORSO, pick(CLOTH_COLORS, a.colors.torso));
  fillRamp(pal, RESERVED_LEGS, pick(CLOTH_COLORS, a.colors.legs));
  fillRamp(pal, RESERVED_FEET, pick(FOOT_COLORS, a.colors.feet));
  return pal;
}

}  // namespace as::domain
