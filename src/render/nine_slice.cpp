#include "render/nine_slice.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace as::render {

namespace {

ImTextureID texId(const Texture& t) { return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(t.tex)); }

// Tiles the source rect (sx, sy, sw, sh) in texture pixels over the destination rect, cropping the
// last row/column of tiles (with matching UVs) so nothing is ever scaled.
void tile(ImDrawList* dl, const Texture& t, float sx, float sy, float sw, float sh, ImVec2 dmin, ImVec2 dmax) {
  if (sw <= 0 || sh <= 0 || dmax.x <= dmin.x || dmax.y <= dmin.y) return;
  const auto tw = static_cast<float>(t.w);
  const auto th = static_cast<float>(t.h);
  for (float y = dmin.y; y < dmax.y; y += sh) {
    const float h = std::min(sh, dmax.y - y);
    for (float x = dmin.x; x < dmax.x; x += sw) {
      const float w = std::min(sw, dmax.x - x);
      dl->AddImage(texId(t), {x, y}, {x + w, y + h}, {sx / tw, sy / th}, {(sx + w) / tw, (sy + h) / th});
    }
  }
}

}  // namespace

void drawNineSlice(ImDrawList* dl, const NineSlice& ns, ImVec2 min, ImVec2 max, FallbackColors fallback) {
  min = {std::floor(min.x), std::floor(min.y)};
  max = {std::floor(max.x), std::floor(max.y)};
  if (!ns.texture) {
    dl->AddRectFilled(min, max, fallback.fill);
    dl->AddRect(min, max, fallback.edge);
    return;
  }
  const auto& t = ns.texture;
  const auto b = static_cast<float>(ns.border);
  const auto tw = static_cast<float>(t.w);
  const auto th = static_cast<float>(t.h);
  const float cw = tw - 2 * b;  // centre tile size in the texture
  const float ch = th - 2 * b;

  // Clamp corners for rects smaller than two borders.
  const float bx = std::min(b, std::floor((max.x - min.x) / 2));
  const float by = std::min(b, std::floor((max.y - min.y) / 2));
  const ImVec2 inMin{min.x + bx, min.y + by};
  const ImVec2 inMax{max.x - bx, max.y - by};

  // Corners
  tile(dl, t, 0, 0, bx, by, min, inMin);
  tile(dl, t, tw - bx, 0, bx, by, {inMax.x, min.y}, {max.x, inMin.y});
  tile(dl, t, 0, th - by, bx, by, {min.x, inMax.y}, {inMin.x, max.y});
  tile(dl, t, tw - bx, th - by, bx, by, inMax, max);
  // Edges
  tile(dl, t, b, 0, cw, by, {inMin.x, min.y}, {inMax.x, inMin.y});
  tile(dl, t, b, th - by, cw, by, {inMin.x, inMax.y}, {inMax.x, max.y});
  tile(dl, t, 0, b, bx, ch, {min.x, inMin.y}, {inMin.x, inMax.y});
  tile(dl, t, tw - bx, b, bx, ch, {inMax.x, inMin.y}, {max.x, inMax.y});
  // Centre
  tile(dl, t, b, b, cw, ch, inMin, inMax);
}

}  // namespace as::render
