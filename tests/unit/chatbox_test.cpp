#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "ui/chat_model.hpp"

using namespace as::ui;
using as::domain::Channel;
using as::domain::ChatMessage;
using std::chrono::sys_seconds;

namespace {
ChatMessage msg(std::string id, int t, Channel ch, std::string text) {
  return {std::move(id), sys_seconds{std::chrono::seconds{t}}, ch, std::move(text)};
}
}  // namespace

TEST_CASE("tabs filter by channel; All shows everything", "[unit][chatbox]") {
  ChatModel m;
  m.loadInitial({msg("act-1", 10, Channel::Tavern, "Mossbeard wrote a Quest Log."),
                 msg("act-2", 20, Channel::Tavern, "Quillwright reached level 20.")});
  m.append(msg("g1", 25, Channel::Game, "Welcome to AgentScape."));

  CHECK(m.visible(ChatTab::All).size() == 3);
  CHECK(m.visible(ChatTab::Tavern).size() == 2);
  REQUIRE(m.visible(ChatTab::Game).size() == 1);
  CHECK(m.visible(ChatTab::Game)[0].text == "Welcome to AgentScape.");
}

TEST_CASE("pushed game messages get ids and keep arrival order", "[unit][chatbox]") {
  ChatModel m;
  m.append({"", {}, Channel::Game, "first"});
  m.append({"", {}, Channel::Game, "second"});
  const auto game = m.visible(ChatTab::Game);
  REQUIRE(game.size() == 2);
  CHECK(game[0].text == "first");
  CHECK(game[1].text == "second");
}

TEST_CASE("the log is capped at 100, dropping the oldest", "[unit][chatbox]") {
  ChatModel m;
  for (int i = 0; i < 150; ++i) m.append(msg("act-" + std::to_string(i), i, Channel::Tavern, "n"));
  CHECK(m.size() == 100);
  const auto all = m.all();
  CHECK(all.front().id == "act-50");  // 0..49 dropped
  CHECK(all.back().id == "act-149");
}

TEST_CASE("duplicate ids from a re-published event are ignored", "[unit][chatbox]") {
  ChatModel m;
  const auto e = msg("act-7", 5, Channel::Tavern, "once");
  m.append(e);
  m.append(e);
  CHECK(m.size() == 1);
}
