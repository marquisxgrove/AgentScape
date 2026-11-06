#pragma once

// Minimal RapidCheck <-> Catch2 v3 bridge (rapidcheck's bundled integration targets Catch2 v2).
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>

#include <string>
#include <utility>

namespace as::testing {

// Runs a RapidCheck property; a failure (with its reproduce string) fails the Catch2 test case.
template <typename Testable>
void checkProperty(const std::string& description, Testable&& testable) {
  INFO("Property: " << description);
  const bool ok = rc::check(description, std::forward<Testable>(testable));
  REQUIRE(ok);
}

}  // namespace as::testing
