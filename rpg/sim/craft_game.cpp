// M6 Steel: crafting's Game hooks (game.h "M6 Steel hooks", rpg/sim/craft.h). NUMBERS lane (its forge half).
//   craftTalk: smiths and the workers of a smelter, tannery, loom or bowyer's bench offer to let the player WORK THE
//   FORGE / SMELTER / TANNERY (Mode::Forge on Game::bench, the economy's own chains: ore -> ingot -> item, hide ->
//   leather); smiths also take commissions (trust: the apprenticeship that teaches the culture's patterns at 60 and an
//   alloy at 100), let the player STUDY a foreign piece (it is destroyed: +25 to its maker's secret) and can be robbed
//   of their patterns (a chance by smithing skill, the night and the urchin's quick hands; failure sets the guards on
//   the thief and costs the kingdom's goodwill).
//   craftChoose: those options (DLG_CRAFT .. DLG_M6_END).
//   benchMake / openBench (craft.h): the forge screen's actions.
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/gear.h"
#include "rpg/world/source.h"
#include "rpg/sim/craft_hooks.h"
#include "rpg/sim/loot_ops.h"

namespace {
// option actions (DLG_CRAFT + n)
enum : int { CR_WORK = DLG_CRAFT, CR_WORK2, CR_COMMISSION, CR_TEACH, CR_STUDY, CR_STUDYITEM, CR_STEAL, CR_MENU };
// Game::marks tags of the NUMBERS / FORGE lane (80..95, game_internal.h Mk)
constexpr Mk MK_THEFT = (Mk)80;   // a smith (npcKey): the day the player last tried to rob their patterns
constexpr Mk MK_BARRED = (Mk)81;  // a smith (npcKey): the last day of the ban after a theft was caught (no trade, no forge)
constexpr int BAR_DAYS = 7;
}  // namespace

