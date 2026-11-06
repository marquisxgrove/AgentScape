#pragma once

#include <cstddef>
#include <vector>

#include "domain/chat_log.hpp"

namespace as::ui {

enum class ChatTab { All, Game, Tavern };

// Pure Chatbox state (no ImGui): a capped, de-duplicated, chronological message log with channel
// filtering. The Chatbox widget wraps this; unit tests drive it directly (game-shell Req 5.2-5.5).
class ChatModel {
 public:
  void loadInitial(const std::vector<domain::ChatMessage>& recent);  // e.g. 20 recent Tavern events
  void clear();

  // Appends one message (dedupe by id, keep newest 100). Empty id -> auto "game-N"; empty timestamp
  // -> ordered right after the current newest, so pushed game messages stay in arrival order.
  void append(domain::ChatMessage message);

  const std::vector<domain::ChatMessage>& all() const { return log_; }
  std::vector<domain::ChatMessage> visible(ChatTab tab) const;
  std::size_t size() const { return log_.size(); }

 private:
  std::vector<domain::ChatMessage> log_;
  int gameSeq_ = 0;
};

}  // namespace as::ui
