#include "screens/hiscores/hiscores_screen.hpp"

#include <imgui.h>

#include <cstdint>

#include "app/services.hpp"
#include "domain/xp.hpp"
#include "game/avatar_renderer.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
constexpr int BOARD_COUNT = 12;   // Overall, Total level, then the ten Skills

std::string withCommas(std::int64_t n) {
  std::string s = std::to_string(n);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
  return s;
}
}  // namespace

int HiscoresScreen::boardIndex() const {
  switch (board_.kind) {
    case domain::AgentBoardKind::Overall: return 0;
    case domain::AgentBoardKind::Total: return 1;
    case domain::AgentBoardKind::Skill: return 2 + static_cast<int>(board_.skill);
  }
  return 0;
}

void HiscoresScreen::setBoardIndex(int index) {
  if (index == 0) {
    board_.kind = domain::AgentBoardKind::Overall;
  } else if (index == 1) {
    board_.kind = domain::AgentBoardKind::Total;
  } else {
    board_.kind = domain::AgentBoardKind::Skill;
    board_.skill = domain::SKILLS[static_cast<std::size_t>(index - 2)];
  }
}

void HiscoresScreen::onEnter(const app::ScreenParams& params) {
  dirty_ = true;   // ratings and XP may have moved while another screen was open
  if (const auto it = params.find("tab"); it != params.end()) tab_ = it->second == "adventurers" ? Tab::Adventurers : Tab::Agents;
  if (const auto it = params.find("board"); it != params.end()) setBoardIndex(std::stoi(it->second));   // dev QA
  if (const auto it = params.find("q"); it != params.end()) {
    std::snprintf(search_.data(), search_.size(), "%s", it->second.c_str());
    pendingSearch_ = it->second;
  }
}

void HiscoresScreen::refresh(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  highlight_.reset();
  searchMissed_ = false;
  if (ctx.services == nullptr) return;
  if (tab_ == Tab::Agents) {
    auto r = ctx.services->hiscores.agents(board_, page_, pendingSearch_);
    if (!r) {
      error_ = r.error().message;
      return;
    }
    agents_ = std::move(*r);
    page_ = agents_->slice.page;
    highlight_ = agents_->match;
    searchMissed_ = agents_->searchMissed;
  } else {
    auto r = ctx.services->hiscores.adventurers(page_, pendingSearch_);
    if (!r) {
      error_ = r.error().message;
      return;
    }
    adventurers_ = std::move(*r);
    page_ = adventurers_->slice.page;
    highlight_ = adventurers_->match;
    searchMissed_ = adventurers_->searchMissed;
  }
  pendingSearch_.clear();
}

void HiscoresScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##hiscores", "Hiscores Obelisk"};
  if (dirty_) refresh(ctx);
  drawControls(ctx);
  if (!error_.empty()) {
    ui::ShadowText(error_, ui::PaletteColor::TextOrange);
    return;
  }
  if (tab_ == Tab::Agents) {
    drawAgents(ctx);
  } else {
    drawAdventurers(ctx);
  }
  drawPaging();
}

