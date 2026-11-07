#include "screens/quest/quest_widgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>

#include <cstdint>

#include "domain/graveyard.hpp"
#include "game/avatar_renderer.hpp"
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

void gemDisplay(ImDrawList* dl, ImVec2 topLeft, int value, float size, ImU32 fill, ImU32 outline) {
  const float h = size * 0.5F;
  for (int i = 0; i < 5; ++i) {
    const ImVec2 c{topLeft.x + static_cast<float>(i) * (size + 2.0F) + h, topLeft.y + h};
    const ImVec2 pts[4] = {{c.x, c.y - h}, {c.x + h, c.y}, {c.x, c.y + h}, {c.x - h, c.y}};
    if (i < value) dl->AddConvexPolyFilled(pts, 4, fill);
    dl->AddPolyline(pts, 4, outline, ImDrawFlags_Closed, 1.0F);
  }
}

void avatarThumb(game::AvatarRenderer* avatars, const std::optional<domain::AdventurerSnapshot>& snapshot, float scale) {
  const float w = (avatars != nullptr ? static_cast<float>(avatars->cellWidth()) : 32.0F) * scale;
  const float h = (avatars != nullptr ? static_cast<float>(avatars->cellHeight()) : 48.0F) * scale;
  SDL_Texture* tex = (avatars != nullptr && snapshot) ? avatars->texture(snapshot->appearance, snapshot->loadout, domain::Facing::S, 0) : nullptr;
  if (tex != nullptr) {
    ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex)), {w, h});
  } else {
    ImGui::Dummy({w, h});
  }
}

bool questCard(const char* id, const services::QuestLogRecord& log, const std::optional<domain::AdventurerSnapshot>& snapshot,
               game::AvatarRenderer* avatars, bool helpedByViewer, float width) {
  ImGui::PushID(id);
  const ImVec2 p0 = ImGui::GetCursorScreenPos();
  const ImVec2 p1{p0.x + width, p0.y + QUEST_CARD_HEIGHT};
  const bool chosen = ImGui::InvisibleButton("##note", {width, QUEST_CARD_HEIGHT});
  const bool hot = ImGui::IsItemHovered() || ImGui::IsItemFocused();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float fs = ImGui::GetFontSize();
  const ImU32 ink = ui::toU32(ui::PaletteColor::Ink);
  const ImU32 paper = ui::toU32(ui::PaletteColor::Parchment);
  const ImU32 dark = IM_COL32(0x3e, 0x35, 0x29, 255);

  dl->AddRectFilled(p0, p1, paper);
  dl->AddRect(p0, p1, hot ? ui::toU32(ui::PaletteColor::TextOrange) : dark, 0.0F, 0, hot ? 2.0F : 1.0F);
  dl->AddCircleFilled({p0.x + width * 0.5F, p0.y + 3.0F}, 2.5F, IM_COL32(0x9a, 0x1e, 0x1e, 255));   // the pin

  // Author: thumbnail and name.
  ImGui::SetCursorScreenPos({p0.x + 6.0F, p0.y + 8.0F});
  avatarThumb(avatars, snapshot, 1.0F);
  const std::string author = snapshot ? snapshot->name : log.authorName;
  dl->PushClipRect({p0.x + 3.0F, p0.y}, {p0.x + 52.0F, p1.y}, true);
  dl->AddText(font, fs, {p0.x + 5.0F, p0.y + 58.0F}, ink, author.c_str());
  dl->PopClipRect();

  const float x = p0.x + 54.0F;
  const float textW = width - 60.0F;
  dl->PushClipRect({x, p0.y}, {p1.x - 3.0F, p1.y - 2.0F}, true);
  float y = p0.y + 8.0F;
  const std::string head = log.agentName + (log.input.version.empty() ? "" : " " + log.input.version);
  dl->AddText(font, fs, {x, y}, ink, head.c_str());
  y += fs + 3.0F;
  dl->AddText(font, fs, {x, y}, dark, domain::displayName(log.input.questType));
  gemDisplay(dl, {x + textW - 66.0F, y - 1.0F}, log.input.overall, 8.0F, IM_COL32(0x9a, 0x1e, 0x1e, 255), ink);
  y += fs + 3.0F;
  dl->AddText(font, fs, {x, y}, ink, log.input.title.c_str());
  y += fs + 3.0F;
  const std::string excerpt = domain::shortEpitaph(log.input.body, 70);
  // The excerpt is one line of plain text; line breaks in the body become spaces here so the note stays tidy.
  std::string flat = excerpt;
  for (auto& ch : flat) {
    if (ch == '\n') ch = ' ';
  }
  dl->AddText(font, fs, {x, y}, dark, flat.c_str(), nullptr, textW);
  y = p1.y - fs - 5.0F;
  std::string foot = log.input.recommend ? "Recommends" : "Not recommended";
  for (const auto& t : log.input.pros) foot += "  +" + t;
  dl->AddText(font, fs, {x, y - fs - 2.0F}, dark, foot.c_str());
  const std::string helpful = std::string(helpedByViewer ? "You and " : "") + std::to_string(log.helpfulCount) + " found it helpful";
  dl->AddText(font, fs, {x, y}, ink, log.helpfulCount > 0 || helpedByViewer ? helpful.c_str() : "No helpful votes yet");
  dl->PopClipRect();

  ImGui::SetCursorScreenPos({p0.x, p1.y + 4.0F});   // the next card goes below
  ImGui::PopID();
  return chosen;
}

}  // namespace as::screens::quest
