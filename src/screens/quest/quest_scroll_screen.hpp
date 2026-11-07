#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "services/codex_store.hpp"
#include "ui/quest_form.hpp"

namespace as::screens {

// The quest scroll (quest-logs Requirement 1): a three-step form for writing, or editing, a Quest Log.
// What the form may hold and when it may move on is ui::QuestForm; whether a log may be saved is
// QuestService. This class only draws, and routes the result.
class QuestScrollScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::QuestScroll; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;
  bool onEscape(app::FrameCtx& ctx) override;

 private:
  bool canStepBack() const;
  void load(app::FrameCtx& ctx);
  void openExisting(app::FrameCtx& ctx, const std::string& questLogId);
  void submit(app::FrameCtx& ctx);
  void drawWhere(app::FrameCtx& ctx);
  void drawRating();
  void drawStory(app::FrameCtx& ctx);
  void drawErrors(ui::QuestStep step);
  void syncBuffers();   // form -> text buffers (after loading a log)

  ui::QuestForm form_;
  bool loaded_ = false;
  bool needAdventurer_ = false;
  bool redirected_ = false;   // already sent to the Costumer once for this visit (don't trap the player there)
  std::string agentOnEnter_;
  std::string scopeOnEnter_;
  std::string editOnEnter_;
  std::optional<int> devStep_;
  bool devDemo_ = false;
  bool devSubmit_ = false;

  std::vector<services::Agent> agents_;
  std::map<std::string, std::string> fieldErrors_;   // field -> message from the last refused Save
  bool confirmDelete_ = false;
  bool escapeArmed_ = false;   // Escape on the first step warned once; the next one discards the draft

  std::array<char, 48> version_{};
  std::array<char, 96> title_{};
  std::array<char, 8100> body_{};   // 2000 characters of up to four bytes each
};

}  // namespace as::screens
