# Requirements Document: Character Creator and Costumer

## Introduction

Players create pixel-art Adventurers with a classic arrow-button character designer. They then dress them up with cosmetic items at the **Costumer**, using a paper-doll equipment screen with nine slots. Seasonal **Kiroween** gear (witch's hat, jack-o'-lantern mask, and more) can be claimed during the event and kept forever. The active Adventurer represents the player across AgentScape: on the world map, on Quest Logs, and on the Hiscores.

## Glossary

- **Adventurer**: A user-owned character. It has a name, an Appearance, and a Loadout.
- **Appearance**: Body type, base clothing styles, hair, facial hair, and colours.
- **Loadout**: A mapping from Equipment_Slot to the equipped Item id (or empty).
- **Equipment_Slot**: One of `HEAD`, `CAPE`, `NECK`, `WEAPON`, `BODY`, `SHIELD`, `LEGS`, `HANDS`, `FEET`.
- **Item**: A cosmetic defined in the code catalog (`src/domain/items.cpp`), with a slot, sprite, examine text, unlock rule, and optional flags (`twoHanded`, `hides`).
- **Unlock**: A record that a user owns an Item. Unlocks are account-wide and shared by all of that user's Adventurers.
- **Avatar_Renderer**: The paper-doll renderer. It composites indexed-palette sprite layers on the CPU and caches the result as an SDL texture.
- **Costumer**: The Costumer screen, which combines the creator and the wardrobe.

## Requirements

### Requirement 1: Create an Adventurer

**User Story:** As a player, I want to create a named Adventurer, so that I have an identity in Promptmoor.

#### Acceptance Criteria

1. WHEN the user submits a name, THE system SHALL accept it only if it is 1–12 characters long, contains only letters, digits, spaces, `_`, or `-`, starts with a letter or digit, and has no leading, trailing, or consecutive separators.
2. THE system SHALL treat names as unique case-insensitively, with spaces, `_`, and `-` considered equivalent (for example, `Kiro Fan` conflicts with `kiro_fan`).
3. IF the name is taken, invalid, or on the blocklist, THEN THE system SHALL reject it with a game-voice message ("That name is not available.").
4. THE system SHALL allow at most 5 Adventurers per user.
5. WHEN a user creates their first Adventurer, THE system SHALL mark it as active.
6. WHEN the first Adventurer has just been created, THE Costumer SHALL offer "Begin" in place of "Back". "Begin" SHALL save any unsaved look and gear changes and then open the World; IF the save is refused, THEN THE Costumer SHALL stay open with the Chatbox message. Later visits to the Costumer show "Back".

### Requirement 2: Design appearance

**User Story:** As a player, I want to change my Adventurer's look with arrow buttons and colour swatches, so that it feels like a classic character designer.

#### Acceptance Criteria

1. THE Costumer SHALL offer previous and next arrows for: body type (A/B), hair style, facial hair (including "none"), torso style, arms style, and legs style.
2. THE Costumer SHALL offer colour swatches for: hair, torso, legs, feet, and skin, each from a fixed palette.
3. Arrow selection SHALL wrap around (next after last returns to first).
4. Every style option SHALL be available to both body types.
5. WHEN the user presses "Randomise", THE system SHALL produce a valid Appearance.
6. WHEN any option changes, THE Avatar_Renderer SHALL update the preview on the next frame.

### Requirement 3: Live preview

**User Story:** As a player, I want to see and rotate my Adventurer while I edit, so that I know exactly how they'll look.

#### Acceptance Criteria

1. THE Avatar_Renderer SHALL draw the Adventurer from layered sprites at an integer scale with nearest-neighbour filtering.
2. THE preview SHALL provide rotate-left and rotate-right controls that cycle through the four facings (S, E, N, W).
3. WHILE "Reduce motion" is off, THE preview SHALL play a 2-frame idle animation. WHILE it is on, the preview SHALL be static.
4. THE preview SHALL have a "Describe" toggle that shows a plain-text description of the Appearance and equipped Items below the preview.