struct CraftOps {
  static int siteOf(const Game& g, const Actor& a) {
    if (a.site >= 0 && a.site < (int)g.world.sites.size()) return a.site;
    if (a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)a.bldg].site;
    return -1;
  }
  static const cult::Culture* cultureOf(const Game& g, int si) {
    const cult::Culture* c = si >= 0 ? g.world.cultureOf(si) : nullptr;
    if (c && g.world.src) c = &g.world.src->culture(c->id);
    return c;
  }
  // the station an actor works: a smith's forge, else the workshop it belongs to (Smithy when none)
  static bool stationOf(const Game& g, const Actor& a, art::Building& at) {
    const art::Building b = a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size() ? g.world.over.bldgs[(size_t)a.bldg].type : art::Building::House;
    if (a.role == Role::Smith) { at = craft::isStation(b) ? b : art::Building::Smithy; return true; }
    if (!a.npc || !a.human || a.hostile) return false;
    if (craft::isStation(b) && (a.role == Role::Villager || a.role == Role::Merchant)) { at = b; return true; }
    return false;
  }
  static int commissionPrice(const Game& g, int si) { return 40 + 10 * std::max(1, si >= 0 ? g.world.sites[(size_t)si].level : 1); }
  static void fillBench(Game& g) {
    const cult::Culture* c = g.bench.culture && g.world.src ? &g.world.src->culture(g.bench.culture) : nullptr;
    g.bench.recipes = craft::benchRecipes(g.bench.at, g.craft, c, g.inv);
    // a smith's forge has its furnace too: the smelter's recipes (ore -> ingot, bronze, steel) after the forge's
    if (g.bench.at == art::Building::Smithy) {
      std::vector<craft::Recipe> sm = craft::benchRecipes(art::Building::Smelter, g.craft, c, g.inv);
      g.bench.recipes.insert(g.bench.recipes.end(), sm.begin(), sm.end());
    }
  }
  // teach what the trust has earned (apprenticeship): the patterns at 60, an alloy at 100
  static void teach(Game& g, const cult::Culture& c, int trust, std::string& said) {
    if (trust >= craft::TRUST_PATTERN) {
      for (craft::SecretKind k : {craft::SecretKind::WeaponPattern, craft::SecretKind::ArmourPattern})
        if (!g.craft.knows(c.id, 0, k) && craft::learnSecret(g.craft, c, 0, k, 100, craft::HOW_APPRENTICE)) {
          said += " WATCH CLOSELY: THIS IS HOW WE MAKE " + craft::secretName(c, 0, k) + ".";
          g.emit(Ev::QuestUpdate, g.pl().p, 0, 1, "LEARNED " + craft::secretName(c, 0, k));
        }
    }
    if (trust >= craft::TRUST_ALLOY)
      for (size_t i = 0; i < c.arms.alloys.size(); i++) {
        const uint8_t al = (uint8_t)(i + 1);
        if (g.craft.knows(c.id, al, craft::SecretKind::AlloyRecipe)) continue;
        if (!craft::howAllowed(c, al, craft::SecretKind::AlloyRecipe, craft::HOW_APPRENTICE)) continue;
        if (craft::learnSecret(g.craft, c, al, craft::SecretKind::AlloyRecipe, 100, craft::HOW_APPRENTICE)) {
          said += " NOW THE SECRET OF " + craft::secretName(c, al, craft::SecretKind::AlloyRecipe) + ". TELL NO ONE.";
          g.emit(Ev::QuestUpdate, g.pl().p, 0, 1, "LEARNED " + craft::secretName(c, al, craft::SecretKind::AlloyRecipe));
          g.sfx((int)Sfx::LevelUp, g.pl().p, 1.1f);
        }
        break;   // one alloy at a time
      }
  }
  // the made thing into the pack (stacks, auto-equip into an empty slot) with the forge's ring
  static void give(Game& g, const Item& made) {
    Item it = made;
    SkillOps::forged(g, it);   // (M9) Smithing's gain and its improvement perks (skills_game.cpp)
    g.addItem(it, true);
    g.sfx((int)Sfx::HitHeavy, g.pl().p, 1.6f, 0.8f);
    g.emit(Ev::Sparkle, g.pl().p + Vec2(0, -10));
  }
  static std::vector<Item> stock(Game& g, const Actor& a) { return LootOps::shopStock(g, a); }
  static bool shopOf(Game& g, int id) {
    const int k = g.findActor(id);
    if (k < 0) return false;
    g.openShop(g.actors[(size_t)k]);
    return true;
  }
  // a piece worth breaking down: made by a culture whose pattern for it (or, once that is known, whose alloy) is not
  // yet known to the player
  static bool studyable(const Game& g, const Item& it) {
    if (!it.culture || !itemEquippable(it.kind)) return false;
    if (it.flags & IF_KIT) return false;   // (P3 LOOT) a companion's own kit is never broken down
    const craft::SecretKind sk = (it.kind == ItemKind::Weapon || it.kind == ItemKind::Bow) ? craft::SecretKind::WeaponPattern : craft::SecretKind::ArmourPattern;
    if (!g.craft.knows(it.culture, 0, sk)) return true;
    return it.mat == Mat::Alloy && it.alloy && !g.craft.knows(it.culture, it.alloy, craft::SecretKind::AlloyRecipe);
  }
  static std::string trustLine(int t) {
    return t >= craft::TRUST_ALLOY ? "YOU ARE AS GOOD AS FAMILY AT THIS FORGE."
           : t >= craft::TRUST_PATTERN ? "YOU HAVE A SMITH'S HANDS NOW. KEEP WORKING."
           : t >= 30 ? "YOU ARE LEARNING. BRING ME MORE WORK."
           : "I DON'T TEACH STRANGERS. BUY FROM ME, COMMISSION A PIECE, AND WE'LL SEE.";
  }
  // (M7b) what more trust would earn, said by the smith (the old "(TRUST 0/100...)" note read as a journal entry)
  static std::string trustHint(int t) {
    return t >= craft::TRUST_ALLOY ? "" : t >= craft::TRUST_PATTERN ? "A LITTLE MORE, AND I'LL TEACH YOU OUR ALLOY."
                                                                    : "EARN MY TRUST AND I'LL SHOW YOU MY PATTERNS.";
  }
};

