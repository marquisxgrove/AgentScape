#include "screens/login/login_screen.hpp"

#include <imgui.h>

#include "app/services.hpp"
#include "ui/layout.hpp"
#include "ui/ui_context.hpp"
#include "domain/settings.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

LoginScreen::LoginScreen() : github_(std::make_unique<platform::GitHubDeviceFlow>()) {}
LoginScreen::~LoginScreen() = default;

void LoginScreen::onExit() {
  if (githubActive_) github_->cancel();  // never leave a worker polling after we leave the screen
  githubActive_ = false;
}

void LoginScreen::onEnter(const app::ScreenParams& params) {
  loaded_ = false;
  const auto it = params.find("error");
  error_ = it != params.end() ? it->second : std::string{};
}

void LoginScreen::refreshUsers(app::FrameCtx& ctx) {
  loaded_ = true;
  auto list = ctx.services->profile.listLocalUsers();
  users_ = list ? *list : std::vector<services::User>{};
}

void LoginScreen::afterSignIn(app::FrameCtx& ctx, const services::User& user, bool remember) {
  auto signed_in = ctx.services->profile.signIn(user.id);
  if (!signed_in) {
    error_ = signed_in.error().message;
    return;
  }
  if (ctx.onSignedIn) ctx.onSignedIn(signed_in->id, remember);
  // Req 2.6: no Adventurer yet -> the Costumer to create one, otherwise the World. The World is
  // navigated first because it resets the history, so "back" from the Costumer lands in town.
  ctx.nav->navigate(app::ScreenId::World);
  auto active = ctx.services->adventurers.getActive(signed_in->id);
  if (active && !*active) ctx.nav->navigate(app::ScreenId::Costumer);
}

void LoginScreen::draw(app::FrameCtx& ctx) {
  if (!loaded_) refreshUsers(ctx);

  // Kiroween bats drift across the whole viewport behind the login panel.
  if (ctx.kiroween) {
    if (!batsInit_) {
      bats_.initBats(static_cast<float>(ui::VIEWPORT.w), static_cast<float>(ui::VIEWPORT.h), 7);
      batsInit_ = true;
    }
    bats_.step(ctx.dt, ctx.reduceMotion);
    // Draw bats on the foreground list so they fly over the stone panel (atmosphere).
    const ImVec2 origin{static_cast<float>(ui::VIEWPORT.x), static_cast<float>(ui::VIEWPORT.y)};
    bats_.drawBats(ImGui::GetForegroundDrawList(), ui::ctx().mapTextures(), origin);
  }

  ui::StonePanel panel{"##login", "Welcome to AgentScape"};
  if (githubActive_) {
    drawGitHubFlow(ctx);
  } else {
    drawChoices(ctx);
  }
}

void LoginScreen::drawGitHubFlow(app::FrameCtx& ctx) {
  const auto snap = github_->poll();
  using platform::DeviceFlowState;

  switch (snap.state) {
    case DeviceFlowState::Idle:
    case DeviceFlowState::RequestingCode:
      ui::ShadowText("Contacting GitHub...", ui::PaletteColor::Parchment);
      break;
    case DeviceFlowState::AwaitingUser:
    case DeviceFlowState::Polling:
      ui::ShadowText("Enter this code at GitHub:", ui::PaletteColor::Parchment);
      ImGui::Spacing();
      ui::ShadowText(snap.userCode, ui::PaletteColor::TextOrange);
      ImGui::Spacing();
      ui::ShadowText(snap.verificationUri, ui::PaletteColor::TextCyan);
      ImGui::Spacing();
      if (ui::PixelButton("Copy code", {96, 0})) ImGui::SetClipboardText(snap.userCode.c_str());
      ImGui::SameLine();
      if (ui::PixelButton("Open page", {96, 0})) platform::openUrl(snap.verificationUri);
      break;
    case DeviceFlowState::Succeeded: {
      githubActive_ = false;
      github_->cancel();
      if (snap.profile) {
        auto user = ctx.services->profile.signInGitHub(snap.profile->id, snap.profile->login, snap.profile->displayName);
        if (user) {
          afterSignIn(ctx, *user, /*remember=*/true);
          return;
        }
        error_ = user.error().message;
      }
      return;
    }
    case DeviceFlowState::Failed:
      error_ = snap.error;  // red game-voice text; Guest login stays available (Req 2.7)
      githubActive_ = false;
      github_->cancel();
      return;
  }

  ImGui::Spacing();
  if (ui::PixelButton("Cancel", {96, 0})) {
    github_->cancel();
    githubActive_ = false;
    error_.clear();
  }
}

void LoginScreen::drawChoices(app::FrameCtx& ctx) {
  ui::ShadowText("Choose how to enter Promptmoor.", ui::PaletteColor::Parchment);
  ImGui::Spacing();

  if (platform::githubSignInAvailable()) {
    if (ui::PixelButton("Sign in with GitHub", {160, 0},
                        ui::hoverText("Sign in", "with GitHub", ui::PaletteColor::TextCyan, 0))) {
      error_.clear();
      github_->start();
      githubActive_ = true;
    }
  }

  if (ui::PixelButton("Play as Guest", {160, 0}, ui::hoverText("Play", "as Guest", ui::PaletteColor::TextCyan, 0))) {
    domain::Settings guestDefaults;
    guestDefaults.reduceMotion = ctx.reduceMotionDefault;
    auto guest = ctx.services->profile.createGuest(guestDefaults);
    if (guest) {
      afterSignIn(ctx, *guest, /*remember=*/false);  // guests are never remembered
      return;
    }
    error_ = guest.error().message;
  }

  if (!users_.empty()) {
    ImGui::Spacing();
    ui::ShadowText("Or continue as:", ui::PaletteColor::Parchment);
    for (const auto& user : users_) {
      const std::string label = user.name + "##user-" + user.id;
      if (ui::PixelButton(label.c_str(), {160, 0},
                          ui::hoverText("Play as", user.name, ui::PaletteColor::TextCyan, 0))) {
        afterSignIn(ctx, user, /*remember=*/true);
        return;
      }
    }
  }

  if (!error_.empty()) {
    ImGui::Spacing();
    ui::RichText(std::string("<col=red>") + ui::escapeMarkup(error_) + "</col>");
  }
}

}  // namespace as::screens
