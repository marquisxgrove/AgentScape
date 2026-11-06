#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "domain/context_menu.hpp"

namespace as::ui {

// Plain-C++ navigation state machine for the Context_Menu (kept out of ImGui so it is unit-testable).
// Built from domain::buildMenu output: default action first, Cancel last, Examine when present.
class ContextMenuModel {
 public:
  ContextMenuModel() = default;

  // Opens with the given items at (x, y) frame pixels; selection starts on the first (default) item.
  void open(std::vector<domain::MenuAction> items, float x, float y);
  void close();
  bool isOpen() const { return open_; }

  const std::vector<domain::MenuAction>& items() const { return items_; }
  std::size_t selected() const { return selected_; }
  float x() const { return x_; }
  float y() const { return y_; }

  void moveUp();     // wraps
  void moveDown();   // wraps
  void moveTo(std::size_t index);

  // Confirms the current item (Enter / A / click). Returns the chosen action and closes the menu.
  // A "cancel" item returns nullopt (no side effect). Returns nullopt if the menu is closed.
  std::optional<domain::MenuAction> confirm();
  // Confirms a specific index (mouse click on a row).
  std::optional<domain::MenuAction> confirmIndex(std::size_t index);
  // Escape / B / click-outside: closes with no side effect.
  void cancel() { close(); }

 private:
  bool isCancel(std::size_t index) const;

  bool open_ = false;
  std::vector<domain::MenuAction> items_;
  std::size_t selected_ = 0;
  float x_ = 0;
  float y_ = 0;
};

}  // namespace as::ui
