# Requirements Document: Game Shell

## Introduction

The Game Shell is the retro MMO-client frame that every AgentScape screen lives inside. It runs as a native desktop window and as a WebAssembly canvas in the browser. It includes:

- the login screen and local profiles
- the integer-scaled game frame (viewport, side tab panel, and chatbox)
- the Promptmoor world hub map with clickable buildings
- right-click context menus and hover text
- the activity chatbox
- the seasonal Kiroween mode
- local data storage and persistence

The other specs (character-creator, agent-codex, quest-logs) render their screens inside this shell.

## Glossary

- **Game_Frame**: The 765×503 logical-pixel layout made of Viewport, Tab_Panel, and Chatbox, drawn into a render target and scaled to fit the window ("Fullscreen", the default) or, with "Fullscreen" off, at an integer scale.
- **Viewport**: The main left region where screens render their primary content (map, creator, codex card, etc.).
- **Tab_Panel**: The right-side panel with icon tabs (Equipment, Skills, Quest Log, Settings).
- **Chatbox**: The bottom region that shows system messages and the activity feed.
- **Screen**: A top-level view shown in the Viewport (World, Costumer, Codex, Tavern, Hiscores, Graveyard, …).
- **World_Map**: The Promptmoor town scene with clickable Hotspots.
- **Hotspot**: A clickable map area, such as the Grand Codex, Tavern, Hiscores Obelisk, Costumer, or Graveyard.
- **Context_Menu**: The right-click menu that lists actions for a target.
- **Kiroween_Mode**: The seasonal Halloween presentation layer.
- **Guest_Login**: A credentials-free login that creates a fresh local Guest User.
- **GitHub_Login**: A desktop-only login that links a local User to a GitHub identity through the OAuth device flow.
- **Local_Store**: The SQLite database file that holds all app data.

## Requirements

### Requirement 1: Integer-scaled game frame

**User Story:** As a player, I want the app to look like a crisp, fixed-size game window, so that it feels like the browser MMOs I remember.

#### Acceptance Criteria

1. BY default, THE Game_Frame SHALL render at the largest scale `s > 0` (fractional allowed) such that `765·s ≤ drawableWidth` and `503·s ≤ drawableHeight`, centred, with letterbox bars in the stone-dark colour; fractional scales SHALL keep pixels even (drawn at the whole part of the scale, then stretched by less than 2× with linear filtering), and text SHALL be rasterised at that whole-part resolution so it stays sharp. WHILE "Fullscreen" is off and the drawable size is at least 765×503 pixels, it SHALL instead render at the largest integer scale `s ≥ 1` that fits.
2. THE desktop window SHALL have a minimum size of 765×503. IN the web build, WHEN the browser viewport is smaller than the frame, THE Game_Frame SHALL render at 1× and the page SHALL scroll.
3. WHEN the window is resized or moved to a display with a different pixel density, THE Game_Frame SHALL recompute its scale on the next frame.
4. THE Game_Frame SHALL render all sprite, UI, and frame textures with nearest-neighbour scaling (no blurring).
5. THE Game_Frame SHALL convert pointer and touch coordinates from window pixels to frame pixels before any UI or World_Map hit testing.

### Requirement 2: Login screen and profiles

**User Story:** As a new player, I want a classic "Welcome to AgentScape" login screen, so that I can sign in and start playing quickly.

#### Acceptance Criteria

1. WHEN the app starts, THE system SHALL show the login screen unless a remembered User (Player or Guest) exists, in which case it SHALL sign that User in. Signing in through the login screen remembers that User on this device until they Log out.
2. THE login screen SHALL show a stone panel titled "Welcome to AgentScape" with a "Play as Guest" button, a "Create a profile" button, and a list of existing local non-Guest Users to continue as.
3. WHEN the user presses "Play as Guest", THE system SHALL create a fresh, isolated Guest User named `Guest-<4 random chars>` and sign in to it.
4. WHERE the build is a desktop build with a GitHub client ID configured, THE login screen SHALL also show "Sign in with GitHub". Pressing it SHALL:
   1. Request a device code, then show the user code and verification URL in a stone panel, open the URL in the default browser, and offer "Copy code".
   2. Poll for authorisation at the interval GitHub returns (adding 5 s after each `slow_down` response) until it's approved, denied, or expired.
   3. On approval, fetch the GitHub profile once, then sign in to the local User linked to that GitHub ID, or create one. The access token SHALL be discarded and never written to disk.
