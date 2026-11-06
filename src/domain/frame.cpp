#include "domain/frame.hpp"

#include <algorithm>

namespace as::domain {

int computeFrameScale(int drawableW, int drawableH) {
  const int sx = drawableW / FRAME_W;
  const int sy = drawableH / FRAME_H;
  return std::max(1, std::min(sx, sy));
}

FrameLayout layoutFrame(int drawableW, int drawableH) {
  FrameLayout layout;
  layout.scale = computeFrameScale(drawableW, drawableH);
  layout.offsetX = (drawableW - FRAME_W * layout.scale) / 2;
  layout.offsetY = (drawableH - FRAME_H * layout.scale) / 2;
  return layout;
}

std::pair<float, float> toFramePoint(const FrameLayout& layout, float x, float y) {
  const auto s = static_cast<float>(layout.scale);
  return {(x - static_cast<float>(layout.offsetX)) / s, (y - static_cast<float>(layout.offsetY)) / s};
}

}  // namespace as::domain
