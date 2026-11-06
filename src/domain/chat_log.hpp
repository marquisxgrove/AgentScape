#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace as::domain {

enum class Channel { Game, Tavern };

struct ChatMessage {
  std::string id;
  std::chrono::sys_seconds at;
  Channel channel = Channel::Game;
  std::string text;

  bool operator==(const ChatMessage&) const = default;
};

// Merges incoming into log: drops duplicate ids (existing wins), orders chronologically
// (ties broken by id), keeps only the newest `max`. Idempotent for repeated batches.
std::vector<ChatMessage> appendMessages(std::span<const ChatMessage> log, std::span<const ChatMessage> incoming,
                                        std::size_t max = 100);

}  // namespace as::domain
