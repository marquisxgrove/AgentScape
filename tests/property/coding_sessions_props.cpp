#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <vector>

#include "domain/coding_session.hpp"
#include "generators/rc_catch.hpp"

using namespace as::domain;
using std::chrono::seconds;
using std::chrono::sys_days;
using std::chrono::sys_seconds;

namespace {
constexpr int SCORING_RUNS = 1000;   // testing.md: scoring math uses 1000 runs

// A moment in November 2026, to the second.
rc::Gen<sys_seconds> genMoment() {
  return rc::gen::map(rc::gen::inRange<long long>(1793500000, 1793500000 + 30LL * 86400),
                      [](long long s) { return sys_seconds{seconds{s}}; });
}

// What other sessions already counted on the days around `from` (each day between 0 and 9 hours, so some days start
// at or over the cap).
CodingDays genUsed(sys_seconds from) {
  CodingDays used;
  const sys_days first = std::chrono::floor<std::chrono::days>(from);
  for (int d = 0; d < 4; ++d) {
    const auto s = *rc::gen::inRange<long long>(0, 9 * 3600 + 1);
    if (s > 0) used[first + std::chrono::days{d}] = seconds{std::min<long long>(s, CODING_DAILY_CAP.count())};
  }
  return used;
}

seconds total(const CodingDays& days) {
  seconds t{0};
  for (const auto& [day, s] : days) t += s;
  return t;
}
}  // namespace

// Validates: coding-sessions Requirement 2.4
TEST_CASE("Property 1: Settling is path-independent", "[property][scoring][coding-sessions]") {
  as::testing::checkPropertyRuns("crediting an interval in pieces equals crediting it at once", SCORING_RUNS, [] {
    const sys_seconds from = *genMoment();
    const auto length = *rc::gen::inRange<long long>(0, 3LL * 86400);
    const sys_seconds to = from + seconds{length};
    const CodingDays before = genUsed(from);

    CodingDays onceUsed = before;
    const CodingDays onceAdded = creditCodingInterval(onceUsed, from, to);

    // The same interval, settled at random moments in between.
    auto cuts = *rc::gen::container<std::vector<long long>>(rc::gen::inRange<long long>(0, length + 1));
    std::sort(cuts.begin(), cuts.end());
    CodingDays piecesUsed = before;
    CodingDays piecesAdded;
    sys_seconds at = from;
    cuts.push_back(length);
    for (const long long c : cuts) {
      const sys_seconds next = from + seconds{c};
      for (const auto& [day, s] : creditCodingInterval(piecesUsed, at, next)) piecesAdded[day] += s;
      at = next;
    }
    RC_ASSERT(piecesUsed == onceUsed);
    RC_ASSERT(piecesAdded == onceAdded);
  });
}

// Validates: coding-sessions Requirement 2.2
TEST_CASE("Property 2: No day counts more than the daily limit", "[property][scoring][coding-sessions]") {
  as::testing::checkPropertyRuns("every day stays within 8 hours, and adds only what it had room for", SCORING_RUNS, [] {
    const sys_seconds from = *genMoment();
    const sys_seconds to = from + seconds{*rc::gen::inRange<long long>(0, 3LL * 86400)};
    const CodingDays before = genUsed(from);
    CodingDays used = before;
    const CodingDays added = creditCodingInterval(used, from, to);
    for (const auto& [day, s] : used) RC_ASSERT(s <= CODING_DAILY_CAP);
    for (const auto& [day, s] : added) {
      RC_ASSERT(s > seconds{0});
      const auto it = before.find(day);
      const seconds had = it == before.end() ? seconds{0} : it->second;
      RC_ASSERT(s <= CODING_DAILY_CAP - had);
    }
  });
}

// Validates: coding-sessions Requirements 2.1 and 2.2
TEST_CASE("Property 3: Counted time never exceeds real time, and is all of it with room to spare", "[property][scoring][coding-sessions]") {
  as::testing::checkPropertyRuns("added time is at most the interval, and equal to it when no day was full", SCORING_RUNS, [] {
    const sys_seconds from = *genMoment();
    const sys_seconds to = from + seconds{*rc::gen::inRange<long long>(0, 3LL * 86400)};
    CodingDays used = genUsed(from);
    const CodingDays before = used;
    const seconds added = total(creditCodingInterval(used, from, to));
    RC_ASSERT(added <= to - from);
    RC_ASSERT(total(used) == total(before) + added);

    CodingDays empty;
    const sys_seconds shortEnd = from + seconds{*rc::gen::inRange<long long>(0, CODING_SESSION_MAX.count() + 1)};
    RC_ASSERT(total(creditCodingInterval(empty, from, shortEnd)) == shortEnd - from);   // 4 h never fills a day
  });
}

// Validates: coding-sessions Requirement 1.5
TEST_CASE("Property 4: A session counts at most four hours", "[property][coding-sessions]") {
  as::testing::checkProperty("countedUntil stays within [start, start + 4 h] and follows now inside it", [] {
    const sys_seconds start = *genMoment();
    const sys_seconds now = start + seconds{*rc::gen::inRange<long long>(-86400, 2LL * 86400)};
    const sys_seconds until = codingCountedUntil(start, now);
    RC_ASSERT(until >= start);
    RC_ASSERT(until <= start + CODING_SESSION_MAX);
    if (now >= start && now <= start + CODING_SESSION_MAX) {
      RC_ASSERT(until == now);
    }
    RC_ASSERT(codingXpFor(until - start) <= codingXpFor(CODING_SESSION_MAX));
  });
}

TEST_CASE("a session over midnight counts each part against its own day", "[coding-sessions]") {
  const sys_seconds midnight{sys_days{std::chrono::floor<std::chrono::days>(sys_seconds{seconds{1793500000}})} + std::chrono::days{1}};
  CodingDays used;
  used[std::chrono::floor<std::chrono::days>(midnight - seconds{1})] = CODING_DAILY_CAP - seconds{600};   // 10 min left
  const auto added = creditCodingInterval(used, midnight - seconds{3600}, midnight + seconds{1800});
  REQUIRE(added.size() == 2);
  CHECK(added.begin()->second == seconds{600});              // only the 10 minutes the old day had left
  CHECK(std::next(added.begin())->second == seconds{1800});  // and the half hour after midnight in full
  CHECK(codingXpFor(seconds{1799}) == 29);                   // whole minutes only
  CHECK(formatUtcDay(std::chrono::floor<std::chrono::days>(midnight)).size() == 10);
  CHECK(formatSessionDuration(seconds{3900}) == "1h 05m");
  CHECK(formatSessionDuration(seconds{59}) == "0m");
}
