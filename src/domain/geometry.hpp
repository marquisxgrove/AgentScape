#pragma once

#include <span>
#include <vector>

namespace as::domain {

struct Point {
  float x = 0;
  float y = 0;
  bool operator==(const Point&) const = default;
};

struct Rect {
  float x, y, w, h;
  constexpr float cx() const { return x + w / 2; }
  constexpr float cy() const { return y + h / 2; }
};

// Even-odd point-in-polygon test (ray casting). A point exactly on an edge counts as inside for at
// most one adjacent polygon, which is fine for mutually-exclusive hotspots.
bool pointInPolygon(std::span<const Point> polygon, Point p);

// Axis-aligned bounding box of a polygon (empty polygon -> {0,0,0,0}).
Rect boundingBox(std::span<const Point> polygon);

}  // namespace as::domain
