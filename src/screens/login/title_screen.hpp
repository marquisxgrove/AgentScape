#pragma once

#include "app/screen_stack.hpp"

namespace as::screens {

// Temporary viewport content until the real login screen (game-shell task 7) replaces it.
class TitleScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::Login; }
  void draw(app::FrameCtx& ctx) override;
};

}  // namespace as::screens
