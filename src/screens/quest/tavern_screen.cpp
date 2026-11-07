#include "screens/quest/tavern_screen.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "app/services.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "screens/quest/quest_widgets.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void TavernScreen::onEnter(const app::ScreenParams& params) {
  dirty_ = true;   // votes, edits and deletions may have happened while a note was open
  if (const auto it = params.find("agent"); it != params.end()) {   // "View all Quest Logs" on a card
    agentSlugOnEnter_ = it->second;
    query_ = {};
  }
  if (const auto it = params.find("sort"); it != params.end()) {    // dev QA
    query_.sort = it->second == "helpful" ? services::QuestSort::MostHelpful : services::QuestSort::Newest;
  }
}

void TavernScreen::refresh(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  if (ctx.services == nullptr) return;
  if (agents_.empty()) {
    if (auto agents = ctx.services->codex.approvedAgents()) agents_ = std::move(*agents);
  }
  if (!agentSlugOnEnter_.empty()) {
    const auto it = std::find_if(agents_.begin(), agents_.end(), [&](const services::Agent& a) { return a.slug == agentSlugOnEnter_; });
    if (it != agents_.end()) query_.agentId = it->id;
    agentSlugOnEnter_.clear();
  }
  auto page = ctx.services->quests.list(query_, ctx.userId);
  if (!page) {
    page_.reset();
    error_ = page.error().message;
    return;
  }
  page_ = std::move(*page);
  query_.page = page_->slice.page;
  snapshots_.clear();
  for (const auto& log : page_->logs) snapshots_.push_back(domain::snapshotFromJson(log.snapshot));
}

void TavernScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##tavern", "The Tavern Quest Board"};
  if (dirty_) refresh(ctx);
  drawControls(ctx);
  if (!error_.empty()) {
    ui::ShadowText(error_, ui::PaletteColor::TextOrange);
    return;
  }
  if (!page_) return;
  if (page_->logs.empty()) {
    ui::ShadowText("The board is bare. Be the first to pin a quest here.", ui::PaletteColor::Parchment);
  } else {
    drawBoard(ctx);
  }

  // Paging.
  const auto& s = page_->slice;
  ImGui::BeginDisabled(s.page == 0);
  if (ui::PixelButton("<##prev", {32, 0})) {
    query_.page = s.page - 1;
    dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ui::ShadowText("Page " + std::to_string(s.page + 1) + "/" + std::to_string(s.pages) + "  (" + std::to_string(page_->total) + " quests)",
                 ui::PaletteColor::Parchment);
  ImGui::SameLine();
  ImGui::BeginDisabled(s.page + 1 >= s.pages);
  if (ui::PixelButton(">##next", {32, 0})) {
    query_.page = s.page + 1;
    dirty_ = true;
  }
  ImGui::EndDisabled();
}

void TavernScreen::drawControls(app::FrameCtx& ctx) {
  if (ui::PixelButton("Back", {64, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  if (ui::PixelButton("Write a Quest Log", {0, 0}) && ctx.nav != nullptr) {
    ctx.nav->navigate(app::ScreenId::QuestScroll, {{"agent", ""}});
  }
  bool first = false;
  std::string agentName = "All";
  if (query_.agentId) {
    const auto it = std::find_if(agents_.begin(), agents_.end(), [&](const services::Agent& a) { return a.id == *query_.agentId; });
    if (it != agents_.end()) agentName = it->name;
  }
  if (codex::flowButton("Familiar: " + agentName, "tfam", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < agents_.size(); ++i) {
      if (query_.agentId && agents_[i].id == *query_.agentId) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(agents_.size()));
    query_.agentId = next < 0 ? std::nullopt : std::optional<std::string>{agents_[static_cast<std::size_t>(next)].id};
    query_.page = 0;
    dirty_ = true;
  }
  if (codex::flowButton(std::string("Quest: ") + (query_.questType ? domain::displayName(*query_.questType) : "All"), "tqt", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < domain::QUEST_TYPES.size(); ++i) {
      if (query_.questType && domain::QUEST_TYPES[i] == *query_.questType) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(domain::QUEST_TYPES.size()));
    query_.questType = next < 0 ? std::nullopt : std::optional{domain::QUEST_TYPES[static_cast<std::size_t>(next)]};
    query_.page = 0;
    dirty_ = true;
  }
  if (codex::flowButton(query_.sort == services::QuestSort::Newest ? "Sort: Newest" : "Sort: Most helpful", "tsort", first)) {
    query_.sort = query_.sort == services::QuestSort::Newest ? services::QuestSort::MostHelpful : services::QuestSort::Newest;
    query_.page = 0;
    dirty_ = true;
  }
  ImGui::Spacing();
}

void TavernScreen::drawBoard(app::FrameCtx& ctx) {
  const float gap = 6.0F;
  const float bottomReserve = ImGui::GetFrameHeightWithSpacing() + 6.0F;
  ImGui::BeginChild("##board", {0, ImGui::GetContentRegionAvail().y - bottomReserve});
  const float colW = std::floor((ImGui::GetContentRegionAvail().x - gap - 6.0F) / 2.0F);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float cardH = quest::questCardHeight();
  for (std::size_t i = 0; i < page_->logs.size(); ++i) {
    const auto& log = page_->logs[i];
    const float col = static_cast<float>(i % 2);
    const float row = static_cast<float>(i / 2);
    ImGui::SetCursorScreenPos({origin.x + col * (colW + gap), origin.y + row * (cardH + gap)});
    if (quest::questCard(log.id.c_str(), log, snapshots_[i], ctx.avatars, page_->helpedByViewer.count(log.id) != 0, colW) &&
        ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::QuestDetail, {{"id", log.id}});
    }
  }
  const float rows = std::ceil(static_cast<float>(page_->logs.size()) / 2.0F);
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy({1.0F, rows * (cardH + gap)});   // tells the child how tall the board is
  ImGui::EndChild();
}

}  // namespace as::screens
