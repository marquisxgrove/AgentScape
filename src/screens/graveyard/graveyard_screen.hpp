#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "services/codex_service.hpp"

namespace as::screens {

// The Hall of Hallucinations (agent-codex Requirement 7), open only while Kiroween is on: up to 13 tombstones
// for the community's top-voted Banes. Each shows the Agent, the tagged Skill and the Bane as an epitaph;
// choosing one opens that Agent's card. A list view offers the same content for keyboard and gamepad use.
// Which Banes qualify and in what order is domain::pickTombstones; this class only draws and routes.
class GraveyardScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Graveyard; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  void drawScene(app::FrameCtx& ctx);
  void drawList(app::FrameCtx& ctx);
  void open(app::FrameCtx& ctx, const services::Tombstone& t);

  bool loaded_ = false;
  bool sceneView_ = true;
  std::vector<services::Tombstone> stones_;
  std::string error_;
  int shown_ = 0;   // the stone whose epitaph is displayed (hovered or focused last)
};

}  // namespace as::screens