// ---------------------------------------------------------------------------------------------- the dialogue
void CraftHooks::talk(Game& g, Actor& a) {
  art::Building at;
  if (!CraftOps::stationOf(g, a, at)) return;
  // (M6 fixer) a smith who caught the player at the pattern wall bars them for a week: no wares, no forge, no lessons
  if (a.role == Role::Smith) {
    const auto mb = g.marks.find(markKey(g.npcKey(a), MK_BARRED));
    if (mb != g.marks.end() && g.day <= mb->second) {
      g.dlg.opts.erase(std::remove_if(g.dlg.opts.begin(), g.dlg.opts.end(), [](const DlgOpt& o) { return o.action == A_TRADE; }), g.dlg.opts.end());
      const int left = mb->second - g.day + 1;
      speech::propose(g.dlg, speech::P_URGENT, speech::Topic::Craft,
                      "YOU. THIEF. GET OUT OF MY FORGE BEFORE I CALL THE WATCH AGAIN, AND DON'T SHOW YOUR FACE HERE FOR " +
                          (left == 1 ? std::string("A DAY.") : speech::countWord(left) + " DAYS."));
      return;
    }
  }
  // (phone-first: two lines at most here; the apprenticeship's choices live one step down, under THE CRAFT). A
  // workshop worker sells its goods (the tanner's leather, the smelter's ingots...): its WARES come first, so the
  // station never is a conversation's first option (headless bots choose option 0; a smith already offers WARES)
  if (a.role != Role::Smith) {
    g.dlg.opts.push_back({"LET ME SEE YOUR WARES.", A_TRADE, 0});
    g.dlg.opts.push_back({craft::stationVerb(at), CR_WORK, (int)at});
    return;
  }
  g.dlg.opts.push_back({craft::stationVerb(at), CR_WORK, (int)at});
  const cult::Culture* c = CraftOps::cultureOf(g, CraftOps::siteOf(g, a));
  if (c && c->id) g.dlg.opts.push_back({"ABOUT THE CRAFT...", CR_MENU, 0});
}

namespace {
// the apprenticeship's options (commission, teach, study, steal), as THE CRAFT shows them
void craftMenu(Game& g, bool study, bool canSteal, int price) {
  g.dlg.opts.clear();
  g.dlg.opts.push_back({"COMMISSION A PIECE (" + std::to_string(price) + " GOLD)", CR_COMMISSION, 0});
  g.dlg.opts.push_back({"TEACH ME YOUR CRAFT", CR_TEACH, 0});
  if (study) g.dlg.opts.push_back({"STUDY A FOREIGN PIECE", CR_STUDY, 0});
  if (canSteal) g.dlg.opts.push_back({"STEAL THE PATTERNS", CR_STEAL, 0});
  g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
}
}  // namespace

