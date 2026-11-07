#pragma once

// RapidCheck generators for the quest-logs domain (ratings, Quest Log input).
#include <rapidcheck.h>

#include <optional>
#include <string>

#include "domain/ratings.hpp"

namespace as::testing {

// A RatingSet in one of the first `questTypes` Quest Types, so that independent draws often collide on the same
// aggregate (which is where incremental-vs-recompute bugs hide). Roughly 1 in 6 are excluded.
inline rc::Gen<domain::RatingSet> genRatingSet(int questTypes = 3) {
  return rc::gen::exec([questTypes]() {
    domain::RatingSet s;
    s.questType = domain::QUEST_TYPES[static_cast<std::size_t>(*rc::gen::inRange(0, questTypes))];
    s.overall = *rc::gen::inRange(1, 6);
    for (const auto skill : domain::SKILLS) {
      if (*rc::gen::inRange(0, 3) == 0) s.skills[skill] = *rc::gen::inRange(1, 6);
    }
    s.excluded = *rc::gen::inRange(0, 6) == 0;
    return s;
  });
}

// A RatingSet or nothing (about 1 in 4 is nothing).
inline rc::Gen<std::optional<domain::RatingSet>> genMaybeRatingSet(int questTypes = 3) {
  return rc::gen::exec([questTypes]() -> std::optional<domain::RatingSet> {
    if (*rc::gen::inRange(0, 4) == 0) return std::nullopt;
    return *genRatingSet(questTypes);
  });
}

}  // namespace as::testing
