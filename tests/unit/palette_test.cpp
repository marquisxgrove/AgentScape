#include <catch2/catch_test_macros.hpp>

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
