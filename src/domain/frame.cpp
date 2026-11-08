#include "domain/frame.hpp"

#include <algorithm>
#include <cmath>

namespace as::domain {

int computeFrameScale(int drawableW, int drawableH) {
  const int sx = drawableW / FRAME_W;
  const int sy = drawableH / FRAME_H;
  return std::max(1, std::min(sx, sy));
}

float computeFitScale(int drawableW, int drawableH) {
  const float sx = static_cast<float>(std::max(1, drawableW)) / static_cast<float>(FRAME_W);
  const float sy = static_cast<float>(std::max(1, drawableH)) / static_cast<float>(FRAME_H);
  return std::min(sx, sy);
}

FrameLayout layoutFrame(int drawableW, int drawableH, FrameScaling mode) {
  FrameLayout layout;
  if (mode == FrameScaling::Integer) {
    const int s = computeFrameScale(drawableW, drawableH);
    layout.scale = static_cast<float>(s);
    layout.width = FRAME_W * s;
    layout.height = FRAME_H * s;
  } else {
    layout.scale = computeFitScale(drawableW, drawableH);
    // Round the presented size, but never past the drawable area (float error at an exact fit).
    layout.width = std::clamp(static_cast<int>(std::lround(FRAME_W * layout.scale)), 1, std::max(1, drawableW));
    layout.height = std::clamp(static_cast<int>(std::lround(FRAME_H * layout.scale)), 1, std::max(1, drawableH));
  }
  layout.offsetX = (drawableW - layout.width) / 2;
  layout.offsetY = (drawableH - layout.height) / 2;
  return layout;
}

int frameRenderRes(const FrameLayout& layout, FrameScaling mode, int maxTextureSize) {
  const int whole = std::max(1, static_cast<int>(std::floor(layout.scale + 1e-4F)));
  const auto fits = [maxTextureSize](int r) {
    return maxTextureSize <= 0 || (FRAME_W * r <= maxTextureSize && FRAME_H * r <= maxTextureSize);
  };
  for (int r = std::min(whole, MAX_FRAME_RES); r > 1; --r) {
    if (!fits(r)) continue;
    if (mode == FrameScaling::Integer && whole % r != 0) continue;
    return r;
  }
  return 1;
}

std::pair<float, float> toFramePoint(const FrameLayout& layout, float x, float y) {
  const float sx = static_cast<float>(layout.width) / static_cast<float>(FRAME_W);
  const float sy = static_cast<float>(layout.height) / static_cast<float>(FRAME_H);
  return {(x - static_cast<float>(layout.offsetX)) / sx, (y - static_cast<float>(layout.offsetY)) / sy};
}

}  // namespace as::domain
