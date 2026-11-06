#include "ui/chat_model.hpp"

namespace as::ui {

void ChatModel::clear() { log_.clear(); }

void ChatModel::loadInitial(const std::vector<domain::ChatMessage>& recent) {
  log_ = domain::appendMessages({}, recent);
}

void ChatModel::append(domain::ChatMessage m) {
  if (m.id.empty()) m.id = "game-" + std::to_string(gameSeq_++);
  if (m.at.time_since_epoch().count() == 0 && !log_.empty()) m.at = log_.back().at;
  log_ = domain::appendMessages(log_, std::vector<domain::ChatMessage>{std::move(m)});
}

std::vector<domain::ChatMessage> ChatModel::visible(ChatTab tab) const {
  if (tab == ChatTab::All) return log_;
  const auto want = tab == ChatTab::Game ? domain::Channel::Game : domain::Channel::Tavern;
  std::vector<domain::ChatMessage> out;
  for (const auto& m : log_) {
    if (m.channel == want) out.push_back(m);
  }
  return out;
}

}  // namespace as::ui
