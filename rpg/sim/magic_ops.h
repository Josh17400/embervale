// M9 "Arcana & Mastery": magic's Game hooks (Game's internals are private; MagicOps is its friend, game.h). MAGIC lane
// (it may ADD functions; the hooks below are called from where phase A put them and keep their meaning). Defined in
// rpg/sim/magic_game.cpp (casting, forms, surfaces, statuses, summons, reactions, signatures) and
// rpg/sim/magic_learn.cpp (teachers, tomes, shelves, loot, attunement, mastery, the loadout rule).
//
// The quick-cast input (Input::castSlot / castHold / castDual), for the UI lane:
//   - a TAP: castSlot = k with castHold false in one step -> the spell is cast (a wind-up first when it has a cast time);
//   - a HOLD: castSlot = k with castHold true on the press step, then castHold true (castSlot -1) while held; every
//     0.45 s held past its cast time pours in an upcast level (up to 3: Ev::Spell Charge, f = the magnitude); letting
//     go (castHold false) launches it. castDual on the press step makes it a dual-cast (needs the school's DUAL CASTING
//     perk). Input::spell (the classic SPELL button) taps the selected slot. A roll interrupts a wind-up (a Fizzle).
//   - a concentration spell (a beam, a charm) cast again lets it go.
#pragma once
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"

struct MagicOps {
  // ---- the player's casting (updatePlayer): the quick slots (Input::castSlot / castHold / castDual), the classic SPELL
  //      button (Input::spell: the selected slot) and SWAP (Input::swapSpell: the next prepared slot). true: the step is
  //      the cast's (updatePlayer returns)
  static bool input(Game& g, const Input& in, bool canAct);
  static void cast(Game& g);                          // Game::castSpell: cast the selected slot now
  // the classic three, with their pre-M9 numbers (a plain cast: level 0, not dual); an upcast or dual-cast one grows
  static void classicCast(Game& g, const magic::SpellDef& d, int level = 0, bool dual = false);
  // ---- the world (game.cpp)
  static void step(Game& g, float dt);                // once per update step: casts in progress, concentration, surfaces
                                                      // and their combos, summons, the non-magicka pools, cooldowns
  static bool actorStep(Game& g, Actor& a, float dt); // updateActor, after burns: statuses tick (true: it cannot act
                                                      // this step: frozen, or it died of them)
  static bool aiOverride(Game& g, Actor& a, float dt);   // updateAI first: frightened / charmed / blinded (true: handled)
  // a spell missile (Projectile::spell != 0) touched an actor (true: handled; updateProjectiles drops it) / ended
  // against a wall or at its range (its burst, the surface it leaves)
  static bool projHit(Game& g, Projectile& pr, Actor& v);
  static void projEnd(Game& g, const Projectile& pr);
  // damage(): a blow about to land on anyone (wet + shock, frozen shatters, wards): the damage to apply; and after it
  // landed (concentration breaks on a heavy hit to the caster)
  static float hurtMul(Game& g, Actor& v, float dmg, int attacker, Ench ench);
  static void hurt(Game& g, Actor& v, float dealt, int attacker);
  // ---- learning and the loadout (game_rpg.cpp)
  static bool learn(Game& g, uint32_t spellId, bool announce = true);   // false: known already / unknown id
  static bool readTome(Game& g, int invIndex);        // useItem: a tome read (true: used up)
  // the UI's spellbook page puts a known spell in a quick slot (id 0: empty it). The rule is magic's (15.5: the
  // loadout is swapped at rest or camp): filling an empty slot is always allowed; replacing or emptying a prepared
  // spell only after a rest (MagicOps::rested), at a camp (a campfire or a bedroll close by), indoors or in a
  // settlement, and never with a foe hunting the hero. false with why when it may not be changed now
  static bool prepare(Game& g, int slot, uint32_t spellId, std::string* why = nullptr);
  static bool mayPrepare(const Game& g, std::string* why = nullptr);
  static void talk(Game& g, Actor& a);                // talkTo: mage teachers and court wizards (DLG_MAGIC options)
  static bool choose(Game& g, const DlgOpt& o);
  static void shopStock(Game& g, const Actor& a, std::vector<Item>& stock);   // tomes on a mage's / scholar's shelf
  static void loot(Game& g, Vec2 at, int D, gear::LootSrc src);   // a rare tome among the loot
  static void rested(Game& g, int hours);             // rest(): the loadout may be swapped (15.5: at rest or camp)
  static void reset(Game& g);                         // resetPlayer: no magic (spells come from a background, tomes, teachers)
  static void background(Game& g);                    // finishCreator: the background's spell (novice MEND, marked FIREBOLT)
  // the pre-M9 fields (Game::spellsKnown / Game::spell: tests, debugKit, the classic HUD) folded into the book, and the
  // book's classic spells mirrored back into them (called by cast, step and a load)
  static void syncLegacy(Game& g);
  static void loaded(Game& g);                        // deserialize: the book's lands' traditions made again
  static void trialsLoaded(Game& g);                  // (fixer M10 r1) deserialize: the met hidden kingdoms' trials registered (before enterSite)
  // ---- the map
  static void clearMap(Game& g);                      // clearNonPlayer: surfaces, missiles in flight, summons
  static void shift(Game& g, Vec2 d);                 // windowMoved: the surfaces move with the window (tiles)
  // ---- tests and scripts
  static void damageRaw(Game& g, Actor& v, float dmg, Vec2 from, int attacker, Ench e, float ep) { g.damage(v, dmg, from, attacker, e, ep, false); }
  static void killRaw(Game& g, Actor& a, int killer) { g.kill(a, killer); }
  static int findActor(const Game& g, int id) { return g.findActor(id); }
  static Rng& rng(Game& g) { return g.rng_; }
  static void emit(Game& g, magic::Phase ph, uint32_t spell, Vec2 p, Vec2 q, float f, uint32_t id, const std::string& s = "");
  // lay a surface on a tile of the current map (scripts `surface`, tests, the spells): combos with what lies there
  static void laySurface(Game& g, int tx, int ty, magic::Surface s, float life, uint8_t power, int owner);
  // put a status on an actor (scripts `status`, tests, the spells)
  static void applyStatus(Game& g, Actor& a, magic::Status s, float secs, int by);

