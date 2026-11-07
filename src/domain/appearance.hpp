#pragma once

#include "core/result.hpp"
#include "core/rng.hpp"

namespace as::domain {

enum class BodyType { A, B };

// Palette indices into the recolourable regions (validated against APPEARANCE_LIMITS.colors).
struct Colors {
  int hair = 0;
  int torso = 0;
  int legs = 0;
  int feet = 0;
  int skin = 0;
  bool operator==(const Colors&) const = default;
};

struct Appearance {
  BodyType bodyType = BodyType::A;
  int hair = 0;
  int facialHair = 0;  // 0 = none
  int torso = 0;       // base clothing style
  int arms = 0;
  int legs = 0;
  Colors colors;
  bool operator==(const Appearance&) const = default;
};

// Counts of each style/colour option. Every style is available to both body types (Req 2.4).
struct AppearanceLimits {
  int bodyType = 2;
  int hair = 10;
  int facialHair = 8;  // includes "none" at 0
  int torso = 6;
  int arms = 4;
  int legs = 6;
  struct {
    int hair = 12;
    int torso = 16;
    int legs = 16;
    int feet = 6;
    int skin = 8;
  } colors;
};
inline constexpr AppearanceLimits APPEARANCE_LIMITS{};

enum class ValidationError { OutOfRange };

// Wraps value into [0, size) after stepping by dir (+1 / -1). size must be >= 1.
int cycle(int value, int size, int dir);

Appearance randomAppearance(core::Rng& rng);
Result<Appearance, ValidationError> validateAppearance(const Appearance& a);

}  // namespace as::domain
