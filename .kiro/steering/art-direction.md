---
inclusion: fileMatch
fileMatchPattern: ["src/ui/**/*", "src/game/**/*", "src/render/**/*", "src/screens/**/*", "assets/sprites/**/*", "assets/ui/**/*", "web/**/*"]
---

# Art Direction and UI Voice

The goal is a lovingly faithful *feel* of an early-2000s fantasy browser MMORPG. Every pixel and font must be **original**.

## Hard rules (judging and legal)

- **Do not use copied assets, fonts, sound, item names, place names, or logos** from any existing game or AI vendor. AI products are referred to only by their plain-text names.
- Record every new file under `assets/` in `docs/ASSETS.md`, including its author and license.
- The demo video cannot include copyrighted music. Use silence or original or CC0 chiptune.

## Game frame

- The logical frame is **765 × 503 px**. It's drawn into a render target and presented at the **largest integer scale** that fits the window's pixel size (minimum 1×), centred, with `STONE_DARK` letterbox bars.
  - Regions: main viewport (left, 512 × 334), side tab panel (right, 241 × 334), chatbox (bottom, 765 × 165), plus a minimap and orb area on the top right.
  - The desktop window has a minimum size of 765 × 503. In the web build, the canvas fills the page, and the page scrolls if the browser viewport is smaller than the frame.
- All panels use the **9-slice `ui::StonePanel`** (bevelled grey-brown stone with darker inner border). Do not use rounded corners, drop shadows, gradients, or blur. Stock ImGui window decorations (title bars, resize grips, collapse arrows) are never visible.
- Every texture uses `SDL_SCALEMODE_NEAREST`. ImGui anti-aliasing is off, and all widget positions and sizes are whole logical pixels.

## Palette (constants in `src/ui/theme.hpp`)

| Constant | Hex | Use |
|---|---|---|
| `STONE_DARK` | `#3e3529` | Panel borders, letterbox |
| `STONE` | `#5d5447` | Panel fill |
| `PARCHMENT` | `#c8b88a` | Scroll and parchment backgrounds |
| `INK` | `#000000` | Text shadow |
| `TEXT_YELLOW` | `#ffff00` | Default UI text, hover text |
| `TEXT_ORANGE` | `#ff981f` | Headings, tab titles |
| `TEXT_CYAN` | `#00ffff` | Agent/NPC names in menus |
| `TEXT_GREEN` | `#00ff00` | Success, level-up |
| `TEXT_RED` | `#ff0000` | Errors, Banes |
| `CHAT_BLUE` | `#0000ff` | Chatbox player names |
| `KIROWEEN_PURPLE` | `#6b2fa0` | Kiroween accents |
| `KIROWEEN_PUMPKIN` | `#ff7518` | Kiroween accents |

`ui::applyTheme()` maps these onto `ImGuiStyle` colours. Widgets must use the constants, never inline colour literals. All text on stone has a 1 px black drop shadow, drawn by `ui::ShadowText` (the text is drawn in `INK` at +1,+1, then in its colour).

## Typography

- **Pixel UI font** (OFL, e.g., *Silkscreen* or *Pixelify Sans*) loaded from `assets/fonts/` with no oversampling and pixel snapping on. Silkscreen is drawn at 12 px (1.5× its 8 px grid): the native 8 px was too small to read on the 765×503 frame. Layouts must not assume a fixed text width; measure with `ImGui::CalcTextSize` or let text wrap or clip.
- **Readable font toggle:** Settings → "Plain font" switches to a legible OFL sans-serif for accessibility. Respect it everywhere.

## Interaction vocabulary

- **Hover text** appears at the top-left of the viewport: `Enter <col=cyan>Grand Codex</col> / 3 more options`. The `<col=…>` markup is parsed by `ui::parseMarkup` and only accepts palette colour names.
- **Left click** runs the default action. **Right click**, a long press (touch, ≥ 500 ms), the `Menu` key, or `Shift+F10` opens a **ContextMenu** that has the default action first, then other actions, and always ends with `Cancel`.
- Every clickable thing has an **Examine** option with a short, witty, factual examine text.
- The **Chatbox** shows system messages in game voice:
  - "Welcome to AgentScape."
  - "Your Quest Log has been added to the Tavern board."
  - "Congratulations, you just advanced a Questing level! You are now level 12."
  - "You can't do that right now."
- Buttons are bevelled stone with a pressed state. A click plays an optional 8-bit sound (off by default).

## Avatars

- **Paper-doll sprites:** 32 × 48 px base, rendered at an integer scale, with 4 facings (S, E, N, W) and a 2-frame idle.
- Layers are authored as **8-bit indexed PNGs** that use reserved placeholder palette indices for each recolourable region (hair, torso, legs, feet, skin). The renderer swaps those palette entries, so we never ship a sprite per colour.

## Kiroween layer

- Applies only when Kiroween mode is active.
  - Purple-and-orange accents, drifting fog overlay (static when "Reduce motion" is on), jack-o'-lanterns on map tiles, and bats on the login screen.
  - The graveyard map area holds the Hall of Hallucinations.
- Halloween cosmetics: Witch's Hat, Jack-o'-Lantern Mask, Skull Mask, Ghostly Sheet, Vampire Cape, Reaper's Scythe, Candy Pail, Cobweb Boots.

## Accessibility

- Every map or scene interaction has an equivalent ImGui list control (for example, a list of map locations beside the map, a table beside the bookshelf, and a list beside the graveyard).
- Full keyboard and gamepad navigation. The nav cursor is a visible 2 px `TEXT_YELLOW` outline.
- Respect the "Reduce motion" setting everywhere. In the web build, its default comes from `prefers-reduced-motion`.
- Text contrast must be at least 4.5:1 (yellow and white on stone pass).
- Known limitation: Dear ImGui doesn't expose an accessibility tree, so screen readers aren't supported in the MVP. AccessKit integration is a stretch goal.
