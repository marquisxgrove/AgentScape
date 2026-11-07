#include "app/app.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <string>
#include <vector>

#include "domain/items.hpp"
#include "domain/xp.hpp"
#include "game/hotspots.hpp"
#include "ui/chatbox.hpp"

// The scripted smoke runs (--smoke-path, --smoke-codex, --smoke-costumer, --smoke-demo). Each takes one step per
// frame, so a step sees what the previous frame rendered, and saves a snapshot of the screens it checks into the
// run's directory. They drive screens by navigation and services by calls; none of them simulates mouse or keyboard
// input. ctest runs them against the real app (see CMakeLists.txt).

namespace as::app {

const std::optional<std::string>& App::smokeDir() const {
  if (config_.smokePathDir) return config_.smokePathDir;
  if (config_.smokeCodexDir) return config_.smokeCodexDir;
  if (config_.smokeCostumerDir) return config_.smokeCostumerDir;
  return config_.smokeDemoDir;
}

bool App::runSmokeStep() {
  if (config_.smokePathDir) return runPathStep();
  if (config_.smokeCodexDir) return runCodexStep();
  if (config_.smokeCostumerDir) return runCostumerStep();
  return runDemoStep();
}

bool App::smokeFail(const char* why) {
  SDL_Log("SMOKE FAIL: %s", why);
  demoFailed_ = true;
  return true;  // the run is over
}

void App::smokeShot(const char* file) {
  SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
  if (shot == nullptr) {
    SDL_Log("Snapshot failed: %s", SDL_GetError());
    return;
  }
  const std::string path = *smokeDir() + "/" + file;
  if (!IMG_SavePNG(shot, path.c_str())) SDL_Log("Snapshot write failed: %s", SDL_GetError());
  SDL_DestroySurface(shot);
}

// The documented demo path (testing.md), end to end, in the real app:
//   guest login -> World -> Costumer: create an Adventurer, claim and equip the Kiroween gear -> Codex card ->
//   write a Quest Log through the real quest scroll -> "+250 XP" and the level-up -> Hiscores.
// Phase 2 is a SEPARATE process on the same database (what a page reload is on the web): everything must still be
// there. It saves a snapshot of every screen. It drives screens by navigation and services by calls; it does not
// simulate mouse or keyboard input.
bool App::runPathStep() {
  const auto fail = [this](const char* why) { return smokeFail(why); };
  const auto shoot = [this](const char* file) { smokeShot(file); };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = *services_;
  const auto geared = [](const services::Adventurer& a) {
    const auto head = a.loadout.find(domain::Slot::Head);
    const auto weapon = a.loadout.find(domain::Slot::Weapon);
    return head != a.loadout.end() && head->second == "jack_mask" && weapon != a.loadout.end() && weapon->second == "reapers_scythe";
  };

  if (config_.smokePhase == 1) {
    switch (demoStep_++) {
      case 0: {   // the login screen, then "Play as Guest"
        if (screens_.currentId() != ScreenId::Login) return fail("did not start on the login screen");
        shoot("p1-login.png");
        auto guest = svc.profile.createGuest();
        if (!guest) return fail("createGuest failed");
        svc.profile.signIn(guest->id);
        adoptUser(guest->id, guest->settings);
        if (currentLevel_ != 13) return fail("a Guest should start at Questing level 13");
        store_->db.prepare("INSERT OR REPLACE INTO app_state(key, value) VALUES ('smoke_user_id', ?)").bind(1, guest->id).run();
        screens_.navigate(ScreenId::World);
        return false;
      }
      case 1:
        if (screens_.currentId() != ScreenId::World) return fail("did not reach the World");
        shoot("p2-world.png");
        screens_.navigate(ScreenId::Costumer);
        return false;
      case 2: {   // no Adventurer yet: the Costumer asks for a name; create one and dress it in the Kiroween gear
        if (screens_.currentId() != ScreenId::Costumer) return fail("did not reach the Costumer");
        shoot("p3-costumer-create.png");
        const std::string uid = *currentUserId_;
        auto created = svc.adventurers.createAdventurer(uid, "Wanderer");
        if (!created || !created->isActive) return fail("createAdventurer failed or not active");
        if (!svc.adventurers.claimKiroweenItem(uid, "jack_mask", /*kiroweenActive=*/true, currentLevel_)) return fail("claim mask");
        if (!svc.adventurers.claimKiroweenItem(uid, "reapers_scythe", true, currentLevel_)) return fail("claim scythe");
        auto owned = svc.adventurers.ownedItems(uid);
        if (!owned) return fail("ownedItems failed");
        auto gear = domain::equip(created->loadout, "jack_mask", *owned);
        if (gear) gear = domain::equip(*gear, "reapers_scythe", *owned);
        if (!gear) return fail("equip failed");
        if (!svc.adventurers.saveAdventurer(uid, created->id, created->appearance, *gear)) return fail("save failed");
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 3: {
        auto active = svc.adventurers.getActive(*currentUserId_);
        if (!active || !*active || !geared(**active)) return fail("the Kiroween gear is not equipped");
        shoot("p4-costumer-equipped.png");
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Codex, {{"q", "gryphon"}});
        return false;
      }
      case 4:   // search the Codex, open a familiar's card
        if (screens_.currentId() != ScreenId::Codex) return fail("did not reach the Codex");
        shoot("p5-codex.png");
        screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}});
        return false;
      case 5:
        if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the examine card");
        shoot("p6-codex-card.png");
        screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"step", "3"}});   // the story step, filled in
        return false;
      case 6:
        if (screens_.currentId() != ScreenId::QuestScroll) return fail("did not reach the quest scroll");
        shoot("p7-quest-scroll.png");
        screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"submit", "1"}});   // and saved
        return false;
      case 7:   // saved on its first frame; one more frame so the effects are drawn
        return false;
      case 8: {
        const std::string uid = *currentUserId_;
        auto logId = svc.quests.findOwn(uid, "seed:agent:gryphon", "dev");
        if (!logId || !*logId) return fail("the quest scroll did not save a Quest Log");
        if (totalXp_ != domain::guestSeedXp() + 250) return fail("the Quest Log should have paid exactly 250 XP");
        if (currentLevel_ != 14) return fail("a Guest's first Quest Log should take it from level 13 to 14");
        if (effects_.drops().empty() || effects_.bursts().empty()) return fail("expected the +250 XP drop and the level-up fireworks");
        shoot("p8-level-up.png");
        screens_.navigate(ScreenId::Hiscores, {{"tab", "adventurers"}, {"q", "Wanderer"}});
        return false;
      }
      case 9: {
        if (screens_.currentId() != ScreenId::Hiscores) return fail("did not reach the Hiscores");
        auto board = svc.hiscores.adventurers(0, "Wanderer");
        if (!board || !board->match) return fail("the Hiscores should list the new Adventurer");
        const auto& row = board->rows[*board->match];
        if (row.xp != totalXp_ || row.level != 14) return fail("the Hiscores row should show the new XP and level 14");
        shoot("p9-hiscores.png");
        SDL_Log("SMOKE OK: demo path phase 1 (level 14, rank %d)", row.rank);
        return true;
      }
      default: return true;
    }
  }

  // Phase 2: a brand-new process on the same database, as after a page reload.
  switch (demoStep_++) {
    case 0: {
      auto st = store_->db.prepare("SELECT value FROM app_state WHERE key = 'smoke_user_id'");
      if (!st.step()) return fail("phase 1 left no user behind");
      const std::string uid = st.columnText(0);
      auto user = svc.profile.getUser(uid);
      if (!user) return fail("the Guest from phase 1 is gone");
      adoptUser(uid, user->settings);
      if (totalXp_ != domain::guestSeedXp() + 250 || currentLevel_ != 14) return fail("the XP and level did not survive the restart");
      auto active = svc.adventurers.getActive(uid);
      if (!active || !*active || !geared(**active)) return fail("the equipped gear did not survive the restart");
      auto logId = svc.quests.findOwn(uid, "seed:agent:gryphon", "dev");
      if (!logId || !*logId) return fail("the Quest Log did not survive the restart");
      auto board = svc.hiscores.adventurers(0, "Wanderer");
      if (!board || !board->match || board->rows[*board->match].level != 14) return fail("the Hiscores did not keep the Adventurer");
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Tavern);
      return false;
    }
    case 1:
      if (screens_.currentId() != ScreenId::Tavern) return fail("did not reach the Tavern after the restart");
      shoot("p10-restored-tavern.png");
      SDL_Log("SMOKE OK: demo path phase 2 (everything survived a restart)");
      return true;
    default: return true;
  }
}

