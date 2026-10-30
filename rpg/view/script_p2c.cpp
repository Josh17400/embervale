// P2c style slice test commands (rpg/view/script_p2c.cpp; SLICE-DOLL, docs/P2C_CONTRACTS.md 7, docs/PLAN.md "## P2c"). Test
// only: they set the doll's gear fields directly and stage the slice's rows. Nothing here is saved; a recalc of the hero's
// look (gear, appearance) undoes `slicegear`.
//   slicegear <tag|field=value ...>     the hero's look: a slice tag (hero guard knight archer knightm archerw: the slice's
//                                       looks-file lines over A3's hero / woman) and / or HumanLook fields by name, through the
//                                       one looks-field parser (art::doll::applyLookField: outfit armorStyle bodyForm helmet
//                                       helmStyle helmForm shield shieldStyle shieldForm weapon bladeForm polearmForm bowForm
//                                       gloves boots pauldron skirt crest plumeColor ...)
//   p2cline slice|fit|clear|face <0-3>  a row before the hero on clear ground (ids 940000+; a new row replaces the last):
//                                       slice = the six slice looks (guard knight archer knightm archerw hero); fit = mail,
//                                       plate and leather on the man and the woman; face turns the row and the hero; clear
//                                       removes the row
//   p2cline npc <people> <role> [rank] [n] [mixed]   census fighters as the game dresses them (the npc= macro), one seed each
//   expect doll gear <role>=<value>     the hero's recipe carries that gear (roles armour helm shield pauldron held sheath
//                                       back tabard, as Witness's player.sprite.doll.gear); the other `expect doll ...`
//                                       checks are P2b's (script_p2b.cpp), handed on unchanged
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "rpg/art/doll/doll_forge.h"
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/sim/gear_ladder.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"   // (envelope fix r2) View::settleBakes and draw: what `settle` waits on

