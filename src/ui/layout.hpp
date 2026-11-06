#pragma once

#include "domain/frame.hpp"

namespace as::ui {

// Fixed Game_Frame regions in frame pixels (art-direction.md: viewport 512x334, tab panel 241x334,
// chatbox 765x165). 4 px stone gutters around the top regions.
struct FrameRect {
  int x, y, w, h;
  constexpr int right() const { return x + w; }
  constexpr int bottom() const { return y + h; }
};

inline constexpr FrameRect VIEWPORT{4, 4, 512, 334};
inline constexpr FrameRect TAB_PANEL{520, 4, 241, 334};
inline constexpr FrameRect CHATBOX{0, 338, 765, 165};

static_assert(TAB_PANEL.right() + 4 == domain::FRAME_W);
static_assert(CHATBOX.bottom() == domain::FRAME_H);
static_assert(VIEWPORT.bottom() == CHATBOX.y);  // the chatbox border is the divider

}  // namespace as::ui
