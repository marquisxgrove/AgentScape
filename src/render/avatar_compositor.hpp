#pragma once

#include <optional>
#include <string>

#include "domain/appearance.hpp"
#include "domain/loadout.hpp"
#include "render/avatar_atlas.hpp"

namespace as::render {

// Atlas row name for a drawable layer, or nullopt when the layer has no sprite (an item whose sprite
// is empty because it is the base layer, e.g. Plain Tunic).
std::optional<std::string> layerRowName(domain::LayerKey key, const domain::Appearance& appearance,
                                        const domain::Loadout& loadout);

// Body type B is a slimmer build: layers that wrap the torso lose their two centre columns across the
// torso band, pulling both sides in by one pixel. Cape, weapon, shield and head layers are untouched.
bool narrowedForBodyB(domain::LayerKey key);

// Composites one 32x48 frame: the layers of domain::drawOrder, each recoloured by the palette that
// domain::buildPalette derives from the Appearance. `frame` is the idle frame (0 or 1).
RgbaImage compositeAvatar(const AvatarAtlas& atlas, const domain::Appearance& appearance,
                          const domain::Loadout& loadout, domain::Facing facing, int frame);

}  // namespace as::render
