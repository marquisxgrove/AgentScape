#include "screens/wilds/wilds_screen.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>

#include "app/services.hpp"
#include "domain/coding_session.hpp"
#include "game/avatar_renderer.hpp"
#include "platform/paths.hpp"
#include "render/texture_cache.hpp"
#include "ui/chatbox.hpp"
#include "ui/theme.hpp"
#include "ui/ui_context.hpp"
#include "ui/widgets.hpp"

namespace as::screens {

namespace {
constexpr float SCENE_W = 300;
constexpr float SCENE_H = 200;
constexpr float GROUND_Y = 160;      // where feet stand, in scene pixels
constexpr float MONSTER = 32;        // bugs.png cell
constexpr float WISP = 16;           // wisp.png cell
constexpr double REFRESH_SECONDS = 5;

ImTextureID texId(const render::Texture& t) { return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(t.tex)); }

render::Texture sprite(const char* file) {
  return ui::ctx().mapTextures().load(platform::assetsDir() / "sprites" / file);
}

ImU32 withAlpha(ImU32 c, float a) {
  return (c & 0x00FFFFFFU) | (static_cast<ImU32>(std::clamp(a, 0.0F, 1.0F) * 255.0F) << 24);
}

// A hit number in a small coloured chip: red for the Adventurer, blue for the familiar.
void hitChip(ImDrawList* dl, ImVec2 centre, int damage, bool familiar, float alpha) {
  const std::string text = std::to_string(damage);
  const ImVec2 size = ImGui::CalcTextSize(text.c_str());
  const ImVec2 a{std::floor(centre.x - size.x / 2 - 3), std::floor(centre.y - size.y / 2 - 1)};
  const ImVec2 b{a.x + size.x + 6, a.y + size.y + 2};
  dl->AddRectFilled(a, b, withAlpha(familiar ? IM_COL32(40, 70, 160, 255) : IM_COL32(150, 30, 25, 255), alpha));
  dl->AddRect(a, b, withAlpha(IM_COL32(20, 15, 10, 255), alpha));
  dl->AddText({a.x + 3, a.y + 1}, withAlpha(IM_COL32(255, 255, 255, 255), alpha), text.c_str());
}

void shadowTextAt(ImDrawList* dl, ImVec2 at, const std::string& text, ImU32 color) {
  dl->AddText({at.x + 1, at.y + 1}, IM_COL32(0, 0, 0, 255), text.c_str());
  dl->AddText(at, color, text.c_str());
}

void centredText(ImDrawList* dl, float centreX, float y, const std::string& text, ImU32 color) {
  const float w = ImGui::CalcTextSize(text.c_str()).x;
  shadowTextAt(dl, {std::floor(centreX - w / 2), y}, text, color);
}

std::string talliesText(const game::SkillTally& t) {
  return "Logs " + std::to_string(t.logs) + "  Ore " + std::to_string(t.ore) + "  Fish " + std::to_string(t.fish);
}
}  // namespace

void WildsScreen::onEnter(const app::ScreenParams& params) {
  loaded_ = false;
  refreshIn_ = 0;
  if (const auto it = params.find("with"); it != params.end()) choiceOnEnter_ = it->second;
}

void WildsScreen::load(app::FrameCtx& ctx) {
  loaded_ = true;
  if (ctx.services == nullptr) return;
  if (auto agents = ctx.services->codex.approvedAgents()) agents_ = std::move(*agents);
  adventurer_.reset();
  if (auto active = ctx.services->adventurers.getActive(ctx.userId); active && *active) adventurer_ = **active;
  if (!adventurer_ && !redirected_ && ctx.nav != nullptr) {   // Requirement 1.6, as for questing
    redirected_ = true;
    ui::pushGameMessage("You need an Adventurer to head into the Wilds.");
    ctx.nav->navigate(app::ScreenId::Costumer);
    return;
  }
  for (std::size_t i = 0; i < agents_.size(); ++i) {
    if (agents_[i].slug == choiceOnEnter_) choice_ = static_cast<int>(i);
  }
  refresh(ctx);
}

