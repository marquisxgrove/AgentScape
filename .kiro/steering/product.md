---
inclusion: always
---

# Product Overview: AgentScape

AgentScape is a cross-platform game client (native on Windows, macOS, and Linux, and in the browser through WebAssembly) where people **rate their real experiences using AI models and AI tools** and build a **codex of each AI's strengths and weaknesses**, all wrapped in the look and feel of an **early-2000s fantasy browser MMORPG**. It's written in C++20 with SDL3 and Dear ImGui.

The Codex ships with a seeded **community snapshot** (sample Agents, NPC adventurers, Quest Logs, and Notes). The player's own contributions are stored locally in SQLite and update it immediately. There is no server.

Users create and dress up pixel-art adventurers, wander a small medieval town, and visit buildings that map to app features. The app ships with a seasonal **Kiroween** layer (fog, jack-o'-lanterns, a graveyard, and Halloween cosmetics such as a witch's hat and jack-o'-lantern mask).

## Hackathon context

- Built for **Kiroween 2026** in the **Resurrection** category. It brings back the dead browser-MMO era (fixed-size game frame, stone UI panels, chatbox, right-click menus, paper-doll character creator, hiscores, fansite-style item databases) and uses it to solve a modern problem: choosing between AI models and tools. It's rebuilt as a native C++ client that also compiles back to the browser.
- We are also targeting the **Best Demo** bonus prize, so every feature should look good in a 3-minute video.
- The repo must stay public, open source, and contain this `.kiro/` directory. Never add `.kiro` to `.gitignore`.

## Target users

1. **Developers and knowledge workers** who use several AI tools and want honest, task-specific comparisons.
2. **Hobbyists and nostalgia fans** who enjoy the retro-MMO presentation and character customization.
3. **Hackathon judges** who need a working demo in under 5 minutes, without setup: they open the hosted web build (or a release binary) and click "Play as Guest".

## Domain glossary (use these terms consistently in code, UI, and docs)

| Term | Meaning |
|---|---|
| **User** | A local player profile stored in the database: a `PLAYER` (optionally linked to GitHub), a `GUEST`, or a seeded `NPC`. |
| **NPC Adventurer** | A seeded community member whose Quest Logs, Notes, and XP make up the community snapshot. |
| **Codex Keeper** | A User with the local moderator role (`KEEPER`). They approve proposals, hide Notes, and exclude Quest Logs. |
| **Adventurer** | A user's character (avatar). A user can own up to 5 Adventurers; one is *active*. |
| **Agent** | An AI model or AI tool that can be reviewed. `kind` is `MODEL` or `TOOL`. |
| **Grand Codex** | The knowledge base of Agents. Each Agent has an *examine card* page. |
| **Skill** | A rating dimension for Agents (see list below). Shown as a level from 1 to 99. |
| **Quest Log** | A user's review of one experience using an Agent for a task. |
| **Quest Type** | The task category of a Quest Log (Coding, Debugging, Writing, etc.). |
| **Boon / Bane** | A short, community-voted strength / weakness note about an Agent. |
| **Questing XP** | Experience a user's account earns by contributing. It is shared by all of their Adventurers and drives the Questing level and cosmetic unlocks. |
| **Hiscores** | Leaderboards for Agents (by Skill or total level) and Adventurers (by Questing XP). |
| **Costumer** | The wardrobe shop where Adventurers equip cosmetic items. |
| **Promptmoor** | The town that serves as the world hub. |
| **Hall of Hallucinations** | The Kiroween graveyard, where top-voted Banes appear as tombstones. |

### Agent Skills (fixed list, in display order)

`Coding`, `Reasoning`, `Writing`, `Research`, `Creativity`, `Tool Use`, `Context` (long-context handling), `Reliability` (accuracy / low hallucination), `Speed`, `Value` (cost-effectiveness).

## Core loop

1. Create an Adventurer and dress them up.
2. Visit the **Grand Codex** to learn about Agents.
3. Write a **Quest Log** at the **Tavern** after using an Agent, and submit **Boons/Banes**.
4. Earn **Questing XP**, level up, and unlock new cosmetics, including Kiroween gear.
5. Check the **Hiscores** to see which Agents lead each Skill.

## Product principles

- **Nostalgia skin over a modern core.** The UI looks retro, but it must be fast, keyboard- and gamepad-navigable, work offline, and run on Windows, macOS, Linux, and modern desktop browsers.
- **Honest, task-specific data.** Ratings are always tied to a Quest Type. Never show a single "best AI" claim without its context.
- **Neutral and fair.** Agent descriptions are factual and vendor-neutral. Do not use vendor logos.
- **Delight in every interaction.** Use game-style feedback for actions, e.g., "Congratulations, you just advanced a Questing level."
- **Original assets only.** All art, fonts, and audio must be original or permissively licensed. Do not copy assets, names, or fonts from any existing commercial game.

## Out of scope (for the hackathon)

- A hosted backend or shared online database. Everything is local. The chatbox shows local activity events, including those from the seeded community.
- Real-time multiplayer movement or chat between users.
- A touch-first mobile layout.
- Payments or a real-money item shop.
- Calling AI APIs from the app. AgentScape stores human experiences and does not benchmark Agents.
