#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "game/effects.hpp"
#include "game/xp_feedback.hpp"
#include "ui/quest_form.hpp"

using namespace as;
using game::Effects;
using ui::QuestForm;
using ui::QuestStep;

// --- XP feedback wording and decisions ---------------------------------------------------------------------------

TEST_CASE("the level-up and unlock messages use the classic wording", "[unit][xp-feedback]") {
  CHECK(game::levelUpMessage(14) == "Congratulations, you just advanced a Questing level! You are now level 14.");
  CHECK(game::unlockMessage("Wizard Hat") == "You have unlocked: Wizard Hat!");
  CHECK(game::dropText(250) == "+250 XP");
}

TEST_CASE("a gain shows a drop, and a level rise shows the new level once", "[unit][xp-feedback]") {
  const auto plain = game::feedbackFor(100, 350, 3, 3);
  CHECK(plain.gained == 250);
  CHECK_FALSE(plain.levelUp.has_value());
  const auto up = game::feedbackFor(3460, 3710, 13, 14);
  CHECK(up.gained == 250);
  CHECK(up.levelUp == std::optional<int>{14});
  const auto jump = game::feedbackFor(0, 5000, 1, 15);   // several levels at once: one message, the final level
  CHECK(jump.levelUp == std::optional<int>{15});
}

TEST_CASE("losing XP, or no change, shows nothing", "[unit][xp-feedback]") {
  const auto lost = game::feedbackFor(3710, 3460, 14, 13);
  CHECK(lost.gained == 0);
  CHECK_FALSE(lost.levelUp.has_value());
  const auto same = game::feedbackFor(500, 500, 5, 5);
  CHECK(same.gained == 0);
  CHECK_FALSE(same.levelUp.has_value());
}

// --- Effects --------------------------------------------------------------------------------------------------------

TEST_CASE("an XP drop rises and fades, then goes away", "[unit][effects]") {
  Effects fx;
  fx.addXpDrop(250);
  REQUIRE(fx.drops().size() == 1);
  CHECK(fx.drops()[0].amount == 250);
  fx.step(0.4, false);
  const float riseEarly = fx.drops()[0].rise;
  CHECK(riseEarly > 0.0F);
  CHECK(fx.drops()[0].alpha == 1.0F);   // solid for the first half
  fx.step(0.8, false);
  CHECK(fx.drops()[0].rise > riseEarly);
  CHECK(fx.drops()[0].alpha < 1.0F);
  fx.step(1.0, false);
  CHECK(fx.drops().empty());
  CHECK_FALSE(fx.active());
}

TEST_CASE("with Reduce motion an XP drop stays put and solid, then goes away", "[unit][effects]") {
  Effects fx;
  fx.addXpDrop(25);
  fx.step(1.0, true);
  REQUIRE(fx.drops().size() == 1);
  CHECK(fx.drops()[0].rise == 0.0F);
  CHECK(fx.drops()[0].alpha == 1.0F);
  fx.step(1.0, true);
  CHECK(fx.drops().empty());   // still feedback, but it ends
}

TEST_CASE("a zero or negative drop is ignored", "[unit][effects]") {
  Effects fx;
  fx.addXpDrop(0);
  fx.addXpDrop(-5);
  CHECK_FALSE(fx.active());
}

TEST_CASE("fireworks are the same for the same seed", "[unit][effects]") {
  Effects a, b, c;
  a.addFireworks(500, 300, 42);
  b.addFireworks(500, 300, 42);
  c.addFireworks(500, 300, 43);
  a.step(0.5, false);
  b.step(0.5, false);
  c.step(0.5, false);
  REQUIRE(a.bursts().size() == game::BURSTS_PER_LEVEL_UP);
  REQUIRE(a.bursts()[0].sparks.size() == game::SPARKS_PER_BURST);
  for (std::size_t i = 0; i < a.bursts().size(); ++i) {
    for (std::size_t j = 0; j < a.bursts()[i].sparks.size(); ++j) {
      CHECK(a.bursts()[i].sparks[j].x == b.bursts()[i].sparks[j].x);
      CHECK(a.bursts()[i].sparks[j].y == b.bursts()[i].sparks[j].y);
    }
  }
  CHECK(a.bursts()[0].sparks[0].x != c.bursts()[0].sparks[0].x);
}

TEST_CASE("fireworks burst outward, fall, and finish", "[unit][effects]") {
  Effects fx;
  fx.addFireworks(500, 300, 7);
  const auto start = fx.bursts()[0].sparks[0];
  fx.step(0.5, false);   // the first burst has been going for 0.5 s; later ones are staggered
  const auto later = fx.bursts()[0].sparks[0];
  CHECK(std::hypot(later.x - start.x, later.y - start.y) > 10.0F);
  CHECK(later.life < start.life);
  for (int i = 0; i < 20; ++i) fx.step(0.1, false);
  CHECK(fx.bursts().empty());
  CHECK_FALSE(fx.active());
}

