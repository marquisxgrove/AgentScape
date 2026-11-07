#include <catch2/catch_test_macros.hpp>

#include <string>

#include "domain/codex_validation.hpp"

using namespace as::domain;

namespace {
AgentInput goodInput() {
  AgentInput in;
  in.name = "Example Model";
  in.vendor = "Example Labs";
  in.kind = AgentKind::Model;
  in.examine = "A model that exists only for tests.";
  in.about = "Line one.\nLine two.";
  in.website = "https://example.com/model";
  return in;
}
}  // namespace

TEST_CASE("utf8Length counts code points and rejects malformed text", "[unit][codex-validation]") {
  CHECK(utf8Length("") == 0);
  CHECK(utf8Length("hello") == 5);
  CHECK(utf8Length("h\xC3\xA9llo") == 5);              // é is two bytes but one character
  CHECK(utf8Length("\xE2\x82\xAC") == 1);               // €
  CHECK(utf8Length("\xF0\x9F\x8E\x83") == 1);           // a pumpkin emoji, four bytes
  CHECK_FALSE(utf8Length("\xC3").has_value());          // truncated
  CHECK_FALSE(utf8Length("\xC0\xAF").has_value());      // overlong '/'
  CHECK_FALSE(utf8Length("\xED\xA0\x80").has_value());  // a surrogate half
  CHECK_FALSE(utf8Length("\xF4\x90\x80\x80").has_value());  // beyond U+10FFFF
  CHECK_FALSE(utf8Length("ab\x80").has_value());        // stray continuation byte
  CHECK_FALSE(utf8Length("\xFF").has_value());
}

TEST_CASE("a well-formed Agent proposal is accepted and cleaned", "[unit][codex-validation]") {
  auto in = goodInput();
  in.name = "  Example Model  ";
  in.website = "  https://example.com/model  ";
  const auto out = validateAgentInput(in);
  REQUIRE(out.has_value());
  CHECK(out->name == "Example Model");
  CHECK(out->website == "https://example.com/model");
  CHECK(out->about == "Line one.\nLine two.");   // about may span lines
}

TEST_CASE("Agent fields enforce their limits and name the failing field", "[unit][codex-validation]") {
  const auto fieldOf = [](AgentInput in) { return validateAgentInput(std::move(in)).error().field; };
  auto in = goodInput();
  in.name = "";
  CHECK(fieldOf(in) == "name");
  in = goodInput();
  in.name = std::string(41, 'x');
  CHECK(fieldOf(in) == "name");
  in = goodInput();
  in.name = std::string(40, 'x');
  CHECK(validateAgentInput(in).has_value());
  in = goodInput();
  in.vendor = "   ";
  CHECK(fieldOf(in) == "vendor");
  in = goodInput();
  in.examine = std::string(201, 'e');
  CHECK(fieldOf(in) == "examine");
  in.examine = std::string(200, 'e');
  CHECK(validateAgentInput(in).has_value());
  in = goodInput();
  in.examine = "two\nlines";
  CHECK(fieldOf(in) == "examine");               // examine text is a single line
  in = goodInput();
  in.about = std::string(1001, 'a');
  CHECK(fieldOf(in) == "about");
  in = goodInput();
  in.name = "bad\x01name";
  CHECK(fieldOf(in) == "name");
  in = goodInput();
  in.name = "caf\xC3";                           // malformed UTF-8
  CHECK(fieldOf(in) == "name");
}

TEST_CASE("only https websites with a real host are accepted", "[unit][codex-validation]") {
  const auto ok = [](const char* u) { return validateWebsite(u).has_value(); };
  CHECK(ok("https://example.com"));
  CHECK(ok("https://www.example.co.uk/path?q=1#frag"));
  CHECK(ok("https://example.com:8443/x"));
  CHECK_FALSE(ok("http://example.com"));              // not https
  CHECK_FALSE(ok("ftp://example.com"));
  CHECK_FALSE(ok("javascript:alert(1)"));
  CHECK_FALSE(ok("//example.com"));
  CHECK_FALSE(ok("https://"));
  CHECK_FALSE(ok("https://localhost"));                // needs a dotted host
  CHECK_FALSE(ok("https://example..com"));
  CHECK_FALSE(ok("https://.example.com"));
  CHECK_FALSE(ok("https://example.com ./x"));          // whitespace
  CHECK_FALSE(ok("https://exa mple.com"));
  CHECK_FALSE(ok("https://example.com:abc/"));
  CHECK_FALSE(ok("https://example.com:/"));
  CHECK_FALSE(ok("https://user:pw@example.com"));      // a login would disguise the real host
  CHECK_FALSE(ok("https://good.com@evil.com/"));
  CHECK_FALSE(ok("https://exämple.com"));              // non-ASCII hosts are refused outright
  CHECK_FALSE(ok(("https://example.com/" + std::string(200, 'a')).c_str()));   // too long
  // An empty optional website is fine for a proposal.
  auto in = goodInput();
  in.website = "   ";
  const auto out = validateAgentInput(in);
  REQUIRE(out.has_value());
  CHECK_FALSE(out->website.has_value());
}

TEST_CASE("the host shown in the leave-the-game prompt is the real, lower-cased one", "[unit][codex-validation]") {
  CHECK(websiteHost("https://Example.COM/path") == "example.com");
  CHECK(websiteHost("https://www.example.com:8443/x?y#z") == "www.example.com");
  CHECK(websiteHost("https://example.com?x=1") == "example.com");
  CHECK_FALSE(websiteHost("http://example.com").has_value());
  CHECK_FALSE(websiteHost("https://good.com@evil.com").has_value());   // never offer a spoofable host
}

TEST_CASE("Boon and Bane text must be 10 to 280 characters of plain text", "[unit][codex-validation]") {
  CHECK_FALSE(validateNoteText("too short").has_value());                      // 9
  CHECK(validateNoteText("just right").has_value());                           // 10
  CHECK(validateNoteText("  padded text here  ").value() == "padded text here");
  CHECK_FALSE(validateNoteText("          too short          ").has_value());   // padding does not count toward 10
  CHECK(validateNoteText("          ten spaces          ").has_value());      // trims to exactly 10
  CHECK(validateNoteText(std::string(280, 'x')).has_value());
  CHECK_FALSE(validateNoteText(std::string(281, 'x')).has_value());
  // Length is in characters, not bytes: 280 two-byte characters are fine.
  std::string accents;
  for (int i = 0; i < 280; ++i) accents += "\xC3\xA9";
  CHECK(validateNoteText(accents).has_value());
  CHECK_FALSE(validateNoteText(accents + "\xC3\xA9").has_value());
  CHECK_FALSE(validateNoteText("one line\nanother line").has_value());          // a single line
  CHECK_FALSE(validateNoteText("tab\there is bad text").has_value());
  CHECK_FALSE(validateNoteText("bad \xC3 utf-8 here").has_value());
  // Markup is just text here: it is rendered as plain text later, so it is allowed and not interpreted.
  CHECK(validateNoteText("<b>bold</b> & <script>x</script>").has_value());
  const auto err = validateNoteText("short").error();
  CHECK(err.field == "text");
  CHECK(err.message == "That is too short.");
}
