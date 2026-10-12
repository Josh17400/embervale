// M8b Treasure script commands (POWERS lane): stage the worn powers for screenshots and checks (VISION_PLAN 15.27).
//   power give <name>: the worn Legendary / Mythic piece (the weapon first, then any equipped piece) carries that power
//        (name: the power's words without spaces, any case: tempest, inferno, soulfeast, bramblecrown, chronos, phoenix,
//        starfall, wolfking, chainlightning, firetrail ...); the hero's gear is summed again
//   power affix <name> <value>: the worn weapon's first affix becomes that one (name: the enum word: burn, vsbeasts,
//        attackspeed, healthregen ...)
//   power proc <name>: fire that power now where the hero stands (the nearest foe its target): a chain, patches, a star,
//        a wolf, the phoenix's burst, the time slow, the aura's sting, lifesteal motes
//   power ready: every power's cooldown ready (Phoenix, Chronos, Shadowstep, Starfall)
//   power foes <monster> <n> <level> [tough]: n monsters of that level in a ring round the hero, already hunting
//        (tough: ten times their health, so they live through a staged fight)
//   power arena: step out to open country near by (no town, no walls within a few tiles): a clear stage
//   expect power <name>: the hero wears that power
//   expect powerfx <lightning|firepatch|burn|thorns|timeslow|lifesteal|phoenix|star|wolf|proc> <n>: at least n of them fired
//   expect wolves <n>: at least n spectral wolves in play (n 0: none)
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/power_ops.h"
#include "rpg/sim/powers.h"
#include "rpg/story/dsl.h"
#include "rpg/view/script_api.h"

namespace {

std::string squash(const std::string& s) {
  std::string r;
  for (char ch : s) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\'') continue;
    r += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  }
  return r;
}

Unique uniqueWord(const std::string& w) {
  const std::string want = squash(w);
  for (int u = 1; u < (int)Unique::COUNT; u++) if (squash(uniqueInfo((Unique)u).name) == want) return (Unique)u;
  // the enum words too (uniqueInfo's names are the item words: "REFLECTION" for ReflectBlock...)
  static const char* en[] = {"", "chainlightning", "firetrail", "spectralwolf", "splitarrows", "timeslow", "reflectblock", "bloodmagic", "stillcrit",
                             "executioner", "bulwark", "vampire", "frostnova", "thunderclap", "goldentouch", "windwalker", "berserker", "ward",
                             "huntsman", "gravebane", "dragonbane", "ember", "rime", "echo", "secondwind", "stormcaller", "shadowstep",
                             "thornmail", "quickdraw", "arcanist", "lifebloom", "warcry", "ironhide", "wayfarer", "tempest", "inferno",
                             "soulfeast", "bramblecrown", "chronos", "phoenix", "starfall", "wolfking"};
  for (int u = 1; u < (int)Unique::COUNT && u < (int)(sizeof en / sizeof en[0]); u++) if (want == en[u]) return (Unique)u;
  return Unique::None;
}

Affix affixWord(const std::string& w) {
  static const char* en[] = {"", "firedmg", "frostdmg", "shadowdmg", "lifedrain", "critchance", "critdmg", "staminaregen", "movespeed", "rolldist",
                             "thorns", "resfire", "resfrost", "resshadow", "spellpower", "magicka", "potioneffect", "goldfind", "health",
                             "vsbeasts", "vsundead", "vspeople", "burn", "attackspeed", "armourpct", "healthregen", "stamina"};
  const std::string want = squash(w);
  for (int a = 1; a < (int)Affix::COUNT && a < (int)(sizeof en / sizeof en[0]); a++) if (want == en[a]) return (Affix)a;
  return Affix::None;
}

int fxWord(const std::string& w) {
  static const char* n[] = {"lightning", "firepatch", "burn", "thorns", "timeslow", "lifesteal", "phoenix", "star", "wolf", "proc"};
  const std::string want = squash(w);
  for (int i = 0; i < (int)powers::Fx::COUNT; i++) if (want == n[i]) return i;
  return -1;
}

Item* wornPiece(Game& g) {
  for (int idx : {g.eqWeapon, g.eqRing, g.eqAmulet, g.eqArmor, g.eqHelmet, g.eqShield, g.eqGloves, g.eqBoots, g.eqCloak, g.eqBow, g.eqStaff})
    if (idx >= 0 && idx < (int)g.inv.size() && g.inv[(size_t)idx].rarity >= Rarity::Legendary) return &g.inv[(size_t)idx];
  if (g.eqWeapon >= 0 && g.eqWeapon < (int)g.inv.size()) return &g.inv[(size_t)g.eqWeapon];
  return nullptr;
}

