#include <catch2/catch_test_macros.hpp>

#include <set>
#include <tuple>

#include "core/rng.hpp"
#include "domain/appearance.hpp"
#include "domain/palette.hpp"

using namespace as::domain;

namespace {
Palette256 basePalette() {
  Palette256 p;
  for (std::size_t i = 0; i < p.size(); ++i) {
    p[i] = {static_cast<std::uint8_t>(i), 1, 2, 255};  // distinctive so we can spot changes
  }
  p[0] = {0, 0, 0, 0};  // transparent colour key
  return p;
}
}  // namespace

TEST_CASE("buildPalette only touches the reserved region", "[unit][palette]") {
  as::core::SplitMix64 rng(5);
  const Appearance a = randomAppearance(rng);
  const Palette256 base = basePalette();
  const Palette256 result = buildPalette(a, base);

  for (int i = 0; i < 256; ++i) {
    const bool reserved = i >= RESERVED_BEGIN && i < RESERVED_END;
    if (reserved) continue;  // may or may not differ, but must be allowed to differ only here
    CHECK(result[static_cast<std::size_t>(i)] == base[static_cast<std::size_t>(i)]);
  }
  // Index 0 (transparent) is outside the reserved range, so it is preserved.
  CHECK(result[0] == base[0]);
}

TEST_CASE("different colour choices produce different reserved ramps", "[unit][palette]") {
  Appearance a;
  a.colors.hair = 0;
  Appearance b = a;
  b.colors.hair = 5;
  const Palette256 base = basePalette();
  const auto pa = buildPalette(a, base);
  const auto pb = buildPalette(b, base);
  bool anyDiff = false;
  for (int i = RESERVED_HAIR.first; i < RESERVED_HAIR.first + RESERVED_HAIR.count; ++i) {
    if (!(pa[static_cast<std::size_t>(i)] == pb[static_cast<std::size_t>(i)])) anyDiff = true;
  }
  CHECK(anyDiff);
}

TEST_CASE("buildPalette is deterministic", "[unit][palette]") {
  as::core::SplitMix64 rng(9);
  const Appearance a = randomAppearance(rng);
  const Palette256 base = basePalette();
  CHECK(buildPalette(a, base) == buildPalette(a, base));
}

TEST_CASE("every skin tone is a plausible skin colour (warm: red >= green >= blue)", "[unit][palette]") {
  const Palette256 base = basePalette();
  for (int tone = 0; tone < APPEARANCE_LIMITS.colors.skin; ++tone) {
    Appearance a;
    a.colors.skin = tone;
    const Rgba main = buildPalette(a, base)[static_cast<std::size_t>(RESERVED_SKIN.first + 4)];  // shade 4 = the tone
    CHECK(main.r >= main.g);
    CHECK(main.g >= main.b);
  }
}

TEST_CASE("every colour choice in a region is distinct, and shades run dark to light", "[unit][palette]") {
  const Palette256 base = basePalette();
  const auto distinct = [&](int count, auto set, ReservedRange range) {
    std::set<std::tuple<int, int, int>> seen;
    for (int choice = 0; choice < count; ++choice) {
      Appearance a;
      set(a, choice);
      const Rgba c = buildPalette(a, base)[static_cast<std::size_t>(range.first + 4)];
      CHECK(seen.insert({c.r, c.g, c.b}).second);
    }
  };
  distinct(APPEARANCE_LIMITS.colors.skin, [](Appearance& a, int i) { a.colors.skin = i; }, RESERVED_SKIN);
  distinct(APPEARANCE_LIMITS.colors.hair, [](Appearance& a, int i) { a.colors.hair = i; }, RESERVED_HAIR);
  distinct(APPEARANCE_LIMITS.colors.torso, [](Appearance& a, int i) { a.colors.torso = i; }, RESERVED_TORSO);
  distinct(APPEARANCE_LIMITS.colors.legs, [](Appearance& a, int i) { a.colors.legs = i; }, RESERVED_LEGS);
  distinct(APPEARANCE_LIMITS.colors.feet, [](Appearance& a, int i) { a.colors.feet = i; }, RESERVED_FEET);

  Appearance a;
  a.colors.torso = 7;  // a mid-blue
  const auto pal = buildPalette(a, base);
  for (int i = 1; i < RESERVED_TORSO.count; ++i) {
    const auto& dark = pal[static_cast<std::size_t>(RESERVED_TORSO.first + i - 1)];
    const auto& light = pal[static_cast<std::size_t>(RESERVED_TORSO.first + i)];
    CHECK(light.b >= dark.b);
  }
}
