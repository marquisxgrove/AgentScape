#include "domain/geometry.hpp"

#include <algorithm>
#include <limits>

namespace as::domain {

bool pointInPolygon(std::span<const Point> poly, Point p) {
  if (poly.size() < 3) return false;
  bool inside = false;
  for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    const Point& a = poly[i];
    const Point& b = poly[j];
    const bool straddles = (a.y > p.y) != (b.y > p.y);
    if (straddles) {
      const float xCross = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
      if (p.x < xCross) inside = !inside;
    }
  }
  return inside;
}

Rect boundingBox(std::span<const Point> poly) {
  if (poly.empty()) return {0, 0, 0, 0};
  float minX = std::numeric_limits<float>::max();
  float minY = minX;
  float maxX = std::numeric_limits<float>::lowest();
  float maxY = maxX;
  for (const auto& pt : poly) {
    minX = std::min(minX, pt.x);
    minY = std::min(minY, pt.y);
    maxX = std::max(maxX, pt.x);
    maxY = std::max(maxY, pt.y);
  }
  return {minX, minY, maxX - minX, maxY - minY};
}

}  // namespace as::domain