namespace {

uint32_t hexc(uint32_t rgb) { return rgba((int)(rgb >> 16 & 255), (int)(rgb >> 8 & 255), (int)(rgb & 255)); }

void a3Hero(art::HumanLook& L) {
  art::HumanLook n;
  n.body = 0; n.hair = art::Hair::Short; n.hairVar = 2; n.neck = 1; n.weapon = 1; n.outfit = art::Outfit::Tunic;
  n.skin = hexc(0xE9A077); n.hairColor = hexc(0xAA4A2C); n.topColor = hexc(0x28728C); n.bottomColor = hexc(0x4F4B6B);
  n.tabardColor = hexc(0xC83E38); n.eyeColor = hexc(0x3A78B0);
  L = n;
}
void a3Woman(art::HumanLook& L) {
  art::HumanLook n;
  n.body = 1; n.hair = art::Hair::Braids; n.hairVar = 0; n.outfit = art::Outfit::Dress;
  n.skin = hexc(0xEEAA86); n.hairColor = hexc(0xC79346); n.topColor = hexc(0x922F60); n.tabardColor = hexc(0x28728C);
  n.eyeColor = hexc(0x3F8A5A);
  L = n;
}

// the slice's looks (tools/doll/slice_looks.txt: the same lines)
struct SliceLook { const char* tag; int who; const char* spec; };
const SliceLook kSlice[] = {
    {"guard", 0, "outfit=8,armorStyle=2,bodyForm=3,tabardColor=#A8322E,helmet=1,helmStyle=2,helmForm=1,shield=1,shieldStyle=3,shieldForm=2,"
                 "shieldDevice=#E8C25A,weapon=1,bladeForm=1,gloves=1,boots=1,pauldron=1,neck=0,head=1,hair=1,hairVar=1,hairColor=#4A3022,beard=1,"
                 "beardStyle=0,skin=#D9A07A"},
    {"knight", 1, "outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,"
                  "weapon=8,polearmForm=1,hair=5,hairVar=0,hairColor=#2E1E18,skin=#E8B08A"},
    {"archer", 0, "outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=3,bowForm=1,gloves=1,boots=1,neck=0,topColor=#5E6A3A,"
                  "bottomColor=#4A3A2C,hair=1,hairVar=2,hairColor=#6A4426,beard=1,beardStyle=1,skin=#B98060"},
    {"knightm", 0, "outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,helmet=1,helmStyle=3,helmForm=9,crest=3,plumeColor=#B83A32,"
                   "weapon=0,neck=0,head=4,beard=1,beardStyle=2"},
    {"archerw", 1, "outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,helmet=1,helmStyle=1,weapon=0,gloves=1,boots=1,topColor=#5E6A3A,"
                   "bottomColor=#4A3A2C,hair=3,hairVar=0,hairColor=#3A2418"},
    {"hero", 0, ""},
};
const SliceLook* sliceLook(const std::string& tag) {
  for (const SliceLook& s : kSlice) if (tag == s.tag) return &s;
  return nullptr;
}
void applySlice(art::HumanLook& L, const SliceLook& s) {
  gear::installLadderMacros();
  if (s.who) a3Woman(L); else a3Hero(L);
  art::doll::applyLookSpec(L, s.spec);
}

bool cmdSliceGear(ScriptCtx& c) {
  art::HumanLook& L = c.game.pl().look;
  for (size_t i = 1; i < c.a.size(); i++) {
    const std::string w = c.arg(i);
    if (const SliceLook* s = sliceLook(w)) { applySlice(L, *s); continue; }
    const size_t eq = w.find('=');
    if (eq == std::string::npos || !art::doll::applyLookField(L, w.substr(0, eq), w.substr(eq + 1))) { c.fail("slicegear: unknown '" + w + "'"); return true; }
  }
  return true;
}
EMB_SCRIPT_CMD("slicegear", "slicegear <guard|knight|archer|knightm|archerw|hero|field=value ...>: the hero's doll gear fields (P2c slice, test only)", cmdSliceGear);

// (P2c ladder) the ladder slice's looks (tools/doll/ladder_looks.txt: the same lines) and `ladderkit <tag|spec>`: the hero's
// look from a ladder tag, or from a spec through the looks macros (hero=1 heroine=1 kit= npc= wpn= rarity= tier= ...)
const SliceLook kLadder[] = {
    {"lt1", 0, "hero=1,kit=heartland:1:7"}, {"lt2", 0, "hero=1,kit=heartland:2:7"}, {"lt3", 0, "hero=1,kit=heartland:3:7"},
    {"lt4", 0, "hero=1,kit=heartland:4:7"}, {"lt5", 0, "hero=1,kit=heartland:5:7"}, {"lt6", 0, "hero=1,kit=heartland:6:7"},
    {"lt6m", 0, "hero=1,kit=heartland:6:7,rarity=mythic"}, {"lt6w", 1, "heroine=1,kit=heartland:6:7,wpn=spear,hairColor=#8C5A2A"},
    {"lt1w", 1, "heroine=1,kit=heartland:1:7"},
    {"jade3", 0, "kit=jade:3:1,wpn=spear,head=6,hair=1,hairVar=1,hairColor=#1E1A22,skin=#E2B58A"},
    {"imp3", 0, "kit=imperial:3:2,wpn=sword,head=2,hair=1,hairVar=0,hairColor=#3A2418,beard=1,beardStyle=2,skin=#C98E64"},
    {"fjord3", 0, "kit=fjordfolk:3:5,wpn=sword,head=4,hair=2,hairVar=1,hairColor=#B87A3E,beard=1,beardStyle=4,skin=#EAC0A0"},
    {"nband", 0, "npc=heartland:bandit:0:7"}, {"nroyal", 1, "npc=heartland:guard:2:7"}, {"nking", 0, "npc=heartland:king:0:7"},
};
const SliceLook* ladderLook(const std::string& tag) {
  for (const SliceLook& s : kLadder) if (tag == s.tag) return &s;
  return nullptr;
}
bool cmdLadderKit(ScriptCtx& c) {
  art::HumanLook& L = c.game.pl().look;
  const std::string w = c.arg(1);
  if (w.empty()) { c.fail("ladderkit <lt1..lt6|lt6m|lt6w|lt1w|jade3|imp3|fjord3|nband|nroyal|nking|spec>"); return true; }
  if (const SliceLook* s = ladderLook(w)) { applySlice(L, *s); return true; }
  gear::installLadderMacros();
  a3Hero(L);
  std::string bad;
  art::doll::applyLookSpec(L, w, nullptr, nullptr, &bad);
  if (!bad.empty()) c.fail("ladderkit: unknown " + bad);
  return true;
}
// (P2c SWITCH, C8's capital sweep) `compkit <spec>`: every companion in play dressed by a looks spec with the ladder macros
// over their own look (e.g. kit=heartland:6:3: a T6 kit; the seed moves by one per companion so no two match); test only
bool cmdCompKit(ScriptCtx& c) {
  const std::string w = c.arg(1);
  if (w.empty()) { c.fail("compkit <spec>"); return true; }
  gear::installLadderMacros();
  int n = 0;
  for (Actor& a : c.game.actors) {
    if (!a.comp || !a.human) continue;
    std::string spec = w;
    const size_t k = spec.find("kit=");
    if (k != std::string::npos) {   // the kit's seed + n
      const size_t e = spec.find(',', k), last = spec.rfind(':', e == std::string::npos ? spec.size() : e);
      if (last != std::string::npos && last > k) {
        const int seed = std::atoi(spec.substr(last + 1, (e == std::string::npos ? spec.size() : e) - last - 1).c_str()) + n;
        spec = spec.substr(0, last + 1) + std::to_string(seed) + (e == std::string::npos ? std::string() : spec.substr(e));
      }
    }
    std::string bad;
    art::doll::applyLookSpec(a.look, spec, nullptr, nullptr, &bad);
    if (!bad.empty()) { c.fail("compkit: unknown " + bad); return true; }
    n++;
  }
  if (n == 0) c.fail("compkit: no companion in play");
  return true;
}
EMB_SCRIPT_CMD("compkit", "compkit <spec>: every companion in play dressed by a looks spec with the ladder macros (P2c SWITCH, C8's sweep; test only)", cmdCompKit);
EMB_SCRIPT_CMD("ladderkit", "ladderkit <ladder tag|spec>: the hero's look from the ladder slice (lt1..lt6, lt6m, lt6w, the flavour row, the census ranks) or a looks spec with the ladder macros (P2c ladder, test only)", cmdLadderKit);

constexpr int kRowId = 940000;
std::vector<int>& foes() { static std::vector<int> v; return v; }   // (p2cline foe: the bandits spawned, removed by clear)
bool inRow(const Actor& a) { return a.id >= kRowId && a.id < kRowId + 1000; }

void spawnRow(Game& g, const std::vector<std::pair<std::string, art::HumanLook>>& people) {
  g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return inRow(a); }), g.actors.end());
  const int n = (int)people.size();
  const float pitch = 26.0f, rowW = pitch * (float)(n - 1);
  Vec2 centre = g.pl().p + Vec2(0, -2.0f * TILE);
  // (forge fix i1, the owner's village_rig2: "two guard sprites overlap") the hero stands 3.5 tiles in front of the row (2
  // tiles put his head on the middle figure's boots: a doll is 44 px tall) and half a pitch aside, so no two overlap
  const Vec2 heroAt((n & 1) ? 0.5f * pitch : 0.0f, 3.5f * TILE);
  if (!g.inside) {   // the row on open ground (no walls, props or water), the hero in front of it
    const Map& m = g.map();
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    const int half = (int)std::ceil(rowW * 0.5f / TILE) + 1;
    bool found = false;
    for (int r = 0; r <= 90 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const int cx = px + dx, cy = py + dy;
          bool clear = true;
          for (int y = cy - 3; y <= cy + 5 && clear; y++)
            for (int x = cx - half; x <= cx + half && clear; x++) {
              if (!m.in(x, y) || m.blocked(x, y) || m.propAt(x, y)) clear = false;
            }
          if (clear) { found = true; centre = Vec2(cx * TILE + 8.0f, cy * TILE + 10.0f); }
        }
    g.pl().p = centre + heroAt;
  }
  const Vec2 o = centre - Vec2(rowW * 0.5f, 0);
  for (int k = 0; k < n; k++) {
    Actor a;
    a.id = kRowId + k;
    a.human = true; a.npc = true; a.role = (Role)0; a.hostile = false;
    a.p = o + Vec2((float)k * pitch, 0);
    a.home = a.p; a.goal = a.p; a.face = 0; a.speed = 0; a.special = 0.01f;
    a.thinkT = 1.0e9f;   // (no stroll: the row keeps its spot and its facing)
    a.name = "slice " + people[(size_t)k].first;
    a.look = people[(size_t)k].second;
    g.actors.push_back(a);
  }
}

