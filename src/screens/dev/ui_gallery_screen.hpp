#pragma once

#include "app/screen_stack.hpp"

namespace as::screens {

// Dev-only (AS_ENABLE_DEV_SCREENS) visual QA page for the retro UI primitives:
// palette, ShadowText, RichText/markup, PixelButton states, StonePanel variants, hover text.
class UiGalleryScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::DevGallery; }
  void draw(app::FrameCtx& ctx) override;

 private:
  int clicks_ = 0;
};

}  // namespace as::screens
