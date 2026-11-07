#pragma once

#include "app/screen_stack.hpp"

namespace as::screens {

// Dev-only (AS_ENABLE_DEV_SCREENS) visual QA for the paper-doll art: every catalog item on both body
// types in four facings, and every hair / facial hair / clothing style.
class AvatarGalleryScreen : public app::Screen {
 public:
  app::ScreenId id() const override { return app::ScreenId::DevAvatars; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;

 private:
  enum class Page { Items, Looks };
  Page page_ = Page::Items;
  double clock_ = 0;
};

}  // namespace as::screens