bool cmdP2cLine(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  if (w == "slice") {
    std::vector<std::pair<std::string, art::HumanLook>> row;
    for (const SliceLook& s : kSlice) { art::HumanLook L; applySlice(L, s); row.push_back({s.tag, L}); }
    spawnRow(g, row);
  } else if (w == "fit") {   // mail, plate and leather on the man and the woman (the fit pairs, STYLE_P2 26 item 3)
    std::vector<std::pair<std::string, art::HumanLook>> row;
    const char* forms[3][2] = {{"mail", "outfit=4,bodyForm=3,neck=0"}, {"plate", "outfit=5,armorStyle=3,bodyForm=7,pauldron=3,skirt=2,gloves=3,boots=3,neck=0"},
                               {"leather", "outfit=3,armorStyle=1,bodyForm=2,armsOrnament=16,gloves=1,boots=1,neck=0"}};
    for (auto& f : forms)
      for (int who = 0; who < 2; who++) {
        art::HumanLook L;
        if (who) a3Woman(L); else a3Hero(L);
        L.weapon = 0;
        art::doll::applyLookSpec(L, f[1]);
        row.push_back({std::string(f[0]) + (who ? " woman" : " man"), L});
      }
    spawnRow(g, row);
  } else if (w == "ladder" || w == "flavour" || w == "ranks") {   // (P2c ladder) T1..T6 and the Mythic; the T3 peoples; rank to tier
    static const char* const kRow[3][8] = {{"lt1", "lt2", "lt3", "lt4", "lt5", "lt6", "lt6m", nullptr},
                                           {"lt3", "jade3", "imp3", "fjord3", nullptr},
                                           {"nband", "nroyal", "nking", "lt6", "lt6w", nullptr}};
    const int k = w == "ladder" ? 0 : w == "flavour" ? 1 : 2;
    std::vector<std::pair<std::string, art::HumanLook>> row;
    for (int i = 0; i < 8 && kRow[k][i]; i++) { art::HumanLook L; applySlice(L, *ladderLook(kRow[k][i])); row.push_back({kRow[k][i], L}); }
    spawnRow(g, row);
  } else if (w == "npc") {
    // (polish i2) p2cline npc <people> <role> [rank 0-2] [n] [mixed]: census fighters as the game makes them (the role's
    // base as Game::makeLook sets it, weapon included, then census::dress and the rank's tier: the looks macro npc=), one
    // seed a man, so each wears his own kit (P2b's `lineup` dresses a bare look: no weapon in hand)
    const std::string people = c.arg(2), role = c.arg(3);
    const int rank = c.a.size() > 4 ? std::clamp(std::atoi(c.arg(4).c_str()), 0, 2) : 0;
    const int n = c.a.size() > 5 ? std::clamp(std::atoi(c.arg(5).c_str()), 1, 12) : 6;
    const bool mixed = c.a.size() > 6 && c.arg(6) == "mixed";
    std::vector<std::pair<std::string, art::HumanLook>> row;
    for (int k = 0; k < n; k++) {
      art::HumanLook L;
      L.body = (uint8_t)(mixed && (k & 1) ? 1 : 0);
      std::string bad;
      gear::applyLadderSpec(L, "npc=" + people + ":" + role + ":" + std::to_string(rank) + ":" + std::to_string(k + 1), nullptr, nullptr, &bad);
      if (!bad.empty()) { c.fail("p2cline npc <people> <role> [rank] [n] [mixed]: bad " + bad); return true; }
      row.push_back({people + " " + role, L});
    }
    spawnRow(g, row);
  } else if (w == "face") {
    const int f = std::clamp(std::atoi(c.arg(2).c_str()), 0, 3);
    for (Actor& a : g.actors) if (inRow(a)) a.face = (uint8_t)f;
    g.pl().face = (uint8_t)f;   // (the hero too: `pose` frames in every facing)
  } else if (w == "foe") {   // a hostile bandit three tiles east of the hero (weapon: an art id, default its own)
    const int wpn = c.a.size() > 2 ? std::atoi(c.arg(2).c_str()) : -1;
    const int i = AnimOps::spawnBandit(g, g.pl().p + Vec2(3.0f * TILE, 0), wpn);
    if (i < 0) { c.fail("p2cline foe: no bandit spawned"); return true; }
    foes().push_back(g.actors[(size_t)i].id);
  } else if (w == "clear") {
    std::vector<int>& f = foes();
    g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(),
                                  [&](const Actor& a) { return inRow(a) || std::find(f.begin(), f.end(), a.id) != f.end(); }), g.actors.end());
    f.clear();
  } else c.fail("p2cline slice|fit|ladder|flavour|ranks|face <0-3>|foe [weapon]|clear");
  return true;
}
EMB_SCRIPT_CMD("p2cline", "p2cline slice|fit|ladder|flavour|ranks|npc <people> <role> [rank] [n] [mixed]|face <0-3>|foe [weapon]|clear: the P2c slice's rows before the hero (test only)", cmdP2cLine);

