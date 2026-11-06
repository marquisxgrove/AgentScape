#include <catch2/catch_test_macros.hpp>

#include "core/result.hpp"

TEST_CASE("Result carries a value or a game-voice error", "[unit][core]") {
  as::Result<int> ok = 7;
  CHECK(ok.value() == 7);

  as::Result<int> bad = as::makeError(as::GameErrorCode::StorageFailed, "Something went wrong. Nothing was saved.");
  REQUIRE_FALSE(bad.has_value());
  CHECK(bad.error().code == as::GameErrorCode::StorageFailed);
  CHECK(bad.error().message == "Something went wrong. Nothing was saved.");
}
