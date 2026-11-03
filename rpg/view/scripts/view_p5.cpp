// P5 PACK test commands (docs/FIX_BATCH_P5.md 5.6): every people's fingerprint on its armour, read as state. Every command
// is also a Witness call. Test only (SOLID 5: rpg/view/scripts/).
//   expect makers <slot> <people> [hero|companion [m]|near]   the maker people of the piece worn in that tier slot
//          (HumanLook::makerAt, FIX_BATCH_P5 4.2 / 4.4) on the hero (default), the m-th companion in play (0) or the
//          nearest census person (a townsperson, the watch, a lineup row: any human NPC not in the party). <slot>: body
//          helm shield weapon gloves boots cloak back; <people>: an archetype name (fjordfolk ... starspire, spaces and
//          case ignored), a number 0-12 (cult::Archetype + 1) or none
//   expect makers same|differ <slotA> <slotB> [who...]   the two slots' makers are equal (same) or both set and unequal
// The looks are derived, never saved: these commands read what the doll is handed (Actor::look) and change nothing.
// (`p5stage` of the plan's 5.6 is not added: gotoculture, lineup, gear6 and party give stage every shot.)
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/core/art_ids.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace {
// lower case, letters and digits only ("Sun Temple" -> "suntemple")
std::string keyOf(const std::string& s) {
  std::string o;
  for (char ch : s) if (std::isalnum((unsigned char)ch)) o += (char)std::tolower((unsigned char)ch);
  return o;
}

// the tier slot words, in TIER_* order (art_ids.h)
constexpr const char* kSlots[art::HumanLook::TIER_SLOTS] = {"body", "helm", "shield", "weapon", "gloves", "boots", "cloak", "back"};
int slotOf(const std::string& w) {
  const std::string k = keyOf(w);
  for (int s = 0; s < art::HumanLook::TIER_SLOTS; s++) if (k == kSlots[s]) return s;
  if (k == "armor" || k == "armour" || k == "chest") return art::HumanLook::TIER_BODY;
  if (k == "helmet" || k == "head") return art::HumanLook::TIER_HELM;
  return -1;
}

// a people word as the look stores it: 0 none, 1..12 cult::Archetype + 1; -1 unknown
constexpr int kPeople = (int)cult::Archetype::COUNT;
int peopleOf(const std::string& w) {
  const std::string k = keyOf(w);
  if (k == "none" || k == "-") return 0;
  if (!k.empty() && std::isdigit((unsigned char)k[0])) {
    const int n = std::atoi(k.c_str());
    return n >= 0 && n <= kPeople ? n : -1;
  }
  for (int a = 0; a < kPeople; a++)
    if (keyOf(cult::archetypeName((cult::Archetype)a)) == k) return a + 1;
  return -1;
}
// "fjordfolk" ... "suntemple" (fp::row(p).name's spelling); "none"
std::string peopleName(int p) { return p <= 0 || p > kPeople ? std::string("none") : keyOf(cult::archetypeName((cult::Archetype)(p - 1))); }

// HumanLook::makerAt (CORE's wave-1 contract, FIX_BATCH_P5 4.2); -1 on a tree without it, so this file builds on a
// branch cut before the contract merge (the command then fails, saying so) and reads the field everywhere after it
template <class Look>
auto makerOf(const Look& L, int slot, int) -> decltype(L.makerAt(slot)) { return L.makerAt(slot); }
template <class Look>
int makerOf(const Look&, int, long) { return -1; }

// whose look: the hero, the m-th companion in play, or the nearest census person; null (and why) if none
const art::HumanLook* lookOf(ScriptCtx& c, size_t at, std::string& who) {
  Game& g = c.game;
  const std::string w = keyOf(c.arg(at));
  if (w.empty() || w == "hero" || w == "player") { who = "the hero"; return &g.pl().look; }
  if (w == "companion" || w == "comp") {
    const int m = c.arg(at + 1).empty() ? 0 : std::atoi(c.arg(at + 1).c_str());
    int n = 0;
    for (const Actor& a : g.actors)
      if (a.comp && a.human && n++ == m) { who = "companion " + std::to_string(m); return &a.look; }
    who = "companion " + std::to_string(m) + " (not in play)";
    return nullptr;
  }
  if (w == "near" || w == "nearest") {
    const Vec2 p = g.pl().p;
    const Actor* best = nullptr;
    float bd = 1e30f;
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& a = g.actors[i];
      if (!a.human || !a.npc || a.comp) continue;
      const float d = (a.p.x - p.x) * (a.p.x - p.x) + (a.p.y - p.y) * (a.p.y - p.y);
      if (d < bd) { bd = d; best = &a; }
    }
    if (best) { who = "the nearest person (" + best->name + ", " + std::to_string((int)std::sqrt(bd)) + " px)"; return &best->look; }
    who = "the nearest person (nobody in play)";
    return nullptr;
  }
  who = "'" + c.arg(at) + "' (hero|companion [m]|near)";
  return nullptr;
}

bool expMakers(ScriptCtx& c) {
  const std::string first = keyOf(c.arg(2));
  if (first == "same" || first == "differ") {
    const int sa = slotOf(c.arg(3)), sb = slotOf(c.arg(4));
    if (sa < 0 || sb < 0) { c.fail("expect makers same|differ <slotA> <slotB> [hero|companion [m]|near]"); return true; }
    std::string who;
    const art::HumanLook* L = lookOf(c, 5, who);
    if (!L) { c.fail("expect makers: no look for " + who); return true; }
    const int a = makerOf(*L, sa, 0), b = makerOf(*L, sb, 0);
    if (a < 0) { c.fail("expect makers: this build's looks carry no makers (HumanLook::makerAt)"); return true; }
    const bool ok = first == "same" ? a == b : (a > 0 && b > 0 && a != b);
    if (!ok)
      c.fail("expect makers " + first + " " + kSlots[sa] + " " + kSlots[sb] + ": " + who + " wears " + peopleName(a) + " / " + peopleName(b));
    return true;
  }
  const int s = slotOf(c.arg(2)), want = peopleOf(c.arg(3));
  if (s < 0 || want < 0) { c.fail("expect makers <slot> <people> [hero|companion [m]|near]: unknown '" + (s < 0 ? c.arg(2) : c.arg(3)) + "'"); return true; }
  std::string who;
  const art::HumanLook* L = lookOf(c, 4, who);
  if (!L) { c.fail("expect makers: no look for " + who); return true; }
  const int have = makerOf(*L, s, 0);
  if (have < 0) { c.fail("expect makers: this build's looks carry no makers (HumanLook::makerAt)"); return true; }
  if (have != want)
    c.fail("expect makers " + std::string(kSlots[s]) + " " + peopleName(want) + ": " + who + "'s " + kSlots[s] + (have ? " is " + peopleName(have) + "'s make" : std::string(" has no maker")));
  return true;
}
EMB_SCRIPT_CMD("expect:makers", "expect makers <slot> <people>|same|differ ... [hero|companion [m]|near]: the maker people of a worn piece (P5 5.6)", expMakers);
}  // namespace
