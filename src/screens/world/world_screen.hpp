#pragma once

#include "app/screen_stack.hpp"
#include "game/hotspots.hpp"
#include <optional>

#include "game/kiroween_layer.hpp"
#include "services/adventurer_service.hpp"

namespace as::screens {

// The Promptmoor hub: a clickable town map in the viewport plus an accessible, keyboard/gamepad
// navigable hotspot list. Left-click runs the default action; right-click / Menu key opens the
// context menu; hover (or focus) shows hover text.
class WorldScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::World; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;
  void onContextAction(app::FrameCtx& ctx, const domain::MenuAction& action) override;

 private:
  void activate(app::FrameCtx& ctx, const game::Hotspot& h);
  void openMenu(app::FrameCtx& ctx, const game::Hotspot& h, float frameX, float frameY);

  int listFocus_ = 0;  // index into the visible hotspot list (keyboard/gamepad)
  std::optional<services::Adventurer> adventurer_;  // the active Adventurer standing in the square
  bool adventurerLoaded_ = false;
  double clock_ = 0;
  game::KiroweenLayer kiroween_;
  bool kiroweenInit_ = false;
};

}  // namespace as::screens
