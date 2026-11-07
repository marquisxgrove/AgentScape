#include "screens/codex/codex_card_screen.hpp"

#include <imgui.h>

#include <cmath>
#include <variant>

#include "app/services.hpp"
#include "core/civil_time.hpp"
#include "domain/codex_validation.hpp"
#include "platform/github_device_flow.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void CodexCardScreen::onEnter(const app::ScreenParams& params) {
  if (const auto it = params.find("agent"); it != params.end()) {  // a fresh visit; coming back passes nothing
    slug_ = it->second;
    scope_.reset();
    if (const auto s = params.find("scope"); s != params.end()) scope_ = domain::questTypeFromStorage(s->second);
    scrollToEnd_ = params.count("scroll") != 0 ? 3 : 0;
    // A draft belongs to the card it was written on; never carry it over to another familiar.
    composing_ = false;
    composeText_.fill('\0');
    composeVersion_.fill('\0');
    composeSkill_ = -1;
    if (const auto c = params.find("compose"); c != params.end()) {
      composing_ = true;
      composeKind_ = c->second == "bane" ? services::NoteKind::Bane : services::NoteKind::Boon;
    }
    reportNoteId_.clear();
    reportAgent_ = false;
    confirmLeave_ = false;
    showAllBoons_ = showAllBanes_ = false;
  }
  dirty_ = true;
}

// Escape closes what is open on the card before it leaves the card. A closed Boon/Bane draft keeps its text,
// exactly as its Cancel button does.
bool CodexCardScreen::onEscape(app::FrameCtx&) {
  if (confirmLeave_) {
    confirmLeave_ = false;
    return true;
  }
  if (!reportNoteId_.empty() || reportAgent_) {
    reportNoteId_.clear();
    reportAgent_ = false;
    return true;
  }
  if (composing_) {
    composing_ = false;
    return true;
  }
  return false;
}

void CodexCardScreen::reload(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  card_.reset();
  allBoons_.clear();
  allBanes_.clear();
  myVotes_.clear();
  topLogs_.clear();
  if (ctx.services == nullptr) return;
  auto card = ctx.services->codex.card(slug_, scope_);
  if (!card) {
    error_ = card.error().message;
    return;
  }
  if (!*card) {
    error_ = "That familiar has vanished from the shelves.";
    return;
  }
  card_ = std::move(**card);
  if (auto votes = ctx.services->notes.myVotes(ctx.userId, card_->agent.id)) myVotes_ = std::move(*votes);
  if (auto logs = ctx.services->quests.topHelpful(card_->agent.id, 5)) topLogs_ = std::move(*logs);
  if (showAllBoons_) {
    if (auto n = ctx.services->codex.notes(card_->agent.id, services::NoteKind::Boon)) allBoons_ = *n;
  }
  if (showAllBanes_) {
    if (auto n = ctx.services->codex.notes(card_->agent.id, services::NoteKind::Bane)) allBanes_ = *n;
  }
}

void CodexCardScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##codexcard", card_ ? card_->agent.name : std::string("Examine")};
  if (dirty_) reload(ctx);

  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  if (!card_) {
    ui::ShadowText(error_.empty() ? "Nothing to examine." : error_, ui::PaletteColor::TextOrange);
    return;
  }
  ImGui::SameLine();
  bool first = false;
  if (codex::flowButton(std::string("Quest Type: ") + (scope_ ? domain::displayName(*scope_) : "All"), "scope", first)) {
    int cur = -1;
    for (std::size_t i = 0; i < domain::QUEST_TYPES.size(); ++i) {
      if (scope_ && domain::QUEST_TYPES[i] == *scope_) cur = static_cast<int>(i);
    }
    const int next = codex::cycleNext(cur, static_cast<int>(domain::QUEST_TYPES.size()));
    scope_ = next < 0 ? std::nullopt : std::optional{domain::QUEST_TYPES[static_cast<std::size_t>(next)]};
    dirty_ = true;
  }

  ImGui::BeginChild("##cardbody", {0, 0});
  drawHeader(ctx);
  drawSkills();
  drawQuestLogs(ctx);
  drawNotes(ctx, "Boons (what players praise)", services::NoteKind::Boon);
  drawNotes(ctx, "Banes (what players warn about)", services::NoteKind::Bane);
  drawCompose(ctx);
  if (scrollToEnd_ > 0) {
    ImGui::SetScrollY(ImGui::GetScrollMaxY());
    --scrollToEnd_;
  }
  ImGui::EndChild();
}

