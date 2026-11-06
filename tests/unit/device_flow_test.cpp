#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include "domain/device_flow.hpp"

using namespace as::domain;
using namespace std::chrono;

TEST_CASE("device code response parses, with defaults for missing timings", "[unit][device-flow]") {
  const auto dc = parseDeviceCode(R"({
    "device_code": "3584d83530557fdd1f46af8289938c8ef79f9dc5",
    "user_code": "WDJB-MJHT",
    "verification_uri": "https://github.com/login/device",
    "expires_in": 899,
    "interval": 5
  })");
  REQUIRE(dc.has_value());
  CHECK(dc->userCode == "WDJB-MJHT");
  CHECK(dc->verificationUri == "https://github.com/login/device");
  CHECK(dc->interval == seconds{5});
  CHECK(dc->expiresIn == seconds{899});

  // Missing interval/expires_in -> sane defaults; missing required field -> nullopt.
  const auto minimal = parseDeviceCode(R"({"device_code":"d","user_code":"U","verification_uri":"https://x"})");
  REQUIRE(minimal.has_value());
  CHECK(minimal->interval == seconds{5});
  CHECK_FALSE(parseDeviceCode(R"({"user_code":"U","verification_uri":"https://x"})").has_value());
  CHECK_FALSE(parseDeviceCode("not json").has_value());
}

TEST_CASE("token poll maps each GitHub response to the right outcome", "[unit][device-flow]") {
  CHECK(std::holds_alternative<TokenSuccess>(parseTokenPoll(R"({"access_token":"gho_abc","token_type":"bearer"})")));
  CHECK(std::get<TokenSuccess>(parseTokenPoll(R"({"access_token":"gho_abc"})")).accessToken == "gho_abc");
  CHECK(std::holds_alternative<TokenPending>(parseTokenPoll(R"({"error":"authorization_pending"})")));
  CHECK(std::holds_alternative<TokenSlowDown>(parseTokenPoll(R"({"error":"slow_down","interval":10})")));
  CHECK(std::holds_alternative<TokenDenied>(parseTokenPoll(R"({"error":"access_denied"})")));
  CHECK(std::holds_alternative<TokenExpired>(parseTokenPoll(R"({"error":"expired_token"})")));
  CHECK(std::holds_alternative<TokenError>(parseTokenPoll(R"({"error":"invalid_client"})")));
  // Neither token nor error, or malformed -> error, never a crash.
  CHECK(std::holds_alternative<TokenError>(parseTokenPoll(R"({"foo":"bar"})")));
  CHECK(std::holds_alternative<TokenError>(parseTokenPoll("<html>502</html>")));
  CHECK(std::holds_alternative<TokenError>(parseTokenPoll(R"({"access_token":""})")));
}

TEST_CASE("slow_down adds 5s to the poll interval; otherwise it is unchanged", "[unit][device-flow]") {
  CHECK(nextPollInterval(seconds{5}, /*slowDown=*/true) == seconds{10});
  CHECK(nextPollInterval(seconds{10}, true) == seconds{15});
  CHECK(nextPollInterval(seconds{5}, false) == seconds{5});
  CHECK(nextPollInterval(seconds{0}, false) == seconds{1});  // clamped to a sane minimum
}

TEST_CASE("terminal poll results carry the game-voice error text", "[unit][device-flow]") {
  CHECK(deviceFlowMessage(TokenDenied{}) == "GitHub sign-in was cancelled.");
  CHECK(deviceFlowMessage(TokenExpired{}) == "That code expired. Try again.");
  CHECK(deviceFlowMessage(TokenError{"You can't reach GitHub right now."}) == "You can't reach GitHub right now.");
  CHECK(deviceFlowMessage(TokenPending{}).empty());  // not terminal
}

TEST_CASE("profile parses id and login, falling back to login when name is null", "[unit][device-flow]") {
  const auto p = parseProfile(R"({"id":583231,"login":"octocat","name":"The Octocat"})");
  REQUIRE(p.has_value());
  CHECK(p->id == 583231);
  CHECK(p->login == "octocat");
  CHECK(p->displayName == "The Octocat");

  const auto noName = parseProfile(R"({"id":1,"login":"mona","name":null})");
  REQUIRE(noName.has_value());
  CHECK(noName->displayName == "mona");
  CHECK_FALSE(parseProfile(R"({"login":"x"})").has_value());  // missing id
  CHECK_FALSE(parseProfile("nope").has_value());
}
