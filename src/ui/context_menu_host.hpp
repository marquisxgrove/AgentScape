#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/context_menu.hpp"
#include "ui/context_menu_model.hpp"

namespace as::ui {

// ImGui host for the Context_Menu. Owns a ContextMenuModel and drives it from pointer, keyboard
// (arrows/Enter/Escape) and gamepad (D-pad/A/B) input. Opens at the pointer or focus rect.
class ContextMenuHost {
 public:
  // Opens with the given actions and optional examine text at (x, y) frame pixels.
  void open(std::vector<domain::MenuAction> actions, std::optional<std::string> examineText, float x, float y);
  bool isOpen() const { return model_.isOpen(); }

  // Draws the menu and processes input. Returns the chosen action id when one is confirmed this
  // frame (never "cancel"); std::nullopt otherwise. Closing with Escape/outside yields nullopt.
  std::optional<domain::MenuAction> draw();

  // Confirms the currently-selected item programmatically (used by the headless smoke test).
  std::optional<domain::MenuAction> confirmSelected() { return model_.confirm(); }

 private:
  ContextMenuModel model_;
  bool justOpened_ = false;
};

}  // namespace as::ui
