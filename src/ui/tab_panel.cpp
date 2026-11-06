#include "ui/tab_panel.hpp"

#include <imgui.h>

#include "ui/widgets.hpp"

namespace as::ui {

TabPanelResult drawTabPanel(const domain::Settings& current) {
  TabPanelResult result;
  result.settings = current;

  StonePanel panel{"##settings", "Settings"};
  result.settingsChanged |= Toggle("Plain font", &result.settings.plainFont, "Switch to a legible font");
  result.settingsChanged |= Toggle("Reduce motion", &result.settings.reduceMotion, "Calm the fog, idle and fireworks");
  result.settingsChanged |= Toggle("Sound effects", &result.settings.sound, "Play 8-bit UI clicks");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  if (PixelButton("Log out", {0, 0}, hoverText("Log out", "", PaletteColor::TextCyan, 0))) result.logOut = true;

  return result;
}

}  // namespace as::ui
