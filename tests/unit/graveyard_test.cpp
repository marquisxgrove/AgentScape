#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "domain/codex_validation.hpp"
#include "domain/graveyard.hpp"

using namespace as::domain;

namespace {
BaneCandidate bane(const std::string& id, std::int64_t agree, std::int64_t disagree, bool hidden = false) {
  return {id, agree, disagree, hidden};
}
std::vector<std::string> ids(const std::vector<BaneCandidate>& all, const std::vector<std::size_t>& picked) {
  std::vector<std::string> out;
  for (const auto i : picked) out.push_back(all[i].id);
  return out;
}
}  // namespace

TEST_CASE("a Bane needs at least three votes to get a tombstone", "[unit][graveyard]") {
  const std::vector<BaneCandidate> all{bane("a", 2, 0), bane("b", 3, 0), bane("c", 1, 1), bane("d", 2, 1)};
  CHECK(ids(all, pickTombstones(all)) == std::vector<std::string>{"b", "d"});
}

TEST_CASE("hidden Banes never get a tombstone", "[unit][graveyard]") {
  const std::vector<BaneCandidate> all{bane("a", 50, 0, true), bane("b", 3, 0)};
  CHECK(ids(all, pickTombstones(all)) == std::vector<std::string>{"b"});
}

TEST_CASE("tombstones are ranked by Note_Score, so many votes beat a few equally positive ones", "[unit][graveyard]") {
  const std::vector<BaneCandidate> all{bane("few", 3, 0), bane("many", 200, 0), bane("mixed", 10, 10)};
  CHECK(ids(all, pickTombstones(all)) == std::vector<std::string>{"many", "few", "mixed"});
}

TEST_CASE("a well-supported 90% beats a thinly supported 100%", "[unit][graveyard]") {
  const std::vector<BaneCandidate> all{bane("thin", 3, 0), bane("solid", 90, 10)};
  CHECK(ids(all, pickTombstones(all)) == std::vector<std::string>{"solid", "thin"});
}

TEST_CASE("equal scores break ties by votes and then id, so the order is total", "[unit][graveyard]") {
  const std::vector<BaneCandidate> all{bane("z", 5, 0), bane("a", 5, 0), bane("m", 5, 0)};
  CHECK(ids(all, pickTombstones(all)) == std::vector<std::string>{"a", "m", "z"});
  const std::vector<BaneCandidate> shuffled{all[1], all[2], all[0]};
  CHECK(ids(shuffled, pickTombstones(shuffled)) == std::vector<std::string>{"a", "m", "z"});
}

TEST_CASE("at most thirteen tombstones, the best thirteen", "[unit][graveyard]") {
  std::vector<BaneCandidate> all;
  for (int i = 0; i < 20; ++i) all.push_back(bane("n" + std::string(i < 10 ? "0" : "") + std::to_string(i), 3 + i, 0));
  const auto picked = pickTombstones(all);
  REQUIRE(picked.size() == MAX_TOMBSTONES);
  CHECK(all[picked.front()].id == "n19");
  CHECK(all[picked.back()].id == "n07");
  CHECK(pickTombstones(all, 5).size() == 5);
  CHECK(pickTombstones(all, 0).empty());
}

TEST_CASE("an empty graveyard is fine", "[unit][graveyard]") {
  CHECK(pickTombstones({}).empty());
}

TEST_CASE("short epitaphs are left alone; long ones are cut with an ellipsis", "[unit][graveyard]") {
  CHECK(shortEpitaph("Short and sweet.", 40) == "Short and sweet.");
  const std::string exactly(40, 'x');
  CHECK(shortEpitaph(exactly, 40) == exactly);
  const auto cut = shortEpitaph(std::string(41, 'x'), 40);
  CHECK(cut == std::string(37, 'x') + "...");
  CHECK(utf8Length(cut) == 40u);
  CHECK(shortEpitaph("It cited a paper that never existed, twice.", 20) == "It cited a paper...");   // trailing space trimmed
  CHECK(shortEpitaph("anything", 3) == "...");
  CHECK(shortEpitaph("", 10).empty());
}

TEST_CASE("cutting never splits a multi-byte character", "[unit][graveyard]") {
  std::string text;
  for (int i = 0; i < 30; ++i) text += "\xC3\xA9\xE2\x82\xAC";   // e-acute (2 bytes) + euro sign (3 bytes)
  for (std::size_t limit = 4; limit < 40; ++limit) {
    const auto cut = shortEpitaph(text, limit);
    REQUIRE(utf8Length(cut).has_value());   // always valid UTF-8
    CHECK(utf8Length(cut).value() <= limit);
  }
}
