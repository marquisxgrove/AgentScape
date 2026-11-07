#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.hpp"

namespace as::domain {

// 95% Wilson score lower bound of agree votes out of agree + disagree. 0 when there are no votes.
// A note with 2 votes cannot outrank a note with 200 equally positive ones (design: Boons and Banes).
double wilsonLowerBound(std::int64_t agree, std::int64_t disagree, double z = 1.96);

enum class VoteValue : int { Disagree = -1, Agree = 1 };

struct VoteOp {
  std::string userId;
  std::string noteId;
  std::optional<VoteValue> value;  // nullopt = retract the user's vote
};

using VoteMap = std::map<std::string, VoteValue>;  // userId -> vote, for one Note

enum class VoteError { SelfVote };

// Sets, changes or retracts one user's vote (at most one vote per user per Note). Voting on your own Note
// is refused and leaves the votes unchanged (Requirements 5.3, 5.4).
Result<VoteMap, VoteError> applyVote(const VoteMap& votes, const VoteOp& op, std::string_view noteAuthorId);

struct Tally {
  std::int64_t agree = 0;
  std::int64_t disagree = 0;
  bool operator==(const Tally&) const = default;
};
Tally tally(const VoteMap& votes);

}  // namespace as::domain
