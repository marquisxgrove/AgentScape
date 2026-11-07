#include "domain/items.hpp"

#include <algorithm>
#include <array>

namespace as::domain {

namespace {
// hides-layer arrays referenced by span from the catalog below (must outlive ITEMS()).
constexpr std::array<Layer, 2> HIDE_HAIR_FACIAL{Layer::Hair, Layer::FacialHair};
constexpr std::array<Layer, 1> HIDE_FACIAL{Layer::FacialHair};
constexpr std::array<Layer, 3> HIDE_ARMS_HANDS_LEGS{Layer::Arms, Layer::Hands, Layer::Legs};

const std::array<Item, 23>& catalog() {
  static const std::array<Item, 23> ITEMS_TABLE = {{
      // HEAD
      {"apprentice_hood", "Apprentice Hood", Slot::Head, "A hood for those still learning the words of power.",
       "apprentice_hood", UnlockLevel{5}},
      {"wizard_hat", "Wizard Hat", Slot::Head, "Pointed, and faintly humming with potential.", "wizard_hat",
       UnlockLevel{20}},
      {"witch_hat", "Witch's Hat", Slot::Head, "Wide-brimmed and best worn on a broom.", "witch_hat",
       UnlockKiroween{}},
      {"jack_mask", "Jack-o'-Lantern Mask", Slot::Head, "A grinning gourd. You can't see your own hair in it.",
       "jack_mask", UnlockKiroween{}, false, HIDE_HAIR_FACIAL},
      {"skull_mask", "Skull Mask", Slot::Head, "Bone-white and missing a jaw's worth of manners.", "skull_mask",
       UnlockKiroween{}, false, HIDE_FACIAL},
      // CAPE
      {"travellers_cape", "Traveller's Cape", Slot::Cape, "Road-worn wool that has seen a hundred inns.",
       "travellers_cape", UnlockLevel{10}},
      {"keepers_cape", "Codex Keeper's Cape", Slot::Cape, "Only worn by those trusted to guard the Codex.",
       "keepers_cape", UnlockLevel{40}},
      {"vampire_cape", "Vampire Cape", Slot::Cape, "High-collared and thirsty for drama.", "vampire_cape",
       UnlockKiroween{}},
      // NECK
      {"token_amulet", "Token Amulet", Slot::Neck, "A pendant that counts the words you've spent.", "token_amulet",
       UnlockLevel{15}},
      // WEAPON
      {"wooden_stick", "Wooden Stick", Slot::Weapon, "A humble beginning. Pointy enough.", "wooden_stick",
       UnlockDefault{}},
      {"quill_sword", "Quill Sword", Slot::Weapon, "The pen, sharpened into a blade.", "quill_sword", UnlockLevel{8}},
      {"prompt_staff", "Prompt Staff", Slot::Weapon, "Two hands, one perfectly phrased request.", "prompt_staff",
       UnlockLevel{25}, true},
      {"reapers_scythe", "Reaper's Scythe", Slot::Weapon, "It harvests only hallucinations. Mostly.", "reapers_scythe",
       UnlockKiroween{13}, true},
      // BODY
      {"plain_tunic", "Plain Tunic", Slot::Body, "Unassuming cloth for an unassuming start.", "",  // no overlay: this IS the base layer
       UnlockDefault{}},
      {"scholar_robe", "Scholar Robe", Slot::Body, "Flowing robes that rustle with footnotes.", "scholar_robe",
       UnlockLevel{12}},
      {"ghostly_sheet", "Ghostly Sheet", Slot::Body, "Two eyeholes and infinite commitment to the bit.",
       "ghostly_sheet", UnlockKiroween{}, false, HIDE_ARMS_HANDS_LEGS},
      // SHIELD
      {"codex_tome", "Codex Tome", Slot::Shield, "A book so heavy it stops blades.", "codex_tome", UnlockLevel{18}},
      {"candy_pail", "Candy Pail", Slot::Shield, "Trick-or-treat defence. Surprisingly sturdy.", "candy_pail",
       UnlockKiroween{}},
      // LEGS
      {"plain_trousers", "Plain Trousers", Slot::Legs, "Sensible and seldom remarked upon.", "",  // no overlay: this IS the base layer
       UnlockDefault{}},
      {"scholar_skirt", "Scholar Skirt", Slot::Legs, "Pleated for pacing the library aisles.", "scholar_skirt",
       UnlockLevel{12}},
      // HANDS
      {"leather_gloves", "Leather Gloves", Slot::Hands, "Keeps ink and frostbite off your knuckles.", "leather_gloves",
       UnlockLevel{3}},
      // FEET
      {"leather_boots", "Leather Boots", Slot::Feet, "Broken in, and good for a long quest.", "",  // no overlay: this IS the base layer
       UnlockDefault{}},
      {"cobweb_boots", "Cobweb Boots", Slot::Feet, "Trailing silk from some very large spider.", "cobweb_boots",
       UnlockKiroween{}},
  }};
  return ITEMS_TABLE;
}
}  // namespace

std::span<const Item> ITEMS() { return catalog(); }

const Item* findItem(std::string_view id) {
  const auto& items = catalog();
  const auto it = std::find_if(items.begin(), items.end(), [&](const Item& i) { return i.id == id; });
  return it == items.end() ? nullptr : &*it;
}

bool isDefault(const Item& item) { return std::holds_alternative<UnlockDefault>(item.unlock); }
bool isKiroween(const Item& item) { return std::holds_alternative<UnlockKiroween>(item.unlock); }

const char* slotName(Slot slot) {
  switch (slot) {
    case Slot::Head: return "Head";
    case Slot::Cape: return "Cape";
    case Slot::Neck: return "Neck";
    case Slot::Weapon: return "Weapon";
    case Slot::Body: return "Body";
    case Slot::Shield: return "Shield";
    case Slot::Legs: return "Legs";
    case Slot::Hands: return "Hands";
    case Slot::Feet: return "Feet";
  }
  return "?";
}

}  // namespace as::domain
