#include "screens/quest/quest_widgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>

#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens::quest {

namespace {
constexpr float GEM = 13.0F;
constexpr float GAP = 3.0F;

void drawGem(ImDrawList* dl, ImVec2 topLeft, bool filled, bool hot) {
  const float h = GEM * 0.5F;
  const ImVec2 c{topLeft.x + h, topLeft.y + h};
  const ImVec2 pts[4] = {{c.x, c.y - h}, {c.x + h, c.y}, {c.x, c.y + h}, {c.x - h, c.y}};
  if (filled) {
    dl->AddConvexPolyFilled(pts, 4, ui::toU32(ui::PaletteColor::TextOrange));
    const ImVec2 shine[3] = {{c.x, c.y - h + 2}, {c.x + 3, c.y - 1}, {c.x - 3, c.y - 1}};
    dl->AddConvexPolyFilled(shine, 3, ui::toU32(ui::PaletteColor::TextYellow));
  } else {
    dl->AddConvexPolyFilled(pts, 4, ui::toU32(ui::PaletteColor::StoneDark));
  }
  dl->AddPolyline(pts, 4, hot ? ui::toU32(ui::PaletteColor::TextYellow) : ui::toU32(ui::PaletteColor::Ink),
                  ImDrawFlags_Closed, 1.0F);
}
}  // namespace

bool gemRating(const char* id, int* value, int minValue) {
  bool changed = false;
  ImGui::PushID(id);
  const int before = *value;
  if (ui::PixelButton("-", {24, 0})) *value = std::max(minValue, *value - 1);
  ImGui::SameLine(0, 4);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  const float lineH = ImGui::GetFrameHeight();
  const float top = start.y + (lineH - GEM) * 0.5F;
  int hovered = 0;
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);   // the +/- buttons are the keyboard and gamepad controls
  for (int i = 0; i < 5; ++i) {
    ImGui::SetCursorScreenPos({start.x + static_cast<float>(i) * (GEM + GAP), start.y});
    ImGui::PushID(i);
    if (ImGui::InvisibleButton("##gem", {GEM, lineH})) {
      *value = (*value == i + 1 && minValue == 0) ? 0 : i + 1;   // clicking the current top gem clears an optional rating
    }
    if (ImGui::IsItemHovered()) hovered = i + 1;
    ImGui::PopID();
  }
  ImGui::PopItemFlag();
  for (int i = 0; i < 5; ++i) {
    const bool filled = i < (hovered > 0 ? hovered : *value);
    drawGem(dl, {start.x + static_cast<float>(i) * (GEM + GAP), top}, filled, hovered > 0 && i < hovered);
  }
  ImGui::SetCursorScreenPos({start.x + 5.0F * (GEM + GAP) + 2.0F, start.y});
  ImGui::Dummy({0, lineH});
  ImGui::SameLine(0, 2);
  if (ui::PixelButton("+", {24, 0})) *value = std::min(5, *value + 1);
  ImGui::SameLine(0, 6);
  ui::ShadowText(*value > 0 ? std::to_string(*value) + "/5" : std::string("-"),
                 *value > 0 ? ui::PaletteColor::TextYellow : ui::PaletteColor::Parchment);
  changed = *value != before;
  ImGui::PopID();
  return changed;
}

}  // namespace as::screens::quest