  // ---- (MAGIC lane) the casting pipeline
  // begin a cast from a quick slot (a tap: held false; a hold: held true). true: something happened (a cast, a wind-up)
  static bool begin(Game& g, int slot, bool held, bool dual);
  // the release: pay, announce and make the spell happen (slot -1: not from a slot, no slot cooldown)
  static bool launch(Game& g, const magic::SpellDef& d, int slot, int level, bool dual);
  // a spell's essence touching a tile: a combo with the surface there (true: one went off)
  static bool touchSurface(Game& g, int tx, int ty, magic::Essence e, int owner);
  // what a cast costs and whether the hero can pay it now
  static float costOf(const Game& g, const magic::SpellDef& d, int level, bool dual);
  static bool canPay(const Game& g, const magic::SpellDef& d, int level, bool dual);
  // the hero's spell power multiplier for this spell now (level, focus, perks, the signature's conditions)
  static float powerOf(const Game& g, const magic::SpellDef& d);
  static float signatureMul(const Game& g, const magic::SpellDef& d);
  static void releaseConc(Game& g, bool broken);
  // the world's conditions (the signatures read them; tests and scripts may force them through Live)
  static bool raining(const Game& g);
  static bool nearWater(const Game& g, Vec2 p, int tiles);
  static bool nearGraves(const Game& g, Vec2 p);
  static bool fullMoon(const Game& g);
  // ---- (MAGIC lane) traditions, attunement and mastery (magic_learn.cpp)
  static const cult::Culture* cultureHere(const Game& g, const Actor* who);   // the speaker's land, else the hero's
  static bool attune(Game& g, const magic::Tradition& t, std::string* why = nullptr);
  static bool attuned(const Game& g, uint32_t traditionId);
  static bool mayLearn(const Game& g, const magic::SpellDef& d, std::string* why = nullptr);
  static int masteryRankOf(const Game& g, uint32_t traditionId);
  static void gainMastery(Game& g, uint32_t traditionId, int xp);
  static bool affinity(const Game& g, const magic::Tradition& t);
  // the spell a companion of that culture casts (its tradition's rank-1 bolt; Common Arcana: FIREBOLT)
  static uint32_t companionSpell(Game& g, uint64_t culture);
  // scripts and tests: set the conditions
  static magic::Live& live(Game& g) { return g.arcana.live; }
  // ---- Game's private doors, for magic's own helpers (magic_game.cpp, magic_learn.cpp)
  static void ev(Game& g, Ev t, Vec2 p, int a = 0, float f = 1, const std::string& s = "") { g.emit(t, p, a, f, s); }
  static void tell(Game& g, const std::string& s) { g.say(s); }
  static void snd(Game& g, int s, Vec2 p, float pitch = 1, float vol = 1) { g.sfx(s, p, pitch, vol); }
  static void hit(Game& g, Actor& v, float dmg, Vec2 from, int attacker, Ench e = Ench::None, float ep = 0) { g.damage(v, dmg, from, attacker, e, ep, false); }
  static void move(Game& g, Actor& a, Vec2 d) { g.moveActor(a, d); }
  static bool fits(const Game& g, Vec2 p, float r) { return g.bodyFree(p, r, false); }
  static int newId(Game& g) { return g.nextId_++; }
  static void giveXp(Game& g, int xp) { g.gainXp(xp); }
  // ---- tests and scripts: a teacher beside the hero (its actor id), a talk opened with them, a land forced (the
  // culture cultureHere answers while set: a script or a test stands in a far kingdom; nullptr: the real land)
  static int spawnTeacher(Game& g, Vec2 at, Role r, int slot = 7) {
    Spawn sp;
    sp.npc = true; sp.role = r; sp.site = -1; sp.slot = slot;
    sp.x = (int)(at.x / TILE); sp.y = (int)(at.y / TILE);
    return g.spawnHuman(sp, at);
  }
  static void talkTo(Game& g, Actor& a) { g.talkTo(a); }
  static void setLand(const cult::Culture* c);