void CodexCardScreen::drawHeader(app::FrameCtx& ctx) {
  const auto& a = card_->agent;
  // The panel title already names the familiar; this line says what it is.
  ui::ShadowText(std::string(services::kindDisplay(a.kind)) + " by " + a.vendor + "  -  updated " +
                     core::formatIso8601(a.updatedAt).substr(0, 10),
                 ui::PaletteColor::Parchment);
  {
    // The card itself: dark ink on parchment (the palette's coloured text doesn't read on it). Sized to
    // its text, with the same 10 px padding StonePanel uses, so short entries don't float in a big box.
    constexpr float PAD = 10.0F;
    const float textW = ImGui::GetContentRegionAvail().x - 2 * PAD;
    const float textH = ImGui::CalcTextSize(a.examine.c_str(), nullptr, false, textW).y +
                        ImGui::GetStyle().ItemSpacing.y * 3 +
                        ImGui::CalcTextSize(a.about.c_str(), nullptr, false, textW).y;
    ui::StonePanel parchment{"##examine", "", {0, std::floor(textH + 2 * PAD + 2)}, ui::PanelVariant::Parchment};
    ImGui::PushStyleColor(ImGuiCol_Text, ui::toU32(ui::PaletteColor::Ink));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(a.examine.c_str());
    ImGui::Spacing();
    ImGui::TextUnformatted(a.about.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }

  if (a.website) {
    if (!confirmLeave_) {
      if (ui::PixelButton("Visit website", {0, 0})) confirmLeave_ = true;
    } else {
      const auto host = domain::websiteHost(*a.website).value_or("this site");
      ui::ShadowText("Leave AgentScape to visit " + host + "?", ui::PaletteColor::TextOrange);
      if (ui::PixelButton("Leave##yes", {70, 0})) {
        platform::openUrl(*a.website);
        confirmLeave_ = false;
      }
      ImGui::SameLine();
      if (ui::PixelButton("Stay##no", {70, 0})) confirmLeave_ = false;
    }
    ImGui::SameLine();
  }
  if (ui::PixelButton("Write a Quest Log", {0, 0}) && ctx.nav != nullptr) {
    app::ScreenParams p{{"agent", card_->agent.slug}};
    if (scope_) p["scope"] = domain::storageName(*scope_);
    ctx.nav->navigate(app::ScreenId::QuestScroll, p);
  }
  ImGui::SameLine();
  if (ui::PixelButton("Compare##agent", {0, 0}) && ctx.nav != nullptr) {
    app::ScreenParams p{{"add", card_->agent.slug}};
    if (scope_) p["scope"] = domain::storageName(*scope_);
    ctx.nav->navigate(app::ScreenId::Proving, p);
  }
  ImGui::SameLine();
  if (ui::PixelButton("Report##agent", {0, 0})) {
    reportAgent_ = !reportAgent_;
    reportNoteId_.clear();
    reportReason_.fill('\0');
  }
  if (reportAgent_) drawReportForm(ctx, services::ReportTarget::Agent, card_->agent.id);
  ImGui::Spacing();
}

void CodexCardScreen::drawSkills() {
  const auto& lv = card_->levels;
  ui::ShadowText("Overall " + codex::levelText(lv.overall) + "  (" + std::to_string(lv.overallCount) +
                     " Quest Logs)    Total level " + std::to_string(lv.total),
                 ui::PaletteColor::TextYellow);
  if (ImGui::BeginTable("##skills", 2, ImGuiTableFlags_SizingStretchSame)) {
    for (std::size_t i = 0; i < domain::SKILLS.size(); ++i) {
      ImGui::TableNextColumn();
      codex::skillIcon(static_cast<int>(i));
      ImGui::SameLine();
      ui::ShadowText(std::string(domain::displayName(domain::SKILLS[i])) + "  " + codex::levelText(lv.skills[i]) + " (" +
                         std::to_string(lv.counts[i]) + ")",
                     lv.skills[i] ? ui::PaletteColor::TextCyan : ui::PaletteColor::Parchment);
    }
    ImGui::EndTable();
  }
  ui::ShadowText("A skill needs 3 ratings to rank.", ui::PaletteColor::Parchment);
  ImGui::Spacing();
}

void CodexCardScreen::drawQuestLogs(app::FrameCtx& ctx) {
  ui::ShadowText("Helpful Quest Logs (what adventurers did with it)", ui::PaletteColor::TextOrange);
  if (topLogs_.empty()) {
    ui::ShadowText("Nobody has logged a quest with it yet.", ui::PaletteColor::Parchment);
  }
  for (const auto& log : topLogs_) {
    ImGui::PushID(log.id.c_str());
    // One selectable row per log: title, the gems as a number (never colour alone), who and how helpful.
    const std::string row = log.input.title + "   " + std::to_string(log.input.overall) + "/5   by " + log.authorName + "   (" +
                            std::to_string(log.helpfulCount) + " helpful)";
    if (ImGui::Selectable((row + "##row").c_str(), false) && ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::QuestDetail, {{"id", log.id}});
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
      ui::setHoverText(ui::hoverText("Read", log.input.title, ui::PaletteColor::TextCyan, 0));
    }
    ImGui::PopID();
  }
  if (ui::PixelButton("View all Quest Logs##tavern", {0, 0}) && ctx.nav != nullptr) {
    ctx.nav->navigate(app::ScreenId::Tavern, {{"agent", card_->agent.slug}});
  }
  ImGui::Spacing();
}