// `expect doll gear <role>=<value>`; every other `expect doll ...` goes on to P2b's check (script_p2b.cpp)
bool expDollGear(ScriptCtx& c);
bool otherExpDoll(ScriptCtx& c) {
  for (const ScriptExt& e : scriptExts())
    if (!std::strcmp(e.name, "expect:doll") && e.fn != &expDollGear) return e.fn(c);
  c.fail("expect doll: P2b's check is not registered");
  return true;
}
std::string gearOf(const art::HumanLook& L, const std::string& role) {
  const art::doll::DollRecipe r = art::doll::resolve(L);
  std::string out;
  for (const art::doll::Need& n : r.needs) {
    std::string k;
    switch (n.role) {
      case art::doll::Role::Armour: k = "armour"; break;
      case art::doll::Role::Helm: k = "helm"; break;
      case art::doll::Role::Shield: k = n.value == "arm" ? "" : "shield"; break;
      case art::doll::Role::Pauldron: k = "pauldron"; break;
      case art::doll::Role::Held: k = "held"; break;
      case art::doll::Role::Overlay: k = n.value == "tabard" ? "tabard" : ""; break;
      case art::doll::Role::Sheath:
        k = (n.value == "quiver" || n.value == "bowback" || n.value == "greatsword" || n.value == "staffback" || n.value == "shieldback") ? "back" : "sheath";
        break;
      default: break;
    }
    if (k == role) out += (out.empty() ? "" : "+") + n.value;
  }
  return out;
}
bool expDollGear(ScriptCtx& c) {
  // (P2c ladder) `expect doll tier <n>`: the hero's highest piece's tier (HumanLook::topTier)
  if (c.arg(2) == "tier") {
    const int want = std::atoi(c.arg(3).c_str()), have = c.game.pl().look.topTier();
    if (have != want) c.fail("expect doll tier " + c.arg(3) + ": the hero's top tier is " + std::to_string(have));
    return true;
  }
  // (P2c forge) `expect doll forge <base>` or `expect doll forge <axis>=<value>`: the hero's body forge style (the cut as the
  // doll draws it, on its armour's family; crest the helm's)
  if (c.arg(2) == "forge") {
    namespace F = art::doll::forge;
    const art::HumanLook& L = c.game.pl().look;
    const art::doll::DollRecipe r = art::doll::resolve(L);
    const std::string want = c.arg(3);
    const size_t eq = want.find('=');
    std::string have;
    if (eq == std::string::npos) have = L.forge[art::HumanLook::TIER_BODY] ? F::valueName(F::AX_BASE, r.forgeAx[0][F::AX_BASE]) : "none";
    else {
      const int a = F::axisOfName(want.substr(0, eq).c_str());
      if (a < 1) { c.fail("expect doll forge <base>|<axis>=<value> (shoulder crest cut edge collar limb fit)"); return true; }
      const int v = a == F::AX_CREST ? r.forgeAx[art::HumanLook::TIER_HELM][a] : r.forgeAx[art::HumanLook::TIER_BODY][a];
      have = v ? F::valueName(a, v) : "none";
    }
    const std::string value = eq == std::string::npos ? want : want.substr(eq + 1);
    if (have != value) c.fail("expect doll forge " + want + ": the hero's is '" + have + "'");
    return true;
  }
  // (P2c envelope) `expect doll envelope <field>=<value>`: tier, shoulders, rows (the helm over the crown), area (noHeld px)
  if (c.arg(2) == "envelope") {
    const std::string want = c.arg(3);
    const size_t eq = want.find('=');
    if (eq == std::string::npos) { c.fail("expect doll envelope <tier|shoulders|rows|area>=<n>"); return true; }
    const art::doll::EnvelopeMeasure m = art::doll::measureEnvelope(c.game.pl().look);
    const std::string f = want.substr(0, eq);
    const int v = std::atoi(want.c_str() + eq + 1);
    const int have = f == "tier" ? m.tier : f == "shoulders" ? m.shoulders : f == "rows" ? m.rows : f == "area" ? m.area : -9999;
    if (have == -9999) c.fail("expect doll envelope: unknown field " + f + " (tier shoulders rows area)");
    else if (have != v) c.fail("expect doll envelope " + want + ": the hero's is " + std::to_string(have));
    return true;
  }
  if (c.arg(2) != "gear") return otherExpDoll(c);
  const std::string want = c.arg(3);
  const size_t eq = want.find('=');
  if (eq == std::string::npos) { c.fail("expect doll gear <role>=<value> (armour helm shield pauldron held sheath back tabard)"); return true; }
  const std::string role = want.substr(0, eq), value = want.substr(eq + 1);
  const std::string have = gearOf(c.game.pl().look, role);
  // a `back` of several items: any of them; "-" or "none": nothing in that role
  bool ok = have == value || ((value == "-" || value == "none") && have.empty());
  if (!ok && have.find('+') != std::string::npos) {
    size_t a = 0;
    while (a <= have.size() && !ok) { size_t b = have.find('+', a); if (b == std::string::npos) b = have.size(); ok = have.substr(a, b - a) == value; a = b + 1; }
  }
  if (!ok) c.fail("expect doll gear " + want + ": the hero's " + role + " is '" + have + "'");
  return true;
}
// registered first among the `expect:doll` checks (the registry takes the first match): moved ahead of P2b's at start-up
int registerGear() {
  const int n = registerScriptExt(ScriptExt{"expect:doll", "expect doll gear <role>=<value>: the hero's doll gear (P2c); covered|missing: P2b's check", expDollGear});
  std::vector<ScriptExt>& v = scriptExts();
  for (size_t i = 0; i + 1 < v.size(); i++)
    if (!std::strcmp(v[i].name, "expect:doll") && v[i].fn != &expDollGear) { std::swap(v[i], v.back()); break; }
  return n;
}
const int kGearReg = registerGear();

