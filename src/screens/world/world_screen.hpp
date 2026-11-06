#pragma once

#include "app/screen_stack.hpp"

namespace as::screens {

// Placeholder Promptmoor hub until the World_Map (game-shell task 9) lands.
class WorldScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::World; }
  void draw(app::FrameCtx& ctx) override;
};

}  // namespace as::screens
