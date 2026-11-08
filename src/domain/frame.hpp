#pragma once

#include <utility>

namespace as::domain {

inline constexpr int FRAME_W = 765;
inline constexpr int FRAME_H = 503;

// How the 765x503 frame is presented in the window.
enum class FrameScaling {
  Fit,      // as large as fits, fractional scales allowed (the default, Settings -> "Fullscreen" on)
  Integer,  // whole-number scales only ("Fullscreen" off), minimum 1x
};

struct FrameLayout {
  float scale = 1.0F;  // presented frame pixels per logical frame pixel
  int offsetX = 0;     // may be negative when the window is smaller than the frame (Integer mode)
  int offsetY = 0;
  int width = FRAME_W;   // presented size in drawable pixels
  int height = FRAME_H;
};

// Largest integer scale (>= 1) at which the 765x503 frame fits the drawable area.
int computeFrameScale(int drawableW, int drawableH);

// Largest scale (> 0, possibly fractional, possibly below 1) at which the frame fits the drawable area.
float computeFitScale(int drawableW, int drawableH);

// Scale, presented size and centring offsets (letterbox) for the drawable area.
FrameLayout layoutFrame(int drawableW, int drawableH, FrameScaling mode = FrameScaling::Integer);

// The frame is drawn into a texture `res` times its logical size (fonts rasterised at `res` times their size), so
// text stays sharp when the frame is shown large. Never more than MAX_FRAME_RES.
inline constexpr int MAX_FRAME_RES = 4;

// The render resolution for a layout: the whole part of the scale (Fit), or the largest factor of the whole-number
// scale (Integer), so the final blit stretches by less than 2x (Fit) or by an exact whole number (Integer). It is
// at least 1, at most MAX_FRAME_RES, and the texture stays within `maxTextureSize` (0 = no limit).
int frameRenderRes(const FrameLayout& layout, FrameScaling mode, int maxTextureSize = 0);

// Converts a pointer position in drawable pixels to frame pixels.
std::pair<float, float> toFramePoint(const FrameLayout& layout, float x, float y);

}  // namespace as::domain
