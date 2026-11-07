#include "ui/tab_panel.hpp"

#include <imgui.h>

#include <string>

#include "ui/widgets.hpp"

namespace as::ui {

namespace {
bool g_confirmKeeper = false;  // the on/off confirmation is showing (one Tab_Panel exists, so one flag)
}

TabPanelResult drawTabPanel(const domain::Settings& current, bool isKeeper, const XpStatus& xp, bool canGoToTown) {
  TabPanelResult result;
  result.settings = current;

  StonePanel panel{"##settings", "Settings"};
  // The XP counter: level, total, and what the next level needs.
  result.xpAnchor = ImGui::GetCursorScreenPos();
  ShadowText("Questing level " + std::to_string(xp.level), PaletteColor::TextYellow);
  ShadowText("XP " + std::to_string(xp.totalXp) +
                 (xp.toNextLevel > 0 ? "  (" + std::to_string(xp.toNextLevel) + " to go)" : "  (max level)"),
             PaletteColor::Parchment);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  result.settingsChanged |= Toggle("Plain font", &result.settings.plainFont, "Switch to a legible font");
  result.settingsChanged |= Toggle("Reduce motion", &result.settings.reduceMotion, "Calm the fog, idle and fireworks");
  result.settingsChanged |= Toggle("Sound effects", &result.settings.sound, "Play 8-bit UI clicks");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  // Codex Keeper tools turn on and off only after a confirmation (agent-codex Requirement 8.1).
  bool keeperToggle = isKeeper;
  if (Toggle("Codex Keeper tools", &keeperToggle, "Review proposals and reports in your local Codex")) {
    g_confirmKeeper = true;
  }
  if (g_confirmKeeper) {
    ShadowText(isKeeper ? "Give up the Keeper tools?" : "Become a Codex Keeper?", PaletteColor::TextOrange);
    if (!isKeeper) ShadowText("Only on this device.", PaletteColor::Parchment);
    if (PixelButton("Yes##keeper", {60, 0})) {
      result.setKeeper = !isKeeper;
      g_confirmKeeper = false;
    }
    ImGui::SameLine();
    if (PixelButton("No##keeper", {60, 0})) g_confirmKeeper = false;
  }
  ImGui::Spacing();
  if (canGoToTown) {
    if (PixelButton("To town", {0, 0}, hoverText("Return", "to town", PaletteColor::TextCyan, 0))) result.toTown = true;
    ImGui::SameLine();
  }
  if (PixelButton("Log out", {0, 0}, hoverText("Log out", "", PaletteColor::TextCyan, 0))) result.logOut = true;

  return result;
}

void resetTabPanel() { g_confirmKeeper = false; }

}  // namespace as::ui
