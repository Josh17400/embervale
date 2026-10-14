// M9 "Arcana & Mastery": the skills' Game hooks (Game's internals are private; SkillOps is its friend, game.h). SKILLS
// lane (it may ADD functions; the hooks below are called from where phase A put them and keep their meaning).
// Defined in rpg/sim/skills_game.cpp. Phase A: every hook is wired at its call site and NEUTRAL in numbers (the
// multipliers return 1, the gains count the acts and raise the skills, no character XP and no family / armour effect
// yet), so every balance gate (--metrics, --power-curve, --foes, --arms, --companion) is exactly as before. The SKILLS
// lane switches them on and keeps the gates green (re-recording a baseline only with a written reason).
#pragma once
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/skills.h"

struct SkillOps {
  // ---- the one doorway for gains (Game::skillUse calls it): `base` use points of an act of skill s (1 = one ordinary
  //      act); levels, the slowing curve, the character XP of a skill-up (gainXp), Ev::Skill for the view
  static void use(Game& g, skills::Skill s, float base);

  // ---- combat (game.cpp)
  // the player's blow (meleeHit, a player-owned arrow in updateProjectiles) is about to land on v: the multiplier on its
  // damage from the family's level, its vs-tags, the perks (a dagger's backstab on the unaware, a power attack's...).
  // fam: the weapon family (Bow for arrows); power: the combo finisher / a charged shot
  static float strikeMul(Game& g, const Actor& v, skills::Skill fam, bool power);
  // ... and after it landed (dealt: the damage done; killed: it died): the family's gain, perk side effects (stagger,
  // bleed, a silent kill)
  static void struck(Game& g, Actor& v, skills::Skill fam, float dealt, bool crit, bool power, bool killed);
  // the player is about to take `dmg` (damage(), after armour): the worn class's extra reduction and perks; returns the
  // damage to apply. taken: after it landed (the armour skill's gain, Block's)
  static float takenMul(Game& g, int attacker, float dmg);
  static void taken(Game& g, int attacker, float dealt);
  // the player's swing timing (x windup / recovery), reach (+px), stamina cost (x): the family's traits and perks
  static float swingTime(const Game& g);
  static float reachAdd(const Game& g);
  static float swingCost(const Game& g);
  // movement: x walk speed (recalcPlayer), the roll (x distance, x stamina), x stamina regeneration (updatePlayer)
  static float speedMul(const Game& g);
  static float rollMul(const Game& g);
  static float rollCostMul(const Game& g);
  static float staminaRegenMul(const Game& g);
  // a roll began (updatePlayer): Athletics; a heavy-armour charge perk may turn it into a shoulder charge (true: it did)
  static bool rolled(Game& g);
  // the bow: x draw time (shootArrow's cooldown), and a loosed arrow (the slow-time draw perk)
  static float drawMul(const Game& g);
  static void loosed(Game& g);
  // stealth (ai.cpp): x how far a foe senses the player (sneaking, the worn class's noise, perks)
  static float detectMul(const Game& g, const Actor& foe);
  // once per update step (after updatePlayer): walking / sprinting distance (Athletics), sneaking (Stealth), riding
  // distance (Riding), the time worn per material and culture (Heritage), timers
  static void step(Game& g, float dt);
  // recalcPlayer, after the gear is summed: the perks' and set bonuses' additions to the hero's stats
  static void applyPerks(Game& g, gear::GearStats& s);
  // gainXp: a character level was reached (a perk point)
  static void leveledUp(Game& g);

  // ---- the economy and talk (game_rpg.cpp: MAGIC lane's file; these calls stay where phase A put them)
  static float buyMul(const Game& g, Role seller);    // x the price on a shelf (priceFactor)
  static float sellMul(const Game& g);                // x what a merchant pays (sell)
  static void traded(Game& g, int gold);              // a purchase or a sale of that much (Speech)
  // talkTo (a section-7 hook): trainers (census jobs teach their trade's skills for gold, capped per level), companions
  // teaching their vocation, speech options (persuade / intimidate); options in [DLG_SKILLS, DLG_SKILLS_END)
  static void talk(Game& g, Actor& a);
  static bool choose(Game& g, const DlgOpt& o);
  // shopStock: skill books on a scholar's / a trainer's shelf (appended to the stock, after MagicOps::shopStock)
  static void shopStock(Game& g, const Actor& a, std::vector<Item>& stock);
  // useItem: a skill book read (true: used up)
  static bool readBook(Game& g, int invIndex);
  // dropLoot / openChest: a skill book may lie among the loot (M8b sources), placed as a pickup at `at`
  static void loot(Game& g, Vec2 at, int D, gear::LootSrc src);

  // ---- the crafts (craft*.cpp, home*.cpp: SKILLS lane's hooks there)
  static void forged(Game& g, Item& made);            // the forge made a piece: Smithing, its improvement perks
  static void cooked(Game& g, const Item& meal);      // a meal cooked: Cooking
  static void farmed(Game& g, int act);               // 0 till, 1 plant, 2 water, 3 harvest, 4 tend an animal: Farming

