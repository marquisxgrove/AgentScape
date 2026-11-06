#include "render/game_frame.hpp"

namespace as::render {

GameFrame::GameFrame(SDL_Renderer* renderer) : renderer_(renderer) {
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, domain::FRAME_W,
                               domain::FRAME_H);
  if (texture_ == nullptr) {
    SDL_Log("GameFrame: could not create frame texture: %s", SDL_GetError());
    return;
  }
  SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
}

GameFrame::~GameFrame() {
  if (texture_ != nullptr) {
    SDL_DestroyTexture(texture_);
  }
}

void GameFrame::updateLayout(SDL_Window* window) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  layout_ = domain::layoutFrame(w, h);
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

void GameFrame::present() {
  SDL_SetRenderTarget(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xff);
  SDL_RenderClear(renderer_);
  const SDL_FRect dst{static_cast<float>(layout_.offsetX), static_cast<float>(layout_.offsetY),
                      static_cast<float>(domain::FRAME_W * layout_.scale),
                      static_cast<float>(domain::FRAME_H * layout_.scale)};
  SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
  SDL_RenderPresent(renderer_);
}

}  // namespace as::render
