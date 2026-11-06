#include "domain/context_menu.hpp"

#include <algorithm>

namespace as::domain {

std::vector<MenuAction> buildMenu(std::span<const MenuAction> actions, std::optional<std::string_view> examineText) {
  std::vector<MenuAction> menu;
  menu.reserve(actions.size() + 2);

  const auto defaultIt = std::find_if(actions.begin(), actions.end(), [](const MenuAction& a) { return a.isDefault; });
  if (defaultIt != actions.end()) {
    menu.push_back(*defaultIt);
  }
  for (auto it = actions.begin(); it != actions.end(); ++it) {
    if (it != defaultIt) {
      menu.push_back(*it);
    }
  }
  if (examineText) {
    menu.push_back(MenuAction{"examine", "Examine", std::string(*examineText), false});
  }
  menu.push_back(MenuAction{"cancel", "Cancel", "", false});
  return menu;
}

}  // namespace as::domain
