#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "app/screen_stack.hpp"
#include "core/rng.hpp"
#include "services/adventurer_service.hpp"
#include "ui/costumer_draft.hpp"

namespace as::screens {

// The Costumer (character-creator Requirements 1-8): create Adventurers, design the look with arrows and
// swatches, dress the paper-doll from the wardrobe, claim Kiroween gear, and switch / delete Adventurers.
// All rules live in the domain, ui::CostumerDraft and AdventurerService; this class only draws and routes.
class CostumerScreen : public app::Screen {
 public:
  CostumerScreen();
  app::ScreenId id() const override { return app::ScreenId::Costumer; }
  void onEnter(const app::ScreenParams& params) override;
  void draw(app::FrameCtx& ctx) override;
  void onContextAction(app::FrameCtx& ctx, const domain::MenuAction& action) override;

 private:
  enum class Tab { Look, Gear, Adventurers };

  void reload(app::FrameCtx& ctx);
  void drawCreate(app::FrameCtx& ctx);
  void drawPreview(app::FrameCtx& ctx);
  void drawLook();
  void drawGear(app::FrameCtx& ctx);
  void drawAdventurers(app::FrameCtx& ctx);
  void save(app::FrameCtx& ctx);
  void create(app::FrameCtx& ctx);
  void claim(app::FrameCtx& ctx, const domain::Item& item);

  bool loaded_ = false;
  Tab tab_ = Tab::Look;
  std::optional<services::Adventurer> active_;
  std::vector<services::Adventurer> list_;
  ui::CostumerDraft draft_;
  core::SystemRng rng_;
  double clock_ = 0;

  bool describe_ = false;
  std::optional<domain::Slot> slotFilter_;
  bool ownedOnly_ = false;

  std::array<char, 16> nameBuf_{};
  std::string pendingDeleteId_;
  std::array<char, 16> confirmBuf_{};
};

}  // namespace as::screens
