#pragma once

#include <string>
#include <vector>

#include "app/event_bus.hpp"
#include "domain/chat_log.hpp"
#include "ui/chat_model.hpp"

namespace as::ui {

// The bottom Chatbox widget: All / Game / Tavern tabs over a ChatModel. Subscribes to
// ActivityPosted and GameMessage so committed events and pushed messages appear live (Req 5).
class Chatbox {
 public:
  explicit Chatbox(app::EventBus& bus);
  ~Chatbox();
  Chatbox(const Chatbox&) = delete;
  Chatbox& operator=(const Chatbox&) = delete;

  void loadInitial(const std::vector<domain::ChatMessage>& recent);
  void append(const domain::ChatMessage& message);
  void draw();

  ChatTab tab() const { return tab_; }
  void setTab(ChatTab tab) { tab_ = tab; }

 private:
  app::EventBus& bus_;
  app::Subscription activitySub_;
  app::Subscription gameSub_;
  ChatModel model_;
  ChatTab tab_ = ChatTab::All;
  bool autoScroll_ = true;
};

// Shared API: push a game-voice system message to the active Chatbox (Req 5.6). No-op if none.
void pushGameMessage(const std::string& text);

}  // namespace as::ui
