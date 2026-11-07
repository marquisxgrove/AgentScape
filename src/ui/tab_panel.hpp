#pragma once

#include <optional>

#include <imgui.h>

#include <cstdint>

#include "app/screen_stack.hpp"
#include "domain/settings.hpp"

namespace as::ui {

// The right-side Tab_Panel. For now it shows only Settings (Equipment/Skills/Quest Log arrive with
// their specs). Draws inside the current ImGui region.
// The signed-in user's Questing standing, shown at the top of the panel (quest-logs Requirement 5.6).
struct XpStatus {
  int level = 1;
  std::int64_t totalXp = 0;
  std::int64_t toNextLevel = 0;   // XP still needed for the next level; 0 at the maximum level
};

struct TabPanelResult {
  bool settingsChanged = false;  // a toggle flipped this frame
  bool logOut = false;           // "Log out" pressed
  bool toTown = false;           // "To town" pressed: straight back to the World from any depth
  std::optional<bool> setKeeper; // the player confirmed turning Codex Keeper tools on (true) or off (false)
  domain::Settings settings;
  ImVec2 xpAnchor{0, 0};         // screen position of the level line, where floating "+N XP" drops start
};

// `isKeeper`: the user currently holds the KEEPER role (shows the Codex Keeper tools toggle).
// `canGoToTown`: show "To town" (the user is somewhere other than the World and may leave).
TabPanelResult drawTabPanel(const domain::Settings& current, bool isKeeper, const XpStatus& xp, bool canGoToTown);

// Forgets any half-finished confirmation (call on log out).
void resetTabPanel();

}  // namespace as::ui