// (P3 LOOT) the piece a smith's commission makes this restock: the settlement's band, Uncommon to Rare, in its people's
// make capped at their warriors' alloy (ruling 7), seeded per smith and restock (day / 2)
Item CraftHooks::commissionPiece(Game& g, const Actor& a) {
  const int si = CraftOps::siteOf(g, a);
  const cult::Culture* c = CraftOps::cultureOf(g, si);
  const uint64_t key = g.npcKey(a);
  // (P3 LOOT) seeded per smith and restock (day / 2), never by the hero's purse: the same visit offers the same piece
  Rng r(hash32((uint32_t)key ^ (uint32_t)(key >> 32) ^ (uint32_t)((g.day / 2) * 131) ^ (uint32_t)g.seed ^ 0xC0A1u));
  static const ItemKind kinds[] = {ItemKind::Weapon, ItemKind::Armor, ItemKind::Helmet, ItemKind::Shield, ItemKind::Gloves, ItemKind::Boots};
  const ItemKind k = kinds[r.irange(6)];
  const bool spear = c && c->arms.polearm != cult::Polearm::None;
  const int sub = k == ItemKind::Weapon ? r.irange(spear ? 6 : 5) : 0;
  // (M8b) a commission is the smith's best work for the settlement's band: Uncommon or better, a shop's cap (Rare;
  // the gate by the settlement's band, never the player's level: 7.5 rule 4)
  gear::DropSource ds;
  ds.D = std::clamp(si >= 0 ? g.world.sites[(size_t)si].level : 1, 1, gear::MAX_D);
  ds.src = gear::LootSrc::Shop;
  ds.playerLevel = ds.D;
  ds.culture = c ? c->id : 0;
  ds.atLeast = Rarity::Uncommon;
  ds.atMost = Rarity::Rare;
  const Rarity rar = gear::rollRarity(r, ds);
  // (P3 LOOT, ruling 7) the cap at the warriors' alloy; a hidden people's smiths and a capital smith who trusts the
  // hero (trust 60) make their masterwork
  const int tr = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
  const bool capital = si >= 0 && g.world.sites[(size_t)si].capital;
  const int cap = !c || c->hidden || (capital && tr >= gear::COMMISSION_TRUST) ? 0 : gear::SHELF_ALLOY_CAP;
  Item it = gear::makeGearCapped(r, k, sub, ds.D, rar, c, cap);
  return it;
}

