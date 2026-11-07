# Implementation Plan: Character Creator and Costumer

> Depends on: `game-shell` (tasks 1–8)

- [x] 1. Implement appearance and name domain
  - [x] 1.1 Implement `appearance` (types, limits, `cycle`, `randomAppearance`, `validateAppearance`, JSON codec) and `adventurer_name` (`validateName`, `nameKey`, blocklist).
    - _Requirements: 1.1, 1.2, 2.1–2.5_
  - [x]* 1.2 Write property test for cycling (Property 7).
    - **Property 7: Cycling wraps**
    - **Validates: Requirement 2.3**
  - [x]* 1.3 Write property test for random appearance validity (Property 8).
    - **Property 8: Random appearance is valid**
    - **Validates: Requirement 2.5**
  - [x]* 1.4 Write property test for name rules (Property 9).
    - **Property 9: Name rules**
    - **Validates: Requirements 1.1, 1.2**

- [x] 2. Implement item catalog, loadout, and palette domain
  - [x] 2.1 Implement `items` with the full catalog, including all eight Kiroween items and their `twoHanded`/`hides` flags.
    - _Requirements: 5.1, 6.1_
  - [x] 2.2 Implement `loadout`: `equip`, `unequip`, `validateLoadout`, `hiddenLayers`, `DRAW_ORDER`, `drawOrder`, and the JSON codec. Implement `palette::buildPalette`.
    - _Requirements: 4.1–4.5, 3.1_
  - [x]* 2.3 Write property test for appearance and loadout round-trip (Property 1).
    - **Property 1: Appearance and loadout round-trip**
    - **Validates: Requirement 7.1**
  - [x]* 2.4 Write model-based property tests for slot integrity, two-handed exclusivity, and locked items (Properties 2–4).
    - **Property 2: Slot integrity** / **Property 3: Two-handed exclusivity** / **Property 4: Locked items are never equipped**
    - **Validates: Requirements 4.1–4.4**
  - [x]* 2.5 Write property tests for hiding and draw order (Properties 5–6).
    - **Property 5: Hiding does not mutate appearance** / **Property 6: Draw order is a filtered permutation**
    - **Validates: Requirements 3.1, 4.5**

- [x] 3. Add persistence and services
  - [x] 3.1 Add migration `0002_adventurers.sql` (`adventurer`, `item_unlock`, partial unique index) and the repositories.
    - _Requirements: 1.4, 6.3, 7.1, 7.2_
  - [x] 3.2 Implement `AdventurerService`: `createAdventurer`, `saveAdventurer`, `setActiveAdventurer`, `deleteAdventurer`, and `claimKiroweenItem`, with ownership checks and the single-active transaction.
    - _Requirements: 1.1–1.5, 6.2, 6.3, 7.1–7.3_
    - _Note: `claimKiroweenItem` takes the user's Questing level as an argument (the Reaper's Scythe needs 13). XP lives in the quest-logs spec, so the caller supplies it; guests start at level 13 there. `grantItem` is the hook quest-logs will use to write LEVEL unlocks. Repositories are SQL inside the service for now (one consumer)._
  - [x]* 3.3 Write integration tests for the single-active invariant and the 5-Adventurer limit.
    - _Requirements: 1.4, 7.2, 7.3_

- [ ] 4. Create sprites
  - Draw the original 32×48 paper-doll layers for both body types, all styles, all catalog items, and four facings with 2 idle frames, as 8-bit indexed PNGs that use the reserved placeholder indices.
  - Pack them into `assets/sprites/avatar.png` and `avatar.json`, and log them in `docs/ASSETS.md`.
  - _Requirements: 3.1, 6.1_

- [ ] 5. Build the AvatarRenderer
  - [ ] 5.1 Implement `AvatarRenderer`: atlas loading, palette swap, layer compositing via `drawOrder`, the texture cache, facings, and idle animation that respects "Reduce motion".
    - _Requirements: 2.6, 3.1–3.3, 4.5_
  - [ ] 5.2 Implement `exportSurface`, `platform::exportPng` (desktop save dialog; web download), and the Export button.
    - _Requirements: 8.1, 8.2_
  - [ ] 5.3 Add the dev-only avatar gallery screen for visual QA.
    - _Requirements: 3.1_

- [ ] 6. Build the Costumer screen
  - [ ] 6.1 Build the designer panel: arrow controls, colour swatches, Randomise, rotate buttons, and the "Describe" text toggle.
    - _Requirements: 2.1–2.6, 3.2, 3.4_
  - [ ] 6.2 Build the paper-doll equipment panel (9 slots) with the context menu (Remove/Examine) and the wardrobe grid with slot and "Owned only" filters and locked states.
    - _Requirements: 4.1–4.6, 5.1–5.3_
  - [ ] 6.3 Add Kiroween claim actions and the "Returns next Kiroween" state.
    - _Requirements: 6.2–6.4_
  - [ ] 6.4 Add the Adventurer switcher and delete confirmation, and show the active Adventurer on the World_Map.
    - _Requirements: 7.2, 7.3_

- [ ] 7. Checkpoint
  - Extend the smoke test: create an Adventurer → claim and equip Jack-o'-Lantern Mask + Reaper's Scythe (Guest users start at level 13) → Save → restart the `App` on the same database → still equipped.
  - Make sure all tests pass, and ask the user if questions arise.
