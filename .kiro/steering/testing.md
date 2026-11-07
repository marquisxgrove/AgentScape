---
inclusion: fileMatch
fileMatchPattern: ["tests/**/*", "src/domain/**/*", "**/*_test.cpp", "**/*_props.cpp"]
---

# Testing Strategy

## Pyramid

1. **Property-based tests (RapidCheck)** for every public function in `src/domain/`. These are the primary safety net.
2. **Unit and integration tests (Catch2 v3).**
   - Unit tests cover `src/core`, UI state models (context menu navigation, chat filtering, the hover-text markup parser), and platform parsers (device-flow responses).
   - Integration tests run services and repositories against a fresh in-memory SQLite database with all migrations applied.
3. **Headless smoke tests** for the main paths only. They run the real app with `SDL_VIDEO_DRIVER=dummy` and the software renderer, take one scripted step per frame, and save a PNG snapshot of each screen as a CI artifact for visual review. The steps navigate screens and call services directly; they do not simulate mouse or keyboard input. The runs live in `src/app/smoke_runs.cpp` and are registered in `CMakeLists.txt` (label `smoke`, each driven by a `cmake/RunSmoke*.cmake` script):
   - `smoke_demo_path` (`--smoke-path`): guest login → create Adventurer → equip Kiroween gear → open an Agent in the Codex → write a Quest Log → see the level-up → Hiscores; then a second process on the same database checks that everything survived a restart.
   - `smoke_codex` (`--smoke-codex`): search, examine card, Boons and votes, Proving Grounds, propose and approve, Graveyard, a Quest Log, Helpful votes, Hiscores.
   - `smoke_costumer` (`--smoke-costumer`): create, claim, equip and save, then verify after a restart.
   - `smoke_demo` (`--smoke-demo`): the shell path, guest login → World → the Tavern context menu.

## Property-based testing rules

- Each spec's `design.md` lists **Correctness Properties** (numbered P1, P2, …). Every property gets exactly one test named `Property <n>: <title>`, with a comment linking to the requirement it validates:

  ```cpp
  // Validates: agent-codex Requirements 3.2, 3.3
  TEST_CASE("Property 1: Level bounds and ranking threshold", "[property][scoring]") {
    rc::prop("level is null below 3 ratings, else in [1, 99]", [](const Ratings& r) { /* … */ });
  }
  ```

- Put shared generators in `tests/generators/` (for example, `genAppearance`, `genLoadout`, `genRatings`, `genEquipOps`, `genVoteOps`, and `rc::Arbitrary<>` specialisations for domain types). Reuse them; don't redefine them inline.
- CTest sets `RC_PARAMS=max_success=200` for the `property` label. Scoring and aggregation math (label `scoring`) uses `max_success=1000`.
- Model-based tests (`rc::state`) are preferred for stateful rules: equip sequences, vote sequences, and incremental aggregates compared with a full recompute.
- When a property fails, RapidCheck prints a `reproduce` string. Add the shrunk counterexample as a regular Catch2 test in `tests/unit/regressions_<spec>.cpp`, then fix the code.
- Never weaken a property to make it pass without updating the requirement in the spec.

## Conventions

- Tests must be deterministic. Inject `now` and `core::Rng`, and don't use real clocks, sleeps, the network, or the user's data directory.
- DB tests open `:memory:` per test case and apply every migration through the real migration runner, so migrations are tested on every run.
- The `asan` preset runs the full suite under AddressSanitizer and UBSan in CI. A sanitizer report fails the build.
- Test names describe behavior in domain terms, e.g., `"equipping a two-handed weapon removes the shield"`.