bool CraftHooks::choose(Game& g, const DlgOpt& o) {
  const int ai = g.findActor(g.dlg.actor);
  if (ai < 0) { g.mode = Mode::Play; return true; }
  Actor& a = g.actors[(size_t)ai];
  const int si = CraftOps::siteOf(g, a);
  const cult::Culture* c = CraftOps::cultureOf(g, si);
  const uint64_t key = g.npcKey(a);
  switch (o.action) {
    case CR_MENU: {   // ABOUT THE CRAFT...
      bool study = false;
      for (const Item& it : g.inv) study |= CraftOps::studyable(g, it);
      const auto mt = g.marks.find(markKey(key, MK_THEFT));
      const int t = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
      g.dlg.text = speech::joinFit(CraftOps::trustLine(t), CraftOps::trustHint(t));
      if (t < 30) g.dlg.text = "I DON'T TEACH STRANGERS. BUY FROM ME, COMMISSION A PIECE, AND IN TIME I'LL SHOW YOU MY PATTERNS.";
      craftMenu(g, study, mt == g.marks.end() || mt->second != g.day, CraftOps::commissionPrice(g, si));
      return true;
    }
    case CR_WORK: case CR_WORK2:
      craft::openBench(g, a.id, (art::Building)o.arg);
      return true;
    case CR_COMMISSION: {
      const int price = CraftOps::commissionPrice(g, si);
      if (g.gold < price) { g.dlg.text = "THAT'S NOT ENOUGH FOR A PIECE OF MY WORK."; return true; }
      g.gold -= price;
      Item it = CraftHooks::commissionPiece(g, a);
      it.flags |= IF_CRAFTED;
      g.addItem(it);
      g.sfx((int)Sfx::Buy, g.pl().p);
      // (M6 fixer r4) only the first commission of a restock (every 2 days) builds trust, and less once the patterns
      // are taught: gold alone cannot buy the apprenticeship (15.11) in one visit
      const int t0 = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
      int& last = g.craft.live.commissionAt[key];
      const bool earns = last != g.day / 2 + 1;
      const int nt = earns ? std::min(100, t0 + (t0 < craft::TRUST_PATTERN ? craft::TRUST_COMMISSION : craft::TRUST_COMMISSION_HIGH)) : t0;
      if (earns) last = g.day / 2 + 1;
      g.craft.trust[key] = (uint8_t)nt;
      std::string said = "HERE: " + it.name + ". HONEST WORK. " +
                         (!earns ? std::string("TRUST IS EARNED OVER SEASONS, NOT BOUGHT IN AN AFTERNOON.")
                          : nt < 30 ? std::string("KEEP BUYING FROM ME AND WE'LL SEE ABOUT LESSONS.") : CraftOps::trustLine(nt));
      if (c) CraftOps::teach(g, *c, nt, said);
      g.dlg.text = said;
      return true;
    }
    case CR_TEACH: {
      const int t = g.craft.trust.count(key) ? g.craft.trust[key] : 0;
      std::string said = t < 30 ? std::string("NOT YET. I DON'T KNOW YOU WELL ENOUGH TO SHOW YOU MY WORK.") : speech::joinFit(CraftOps::trustLine(t), CraftOps::trustHint(t));
      if (c) CraftOps::teach(g, *c, t, said);
      g.dlg.text = said;
      return true;
    }
    case CR_STUDY: {
      g.dlg.text = "WHICH PIECE? BREAKING IT DOWN TEACHES YOU HOW ITS MAKERS WORK, BUT YOU WON'T GET IT BACK.";
      g.dlg.opts.clear();
      int n = 0;
      for (int i = 0; i < (int)g.inv.size() && n < 4; i++) {
        const Item& it = g.inv[(size_t)i];
        if (!CraftOps::studyable(g, it)) continue;
        bool worn = false;
        for (int e : {g.eqWeapon, g.eqBow, g.eqStaff, g.eqArmor, g.eqHelmet, g.eqShield, g.eqRing, g.eqAmulet, g.eqGloves, g.eqBoots, g.eqCloak}) worn |= e == i;
        if (worn) continue;
        g.dlg.opts.push_back({"BREAK DOWN " + it.name, CR_STUDYITEM, i});
        n++;
      }
      if (!n) g.dlg.text = "TAKE IT OFF FIRST. I WON'T BREAK A PIECE YOU'RE WEARING.";
      g.dlg.opts.push_back({"FAREWELL.", A_BYE, 0});
      return true;
    }
    case CR_STUDYITEM: {
      if (o.arg < 0 || o.arg >= (int)g.inv.size() || !g.inv[(size_t)o.arg].culture || !g.world.src) { g.mode = Mode::Play; return true; }
      if (g.inv[(size_t)o.arg].flags & IF_KIT) { g.dlg.text = "THAT IS YOUR COMPANION'S OWN KIT. I WON'T BREAK IT."; g.dlg.opts = {{"FAREWELL.", A_BYE, 0}}; return true; }
      const Item it = g.inv[(size_t)o.arg];
      const cult::Culture& M = g.world.src->culture(it.culture);
      craft::SecretKind sk = (it.kind == ItemKind::Weapon || it.kind == ItemKind::Bow) ? craft::SecretKind::WeaponPattern : craft::SecretKind::ArmourPattern;
      uint8_t al = 0;
      if (it.mat == Mat::Alloy && it.alloy && craft::howAllowed(M, it.alloy, craft::SecretKind::AlloyRecipe, craft::HOW_BREAKDOWN) &&
          g.craft.knows(M.id, 0, sk)) { sk = craft::SecretKind::AlloyRecipe; al = it.alloy; }
      g.dropItem(o.arg);
      const std::string what = craft::secretName(M, al, sk);
      const bool learned = craft::learnSecret(g.craft, M, al, sk, craft::STUDY_PROGRESS * SkillOps::studyMul(g), craft::HOW_BREAKDOWN);   // (M9) QUICK STUDY
      SkillOps::studied(g);   // (M9) Smithing's act (the forge's number: craft.skill)
      g.sfx((int)Sfx::HitHeavy, g.pl().p, 1.3f, 0.7f);
      g.dlg.text = "YOU TAKE " + it.name + " APART, RIVET BY RIVET. " + what + ": " + std::to_string(g.craft.progress(M.id, al, sk)) + "%." +
                 (learned ? " NOW YOU UNDERSTAND IT." : "");
      if (learned) g.emit(Ev::QuestUpdate, g.pl().p, 0, 1, "LEARNED " + what);
      g.dlg.opts = {{"FAREWELL.", A_BYE, 0}};
      return true;
    }
    case CR_STEAL: {
      g.marks[markKey(key, MK_THEFT)] = g.day;
      g.deed(party::Deed::Stole, key);   // M8: the companions see the theft
      if (!c || !c->id) { g.dlg.text = "THERE'S NOTHING HERE WORTH STEALING."; return true; }
      // the patterns first (whichever is less known), then an alloy that can be taken by theft
      craft::SecretKind sk = g.craft.progress(c->id, 0, craft::SecretKind::WeaponPattern) <= g.craft.progress(c->id, 0, craft::SecretKind::ArmourPattern)
                                 ? craft::SecretKind::WeaponPattern : craft::SecretKind::ArmourPattern;
      uint8_t al = 0;
      if (g.craft.knows(c->id, 0, craft::SecretKind::WeaponPattern) && g.craft.knows(c->id, 0, craft::SecretKind::ArmourPattern))
        for (size_t i = 0; i < c->arms.alloys.size(); i++)
          if (!g.craft.knows(c->id, (uint8_t)(i + 1), craft::SecretKind::AlloyRecipe) && craft::howAllowed(*c, (uint8_t)(i + 1), craft::SecretKind::AlloyRecipe, craft::HOW_THEFT)) {
            sk = craft::SecretKind::AlloyRecipe; al = (uint8_t)(i + 1); break;
          }
      float chance = 0.30f + (float)g.craft.skillLevel() / 250.0f + (g.isNight() ? 0.15f : 0.0f) + (g.background == Background::Urchin ? 0.15f : 0.0f);
      if (al) chance -= (float)c->arms.alloys[(size_t)al - 1].secrecy / 1000.0f;
      Rng r(hash32((uint32_t)key ^ (uint32_t)(g.day * 7919) ^ (uint32_t)g.seed));
      if (r.f() < chance) {
        const std::string what = craft::secretName(*c, al, sk);
        const bool learned = craft::learnSecret(g.craft, *c, al, sk, 50, craft::HOW_THEFT);
        g.dlg.text = "WHILE " + a.name + " LOOKS AWAY YOU COPY THE PATTERNS FROM THE WALL. " + what + ": " + std::to_string(g.craft.progress(c->id, al, sk)) + "%." +
                   (learned ? " IT IS YOURS NOW." : "");
        if (learned) g.emit(Ev::QuestUpdate, g.pl().p, 0, 1, "LEARNED " + what);
        g.sfx((int)Sfx::Pickup, g.pl().p, 0.8f);
        g.dlg.opts = {{"FAREWELL.", A_BYE, 0}};
        return true;
      }
      // caught: the smith shouts, the watch comes, the kingdom remembers
      g.craft.trust[key] = 0;
      if (a.npc && a.human) g.talkMem.flag(key, speech::TM_SLIGHTED, g.day);   // (M7b) the smith remembers
      if (si >= 0) if (const Kingdom* K = g.world.kingdomOf(si)) g.realm.addRep(K->id, -20);
      int guards = 0;
      for (size_t k = 1; k < g.actors.size(); k++) {
        Actor& gd = g.actors[k];
        if (!gd.npc || gd.role != Role::Guard || gd.st == AState::Dead || len2(gd.p - g.pl().p) > (30.0f * TILE) * (30.0f * TILE)) continue;
        gd.hostile = true; gd.aggro = true; gd.target = g.pl().id;
        g.emit(Ev::Text, gd.p + Vec2(0, -24), (int)rgba(255, 110, 90), 0, "THIEF!");
        guards++;
      }
      // (M6 fixer) the smith bars the thief from the forge and the shop for a week, and the watch takes its fine (a
      // smith works indoors, where no guard stands, so the fine is what makes a caught theft cost something)
      g.marks[markKey(key, MK_BARRED)] = g.day + BAR_DAYS - 1;
      const int lvl = std::max(1, si >= 0 ? g.world.sites[(size_t)si].level : 1);
      const int fine = std::min(g.gold, 60 + 20 * lvl);
      g.gold -= fine;
      g.sfx((int)Sfx::Bell, g.pl().p);
      g.say(std::string(guards ? "THIEF! THE WATCH IS COMING FOR YOU" : "THIEF! THE WATCH FINES YOU") +
          (fine > 0 ? " (-" + std::to_string(fine) + " GOLD)" : "") + ". BARRED FROM THIS FORGE FOR " + std::to_string(BAR_DAYS) + " DAYS");
      g.mode = Mode::Play;
      return true;
    }
    default: break;
  }
  return false;
}

