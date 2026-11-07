#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <set>
#include <sstream>

#include "domain/items.hpp"
#include "render/avatar_atlas.hpp"
#include "render/avatar_compositor.hpp"

using namespace as;
using namespace as::render;
using domain::Facing;
using domain::LayerKey;

namespace {
// A synthetic atlas: each named layer is a single marker pixel per cell, at a chosen position and index.
struct Marker {
  std::string layer;
  int x, y;
  std::uint8_t index;
};

domain::Palette256 testPalette() {
  domain::Palette256 p{};
  for (std::size_t i = 0; i < 256; ++i) {
    p[i] = {static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(255 - i), static_cast<std::uint8_t>(i / 2), 255};
  }
  p[0].a = 0;
  return p;
}

AvatarAtlas makeAtlas(const std::vector<Marker>& markers) {
  AvatarAtlas atlas;
  atlas.meta.palette = testPalette();
  const int cols = atlas.meta.facings * atlas.meta.frames;
  atlas.image.w = cols * atlas.meta.cellW;
  atlas.image.h = static_cast<int>(markers.size()) * atlas.meta.cellH;
  atlas.image.px.assign(static_cast<std::size_t>(atlas.image.w) * static_cast<std::size_t>(atlas.image.h), 0);
  int row = 0;
  for (const auto& m : markers) {
    atlas.meta.rows[m.layer] = row;
    for (int c = 0; c < cols; ++c) {
      const std::size_t at = static_cast<std::size_t>((row * atlas.meta.cellH + m.y) * atlas.image.w +
                                                      c * atlas.meta.cellW + m.x);
      atlas.image.px[at] = m.index;
    }
    ++row;
  }
  return atlas;
}

domain::Rgba pixel(const RgbaImage& im, int x, int y) {
  const auto* p = im.at(x, y);
  return {p[0], p[1], p[2], p[3]};
}
}  // namespace

TEST_CASE("the metadata parser accepts a well-formed atlas and rejects broken ones", "[unit][avatar]") {
  std::string pal = "[";
  for (int i = 0; i < 256; ++i) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "\"#%02x%02x%02x\"%s", i, 255 - i, i / 2, i == 255 ? "" : ",");
    pal += buf;
  }
  pal += "]";
  const std::string good = R"({"cell":{"w":32,"h":48},"facings":["S","E","N","W"],"frames":2,"palette":)" + pal +
                           R"(,"rows":{"body/A":0,"hair/3":1}})";
  const auto meta = parseAtlasMeta(good);
  REQUIRE(meta.has_value());
  CHECK(meta->cellW == 32);
  CHECK(meta->rows.at("hair/3") == 1);
  CHECK(meta->palette[0].a == 0);    // index 0 is always the transparent key
  CHECK(meta->palette[5].r == 5);

  CHECK_FALSE(parseAtlasMeta("not json").has_value());
  CHECK_FALSE(parseAtlasMeta(R"({"cell":{"w":32,"h":48}})").has_value());
  std::string shortPalette = good;
  const auto first = shortPalette.find("\"#00ff00\",");
  REQUIRE(first != std::string::npos);
  shortPalette.erase(first, 10);  // drop entry 0: 255 entries now
  CHECK_FALSE(parseAtlasMeta(shortPalette).has_value());
}

TEST_CASE("indices are recovered from decoded RGBA pixels", "[unit][avatar]") {
  const auto pal = testPalette();
  // Pixels: transparent, index 7, index 200, and a colour that is not in the palette.
  const std::uint8_t rgba[] = {0, 0, 0, 0, pal[7].r, pal[7].g, pal[7].b, 255, pal[200].r, pal[200].g, pal[200].b, 255, 1, 2, 3, 255};
  int unknown = 0;
  const auto img = indexFromRgba(rgba, 4, 1, pal, &unknown);
  CHECK(img.px == std::vector<std::uint8_t>{0, 7, 200, 0});
  CHECK(unknown == 1);
}

TEST_CASE("layers draw back to front, so a later layer wins a shared pixel", "[unit][avatar]") {
  const auto atlas = makeAtlas({{"body/A", 5, 20, 20}, {"legs/0", 5, 20, 41}, {"feet/0", 8, 44, 49},
                                {"torso/0", 5, 20, 33}, {"arms/0", 0, 0, 0}, {"hands/0", 0, 0, 0}, {"hair/0", 0, 0, 0}});
  domain::Appearance a;  // all style 0
  const auto im = compositeAvatar(atlas, a, {}, Facing::S, 0);
  // Torso (drawn after body and legs) wins the shared pixel; its index 33 is in the torso ramp.
  const auto pal = domain::buildPalette(a, atlas.meta.palette);
  CHECK(pixel(im, 5, 20) == pal[33]);
  CHECK(pixel(im, 5, 20).a == 255);
  CHECK(pixel(im, 8, 44) == pal[49]);   // an uncontested pixel from another layer
  CHECK(pixel(im, 0, 0).a == 0);        // untouched pixels stay transparent
}

