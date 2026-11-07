#include "ui/tab_panel.hpp"

#include <imgui.h>

#include <string>

#include "ui/widgets.hpp"

namespace as::ui {

namespace {
// One Tab_Panel exists, so one flag each.
bool g_confirmKeeper = false;  // the Keeper tools on/off confirmation is showing
bool g_confirmLogOut = false;  // a Guest pressed Log out and is being asked to confirm
}  // namespace

TabPanelResult drawTabPanel(const domain::Settings& current, const TabPanelView& view) {
  TabPanelResult result;
  result.settings = current;
  const XpStatus& xp = view.xp;

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
  bool keeperToggle = view.isKeeper;
  if (Toggle("Codex Keeper tools", &keeperToggle, "Review proposals and reports in your local Codex")) {
    g_confirmKeeper = true;
  }
  if (g_confirmKeeper) {
    ShadowText(view.isKeeper ? "Give up the Keeper tools?" : "Become a Codex Keeper?", PaletteColor::TextOrange);
    if (!view.isKeeper) ShadowText("Only on this device.", PaletteColor::Parchment);
    if (PixelButton("Yes##keeper", {60, 0})) {
      result.setKeeper = !view.isKeeper;
      g_confirmKeeper = false;
    }
    ImGui::SameLine();
    if (PixelButton("No##keeper", {60, 0})) g_confirmKeeper = false;
  }
  ImGui::Spacing();

  // A Guest never appears in "continue as", so logging out leaves its progress unreachable (and the idle
  // cleanup removes it a day later). Say so before doing it.
  if (g_confirmLogOut) {
    ShadowText("Log out of this Guest?", PaletteColor::TextOrange);
    ShadowText("Its progress will be lost.", PaletteColor::Parchment);
    if (PixelButton("Log out##guest", {0, 0})) {
      result.logOut = true;
      g_confirmLogOut = false;
    }
    ImGui::SameLine();
    if (PixelButton("Stay##guest", {0, 0})) g_confirmLogOut = false;
    return result;
  }
  if (view.canGoToTown) {
    if (PixelButton("To town", {0, 0}, hoverText("Return", "to town", PaletteColor::TextCyan, 0))) result.toTown = true;
    ImGui::SameLine();
  }
  if (PixelButton("Log out", {0, 0}, hoverText("Log out", "", PaletteColor::TextCyan, 0))) {
    if (view.isGuest) {
      g_confirmLogOut = true;
    } else {
      result.logOut = true;
    }
  }

  return result;
}

void resetTabPanel() {
  g_confirmKeeper = false;
  g_confirmLogOut = false;
}

}  // namespace as::ui
