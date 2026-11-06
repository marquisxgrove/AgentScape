#include "screens/world/world_screen.hpp"

#include <imgui.h>

#include "ui/widgets.hpp"

namespace as::screens {

void WorldScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##world", "Promptmoor"};
  ui::ShadowText("You stand in the town square.", ui::PaletteColor::Parchment);
  ImGui::Spacing();
  ui::RichText("The clickable <col=cyan>world map</col> arrives with game-shell task 9.");
  if (ctx.kiroween) {
    ImGui::Spacing();
    ui::RichText("<col=pumpkin>Fog drifts between the buildings. Something stirs in the graveyard.</col>");
  }
}

}  // namespace as::screens