5. WHERE the build is the web build, THE login screen SHALL hide "Sign in with GitHub".
6. WHEN sign-in succeeds and the User has no Adventurer, THE system SHALL open the Costumer to create one. Otherwise, it SHALL open the World screen.
7. IF sign-in fails (denied, expired, or network error), THEN THE login screen SHALL show the error in red game-voice text and stay on the login screen.
8. THE Tab_Panel Settings tab SHALL offer "Log out", which returns to the login screen. WHEN the signed-in User is a Guest, THE system SHALL first ask "Log out of this Guest? Its progress will be lost." and log out only on confirmation, because a Guest is never listed to continue as.
9. WHEN the user presses "Create a profile" and enters a name, THE system SHALL create a permanent local Player with that name, sign in to it, and list it to continue as from then on. The name SHALL follow the Adventurer name rules (1–12 letters or digits, with single spaces, `-` or `_` between them) and SHALL NOT match, ignoring case and separators, another Player, a seeded NPC, a reserved name, or a name beginning "Guest ". IF the name is refused, THEN THE login screen SHALL say why in game-voice text and keep the form open. `Escape` or "Cancel" closes the form.

### Requirement 3: Promptmoor world hub

**User Story:** As a player, I want to click buildings on a town map to reach features, so that navigating the app feels like exploring a game world.

#### Acceptance Criteria

1. THE World_Map SHALL show these Hotspots: Grand Codex (Codex screen), Tavern (Tavern screen), Hiscores Obelisk (Hiscores screen), Costumer (Costumer screen), and, WHILE Kiroween_Mode is active, the Graveyard (Graveyard screen).
2. WHEN the pointer hovers a Hotspot, THE Viewport SHALL show hover text in the form `<Default action> <Name> / N more options`.
3. WHEN the user left-clicks a Hotspot, THE system SHALL perform its default action (for example, "Enter").
4. WHEN the user right-clicks, long-presses (≥ 500 ms) on a touch screen, or presses the `Menu` key or `Shift+F10` on a focused Hotspot, THE Context_Menu SHALL open at the pointer or focus position.
5. THE World_Map SHALL render the active Adventurer standing in the town square.
6. THE Viewport SHALL provide a keyboard- and gamepad-navigable list of all Hotspots that offers the same actions as the map.

### Requirement 4: Context menu and examine

**User Story:** As a player, I want right-click menus with "Examine", so that I can inspect anything the way I did in classic MMOs.

#### Acceptance Criteria

1. THE Context_Menu SHALL list the target's default action first and `Cancel` last.
2. THE Context_Menu SHALL include `Examine` for every target that has examine text.
3. WHEN the user selects `Examine`, THE Chatbox SHALL print the target's examine text as a game message.
4. WHEN the user presses `Escape` (or gamepad B), clicks outside the menu, or selects `Cancel`, THE Context_Menu SHALL close without side effects.
5. THE Context_Menu SHALL be fully operable by keyboard (arrow keys, `Enter`, and `Escape`) and gamepad (D-pad, A, and B).

### Requirement 5: Chatbox and activity feed

**User Story:** As a player, I want a chatbox that narrates what's happening, so that the world feels alive.

#### Acceptance Criteria

1. THE Chatbox SHALL show a "Welcome to AgentScape." message when the user enters the game.
2. THE Chatbox SHALL have filter tabs: `All`, `Game` (system messages for this user), and `Tavern` (community activity).
3. WHEN the user enters the game, THE Chatbox SHALL load the 20 most recent ActivityEvents from the Local_Store into the `Tavern` channel.
4. WHEN a service commits an ActivityEvent, THE Chatbox SHALL append it through the in-process EventBus, in chronological order and without duplicates.
5. THE Chatbox SHALL keep the most recent 100 messages and drop the oldest ones first.
6. Any feature SHALL be able to push a game message to the Chatbox through a shared API (`ui::pushGameMessage`).