// ---------------------------------------------------------------------------------------------- the forge screen
namespace craft {

const cult::Culture* benchCulture(const Game& g) { return g.bench.culture && g.world.src ? &g.world.src->culture(g.bench.culture) : nullptr; }
int benchDanger(const Game& g) {
  return std::clamp(g.bench.site >= 0 && g.bench.site < (int)g.world.sites.size() ? g.world.sites[(size_t)g.bench.site].level : 1, 1, gear::MAX_D);
}

std::vector<Item> stockOf(Game& g, const Actor& a) { return CraftOps::stock(g, a); }
bool openShopOf(Game& g, int actorId) { return CraftOps::shopOf(g, actorId); }

bool openBench(Game& g, int actorId, art::Building at) {
  int ai = -1;
  for (int i = 0; i < (int)g.actors.size(); i++) if (g.actors[(size_t)i].id == actorId) ai = i;
  if (ai < 0) return false;
  const Actor& a = g.actors[(size_t)ai];
  craft::Bench b;
  b.actor = actorId;
  b.at = isStation(at) ? at : art::Building::Smithy;
  b.site = CraftOps::siteOf(g, a);
  const cult::Culture* c = CraftOps::cultureOf(g, b.site);
  b.culture = c ? c->id : 0;
  const std::string place = b.site >= 0 ? g.world.sites[(size_t)b.site].name : std::string();
  b.title = std::string("THE ") + stationName(b.at) + (place.empty() ? std::string() : " OF " + place);
  g.bench = b;
  CraftOps::fillBench(g);
  g.mode = Mode::Forge;
  return true;
}

bool benchMake(Game& g, int index, std::string* why, Item* made) {
  if (index < 0 || index >= (int)g.bench.recipes.size()) { if (why) *why = "NOTHING TO MAKE"; return false; }
  const Recipe r = g.bench.recipes[(size_t)index];
  std::string w;
  if (!canMake(r, g.inv, g.craft, &w)) { if (why) *why = w; return false; }
  const cult::Culture* c = r.culture && g.world.src ? &g.world.src->culture(r.culture) : benchCulture(g);
  Rng rng(hash32((uint32_t)g.craft.skill * 2654435761u ^ (uint32_t)g.inv.size() * 977u ^ (uint32_t)(g.time * 60.0f) ^ (uint32_t)g.seed));
  Item out;
  if (!make(r, g.inv, g.craft, rng, benchDanger(g), c, out, g.plLevel)) { if (why) *why = "THE WORK FAILED"; return false; }
  // the emptied stacks go (dropItem keeps the eq* indices valid), highest first
  for (int i = (int)g.inv.size() - 1; i >= 0; i--)
    if (g.inv[(size_t)i].count <= 0) g.dropItem(i);
  CraftOps::give(g, out);
  g.bench.lastMade = "MADE " + out.name + (out.count > 1 ? " x" + std::to_string(out.count) : std::string());
  g.bench.focus = index;
  if (made) *made = out;
  // the list keeps its order while the screen is open (the selection does not jump); only new secrets add rows
  return true;
}

}  // namespace craft
