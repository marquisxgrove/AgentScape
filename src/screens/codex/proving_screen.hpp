#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "services/codex_service.hpp"
#include "services/proving_service.hpp"

namespace as::screens {

// The Proving Grounds (agent-codex Requirement 6): pick 2 or 3 approved Agents and a Quest Type, and see every
// Skill side by side with the highest level in each row marked. The last comparison is remembered per user.
// The table itself comes from domain::compare; this class only draws and routes.
class ProvingScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Proving; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void load(app::FrameCtx& ctx);
  void save(app::FrameCtx& ctx);
  void drawPicker(app::FrameCtx& ctx);
  void drawTable(app::FrameCtx& ctx);

  bool loaded_ = false;
  std::optional<std::string> addOnEnter_;
  std::optional<std::string> scopeOnEnter_;
  services::ProvingSelection sel_;
  std::vector<services::Agent> agents_;   // every approved Agent, for the picker
  int picking_ = -1;                      // which slot's list is open, or -1
  bool devSelection_ = false;             // dev QA: a fixed selection that is not saved
  bool dirty_ = true;                     // the comparison must be rebuilt
  std::vector<domain::CompareColumn> columns_;
};

}  // namespace as::screens
