#pragma once

#include <optional>
#include <string>

#include "app/screen_stack.hpp"
#include "services/keeper_service.hpp"

namespace as::screens {

// The Codex Keeper's queue (agent-codex Requirement 8.3): pending proposals to approve or reject, unresolved
// reports to act on, and hidden Notes that can be brought back. Everything players wrote is shown as plain text.
// The role check lives in KeeperService; this screen only draws, and shows the refusal if there is one.
class KeeperScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Keeper; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void reload(app::FrameCtx& ctx);

  bool dirty_ = true;
  std::optional<services::KeeperQueue> queue_;
  std::string error_;
};

}  // namespace as::screens
