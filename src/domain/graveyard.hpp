#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace as::domain {

inline constexpr std::size_t MAX_TOMBSTONES = 13;   // Requirement 7.1
inline constexpr std::int64_t MIN_TOMBSTONE_VOTES = 3;

// A Bane that might get a tombstone.
struct BaneCandidate {
  std::string id;
  std::int64_t agree = 0;
  std::int64_t disagree = 0;
  bool hidden = false;
};

// The indices of the Banes that get a tombstone, best first: not hidden, at least MIN_TOMBSTONE_VOTES votes
// (agree + disagree), ranked by Note_Score (the Wilson lower bound), then by more votes, then by id so the
// order is total. At most `max` are returned (Requirement 7.1).
std::vector<std::size_t> pickTombstones(std::span<const BaneCandidate> banes, std::size_t max = MAX_TOMBSTONES);

// Shortens text to at most `maxCodePoints` code points for a tombstone face, ending in "..." when it was cut.
// Never splits a UTF-8 sequence. Text that already fits is returned unchanged.
std::string shortEpitaph(std::string_view text, std::size_t maxCodePoints);

}  // namespace as::domain