### Requirement 4: Equip items

**User Story:** As a player, I want to equip cosmetic items on a paper-doll, so that I can dress up my Adventurer.

#### Acceptance Criteria

1. THE Loadout SHALL contain at most one Item per Equipment_Slot, and each Item SHALL only occupy its own slot.
2. WHEN the user equips an Item into an occupied slot, THE system SHALL replace the previous Item.
3. WHEN the user equips a `twoHanded` weapon, THE system SHALL unequip any `SHIELD`-slot Item. WHEN the user equips a `SHIELD` while a `twoHanded` weapon is equipped, THE system SHALL unequip the weapon.
4. IF the user tries to equip an Item they have not unlocked, THEN THE system SHALL refuse and print "You don't have that item yet." in the Chatbox.
5. WHEN an Item has `hides` flags (for example, a full mask hides hair and facial hair), THE Avatar_Renderer SHALL not draw the hidden layers. The underlying Appearance SHALL remain unchanged.
6. WHEN the user clicks an equipped slot, THE system SHALL offer `Remove` and `Examine` through the Context_Menu.

### Requirement 5: Wardrobe catalog

**User Story:** As a player, I want to browse every cosmetic and see how to unlock it, so that I have goals to work toward.

#### Acceptance Criteria

1. THE Costumer SHALL list all catalog Items grouped by Equipment_Slot, with icon, name, and examine text.
2. Locked Items SHALL appear greyed out with their unlock requirement (for example, "Requires Questing level 10").
3. THE wardrobe SHALL support filtering by slot and by "Owned only".

### Requirement 6: Kiroween gear

**User Story:** As a player during spooky season, I want to claim Halloween costumes like a witch's hat and a jack-o'-lantern mask, so that my Adventurer can dress up for Kiroween.

#### Acceptance Criteria

1. THE catalog SHALL include these Kiroween Items: Witch's Hat (HEAD), Jack-o'-Lantern Mask (HEAD, hides hair and facial hair), Skull Mask (HEAD, hides facial hair), Ghostly Sheet (BODY, hides arms, hands, and legs layers), Vampire Cape (CAPE), Reaper's Scythe (WEAPON, two-handed), Candy Pail (SHIELD), and Cobweb Boots (FEET).
2. WHILE Kiroween_Mode is active, THE Costumer SHALL show a "Claim" action on each unclaimed Kiroween Item whose extra requirement (if any) is met. The Reaper's Scythe additionally requires Questing level 13.
3. WHEN the user claims a Kiroween Item, THE system SHALL create an Unlock that persists after the event ends.
4. WHILE Kiroween_Mode is inactive, unclaimed Kiroween Items SHALL show "Returns next Kiroween".

### Requirement 7: Manage Adventurers

**User Story:** As a player with several Adventurers, I want to save, switch, and delete them, so that I can keep different looks.

#### Acceptance Criteria

1. WHEN the user presses "Save", THE system SHALL validate the Appearance and Loadout again in the service layer with the same domain rules, then persist them.
2. THE system SHALL let the user switch which Adventurer is active, and exactly one Adventurer per user SHALL be active while the user has any.
3. WHEN the user deletes an Adventurer, THE system SHALL require typing its name to confirm. IF it was active, THEN THE system SHALL activate the most recently updated remaining Adventurer.
4. Quest Logs written while an Adventurer was active SHALL keep a snapshot of that Adventurer's name and Loadout.

### Requirement 8: Export avatar

**User Story:** As a player, I want to save my Adventurer as an image, so that I can share it.

#### Acceptance Criteria

1. WHEN the user presses "Export", THE system SHALL produce a transparent PNG of the current facing at 4× scale, named `<adventurer-name>.png`.
   - On desktop, it SHALL open a native save dialog.
   - In the web build, it SHALL trigger a browser download.
2. IF the user cancels the save dialog, THEN THE system SHALL do nothing. IF writing fails, THEN THE Chatbox SHALL print "Your portrait could not be saved."