void WildsScreen::refresh(app::FrameCtx& ctx) {
  refreshIn_ = REFRESH_SECONDS;
  if (ctx.services == nullptr) return;
  auto& coding = ctx.services->coding;
  running_.reset();
  sessionXp_ = 0;
  if (auto r = coding.running(ctx.userId); r && *r) {
    running_ = **r;
    if (auto xp = coding.sessionXp(running_->id)) sessionXp_ = *xp;
  }
  if (auto today = coding.todayXp(ctx.userId)) todayXp_ = *today;
}

double WildsScreen::elapsedSeconds() const {
  if (!running_) return 0;
  const std::chrono::duration<double> d = std::chrono::system_clock::now() - running_->startedAt;
  return std::clamp(d.count(), 0.0, static_cast<double>(domain::CODING_SESSION_MAX.count()));
}

void WildsScreen::start(app::FrameCtx& ctx) {
  const std::string slug = choice_ >= 0 ? agents_[static_cast<std::size_t>(choice_)].slug : std::string{};
  auto r = ctx.services->coding.start(ctx.userId, slug);
  if (!r) {
    ui::pushGameMessage(r.error().message);
    return;
  }
  summary_.reset();
  ui::pushGameMessage(r->solo() ? "You head into the Wilds to work alone." : "You and " + r->agentName + " head into the Wilds.");
  refresh(ctx);
}

void WildsScreen::stop(app::FrameCtx& ctx) {
  auto r = ctx.services->coding.stop(ctx.userId);
  if (!r) {
    ui::pushGameMessage(r.error().message);
    refresh(ctx);
    return;
  }
  const double secs = static_cast<double>(r->duration.count());
  const auto seed = game::wildsSeed(r->session.id);
  summarySquashed_ = r->session.solo() ? 0 : game::combatAt(seed, secs, true).squashedTotal;
  summaryTally_ = r->session.solo() ? game::skillingAt(seed, secs).tally : game::SkillTally{};
  summary_ = *r;
  ui::pushGameMessage("Session over: " + domain::formatSessionDuration(r->duration) + " of coding, +" +
                      std::to_string(r->xp) + " XP.");
  refresh(ctx);
}

void WildsScreen::draw(app::FrameCtx& ctx) {
  ui::StonePanel panel{"##wilds", "The Wilds"};
  if (!loaded_) load(ctx);
  clock_ += ctx.dt;
  refreshIn_ -= ctx.dt;
  if (refreshIn_ <= 0) refresh(ctx);

  if (ui::PixelButton("Back", {70, 0}) && ctx.nav != nullptr) ctx.nav->back();
  ImGui::SameLine();
  ui::AlignTextToButton();
  ui::ShadowText("Code for real; your Adventurer earns XP out here.", ui::PaletteColor::Parchment);

  const ImVec2 sceneAt = ImGui::GetCursorScreenPos();
  drawScene(ctx, sceneAt, {SCENE_W, SCENE_H});
  ImGui::Dummy({SCENE_W, SCENE_H});
  ImGui::SameLine();
  ImGui::BeginGroup();
  if (summary_) {
    drawSummaryColumn(ctx);
  } else if (running_) {
    drawRunningColumn(ctx);
  } else {
    drawIdleColumn(ctx);
  }
  ImGui::EndGroup();
}

void WildsScreen::drawIdleColumn(app::FrameCtx& ctx) {
  ui::ShadowText("Who are you coding with?", ui::PaletteColor::TextOrange);
  ImGui::BeginChild("##codingwith", {0, 104}, ImGuiChildFlags_Borders);
  if (ImGui::Selectable("Solo (skilling)", choice_ < 0)) choice_ = -1;
  for (std::size_t i = 0; i < agents_.size(); ++i) {
    const auto& a = agents_[i];
    if (ImGui::Selectable((a.name + "##with-" + a.slug).c_str(), choice_ == static_cast<int>(i))) choice_ = static_cast<int>(i);
  }
  ImGui::EndChild();
  if (ui::PixelButton("Start session", {0, 0}, ui::hoverText("Start", "a coding session", ui::PaletteColor::TextCyan, 0))) {
    start(ctx);
  }
  ui::ShadowText("1 XP a minute, even with", ui::PaletteColor::Parchment);
  ui::ShadowText("the game closed. 8 h a day.", ui::PaletteColor::Parchment);
  ui::ShadowText("Today: " + std::to_string(todayXp_) + " / " +
                     std::to_string(domain::codingXpFor(domain::CODING_DAILY_CAP)) + " XP",
                 ui::PaletteColor::TextYellow);
}

