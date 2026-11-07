#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/rng.hpp"
#include "domain/appearance.hpp"
#include "domain/loadout.hpp"
#include "domain/palette.hpp"

namespace as::ui {

enum class StyleOption { BodyType, Hair, FacialHair, Torso, Arms, Legs };

// The Costumer's unsaved working copy of an Adventurer (pure; no ImGui). Every edit goes through the same
// domain rules the service re-checks on Save, so the screen can't build something Save would refuse.
class CostumerDraft {
 public:
  // Starts editing a saved Adventurer. `owned` is the account-wide owned set at this moment.
  void load(const domain::Appearance& saved, const domain::Loadout& savedLoadout, domain::OwnedSet owned);
  // Called after a successful claim so newly owned items can be equipped.
  void setOwned(domain::OwnedSet owned) { owned_ = std::move(owned); }
  // Marks the current draft as the saved state (after a successful Save).
  void markSaved();

  const domain::Appearance& appearance() const { return appearance_; }
  const domain::Loadout& loadout() const { return loadout_; }
  const domain::OwnedSet& owned() const { return owned_; }
  domain::Facing facing() const { return facing_; }

  // Arrow buttons: steps through the option's styles, wrapping (Req 2.3).
  void cycle(StyleOption option, int dir);
  int count(StyleOption option) const;
  int value(StyleOption option) const;

  void setColor(domain::ColorRegion region, int choice);  // out-of-range choices are ignored
  int color(domain::ColorRegion region) const;

  void randomise(core::Rng& rng);  // a valid Appearance; the loadout is left alone (Req 2.5)
  void rotate(int dir);            // S, E, N, W (Req 3.2)

  // Equips an item. On refusal the draft is unchanged and the game-voice message is returned
  // ("You don't have that item yet.").
  std::optional<std::string> equip(std::string_view itemId);
  void unequip(domain::Slot slot);

  bool dirty() const;  // differs from the last saved state

 private:
  domain::Appearance appearance_;
  domain::Loadout loadout_;
  domain::Appearance savedAppearance_;
  domain::Loadout savedLoadout_;
  domain::OwnedSet owned_;
  domain::Facing facing_ = domain::Facing::S;
};

}  // namespace as::ui
