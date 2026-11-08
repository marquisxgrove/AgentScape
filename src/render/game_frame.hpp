#pragma once

#include <SDL3/SDL.h>

#include "domain/frame.hpp"

namespace as::render {

// Owns the frame render-target texture. All game drawing (including ImGui) goes into it; present() blits it to the
// window, letterboxed.
//
// The texture is res() times the 765x503 logical frame (domain::frameRenderRes), and drawing into it uses an SDL
// render scale of res(), so game code still works in 765x503 frame pixels. Art is enlarged nearest-neighbour, as
// before, while text is rasterised at res() times its size, so it stays sharp when the frame is shown large. The
// final blit is then either an exact whole-number enlargement (nearest) or a stretch of less than 2x (linear), which
// is "sharp bilinear": each frame pixel keeps a solid core and only its edges are blended.
class GameFrame {
 public:
  explicit GameFrame(SDL_Renderer* renderer);
  ~GameFrame();
  GameFrame(const GameFrame&) = delete;
  GameFrame& operator=(const GameFrame&) = delete;

  bool valid() const { return texture_ != nullptr; }

  // Recomputes the layout and the render resolution from the window size in pixels. Recreates the texture when
  // the resolution changes; if a larger texture can't be made, it keeps drawing at 1x.
  void updateLayout(SDL_Window* window, domain::FrameScaling mode);
  const domain::FrameLayout& layout() const { return layout_; }
  int res() const { return res_; }  // device pixels per frame pixel inside the texture

  // Converts window pixel coordinates to frame pixels.
  SDL_FPoint toFrame(float x, float y) const;

  void beginFrame();  // redirect rendering into the frame texture (scaled by res()) and clear it
  void present();     // restore the window target, clear the letterbox, blit scaled

 private:
  bool makeTexture(int res);

  SDL_Renderer* renderer_;
  SDL_Texture* texture_ = nullptr;
  int res_ = 1;
  int maxTextureSize_ = 0;  // 0 = unknown (no limit applied)
  domain::FrameLayout layout_{};
};

}  // namespace as::render
