#include "ui/costumer_draft.hpp"

namespace as::ui {

namespace {
int* styleField(domain::Appearance& a, StyleOption o) {
  switch (o) {
    case StyleOption::Hair: return &a.hair;
    case StyleOption::FacialHair: return &a.facialHair;
    case StyleOption::Torso: return &a.torso;
    case StyleOption::Arms: return &a.arms;
    case StyleOption::Legs: return &a.legs;
    case StyleOption::BodyType: return nullptr;
  }
  return nullptr;
}

int* colorField(domain::Appearance& a, domain::ColorRegion r) {
  switch (r) {
    case domain::ColorRegion::Hair: return &a.colors.hair;
    case domain::ColorRegion::Torso: return &a.colors.torso;
    case domain::ColorRegion::Legs: return &a.colors.legs;
    case domain::ColorRegion::Feet: return &a.colors.feet;
    case domain::ColorRegion::Skin: return &a.colors.skin;
  }
  return nullptr;
}
}  // namespace

void CostumerDraft::load(const domain::Appearance& saved, const domain::Loadout& savedLoadout,
                         domain::OwnedSet owned) {
  appearance_ = savedAppearance_ = saved;
  loadout_ = savedLoadout_ = savedLoadout;
  owned_ = std::move(owned);
  facing_ = domain::Facing::S;
}

void CostumerDraft::markSaved() {
  savedAppearance_ = appearance_;
  savedLoadout_ = loadout_;
}

int CostumerDraft::count(StyleOption o) const {
  const auto& L = domain::APPEARANCE_LIMITS;
  switch (o) {
    case StyleOption::BodyType: return L.bodyType;
    case StyleOption::Hair: return L.hair;
    case StyleOption::FacialHair: return L.facialHair;
    case StyleOption::Torso: return L.torso;
    case StyleOption::Arms: return L.arms;
    case StyleOption::Legs: return L.legs;
  }
  return 1;
}

int CostumerDraft::value(StyleOption o) const {
  if (o == StyleOption::BodyType) return appearance_.bodyType == domain::BodyType::A ? 0 : 1;
  domain::Appearance copy = appearance_;
  return *styleField(copy, o);
}

void CostumerDraft::cycle(StyleOption o, int dir) {
  if (o == StyleOption::BodyType) {
    appearance_.bodyType = domain::cycle(value(o), count(o), dir) == 0 ? domain::BodyType::A : domain::BodyType::B;
    return;
  }
  int* field = styleField(appearance_, o);
  *field = domain::cycle(*field, count(o), dir);
}

void CostumerDraft::setColor(domain::ColorRegion region, int choice) {
  if (choice < 0 || choice >= domain::colorCount(region)) return;
  *colorField(appearance_, region) = choice;
}

int CostumerDraft::color(domain::ColorRegion region) const {
  domain::Appearance copy = appearance_;
  return *colorField(copy, region);
}

void CostumerDraft::randomise(core::Rng& rng) { appearance_ = domain::randomAppearance(rng); }

void CostumerDraft::rotate(int dir) {
  facing_ = static_cast<domain::Facing>(domain::cycle(static_cast<int>(facing_), 4, dir));
}

std::optional<std::string> CostumerDraft::equip(std::string_view itemId) {
  auto next = domain::equip(loadout_, itemId, owned_);
  if (!next) return std::string("You don't have that item yet.");
  loadout_ = std::move(*next);
  return std::nullopt;
}

void CostumerDraft::unequip(domain::Slot slot) { loadout_ = domain::unequip(loadout_, slot); }

bool CostumerDraft::dirty() const { return !(appearance_ == savedAppearance_) || loadout_ != savedLoadout_; }

}  // namespace as::ui
