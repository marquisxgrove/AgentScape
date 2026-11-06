#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "domain/context_menu.hpp"
#include "ui/context_menu_model.hpp"

using namespace as::ui;
using as::domain::MenuAction;

namespace {
ContextMenuModel openMenu() {
  // As buildMenu would produce it: default first, then Examine, then Cancel.
  std::vector<MenuAction> items{
      {"enter", "Enter", "codex", true}, {"examine", "Examine", "A great library.", false}, {"cancel", "Cancel", "", false}};
  ContextMenuModel m;
  m.open(items, 100, 50);
  return m;
}
}  // namespace

TEST_CASE("the menu opens on the default item at the given position", "[unit][context-menu]") {
  auto m = openMenu();
  CHECK(m.isOpen());
  CHECK(m.selected() == 0);
  CHECK(m.x() == 100.0F);
  CHECK(m.y() == 50.0F);
  ContextMenuModel empty;
  empty.open({}, 0, 0);
  CHECK_FALSE(empty.isOpen());
}

TEST_CASE("arrow navigation wraps in both directions", "[unit][context-menu]") {
  auto m = openMenu();
  m.moveUp();
  CHECK(m.selected() == 2);  // wrapped to Cancel
  m.moveDown();
  CHECK(m.selected() == 0);
  m.moveDown();
  m.moveDown();
  CHECK(m.selected() == 2);
  m.moveDown();
  CHECK(m.selected() == 0);  // wrapped forward
}

TEST_CASE("confirming a real action returns it and closes the menu", "[unit][context-menu]") {
  auto m = openMenu();
  m.moveDown();  // Examine
  const auto chosen = m.confirm();
  REQUIRE(chosen.has_value());
  CHECK(chosen->id == "examine");
  CHECK(chosen->target == "A great library.");
  CHECK_FALSE(m.isOpen());
}

TEST_CASE("confirming Cancel closes with no action", "[unit][context-menu]") {
  auto m = openMenu();
  m.moveTo(2);
  CHECK_FALSE(m.confirm().has_value());
  CHECK_FALSE(m.isOpen());
}

TEST_CASE("Escape and click-outside close without side effects", "[unit][context-menu]") {
  auto m = openMenu();
  m.cancel();
  CHECK_FALSE(m.isOpen());
  CHECK_FALSE(m.confirm().has_value());  // confirm on a closed menu is a no-op
}

TEST_CASE("clicking a row confirms that row directly", "[unit][context-menu]") {
  auto m = openMenu();
  const auto chosen = m.confirmIndex(0);
  REQUIRE(chosen.has_value());
  CHECK(chosen->id == "enter");
  CHECK_FALSE(m.isOpen());
}
