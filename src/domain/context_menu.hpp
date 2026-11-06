#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace as::domain {

struct MenuAction {
  std::string id;
  std::string label;
  std::string target;
  bool isDefault = false;
};

// Default action first, other actions in input order, "Examine" (only when text is given), "Cancel" last.
std::vector<MenuAction> buildMenu(std::span<const MenuAction> actions, std::optional<std::string_view> examineText);

}  // namespace as::domain
