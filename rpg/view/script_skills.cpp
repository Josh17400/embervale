// M9 script commands (SKILLS lane): stage the skills for screenshots and checks (VISION_PLAN 15.29).
//   skill set <skill> <level>      the skill at that level (skill: its name without spaces, any case: swords, heavyarmour,
//                                  destruction ...; or its enum word: sword, mace, bow ...)
//   skill use <skill> <n>          n ordinary acts of it (the curve, the toasts, the character XP as play would give them)
//   skill perk <skill> <index>     take that perk (fails the line when it is blocked, with the reason)
//   skill points <n>               n unspent perk points
//   skill book <skill> [read]      a book of that skill into the pack (read: the real useItem reads it at once)
//   skill sneak 0|1                sneaking off / on (as the sneak toggle does)
//   skill foe <wolf|...|bandit|soldier> <dx> <dy> [unaware] [level L]   a foe that many tiles from the hero (unaware: it
//                                  has not noticed the hero yet; it stays put until it does)
//   skill gold <n>                 the hero's purse
//   skill kit <family> <none|light|medium|heavy> [shield]   M6 gear of the hero's level: the family's weapon (a bow and
//                                  arrows; unarmed: none), a full set of one armour class, a shield
//   skill clearing <r>             a clean stage: props and their blocking cleared r tiles round the hero
//   expect skill <skill> <min>     the skill is at least at that level
//   expect perk <skill> <index>    that perk is taken
//   expect skillword <words>       a perk said those words over the action (STAGGERED, BOWLED OVER, PARRY, SILENT...)
//   expect booksread <n>           at least n skill books read
//   expect trained <n>             at least n levels bought from trainers at this character level
//   expect skillfoes <state> <n>   at least n living foes are in that state: hurt (staggered), unaware, bleeding
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/skill_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/script_api.h"

namespace {
std::string squashSk(const std::string& s) {
  std::string r;
  for (char ch : s) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\'') continue;
    r += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  }
  return r;
}
bool skillWord(const std::string& w, skills::Skill& out) {
  static const char* enumWords[] = {"sword", "axe", "mace", "spear", "dagger", "bow", "unarmed", "block", "lightarmour", "mediumarmour",
                                    "heavyarmour", "heritage", "athletics", "smithing", "cooking", "farming", "riding", "speech", "stealth",
                                    "lockpicking", "destruction", "restoration", "conjuration", "illusion", "alteration"};
  const std::string q = squashSk(w);
  for (int i = 0; i < skills::SKILLS; i++)
    if (q == squashSk(skills::skillInfo((skills::Skill)i).name) || (i < (int)(sizeof(enumWords) / sizeof(enumWords[0])) && q == enumWords[i])) { out = (skills::Skill)i; return true; }
  return false;
}
bool monsterWord(const std::string& w, art::Monster& out) {
  const std::string q = squashSk(w);
  for (int m = 0; m < (int)art::Monster::COUNT; m++) {
    const std::string n = squashSk(monsterName((art::Monster)m));
    if (q == n) { out = (art::Monster)m; return true; }
  }
  return false;
}