// (envelope fix round, D65; the packager: phone_NEAR_t1 showed a square block of forest beside the hero) a capture after
// `skill clearing` waits for the world to be still: the terrain chunks under the cleared props are re-baked in the background
// and a stale chunk stays on screen until its bake lands (terrain.cpp), so the shot 0.8 s after the clearing caught the old
// canopy floor in one chunk's square. settle [max s] [gap s]: every `gap` seconds (0.5) the world is drawn (the hero hidden,
// the UI as set; a draw takes the finished bakes in and queues what it newly needs) after View::settleBakes; the world is
// still when two such draws `gap` apart both found no terrain bake in flight, `max` seconds at most (3). It never fails the
// script. (envelope fix r2, DOLL; the judge's round 4: r1 compared whole canvases, and a living scene (the grass and flowers
// swaying, the critters, the water) is never pixel-still, so every settle timed out at 3.0 s; the bakes are what a capture
// after `skill clearing` waits for, so the bakes are what it now asks after.) Logs what it waited
bool cmdSettle(ScriptCtx& c) {
  static int line = -1;
  static float last = -1;
  static int idleRuns = 0;
  const float maxS = c.arg(1).empty() ? 3.0f : (float)std::atof(c.arg(1).c_str());
  const float gap = c.arg(2).empty() ? 0.5f : (float)std::atof(c.arg(2).c_str());
  if (c.waited == 0 || line != c.line) { line = c.line; last = -1; idleRuns = 0; }
  if (last >= 0 && c.waited - last < gap) return false;
  last = c.waited;
  // settleBakes returns at once when no bake is queued or being painted, and sleeps 1 ms a turn while one is: a return in
  // under half a millisecond is a world with nothing in flight
  const auto t0 = std::chrono::steady_clock::now();
  c.view.settleBakes();
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  const bool wasHidden = c.view.p2a.hidePlayer;
  c.view.p2a.hidePlayer = true;
  c.view.draw(c.game, false);
  c.view.p2a.hidePlayer = wasHidden;
  idleRuns = ms < 0.5 ? idleRuns + 1 : 0;
  if (idleRuns >= 2) { std::printf("settle: still after %.1f s (no terrain bake in flight on two draws %.1f s apart)\n", c.waited, gap); return true; }
  if (c.waited >= maxS) { std::printf("settle: not still after %.1f s (shooting anyway; last wait on the bakes %.1f ms)\n", c.waited, ms); return true; }
  return false;
}
// (P2c SWITCH, C7) `expect menu ...` on the menu figures drawn last frame (View::menuFigs; Witness state.menus):
//   expect menu v1 <n>                  people frames from v1 sheets last frame (View::v1Draws: world and menus) == n
//   expect menu <where> doll            the figure at <where> (paperdoll creator hero party dialogue portrait loot) came from
//                                       the doll (rig 2) at a whole multiple
//   expect menu <where> uncropped       ... and stands inside its stage
//   expect menu <where> scale <n>       ... at that multiple
//   expect menu <where> aura <n>        no finish light further than n px off the figure's silhouette (the Mythic's flame, C7)
//   expect menu <where> lights          the living finish drew on it (T5-T6)
bool expMenu(ScriptCtx& c) {
  const std::string what = c.arg(2), test = c.arg(3);
  if (what == "v1") {
    const int want = std::atoi(test.c_str());
    if (c.view.v1Draws != want) c.fail("expect menu v1 " + test + ": " + std::to_string(c.view.v1Draws) + " people frames from v1 sheets last frame");
    return true;
  }
  static const char* const kWhere[] = {"paperdoll", "creator", "hero", "party", "dialogue", "portrait", "loot"};
  int where = -1;
  for (int i = 0; i < View::kMenuWhereN; i++) if (what == kWhere[i]) where = i;
  if (where < 0) { c.fail("expect menu <paperdoll|creator|hero|party|dialogue|portrait|loot|v1> ..."); return true; }
  const View::MenuFigure* f = nullptr;
  for (const View::MenuFigure& m : c.view.menuFigs) if (m.where == where) { f = &m; break; }
  if (!f) { c.fail("expect menu " + what + ": no figure drawn there last frame"); return true; }
  const std::string at = " (rig " + std::to_string(f->rig) + ", " + std::to_string(f->scale) + "x, " + std::to_string(f->w) + "x" + std::to_string(f->h) + ")";
  if (test == "doll" || test.empty()) { if (f->rig < 2 || f->scale < 1) c.fail("expect menu " + what + " doll: drawn from a v1 sheet" + at); }
  else if (test == "uncropped") { if (f->rig < 2 || f->cropped) c.fail("expect menu " + what + " uncropped: the figure leaves its stage" + at); }
  else if (test == "scale") { if (f->scale != std::atoi(c.arg(4).c_str())) c.fail("expect menu " + what + " scale " + c.arg(4) + ": drawn" + at); }
  else if (test == "aura") { if (f->auraOut > std::atoi(c.arg(4).c_str())) c.fail("expect menu " + what + " aura " + c.arg(4) + ": a light " + std::to_string(f->auraOut) + " px off the silhouette" + at); }
  else if (test == "lights") { if (f->lights <= 0) c.fail("expect menu " + what + " lights: no living finish drawn" + at); }
  else c.fail("expect menu <where> doll|uncropped|scale <n>|aura <n>|lights");
  return true;
}
const int kMenuReg = registerScriptExt(ScriptExt{"expect:menu", "expect menu v1 <n> | <where> doll|uncropped|scale <n>|aura <n>|lights: the menu figures drawn last frame (P2c SWITCH, C7)", expMenu});

