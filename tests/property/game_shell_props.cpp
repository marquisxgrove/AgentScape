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

// ---------------------------------------------------------------------------------------------
#include <algorithm>
#include <set>

#include "domain/chat_log.hpp"
#include "domain/context_menu.hpp"

namespace {
std::vector<MenuAction> makeActions(std::size_t n, std::size_t defaultIdx) {
  std::vector<MenuAction> actions;
  for (std::size_t i = 0; i < n; ++i) {
    actions.push_back({"a" + std::to_string(i), "Action " + std::to_string(i), "Target", i == defaultIdx});
  }
  return actions;
}
}  // namespace

// Validates: game-shell Requirements 4.1, 4.2
TEST_CASE("Property 3: Context menu shape", "[property][game-shell]") {
  as::testing::checkProperty("default first, Cancel last, Examine iff text, each action once",
                             [](const std::size_t rawN, const std::size_t rawDefault, const bool hasExamine) {
                               const std::size_t n = 1 + rawN % 8;
                               const auto actions = makeActions(n, rawDefault % n);
                               const std::optional<std::string_view> examine =
                                   hasExamine ? std::optional<std::string_view>{"A thing."} : std::nullopt;
                               const auto menu = buildMenu(actions, examine);

                               RC_ASSERT(menu.front().isDefault);
                               RC_ASSERT(menu.back().id == "cancel");
                               const auto examineCount =
                                   std::count_if(menu.begin(), menu.end(), [](const auto& m) { return m.id == "examine"; });
                               RC_ASSERT(examineCount == (hasExamine ? 1 : 0));
                               for (const auto& a : actions) {
                                 const auto c = std::count_if(menu.begin(), menu.end(),
                                                              [&](const auto& m) { return m.id == a.id; });
                                 RC_ASSERT(c == 1);
                               }
                               RC_ASSERT(menu.size() == n + (hasExamine ? 1U : 0U) + 1U);
                             });
}

namespace {
// An id identifies exactly one event, so the message content (time, channel) is derived from the id.
// Timestamps deliberately collide (id % 7) to exercise tie-breaking.
std::vector<ChatMessage> makeMessages(const std::vector<int>& idNums) {
  std::vector<ChatMessage> out;
  for (const int raw : idNums) {
    const int n = std::abs(raw % 40);
    out.push_back({"m" + std::to_string(n), sys_seconds{std::chrono::seconds{n % 7}},
                   n % 2 == 0 ? Channel::Game : Channel::Tavern, "text " + std::to_string(n)});
  }
  return out;
}
}  // namespace

// Validates: game-shell Requirements 5.4, 5.5
TEST_CASE("Property 4: Chat log append", "[property][game-shell]") {
  as::testing::checkProperty(
      "bounded, unique ids, chronological, newest kept, idempotent",
      [](const std::vector<int>& logSpec, const std::vector<int>& inSpec,
         const std::size_t rawMax) {
        const std::size_t max = 1 + rawMax % 30;
        const auto log = appendMessages(makeMessages(logSpec), {}, 1000);  // a valid existing log
        const auto incoming = makeMessages(inSpec);
        const auto result = appendMessages(log, incoming, max);

        RC_ASSERT(result.size() <= max);
        std::set<std::string> ids;
        for (const auto& m : result) RC_ASSERT(ids.insert(m.id).second);
        RC_ASSERT(std::is_sorted(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.at < b.at; }));

        // Newest kept: every dropped message is not newer than the oldest kept one.
        const auto all = appendMessages(log, incoming, 100000);
        if (!result.empty()) {
          for (const auto& m : all) {
            if (ids.count(m.id) == 0) RC_ASSERT(m.at <= result.front().at);
          }
        }
        // Idempotent.
        RC_ASSERT(appendMessages(result, incoming, max) == result);
      });
}
