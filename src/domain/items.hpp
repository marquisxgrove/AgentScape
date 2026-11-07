#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace as::domain {

enum class Slot { Head, Cape, Neck, Weapon, Body, Shield, Legs, Hands, Feet };

// Avatar sprite layers that an item can hide (e.g. a full mask hides Hair + FacialHair).
enum class Layer { Hair, FacialHair, Arms, Hands, Legs, Feet, Torso };

struct UnlockDefault {};              // owned from the start
struct UnlockLevel { int level; };    // unlocked at a Questing level
struct UnlockKiroween { std::optional<int> minLevel; };  // claimable during Kiroween (optional level gate)
using UnlockRule = std::variant<UnlockDefault, UnlockLevel, UnlockKiroween>;

struct Item {
  std::string_view id;
  std::string_view name;
  Slot slot;
  std::string_view examine;
  std::string_view sprite;
  UnlockRule unlock;
  bool twoHanded = false;
  std::span<const Layer> hides = {};
};

std::span<const Item> ITEMS();
const Item* findItem(std::string_view id);

// Convenience predicates on unlock rules.
bool isDefault(const Item& item);
bool isKiroween(const Item& item);

const char* slotName(Slot slot);

}  // namespace as::domain
