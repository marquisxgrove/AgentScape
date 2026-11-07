#include "screens/quest/quest_detail_screen.hpp"

#include <imgui.h>

#include "app/services.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "screens/quest/quest_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void QuestDetailScreen::onEnter(const app::ScreenParams& params) {
  if (const auto it = params.find("id"); it != params.end()) {   // a fresh visit; coming back passes nothing
    questLogId_ = it->second;
    reporting_ = false;
    confirmDelete_ = false;
    reason_.fill('\0');
  }
  dirty_ = true;
}

void QuestDetailScreen::reload(app::FrameCtx& ctx) {
  dirty_ = false;
  log_.reset();
  snapshot_.reset();
  if (ctx.services == nullptr) return;
  auto record = ctx.services->quests.get(questLogId_);
  if (!record || !*record) return;
  // A hidden log is for its author and the Keepers only (Requirement 4.2).
  if ((*record)->hidden && (*record)->authorId != ctx.userId && !ctx.isKeeper) return;
  log_ = std::move(**record);
  snapshot_ = domain::snapshotFromJson(log_->snapshot);
  iVoted_ = false;
  if (auto v = ctx.services->quests.hasVoted(ctx.userId, questLogId_)) iVoted_ = *v;
  agentSlug_ = log_->agentSlug;
}

void QuestDetailScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##questdetail", "Quest Log"};
  if (dirty_) reload(ctx);
  if (ui::PixelButton("Back", {64, 0}) && ctx.nav != nullptr) ctx.nav->back();
  if (!log_) {
    ui::ShadowText("That quest has been taken off the board.", ui::PaletteColor::Parchment);
    return;
  }
  const auto& log = *log_;
  const bool mine = log.authorId == ctx.userId;

  ImGui::BeginChild("##detailbody", {0, 0});
  // Header: the author as they were, and what the quest was.
  quest::avatarThumb(ctx.avatars, snapshot_, 2.0F);
  ImGui::SameLine();
  ImGui::BeginGroup();
  ui::ShadowText(log.input.title, ui::PaletteColor::TextYellow);
  ui::ShadowText((snapshot_ ? snapshot_->name : log.authorName) + " went questing with " + log.agentName +
                     (log.input.version.empty() ? "" : " (" + log.input.version + ")"),
                 ui::PaletteColor::Parchment);
  ui::ShadowText(std::string(domain::displayName(log.input.questType)) + " quest  -  " +
                     (log.input.recommend ? "would recommend" : "would not recommend"),
                 log.input.recommend ? ui::PaletteColor::TextGreen : ui::PaletteColor::TextOrange);
  {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    quest::gemDisplay(ImGui::GetWindowDrawList(), at, log.input.overall, 11.0F, ui::toU32(ui::PaletteColor::TextOrange),
                      ui::toU32(ui::PaletteColor::Ink));
    ImGui::Dummy({5 * 13.0F, 13.0F});
    ImGui::SameLine();
    ui::ShadowText(std::to_string(log.input.overall) + "/5 overall", ui::PaletteColor::TextYellow);
  }
  ImGui::EndGroup();
  ImGui::Spacing();

  // Plain text, line breaks kept (Requirement 1.6): never markup.
  {
    ui::StonePanel note{"##story", "", {0, 96}, ui::PanelVariant::Parchment};
    ImGui::PushStyleColor(ImGuiCol_Text, ui::toU32(ui::PaletteColor::Ink));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(log.input.body.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }

  if (!log.input.pros.empty() || !log.input.cons.empty()) {
    std::string good, bad;
    for (const auto& t : log.input.pros) good += (good.empty() ? "" : ", ") + t;
    for (const auto& t : log.input.cons) bad += (bad.empty() ? "" : ", ") + t;
    if (!good.empty()) ui::RichText(ui::escapeMarkup("Good: " + good), ui::PaletteColor::TextGreen, ImGui::GetContentRegionAvail().x);
    if (!bad.empty()) ui::RichText(ui::escapeMarkup("Bad: " + bad), ui::PaletteColor::TextOrange, ImGui::GetContentRegionAvail().x);
  }
  if (!log.input.skills.empty()) {
    ui::ShadowText("Skills rated:", ui::PaletteColor::TextOrange);
    for (std::size_t i = 0; i < domain::SKILLS.size(); ++i) {
      const auto it = log.input.skills.find(domain::SKILLS[i]);
      if (it == log.input.skills.end()) continue;
      codex::skillIcon(static_cast<int>(i));
      ImGui::SameLine();
      ui::ShadowText(std::string(domain::displayName(domain::SKILLS[i])) + "  " + std::to_string(it->second) + "/5", ui::PaletteColor::TextCyan);
    }
  }
  ImGui::Spacing();
  ImGui::Separator();

  ui::ShadowText(std::to_string(log.helpfulCount) + (log.helpfulCount == 1 ? " adventurer" : " adventurers") + " found this helpful.",
                 ui::PaletteColor::Parchment);
  if (log.hidden) ui::ShadowText("Hidden from the board by a Codex Keeper.", ui::PaletteColor::TextOrange);
  if (log.excluded) ui::ShadowText("Its ratings are not counted in the Codex.", ui::PaletteColor::TextOrange);

  bool first = true;
  if (!agentSlug_.empty() && codex::flowButton("Examine " + log.agentName, "examine", first) && ctx.nav != nullptr) {
    ctx.nav->navigate(app::ScreenId::CodexCard, {{"agent", agentSlug_}});
  }
  if (mine) {
    if (codex::flowButton("Edit", "edit", first) && ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::QuestScroll, {{"agent", agentSlug_}, {"edit", log.id}});
    }
    if (!confirmDelete_) {
      if (codex::flowButton("Delete", "delete", first)) confirmDelete_ = true;
    } else {
      ui::ShadowText("Delete this quest and its XP?", ui::PaletteColor::TextOrange);
      if (ui::PixelButton("Yes##del", {50, 0})) {
        auto r = ctx.services->quests.deleteQuestLog(ctx.userId, log.id);
        if (r) {
          ui::pushGameMessage("Your Quest Log has been deleted.");
          if (ctx.nav != nullptr) ctx.nav->back();
          ImGui::EndChild();
          return;
        }
        ui::pushGameMessage(r.error().message);
        confirmDelete_ = false;
      }
      ImGui::SameLine();
      if (ui::PixelButton("No##del", {50, 0})) confirmDelete_ = false;
    }
  } else {
    if (codex::flowButton(iVoted_ ? "[Helpful]" : "Helpful", "helpful", first)) {
      auto r = ctx.services->quests.toggleHelpful(ctx.userId, log.id);
      if (!r) ui::pushGameMessage(r.error().message);
      dirty_ = true;
    }
    if (codex::flowButton("Report", "report", first)) {
      reporting_ = !reporting_;
      reason_.fill('\0');
    }
  }
  if (ctx.isKeeper) {
    if (codex::flowButton(log.hidden ? "Unhide" : "Hide", "hide", first)) {
      auto r = ctx.services->quests.setQuestLogModeration(ctx.userId, log.id, !log.hidden, log.excluded);
      if (!r) ui::pushGameMessage(r.error().message);
      dirty_ = true;
    }
    if (codex::flowButton(log.excluded ? "Include ratings" : "Exclude ratings", "exclude", first)) {
      auto r = ctx.services->quests.setQuestLogModeration(ctx.userId, log.id, log.hidden, !log.excluded);
      if (!r) ui::pushGameMessage(r.error().message);
      dirty_ = true;
    }
  }
  if (reporting_) {
    ui::ShadowText("Why? (3 to 200 characters, plain text)", ui::PaletteColor::TextOrange);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
    ImGui::InputText("##reportreason", reason_.data(), reason_.size());
    if (ui::PixelButton("Send report##send", {0, 0})) {
      auto r = ctx.services->keeper.reportContent(ctx.userId, services::ReportTarget::QuestLog, log.id, reason_.data());
      if (r) {
        ui::pushGameMessage("Thank you. The Codex Keepers will take a look.");
        reporting_ = false;
      } else {
        ui::pushGameMessage(r.error().message);
      }
    }
    ImGui::SameLine();
    if (ui::PixelButton("Cancel##report", {0, 0})) reporting_ = false;
  }
  ImGui::EndChild();
}

}  // namespace as::screens
