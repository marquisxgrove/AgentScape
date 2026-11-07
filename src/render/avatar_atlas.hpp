#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/palette.hpp"

namespace as::render {

// Pure (no SDL) paper-doll atlas model. The atlas PNG is 8-bit indexed, but SDL3_image builds without
// libpng decode it to RGBA and drop the palette, so the indices are recovered from the colours using
// the palette stored in avatar.json (all 256 entries are unique by construction).

struct IndexedImage {
  int w = 0;
  int h = 0;
  std::vector<std::uint8_t> px;  // w*h palette indices; 0 = transparent
};

struct AtlasMeta {
  int cellW = 32;
  int cellH = 48;
  int frames = 2;
  int facings = 4;  // columns are facing-major: column = facing * frames + frame
  domain::Palette256 palette{};
  std::map<std::string, int> rows;  // layer name -> row index
};

struct AvatarAtlas {
  AtlasMeta meta;
  IndexedImage image;
  bool hasLayer(const std::string& name) const { return meta.rows.count(name) != 0; }
};

// Parses avatar.json. Returns nullopt on malformed JSON or a missing/invalid field.
std::optional<AtlasMeta> parseAtlasMeta(std::string_view json);

// Converts decoded RGBA pixels (4 bytes per pixel) back to palette indices. alpha == 0 -> 0. A colour
// that is not in the palette maps to 0 and is counted in *unknown (if given).
IndexedImage indexFromRgba(const std::uint8_t* rgba, int w, int h, const domain::Palette256& palette,
                           int* unknown = nullptr);

struct RgbaImage {
  int w = 0;
  int h = 0;
  std::vector<std::uint8_t> px;  // w*h*4, RGBA
  const std::uint8_t* at(int x, int y) const { return &px[static_cast<std::size_t>((y * w + x) * 4)]; }
};

// Nearest-neighbour integer upscale (used for the 4x PNG export).
RgbaImage scaleNearest(const RgbaImage& src, int factor);

}  // namespace as::render
