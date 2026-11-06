#include "screens/login/login_screen.hpp"

#include <imgui.h>

#include "app/services.hpp"
#include "domain/settings.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

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
  // Req 2.6: no Adventurer yet -> Costumer, else World. Adventurers arrive with character-creator,
  // so for now everyone lands on the World hub.
  ctx.nav->navigate(app::ScreenId::World);
}

void LoginScreen::draw(app::FrameCtx& ctx) {
  if (!loaded_) refreshUsers(ctx);

  ui::StonePanel panel{"##login", "Welcome to AgentScape"};
  ui::ShadowText("Choose how to enter Promptmoor.", ui::PaletteColor::Parchment);
  ImGui::Spacing();

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
