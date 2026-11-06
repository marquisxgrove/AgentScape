#include "generators/rc_catch.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <string_view>

#include "domain/frame.hpp"
#include "domain/kiroween.hpp"

using namespace as::domain;
using std::chrono::sys_seconds;

// Validates: game-shell Requirement 1.1
TEST_CASE("Property 1: Frame scale is the largest integer that fits", "[property][game-shell]") {
  as::testing::checkProperty("scale >= 1, and largest fitting integer when the frame fits", [](const int rawW, const int rawH) {
    const int w = 1 + std::abs(rawW % 8000);
    const int h = 1 + std::abs(rawH % 6000);
    const int s = computeFrameScale(w, h);
    RC_ASSERT(s >= 1);
    if (w >= FRAME_W && h >= FRAME_H) {
      RC_ASSERT(FRAME_W * s <= w);
      RC_ASSERT(FRAME_H * s <= h);
      RC_ASSERT((FRAME_W * (s + 1) > w) || (FRAME_H * (s + 1) > h));
    }
  });

  as::testing::checkProperty("pointer conversion round-trips the frame origin", [](const int rawW, const int rawH) {
    const int w = FRAME_W + std::abs(rawW % 4000);
    const int h = FRAME_H + std::abs(rawH % 3000);
    const auto layout = layoutFrame(w, h);
    const auto [fx, fy] = toFramePoint(layout, static_cast<float>(layout.offsetX), static_cast<float>(layout.offsetY));
    RC_ASSERT(fx == 0.0F);
    RC_ASSERT(fy == 0.0F);
  });
}

namespace {
bool inKiroweenWindow(sys_seconds t) {
  using namespace std::chrono;
  const auto days = floor<std::chrono::days>(t);
  const year_month_day ymd{days};
  const auto m = static_cast<unsigned>(ymd.month());
  const auto d = static_cast<unsigned>(ymd.day());
  return m == 10 || (m == 11 && d <= 15);
}
}  // namespace

// Validates: game-shell Requirements 6.1, 6.2, 6.3
TEST_CASE("Property 2: Kiroween resolution", "[property][game-shell]") {
  as::testing::checkProperty("On/Off force; Auto/nullopt follow the 1 Oct - 15 Nov UTC window", [](const std::int64_t raw) {
    // 1970 .. ~2100
    const sys_seconds now{std::chrono::seconds{std::abs(raw % 4'100'000'000LL)}};
    RC_ASSERT(isKiroweenActive(KiroweenSetting::On, now));
    RC_ASSERT_FALSE(isKiroweenActive(KiroweenSetting::Off, now));
    const bool expected = inKiroweenWindow(now);
    RC_ASSERT(isKiroweenActive(KiroweenSetting::Auto, now) == expected);
    RC_ASSERT(isKiroweenActive(std::nullopt, now) == expected);
  });

  as::testing::checkProperty("resolve returns the first source that parses", [](const bool aValid, const bool bValid) {
    const std::array<std::optional<std::string_view>, 4> sources{
        std::nullopt, aValid ? std::optional<std::string_view>{"on"} : std::optional<std::string_view>{"bogus"},
        bValid ? std::optional<std::string_view>{"off"} : std::optional<std::string_view>{"nope"},
        std::optional<std::string_view>{"auto"}};
    const auto result = resolveKiroweenSetting(sources);
    RC_ASSERT(result.has_value());
    if (aValid) {
      RC_ASSERT(*result == KiroweenSetting::On);
    } else if (bValid) {
      RC_ASSERT(*result == KiroweenSetting::Off);
    } else {
      RC_ASSERT(*result == KiroweenSetting::Auto);
    }
  });

  SECTION("edge dates") {
    using namespace std::chrono;
    const auto at = [](int y, unsigned m, unsigned d, int hour) {
      return sys_seconds{sys_days{year{y} / month{m} / day{d}}} + hours{hour};
    };
    CHECK_FALSE(isKiroweenActive(std::nullopt, at(2026, 9, 30, 23)));
    CHECK(isKiroweenActive(std::nullopt, at(2026, 10, 1, 0)));
    CHECK(isKiroweenActive(std::nullopt, at(2026, 11, 15, 23)));
    CHECK_FALSE(isKiroweenActive(std::nullopt, at(2026, 11, 16, 0)));
    CHECK_FALSE(isKiroweenActive(std::nullopt, at(2026, 12, 25, 12)));
  }
}