TEST_CASE("sparks stay inside a sensible region while they live", "[unit][effects]") {
  Effects fx;
  fx.addFireworks(500, 300, 99);
  for (int i = 0; i < 12; ++i) {
    fx.step(0.1, false);
    for (const auto& b : fx.bursts()) {
      for (const auto& s : b.sparks) {
        CHECK(s.x > -200.0F);
        CHECK(s.x < 700.0F);
        CHECK(s.y > -200.0F);
        CHECK(s.y < 600.0F);
      }
    }
  }
}

TEST_CASE("with Reduce motion a firework is one frozen burst that then ends", "[unit][effects]") {
  Effects fx;
  fx.addFireworks(500, 300, 5);
  fx.step(0.3, true);
  fx.step(0.3, true);
  REQUIRE_FALSE(fx.bursts().empty());
  // Every spark of a burst that has begun is at rest, and stays exactly where it is as time passes.
  std::vector<std::pair<float, float>> positions;
  for (const auto& s : fx.bursts()[0].sparks) {
    CHECK(s.vx == 0.0F);
    CHECK(s.vy == 0.0F);
    positions.emplace_back(s.x, s.y);
  }
  fx.step(0.2, true);
  for (std::size_t i = 0; i < positions.size(); ++i) {
    CHECK(fx.bursts()[0].sparks[i].x == positions[i].first);
    CHECK(fx.bursts()[0].sparks[i].y == positions[i].second);
  }
  for (int i = 0; i < 20; ++i) fx.step(0.1, true);
  CHECK(fx.bursts().empty());
}

// --- QuestForm -------------------------------------------------------------------------------------------------------

namespace {
QuestForm filled() {
  QuestForm f;
  f.begin("seed:agent:gryphon");
  f.setOverall(4);
  f.mutableInput().title = "A solid helper";
  f.mutableInput().body = "It wrote working code on the first try.";
  return f;
}
}  // namespace

TEST_CASE("a new form is about the chosen Agent, in Coding, with nothing rated yet", "[unit][quest-form]") {
  QuestForm f;
  f.begin("seed:agent:gryphon");
  CHECK(f.input().agentId == "seed:agent:gryphon");
  CHECK(f.input().questType == domain::QuestType::Coding);
  CHECK(f.input().overall == 0);
  CHECK(f.step() == QuestStep::Where);
  CHECK_FALSE(f.editing());
  CHECK_FALSE(f.ready());
}

TEST_CASE("step one needs an Agent, step two an overall rating, and Next refuses until they are fine", "[unit][quest-form]") {
  QuestForm f;
  f.begin("");
  CHECK_FALSE(f.next());
  CHECK(f.step() == QuestStep::Where);
  CHECK_FALSE(f.errorsFor(QuestStep::Where).empty());
  f.setAgent("seed:agent:cairn");
  CHECK(f.errorsFor(QuestStep::Where).empty());
  REQUIRE(f.next());
  CHECK(f.step() == QuestStep::Rating);

  CHECK_FALSE(f.next());                           // no overall rating yet
  CHECK_FALSE(f.errorsFor(QuestStep::Rating).empty());
  f.setOverall(3);
  REQUIRE(f.next());
  CHECK(f.step() == QuestStep::Story);
  CHECK_FALSE(f.next());                           // the last step: nowhere further
  CHECK(f.step() == QuestStep::Story);
}

TEST_CASE("an empty title or short story blocks the form, and back goes one step", "[unit][quest-form]") {
  QuestForm f;
  f.begin("seed:agent:cairn");
  f.setOverall(5);
  f.goTo(QuestStep::Story);
  CHECK_FALSE(f.ready());
  CHECK_FALSE(f.errorsFor(QuestStep::Story).empty());
  f.mutableInput().title = "Great";
  f.mutableInput().body = std::string(20, 'x');
  CHECK(f.ready());
  CHECK(f.errorsFor(QuestStep::Story).empty());
  f.back();
  CHECK(f.step() == QuestStep::Rating);
  f.back();
  CHECK(f.step() == QuestStep::Where);
  f.back();
  CHECK(f.step() == QuestStep::Where);   // never before the first step
}