  // ---- (M10 ARCANA lane) the hidden kingdoms' magic (magic_game.cpp: the six signatures and the trials' puzzles;
  //      magic_learn.cpp: the five steps, the teachers, the focus, the dusk song-fire)
  static magic::Signature sigOf(const magic::SpellDef& d);            // its tradition's signature (Common for the schools')
  static bool attunedTo(const Game& g, magic::Signature s);           // an attuned tradition has that signature
  static float bloodMul(const Game& g);                                // Blood rites: 1 + 0.8 (1 - hp / maxHp)
  // the five steps (6.4) of a hidden tradition for the hero now: 0 all met for that rank, else the first unmet step
  // (1 landed, 2 Friendly 25, 3 level 25, 4 the Trial of the Threshold, 5 a teacher's rank 1 (attunement), 6 Honoured
  // 60, 7 the second trial, 8 mastery) with why
  static int hiddenStep(const Game& g, const magic::Tradition& t, const Actor* teacher, int rank, std::string* why = nullptr);
  static int standing(const Game& g, const magic::Tradition& t, const Actor* who);   // the realm's playerRep there
  static bool landed(const Game& g, const magic::Tradition& t);       // its land has been set foot on (sea LF_LANDED)
  static Item focusOf(Game& g, const magic::Tradition& t);            // its focus (a staff-kind item), sold only there
  static void findSongLand(Game& g);                                   // the nearest hidden land of this world
  // the trials (rpg/sim/rooms.h TrialLayout): a site made a trial of a culture's tradition (scripts, tests; the WORLD
  // lane's ew trialOf sites are registered by step); a rite passed
  static bool markTrial(Game& g, int site, const cult::Culture& c, int which);
  static void passRite(Game& g, uint32_t traditionId, int which);
  // Verdant shaping: every grown tile restored now (a map change; all: even under a body)
  static void revertGrowths(Game& g, bool all);
  static bool growTile(Game& g, int tx, int ty, uint8_t kind, float life, uint32_t spell, uint8_t order);
  // Dreamweft: a decoy of the hero at p (its actor id)
  static int spawnDecoy(Game& g, const magic::SpellDef& d, Vec2 p, float mag, float secs);
  static bool inBubble(const Game& g, Vec2 p);
  static void updateAIScaled(Game& g, Actor& a, float dt) { g.updateAI(a, dt); }
  static void enterSiteRaw(Game& g, int site) { g.enterSite(site); }   // tests / scripts: walk into a trial
  static void moveRaw(Game& g, Actor& a, Vec2 d) { g.moveActor(a, d); }
};
