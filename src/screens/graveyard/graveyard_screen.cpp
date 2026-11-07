#include "screens/graveyard/graveyard_screen.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <variant>

#include "app/services.hpp"
#include "domain/graveyard.hpp"
#include "domain/skills.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
std::string skillLabel(const std::optional<std::string>& skill) {
  if (skill) {
    if (const auto key = domain::statKeyFromStorage(*skill); key && std::holds_alternative<domain::Skill>(*key)) {
      return domain::displayName(std::get<domain::Skill>(*key));
    }
  }
  return "General";
}
}  // namespace

void GraveyardScreen::onEnter(const app::ScreenParams& params) {
  loaded_ = false;   // votes and hidden Notes may have changed while a card was open
  if (const auto it = params.find("view"); it != params.end()) sceneView_ = it->second != "list";   // dev QA
}

void GraveyardScreen::open(app::FrameCtx& ctx, const services::Tombstone& t) {
  if (ctx.nav != nullptr) ctx.nav->navigate(app::ScreenId::CodexCard, {{"agent", t.agentSlug}});
}

void GraveyardScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##graveyard", "Hall of Hallucinations"};
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();

  if (!ctx.kiroween) {   // Requirement 7.1: only while Kiroween is on
    ui::ShadowText("The gate is locked. The Hall of Hallucinations opens during Kiroween.", ui::PaletteColor::Parchment);
    return;
  }
  if (!loaded_ && ctx.services != nullptr) {
    loaded_ = true;
    error_.clear();
    stones_.clear();
    shown_ = 0;
    if (auto s = ctx.services->codex.tombstones()) {
      stones_ = std::move(*s);
    } else {
      error_ = s.error().message;
    }
  }
  ImGui::SameLine();
  if (ui::PixelButton(sceneView_ ? "View: Graves##gv" : "View: List##gv", {0, 0})) sceneView_ = !sceneView_;
  ImGui::SameLine();
  ui::ShadowText("Sample snapshot: fictional agents", ui::PaletteColor::Parchment);

  if (!error_.empty()) {
    ui::ShadowText(error_, ui::PaletteColor::TextOrange);
    return;
  }
  if (stones_.empty()) {
    ui::ShadowText("The graves are bare. No Bane has gathered three votes yet.", ui::PaletteColor::Parchment);
    return;
  }
  if (sceneView_) {
    drawScene(ctx);
  } else {
    drawList(ctx);
  }
}

void GraveyardScreen::drawScene(app::FrameCtx& ctx) {
  constexpr int COLS = 5;
  constexpr float GAP = 8.0F;
  constexpr float H = 58.0F;
  const float availW = ImGui::GetContentRegionAvail().x;
  const float w = std::floor((availW - GAP * (COLS - 1) - 4.0F) / COLS);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float fs = ImGui::GetFontSize();
  const ImU32 ink = ui::toU32(ui::PaletteColor::Ink);

  ImGui::Spacing();
  for (std::size_t i = 0; i < stones_.size(); ++i) {
    const auto& t = stones_[i];
    if (i % COLS != 0) ImGui::SameLine(0, GAP);
    ImGui::PushID(t.noteId.c_str());
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1{p0.x + w, p0.y + H};
    if (ImGui::InvisibleButton("##grave", {w, H})) open(ctx, t);
    const bool active = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    if (active) shown_ = static_cast<int>(i);

    dl->AddRectFilled(p0, p1, ui::toU32(ui::PaletteColor::Stone), 18.0F, ImDrawFlags_RoundCornersTop);
    dl->AddRect(p0, p1, active ? ui::toU32(ui::PaletteColor::TextYellow) : ink, 18.0F, ImDrawFlags_RoundCornersTop);
    dl->PushClipRect({p0.x + 3, p0.y}, {p1.x - 3, p1.y}, true);   // long names are clipped, not spilled
    dl->AddText(font, fs, {p0.x + (w - fs * 3.0F) / 2.0F + 1, p0.y + 7}, ink, "RIP");
    dl->AddText(font, fs, {p0.x + (w - fs * 3.0F) / 2.0F, p0.y + 6}, ui::toU32(ui::PaletteColor::Parchment), "RIP");
    dl->AddText(font, fs, {p0.x + 6, p0.y + 24}, ink, t.agentName.c_str());
    dl->AddText(font, fs, {p0.x + 5, p0.y + 23}, ui::toU32(ui::PaletteColor::TextYellow), t.agentName.c_str());
    const std::string skill = skillLabel(t.skill);
    dl->AddText(font, fs, {p0.x + 5, p0.y + 38}, ui::toU32(ui::PaletteColor::TextOrange), skill.c_str());
    dl->PopClipRect();
    if (active) ui::setHoverText(ui::hoverText("Examine", t.agentName, ui::PaletteColor::TextCyan, 0));
    ImGui::PopID();
  }

  // The epitaph of the hovered or focused grave, in full.
  ImGui::Spacing();
  ImGui::Separator();
  const auto& t = stones_[static_cast<std::size_t>(std::clamp(shown_, 0, static_cast<int>(stones_.size()) - 1))];
  ui::ShadowText("Here lies " + skillLabel(t.skill) + " for " + t.agentName + ":", ui::PaletteColor::TextOrange);
  ui::RichText(ui::escapeMarkup("\"" + t.text + "\""), ui::PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
  ui::ShadowText("+" + std::to_string(t.agree) + " / -" + std::to_string(t.disagree), ui::PaletteColor::Parchment);
}

void GraveyardScreen::drawList(app::FrameCtx& ctx) {
  if (!ImGui::BeginTable("##gravelist", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg,
                         {0, ImGui::GetContentRegionAvail().y - 4.0F})) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Agent", ImGuiTableColumnFlags_WidthStretch, 2.0F);
  ImGui::TableSetupColumn("Skill", ImGuiTableColumnFlags_WidthStretch, 1.6F);
  ImGui::TableSetupColumn("Epitaph", ImGuiTableColumnFlags_WidthStretch, 6.0F);
  ImGui::TableHeadersRow();
  for (const auto& t : stones_) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(t.noteId.c_str());
    if (ImGui::Selectable(t.agentName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
      open(ctx, t);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
      ui::setHoverText(ui::hoverText("Examine", t.agentName, ui::PaletteColor::TextCyan, 0));
    }
    ImGui::PopID();
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(skillLabel(t.skill).c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", t.text.c_str());   // plain text, never markup
  }
  ImGui::EndTable();
}

}  // namespace as::screens
