#include "domain/graveyard.hpp"

#include <algorithm>

#include "domain/voting.hpp"

namespace as::domain {

std::vector<std::size_t> pickTombstones(std::span<const BaneCandidate> banes, std::size_t max) {
  struct Scored {
    std::size_t index;
    double score;
    std::int64_t votes;
  };
  std::vector<Scored> eligible;
  for (std::size_t i = 0; i < banes.size(); ++i) {
    const auto& b = banes[i];
    const std::int64_t votes = b.agree + b.disagree;
    if (b.hidden || votes < MIN_TOMBSTONE_VOTES) continue;
    eligible.push_back({i, wilsonLowerBound(b.agree, b.disagree), votes});
  }
  std::sort(eligible.begin(), eligible.end(), [&](const Scored& a, const Scored& b) {
    if (a.score != b.score) return a.score > b.score;
    if (a.votes != b.votes) return a.votes > b.votes;
    return banes[a.index].id < banes[b.index].id;
  });
  if (eligible.size() > max) eligible.resize(max);
  std::vector<std::size_t> out;
  out.reserve(eligible.size());
  for (const auto& e : eligible) out.push_back(e.index);
  return out;
}

std::string shortEpitaph(std::string_view text, std::size_t maxCodePoints) {
  // Walk code point starts: a byte starts one unless it is a UTF-8 continuation byte (10xxxxxx).
  const std::size_t keep = maxCodePoints > 3 ? maxCodePoints - 3 : 0;   // leave room for the "..."
  std::size_t count = 0;
  std::size_t keepEnd = text.size();
  for (std::size_t i = 0; i < text.size(); ++i) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0U) == 0x80U) continue;
    if (count == keep) keepEnd = i;
    ++count;
  }
  if (count <= maxCodePoints) return std::string(text);
  std::string out(text.substr(0, keepEnd));
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out + "...";
}

}  // namespace as::domain