### Requirement 6: Kiroween mode

**User Story:** As a player visiting during spooky season, I want a Halloween version of the world, so that the app feels seasonal and fun.

#### Acceptance Criteria

1. THE system SHALL resolve the Kiroween setting from, in order of precedence: the `--kiroween` command-line flag, the `KIROWEEN_MODE` environment variable, the web URL query parameter `kiroween`, and the build default `AS_KIROWEEN_DEFAULT`. Unrecognised values SHALL be ignored.
2. WHEN the resolved setting is `on`, THE system SHALL activate Kiroween_Mode. WHEN it is `off`, THE system SHALL deactivate it.
3. WHEN the resolved setting is `auto` or unset, THE system SHALL activate Kiroween_Mode only if the current UTC date is between 1 October and 15 November, inclusive.
4. WHILE Kiroween_Mode is active, THE World_Map SHALL show jack-o'-lanterns, a fog overlay, and the Graveyard Hotspot, and THE UI SHALL use the Kiroween accent palette.
5. WHILE "Reduce motion" is on, THE fog overlay SHALL be static.

### Requirement 7: Accessibility and settings

**User Story:** As a player with accessibility needs, I want the retro UI to stay usable, so that the nostalgic style doesn't lock me out.

#### Acceptance Criteria

1. THE Settings tab SHALL offer "Plain font", "Reduce motion", "Sound effects", and "Fullscreen" (on by default) toggles, persisted per User.
2. WHEN "Plain font" is on, THE UI SHALL render all text in a legible sans-serif font.
3. All interactive elements SHALL be reachable by keyboard and gamepad navigation and SHALL show a visible focus indicator.
4. Every map or scene interaction SHALL have an equivalent list control.
5. IN the web build, a new User's "Reduce motion" default SHALL follow the browser's `prefers-reduced-motion` setting.
6. EVERY screen other than the login screen and the World SHALL offer a visible way back ("Back" or "Cancel"), and THE Tab_Panel SHALL offer "To town", which opens the World and clears the navigation history. The Costumer before the first Adventurer exists is the one exception (Requirement 2.6): only "Log out" leaves it.
7. WHEN the user presses `Escape` (or gamepad B) while no text field is being edited and no Context_Menu is open, THE current screen SHALL first close what it has open (a form, a confirmation, or a step of the quest scroll) and otherwise go back one screen, under the same exception as criterion 6.
8. WHERE going back with `Escape` would discard unsaved work (a started Quest Log, unsaved Costumer changes, a started proposal), THE first press SHALL only print "Press Escape again to leave without saving…" in the Chatbox, and the second press SHALL leave.

### Requirement 8: Local data and persistence

**User Story:** As a player, I want my Adventurers and Quest Logs to still be there next time, so that my progress isn't lost.

#### Acceptance Criteria

1. WHEN the app starts, THE system SHALL open the Local_Store (desktop: the SDL pref path or `--data-dir`; web: an IndexedDB-backed file system) and apply any pending migrations in order inside a single transaction.
2. IF the Local_Store can't be opened or migrated, THEN THE system SHALL show "Your save could not be loaded." with a "Start fresh" option that renames the old file to `agentscape.db.bak-<timestamp>` before creating a new one.
3. IN the web build, THE system SHALL wait for the IndexedDB file system to load before opening the Local_Store, and show a loading message meanwhile.
4. IN the web build, THE system SHALL flush the Local_Store to IndexedDB within 1 s after every committed write transaction and when the page is hidden.
5. WHEN the app starts, THE system SHALL delete Guest Users whose last activity is older than 24 hours, together with their content, except the remembered Guest (see quest-logs Requirement 9.2).