bool cmdSkill(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (op == "points") { g.skills.perkPts = (uint16_t)std::atoi(c.arg(2).c_str()); return true; }
  if (op == "sneak") { g.skills.live.sneaking = std::atoi(c.arg(2).c_str()) != 0; SkillOps::recalc(g); return true; }
  if (op == "gold") { g.gold = std::atoi(c.arg(2).c_str()); return true; }
  if (op == "clearing") {   // a clean stage: the props and their blocking cleared round the hero (r tiles)
    const int r = std::max(1, std::atoi(c.arg(2).c_str()));
    Map& m = g.map();
    const int cx = (int)std::floor(g.pl().p.x / TILE), cy = (int)std::floor(g.pl().p.y / TILE);
    for (int y = cy - r; y <= cy + r; y++)
      for (int x = cx - r; x <= cx + r; x++) {
        if (!m.in(x, y) || (x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r) continue;
        const size_t i = (size_t)y * m.w + x;
        if (i < m.bldgAt.size() && m.bldgAt[i] >= 0) continue;
        m.setP(x, y, 0);
        if (i < m.solid.size() && !(i < m.wall.size() && m.wall[i])) m.solid[i] = 0;
      }
    return true;
  }
  if (op == "kit") {   // skill kit <family> <none|light|medium|heavy> [shield]: M6 gear of the hero's level, a full set of one class
    skills::Skill fam = skills::Skill::Sword;
    const std::string fw = squashSk(c.arg(2));
    if (fw != "unarmed" && fw != "fists" && !skillWord(c.arg(2), fam)) { c.fail("skill kit: no weapon family " + c.arg(2)); return true; }
    if (fw == "unarmed" || fw == "fists") fam = skills::Skill::Unarmed;
    const std::string ww = squashSk(c.arg(3));
    const Mat mat = ww == "light" ? Mat::Leather : ww == "medium" ? Mat::Bronze : ww == "heavy" ? Mat::Iron : Mat::None;
    const bool shield = squashSk(c.arg(4)) == "shield";
    const int L = std::max(1, g.plLevel);
    Rng r(0x5C1Du + (uint32_t)fam);
    g.eqWeapon = g.eqBow = g.eqArmor = g.eqHelmet = g.eqShield = g.eqGloves = g.eqBoots = -1;
    auto put = [&](Item it, int& slot) { g.inv.push_back(it); slot = (int)g.inv.size() - 1; };
    static const WeaponType wt[] = {WeaponType::Sword, WeaponType::Axe, WeaponType::Mace, WeaponType::Spear, WeaponType::Dagger};
    if (fam == skills::Skill::Bow) { put(gear::makeGear(r, ItemKind::Bow, 0, L, Rarity::Common, 0, Mat::Wood), g.eqBow); g.inv.push_back(makeArrows(60)); }
    else if ((int)fam < 5) put(gear::makeGear(r, ItemKind::Weapon, (int)wt[(int)fam], L, Rarity::Common, 0, Mat::Iron), g.eqWeapon);
    if (mat != Mat::None) {
      put(gear::makeGear(r, ItemKind::Armor, 0, L, Rarity::Common, 0, mat), g.eqArmor);
      put(gear::makeGear(r, ItemKind::Helmet, 0, L, Rarity::Common, 0, mat), g.eqHelmet);
      put(gear::makeGear(r, ItemKind::Gloves, 0, L, Rarity::Common, 0, mat), g.eqGloves);
      put(gear::makeGear(r, ItemKind::Boots, 0, L, Rarity::Common, 0, mat), g.eqBoots);
    }
    if (shield) put(gear::makeGear(r, ItemKind::Shield, 0, L, Rarity::Common, 0, Mat::Wood), g.eqShield);
    SkillOps::recalc(g);
    return true;
  }
  if (op == "foe") {
    const std::string kind = squashSk(c.arg(2));
    const Vec2 at = g.pl().p + Vec2((float)std::atof(c.arg(3).c_str()) * TILE, (float)std::atof(c.arg(4).c_str()) * TILE);
    int level = std::max(1, g.plLevel);
    bool unaware = false;
    for (size_t i = 5; i < c.a.size(); i++) {
      if (squashSk(c.a[i]) == "unaware") unaware = true;
      if (squashSk(c.a[i]) == "level" && i + 1 < c.a.size()) level = std::atoi(c.a[i + 1].c_str());
    }
    int id = -1;
    art::Monster m = art::Monster::Wolf;
    if (kind == "bandit") id = SkillOps::spawnPerson(g, Role::Bandit, at, level);
    else if (kind == "soldier") id = SkillOps::spawnPerson(g, Role::Soldier, at, level);
    else if (monsterWord(c.arg(2), m)) id = SkillOps::spawnMonster(g, m, at, level);
    else { c.fail("skill foe: no foe called " + c.arg(2)); return true; }
    const int k = SkillOps::findActor(g, id);
    if (k >= 0) {
      Actor& a = g.actors[(size_t)k];
      a.aggro = !unaware;
      if (unaware) { a.aggroR = std::min(a.aggroR, 30.0f); a.home = a.goal = a.p; a.skillTag |= SkillOps::TAG_DOZE; }   // (it dozes: a short sense, no wandering off)
    }
    return true;
  }
  skills::Skill s;
  if (!skillWord(c.arg(2), s)) { c.fail("skill: no skill called " + c.arg(2)); return true; }
  const int n = std::atoi(c.arg(3).c_str());
  if (op == "set") SkillOps::setLevel(g, s, n);
  else if (op == "use") for (int i = 0; i < n; i++) g.skillUse(s, 1.0f);
  else if (op == "perk") { std::string why; if (!SkillOps::takePerk(g, s, n, &why)) c.fail("skill perk: " + why); }
  else if (op == "book") {
    Item b = skills::makeSkillBook(s, 0x5C1Bu + (uint32_t)g.inv.size() * 977u + (uint32_t)g.skills.booksRead.size() * 131u, std::max(1, g.plLevel));
    g.inv.push_back(b);
    if (squashSk(c.arg(3)) == "read") g.useItem((int)g.inv.size() - 1);
  }
  else c.fail("skill: set | use | perk | points | book | sneak | foe | gold");
  return true;
}
EMB_SCRIPT_CMD("skill", "skill set|use|perk <skill> <n> / points <n> / book <skill> [read] / sneak 0|1 / foe <kind> <dx> <dy> [unaware] [level L] / gold <n> / kit <family> <class> [shield] / clearing <r> (M9)", cmdSkill);

bool expSkill(ScriptCtx& c) {
  skills::Skill s;
  if (!skillWord(c.arg(2), s)) { c.fail("expect skill: no skill called " + c.arg(2)); return true; }
  const int want = std::atoi(c.arg(3).c_str());
  if (c.game.skills.level(s) < want) c.fail("expect skill: " + std::string(skills::skillInfo(s).name) + " is " + std::to_string(c.game.skills.level(s)) + ", wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:skill", "expect skill <skill> <min level> (M9)", expSkill);

bool expPerk(ScriptCtx& c) {
  skills::Skill s;
  if (!skillWord(c.arg(2), s)) { c.fail("expect perk: no skill called " + c.arg(2)); return true; }
  if (!c.game.skills.has(s, std::atoi(c.arg(3).c_str()))) c.fail("expect perk: not taken");
  return true;
}
EMB_SCRIPT_CMD("expect:perk", "expect perk <skill> <index>: that perk is taken (M9)", expPerk);

bool expWord(ScriptCtx& c) {
  const std::string want = c.rest(2);
  for (const std::string& w : c.game.skills.live.words) if (w.find(want) != std::string::npos) return true;
  std::string seen;
  for (const std::string& w : c.game.skills.live.words) seen += (seen.empty() ? "" : ", ") + w;
  c.fail("expect skillword: no " + want + " (said: " + seen + ")");
  return true;
}
EMB_SCRIPT_CMD("expect:skillword", "expect skillword <words>: a perk said them over the action (M9)", expWord);

bool expBooks(ScriptCtx& c) {
  if ((int)c.game.skills.booksRead.size() < std::atoi(c.arg(2).c_str())) c.fail("expect booksread: " + std::to_string(c.game.skills.booksRead.size()));
  return true;
}
EMB_SCRIPT_CMD("expect:booksread", "expect booksread <n>: skill books read (M9)", expBooks);

bool expTrained(ScriptCtx& c) {
  const Game& g = c.game;
  const int n = g.skills.trainedLevel == (uint8_t)std::min(g.plLevel, 255) ? g.skills.trainedCount : 0;
  if (n < std::atoi(c.arg(2).c_str())) c.fail("expect trained: " + std::to_string(n) + " lessons at this level");
  return true;
}
EMB_SCRIPT_CMD("expect:trained", "expect trained <n>: levels bought from trainers at this character level (M9)", expTrained);

bool expFoes(ScriptCtx& c) {
  const std::string st = squashSk(c.arg(2));
  const int want = std::atoi(c.arg(3).c_str());
  int n = 0;
  for (const Actor& a : c.game.actors) {
    if (!a.hostile || a.st == AState::Dead) continue;
    if (st == "hurt" && a.st == AState::Hurt) n++;
    else if (st == "unaware" && !a.aggro) n++;
    else if (st == "bleeding" && a.bleedT > 0) n++;
  }
  if (n < want) c.fail("expect skillfoes " + st + ": " + std::to_string(n) + ", wanted " + std::to_string(want));
  return true;
}
EMB_SCRIPT_CMD("expect:skillfoes", "expect skillfoes hurt|unaware|bleeding <n>: living foes in that state (M9)", expFoes);
}  // namespace
