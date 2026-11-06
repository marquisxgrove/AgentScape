#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "platform/github_device_flow.hpp"
#include "services/profile_service.hpp"

namespace as::screens {

// game-shell Requirement 2: "Welcome to AgentScape", Play as Guest, continue-as list.
// "Sign in with GitHub" appears only on desktop builds with a configured client id (Req 2.4/2.5).
class LoginScreen : public app::Screen {
 public:
  LoginScreen();
  ~LoginScreen() override;
  app::ScreenId id() const override { return app::ScreenId::Login; }
  void onEnter(const app::ScreenParams& params) override;
  void onExit() override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void drawGitHubFlow(app::FrameCtx& ctx);
  void drawChoices(app::FrameCtx& ctx);
  std::unique_ptr<platform::GitHubDeviceFlow> github_;
  bool githubActive_ = false;

  void refreshUsers(app::FrameCtx& ctx);
  void afterSignIn(app::FrameCtx& ctx, const services::User& user, bool remember);

  std::vector<services::User> users_;
  std::string error_;
  bool loaded_ = false;
};

}  // namespace as::screens
