#include "screens/codex/codex_card_screen.hpp"

#include <imgui.h>

#include <variant>

#include "app/services.hpp"
#include "core/civil_time.hpp"
#include "domain/codex_validation.hpp"
#include "platform/github_device_flow.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

void CodexCardScreen::onEnter(const app::ScreenParams& params) {
  if (const auto it = params.find("agent"); it != params.end()) {  // a fresh visit; coming back passes nothing
    slug_ = it->second;
    scope_.reset();
    if (const auto s = params.find("scope"); s != params.end()) scope_ = domain::questTypeFromStorage(s->second);
    confirmLeave_ = false;
    showAllBoons_ = showAllBanes_ = false;
  }
  dirty_ = true;
}

void CodexCardScreen::reload(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  card_.reset();
  allBoons_.clear();
  allBanes_.clear();
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
  drawNotes(ctx, "Boons (what players praise)", services::NoteKind::Boon);
  drawNotes(ctx, "Banes (what players warn about)", services::NoteKind::Bane);
  ImGui::EndChild();
}

void CodexCardScreen::drawHeader(app::FrameCtx& ctx) {
  const auto& a = card_->agent;
  const float w = ImGui::GetContentRegionAvail().x;
  ui::ShadowText(a.name, ui::PaletteColor::TextYellow);
  ui::ShadowText(std::string(services::kindDisplay(a.kind)) + " by " + a.vendor + "  -  updated " +
                     core::formatIso8601(a.updatedAt).substr(0, 10),
                 ui::PaletteColor::Parchment);
  {
    // The card itself: dark ink on parchment (the palette's coloured text doesn't read on it).
    ui::StonePanel parchment{"##examine", "", {0, 96}, ui::PanelVariant::Parchment};
    ImGui::PushStyleColor(ImGuiCol_Text, ui::toU32(ui::PaletteColor::Ink));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(a.examine.c_str());
    ImGui::Spacing();
    ImGui::TextUnformatted(a.about.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  (void)w;

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
  if (ui::PixelButton("Write a Quest Log", {0, 0})) {
    ui::pushGameMessage("The Quest Log scribes aren't open for business yet.");
  }
  (void)ctx;
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
    ImGui::Spacing();
    ImGui::PopID();
  }
  if (!showAll && total > notes.size()) {
    if (ui::PixelButton((std::string("View all ") + std::to_string(total) + "##" + title).c_str(), {0, 0})) {
      showAll = true;
      dirty_ = true;
    }
  }
  (void)ctx;
  ImGui::Spacing();
}

}  // namespace as::screens