TEST_CASE("the reserved ramps take the Appearance colours; other indices keep the atlas palette",
          "[unit][avatar]") {
  const auto atlas = makeAtlas({{"body/A", 3, 3, 20}, {"torso/0", 4, 3, 36}, {"legs/0", 5, 3, 44}, {"feet/0", 6, 3, 50},
                                {"hair/0", 7, 3, 28}, {"hands/0", 8, 3, 9}, {"arms/0", 0, 0, 0}});
  domain::Appearance red;
  domain::Appearance blue;
  blue.colors = {5, 5, 5, 5, 5};
  const auto a = compositeAvatar(atlas, red, {}, Facing::S, 0);
  const auto b = compositeAvatar(atlas, blue, {}, Facing::S, 0);
  for (const int x : {3, 4, 5, 6, 7}) CHECK_FALSE(pixel(a, x, 3) == pixel(b, x, 3));  // recoloured
  CHECK(pixel(a, 8, 3) == pixel(b, 8, 3));                                            // index 9 is fixed
  CHECK(pixel(a, 8, 3) == atlas.meta.palette[9]);
}

TEST_CASE("equipped items draw from their item sprite; a base-layer item draws nothing", "[unit][avatar]") {
  const auto atlas = makeAtlas({{"body/A", 0, 0, 0}, {"legs/0", 0, 0, 0}, {"feet/0", 0, 0, 0}, {"torso/0", 0, 0, 0},
                                {"arms/0", 0, 0, 0}, {"hands/0", 0, 0, 0}, {"hair/0", 0, 0, 0},
                                {"item/witch_hat", 10, 2, 12}});
  domain::Appearance a;
  domain::Loadout withHat = domain::defaultLoadout();  // Plain Tunic, Trousers, Boots have no overlay
  withHat[domain::Slot::Head] = "witch_hat";
  const auto pal = domain::buildPalette(a, atlas.meta.palette);
  CHECK(pixel(compositeAvatar(atlas, a, withHat, Facing::S, 0), 10, 2) == pal[12]);
  CHECK(pixel(compositeAvatar(atlas, a, domain::defaultLoadout(), Facing::S, 0), 10, 2).a == 0);
  CHECK_FALSE(layerRowName(LayerKey::BodyItem, a, domain::defaultLoadout()).has_value());
  CHECK(layerRowName(LayerKey::HeadItem, a, withHat) == "item/witch_hat");
}

TEST_CASE("a hiding item removes the hidden layers from the picture, not from the appearance",
          "[unit][avatar]") {
  const auto atlas = makeAtlas({{"body/A", 0, 0, 0}, {"legs/0", 0, 0, 0}, {"feet/0", 0, 0, 0}, {"torso/0", 0, 0, 0},
                                {"arms/0", 0, 0, 0}, {"hands/0", 0, 0, 0}, {"hair/4", 12, 4, 28},
                                {"facial/2", 13, 12, 25}, {"item/jack_mask", 0, 0, 0}});
  domain::Appearance a;
  a.hair = 4;
  a.facialHair = 2;
  const domain::Appearance before = a;
  domain::Loadout masked;
  masked[domain::Slot::Head] = "jack_mask";  // hides hair and facial hair
  const auto bare = compositeAvatar(atlas, a, {}, Facing::S, 0);
  const auto hidden = compositeAvatar(atlas, a, masked, Facing::S, 0);
  CHECK(bare.at(12, 4)[3] == 255);
  CHECK(bare.at(13, 12)[3] == 255);
  CHECK(hidden.at(12, 4)[3] == 0);
  CHECK(hidden.at(13, 12)[3] == 0);
  CHECK(a == before);
}

TEST_CASE("facing and idle frame pick the right atlas column", "[unit][avatar]") {
  AvatarAtlas atlas = makeAtlas({{"body/A", 0, 0, 0}, {"legs/0", 0, 0, 0}, {"feet/0", 0, 0, 0}, {"torso/0", 0, 0, 0},
                                 {"arms/0", 0, 0, 0}, {"hands/0", 0, 0, 0}, {"hair/0", 0, 0, 0}});
  // Put a unique marker in each of the 8 columns of the hair row.
  const int hairRow = atlas.meta.rows.at("hair/0");
  for (int c = 0; c < 8; ++c) {
    atlas.image.px[static_cast<std::size_t>((hairRow * 48 + 10) * atlas.image.w + c * 32 + 10)] =
        static_cast<std::uint8_t>(100 + c);
  }
  const domain::Appearance a;
  const auto pal = domain::buildPalette(a, atlas.meta.palette);
  int expected = 0;
  for (const auto facing : {Facing::S, Facing::E, Facing::N, Facing::W}) {
    for (int frame = 0; frame < 2; ++frame, ++expected) {
      CHECK(pixel(compositeAvatar(atlas, a, {}, facing, frame), 10, 10) == pal[static_cast<std::size_t>(100 + expected)]);
    }
  }
}