// The agent-codex flow, one step per frame so each step's screen is what the previous frame rendered:
//   guest -> Codex (searching "lanternfold" finds 2) -> examine card -> the same card in Debugging (narrower)
//   -> write a Boon, another user votes, you can't vote on your own -> Proving Grounds remembers 2 Agents
//   -> propose, a Keeper approves, the Codex grows -> Graveyard (Kiroween) holds 1 to 13 tombstones.
// It drives the real screens by navigation and the real services by calls. It does NOT simulate mouse or
// keyboard input, so it proves that each screen loads, renders and persists what the services give it.
bool App::runCodexStep() {
  const auto fail = [this](const char* why) { return smokeFail(why); };
  const auto shoot = [this](const char* file) { smokeShot(file); };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = *services_;
  const std::string me = currentUserId_.value_or("");
  using domain::QuestType;

  switch (demoStep_++) {
    case 0: {
      auto guest = svc.profile.createGuest();
      if (!guest) return fail("createGuest failed");
      svc.profile.signIn(guest->id);
      adoptUser(guest->id, guest->settings);
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Codex, {{"q", "lanternfold"}});
      return false;
    }
    case 1: {
      if (screens_.currentId() != ScreenId::Codex) return fail("did not reach the Codex");
      domain::ListQuery q;
      q.text = "lanternfold";
      auto page = svc.codex.list(q, std::nullopt);
      if (!page || page->total != 2) return fail("searching 'lanternfold' should find 2 familiars");
      shoot("x1-codex-search.png");
      screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}});
      return false;
    }
    case 2: {
      if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the examine card");
      auto card = svc.codex.card("gryphon", std::nullopt);
      if (!card || !*card || (*card)->levels.overallCount == 0) return fail("Gryphon's card has no Quest Logs");
      shoot("x2-card.png");
      screens_.navigate(ScreenId::CodexCard, {{"agent", "gryphon"}, {"scope", "DEBUGGING"}});
      return false;
    }
    case 3: {
      if (screens_.currentId() != ScreenId::CodexCard) return fail("did not reach the Debugging card");
      auto all = svc.codex.card("gryphon", std::nullopt);
      auto dbg = svc.codex.card("gryphon", QuestType::Debugging);
      if (!all || !dbg || !*all || !*dbg) return fail("card lookup failed");
      if ((*dbg)->levels.overallCount >= (*all)->levels.overallCount) return fail("Debugging should narrow the Quest Logs");
      shoot("x3-card-debugging.png");

      // Boons and votes.
      auto other = svc.profile.createGuest();
      if (!other) return fail("second guest failed");
      services::NewNote note;
      note.agentId = (*all)->agent.id;
      note.kind = services::NoteKind::Boon;
      note.text = "Smoke test remark: tidy, readable answers.";
      auto id = svc.notes.createNote(me, note);
      if (!id) return fail("createNote failed");
      if (svc.notes.voteOnNote(me, *id, domain::VoteValue::Agree)) return fail("voting on your own Note was allowed");
      if (!svc.notes.voteOnNote(other->id, *id, domain::VoteValue::Agree)) return fail("another user's vote failed");
      auto boons = svc.codex.notes((*all)->agent.id, services::NoteKind::Boon);
      if (!boons) return fail("reading Boons failed");
      const auto mine = std::find_if(boons->begin(), boons->end(), [&](const services::NoteView& n) { return n.id == *id; });
      if (mine == boons->end() || mine->agree != 1) return fail("the new Boon should show with one agree vote");

      screens_.navigate(ScreenId::Proving, {{"add", "gryphon"}, {"scope", "DEBUGGING"}});
      return false;
    }
    case 4:
      if (screens_.currentId() != ScreenId::Proving) return fail("did not reach the Proving Grounds");
      screens_.navigate(ScreenId::Proving, {{"add", "cairn"}});   // a fresh visit restores Gryphon + Debugging
      return false;
    case 5: {
      if (screens_.currentId() != ScreenId::Proving) return fail("did not return to the Proving Grounds");
      auto saved = svc.proving.restore(me);
      if (!saved || saved->slugs != std::vector<std::string>{"gryphon", "cairn"}) return fail("the comparison was not remembered");
      if (saved->questType != std::optional{QuestType::Debugging}) return fail("the Quest Type was not remembered");
      auto cols = svc.codex.compareColumns(saved->slugs, saved->questType);
      if (!cols || cols->size() != 2) return fail("compare should have 2 columns");
      shoot("x4-proving.png");

      // Propose, approve, and the Codex grows.
      domain::AgentInput in;
      in.name = "Smoke Familiar";
      in.vendor = "Candlewick";
      in.examine = "A made-up familiar proposed by the smoke test.";
      auto proposed = svc.agents.proposeAgent(me, in);
      if (!proposed) return fail("proposeAgent failed");
      auto before = svc.codex.list({}, std::nullopt);
      if (!before || before->total != 14) return fail("a pending proposal must not be in the Codex");
      if (!svc.keeper.setKeeper(me, true)) return fail("setKeeper failed");
      if (!svc.keeper.moderateAgent(me, *proposed, true)) return fail("approving the proposal failed");
      auto after = svc.codex.list({}, std::nullopt);
      if (!after || after->total != 15) return fail("the approved familiar should join the Codex");
      svc.keeper.setKeeper(me, false);

      screens_.navigate(ScreenId::Graveyard);
      return false;
    }
    case 6: {
      if (screens_.currentId() != ScreenId::Graveyard) return fail("did not reach the Graveyard");
      auto stones = svc.codex.tombstones();
      if (!stones || stones->empty() || stones->size() > domain::MAX_TOMBSTONES) {
        return fail("the Graveyard should hold 1 to 13 tombstones");
      }
      shoot("x5-graveyard.png");
      smokeTombstones_ = stones->size();

      // Write a Quest Log through the real screen: a Guest (level 13) with an Adventurer, the form prefilled by the
      // dev "demo" switch and saved on its first frame. The XP, the level-up and the effects come from the events.
      if (!svc.adventurers.createAdventurer(me, "Smokey")) return fail("createAdventurer failed");
      // The Guest started at level 13 (seed XP); the Note (+50) and the approved proposal (+500) earlier in this flow
      // have raised it since, so remember where it stands now and check the Quest Log adds exactly 250 on top.
      if (totalXp_ != domain::guestSeedXp() + 50 + 500) return fail("the Guest's XP should be its seed plus a Note and an approval");
      if (currentLevel_ != domain::levelForXp(totalXp_)) return fail("the level should follow the XP");
      smokeXpBefore_ = totalXp_;
      smokeLevelBefore_ = currentLevel_;
      screens_.navigate(ScreenId::QuestScroll, {{"agent", "gryphon"}, {"demo", "1"}, {"submit", "1"}});
      return false;
    }
    case 7:   // the screen loaded and saved on its first draw; wait one more frame for the effects to be drawn
      return false;
    case 8: {
      auto logs = svc.quests.findOwn(me, "seed:agent:gryphon", "dev");
      if (!logs || !*logs) return fail("the quest scroll did not save a Quest Log");
      auto record = svc.quests.get(**logs);
      if (!record || !*record || (*record)->input.title != "A solid helper") return fail("the saved Quest Log is wrong");
      if (totalXp_ != smokeXpBefore_ + 250) return fail("the Quest Log should have paid exactly 250 XP");
      if (currentLevel_ != domain::levelForXp(totalXp_)) return fail("the level should follow the XP");
      if (currentLevel_ <= smokeLevelBefore_) return fail("250 XP should take this Guest up a level");
      if (effects_.drops().empty()) return fail("expected a floating +250 XP drop");
      if (effects_.bursts().empty()) return fail("expected level-up fireworks");
      if (screens_.currentId() == ScreenId::QuestScroll) return fail("the scroll should close after saving");
      shoot("x6-level-up.png");
      screens_.navigate(ScreenId::Tavern);
      return false;
    }
    case 9: {   // the Tavern board shows the new Quest Log; open it
      if (screens_.currentId() != ScreenId::Tavern) return fail("did not reach the Tavern");
      services::QuestListQuery q;
      q.agentId = std::string("seed:agent:gryphon");
      auto page = svc.quests.list(q, me);
      if (!page || page->logs.empty()) return fail("the Tavern board should hold the new Quest Log");
      const auto mine = std::find_if(page->logs.begin(), page->logs.end(), [&](const services::QuestLogRecord& l) { return l.authorId == me; });
      if (mine == page->logs.end()) return fail("the board should list my own Quest Log");
      auto top = svc.quests.topHelpful("seed:agent:gryphon", 5);
      if (!top || top->empty()) return fail("Gryphon's card should list its Quest Logs");
      smokeQuestLogId_ = mine->id;
      shoot("x7-tavern.png");
      screens_.navigate(ScreenId::QuestDetail, {{"id", mine->id}});
      return false;
    }
    case 10: {  // the detail screen is up; another adventurer marks it helpful and the author is paid
      if (screens_.currentId() != ScreenId::QuestDetail) return fail("did not reach the Quest Log detail");
      shoot("x8-quest-detail.png");
      auto other = svc.profile.createGuest();
      if (!other) return fail("second guest failed");
      const auto before = totalXp_;
      auto voted = svc.quests.toggleHelpful(other->id, smokeQuestLogId_);
      if (!voted || !*voted) return fail("another adventurer's Helpful vote failed");
      if (totalXp_ != before + 25) return fail("a Helpful vote should have paid the author 25 XP");
      if (svc.quests.toggleHelpful(me, smokeQuestLogId_)) return fail("voting on your own Quest Log was allowed");
      screens_.navigate(ScreenId::Hiscores, {{"tab", "adventurers"}, {"q", "Smokey"}});
      return false;
    }
    case 11: {  // the Hiscores know about the Guest, at the rank its XP deserves
      if (screens_.currentId() != ScreenId::Hiscores) return fail("did not reach the Hiscores");
      auto board = svc.hiscores.adventurers(0, "Smokey");
      if (!board || !board->match) return fail("the Hiscores should list the Guest's Adventurer by name");
      const auto& row = board->rows[*board->match];
      if (row.xp != totalXp_ || row.level != currentLevel_) return fail("the Hiscores row should match the Guest's XP and level");
      int above = 0;
      for (const auto& r : board->rows) above += r.xp > row.xp ? 1 : 0;
      if (row.rank != 1 + above) return fail("the Guest's rank should be 1 plus the people with more XP");
      auto agents = svc.hiscores.agents({}, 0, "");
      if (!agents || agents->rows.empty() || agents->rows.front().rank != 1) return fail("the Agent board should start at rank 1");
      shoot("x9-hiscores.png");
      SDL_Log("SMOKE OK: codex flow completed (%zu tombstones, level %d, rank %d)", smokeTombstones_, currentLevel_, row.rank);
      return true;
    }
    default:
      return true;
  }
}