void WildsScreen::drawRunningColumn(app::FrameCtx& ctx) {
  const double elapsed = elapsedSeconds();
  if (running_->solo()) {
    ui::ShadowText("Coding solo", ui::PaletteColor::TextOrange);
  } else {
    ui::ShadowText("Coding with", ui::PaletteColor::TextOrange);
    ui::ShadowText(running_->agentName, ui::PaletteColor::TextCyan);
  }
  ui::ShadowText(domain::formatSessionDuration(std::chrono::seconds{static_cast<std::int64_t>(elapsed)}) + "  (4 h max)",
                 ui::PaletteColor::TextYellow);
  ui::ShadowText("+" + std::to_string(sessionXp_) + " XP this session", ui::PaletteColor::TextGreen);
  ui::ShadowText("Today: " + std::to_string(todayXp_) + " / " +
                     std::to_string(domain::codingXpFor(domain::CODING_DAILY_CAP)) + " XP",
                 ui::PaletteColor::Parchment);
  ImGui::Spacing();
  const auto seed = game::wildsSeed(running_->id);
  if (running_->solo()) {
    ui::ShadowText(talliesText(game::skillingAt(seed, elapsed).tally), ui::PaletteColor::Parchment);
  } else {
    ui::ShadowText("Bugs squashed: " + std::to_string(game::combatAt(seed, elapsed, true).squashedTotal),
                   ui::PaletteColor::Parchment);
  }
  ImGui::Spacing();
  if (ui::PixelButton("Stop session", {0, 0}, ui::hoverText("Stop", "the coding session", ui::PaletteColor::TextCyan, 0))) {
    stop(ctx);
  }
}

void WildsScreen::drawSummaryColumn(app::FrameCtx& ctx) {
  const auto& s = *summary_;
  ui::ShadowText("Session over", ui::PaletteColor::TextOrange);
  if (!s.session.solo()) ui::ShadowText("with " + s.session.agentName, ui::PaletteColor::TextCyan);
  ui::ShadowText(domain::formatSessionDuration(s.duration) + " of coding", ui::PaletteColor::TextYellow);
  ui::ShadowText("+" + std::to_string(s.xp) + " XP", ui::PaletteColor::TextGreen);
  ui::ShadowText(s.session.solo() ? talliesText(summaryTally_) : "Bugs squashed: " + std::to_string(summarySquashed_),
                 ui::PaletteColor::Parchment);
  ImGui::Spacing();
  if (!s.session.solo() && !s.session.agentSlug.empty()) {
    // The session is fresh in mind: a good moment to rate the familiar.
    if (ui::PixelButton("Write a Quest Log", {0, 0}) && ctx.nav != nullptr) {
      ctx.nav->navigate(app::ScreenId::QuestScroll, {{"agent", s.session.agentSlug}});
    }
  }
  if (ui::PixelButton("Done", {70, 0})) summary_.reset();
}

void WildsScreen::drawScene(app::FrameCtx& ctx, ImVec2 origin, ImVec2 size) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
  if (const auto bg = sprite("wilds.png")) {
    dl->AddImage(texId(bg), origin, {origin.x + size.x, origin.y + size.y});
  } else {
    dl->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, ui::toU32(ui::PaletteColor::Stone));
  }
  if (running_) {
    if (running_->solo()) {
      drawSkilling(ctx, dl, origin, elapsedSeconds());
    } else {
      drawCombat(ctx, dl, origin, elapsedSeconds());
    }
  } else {
    drawAdventurer(ctx, dl, {origin.x + 134, origin.y + GROUND_Y - 48}, static_cast<int>(domain::Facing::S));
    centredText(dl, origin.x + size.x / 2, origin.y + 12, "Start a session to head out.", ui::toU32(ui::PaletteColor::TextYellow));
  }
  dl->AddRect(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(30, 25, 18, 255));
  dl->PopClipRect();
}

