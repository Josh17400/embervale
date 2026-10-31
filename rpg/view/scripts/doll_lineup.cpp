// P2b New People test commands (VISION_PLAN 15.36 phase A item 5; the phase A harness, frozen: areas report a harness
// problem instead of editing it). Every command is also a Witness call (rpg/main.cpp lists the registered commands).
//   camhold 1|0                                  the camera stays where it is (a body crosses still ground: `witness
//                                                measure motion` reads the walk's registration); 0 lets it follow again
//   lineup <archetype> <role> [n] [female|male|mixed] [child|youth|adult|elder]
//                                                a row of n (default 6, at most 16) census people of that people and role,
//                                                dressed by census::dress (cult::Atlas::make of the archetype), on the
//                                                nearest clear ground facing the camera, the hero two tiles south of its
//                                                middle; a new lineup replaces the last
//                                                (archetype: fjordfolk highland heartland imperial dune steppe marsh jade
//                                                river suntemple sylvan starspire; role: villager guard merchant smith ...)
//   dollpart <part> <D|U|R|L> [key] | dollpart off   the hero drawn as that one doll part alone at rig 2 (doll_gallery
//                                                names the parts); off: the whole figure again
//   expect doll covered|missing [<role>=<value>]   whether the doll covers the hero's look (LOCO and DEATH; ACTION when
//                                                armed: art::rig2Covers); with a value, that it is (or is not) among the
//                                                missing ("expect doll missing hair=tousled")
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/art/doll/doll.h"
#include "rpg/culture/culture.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

struct P2bScriptOps {
  static Pix* pix(View& v) { return v.pix_; }
  static std::unordered_map<uint64_t, View::RigEntry>& cache(View& v) { return v.rigTex_; }
  static float now(const View& v) { return v.t_; }
};

// the `dollpart` frame: composed by the doll for the hero's recipe, baked once per (look, part, view, key, pool)
View::RigTex View::dollPartTex(const art::HumanLook& L) {
  RigTex r;
  if (p2b.dollPart.empty() || !pix_) return r;
  uint64_t k = 0xD0119A47ull ^ L.key() * 0x9E3779B97F4A7C15ull ^ (uint64_t)p2b.dollFacing << 3 ^ (uint64_t)art::doll::poolGeneration() << 40;
  for (char ch : p2b.dollPart + "|" + p2b.dollKey) k = (k ^ (uint8_t)ch) * 1099511628211ull;
  auto it = rigTex_.find(k);
  if (it == rigTex_.end()) {
    RigEntry e;
    e.sheet = art::doll::composePart(art::doll::resolve(L), p2b.dollPart, p2b.dollFacing, p2b.dollKey);
    if (!e.sheet.empty() && e.sheet.atlas.w > 0) e.tex = pix_->bake(e.sheet.atlas);
    e.sheet.atlas = Canvas();
    it = rigTex_.emplace(k, std::move(e)).first;
  }
  it->second.used = t_;
  r.tex = &it->second.tex;
  r.sheet = &it->second.sheet;
  return r;
}