TEST_CASE("a Skill rating of zero clears it, and ratings stay within 1 to 5", "[unit][quest-form]") {
  QuestForm f = filled();
  f.setSkill(domain::Skill::Speed, 3);
  CHECK(f.skillStars(domain::Skill::Speed) == 3);
  f.setSkill(domain::Skill::Speed, 9);
  CHECK(f.skillStars(domain::Skill::Speed) == 5);
  f.setSkill(domain::Skill::Speed, 0);
  CHECK(f.skillStars(domain::Skill::Speed) == 0);
  CHECK(f.input().skills.empty());
  f.setSkill(domain::Skill::Speed, -2);   // negative also clears
  CHECK(f.input().skills.empty());
  f.setOverall(99);
  CHECK(f.input().overall == 5);
  f.setOverall(-1);
  CHECK(f.input().overall == 0);
  CHECK(f.skillStars(domain::Skill::Value) == 0);
}

TEST_CASE("tags toggle on and off, at most three a side", "[unit][quest-form]") {
  QuestForm f = filled();
  CHECK(f.toggleTag(true, "Accurate"));
  CHECK(f.toggleTag(true, "Fast"));
  CHECK(f.toggleTag(true, "Creative"));
  CHECK_FALSE(f.toggleTag(true, "Clean code"));   // a fourth is refused
  CHECK(f.input().pros.size() == 3);
  CHECK(f.hasTag(true, "Fast"));
  CHECK_FALSE(f.hasTag(true, "Clean code"));
  CHECK(f.toggleTag(true, "Fast"));               // taking one off frees a slot
  CHECK_FALSE(f.hasTag(true, "Fast"));
  CHECK(f.toggleTag(true, "Clean code"));
  CHECK(f.toggleTag(false, "Slow"));              // the other side is separate
  CHECK(f.input().cons.size() == 1);
  CHECK(f.ready());
}

TEST_CASE("a form built only from the controls is always valid for the tags", "[unit][quest-form]") {
  QuestForm f = filled();
  for (const auto tag : domain::PRO_TAGS) f.toggleTag(true, std::string(tag));
  for (const auto tag : domain::CON_TAGS) f.toggleTag(false, std::string(tag));
  CHECK(f.input().pros.size() == 3);
  CHECK(f.input().cons.size() == 3);
  CHECK(f.ready());
}

TEST_CASE("the Quest Type cycles through all eight and wraps", "[unit][quest-form]") {
  QuestForm f = filled();
  const auto first = f.input().questType;
  for (int i = 0; i < 8; ++i) f.cycleQuestType();
  CHECK(f.input().questType == first);
  f.setQuestType(domain::QuestType::Other);
  f.cycleQuestType();
  CHECK(f.input().questType == domain::QuestType::Coding);
}

TEST_CASE("fields map to the step they live on", "[unit][quest-form]") {
  CHECK(QuestForm::stepOfField("agent") == QuestStep::Where);
  CHECK(QuestForm::stepOfField("version") == QuestStep::Where);
  CHECK(QuestForm::stepOfField("overall") == QuestStep::Rating);
  CHECK(QuestForm::stepOfField("skills") == QuestStep::Rating);
  CHECK(QuestForm::stepOfField("title") == QuestStep::Story);
  CHECK(QuestForm::stepOfField("body") == QuestStep::Story);
  CHECK(QuestForm::stepOfField("pros") == QuestStep::Story);
  CHECK(QuestForm::stepOfField("cons") == QuestStep::Story);
}

TEST_CASE("the first step with a problem is found, so a refused Save can jump there", "[unit][quest-form]") {
  QuestForm f;
  f.begin("seed:agent:cairn");
  CHECK(f.firstBadStep() == std::optional{QuestStep::Rating});   // no overall yet; title and body are later
  f.setOverall(3);
  CHECK(f.firstBadStep() == std::optional{QuestStep::Story});
  f.mutableInput().title = "A fine title";
  f.mutableInput().body = std::string(20, 'x');
  CHECK_FALSE(f.firstBadStep().has_value());
  f.mutableInput().version = std::string(60, 'v');
  CHECK(f.firstBadStep() == std::optional{QuestStep::Where});
}

TEST_CASE("editing an existing log starts at the ratings and can't change the Agent", "[unit][quest-form]") {
  domain::QuestLogInput saved;
  saved.agentId = "seed:agent:gryphon";
  saved.version = "v1";
  saved.questType = domain::QuestType::Debugging;
  saved.overall = 2;
  saved.title = "Second thoughts";
  saved.body = "Not as good as I first thought it was.";
  QuestForm f;
  f.edit("log-1", saved);
  CHECK(f.editing());
  CHECK(f.editingId() == "log-1");
  CHECK(f.step() == QuestStep::Rating);
  CHECK(f.input() == saved);
  CHECK(f.ready());
  f.setAgent("seed:agent:cairn");
  CHECK(f.input().agentId == "seed:agent:gryphon");   // refused
  f.back();
  CHECK(f.step() == QuestStep::Rating);               // no "where" step when editing
  f.begin("seed:agent:cairn");                         // starting over forgets the edit
  CHECK_FALSE(f.editing());
}
