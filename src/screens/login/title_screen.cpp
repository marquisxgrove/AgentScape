#include "screens/login/title_screen.hpp"

#include <imgui.h>

#include "ui/widgets.hpp"

namespace as::screens {

void TitleScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##title", "Welcome to AgentScape"};
  ui::ShadowText("Rate your familiars, brave adventurer.");
  ImGui::Spacing();
  if (ctx.kiroween) ui::ShadowText("Kiroween is upon Promptmoor...", ui::PaletteColor::KiroweenPumpkin);
  ui::ShadowText("The login screen arrives with game-shell task 7.", ui::PaletteColor::Parchment);
}

}  // namespace as::screens