void HiscoresScreen::drawControls(app::FrameCtx& ctx) {
  if (ui::PixelButton("Back", {64, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  if (ui::PixelButton(tab_ == Tab::Agents ? "[Familiars]##tab" : "Familiars##tab", {0, 0}) && tab_ != Tab::Agents) {
    tab_ = Tab::Agents;
    page_ = 0;
    dirty_ = true;
  }
  ImGui::SameLine();
  if (ui::PixelButton(tab_ == Tab::Adventurers ? "[Adventurers]##tab" : "Adventurers##tab", {0, 0}) && tab_ != Tab::Adventurers) {
    tab_ = Tab::Adventurers;
    page_ = 0;
    dirty_ = true;
  }

  bool first = false;
  if (tab_ == Tab::Agents) {
    if (codex::flowButton(std::string("Board: ") + domain::boardName(board_), "board", first)) {
      setBoardIndex((boardIndex() + 1) % BOARD_COUNT);
      page_ = 0;
      dirty_ = true;
    }
    if (codex::flowButton(std::string("Quest: ") + (board_.scope ? domain::displayName(*board_.scope) : "All"), "hscope", first)) {
      int cur = -1;
      for (std::size_t i = 0; i < domain::QUEST_TYPES.size(); ++i) {
        if (board_.scope && domain::QUEST_TYPES[i] == *board_.scope) cur = static_cast<int>(i);
      }
      const int next = codex::cycleNext(cur, static_cast<int>(domain::QUEST_TYPES.size()));
      board_.scope = next < 0 ? std::nullopt : std::optional{domain::QUEST_TYPES[static_cast<std::size_t>(next)]};
      page_ = 0;
      dirty_ = true;
    }
  }
  ImGui::SetNextItemWidth(130);
  const bool enter = ImGui::InputTextWithHint("##hsearch", "Search by name", search_.data(), search_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (ui::PixelButton("Find##search", {0, 0}) || enter) {
    pendingSearch_ = search_.data();
    dirty_ = true;
  }
  if (searchMissed_) {
    ImGui::SameLine();
    ui::ShadowText("Nobody by that name.", ui::PaletteColor::TextOrange);
  }
  ImGui::Spacing();
}

void HiscoresScreen::drawAgents(app::FrameCtx& ctx) {
  if (!agents_) return;
  if (agents_->rows.empty()) {
    ui::ShadowText("No familiar is ranked on this board yet.", ui::PaletteColor::Parchment);
    return;
  }
  const float reserve = ImGui::GetFrameHeightWithSpacing() + 6.0F;
  if (!ImGui::BeginTable("##agentboard", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
                         {0, ImGui::GetContentRegionAvail().y - reserve})) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 36.0F);
  ImGui::TableSetupColumn("Familiar", ImGuiTableColumnFlags_WidthStretch, 3.0F);
  ImGui::TableSetupColumn("Vendor", ImGuiTableColumnFlags_WidthStretch, 3.0F);
  ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 44.0F);
  ImGui::TableHeadersRow();
  for (std::size_t i = 0; i < agents_->rows.size(); ++i) {
    const auto& r = agents_->rows[i];
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(std::to_string(r.rank).c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(r.agent.slug.c_str());
    const bool marked = highlight_ && *highlight_ == i;   // the search found this one
    if (ImGui::Selectable((std::string(marked ? "> " : "") + r.agent.name).c_str(), marked, ImGuiSelectableFlags_SpanAllColumns) &&
        ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::CodexCard, {{"agent", r.agent.slug}});
    }
    ImGui::PopID();
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(r.agent.vendor.c_str());
    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(std::to_string(r.score).c_str());
  }
  ImGui::EndTable();
}

void HiscoresScreen::drawAdventurers(app::FrameCtx& ctx) {
  if (!adventurers_) return;
  if (adventurers_->rows.empty()) {
    ui::ShadowText("Nobody has earned any XP yet.", ui::PaletteColor::Parchment);
    return;
  }
  const float reserve = ImGui::GetFrameHeightWithSpacing() + 6.0F;
  if (!ImGui::BeginTable("##advboard", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
                         {0, ImGui::GetContentRegionAvail().y - reserve})) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 36.0F);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 38.0F);
  ImGui::TableSetupColumn("Adventurer", ImGuiTableColumnFlags_WidthStretch, 4.0F);
  ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 44.0F);
  ImGui::TableSetupColumn("XP", ImGuiTableColumnFlags_WidthFixed, 70.0F);
  ImGui::TableHeadersRow();
  for (std::size_t i = 0; i < adventurers_->rows.size(); ++i) {
    const auto& r = adventurers_->rows[i];
    ImGui::TableNextRow(0, 28.0F);
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(std::to_string(r.rank).c_str());
    ImGui::TableSetColumnIndex(1);
    if (r.look && ctx.avatars != nullptr) {   // the top half of the sprite: head and shoulders
      if (SDL_Texture* tex = ctx.avatars->texture(r.look->appearance, r.look->loadout, domain::Facing::S, 0)) {
        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex)),
                     {static_cast<float>(ctx.avatars->cellWidth()), static_cast<float>(ctx.avatars->cellHeight()) * 0.5F}, {0, 0}, {1, 0.5F});
      }
    }
    ImGui::TableSetColumnIndex(2);
    const bool marked = highlight_ && *highlight_ == i;
    ImGui::PushID(r.userId.c_str());
    ImGui::Selectable((std::string(marked ? "> " : "") + r.name).c_str(), marked, ImGuiSelectableFlags_SpanAllColumns);
    ImGui::PopID();
    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(std::to_string(r.level).c_str());
    ImGui::TableSetColumnIndex(4);
    ImGui::TextUnformatted(withCommas(r.xp).c_str());
  }
  ImGui::EndTable();
}

void HiscoresScreen::drawPaging() {
  const std::size_t pages = tab_ == Tab::Agents ? (agents_ ? agents_->slice.pages : 1) : (adventurers_ ? adventurers_->slice.pages : 1);
  const std::size_t total = tab_ == Tab::Agents ? (agents_ ? agents_->total : 0) : (adventurers_ ? adventurers_->total : 0);
  ImGui::BeginDisabled(page_ == 0);
  if (ui::PixelButton("<##prev", {32, 0})) {
    --page_;
    dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ui::ShadowText("Page " + std::to_string(page_ + 1) + "/" + std::to_string(pages) + "  (" + std::to_string(total) + " ranked)",
                 ui::PaletteColor::Parchment);
  ImGui::SameLine();
  ImGui::BeginDisabled(page_ + 1 >= pages);
  if (ui::PixelButton(">##next", {32, 0})) {
    ++page_;
    dirty_ = true;
  }
  ImGui::EndDisabled();
}

}  // namespace as::screens
