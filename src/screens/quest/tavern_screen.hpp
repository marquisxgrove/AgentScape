#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "domain/snapshot.hpp"
#include "services/codex_store.hpp"
#include "services/quest_service.hpp"

namespace as::screens {

// The Tavern quest board (quest-logs Requirement 3): other adventurers' Quest Logs as pinned parchment notes, twelve
// to a page, filterable by familiar and Quest Type and sortable by newest or most helpful. Which logs appear and in
// what order is QuestService::list; this class only draws and routes.
class TavernScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Tavern; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void refresh(app::FrameCtx& ctx);
  void drawControls(app::FrameCtx& ctx);
  void drawBoard(app::FrameCtx& ctx);

  services::QuestListQuery query_;
  std::vector<services::Agent> agents_;
  bool dirty_ = true;
  std::optional<services::QuestPage> page_;
  std::vector<std::optional<domain::AdventurerSnapshot>> snapshots_;   // parallel to page_->logs
  std::string error_;
  std::string agentSlugOnEnter_;
};

}  // namespace as::screens
