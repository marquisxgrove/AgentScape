#include "screens/world/world_screen.hpp"

#include <imgui.h>

#include <string>
#include <vector>

#include "app/context_request.hpp"
#include "ui/chatbox.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
// A placeholder hotspot list until the real World_Map (task 9). Each exposes a default action,
// Examine text, and (via the context menu) the classic right-click options.
struct Hotspot {
  const char* name;
  const char* action;      // default action verb
  const char* target;      // screen id the action leads to
  const char* examine;     // witty, factual examine text
};
constexpr Hotspot HOTSPOTS[] = {
    {"Grand Codex", "Enter", "codex", "A towering library of familiars and their deeds."},
    {"Tavern", "Enter", "tavern", "Where adventurers swap tales of their quests."},
    {"Hiscores Obelisk", "Read", "hiscores", "A stone that remembers every champion."},
    {"Costumer", "Enter", "costumer", "Racks of tunics, hoods, and the occasional cursed hat."},
};
}  // namespace

void WorldScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##world", "Promptmoor"};
  ui::ShadowText("You stand in the town square. Choose a place to visit:", ui::PaletteColor::Parchment);
  if (ctx.kiroween) {
    ui::RichText("<col=pumpkin>Fog drifts between the buildings. Something stirs in the graveyard.</col>");
  }
  ImGui::Spacing();

  for (const auto& h : HOTSPOTS) {
    const std::string label = std::string(h.name) + "##hs-" + h.target;
    const std::string hover = ui::hoverText(h.action, h.name, ui::PaletteColor::TextCyan, /*moreOptions=*/1);
    if (ui::PixelButton(label.c_str(), {200, 0}, hover)) {
      // Left click = default action (navigation lands with each destination's spec).
      ui::pushGameMessage(std::string(h.action) + " " + h.name + "...");
    }
    // Right click (or the context key) on the focused/hovered button opens the menu.
    const bool rightClick = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    const bool menuKey = ImGui::IsItemFocused() &&
                         (ImGui::IsKeyPressed(ImGuiKey_Menu) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp));
    if ((rightClick || menuKey) && ctx.contextRequest != nullptr) {
      const ImVec2 at = ImGui::GetItemRectMin();
      std::vector<as::domain::MenuAction> actions{{h.target, std::string(h.action) + " " + h.name, h.target, true}};
      ctx.contextRequest->open(actions, std::string(h.examine), at.x + 8, at.y + 8);
    }
  }
}

}  // namespace as::screens
