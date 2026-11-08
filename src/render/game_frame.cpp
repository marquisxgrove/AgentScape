#include "render/game_frame.hpp"

namespace as::render {

GameFrame::GameFrame(SDL_Renderer* renderer) : renderer_(renderer) {
  maxTextureSize_ = static_cast<int>(
      SDL_GetNumberProperty(SDL_GetRendererProperties(renderer_), SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0));
  if (!makeTexture(1)) SDL_Log("GameFrame: could not create frame texture: %s", SDL_GetError());
}

GameFrame::~GameFrame() {
  if (texture_ != nullptr) SDL_DestroyTexture(texture_);
}

bool GameFrame::makeTexture(int res) {
  SDL_Texture* made = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                        domain::FRAME_W * res, domain::FRAME_H * res);
  if (made == nullptr) return false;
  SDL_SetTextureScaleMode(made, SDL_SCALEMODE_NEAREST);
  if (texture_ != nullptr) SDL_DestroyTexture(texture_);
  texture_ = made;
  res_ = res;
  return true;
}

void GameFrame::updateLayout(SDL_Window* window, domain::FrameScaling mode) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  layout_ = domain::layoutFrame(w, h, mode);
  const int wanted = domain::frameRenderRes(layout_, mode, maxTextureSize_);
  if (wanted != res_ && !makeTexture(wanted)) {
    SDL_Log("GameFrame: could not create a %dx frame texture (%s); drawing at 1x", wanted, SDL_GetError());
    if (res_ != 1) makeTexture(1);
  }
}

SDL_FPoint GameFrame::toFrame(float x, float y) const {
  const auto [fx, fy] = domain::toFramePoint(layout_, x, y);
  return SDL_FPoint{fx, fy};
}

void GameFrame::beginFrame() {
  SDL_SetRenderTarget(renderer_, texture_);
  // The scale belongs to this target only; the window target keeps 1.
  SDL_SetRenderScale(renderer_, static_cast<float>(res_), static_cast<float>(res_));
  SDL_SetRenderDrawColor(renderer_, 0x1b, 0x14, 0x10, 0xff);
  SDL_RenderClear(renderer_);
}

void GameFrame::present() {
  const SDL_FRect dst{static_cast<float>(layout_.offsetX), static_cast<float>(layout_.offsetY),
                      static_cast<float>(layout_.width), static_cast<float>(layout_.height)};
  const int texW = domain::FRAME_W * res_;
  const int texH = domain::FRAME_H * res_;
  const bool wholeNumber = layout_.width % texW == 0 && layout_.height == texH * (layout_.width / texW);

  SDL_SetRenderTarget(renderer_, nullptr);
  SDL_SetRenderScale(renderer_, 1.0F, 1.0F);
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xff);
  SDL_RenderClear(renderer_);
  if (wholeNumber) {
    SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
  } else {
    // A stretch of less than 2x (or a shrink, for a window smaller than the frame): linear for this blit only.
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_LINEAR);
    SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
  }
  SDL_RenderPresent(renderer_);
}

}  // namespace as::render