  // ---- (SKILLS lane) more hooks, each called where it acts (game.cpp, ai.cpp, craft_game.cpp, home_game.cpp,
  //      home_actors.cpp, party_talk.cpp)
  // meleeHit / a hero's arrow, before the blow's damage: the victim's state before it (unaware, mid-rush) for the perks,
  // Mace SKULL CRUSHER's broken bones
  static void preStrike(Game& g, Actor& v, skills::Skill fam);
  // meleeHit: the blow is a sure crit (a dagger on the unaware, the sword's RIPOSTE); called after the crit's own draw
  static bool forceCrit(Game& g, const Actor& v, skills::Skill fam);
  // meleeHit: the swing's reach and cone (WHIRLWIND all round, CLEAVE wider, IMPALE down a line)
  static void swingShape(Game& g, bool finisher, float& reach, float& cone);
  // damage(): the hero is not staggered by this blow (STEADFAST, IMMOVABLE, STEADY); x the hero's knockback; the rider
  // keeps the saddle (SURE SEAT); a perfect dodge (EVASION, MATCHLESS)
  static bool unstaggerable(const Game& g, float dmg);
  static float knockMul(const Game& g);
  static bool keepSaddle(const Game& g, float dmg);
  static void dodged(Game& g);
  // ai.cpp: foes lose the hero (SHADOW STRIKE, MATCHLESS: unseen; VANISH: sneaking far enough away)
  static bool hidden(const Game& g);
  static bool losesTrail(const Game& g, const Actor& foe, float dist);
  // the crafts' perks: a piece taken apart (Smithing's act; QUICK STUDY's x progress), a meal's second portion, the Well
  // Fed hours x and the heal a cooked meal adds, a harvest's extra crop, watering the neighbours, x produce collected, x
  // the horse's pace, the stable's keep
  static void studied(Game& g);
  static int studyMul(const Game& g);
  static bool secondHelping(const Game& g);
  static float fedMul(const Game& g, const Item& food);
  static float mealHeal(const Game& g, const Item& food);
  static int harvestBonus(const Game& g);
  static bool rainMaker(const Game& g);
  static int produceMul(const Game& g);
  static float horseMul(const Game& g);
  static int stableFee(const Game& g, int fee);
  // the worn gear the skills' way: the wielded weapon, the class worn (the body piece; without one the heaviest piece),
  // a full set of one class (body, helmet, gloves, boots), the people whose make is the whole set (0: mixed), the class's
  // trade at its skill level
  static const Item* wielded(const Game& g);
  static skills::Weight wornWeight(const Game& g);
  static bool fullSet(const Game& g, skills::Weight w);
  static uint64_t setCulture(const Game& g);
  static skills::ArmourTrade trade(const Game& g);
  // Smithing is craft::Knowledge::skill: the track mirrors the forge's tenths and writes them back
  static void smithFromCraft(Game& g);
  static void craftFromSmith(Game& g);
  // trainers (15.29.4): the skills a census job (or role) teaches (n <= 3) and the level they teach to; a level's price;
  // lessons left at this character level (skills::TRAIN_PER_LEVEL); one lesson (gold 0: a companion's)
  static int teaches(Game& g, const Actor& a, skills::Skill out[3], int& cap);
  static int trainCost(const Game& g, skills::Skill s);
  static int trainLeft(const Game& g);
  static bool train(Game& g, skills::Skill s, int cap, int gold, std::string* why = nullptr);
  // a companion's vocation (party::Member index): what they can show (n <= 3) and how far (their bond)
  static int companionTeaches(const Game& g, int mi, skills::Skill out[3], int& cap);
  // a foe reels for secs (a boss less, the dragon never), with the word over its head (nullptr: none)
  static void stagger(Game& g, Actor& t, float secs, const char* what);
  // a word over the action (Ev::Text), kept in Skills::Live::words for scripts' `expect skillword`
  static void word(Game& g, Vec2 p, int color, float f, const std::string& s);
  static constexpr uint8_t TAG_DOZE = 4;   // Actor::skillTag: a staged sentry dozing where it stands (scripts)
  static constexpr int DLG_COMPANION = DLG_SKILLS + 6;   // the companion's option (arg: members index; party_talk.cpp)

  // ---- tests and scripts
  static void swing(Game& g) { g.meleeHit(g.pl()); }
  static void gain(Game& g, int xp) { g.gainXp(xp); }
  static void recalc(Game& g) { g.recalcPlayer(); }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  // a person of the roster at a level (tests, scripts): a bandit (unarmoured people) or, for any other role, a man-at-arms
  // in his culture's mail with a shield (Soldier, Guard); hostile, aware, of the bandit's body at that level
  static int spawnPerson(Game& g, Role role, Vec2 at, int level);
  static int spawnMonster(Game& g, art::Monster m, Vec2 at, int level) { return g.spawnMonster(m, at, level, false); }
  static void damage(Game& g, Actor& v, float dmg, Vec2 from, int attacker) { g.damage(v, dmg, from, attacker); }
  static void update(Game& g, Actor& a, float dt) { g.updateActor(a, dt); }
  static std::vector<Item> shopStockOf(Game& g, const Actor& a) { return g.shopStock(a); }
  static void shootArrow(Game& g) { g.shootArrow(); }
  static void projectiles(Game& g, float dt) { g.updateProjectiles(dt); }
  static float weaponDamage(const Game& g) { return g.weaponDamage(); }
  // set a skill's level outright (scripts `skill set`, tests): xp 0, Ev::Skill not sent
  static void setLevel(Game& g, skills::Skill s, int level);
  // take a perk (the UI's tap, scripts): false with why when it is blocked (skills::perkBlocked)
  static bool takePerk(Game& g, skills::Skill s, int idx, std::string* why = nullptr);
  // respec at a shrine or a trainer (costly and rare): every perk of the skill back, its points returned, for gold
  static int respecCost(const Game& g, skills::Skill s);
  static bool respec(Game& g, skills::Skill s, std::string* why = nullptr);
};
