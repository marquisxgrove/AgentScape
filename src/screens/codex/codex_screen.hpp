#pragma once

#include <array>
#include <optional>
#include <string>

#include "app/screen_stack.hpp"
#include "domain/codex_query.hpp"
#include "services/codex_service.hpp"

namespace as::screens {

// The Grand Codex (agent-codex Requirement 1): every approved Agent as a book on a shelf, or as the same
// list in a table. Search, filters, Quest Type scope, sort and paging all live in domain::ListQuery; this
// class only draws and routes. The query survives opening a card and coming back.
class CodexScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Codex; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void refresh(app::FrameCtx& ctx);
  void drawControls(app::FrameCtx& ctx);
  void drawShelf(app::FrameCtx& ctx);
  void drawTable(app::FrameCtx& ctx);
  void drawPaging();
  void open(app::FrameCtx& ctx, const services::CodexRow& row);
  int sortIndex() const;
  void setSortIndex(int index);

  domain::ListQuery query_;
  std::optional<domain::QuestType> scope_;
  bool shelf_ = true;
  std::array<char, 48> search_{};
  bool dirty_ = true;
  std::optional<services::CodexPage> page_;
  std::string error_;
};

}  // namespace as::screens
