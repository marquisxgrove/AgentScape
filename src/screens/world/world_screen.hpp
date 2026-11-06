#pragma once

#include "app/screen_stack.hpp"
#include "game/hotspots.hpp"

namespace as::screens {

// The Promptmoor hub: a clickable town map in the viewport plus an accessible, keyboard/gamepad
// navigable hotspot list. Left-click runs the default action; right-click / Menu key opens the
// context menu; hover (or focus) shows hover text.
class WorldScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::World; }
  void draw(app::FrameCtx& ctx) override;

 private:
  void activate(app::FrameCtx& ctx, const game::Hotspot& h);
  void openMenu(app::FrameCtx& ctx, const game::Hotspot& h, float frameX, float frameY);

  int listFocus_ = 0;  // index into the visible hotspot list (keyboard/gamepad)
};

}  // namespace as::screens