void WildsScreen::drawAdventurer(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 at, int facing) {
  if (!adventurer_ || ctx.avatars == nullptr) return;
  const int idle = ctx.reduceMotion ? 0 : static_cast<int>(clock_ * 2.0) % 2;
  if (SDL_Texture* tex = ctx.avatars->texture(adventurer_->appearance, adventurer_->loadout,
                                              static_cast<domain::Facing>(facing), idle)) {
    const auto id = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex));
    dl->AddImage(id, at, {at.x + static_cast<float>(ctx.avatars->cellWidth()), at.y + static_cast<float>(ctx.avatars->cellHeight())});
  }
}

void WildsScreen::drawCombat(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 origin, double elapsed) {
  const auto f = game::combatAt(game::wildsSeed(running_->id), elapsed, true);
  const bool still = ctx.reduceMotion;
  const auto& bug = game::bestiary()[static_cast<std::size_t>(f.monster)];
  const float hitAge = f.lastHit ? static_cast<float>(f.lastHit->age) : 99.0F;

  // The Adventurer lunges on its own hits; the familiar's wisp bobs behind it and sparks on its hits.
  const bool lunge = !still && f.lastHit && !f.lastHit->byFamiliar && hitAge < 0.25F && !f.squashed;
  drawAdventurer(ctx, dl, {origin.x + 96 + (lunge ? 5.0F : 0.0F), origin.y + GROUND_Y - 48}, static_cast<int>(domain::Facing::E));
  const float bob = still ? 0.0F : std::sin(static_cast<float>(clock_) * 3.0F) * 2.0F;
  const ImVec2 wispAt{origin.x + 68, std::floor(origin.y + GROUND_Y - 60 + bob)};
  if (const auto wisp = sprite("wisp.png")) {
    const float u0 = (!still && static_cast<int>(clock_ * 4.0) % 2 == 1) ? 0.5F : 0.0F;
    dl->AddImage(texId(wisp), wispAt, {wispAt.x + WISP, wispAt.y + WISP}, {u0, 0}, {u0 + 0.5F, 1});
  }
  centredText(dl, wispAt.x + WISP / 2, wispAt.y - 14, running_->agentName, ui::toU32(ui::PaletteColor::TextCyan));

  // The bug: its health bar and name above it; it fades once squashed, until the next one arrives.
  const ImVec2 bugAt{origin.x + 190, origin.y + GROUND_Y - MONSTER};
  const float fade = f.squashed ? (still ? 0.0F : std::max(0.0F, 1.0F - hitAge / 0.6F)) : 1.0F;
  if (const auto bugs = sprite("bugs.png"); bugs && fade > 0.0F) {
    const float rows = static_cast<float>(game::bestiary().size());
    const float v0 = static_cast<float>(f.monster) / rows;
    const float u0 = (!still && static_cast<int>(clock_ * 3.0) % 2 == 1) ? 0.5F : 0.0F;
    const bool flash = !still && f.lastHit && hitAge < 0.12F;
    dl->AddImage(texId(bugs), bugAt, {bugAt.x + MONSTER, bugAt.y + MONSTER}, {u0, v0}, {u0 + 0.5F, v0 + 1.0F / rows},
                 withAlpha(flash ? IM_COL32(255, 200, 200, 255) : IM_COL32(255, 255, 255, 255), fade));
  }
  if (!f.squashed) {
    const ImVec2 barA{bugAt.x, bugAt.y - 8};
    const float frac = static_cast<float>(f.hpLeft) / static_cast<float>(f.hpMax);
    dl->AddRectFilled(barA, {barA.x + MONSTER, barA.y + 4}, IM_COL32(150, 30, 25, 255));
    dl->AddRectFilled(barA, {barA.x + std::round(MONSTER * frac), barA.y + 4}, IM_COL32(60, 170, 60, 255));
    dl->AddRect({barA.x - 1, barA.y - 1}, {barA.x + MONSTER + 1, barA.y + 5}, IM_COL32(20, 15, 10, 255));
  }
  centredText(dl, bugAt.x + MONSTER / 2, bugAt.y - 24, std::string(bug.name), ui::toU32(ui::PaletteColor::TextYellow));
  if (f.squashed) centredText(dl, bugAt.x + MONSTER / 2, bugAt.y + 10, "Squashed!", ui::toU32(ui::PaletteColor::TextGreen));

  // The latest hit number floats up from the bug (still, with Reduce motion).
  if (f.lastHit && hitAge < 0.9F) {
    const float rise = still ? 0.0F : hitAge * 18.0F;
    hitChip(dl, {bugAt.x + MONSTER / 2, bugAt.y + 12 - rise}, f.lastHit->damage, f.lastHit->byFamiliar,
            still ? 1.0F : 1.0F - std::max(0.0F, hitAge - 0.6F) / 0.3F);
    if (f.lastHit->byFamiliar && hitAge < 0.3F) {
      dl->AddLine({wispAt.x + WISP, wispAt.y + WISP / 2}, {bugAt.x + 4, bugAt.y + MONSTER / 2}, IM_COL32(120, 200, 255, 220), 1.0F);
    }
  }
}

