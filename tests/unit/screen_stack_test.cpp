#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"

using namespace as::app;

namespace {
struct Log {
  std::vector<std::string> events;
};

class FakeScreen : public Screen {
 public:
  FakeScreen(ScreenId id, std::string name, Log& log) : id_(id), name_(std::move(name)), log_(log) {}
  ScreenId id() const override { return id_; }
  void onEnter(const ScreenParams& params) override {
    const auto it = params.find("agent");
    log_.events.push_back("enter " + name_ + (it != params.end() ? " " + it->second : ""));
  }
  void onExit() override { log_.events.push_back("exit " + name_); }
  void draw(FrameCtx&) override {}

 private:
  ScreenId id_;
  std::string name_;
  Log& log_;
};

ScreenStack makeStack(Log& log) {
  ScreenStack s;
  s.registerScreen(ScreenId::Login, [&] { return std::make_unique<FakeScreen>(ScreenId::Login, "login", log); });
  s.registerScreen(ScreenId::World, [&] { return std::make_unique<FakeScreen>(ScreenId::World, "world", log); });
  s.registerScreen(ScreenId::Codex, [&] { return std::make_unique<FakeScreen>(ScreenId::Codex, "codex", log); });
  return s;
}
}  // namespace

TEST_CASE("navigation is deferred until applyPending", "[unit][screen-stack]") {
  Log log;
  auto s = makeStack(log);
  s.navigate(ScreenId::Login);
  CHECK_FALSE(s.currentId().has_value());
  s.applyPending();
  CHECK(s.currentId() == ScreenId::Login);
}

TEST_CASE("entering the World resets history so back never returns to Login", "[unit][screen-stack]") {
  Log log;
  auto s = makeStack(log);
  s.navigate(ScreenId::Login);
  s.navigate(ScreenId::World);
  s.applyPending();
  CHECK(s.depth() == 1);
  s.back();
  s.applyPending();
  CHECK(s.currentId() == ScreenId::World);
}

TEST_CASE("back pops to the previous screen and passes params on enter", "[unit][screen-stack]") {
  Log log;
  auto s = makeStack(log);
  s.navigate(ScreenId::World);
  s.navigate(ScreenId::Codex, {{"agent", "kiro"}});
  s.applyPending();
  CHECK(s.currentId() == ScreenId::Codex);
  s.back();
  s.applyPending();
  CHECK(s.currentId() == ScreenId::World);
  CHECK(log.events == std::vector<std::string>{"enter world", "exit world", "enter codex kiro", "exit codex",
                                               "enter world"});
}

TEST_CASE("navigating to an unregistered screen is ignored", "[unit][screen-stack]") {
  Log log;
  auto s = makeStack(log);
  s.navigate(ScreenId::World);
  s.navigate(ScreenId::Graveyard);
  s.applyPending();
  CHECK(s.currentId() == ScreenId::World);
}
