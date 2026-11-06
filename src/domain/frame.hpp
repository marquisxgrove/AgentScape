#pragma once

#include <utility>

namespace as::domain {

inline constexpr int FRAME_W = 765;
inline constexpr int FRAME_H = 503;

struct FrameLayout {
  int scale = 1;
  int offsetX = 0;  // may be negative when the window is smaller than the frame
  int offsetY = 0;
};

// Largest integer scale (>= 1) at which the 765x503 frame fits the drawable area.
int computeFrameScale(int drawableW, int drawableH);

// Scale plus centring offsets (letterbox) for the drawable area.
FrameLayout layoutFrame(int drawableW, int drawableH);

// Converts a pointer position in drawable pixels to frame pixels.
std::pair<float, float> toFramePoint(const FrameLayout& layout, float x, float y);

}  // namespace as::domain
