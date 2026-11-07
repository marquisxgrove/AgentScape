#include "game/kiroween_layer.hpp"

#include <cstdint>
#include <filesystem>

#include "platform/paths.hpp"

namespace as::game {

namespace {
ImTextureID texId(const render::Texture& t) {
  return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(t.tex));
}
std::filesystem::path sprite(const char* name) { return platform::assetsDir() / "sprites" / name; }
}  // namespace

void KiroweenLayer::initWorld(std::vector<ImVec2> pumpkinSpots) {
  fog_ = Drifter{0, 0, 7.0F, 2.5F};
  pumpkins_.clear();
  for (const auto& s : pumpkinSpots) {
    Pumpkin p;
    p.x = s.x;
    p.y = s.y;
    p.phase = (s.x + s.y) * 0.05F;  // desync their flicker
    pumpkins_.push_back(p);
  }
}

void KiroweenLayer::initBats(float width, float height, std::size_t count) {
  batWidth_ = width;
  bats_ = makeBats(count, width, height, 0xB475ULL);
}

void KiroweenLayer::step(double dt, bool reduceMotion) {
  const bool animate = !reduceMotion;
  stepDrifter(fog_, dt, 64.0F, 64.0F, animate);
  for (auto& p : pumpkins_) stepPumpkin(p, dt, animate);
  for (auto& b : bats_) stepBat(b, dt, batWidth_, 16.0F, animate);
}

void KiroweenLayer::drawFog(ImDrawList* dl, render::TextureCache& tex, ImVec2 min, ImVec2 max) {
  const auto fog = tex.load(sprite("fog.png"));
  if (!fog) return;
  const auto fw = static_cast<float>(fog.w);
  const auto fh = static_cast<float>(fog.h);
  dl->PushClipRect(min, max, true);
  // Tile with a per-frame scroll offset so the fog drifts.
  for (float y = min.y - fh + fog_.y; y < max.y; y += fh) {
    for (float x = min.x - fw + fog_.x; x < max.x; x += fw) {
      dl->AddImage(texId(fog), {x, y}, {x + fw, y + fh});
    }
  }
  dl->PopClipRect();
}

void KiroweenLayer::drawPumpkins(ImDrawList* dl, render::TextureCache& tex, ImVec2 origin) {
  const auto pk = tex.load(sprite("pumpkin.png"));
  if (!pk) return;
  const auto w = static_cast<float>(pk.w);
  const auto h = static_cast<float>(pk.h);
  for (const auto& p : pumpkins_) {
    const ImVec2 at{origin.x + p.x, origin.y + p.y};
    const auto glow = static_cast<int>(60 * p.flicker);
    dl->AddRectFilled({at.x - 2, at.y - 2}, {at.x + w + 2, at.y + h + 2},
                      IM_COL32(255, 150, 30, static_cast<int>(glow)));  // soft halo
    const auto tint = IM_COL32(255, 255, 255, 255);
    dl->AddImage(texId(pk), at, {at.x + w, at.y + h}, {0, 0}, {1, 1}, tint);
  }
}

void KiroweenLayer::drawBats(ImDrawList* dl, render::TextureCache& tex, ImVec2 origin) {
  const auto bat = tex.load(sprite("bat.png"));
  if (!bat) return;
  constexpr float SCALE = 2.0F;  // the bat sprite is tiny; draw it at 2x so it reads
  const auto fw = static_cast<float>(bat.w) / 2.0F;  // two frames side by side
  const auto fh = static_cast<float>(bat.h);
  for (const auto& b : bats_) {
    const ImVec2 at{origin.x + b.x, origin.y + b.y};
    const float u0 = b.frame == 0 ? 0.0F : 0.5F;
    dl->AddImage(texId(bat), at, {at.x + fw * SCALE, at.y + fh * SCALE}, {u0, 0}, {u0 + 0.5F, 1});
  }
}

}  // namespace as::game
