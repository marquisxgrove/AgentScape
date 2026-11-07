#pragma once

#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/screen_stack.hpp"
#include "game/wilds_sim.hpp"
#include "services/adventurer_service.hpp"
#include "services/codex_store.hpp"
#include "services/coding_service.hpp"

namespace as::screens {

// The Wilds (coding-sessions Requirements 1 and 3): start and stop a coding session, and watch the Adventurer fend
// off bug monsters with the familiar (or skill, when coding solo) while it runs. The rules live in CodingService and
// game::wilds_sim; this class only draws and routes.
class WildsScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Wilds; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void load(app::FrameCtx& ctx);
  void refresh(app::FrameCtx& ctx);
  void start(app::FrameCtx& ctx);
  void stop(app::FrameCtx& ctx);
  double elapsedSeconds() const;   // into the running session, capped at its 4-hour limit

  void drawScene(app::FrameCtx& ctx, ImVec2 origin, ImVec2 size);
  void drawCombat(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 origin, double elapsed);
  void drawSkilling(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 origin, double elapsed);
  void drawAdventurer(app::FrameCtx& ctx, ImDrawList* dl, ImVec2 at, int facing);
  void drawIdleColumn(app::FrameCtx& ctx);
  void drawRunningColumn(app::FrameCtx& ctx);
  void drawSummaryColumn(app::FrameCtx& ctx);

  bool loaded_ = false;
  bool redirected_ = false;
  std::optional<services::Adventurer> adventurer_;
  std::vector<services::Agent> agents_;
  int choice_ = -1;                 // -1: solo; otherwise an index into agents_
  std::string choiceOnEnter_;       // dev QA: --screen=wilds-gryphon preselects a familiar

  std::optional<services::CodingSession> running_;
  int sessionXp_ = 0;
  int todayXp_ = 0;
  double refreshIn_ = 0;            // seconds until the next status read
  double clock_ = 0;                // for idle animation

  // The last stopped session, shown until "Done".
  std::optional<services::CodingSummary> summary_;
  std::int64_t summarySquashed_ = 0;
  game::SkillTally summaryTally_;
};

}  // namespace as::screens