bool cmdPower(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = squash(c.arg(1));
  if (what == "give") {
    const Unique u = uniqueWord(c.rest(2));
    if (u == Unique::None) { c.fail("power give: unknown power '" + c.rest(2) + "'"); return true; }
    Item* it = wornPiece(g);
    if (!it) { c.fail("power give: nothing worn (gear6 <kind> rarity 5 equip first)"); return true; }
    it->unique = u;
    PowerOps::recalc(g);
    return true;
  }
  if (what == "affix") {
    const Affix a = affixWord(c.arg(2));
    if (a == Affix::None) { c.fail("power affix: unknown affix '" + c.arg(2) + "'"); return true; }
    if (g.eqWeapon < 0 || g.eqWeapon >= (int)g.inv.size()) { c.fail("power affix: no weapon worn"); return true; }
    g.inv[(size_t)g.eqWeapon].affix[0].kind = a;
    g.inv[(size_t)g.eqWeapon].affix[0].value = (int16_t)std::atoi(c.arg(3).c_str());
    PowerOps::recalc(g);
    return true;
  }
  if (what == "proc") {
    const Unique u = uniqueWord(c.rest(2));
    if (u == Unique::None) { c.fail("power proc: unknown power '" + c.rest(2) + "'"); return true; }
    if (!PowerOps::proc(g, u)) c.fail("power proc: " + c.rest(2) + " could not fire here (no foe near?)");
    return true;
  }
  if (what == "ready") {
    powers::State& F = g.craft.live.fx;
    F.phoenixCd = F.chronosCd = F.shadowCd = F.starCd = F.tempestCd = 0;
    return true;
  }
  if (what == "foes") {
    const int m = story::dsl::monsterWord(c.arg(2));
    if (m < 0 || m >= (int)art::Monster::COUNT) { c.fail("power foes: unknown monster '" + c.arg(2) + "'"); return true; }
    const int n = std::max(1, std::min(12, std::atoi(c.arg(3).c_str())));
    const int lv = std::max(1, std::min(60, std::atoi(c.arg(4).c_str())));
    const bool tough = squash(c.arg(5)) == "tough";
    for (int k = 0; k < n; k++) {
      const float an = 6.2831853f * (float)k / (float)n + 0.4f;
      const int id = g.debugSpawnAt((art::Monster)m, g.pl().p + Vec2(std::cos(an) * 46.0f, std::sin(an) * 34.0f), lv);
      for (Actor& a : g.actors)
        if (a.id == id && tough) { a.maxHp *= 10.0f; a.hp = a.maxHp; }
    }
    return true;
  }
  if (what == "arena") {
    const int32_t sx = g.world.ox + (int32_t)(g.pl().p.x / TILE), sy = g.world.oy + (int32_t)(g.pl().p.y / TILE);
    static const int off[][2] = {{60, 0}, {-60, 0}, {0, 60}, {0, -60}, {50, 50}, {-50, 50}, {50, -50}, {-50, -50}, {90, 0}, {-90, 0}, {0, 90}, {0, -90}};
    for (const auto& o : off) {
      g.teleportGlobal(sx + o[0], sy + o[1]);
      bool bad = g.settlementAt(g.pl().p) >= 0;
      for (int dx = -9; dx <= 9 && !bad; dx++)
        for (int dy = -6; dy <= 6 && !bad; dy++) {
          const int tx = (int)(g.pl().p.x / TILE) + dx, ty = (int)(g.pl().p.y / TILE) + dy;
          if (g.map().blocked(tx, ty)) bad = true;
        }
      if (!bad) return true;
    }
    return true;   // (the last try stands: still a stage, if a busier one)
  }
  c.fail("power give|affix|proc|ready|foes|arena ...");
  return true;
}
EMB_SCRIPT_CMD("power", "power give <name> | affix <name> <v> | proc <name> | ready: stage the worn powers (M8b)", cmdPower);

bool expPower(ScriptCtx& c) {
  const Unique u = uniqueWord(c.rest(2));
  if (u == Unique::None || !c.game.craft.live.stats.has(u)) c.fail("expect power: the hero does not wear '" + c.rest(2) + "'");
  return true;
}
EMB_SCRIPT_CMD("expect:power", "expect power <name>: the hero wears that power (M8b)", expPower);

bool expPowerFx(ScriptCtx& c) {
  const int k = fxWord(c.arg(2));
  if (k < 0) { c.fail("expect powerfx: unknown kind '" + c.arg(2) + "'"); return true; }
  const int want = std::atoi(c.arg(3).c_str());
  const int have = c.game.craft.live.fx.procs[k];
  if (have < want) c.fail("expect powerfx " + c.arg(2) + ": " + std::to_string(have) + " fired (want " + std::to_string(want) + ")");
  return true;
}
EMB_SCRIPT_CMD("expect:powerfx", "expect powerfx <kind> <n>: at least n power effects of that kind fired (M8b)", expPowerFx);

bool expWolves(ScriptCtx& c) {
  const int want = std::atoi(c.arg(2).c_str());
  int n = 0;
  for (const Actor& a : c.game.actors) if (a.powTag == powers::PT_WOLF && a.st != AState::Dead) n++;
  if (want == 0 ? n != 0 : n < want) c.fail("expect wolves: " + std::to_string(n) + " spectral wolves (want " + std::to_string(want) + ")");
  return true;
}
EMB_SCRIPT_CMD("expect:wolves", "expect wolves <n>: at least n spectral wolves in play (0: none) (M8b)", expWolves);

}  // namespace
