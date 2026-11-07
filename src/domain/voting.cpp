#include "domain/voting.hpp"

#include <algorithm>
#include <cmath>

namespace as::domain {

double wilsonLowerBound(std::int64_t agree, std::int64_t disagree, double z) {
  const std::int64_t n = agree + disagree;
  if (n <= 0 || agree < 0 || disagree < 0) return 0.0;
  const double nn = static_cast<double>(n);
  const double p = static_cast<double>(agree) / nn;
  const double z2 = z * z;
  const double centre = p + z2 / (2.0 * nn);
  const double margin = z * std::sqrt((p * (1.0 - p) + z2 / (4.0 * nn)) / nn);
  const double bound = (centre - margin) / (1.0 + z2 / nn);
  return std::clamp(bound, 0.0, 1.0);
}

Result<VoteMap, VoteError> applyVote(const VoteMap& votes, const VoteOp& op, std::string_view noteAuthorId) {
  if (op.userId == noteAuthorId) return tl::unexpected(VoteError::SelfVote);
  VoteMap next = votes;
  if (op.value) {
    next[op.userId] = *op.value;
  } else {
    next.erase(op.userId);
  }
  return next;
}

Tally tally(const VoteMap& votes) {
  Tally t;
  for (const auto& [user, value] : votes) {
    if (value == VoteValue::Agree) {
      ++t.agree;
    } else {
      ++t.disagree;
    }
  }
  return t;
}

}  // namespace as::domain
