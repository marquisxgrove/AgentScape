#include "render/avatar_compositor.hpp"

namespace as::render {

namespace {
using domain::LayerKey;

constexpr int BAND_TOP = 16;     // torso band (inclusive rows) that body type B slims
constexpr int BAND_BOTTOM = 30;
constexpr int CENTRE_LEFT = 15;  // the two centre columns that are dropped
constexpr int CENTRE_RIGHT = 16;

std::optional<domain::Slot> slotFor(LayerKey key) {
  switch (key) {
    case LayerKey::CapeItem: return domain::Slot::Cape;
    case LayerKey::LegsItem: return domain::Slot::Legs;
    case LayerKey::FeetItem: return domain::Slot::Feet;
    case LayerKey::BodyItem: return domain::Slot::Body;
    case LayerKey::HandsItem: return domain::Slot::Hands;
    case LayerKey::NeckItem: return domain::Slot::Neck;
    case LayerKey::HeadItem: return domain::Slot::Head;
    case LayerKey::ShieldItem: return domain::Slot::Shield;
    case LayerKey::WeaponItem: return domain::Slot::Weapon;
    default: return std::nullopt;
  }
}

void narrowCell(std::vector<std::uint8_t>& cell, int w, int h) {
  for (int y = BAND_TOP; y <= BAND_BOTTOM && y < h; ++y) {
    std::uint8_t* row = &cell[static_cast<std::size_t>(y * w)];
    for (int x = CENTRE_LEFT - 1; x >= 0; --x) row[x + 1] = row[x];    // left half moves right
    row[0] = 0;
    for (int x = CENTRE_RIGHT + 1; x < w; ++x) row[x - 1] = row[x];    // right half moves left
    row[w - 1] = 0;
  }
}
}  // namespace

std::optional<std::string> layerRowName(LayerKey key, const domain::Appearance& a, const domain::Loadout& loadout) {
  switch (key) {
    case LayerKey::BaseBody: return "body/A";
    case LayerKey::Legs: return "legs/" + std::to_string(a.legs);
    case LayerKey::Feet: return "feet/0";
    case LayerKey::Torso: return "torso/" + std::to_string(a.torso);
    case LayerKey::Arms: return "arms/" + std::to_string(a.arms);
    case LayerKey::Hands: return "hands/0";
    case LayerKey::Hair: return "hair/" + std::to_string(a.hair);
    case LayerKey::FacialHair:
      if (a.facialHair == 0) return std::nullopt;
      return "facial/" + std::to_string(a.facialHair);
    default: break;
  }
  const auto slot = slotFor(key);
  if (!slot) return std::nullopt;
  const auto it = loadout.find(*slot);
  if (it == loadout.end()) return std::nullopt;
  const domain::Item* item = domain::findItem(it->second);
  if (item == nullptr || item->sprite.empty()) return std::nullopt;
  return "item/" + std::string(item->sprite);
}

bool narrowedForBodyB(LayerKey key) {
  switch (key) {
    case LayerKey::BaseBody:
    case LayerKey::Torso:
    case LayerKey::Arms:
    case LayerKey::Hands:
    case LayerKey::Legs:
    case LayerKey::LegsItem:
    case LayerKey::BodyItem:
    case LayerKey::HandsItem: return true;
    default: return false;
  }
}

RgbaImage compositeAvatar(const AvatarAtlas& atlas, const domain::Appearance& appearance,
                          const domain::Loadout& loadout, domain::Facing facing, int frame) {
  const int cw = atlas.meta.cellW;
  const int ch = atlas.meta.cellH;
  RgbaImage out;
  out.w = cw;
  out.h = ch;
  out.px.assign(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch) * 4, 0);

  const int col = static_cast<int>(facing) * atlas.meta.frames + (frame % atlas.meta.frames);
  const domain::Palette256 palette = domain::buildPalette(appearance, atlas.meta.palette);
  std::vector<std::uint8_t> cell(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch));

  for (const LayerKey key : domain::drawOrder(appearance, loadout, facing)) {
    const auto name = layerRowName(key, appearance, loadout);
    if (!name) continue;
    const auto row = atlas.meta.rows.find(*name);
    if (row == atlas.meta.rows.end()) continue;  // unknown sprite: skip rather than fail the frame

    for (int y = 0; y < ch; ++y) {
      const std::size_t src = static_cast<std::size_t>((row->second * ch + y) * atlas.image.w + col * cw);
      for (int x = 0; x < cw; ++x) cell[static_cast<std::size_t>(y * cw + x)] = atlas.image.px[src + static_cast<std::size_t>(x)];
    }
    if (appearance.bodyType == domain::BodyType::B && narrowedForBodyB(key)) narrowCell(cell, cw, ch);

    for (std::size_t i = 0; i < cell.size(); ++i) {
      if (cell[i] == 0) continue;
      const domain::Rgba& c = palette[cell[i]];
      std::uint8_t* d = &out.px[i * 4];
      d[0] = c.r;
      d[1] = c.g;
      d[2] = c.b;
      d[3] = 255;
    }
  }
  return out;
}

}  // namespace as::render
