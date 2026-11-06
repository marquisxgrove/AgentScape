#pragma once

#include "app/screen_stack.hpp"
#include "domain/settings.hpp"

namespace as::ui {

// The right-side Tab_Panel. For now it shows only Settings (Equipment/Skills/Quest Log arrive with
// their specs). Draws inside the current ImGui region.
struct TabPanelResult {
  bool settingsChanged = false;  // a toggle flipped this frame
  bool logOut = false;           // "Log out" pressed
  domain::Settings settings;
};

TabPanelResult drawTabPanel(const domain::Settings& current);

}  // namespace as::ui
