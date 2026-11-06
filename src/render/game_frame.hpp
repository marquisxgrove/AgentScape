#pragma once

#include <SDL3/SDL.h>

#include "domain/frame.hpp"

namespace as::render {

// Owns the 765x503 render-target texture. All game drawing (including ImGui) goes into it;
// present() blits it to the window at the largest integer scale, letterboxed.
class GameFrame {
 public:
  explicit GameFrame(SDL_Renderer* renderer);
  ~GameFrame();
  GameFrame(const GameFrame&) = delete;
  GameFrame& operator=(const GameFrame&) = delete;

  bool valid() const { return texture_ != nullptr; }

  // Recomputes the layout from the current window size in pixels.
  void updateLayout(SDL_Window* window);
  const domain::FrameLayout& layout() const { return layout_; }

  // Converts window pixel coordinates to frame pixels.
  SDL_FPoint toFrame(float x, float y) const;

  void beginFrame();  // redirect rendering into the frame texture and clear it
  void present();     // restore the window target, clear the letterbox, blit scaled

 private:
  SDL_Renderer* renderer_;
  SDL_Texture* texture_ = nullptr;
  domain::FrameLayout layout_{};
};

}  // namespace as::render
