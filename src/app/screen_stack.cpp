#include "app/screen_stack.hpp"

#include <utility>

namespace as::app {

namespace {
bool isHub(ScreenId id) { return id == ScreenId::Login || id == ScreenId::World; }
}  // namespace

void ScreenStack::registerScreen(ScreenId id, ScreenFactory factory) { factories_[id] = std::move(factory); }

void ScreenStack::navigate(ScreenId id, ScreenParams params) {
  pending_.push_back({Pending::Kind::Navigate, id, std::move(params)});
}

void ScreenStack::back() { pending_.push_back({Pending::Kind::Back, ScreenId::Login, {}}); }

void ScreenStack::applyPending() {
  auto queue = std::exchange(pending_, {});
  for (const auto& p : queue) {
    if (p.kind == Pending::Kind::Navigate) {
      doNavigate(p.id, p.params);
    } else {
      doBack();
    }
  }
}

void ScreenStack::doNavigate(ScreenId id, const ScreenParams& params) {
  const auto it = factories_.find(id);
  if (it == factories_.end()) return;  // unknown screen: ignore (logged by the caller)
  if (!stack_.empty()) stack_.back()->onExit();
  if (isHub(id)) {
    stack_.clear();  // hub screens reset history, so "back" from the World never returns to Login
  }
  stack_.push_back(it->second());
  stack_.back()->onEnter(params);
}

void ScreenStack::doBack() {
  if (stack_.size() <= 1) return;  // never pop the root
  stack_.back()->onExit();
  stack_.pop_back();
  stack_.back()->onEnter({});
}

Screen* ScreenStack::current() { return stack_.empty() ? nullptr : stack_.back().get(); }

std::optional<ScreenId> ScreenStack::currentId() const {
  if (stack_.empty()) return std::nullopt;
  return stack_.back()->id();
}

}  // namespace as::app
