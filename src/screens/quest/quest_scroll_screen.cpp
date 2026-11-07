#include "screens/quest/quest_scroll_screen.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "app/services.hpp"
#include "domain/codex_validation.hpp"
#include "screens/codex/codex_widgets.hpp"
#include "screens/quest/quest_widgets.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
using ui::QuestStep;

void copyTo(char* dst, std::size_t size, const std::string& src) { std::snprintf(dst, size, "%s", src.c_str()); }
}  // namespace

void QuestScrollScreen::onEnter(const app::ScreenParams& params) {
  if (const auto it = params.find("agent"); it != params.end()) {   // a fresh visit (coming back passes nothing)
    agentOnEnter_ = it->second;
    scopeOnEnter_ = params.count("scope") != 0 ? params.at("scope") : std::string{};
    editOnEnter_ = params.count("edit") != 0 ? params.at("edit") : std::string{};
    devStep_.reset();
    devDemo_ = params.count("demo") != 0;           // dev QA: --screen=quest-scroll-demo prefills a valid log
    devSubmit_ = params.count("submit") != 0;       //         and saves it on the first frame
    if (const auto s = params.find("step"); s != params.end()) devStep_ = std::stoi(s->second);
    redirected_ = false;
    loaded_ = false;
    fieldErrors_.clear();
    confirmDelete_ = false;
  }
}

void QuestScrollScreen::syncBuffers() {
  copyTo(version_.data(), version_.size(), form_.input().version);
  copyTo(title_.data(), title_.size(), form_.input().title);
  copyTo(body_.data(), body_.size(), form_.input().body);
}

void QuestScrollScreen::openExisting(app::FrameCtx& ctx, const std::string& questLogId) {
  auto record = ctx.services->quests.get(questLogId);
  if (!record || !*record || (*record)->authorId != ctx.userId) return;
  form_.edit(questLogId, (*record)->input);
  syncBuffers();
  fieldErrors_.clear();
}

void QuestScrollScreen::load(app::FrameCtx& ctx) {
  loaded_ = true;
  if (ctx.services == nullptr) return;
  if (auto agents = ctx.services->codex.approvedAgents()) agents_ = std::move(*agents);

  // An Adventurer is needed to go questing (Requirement 1.5).
  auto active = ctx.services->adventurers.getActive(ctx.userId);
  needAdventurer_ = active && !*active;
  if (needAdventurer_ && !redirected_ && ctx.nav != nullptr) {
    redirected_ = true;
    ui::pushGameMessage("You need an Adventurer to go questing.");
    ctx.nav->navigate(app::ScreenId::Costumer);
    return;
  }

  const auto agent = std::find_if(agents_.begin(), agents_.end(), [&](const services::Agent& a) { return a.slug == agentOnEnter_; });
  form_.begin(agent != agents_.end() ? agent->id : std::string{});
  if (const auto qt = domain::questTypeFromStorage(scopeOnEnter_)) form_.setQuestType(*qt);
  syncBuffers();
  if (!editOnEnter_.empty()) openExisting(ctx, editOnEnter_);

  if (devDemo_) {   // a valid log, as if typed
    auto& in = form_.mutableInput();
    in.version = "dev";
    in.overall = 4;
    in.skills = {{domain::Skill::Coding, 5}, {domain::Skill::Speed, 3}, {domain::Skill::Value, 4}};
    in.title = "A solid helper";
    in.body = "It wrote working code on the first try.\nThe explanations were clear too.";
    in.recommend = true;
    in.pros = {"Accurate", "Fast"};
    in.cons = {"Expensive"};
    syncBuffers();
  }
  if (devStep_) form_.goTo(static_cast<QuestStep>(std::clamp(*devStep_, 1, 3)));
}

void QuestScrollScreen::drawErrors(QuestStep step) {
  for (const auto& e : form_.errorsFor(step)) {
    // Only nag about a field once the player has tried to move on or save; an untouched empty form isn't an error list.
    if (fieldErrors_.count(e.field) != 0) ui::ShadowText(e.message, ui::PaletteColor::TextOrange);
  }
}

void QuestScrollScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##questscroll", form_.editing() ? "Update your Quest Log" : "Write a Quest Log"};
  if (!loaded_) load(ctx);
  if (ui::PixelButton("Cancel", {80, 0}) && ctx.nav != nullptr) ctx.nav->back();
  if (needAdventurer_) {
    ui::ShadowText("You need an Adventurer to go questing.", ui::PaletteColor::TextOrange);
    if (ui::PixelButton("To the Costumer", {0, 0}) && ctx.nav != nullptr) ctx.nav->navigate(app::ScreenId::Costumer);
    return;
  }
  if (devSubmit_) {
    devSubmit_ = false;
    submit(ctx);
    return;
  }
  ImGui::SameLine();
  ui::ShadowText("Step " + std::to_string(static_cast<int>(form_.step())) + " of 3", ui::PaletteColor::Parchment);

  ImGui::BeginChild("##scrollbody", {0, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - 6.0F});
  switch (form_.step()) {
    case QuestStep::Where: drawWhere(ctx); break;
    case QuestStep::Rating: drawRating(); break;
    case QuestStep::Story: drawStory(ctx); break;
  }
  ImGui::EndChild();

  // Navigation bar.
  const bool canBack = form_.step() != QuestStep::Where && !(form_.editing() && form_.step() == QuestStep::Rating);
  ImGui::BeginDisabled(!canBack);
  if (ui::PixelButton("Back##step", {70, 0})) {
    form_.back();
    fieldErrors_.clear();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (form_.step() != QuestStep::Story) {
    if (ui::PixelButton("Next##step", {70, 0})) {
      if (!form_.next()) {
        for (const auto& e : form_.errorsFor(form_.step())) fieldErrors_[e.field] = e.message;
      } else {
        fieldErrors_.clear();
        // A second log for the same Agent and version opens the first for editing instead (Requirement 1.3).
        if (form_.step() == QuestStep::Rating && !form_.editing() && ctx.services != nullptr) {
          std::string version = form_.input().version;
          if (auto own = ctx.services->quests.findOwn(ctx.userId, form_.input().agentId, version); own && *own) {
            ui::pushGameMessage("You've already logged this quest. Update it instead?");
            openExisting(ctx, **own);
          }
        }
      }
    }
  } else {
    if (ui::PixelButton(form_.editing() ? "Update##save" : "Submit##save", {96, 0})) submit(ctx);
  }
  if (form_.editing()) {
    ImGui::SameLine();
    if (!confirmDelete_) {
      if (ui::PixelButton("Delete##log", {70, 0})) confirmDelete_ = true;
    } else {
      ui::ShadowText("Delete it?", ui::PaletteColor::TextOrange);
      ImGui::SameLine();
      if (ui::PixelButton("Yes##del", {50, 0}) && ctx.services != nullptr) {
        auto r = ctx.services->quests.deleteQuestLog(ctx.userId, form_.editingId());
        if (r) {
          ui::pushGameMessage("Your Quest Log has been deleted.");
          if (ctx.nav != nullptr) ctx.nav->back();
        } else {
          ui::pushGameMessage(r.error().message);
        }
        confirmDelete_ = false;
      }
      ImGui::SameLine();
      if (ui::PixelButton("No##del", {50, 0})) confirmDelete_ = false;
    }
  }
}

