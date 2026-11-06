#include "ui/context_menu_host.hpp"

#include <imgui.h>

#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::ui {

namespace {
constexpr const char* POPUP_ID = "##contextmenu";
}

void ContextMenuHost::open(std::vector<domain::MenuAction> actions, std::optional<std::string> examineText, float x,
                           float y) {
  std::optional<std::string_view> examine;
  if (examineText) examine = *examineText;
  model_.open(domain::buildMenu(actions, examine), x, y);
  justOpened_ = true;
}

std::optional<domain::MenuAction> ContextMenuHost::draw() {
  if (!model_.isOpen()) return std::nullopt;

  if (justOpened_) {
    ImGui::OpenPopup(POPUP_ID);
    ImGui::SetNextWindowPos({model_.x(), model_.y()});
    justOpened_ = false;
  }

  std::optional<domain::MenuAction> chosen;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {4, 4});
  if (ImGui::BeginPopup(POPUP_ID, ImGuiWindowFlags_NoMove)) {
    // Keyboard / gamepad navigation (the popup also handles clicks on each selectable).
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown)) model_.moveDown();
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp)) model_.moveUp();

    const auto& items = model_.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
      const bool selected = i == model_.selected();
      // Agent/NPC targets would colour the label cyan; actions use yellow. Keep it simple here.
      const std::string label = items[i].label + "##cm" + std::to_string(i);
      if (ImGui::Selectable(label.c_str(), selected)) {
        chosen = model_.confirmIndex(i);
      }
      if (ImGui::IsItemHovered()) model_.moveTo(i);
    }

    const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
                         ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown);
    const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight);
    if (confirm && model_.isOpen()) {
      chosen = model_.confirm();
      ImGui::CloseCurrentPopup();
    } else if (cancel) {
      model_.cancel();
      ImGui::CloseCurrentPopup();
    }
    if (chosen || !model_.isOpen()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  } else if (model_.isOpen()) {
    // Popup dismissed by clicking outside: no side effect.
    model_.cancel();
  }
  ImGui::PopStyleVar();
  return chosen;
}

}  // namespace as::ui
