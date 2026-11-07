#include "ui/chatbox.hpp"

#include <imgui.h>

#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::ui {

namespace {
Chatbox* g_active = nullptr;
}

void pushGameMessage(const std::string& text) {
  if (g_active != nullptr) g_active->append({"", {}, domain::Channel::Game, text});
}

Chatbox::Chatbox(app::EventBus& bus) : bus_(bus) {
  activitySub_ = app::Subscription{
      bus_, bus_.subscribe<app::ActivityPosted>([this](const app::ActivityPosted& e) { append(e.message); })};
  gameSub_ = app::Subscription{bus_, bus_.subscribe<app::GameMessage>([this](const app::GameMessage& e) {
                                 append({"", {}, domain::Channel::Game, e.text});
                               })};
  g_active = this;
}

Chatbox::~Chatbox() {
  if (g_active == this) g_active = nullptr;
}

void Chatbox::loadInitial(const std::vector<domain::ChatMessage>& recent) { model_.loadInitial(recent); }

void Chatbox::append(const domain::ChatMessage& message) {
  model_.append(message);
  autoScroll_ = true;
}

void Chatbox::draw() {
  StonePanel panel{"##chatbox", ""};
  const auto tabButton = [&](const char* label, ChatTab t) {
    if (TabButton(label, tab_ == t, {72, 0})) tab_ = t;
  };
  tabButton("All##tab", ChatTab::All);
  ImGui::SameLine();
  tabButton("Game##tab", ChatTab::Game);
  ImGui::SameLine();
  tabButton("Tavern##tab", ChatTab::Tavern);

  ImGui::BeginChild("##chatlog", {0, 0});
  for (const auto& m : model_.visible(tab_)) {
    RichText(m.text, PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
  }
  if (autoScroll_) {
    ImGui::SetScrollHereY(1.0F);
    autoScroll_ = false;
  }
  ImGui::EndChild();
}

}  // namespace as::ui