void CodexCardScreen::drawNotes(app::FrameCtx& ctx, const char* title, services::NoteKind kind) {
  const bool boon = kind == services::NoteKind::Boon;
  const std::size_t total = boon ? card_->boonTotal : card_->baneTotal;
  bool& showAll = boon ? showAllBoons_ : showAllBanes_;
  const auto& notes = showAll ? (boon ? allBoons_ : allBanes_) : (boon ? card_->boons : card_->banes);
  ui::ShadowText(title, ui::PaletteColor::TextOrange);
  if (notes.empty()) {
    ui::ShadowText(boon ? "No Boons yet." : "No Banes yet.", ui::PaletteColor::Parchment);
    return;
  }
  const ImU32 fill = boon ? IM_COL32(0x2e, 0x8a, 0x2e, 255) : IM_COL32(0x9a, 0x1e, 0x1e, 255);
  const ImU32 txt = boon ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
  for (const auto& n : notes) {
    ImGui::PushID(n.id.c_str());
    codex::chip(boon ? "BOON" : "BANE", fill, txt);
    ImGui::SameLine();
    std::string skillName = "General";
    if (n.skill) {
      if (const auto key = domain::statKeyFromStorage(*n.skill); key && std::holds_alternative<domain::Skill>(*key)) {
        skillName = domain::displayName(std::get<domain::Skill>(*key));
      }
    }
    ui::ShadowText(skillName + "   +" + std::to_string(n.agree) + " / -" + std::to_string(n.disagree),
                   ui::PaletteColor::Parchment);
    ui::RichText(ui::escapeMarkup(n.text), ui::PaletteColor::TextYellow, ImGui::GetContentRegionAvail().x);
    ui::ShadowText("- " + n.authorName, ui::PaletteColor::Parchment);
    if (n.authorId == ctx.userId) {
      ImGui::SameLine();
      ui::ShadowText("(your words)", ui::PaletteColor::TextCyan);
    } else {
      const auto mine = myVotes_.find(n.id);
      const bool agreed = mine != myVotes_.end() && mine->second == domain::VoteValue::Agree;
      const bool disagreed = mine != myVotes_.end() && mine->second == domain::VoteValue::Disagree;
      if (ui::PixelButton(((agreed ? "[Agree]" : "Agree") + std::string("##a")).c_str(), {0, 0})) {
        vote(ctx, n, domain::VoteValue::Agree);
      }
      ImGui::SameLine();
      if (ui::PixelButton(((disagreed ? "[Disagree]" : "Disagree") + std::string("##d")).c_str(), {0, 0})) {
        vote(ctx, n, domain::VoteValue::Disagree);
      }
    }
    if (n.authorId != ctx.userId) {
      ImGui::SameLine();
      if (ui::PixelButton("Report##note", {0, 0})) {
        reportNoteId_ = reportNoteId_ == n.id ? std::string{} : n.id;
        reportAgent_ = false;
        reportReason_.fill('\0');
      }
    }
    if (reportNoteId_ == n.id) drawReportForm(ctx, services::ReportTarget::Note, n.id);
    ImGui::Spacing();
    ImGui::PopID();
  }
  if (!showAll && total > notes.size()) {
    if (ui::PixelButton((std::string("View all ") + std::to_string(total) + "##" + title).c_str(), {0, 0})) {
      showAll = true;
      dirty_ = true;
    }
  }
  ImGui::Spacing();
}

