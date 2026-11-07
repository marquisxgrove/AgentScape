#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "domain/snapshot.hpp"
#include "services/quest_service.hpp"

namespace as::screens {

// One Quest Log in full (quest-logs Requirements 1.6, 3.4 and 4): the story as plain text with its line breaks, the
// ratings, Helpful and Report for other people's logs, Edit and Delete for the author's, and Hide / Exclude for a
// Keeper. All rules live in QuestService / KeeperService; this class only draws and routes.
class QuestDetailScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::QuestDetail; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;
  bool onEscape(app::FrameCtx& ctx) override;

 private:
  void reload(app::FrameCtx& ctx);

  std::string questLogId_;
  bool dirty_ = true;
  std::optional<services::QuestLogRecord> log_;
  std::optional<domain::AdventurerSnapshot> snapshot_;
  std::string agentSlug_;
  bool iVoted_ = false;
  bool reporting_ = false;
  bool confirmDelete_ = false;
  std::array<char, 256> reason_{};
};

}  // namespace as::screens
