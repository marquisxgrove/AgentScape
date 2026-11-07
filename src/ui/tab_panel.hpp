#pragma once

#include <optional>

#include "app/screen_stack.hpp"
#include "domain/settings.hpp"

namespace as::ui {

// The right-side Tab_Panel. For now it shows only Settings (Equipment/Skills/Quest Log arrive with
// their specs). Draws inside the current ImGui region.
struct TabPanelResult {
  bool settingsChanged = false;  // a toggle flipped this frame
  bool logOut = false;           // "Log out" pressed
  std::optional<bool> setKeeper; // the player confirmed turning Codex Keeper tools on (true) or off (false)
  domain::Settings settings;
};

// `isKeeper`: the user currently holds the KEEPER role (shows the Codex Keeper tools toggle).
TabPanelResult drawTabPanel(const domain::Settings& current, bool isKeeper);

// Forgets any half-finished confirmation (call on log out).
void resetTabPanel();

}  // namespace as::ui