TEST_CASE("body type B pulls the torso band in by one pixel per side but leaves props alone", "[unit][avatar]") {
  const auto atlas = makeAtlas({{"body/A", 8, 20, 20}, {"legs/0", 0, 0, 0}, {"feet/0", 0, 0, 0}, {"torso/0", 0, 0, 0},
                                {"arms/0", 0, 0, 0}, {"hands/0", 0, 0, 0}, {"hair/0", 9, 20, 28},
                                {"item/quill_sword", 6, 20, 8}});
  domain::Appearance a;
  domain::Appearance b;
  b.bodyType = domain::BodyType::B;
  domain::Loadout sword = domain::defaultLoadout();
  sword[domain::Slot::Weapon] = "quill_sword";

  const auto wide = compositeAvatar(atlas, a, sword, Facing::S, 0);
  const auto slim = compositeAvatar(atlas, b, sword, Facing::S, 0);
  CHECK(wide.at(8, 20)[3] == 255);   // body pixel on the left half...
  CHECK(slim.at(8, 20)[3] == 0);
  CHECK(slim.at(9, 20)[3] == 255);   // ...moved one pixel toward the centre
  CHECK(wide.at(9, 20)[3] == 255);   // hair is a head layer: identical for A and B
  CHECK(slim.at(6, 20)[3] == 255);   // the sword is a held prop: not narrowed
  CHECK(wide.at(6, 20)[3] == 255);
}

TEST_CASE("nearest-neighbour scaling repeats each pixel", "[unit][avatar]") {
  RgbaImage src;
  src.w = 2;
  src.h = 1;
  src.px = {1, 2, 3, 255, 9, 8, 7, 255};
  const auto big = scaleNearest(src, 3);
  CHECK(big.w == 6);
  CHECK(big.h == 3);
  CHECK(big.at(0, 0)[0] == 1);
  CHECK(big.at(2, 2)[0] == 1);
  CHECK(big.at(3, 0)[0] == 9);
  CHECK(big.at(5, 2)[2] == 7);
}

TEST_CASE("the shipped atlas matches the domain: every style, item and the palette", "[unit][avatar][assets]") {
  std::ifstream f(std::string(AS_SOURCE_DIR) + "/assets/sprites/avatar.json");
  REQUIRE(f.good());
  std::stringstream ss;
  ss << f.rdbuf();
  const auto meta = parseAtlasMeta(ss.str());
  REQUIRE(meta.has_value());

  const auto& L = domain::APPEARANCE_LIMITS;
  CHECK(meta->rows.count("body/A") == 1);
  CHECK(meta->rows.count("hands/0") == 1);
  CHECK(meta->rows.count("feet/0") == 1);
  for (int i = 0; i < L.hair; ++i) CHECK(meta->rows.count("hair/" + std::to_string(i)) == 1);
  for (int i = 1; i < L.facialHair; ++i) CHECK(meta->rows.count("facial/" + std::to_string(i)) == 1);
  for (int i = 0; i < L.torso; ++i) CHECK(meta->rows.count("torso/" + std::to_string(i)) == 1);
  for (int i = 0; i < L.arms; ++i) CHECK(meta->rows.count("arms/" + std::to_string(i)) == 1);
  for (int i = 0; i < L.legs; ++i) CHECK(meta->rows.count("legs/" + std::to_string(i)) == 1);

  std::set<std::string> spriteNames;
  for (const auto& item : domain::ITEMS()) {
    if (item.sprite.empty()) {
      CHECK(domain::isDefault(item));  // only the base-clothing defaults may be overlay-free
      continue;
    }
    CHECK(meta->rows.count("item/" + std::string(item.sprite)) == 1);
    spriteNames.insert("item/" + std::string(item.sprite));
  }
  for (const auto& [name, row] : meta->rows) {   // and no orphan item art
    if (name.rfind("item/", 0) == 0) CHECK(spriteNames.count(name) == 1);
  }

  std::set<std::tuple<int, int, int>> seen;      // palette entries are unique (indices are recoverable)
  for (std::size_t i = 1; i < 256; ++i) {
    const auto& c = meta->palette[i];
    CHECK(seen.insert({c.r, c.g, c.b}).second);
  }
}
