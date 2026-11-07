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
  ui::AlignTextToButton();
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
  constexpr float H = 52.0F;        // a tombstone
  constexpr float ROW = H + 12.0F;  // a row: the stone, then its strip of grass and mound
  constexpr float TOP = 10.0F;      // sky above the first row
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImU32 ink = ui::toU32(ui::PaletteColor::Ink);

  ImGui::Spacing();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float areaW = ImGui::GetContentRegionAvail().x;
  const float w = std::floor((areaW - GAP * (COLS - 1) - 8.0F) / COLS);
  const int rows = (static_cast<int>(stones_.size()) + COLS - 1) / COLS;
  const ImVec2 areaMax{origin.x + areaW, origin.y + TOP + static_cast<float>(rows) * ROW};

  // The night: a dark sky, a few stars, and a moon over the top-right corner.
  dl->AddRectFilledMultiColor(origin, areaMax, IM_COL32(24, 22, 40, 255), IM_COL32(24, 22, 40, 255),
                              IM_COL32(44, 40, 58, 255), IM_COL32(44, 40, 58, 255));
  for (int s = 0; s < 18; ++s) {   // fixed positions: the sky doesn't twinkle at random
    const float sx = origin.x + std::fmod(static_cast<float>(s) * 97.0F + 13.0F, areaW - 4.0F);
    const float sy = origin.y + std::fmod(static_cast<float>(s) * 53.0F + 5.0F, areaMax.y - origin.y - 8.0F);
    dl->AddRectFilled({sx, sy}, {sx + 1, sy + 1}, IM_COL32(200, 196, 170, s % 3 == 0 ? 200 : 110));
  }
  // The moon hangs in the open sky beside a short last row (a full last row leaves no sky for it).
  const int lastRowCount = static_cast<int>(stones_.size()) - (rows - 1) * COLS;
  if (lastRowCount <= COLS - 2) {
    const float gapW = static_cast<float>(COLS - lastRowCount) * (w + GAP) / 2.0F;
    const ImVec2 moon{std::floor(areaMax.x - gapW / 2.0F), origin.y + TOP + static_cast<float>(rows - 1) * ROW + 20.0F};
    dl->AddCircleFilled(moon, 14.0F, IM_COL32(216, 210, 176, 50));
    dl->AddCircleFilled(moon, 10.0F, IM_COL32(222, 216, 182, 255));
    dl->AddCircleFilled({moon.x + 3, moon.y - 3}, 2.0F, IM_COL32(190, 182, 150, 255));
    dl->AddCircleFilled({moon.x - 4, moon.y + 3}, 1.5F, IM_COL32(190, 182, 150, 255));
  }

  // Ground fog drifts through the sky behind the stones (the town's fog; still with Reduce motion), so it never
  // dims the lettering.
  if (!fogInit_) {
    fog_.initWorld({});
    fogInit_ = true;
  }
  fog_.step(ctx.dt, ctx.reduceMotion);
  fog_.drawFog(dl, ui::ctx().mapTextures(), origin, areaMax);

  for (int r = 0; r < rows; ++r) {   // each row stands on its own strip of grass
    const float gy = origin.y + TOP + static_cast<float>(r) * ROW + H;
    dl->AddRectFilled({origin.x, gy - 3}, {areaMax.x, gy + 9}, IM_COL32(63, 80, 41, 255));
    dl->AddRectFilled({origin.x, gy - 3}, {areaMax.x, gy - 1}, IM_COL32(74, 93, 50, 255));
  }

  ImFont* font = ImGui::GetFont();
  const float fs = ImGui::GetFontSize();
  const auto centred = [&](const ImVec2& p0, float y, std::string text, ImU32 color) {
    // Long names lose letters (ending in "..") rather than spill over the stone's edge.
    const float room = w - 10.0F;
    if (ImGui::CalcTextSize(text.c_str()).x > room) {
      while (text.size() > 1 && ImGui::CalcTextSize((text + "..").c_str()).x > room) text.pop_back();
      text += "..";
    }
    const float x = std::floor(p0.x + (w - ImGui::CalcTextSize(text.c_str()).x) / 2.0F);
    dl->AddText(font, fs, {x + 1, y + 1}, ink, text.c_str());
    dl->AddText(font, fs, {x, y}, color, text.c_str());
  };

  for (std::size_t i = 0; i < stones_.size(); ++i) {
    const auto& t = stones_[i];
    const int row = static_cast<int>(i) / COLS;
    const int inRow = std::min(COLS, static_cast<int>(stones_.size()) - row * COLS);
    const float indent = static_cast<float>(COLS - inRow) * (w + GAP) / 2.0F;   // a short last row is centred
    const ImVec2 p0{std::floor(origin.x + 4.0F + indent + static_cast<float>(static_cast<int>(i) % COLS) * (w + GAP)),
                    origin.y + TOP + static_cast<float>(row) * ROW};
    const ImVec2 p1{p0.x + w, p0.y + H};
    ImGui::SetCursorScreenPos(p0);
    ImGui::PushID(t.noteId.c_str());
    if (ImGui::InvisibleButton("##grave", {w, H})) open(ctx, t);
    const bool active = ImGui::IsItemHovered() || ImGui::IsItemFocused();
    if (active) shown_ = static_cast<int>(i);

    // The stone: a weathered face with a lit top edge, settled into a mound of earth.
    if (active) dl->AddRectFilled({p0.x - 3, p0.y - 3}, {p1.x + 3, p1.y}, IM_COL32(255, 255, 160, 40), 20.0F, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilled(p0, p1, IM_COL32(111, 103, 91, 255), 18.0F, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilled({p0.x + 3, p0.y + 3}, {p1.x - 3, p0.y + 12}, IM_COL32(130, 122, 108, 255), 15.0F, ImDrawFlags_RoundCornersTop);
    dl->AddRect(p0, p1, active ? ui::toU32(ui::PaletteColor::TextYellow) : ink, 18.0F, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilled({p0.x - 2, p1.y - 2}, {p1.x + 2, p1.y + 5}, IM_COL32(90, 74, 51, 255), 3.0F);
    dl->AddRect({p0.x - 2, p1.y - 2}, {p1.x + 2, p1.y + 5}, ink, 3.0F);

    centred(p0, p0.y + 7, "RIP", ui::toU32(ui::PaletteColor::Parchment));
    centred(p0, p0.y + 21, t.agentName, ui::toU32(ui::PaletteColor::TextYellow));
    centred(p0, p0.y + 35, skillLabel(t.skill), ui::toU32(ui::PaletteColor::TextOrange));
    if (active) ui::setHoverText(ui::hoverText("Examine", t.agentName, ui::PaletteColor::TextCyan, 0));
    ImGui::PopID();
  }

  dl->AddRect(origin, areaMax, ui::toU32(ui::PaletteColor::StoneDark));

  ImGui::SetCursorScreenPos({origin.x, areaMax.y});
  ImGui::Dummy({areaW, 4.0F});

  // The epitaph of the hovered or focused grave, in full.
  const auto& t = stones_[static_cast<std::size_t>(std::clamp(shown_, 0, static_cast<int>(stones_.size()) - 1))];
  ui::ShadowText("Here lies " + skillLabel(t.skill) + " for " + t.agentName + ":", ui::PaletteColor::TextOrange);
  ui::RichText(ui::escapeMarkup("\"" + t.text + "\""), ui::PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
  ui::ShadowText(std::to_string(t.agree) + " agree, " + std::to_string(t.disagree) + " disagree.  Click a grave to examine.",
                 ui::PaletteColor::Parchment);
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
