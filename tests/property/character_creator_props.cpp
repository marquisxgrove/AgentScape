#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "core/rng.hpp"
#include "domain/adventurer_name.hpp"
#include "domain/appearance.hpp"
#include "generators/rc_catch.hpp"

using namespace as::domain;

// Validates: character-creator Requirement 2.3
TEST_CASE("Property 7: Cycling wraps", "[property][character-creator]") {
  as::testing::checkProperty("cycle stays in range, has period size, and inverts", [](const int rawSize,
                                                                                      const int rawV) {
    const int size = 1 + std::abs(rawSize % 64);
    const int v = std::abs(rawV % size);
    RC_ASSERT(cycle(v, size, 1) >= 0);
    RC_ASSERT(cycle(v, size, 1) < size);
    RC_ASSERT(cycle(v, size, -1) >= 0);
    RC_ASSERT(cycle(v, size, -1) < size);
    // Applying +1 `size` times returns to v.
    int acc = v;
    for (int i = 0; i < size; ++i) acc = cycle(acc, size, 1);
    RC_ASSERT(acc == v);
    // -1 undoes +1.
    RC_ASSERT(cycle(cycle(v, size, 1), size, -1) == v);
  });
}

// Validates: character-creator Requirement 2.5
TEST_CASE("Property 8: Random appearance is valid", "[property][character-creator]") {
  as::testing::checkProperty("randomAppearance always validates", [](const std::uint64_t seed) {
    as::core::SplitMix64 rng(seed);
    const auto a = randomAppearance(rng);
    RC_ASSERT(validateAppearance(a).has_value());
  });
}

namespace {
// A generated name from safe fragments, exercising separators and boundaries.
std::string genName(std::uint64_t seed) {
  as::core::SplitMix64 rng(seed);
  static const char* alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static const char* seps = " _-";
  std::string s;
  const int len = 1 + static_cast<int>(rng.below(14));
  for (int i = 0; i < len; ++i) {
    if (rng.below(4) == 0) {
      s += seps[rng.below(3)];
    } else {
      s += alnum[rng.below(62)];
    }
  }
  return s;
}
}  // namespace

// Validates: character-creator Requirements 1.1, 1.2
TEST_CASE("Property 9: Name rules", "[property][character-creator]") {
  const std::vector<std::string_view> blocklist{"admin", "moderator"};

  as::testing::checkProperty("accepted names satisfy every rule; nameKey is idempotent", [&](const std::uint64_t seed) {
    const std::string raw = genName(seed);
    const auto result = validateName(raw, blocklist);
    if (result) {
      const std::string& n = *result;
      RC_ASSERT(!n.empty());
      RC_ASSERT(n.size() <= 12);
      const auto isSep = [](char c) { return c == ' ' || c == '_' || c == '-'; };
      const auto isAlnum = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
      };
      RC_ASSERT(isAlnum(n.front()));
      RC_ASSERT(!isSep(n.back()));
      for (std::size_t i = 0; i + 1 < n.size(); ++i) {
        RC_ASSERT(!(isSep(n[i]) && isSep(n[i + 1])));  // no consecutive separators
        RC_ASSERT(isAlnum(n[i]) || isSep(n[i]));
      }
      // nameKey is idempotent.
      const auto key = nameKey(n);
      RC_ASSERT(nameKey(key) == key);
    }
  });

  SECTION("conflict rules from the spec") {
    CHECK(nameKey("Kiro Fan") == nameKey("kiro_fan"));
    CHECK(nameKey("a-b c") == nameKey("A B C"));
    CHECK(nameKey("x") != nameKey("y"));
    CHECK(validateName("Kiro Fan", {}).has_value());
    CHECK_FALSE(validateName("", {}).has_value());
    CHECK(validateName(" lead", {}).has_value());        // leading whitespace is trimmed -> "lead"
    CHECK_FALSE(validateName("_lead", {}).has_value());  // a leading non-space separator is rejected
    CHECK(validateName("trail ", {}).has_value());         // surrounding whitespace is trimmed -> "trail"
    CHECK_FALSE(validateName("trail-", {}).has_value());   // a real trailing separator is rejected
    CHECK_FALSE(validateName("a__b", {}).has_value());
    CHECK_FALSE(validateName("-start", {}).has_value());
    CHECK_FALSE(validateName("toolongusername", {}).has_value());
    CHECK_FALSE(validateName("bad!", {}).has_value());
    CHECK(validateName("admin", blocklist).error() == NameError::Blocked);
  }
}
