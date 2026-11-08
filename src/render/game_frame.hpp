#pragma once

#include <SDL3/SDL.h>

#include "domain/frame.hpp"

namespace as::render {

// Owns the 765x503 render-target texture. All game drawing (including ImGui) goes into it; present() blits it
// to the window, letterboxed. At an integer scale the blit is nearest-neighbour. At a fractional (Fit) scale it
// is "sharp bilinear": the frame is first enlarged nearest-neighbour by the next whole number into an
// intermediate texture, which is then shrunk to the exact size with linear filtering, so pixels stay crisp and
// even instead of alternating 1- and 2-pixel columns.
class GameFrame {
 public:
  explicit GameFrame(SDL_Renderer* renderer);
  ~GameFrame();
  GameFrame(const GameFrame&) = delete;
  GameFrame& operator=(const GameFrame&) = delete;

  bool valid() const { return texture_ != nullptr; }

  // Recomputes the layout from the current window size in pixels.
  void updateLayout(SDL_Window* window, domain::FrameScaling mode);
  const domain::FrameLayout& layout() const { return layout_; }

  // Converts window pixel coordinates to frame pixels.
  SDL_FPoint toFrame(float x, float y) const;

  void beginFrame();  // redirect rendering into the frame texture and clear it
  void present();     // restore the window target, clear the letterbox, blit scaled

 private:
  // The intermediate texture at `factor` x the frame size, or nullptr if it can't be made.
  SDL_Texture* upscaled(int factor);

  SDL_Renderer* renderer_;
  SDL_Texture* texture_ = nullptr;
  SDL_Texture* upscaled_ = nullptr;  // sharp-bilinear intermediate, recreated when the factor changes
  int upscaledFactor_ = 0;
  int maxTextureSize_ = 0;  // 0 = unknown (no limit applied)
  domain::FrameLayout layout_{};
};

}  // namespace as::render
