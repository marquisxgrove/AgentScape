#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "domain/voting.hpp"
#include "services/codex_service.hpp"

namespace as::screens {

// An Agent's examine card (agent-codex Requirement 2): who it is, its levels in a chosen Quest Type, and the
// community's top Boons and Banes. Everything players wrote is drawn as plain text, never as markup.
class CodexCardScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::CodexCard; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void reload(app::FrameCtx& ctx);
  void drawHeader(app::FrameCtx& ctx);
  void drawSkills();
  void drawNotes(app::FrameCtx& ctx, const char* title, services::NoteKind kind);
  void drawCompose(app::FrameCtx& ctx);
  void vote(app::FrameCtx& ctx, const services::NoteView& note, domain::VoteValue value);

  std::string slug_;
  std::optional<domain::QuestType> scope_;
  bool dirty_ = true;
  std::optional<services::AgentCard> card_;
  std::string error_;
  int scrollToEnd_ = 0;      /* frames left; content height is only known after a frame */  // dev QA: --screen=codex-card-notes
  bool confirmLeave_ = false;
  bool showAllBoons_ = false;
  bool showAllBanes_ = false;
  std::vector<services::NoteView> allBoons_;
  std::vector<services::NoteView> allBanes_;
  std::map<std::string, domain::VoteValue> myVotes_;

  // "Write a Boon / Bane" form.
  bool composing_ = false;
  services::NoteKind composeKind_ = services::NoteKind::Boon;
  int composeSkill_ = -1;  // -1 = General
  std::array<char, 1200> composeText_{};
  std::array<char, 48> composeVersion_{};
};

}  // namespace as::screens
