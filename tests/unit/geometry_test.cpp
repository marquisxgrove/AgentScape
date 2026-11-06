#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "domain/geometry.hpp"

using namespace as::domain;

TEST_CASE("point-in-polygon handles a convex quad", "[unit][geometry]") {
  const std::vector<Point> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
  CHECK(pointInPolygon(square, {5, 5}));
  CHECK_FALSE(pointInPolygon(square, {15, 5}));
  CHECK_FALSE(pointInPolygon(square, {5, 15}));
  CHECK_FALSE(pointInPolygon(square, {-1, -1}));
}

TEST_CASE("point-in-polygon handles a concave L shape", "[unit][geometry]") {
  // An L: outer 10x10 with the top-right 5x5 cut out.
  const std::vector<Point> shape{{0, 0}, {5, 0}, {5, 5}, {10, 5}, {10, 10}, {0, 10}};
  CHECK(pointInPolygon(shape, {2, 2}));    // in the left arm
  CHECK(pointInPolygon(shape, {7, 7}));    // in the bottom arm
  CHECK_FALSE(pointInPolygon(shape, {7, 2}));  // in the notch
}

TEST_CASE("degenerate polygons are never inside", "[unit][geometry]") {
  const std::vector<Point> line{{0, 0}, {10, 0}};
  CHECK_FALSE(pointInPolygon(line, {5, 0}));
  CHECK_FALSE(pointInPolygon({}, {0, 0}));
}

TEST_CASE("bounding box spans the polygon extent", "[unit][geometry]") {
  const std::vector<Point> poly{{60, 70}, {190, 60}, {180, 170}, {70, 160}};
  const auto bb = boundingBox(poly);
  CHECK(bb.x == 60.0F);
  CHECK(bb.y == 60.0F);
  CHECK(bb.w == 130.0F);
  CHECK(bb.h == 110.0F);
  CHECK(boundingBox({}).w == 0.0F);
}
