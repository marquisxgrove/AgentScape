#pragma once

#include <array>
#include <cstdint>

#include "domain/appearance.hpp"

namespace as::domain {

struct Rgba {
  std::uint8_t r = 0, g = 0, b = 0, a = 0;
  bool operator==(const Rgba&) const = default;
};

using Palette256 = std::array<Rgba, 256>;

// Reserved index ranges in the indexed sprites. Each recolourable region owns a contiguous ramp of
// shades; buildPalette overwrites ONLY these indices from the Appearance's chosen colours. All other
// indices (outlines, whites, transparency at 0) are left as the base palette has them.
struct ReservedRange {
  int first;
  int count;
};
inline constexpr ReservedRange RESERVED_SKIN{16, 8};
inline constexpr ReservedRange RESERVED_HAIR{24, 8};
inline constexpr ReservedRange RESERVED_TORSO{32, 8};
inline constexpr ReservedRange RESERVED_LEGS{40, 8};
inline constexpr ReservedRange RESERVED_FEET{48, 8};
inline constexpr int RESERVED_BEGIN = 16;
inline constexpr int RESERVED_END = 56;  // exclusive

// Returns base with only the reserved-region indices replaced by ramps derived from the Appearance
// colour choices. Deterministic.
Palette256 buildPalette(const Appearance& appearance, const Palette256& base);

}  // namespace as::domain