void QuestScrollScreen::drawWhere(app::FrameCtx&) {
  ui::ShadowText("Which familiar did you go questing with?", ui::PaletteColor::TextOrange);
  ImGui::BeginChild("##agentlist", {0, 118}, ImGuiChildFlags_Borders);
  for (const auto& a : agents_) {
    const bool chosen = a.id == form_.input().agentId;
    if (ImGui::Selectable((a.name + "  (" + a.vendor + ")##" + a.slug).c_str(), chosen)) form_.setAgent(a.id);
  }
  ImGui::EndChild();
  drawErrors(QuestStep::Where);

  bool first = true;
  if (codex::flowButton(std::string("Quest Type: ") + domain::displayName(form_.input().questType), "qtype", first)) {
    form_.cycleQuestType();
  }
  ui::ShadowText("Version (optional)", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(200);
  if (ImGui::InputText("##version", version_.data(), version_.size())) form_.mutableInput().version = version_.data();
  if (fieldErrors_.count("agent") != 0 && form_.input().agentId.empty()) {
    ui::ShadowText(fieldErrors_["agent"], ui::PaletteColor::TextOrange);
  }
}

void QuestScrollScreen::drawRating() {
  ui::ShadowText("How did it go overall?", ui::PaletteColor::TextOrange);
  int overall = form_.input().overall;
  if (quest::gemRating("overall", &overall, 1)) form_.setOverall(overall);
  if (fieldErrors_.count("overall") != 0) ui::ShadowText(fieldErrors_["overall"], ui::PaletteColor::TextOrange);

  ImGui::Spacing();
  ui::ShadowText("Rate any skills you tested (optional):", ui::PaletteColor::TextOrange);
  ImGui::BeginChild("##skillrows", {0, 0});
  for (std::size_t i = 0; i < domain::SKILLS.size(); ++i) {
    const auto skill = domain::SKILLS[i];
    ImGui::PushID(static_cast<int>(i));
    codex::skillIcon(static_cast<int>(i));
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2);
    ui::ShadowText(domain::displayName(skill), ui::PaletteColor::TextCyan);
    ImGui::SameLine(110);
    int stars = form_.skillStars(skill);
    if (quest::gemRating("skill", &stars, 0)) form_.setSkill(skill, stars);
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void QuestScrollScreen::drawStory(app::FrameCtx&) {
  ui::ShadowText("Title (5 to 80 characters)", ui::PaletteColor::Parchment);
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
  if (ImGui::InputText("##title", title_.data(), title_.size())) form_.mutableInput().title = title_.data();
  if (fieldErrors_.count("title") != 0) ui::ShadowText(fieldErrors_["title"], ui::PaletteColor::TextOrange);

  const auto bodyLen = domain::utf8Length(body_.data()).value_or(0);
  ui::ShadowText("Your story (20 to 2,000 characters): " + std::to_string(bodyLen), ui::PaletteColor::Parchment);
  if (ImGui::InputTextMultiline("##body", body_.data(), body_.size(), {ImGui::GetContentRegionAvail().x - 8, 62})) {
    form_.mutableInput().body = body_.data();
  }
  if (fieldErrors_.count("body") != 0) ui::ShadowText(fieldErrors_["body"], ui::PaletteColor::TextOrange);

  bool recommend = form_.input().recommend;
  if (ui::Toggle("I would recommend it", &recommend)) form_.setRecommend(recommend);

  const auto tagColumn = [&](bool pro) {
    const auto& tags = pro ? form_.input().pros : form_.input().cons;
    ui::ShadowText(std::string(pro ? "Good points (" : "Bad points (") + std::to_string(tags.size()) + "/3)",
                   pro ? ui::PaletteColor::TextGreen : ui::PaletteColor::TextOrange);
    const auto draw = [&](std::string_view name) {
      bool on = form_.hasTag(pro, std::string(name));
      if (ui::Toggle((std::string(name) + (pro ? "##pro" : "##con")).c_str(), &on)) {
        if (!form_.toggleTag(pro, std::string(name))) ui::pushGameMessage("You can pick up to 3.");
      }
    };
    if (pro) {
      for (const auto name : domain::PRO_TAGS) draw(name);
    } else {
      for (const auto name : domain::CON_TAGS) draw(name);
    }
  };
  if (ImGui::BeginTable("##tags", 2, ImGuiTableFlags_SizingStretchSame)) {
    ImGui::TableNextColumn();
    tagColumn(true);
    ImGui::TableNextColumn();
    tagColumn(false);
    ImGui::EndTable();
  }
}

void QuestScrollScreen::submit(app::FrameCtx& ctx) {
  if (ctx.services == nullptr) return;
  fieldErrors_.clear();
  if (!form_.ready()) {   // same rules the service re-checks; show every problem and jump to the first
    const auto result = domain::validateQuestLogInput(form_.input());
    if (!result) {
      for (const auto& e : result.error()) fieldErrors_[e.field] = e.message;
    }
    if (const auto bad = form_.firstBadStep()) form_.goTo(*bad);
    return;
  }
  if (form_.editing()) {
    auto r = ctx.services->quests.updateQuestLog(ctx.userId, form_.editingId(), form_.input());
    if (r) {
      ui::pushGameMessage("Your Quest Log has been updated.");
      if (ctx.nav != nullptr) ctx.nav->back();
      return;
    }
    for (const auto& f : r.error().fields) fieldErrors_[f.field] = f.message;
    if (r.error().fields.empty()) ui::pushGameMessage(r.error().error.message);
    return;
  }
  auto r = ctx.services->quests.createQuestLog(ctx.userId, form_.input());
  if (r) {
    ui::pushGameMessage("Your quest has been logged.");   // the XP drop, level-up and unlocks come from the XP events
    if (ctx.nav != nullptr) ctx.nav->back();
    return;
  }
  const auto& e = r.error();
  if (e.existingId) {
    ui::pushGameMessage(e.error.message);
    openExisting(ctx, *e.existingId);
  } else if (!e.fields.empty()) {
    for (const auto& f : e.fields) fieldErrors_[f.field] = f.message;
    if (const auto bad = form_.firstBadStep()) form_.goTo(*bad);
  } else {
    ui::pushGameMessage(e.error.message);
    if (e.error.message == "You need an Adventurer to go questing." && ctx.nav != nullptr) ctx.nav->navigate(app::ScreenId::Costumer);
  }
}

}  // namespace as::screens
