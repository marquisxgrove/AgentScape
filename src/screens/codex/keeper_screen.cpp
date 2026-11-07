#include "screens/codex/keeper_screen.hpp"

#include <imgui.h>

#include "app/services.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
// Shows a command's refusal as a game message; returns true on success.
bool done(const Result<void>& r) {
  if (!r) ui::pushGameMessage(r.error().message);
  return r.has_value();
}

void plain(const std::string& text, ui::PaletteColor color = ui::PaletteColor::TextYellow) {
  ui::RichText(ui::escapeMarkup(text), color, ImGui::GetContentRegionAvail().x);
}
}  // namespace

void KeeperScreen::onEnter(const app::ScreenParams&) { dirty_ = true; }

void KeeperScreen::reload(app::FrameCtx& ctx) {
  dirty_ = false;
  error_.clear();
  queue_.reset();
  if (ctx.services == nullptr) return;
  auto q = ctx.services->keeper.queue(ctx.userId);
  if (!q) {
    error_ = q.error().message;
    return;
  }
  queue_ = std::move(*q);
}

void KeeperScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##keeper", "Codex Keeper"};
  if (dirty_) reload(ctx);
  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  if (!queue_) {
    ui::ShadowText(error_.empty() ? "Nothing to review." : error_, ui::PaletteColor::TextOrange);
    return;
  }
  ImGui::SameLine();
  ui::ShadowText("Local moderation: decisions affect only this device.", ui::PaletteColor::Parchment);

  ImGui::BeginChild("##keeperbody", {0, 0});
  const auto& q = *queue_;

  ui::ShadowText("Proposals (" + std::to_string(q.proposals.size()) + ")", ui::PaletteColor::TextOrange);
  if (q.proposals.empty()) ui::ShadowText("None waiting.", ui::PaletteColor::Parchment);
  for (const auto& p : q.proposals) {
    ImGui::PushID(p.agent.id.c_str());
    plain(p.agent.name + " - " + services::kindDisplay(p.agent.kind) + " by " + p.agent.vendor +
          "  (proposed by " + p.proposerName + ")");
    plain(p.agent.examine, ui::PaletteColor::Parchment);
    if (!p.agent.about.empty()) plain(p.agent.about, ui::PaletteColor::Parchment);
    if (p.agent.website) plain("Website: " + *p.agent.website, ui::PaletteColor::Parchment);   // shown, never opened from here
    if (ui::PixelButton("Approve##a", {0, 0}) && done(ctx.services->keeper.moderateAgent(ctx.userId, p.agent.id, true))) {
      ui::pushGameMessage(p.agent.name + " has joined the Grand Codex.");
      dirty_ = true;
    }
    ImGui::SameLine();
    if (ui::PixelButton("Reject##r", {0, 0}) && done(ctx.services->keeper.moderateAgent(ctx.userId, p.agent.id, false))) {
      dirty_ = true;
    }
    ImGui::Spacing();
    ImGui::PopID();
  }

  ImGui::Separator();
  ui::ShadowText("Reports (" + std::to_string(q.reports.size()) + ")", ui::PaletteColor::TextOrange);
  if (q.reports.empty()) ui::ShadowText("None waiting.", ui::PaletteColor::Parchment);
  for (const auto& r : q.reports) {
    ImGui::PushID(r.id.c_str());
    const bool note = r.target == services::ReportTarget::Note;
    plain(std::string(note ? "Note on " : "Familiar ") + r.agentName + ": " + r.targetText);
    plain("Reported by " + r.reporterName + ": " + r.reason, ui::PaletteColor::Parchment);
    if (note) {
      const char* label = r.targetHidden ? "Unhide##h" : "Hide##h";
      if (ui::PixelButton(label, {0, 0}) &&
          done(ctx.services->keeper.setNoteHidden(ctx.userId, r.targetId, !r.targetHidden))) {
        dirty_ = true;
      }
      ImGui::SameLine();
    }
    if (ui::PixelButton("Resolve##s", {0, 0}) && done(ctx.services->keeper.resolveReport(ctx.userId, r.id))) {
      dirty_ = true;
    }
    ImGui::Spacing();
    ImGui::PopID();
  }

  ImGui::Separator();
  ui::ShadowText("Hidden Notes (" + std::to_string(q.hiddenNotes.size()) + ")", ui::PaletteColor::TextOrange);
  if (q.hiddenNotes.empty()) ui::ShadowText("None.", ui::PaletteColor::Parchment);
  for (const auto& n : q.hiddenNotes) {
    ImGui::PushID(n.id.c_str());
    plain(n.agentName + " - " + n.authorName + ": " + n.text, ui::PaletteColor::Parchment);
    if (ui::PixelButton("Unhide##u", {0, 0}) && done(ctx.services->keeper.setNoteHidden(ctx.userId, n.id, false))) {
      dirty_ = true;
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

}  // namespace as::screens
