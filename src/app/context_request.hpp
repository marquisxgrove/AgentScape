#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/context_menu.hpp"

namespace as::app {

// A screen writes this during draw() to ask App to open the Context_Menu this frame.
struct ContextRequest {
  bool requested = false;
  std::vector<domain::MenuAction> actions;
  std::optional<std::string> examineText;
  float x = 0;
  float y = 0;

  void open(std::vector<domain::MenuAction> a, std::optional<std::string> examine, float px, float py) {
    requested = true;
    actions = std::move(a);
    examineText = std::move(examine);
    x = px;
    y = py;
  }
};

}  // namespace as::app
