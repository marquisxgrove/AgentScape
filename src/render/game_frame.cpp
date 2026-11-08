#include "render/game_frame.hpp"

#include <algorithm>
#include <cmath>

namespace as::render {

GameFrame::GameFrame(SDL_Renderer* renderer) : renderer_(renderer) {
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, domain::FRAME_W,
                               domain::FRAME_H);
  if (texture_ == nullptr) {
    SDL_Log("GameFrame: could not create frame texture: %s", SDL_GetError());
    return;
  }
  SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
  maxTextureSize_ = static_cast<int>(
      SDL_GetNumberProperty(SDL_GetRendererProperties(renderer_), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0));
}

GameFrame::~GameFrame() {
  if (upscaled_ != nullptr) SDL_DestroyTexture(upscaled_);
  if (texture_ != nullptr) SDL_DestroyTexture(texture_);
}

void GameFrame::updateLayout(SDL_Window* window, domain::FrameScaling mode) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  layout_ = domain::layoutFrame(w, h, mode);
}

SDL_FPoint GameFrame::toFrame(float x, float y) const {
  const auto [fx, fy] = domain::toFramePoint(layout_, x, y);
  return SDL_FPoint{fx, fy};
}

void GameFrame::beginFrame() {
  SDL_SetRenderTarget(renderer_, texture_);
  SDL_SetRenderDrawColor(renderer_, 0x1b, 0x14, 0x10, 0xff);
  SDL_RenderClear(renderer_);
}

SDL_Texture* GameFrame::upscaled(int factor) {
  if (upscaled_ != nullptr && upscaledFactor_ == factor) return upscaled_;
  if (upscaled_ != nullptr) SDL_DestroyTexture(upscaled_);
  upscaled_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, domain::FRAME_W * factor,
                                domain::FRAME_H * factor);
  upscaledFactor_ = upscaled_ != nullptr ? factor : 0;
  if (upscaled_ == nullptr) {
    SDL_Log("GameFrame: could not create %dx upscale texture: %s", factor, SDL_GetError());
    return nullptr;
  }
  SDL_SetTextureScaleMode(upscaled_, SDL_SCALEMODE_LINEAR);
  return upscaled_;
}

void GameFrame::present() {
  const SDL_FRect dst{static_cast<float>(layout_.offsetX), static_cast<float>(layout_.offsetY),
                      static_cast<float>(layout_.width), static_cast<float>(layout_.height)};
  const bool wholeNumber =
      layout_.width % domain::FRAME_W == 0 && layout_.height == domain::FRAME_H * (layout_.width / domain::FRAME_W);

  SDL_Texture* source = texture_;
  if (!wholeNumber) {
    // Enlarge by the next whole number (capped by the GPU's texture limit), then let linear filtering shrink it.
    int factor = std::max(1, static_cast<int>(std::ceil(layout_.scale)));
    if (maxTextureSize_ > 0) factor = std::min(factor, std::max(1, maxTextureSize_ / domain::FRAME_W));
    if (factor > 1) {
      if (SDL_Texture* big = upscaled(factor)) {
        SDL_SetRenderTarget(renderer_, big);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xff);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);  // nearest: texture_ is NEAREST
        source = big;
      }
    }
  }

  SDL_SetRenderTarget(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xff);
  SDL_RenderClear(renderer_);
  if (source == texture_ && !wholeNumber) {
    // Shrinking below 1x (or no intermediate texture): linear for this blit only.
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
    SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
  } else {
    SDL_RenderTexture(renderer_, source, nullptr, &dst);
  }
  SDL_RenderPresent(renderer_);
}

}  // namespace as::render
