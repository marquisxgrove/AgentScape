#include "render/avatar_atlas.hpp"

#include <nlohmann/json.hpp>

#include <unordered_map>

namespace as::render {

namespace {
std::optional<domain::Rgba> parseHexColour(const std::string& s) {
  if (s.size() != 7 || s[0] != '#') return std::nullopt;
  const auto hex = [&](std::size_t i) -> int {
    int v = 0;
    for (std::size_t k = i; k < i + 2; ++k) {
      const char c = s[k];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= c - '0';
      else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
      else return -1;
    }
    return v;
  };
  const int r = hex(1);
  const int g = hex(3);
  const int b = hex(5);
  if (r < 0 || g < 0 || b < 0) return std::nullopt;
  return domain::Rgba{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 255};
}

std::uint32_t pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  return (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | b;
}
}  // namespace

std::optional<AtlasMeta> parseAtlasMeta(std::string_view jsonText) {
  const auto j = nlohmann::json::parse(jsonText.begin(), jsonText.end(), nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return std::nullopt;

  AtlasMeta meta;
  const auto cell = j.find("cell");
  if (cell == j.end() || !cell->is_object() || !cell->contains("w") || !cell->contains("h")) return std::nullopt;
  if (!(*cell)["w"].is_number_integer() || !(*cell)["h"].is_number_integer()) return std::nullopt;
  meta.cellW = (*cell)["w"].get<int>();
  meta.cellH = (*cell)["h"].get<int>();
  if (meta.cellW <= 0 || meta.cellH <= 0) return std::nullopt;

  const auto frames = j.find("frames");
  const auto facings = j.find("facings");
  if (frames == j.end() || !frames->is_number_integer() || facings == j.end() || !facings->is_array()) {
    return std::nullopt;
  }
  meta.frames = frames->get<int>();
  meta.facings = static_cast<int>(facings->size());
  if (meta.frames < 1 || meta.facings != 4) return std::nullopt;

  const auto pal = j.find("palette");
  if (pal == j.end() || !pal->is_array() || pal->size() != 256) return std::nullopt;
  for (std::size_t i = 0; i < 256; ++i) {
    if (!(*pal)[i].is_string()) return std::nullopt;
    const auto c = parseHexColour((*pal)[i].get<std::string>());
    if (!c) return std::nullopt;
    meta.palette[i] = *c;
  }
  meta.palette[0].a = 0;  // index 0 is the transparent colour key

  const auto rows = j.find("rows");
  if (rows == j.end() || !rows->is_object()) return std::nullopt;
  for (const auto& [name, value] : rows->items()) {
    if (!value.is_number_integer() || value.get<int>() < 0) return std::nullopt;
    meta.rows[name] = value.get<int>();
  }
  return meta;
}

IndexedImage indexFromRgba(const std::uint8_t* rgba, int w, int h, const domain::Palette256& palette, int* unknown) {
  std::unordered_map<std::uint32_t, std::uint8_t> lookup;
  for (int i = 1; i < 256; ++i) {  // index 0 is matched by alpha, never by colour
    const auto& c = palette[static_cast<std::size_t>(i)];
    lookup.emplace(pack(c.r, c.g, c.b), static_cast<std::uint8_t>(i));
  }
  IndexedImage out;
  out.w = w;
  out.h = h;
  out.px.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
  for (std::size_t i = 0; i < out.px.size(); ++i) {
    const std::uint8_t* p = rgba + i * 4;
    if (p[3] == 0) continue;
    const auto it = lookup.find(pack(p[0], p[1], p[2]));
    if (it != lookup.end()) {
      out.px[i] = it->second;
    } else if (unknown != nullptr) {
      ++*unknown;
    }
  }
  return out;
}

RgbaImage scaleNearest(const RgbaImage& src, int factor) {
  RgbaImage out;
  if (factor < 1) factor = 1;
  out.w = src.w * factor;
  out.h = src.h * factor;
  out.px.resize(static_cast<std::size_t>(out.w) * static_cast<std::size_t>(out.h) * 4);
  for (int y = 0; y < out.h; ++y) {
    for (int x = 0; x < out.w; ++x) {
      const std::uint8_t* s = src.at(x / factor, y / factor);
      std::uint8_t* d = &out.px[static_cast<std::size_t>((y * out.w + x) * 4)];
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
      d[3] = s[3];
    }
  }
  return out;
}

}  // namespace as::render
