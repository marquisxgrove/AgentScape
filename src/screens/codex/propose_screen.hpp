#pragma once

#include <array>
#include <optional>
#include <string>

#include "app/screen_stack.hpp"
#include "domain/codex_validation.hpp"
#include "services/agent_service.hpp"

namespace as::screens {

// "Propose a familiar" (agent-codex Requirement 4): a form for adding a missing Agent. The rules live in
// domain::validateAgentInput and AgentService::proposeAgent; this class only collects text and shows the
// message next to the field that failed.
class ProposeScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Propose; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void submit(app::FrameCtx& ctx);
  void fieldError(const char* field);

  std::array<char, 64> name_{};
  std::array<char, 64> vendor_{};
  std::array<char, 256> examine_{};
  std::array<char, 1100> about_{};
  std::array<char, 256> website_{};
  domain::AgentKind kind_ = domain::AgentKind::Model;

  std::string errorField_;
  std::string errorText_;
  std::optional<std::string> openSlug_;  // an approved Agent already has this name
  bool submitOnEnter_ = false;  // dev QA only
  bool done_ = false;
};

}  // namespace as::screens