void WildsScreen::drawSkilling(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 origin, double elapsed) {
  const auto f = game::skillingAt(game::wildsSeed(running_->id), elapsed);
  const bool still = ctx.reduceMotion;
  const bool swing = !still && f.progress > 0.8;
  drawAdventurer(ctx, dl, {origin.x + 110 + (swing ? 4.0F : 0.0F), origin.y + GROUND_Y - 48}, static_cast<int>(domain::Facing::E));

  // The spot being worked, standing on the same ground line.
  const char* file = "tree.png";
  if (f.spot == game::SkillSpot::Rock) file = "rock.png";
  if (f.spot == game::SkillSpot::Pond) file = "pond.png";
  ImVec2 spotCentre{origin.x + 186, origin.y + GROUND_Y - 30};
  if (const auto tex = sprite(file)) {
    const bool twoFrames = f.spot == game::SkillSpot::Pond;   // the pond ripples
    const float w = static_cast<float>(tex.w) / (twoFrames ? 2.0F : 1.0F);
    const float h = static_cast<float>(tex.h);
    const ImVec2 at{origin.x + 166, origin.y + GROUND_Y - h + (twoFrames ? 6.0F : 0.0F)};
    const float u0 = twoFrames && !still && static_cast<int>(clock_ * 2.0) % 2 == 1 ? 0.5F : 0.0F;
    dl->AddImage(texId(tex), at, {at.x + w, at.y + h}, {u0, 0}, {u0 + (twoFrames ? 0.5F : 1.0F), 1});
    spotCentre = {at.x + w / 2, at.y};
  }
  centredText(dl, spotCentre.x, spotCentre.y - 16, std::string(game::skillName(f.spot)), ui::toU32(ui::PaletteColor::TextYellow));

  // The action's progress, and "+1 Logs" floating up as each one lands.
  const ImVec2 barA{origin.x + 104, origin.y + GROUND_Y + 6};
  dl->AddRectFilled(barA, {barA.x + 44, barA.y + 3}, IM_COL32(40, 32, 24, 255));
  dl->AddRectFilled(barA, {barA.x + std::round(44.0F * static_cast<float>(f.progress)), barA.y + 3}, IM_COL32(220, 190, 60, 255));
  if (f.sinceGather < 1.0) {
    const float age = static_cast<float>(f.sinceGather);
    const float rise = still ? 0.0F : age * 16.0F;
    centredText(dl, spotCentre.x, spotCentre.y - 4 - rise, "+1 " + std::string(game::skillResource(f.spot)),
                withAlpha(ui::toU32(ui::PaletteColor::TextGreen), still ? 1.0F : 1.0F - age));
  }
}

}  // namespace as::screens
