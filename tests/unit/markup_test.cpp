#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ui/markup.hpp"

using namespace as::ui;
using P = PaletteColor;

TEST_CASE("plain text is a single default-coloured span", "[unit][markup]") {
  CHECK(parseMarkup("Welcome to AgentScape.") == std::vector<TextSpan>{{"Welcome to AgentScape.", P::TextYellow}});
  CHECK(parseMarkup("").empty());
  CHECK(parseMarkup("hi", P::TextRed) == std::vector<TextSpan>{{"hi", P::TextRed}});
}

TEST_CASE("col tags colour a run and </col> returns to the default", "[unit][markup]") {
  const auto spans = parseMarkup("Enter <col=cyan>Grand Codex</col> / 3 more options");
  CHECK(spans == std::vector<TextSpan>{
                     {"Enter ", P::TextYellow}, {"Grand Codex", P::TextCyan}, {" / 3 more options", P::TextYellow}});
}

TEST_CASE("a new col tag replaces the current colour (no nesting)", "[unit][markup]") {
  const auto spans = parseMarkup("<col=red>a<col=green>b</col>c");
  CHECK(spans == std::vector<TextSpan>{{"a", P::TextRed}, {"b", P::TextGreen}, {"c", P::TextYellow}});
}

TEST_CASE("only palette colour names are accepted; anything else stays literal", "[unit][markup]") {
  CHECK(plainText(parseMarkup("<col=ff00ff>x</col>")) == "<col=ff00ff>x");
  CHECK(plainText(parseMarkup("<col=stone>x")) == "<col=stone>x");  // stone isn't a text colour
  CHECK(plainText(parseMarkup("<b>bold</b>")) == "<b>bold</b>");
  CHECK(plainText(parseMarkup("a < b and <col=cyan")) == "a < b and <col=cyan");
  CHECK(parseMarkup("<col=ff00ff>x").size() == 1);
}

TEST_CASE("adjacent runs of the same colour merge and empty runs vanish", "[unit][markup]") {
  CHECK(parseMarkup("<col=cyan></col>a<col=yellow>b</col>") == std::vector<TextSpan>{{"ab", P::TextYellow}});
}

TEST_CASE("escaped user text renders exactly, even if it looks like markup", "[unit][markup]") {
  const std::vector<std::string> nasty{
      "Evil <col=red>Agent</col>", "<lt>", "<<<", "</col>", "<col=cyan", "x<col=green>y", "100% < 200%", "",
  };
  for (const auto& s : nasty) {
    const auto spans = parseMarkup(escapeMarkup(s));
    CHECK(plainText(spans) == s);
    for (const auto& span : spans) CHECK(span.color == P::TextYellow);
  }
}

TEST_CASE("hover text uses the classic action / target / more-options shape", "[unit][markup]") {
  CHECK(hoverText("Enter", "Grand Codex", P::TextCyan, 3) == "Enter <col=cyan>Grand Codex</col> / 3 more options");
  CHECK(hoverText("Examine", "Tavern", P::TextCyan, 1) == "Examine <col=cyan>Tavern</col> / 1 more option");
  CHECK(hoverText("Walk here", "", P::TextCyan, 0) == "Walk here");
  // Names are escaped, so a malicious Agent name can't recolour the line.
  const auto spans = parseMarkup(hoverText("Examine", "<col=red>X", P::TextCyan, 0));
  CHECK(spans == std::vector<TextSpan>{{"Examine ", P::TextYellow}, {"<col=red>X", P::TextCyan}});
}

TEST_CASE("text on parchment switches light colours to dark ones", "[unit][palette]") {
  CHECK(textColorOn(Surface::Parchment, P::TextYellow) == P::Ink);
  CHECK(textColorOn(Surface::Parchment, P::TextOrange) == P::StoneDark);
  CHECK(textColorOn(Surface::Parchment, P::TextCyan) == P::Ink);
  CHECK(textColorOn(Surface::Parchment, P::ChatBlue) == P::ChatBlue);
  for (const auto c : {P::TextYellow, P::TextOrange, P::TextRed, P::KiroweenPurple}) {
    CHECK(textColorOn(Surface::Stone, c) == c);
  }
  CHECK(shadowOn(Surface::Stone));
  CHECK_FALSE(shadowOn(Surface::Parchment));
}