bool App::runCostumerStep() {
  const auto fail = [this](const char* why) { return smokeFail(why); };
  const auto shoot = [this](const char* file) { smokeShot(file); };
  if (dataState_ != DataState::Ready || !services_) return fail("store not ready");
  auto& svc = services_->adventurers;
  const auto isEquipped = [](const services::Adventurer& a) {
    const auto head = a.loadout.find(domain::Slot::Head);
    const auto weapon = a.loadout.find(domain::Slot::Weapon);
    return head != a.loadout.end() && head->second == "jack_mask" && weapon != a.loadout.end() &&
           weapon->second == "reapers_scythe";
  };

  if (config_.smokePhase == 1) {
    switch (demoStep_++) {
      case 0: {  // a fresh Guest signs in (level 13) and, having no Adventurer, lands in the Costumer
        auto guest = services_->profile.createGuest();
        if (!guest) return fail("createGuest failed");
        services_->profile.signIn(guest->id);
        adoptUser(guest->id, guest->settings);
        if (currentLevel_ != 13) return fail("a Guest should start at Questing level 13");
        store_->db.prepare("INSERT OR REPLACE INTO app_state(key, value) VALUES ('smoke_user_id', ?)")
            .bind(1, guest->id)
            .run();
        screens_.navigate(ScreenId::World);
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 1: {
        if (screens_.currentId() != ScreenId::Costumer) return fail("did not reach the Costumer");
        shoot("c1-create.png");  // the "name your first Adventurer" form
        const std::string uid = *currentUserId_;
        auto created = svc.createAdventurer(uid, "Wanderer");
        if (!created || !created->isActive) return fail("createAdventurer failed or not active");
        // Claim the two Kiroween items (the Scythe needs level 13) and equip them through the domain rules.
        if (!svc.claimKiroweenItem(uid, "jack_mask", /*kiroweenActive=*/true, currentLevel_)) return fail("claim mask");
        if (!svc.claimKiroweenItem(uid, "reapers_scythe", true, currentLevel_)) return fail("claim scythe");
        auto owned = svc.ownedItems(uid);
        if (!owned) return fail("ownedItems failed");
        auto gear = domain::equip(created->loadout, "jack_mask", *owned);
        if (!gear) return fail("equip mask");
        gear = domain::equip(*gear, "reapers_scythe", *owned);
        if (!gear) return fail("equip scythe");
        if (!svc.saveAdventurer(uid, created->id, created->appearance, *gear)) return fail("save failed");
        screens_.navigate(ScreenId::World);   // re-enter so the Costumer reloads from the database
        screens_.navigate(ScreenId::Costumer);
        return false;
      }
      case 2: {
        auto active = svc.getActive(*currentUserId_);
        if (!active || !*active || !isEquipped(**active)) return fail("the saved gear is not equipped");
        shoot("c2-equipped.png");
        SDL_Log("SMOKE OK: phase 1 (created, claimed, equipped, saved)");
        return true;
      }
      default: return true;
    }
  }

  // Phase 2: a brand-new process on the same database.
  switch (demoStep_++) {
    case 0: {
      auto st = store_->db.prepare("SELECT value FROM app_state WHERE key = 'smoke_user_id'");
      if (!st.step()) return fail("phase 1 left no user behind");
      const std::string uid = st.columnText(0);
      auto user = services_->profile.getUser(uid);
      if (!user) return fail("the Guest from phase 1 is gone");
      adoptUser(uid, user->settings);
      screens_.navigate(ScreenId::World);
      screens_.navigate(ScreenId::Costumer);
      return false;
    }
    case 1: {
      auto active = svc.getActive(*currentUserId_);
      if (!active || !*active) return fail("the Adventurer did not survive the restart");
      if ((*active)->name != "Wanderer") return fail("wrong Adventurer after restart");
      if (!isEquipped(**active)) return fail("the gear did not survive the restart");
      if (screens_.currentId() != ScreenId::Costumer) return fail("not on the Costumer");
      shoot("c3-restored.png");
      SDL_Log("SMOKE OK: phase 2 (still equipped after restart)");
      return true;
    }
    default: return true;
  }
}

bool App::runDemoStep() {
  // The demo path (game-shell task 11): guest login -> World -> open the Tavern context menu -> Enter.
  const auto fail = [this](const char* why) { return smokeFail(why); };
  const auto shoot = [this](const char* file) { smokeShot(file); };
  switch (demoStep_++) {
    case 0:
      if (dataState_ != DataState::Ready) return fail("store not ready at login");
      if (screens_.currentId() != ScreenId::Login) return fail("did not start on the login screen");
      shoot("01-login.png");
      return false;
    case 1: {
      // "Play as Guest"
      auto guest = services_->profile.createGuest();
      if (!guest) return fail("createGuest failed");
      auto signedIn = services_->profile.signIn(guest->id);
      if (!signedIn) return fail("guest signIn failed");
      adoptUser(signedIn->id, signedIn->settings);
      screens_.navigate(ScreenId::World);
      return false;
    }
    case 2:
      if (screens_.currentId() != ScreenId::World) return fail("did not reach the World screen");
      shoot("02-world.png");
      // Open the Tavern hotspot's context menu.
      for (const auto* h : game::visibleHotspots(kiroween_)) {
        if (h->id == "tavern") {
          std::vector<domain::MenuAction> actions{
              {std::string(h->id), std::string(h->defaultAction) + " " + std::string(h->name), std::string(h->id), true}};
          contextReq_.open(actions, std::string(h->examine), 200, 120);
        }
      }
      return false;
    case 3:
      if (!contextMenu_.isOpen()) return fail("context menu did not open on the Tavern");
      shoot("03-context-menu.png");
      // Confirm the default action ("Enter Tavern").
      if (auto chosen = contextMenu_.confirmSelected(); !chosen || chosen->id != "tavern") {
        return fail("confirming the menu did not yield the Tavern action");
      }
      ui::pushGameMessage("Enter Tavern...");
      return false;
    case 4:
      shoot("04-entered.png");
      SDL_Log("SMOKE OK: demo path completed");
      return true;
    default:
      return true;
  }
}

}  // namespace as::app
