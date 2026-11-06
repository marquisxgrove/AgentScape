#pragma once

#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "services/profile_service.hpp"

namespace as::screens {

// game-shell Requirement 2: "Welcome to AgentScape", Play as Guest, continue-as list.
// GitHub sign-in (desktop, task 7.2) is not wired yet; the button is always hidden for now.
class LoginScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Login; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void refreshUsers(app::FrameCtx& ctx);
  void afterSignIn(app::FrameCtx& ctx, const services::User& user, bool remember);

  std::vector<services::User> users_;
  std::string error_;
  bool loaded_ = false;
};

}  // namespace as::screens
