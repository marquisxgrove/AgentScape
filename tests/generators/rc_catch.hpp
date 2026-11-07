#pragma once

// Minimal RapidCheck <-> Catch2 v3 bridge (rapidcheck's bundled integration targets Catch2 v2).
#include <catch2/catch_test_macros.hpp>
#include <rapidcheck.h>

#include <iostream>
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

// Same, with an explicit number of successful runs (the spec asks for 1000 on the scoring math).
template <typename Testable>
void checkPropertyRuns(const std::string& description, int runs, Testable&& testable) {
  INFO("Property: " << description << " (" << runs << " runs)");
  using namespace rc::detail;
  rc::detail::configuration();  // load the environment configuration first, as rc::check does
  std::cerr << std::endl << "- " << description << std::endl;
  TestParams params = configuration().testParams;
  params.maxSuccess = runs;
  TestMetadata metadata;
  metadata.id = description;
  metadata.description = description;
  const auto result = checkTestable(std::forward<Testable>(testable), metadata, params);
  printResultMessage(result, std::cerr);
  std::cerr << std::endl;
  REQUIRE(result.template is<SuccessResult>());
}

}  // namespace as::testing
