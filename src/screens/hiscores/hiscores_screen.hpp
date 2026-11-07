#pragma once

#include <array>
#include <optional>
#include <string>

#include "app/screen_stack.hpp"
#include "domain/hiscores.hpp"
#include "services/hiscores_service.hpp"

namespace as::screens {

// The Hiscores Obelisk (quest-logs Requirement 7): which familiars lead each board, and which adventurers have earned
// the most Questing XP. Ranks, paging and the name search are HiscoresService; this class only draws and routes.
class HiscoresScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Hiscores; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  enum class Tab { Agents, Adventurers };

  void refresh(app::FrameCtx& ctx);
  void drawControls(app::FrameCtx& ctx);
  void drawAgents(app::FrameCtx& ctx);
  void drawAdventurers(app::FrameCtx& ctx);
  void drawPaging();
  int boardIndex() const;
  void setBoardIndex(int index);

  Tab tab_ = Tab::Agents;
  domain::AgentBoard board_;
  std::size_t page_ = 0;
  std::array<char, 40> search_{};
  std::string pendingSearch_;      // applied on the next refresh, then forgotten, so paging afterwards is free
  bool dirty_ = true;
  std::optional<services::AgentHiscores> agents_;
  std::optional<services::AdventurerHiscores> adventurers_;
  std::optional<std::size_t> highlight_;   // row on this page that the last search found
  bool searchMissed_ = false;
  std::string error_;
};

}  // namespace as::screens