void CodexCardScreen::vote(app::FrameCtx& ctx, const services::NoteView& note, domain::VoteValue value) {
  // Choosing the vote you already have takes it back.
  const auto mine = myVotes_.find(note.id);
  const bool same = mine != myVotes_.end() && mine->second == value;
  auto r = ctx.services->notes.voteOnNote(ctx.userId, note.id, same ? std::nullopt : std::optional{value});
  if (!r) ui::pushGameMessage(r.error().message);
  dirty_ = true;
}

void CodexCardScreen::drawReportForm(app::FrameCtx& ctx, services::ReportTarget target, const std::string& targetId) {
  ui::ShadowText("Why? (3 to 200 characters, plain text)", ui::PaletteColor::TextOrange);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
  ImGui::InputText("##reportreason", reportReason_.data(), reportReason_.size());
  if (ui::PixelButton("Send report##send", {0, 0})) {
    auto r = ctx.services->keeper.reportContent(ctx.userId, target, targetId, reportReason_.data());
    if (r) {
      ui::pushGameMessage("Thank you. The Codex Keepers will take a look.");
      reportNoteId_.clear();
      reportAgent_ = false;
      reportReason_.fill('\0');
    } else {
      ui::pushGameMessage(r.error().message);
    }
  }
  ImGui::SameLine();
  if (ui::PixelButton("Cancel##report", {0, 0})) {
    reportNoteId_.clear();
    reportAgent_ = false;
  }
}

void CodexCardScreen::drawCompose(app::FrameCtx& ctx) {
  ImGui::Separator();
  if (!composing_) {
    if (ui::PixelButton("Write a Boon", {0, 0})) {
      composing_ = true;
      composeKind_ = services::NoteKind::Boon;
    }
    ImGui::SameLine();
    if (ui::PixelButton("Write a Bane", {0, 0})) {
      composing_ = true;
      composeKind_ = services::NoteKind::Bane;
    }
    return;
  }
  const bool boon = composeKind_ == services::NoteKind::Boon;
  ui::ShadowText(boon ? "Write a Boon (10 to 280 characters, plain text):" : "Write a Bane (10 to 280 characters, plain text):",
                 ui::PaletteColor::TextOrange);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
  ImGui::InputText("##notetext", composeText_.data(), composeText_.size());
  bool first = true;
  const std::string skillLabel = std::string("Skill: ") + (composeSkill_ < 0 ? "General" : domain::displayName(domain::SKILLS[static_cast<std::size_t>(composeSkill_)]));
  if (codex::flowButton(skillLabel, "noteskill", first)) {
    composeSkill_ = codex::cycleNext(composeSkill_, static_cast<int>(domain::SKILLS.size()));
  }
  ImGui::SameLine();
  ui::AlignFrameToButton();
  ImGui::SetNextItemWidth(110);
  ImGui::InputTextWithHint("##noteversion", "Version (optional)", composeVersion_.data(), composeVersion_.size());
  if (ui::PixelButton("Post##note", {0, 0})) {
    services::NewNote n;
    n.agentId = card_->agent.id;
    n.kind = composeKind_;
    n.text = composeText_.data();
    if (composeSkill_ >= 0) n.skill = domain::SKILLS[static_cast<std::size_t>(composeSkill_)];
    n.version = composeVersion_.data();
    auto r = ctx.services->notes.createNote(ctx.userId, n);
    if (r) {
      composeText_.fill('\0');
      composeVersion_.fill('\0');
      composing_ = false;
      ui::pushGameMessage(boon ? "Your Boon has been recorded." : "Your Bane has been recorded.");
      dirty_ = true;
    } else {
      ui::pushGameMessage(r.error().message);
    }
  }
  ImGui::SameLine();
  if (ui::PixelButton("Cancel##note", {0, 0})) composing_ = false;
  // Live length, green once it would be accepted (the service still validates the trimmed text).
  const auto len = domain::utf8Length(composeText_.data()).value_or(0);
  ImGui::SameLine();
  ui::AlignTextToButton();
  ui::ShadowText(
      std::to_string(len) + "/" + std::to_string(domain::MAX_NOTE),
      len >= domain::MIN_NOTE && len <= domain::MAX_NOTE ? ui::PaletteColor::TextGreen : ui::PaletteColor::TextOrange);
}

}  // namespace as::screens
