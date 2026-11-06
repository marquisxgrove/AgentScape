#include "domain/chat_log.hpp"

#include <algorithm>
#include <unordered_set>

namespace as::domain {

std::vector<ChatMessage> appendMessages(std::span<const ChatMessage> log, std::span<const ChatMessage> incoming,
                                        std::size_t max) {
  std::vector<ChatMessage> merged;
  merged.reserve(log.size() + incoming.size());
  std::unordered_set<std::string> seen;
  for (const auto& batch : {log, incoming}) {
    for (const auto& msg : batch) {
      if (seen.insert(msg.id).second) {
        merged.push_back(msg);
      }
    }
  }
  // Total order (time, then id) so trimming is independent of insertion order; this keeps appends idempotent.
  std::sort(merged.begin(), merged.end(), [](const ChatMessage& a, const ChatMessage& b) {
    return a.at != b.at ? a.at < b.at : a.id < b.id;
  });
  if (merged.size() > max) {
    merged.erase(merged.begin(), merged.begin() + static_cast<std::ptrdiff_t>(merged.size() - max));
  }
  return merged;
}

}  // namespace as::domain