// (P2c SWITCH) `drawframe`: one frame drawn now (Witness steps draw only for a shot: state.menus, the v1 count and the
// hero's sprite are what the last drawn frame held); `creatoropen`: the character creator over the running game, as New
// Adventure opens it (Game::beginCreator; test only)
bool cmdDrawFrame(ScriptCtx& c) { c.view.draw(c.game, false); return true; }
const int kDrawFrameReg = registerScriptExt(ScriptExt{"drawframe", "drawframe: draw one frame now (Witness: state.menus, v1Draws read the last drawn frame; P2c SWITCH, test only)", cmdDrawFrame});
// `female 1|0`: the hero's body the woman's or the man's (as Witness's reset param female; the gear kept, the look redressed)
bool cmdFemale(ScriptCtx& c) { c.game.app.female = c.arg(1) != "0"; AnimOps::refreshHero(c.game); return true; }
const int kFemaleReg = registerScriptExt(ScriptExt{"female", "female 1|0: the hero's body the woman's or the man's, worn gear kept (P2c SWITCH, test only)", cmdFemale});
// (P2c flip, R9) `loadsave <file>`: a save file read through Game::deserialize, as CONTINUE does (the game in play, the
// view snapped to it). It fails the script when the file cannot be read or is refused, and it never writes (a scripted
// run has no save path; test only)
bool cmdLoadSave(ScriptCtx& c) {
  const std::string path = c.rest(1);
  if (path.empty()) { c.fail("loadsave <file>"); return true; }
  std::vector<uint8_t> buf;
  if (FILE* f = std::fopen(path.c_str(), "rb")) {
    uint8_t chunk[65536];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) buf.insert(buf.end(), chunk, chunk + n);
    std::fclose(f);
  } else {
    c.fail("loadsave " + path + ": cannot open the file");
    return true;
  }
  if (buf.empty() || !c.game.deserialize(buf)) { c.fail("loadsave " + path + ": the save was refused (" + std::to_string(buf.size()) + " bytes)"); return true; }
  c.game.mode = Mode::Play;
  c.view.snap(c.game);
  std::printf("loadsave: %s loaded (%zu bytes)\n", path.c_str(), buf.size());
  return true;
}
const int kLoadSaveReg = registerScriptExt(ScriptExt{"loadsave", "loadsave <file>: read a save through Game::deserialize as CONTINUE does; fails on a refused file, never writes (P2c flip, test only)", cmdLoadSave});
bool cmdCreatorOpen(ScriptCtx& c) { c.game.beginCreator(); return true; }
const int kCreatorReg = registerScriptExt(ScriptExt{"creatoropen", "creatoropen: the character creator over the game, as New Adventure opens it (P2c SWITCH, test only)", cmdCreatorOpen});

const int kSettleReg = registerScriptExt(ScriptExt{"settle", "settle [max s] [gap s]: wait until two world draws gap s apart (0.5) find no terrain bake in flight, max s at most (3), the hero hidden (D65: a capture after `skill clearing`; test only)", cmdSettle, 10.0f});

}  // namespace