namespace {

std::string lower(std::string s) { for (char& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; }
std::string key(const std::string& s) { std::string k; for (char ch : lower(s)) if (ch != ' ' && ch != '-' && ch != '_') k += ch; return k; }

int archOf(const std::string& w) {
  for (int a = 0; a < (int)cult::Archetype::COUNT; a++)
    if (key(cult::archetypeName((cult::Archetype)a)) == key(w)) return a;
  return -1;
}
int roleOf(const std::string& w) {
  static const char* n[] = {"villager", "guard", "merchant", "smith", "innkeeper", "priest", "jarl", "farmer", "child", "mage", "bandit", "king",
                            "hunter", "fisher", "herbalist", "traveller", "herald", "soldier", "captain", "refugee", "harbourmaster", "shipwright",
                            "sailor", "pirate", "steward", "warlord", "envoy"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Role::COUNT, "a name for every role");
  for (int r = 0; r < (int)Role::COUNT; r++) if (key(w) == n[r]) return r;
  return -1;
}

// (the reset param camera:"hold" sets the same flag at a Witness reset: rpg/main.cpp; `camhold 0` lets it go mid-run)
bool cmdCamHold(ScriptCtx& c) {
  c.view.p2b.camHold = c.arg(1).empty() || c.arg(1) != "0";
  return true;
}
EMB_SCRIPT_CMD("camhold", "camhold 1|0: the camera stays where it is (P2b: witness measure motion on a body crossing still ground)", cmdCamHold);

bool cmdLineup(ScriptCtx& c) {
  Game& g = c.game;
  const int arch = archOf(c.arg(1));
  const int role = roleOf(c.arg(2));
  if (arch < 0 || role < 0) { c.fail("lineup <archetype> <role> [n] [female|male|mixed] [child|youth|adult|elder]"); return true; }
  int n = 6;
  std::string sex = "mixed", age = "adult";
  for (size_t i = 3; i < c.a.size(); i++) {
    const std::string w = lower(c.arg(i));
    if (w == "female" || w == "male" || w == "mixed") sex = w;
    else if (w == "child" || w == "youth" || w == "adult" || w == "elder") age = w;
    else if (std::atoi(w.c_str()) > 0) n = std::clamp(std::atoi(w.c_str()), 1, 16);
    else { c.fail("lineup: unknown word '" + c.arg(i) + "'"); return true; }
  }
  // a fresh lineup replaces the last one (its people carry ids from 910000: the game's own never get there)
  g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return a.id >= 910000 && a.id < 920000; }), g.actors.end());
  const cult::Culture C = cult::Atlas::make((cult::Archetype)arch, 4242u + (uint32_t)arch * 97u);
  // a clear patch of ground for the row (no wall, prop or tree in front of anyone), the nearest within 40 tiles; the hero
  // steps two tiles south of the row's middle, so the camera frames them all
  const float pitch = 22.0f, rowW = pitch * (float)(n - 1);
  Vec2 centre = g.pl().p + Vec2(26.0f + rowW * 0.5f, 0);
  if (!g.inside) {
    const Map& m = g.map();
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    const int half = (int)std::ceil(rowW * 0.5f / TILE) + 1;
    bool found = false;
    for (int r = 0; r <= 40 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const int cx = px + dx, cy = py + dy;
          bool clear = true;
          for (int y = cy - 3; y <= cy + 3 && clear; y++)
            for (int x = cx - half; x <= cx + half && clear; x++) {
              if (!m.in(x, y) || m.blocked(x, y)) { clear = false; break; }
              const int pr = m.propAt(x, y);
              if (pr && art::isTreeProp((art::Prop)(pr - 1))) clear = false;
            }
          if (clear) { found = true; centre = Vec2(cx * TILE + 8.0f, cy * TILE + 10.0f); }
        }
    g.pl().p = centre + Vec2(0, 2.0f * TILE);
  }
  const Vec2 o = centre - Vec2(rowW * 0.5f, 0);
  for (int k = 0; k < n; k++) {
    Actor a;
    a.id = 910000 + k;
    a.human = true; a.npc = true; a.role = (Role)role; a.hostile = false;
    a.p = o + Vec2((float)k * pitch, 0);
    a.home = a.p; a.goal = a.p; a.face = 0; a.speed = 0; a.special = 0.01f;
    const bool female = sex == "female" || (sex == "mixed" && (k & 1));
    census::dress(a.look, a.name, (Role)role, female, C, C, 0x11E0Bull * (uint64_t)(k + 1) ^ (uint64_t)arch << 32 ^ (uint64_t)role << 40, census::Rank{});
    a.look.age = age == "child" ? 1 : age == "youth" ? 2 : age == "elder" ? 3 : 0;
    if (a.look.age == 1 || a.look.age == 2) a.look.beard = false;
    a.name = std::string(cult::archetypeName(C.archetype)) + " " + lower(c.arg(2));
    g.actors.push_back(a);
  }
  return true;
}
EMB_SCRIPT_CMD("lineup", "lineup <archetype> <role> [n] [female|male|mixed] [child|youth|adult|elder]: a row of census people on clear ground before the hero (P2b)", cmdLineup);

bool cmdDollPart(ScriptCtx& c) {
  View::P2bHooks& h = c.view.p2b;
  const std::string w = lower(c.arg(1));
  if (w == "off" || w.empty()) { h.dollPart.clear(); return true; }
  const std::string f = c.arg(2);
  const int facing = f == "D" || f == "d" ? 0 : f == "U" || f == "u" ? 1 : f == "R" || f == "r" ? 2 : f == "L" || f == "l" ? 3 : -1;
  if (facing < 0) { c.fail("dollpart <part> <D|U|R|L> [key] | dollpart off"); return true; }
  if (art::doll::findPart(c.arg(1)) < 0) { c.fail("dollpart: no part " + c.arg(1) + " in the pool"); return true; }
  h.dollPart = c.arg(1); h.dollFacing = facing; h.dollKey = c.a.size() > 3 ? c.arg(3) : "rest";
  return true;
}
EMB_SCRIPT_CMD("dollpart", "dollpart <part> <D|U|R|L> [key] | dollpart off: the hero drawn as one doll part alone at rig 2 (P2b)", cmdDollPart);

bool expDoll(ScriptCtx& c) {
  const art::HumanLook& L = c.game.pl().look;
  const std::string w = lower(c.arg(2));
  if (w != "covered" && w != "missing") { c.fail("expect doll covered|missing [<role>=<value>]"); return true; }
  const bool covered = art::rig2Covers(L);
  if (c.a.size() > 3) {
    const std::string want = lower(c.arg(3));
    bool found = false;
    std::string have;
    for (const art::doll::Missing& m : art::doll::dollMissing(L)) {
      const std::string id = std::string(art::doll::roleName(m.role)) + "=" + m.value;
      if (have.size() < 200) have += (have.empty() ? "" : ", ") + id;
      found = found || id == want;
    }
    if (found != (w == "missing")) c.fail("expect doll " + c.rest(2) + ": the missing are [" + have + "]");
    return true;
  }
  if (covered != (w == "covered")) {
    std::string first;
    for (const art::doll::Missing& m : art::doll::dollMissing(L)) { first = m.what(); break; }
    c.fail("expect doll " + w + ": the hero's look is " + (covered ? "covered" : "not covered (" + first + ")"));
  }
  return true;
}
EMB_SCRIPT_CMD("expect:doll", "expect doll covered|missing [<role>=<value>]: the doll covers the hero's look, or names a value missing (P2b)", expDoll);

}  // namespace
