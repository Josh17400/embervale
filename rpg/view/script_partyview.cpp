// M8 Fellowship, the VIEW lane's script commands: the party on screen staged for screenshots and checks (never the
// player's save). Member index m defaults to the selected member (0).
//   partyview level <n>                 the player's level (20: room for two people, LEADERSHIP_LEVEL)
//   partyview hound [wolf] [name]       a bonded hound (or wolf) pup joins (Game::partyRecruitStranger, Origin::Animal)
//   partyview grown [m]                 the animal is grown (MF_PUP cleared)
//   partyview mercenary [name]          a hireling joins (Game::partyRecruitStranger, Origin::Mercenary)
//   partyview select <m>                the card is selected (its name tag shows)
//   partyview hold [m]                  a finger held on the card (the radial opens after the hold, next frame)
//   partyview slide <order>             the held finger slides onto a wedge (follow|wait|attack|defend|skill|passive|home)
//   partyview release                   the held finger lets go (the order under it is given)
//   partyview radial [m]                the radial opens (as C or a tap on the selected card)
//   partyview choose <order>            the open radial's wedge is chosen
//   partyview close                     the radial closes
//   partyview page [m] [remembered|story|gear <slot 0..11>]   the menu's PARTY page
//   partyview banter <words...>         member 0 says a line (Game::partySay)
//   partyview alert <kind> <words...>   an alert ribbon (Ev::PartyAlert kind 1..8) for member 0
//   partyview hp <pct> [m]              the member's health
//   partyview downt <secs> [m] | bindt <secs> [m]   the bleeding-out clock / the binding's progress (a downed member)
//   partyview mount [m] [breed]         the member rides their own horse while the player rides (MF_MOUNTED)
//   partyview grave [flowers] [old] [name]   a fallen companion's grave two tiles east of the player (a Fallen record)
//   expect partyview radial open|closed | hot <order> | cards <n> | deny | banter | alert | tab
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/home.h"
#include "rpg/sim/party.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {
std::string lwr(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int orderWord(const std::string& w) {
  static const char* const words[] = {"follow", "wait", "attack", "defend", "skill", "passive", "home"};
  for (int k = 0; k < (int)party::Cmd::COUNT; k++) if (w == words[k]) return k;
  return w.size() == 1 && w[0] >= '1' && w[0] <= '7' ? w[0] - '1' : -1;
}
constexpr uint64_t kScriptFinger = 0x5C4197ull;
}  // namespace

bool View::partyViewCommand(Game& g, const std::vector<std::string>& a, std::string& err) {
  auto arg = [&](size_t i) { return i < a.size() ? a[i] : std::string(); };
  const std::string what = lwr(arg(1));
  const int n = (int)g.party.members.size();
  auto member = [&](size_t i, int def) {
    const std::string w = arg(i);
    const int m = w.empty() ? def : std::atoi(w.c_str());
    if (m < 0 || m >= n) { err = "partyview: no member " + std::to_string(m); return -1; }
    return m;
  };
  partyUpdate(g, 0);   // the members' actors now
  if (what == "level") { g.plLevel = std::max(1, std::atoi(arg(2).c_str())); return true; }
  if (what == "hound" || what == "mercenary") {
    const bool wolf = lwr(arg(2)) == "wolf";
    std::string name = what == "hound" ? (wolf ? "ASHFANG" : "BRAN") : "TORVI THE SELLSWORD";
    const size_t ni = what == "hound" ? (wolf ? 3 : 2) : 2;
    if (!arg(ni).empty()) name = arg(ni);
    if (what == "hound")   // one bonded animal: a second `hound` turns the one there is into a wolf (or back)
      for (party::Member& M : g.party.members)
        if (M.voc == party::Vocation::Hound) { M.species = wolf ? 1 : 0; return true; }
    uint64_t cult = 0;
    if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size())
      if (const cult::Culture* c = g.world.cultureOf(g.curSite)) cult = c->id;
    const int mi = g.partyRecruitStranger(name, false, cult, 0xB0D5u + (uint32_t)n * 977u, what == "hound" ? party::Origin::Animal : party::Origin::Mercenary,
                                          g.pl().p + Vec2(-16, 6));
    if (mi < 0) { err = "partyview " + what + ": the party is full"; return false; }
    if (what == "hound") g.party.members[(size_t)mi].species = wolf ? 1 : 0;
    return true;
  }
  if (what == "grown") { const int m = member(2, n - 1); if (m < 0) return false; g.party.members[(size_t)m].flags &= (uint16_t)~party::MF_PUP; return true; }
  if (what == "select") { const int m = member(2, 0); if (m < 0) return false; partySel_ = m; partyTagT_ = 4.0f; return true; }
  if (what == "hold") {
    const int m = member(2, partySel_);
    if (m < 0) return false;
    for (const PartyCard& c : partyCards_)
      if (c.member == m) {
        const Vec2 p(c.x + c.w / 2, c.y + c.h / 2);
        if (!partyPointer(g, 0, kScriptFinger, p, false)) { err = "partyview hold: the card took no press"; return false; }
        partyPress_.t = 1.0f;   // held long enough: the radial opens on the next update
        return true;
      }
    err = "partyview hold: no card for member " + std::to_string(m) + " (is the HUD drawn?)";
    return false;
  }
  if (what == "slide") {
    const int k = orderWord(lwr(arg(2)));
    if (k < 0) { err = "partyview slide: unknown order " + arg(2); return false; }
    if (!radial_.open) { err = "partyview slide: the radial is not open"; return false; }
    const float an = -1.5707963f + k * 6.2831853f / (float)party::Cmd::COUNT;
    // (fixer M8 r2) a held radial picks by the way the finger slides from where it pressed
    const Vec2 p = (radial_.drag ? radial_.from : radial_.c) + Vec2(std::cos(an), std::sin(an)) * 38.0f;
    partyPointer(g, 1, kScriptFinger, p, false);
    return true;
  }
  if (what == "release") {
    Vec2 p = radial_.c;
    if (radial_.hot >= 0) {
      const float an = -1.5707963f + radial_.hot * 6.2831853f / (float)party::Cmd::COUNT;
      p = (radial_.drag ? radial_.from : radial_.c) + Vec2(std::cos(an), std::sin(an)) * 38.0f;
    }
    partyPointer(g, 2, kScriptFinger, p, false);
    return true;
  }
  if (what == "radial") { const int m = member(2, partySel_); if (m < 0) return false; radialOpen(g, m, false, 0); return true; }
  if (what == "choose") {
    const int k = orderWord(lwr(arg(2)));
    if (k < 0 || !radial_.open) { err = "partyview choose: no radial open or unknown order " + arg(2); return false; }
    radial_.hot = k;
    radialChoose(g, k);
    return true;
  }
  if (what == "close") { radial_.open = false; return true; }
  if (what == "page") {
    if (n > 0) { const int m = member(2, partySel_); if (m >= 0) partySel_ = m; }
    openMenu(g, kPartyTab);
    partyPage_ = 0; partyFocus_ = 0;
    const std::string sub = lwr(arg(3));
    if (sub == "remembered") partyPage_ = 1;
    else if (sub == "gear") { partyFocus_ = 1; partySlot_ = std::clamp(std::atoi(arg(4).c_str()), 0, 11); partyPack_ = 0; }
    return true;
  }
  if (what == "banter") {
    if (n == 0) { err = "partyview banter: no party"; return false; }
    std::string s;
    for (size_t i = 2; i < a.size(); i++) { if (i > 2) s += ' '; s += a[i]; }
    g.partySay(0, s, 5.0f);
    return true;
  }
  if (what == "alert") {
    if (n == 0) { err = "partyview alert: no party"; return false; }
    Event e;
    e.type = Ev::PartyAlert; e.a = 0; e.f = (float)std::atoi(arg(2).c_str());
    for (size_t i = 3; i < a.size(); i++) { if (i > 3) e.s += ' '; e.s += a[i]; }
    partyEvent(g, e);
    return true;
  }
  if (what == "hp" || what == "downt" || what == "bindt") {
    const float v = (float)std::atof(arg(2).c_str());
    const int m = member(3, 0);
    if (m < 0) return false;
    party::Member& M = g.party.members[(size_t)m];
    const int ai = partyActor_[(size_t)m];
    if (what == "hp") { if (ai > 0) { Actor& x = g.actors[(size_t)ai]; x.hp = x.maxHp * clampf(v / 100.0f, 0.01f, 1.0f); M.hp = x.hp; } return true; }
    if (M.state != party::State::Downed) { err = "partyview " + what + ": not downed"; return false; }
    if (what == "downt") M.downT = v; else M.reviveT = std::max(0.001f, v);
    return true;
  }
  if (what == "mount") {
    const int m = member(2, 0);
    if (m < 0) return false;
    party::Member& M = g.party.members[(size_t)m];
    M.flags |= party::MF_MOUNTED;
    M.horse = (uint8_t)(1 + std::clamp(arg(3).empty() ? 1 : std::atoi(arg(3).c_str()), 0, (int)home::Breed::COUNT - 1));
    return true;
  }
  if (what == "grave") {
    party::Fallen f;
    f.id = 900 + (uint32_t)g.party.fallen.size();
    f.name = "HALDOR";
    f.voc = party::Vocation::Warrior;
    f.origin = party::Origin::Tavern;
    if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size())
      if (const cult::Culture* c = g.world.cultureOf(g.curSite)) f.culture = c->id;
    f.gx = g.world.ox + (int)std::floor(g.pl().p.x / TILE) + 2;
    f.gy = g.world.oy + (int)std::floor(g.pl().p.y / TILE);
    f.day = g.day;
    f.cause = "FELL TO A FROST TROLL ON THE HIGH ROAD";
    f.seed = 0x6EA7Eu + (uint32_t)g.party.fallen.size() * 131u;
    f.days = 23; f.kills = 14; f.bond = 61;
    for (size_t i = 2; i < a.size(); i++) {
      const std::string w = lwr(a[i]);
      if (w == "flowers") f.graveFlags |= party::GF_FLOWERS;
      else if (w == "old") f.day = g.day - 30;
      else f.name = a[i];
    }
    // (fixer M8 r3, review: "sited across a yurt doorway, in the middle of the village path") the sim sites it, by
    // its own rules (open, quiet ground clear of doors, buildings, pens and paths), near where it was staged; the
    // player is then stood two tiles west of it, facing it, as the staging always had them
    if (!g.inside) {
      PartyOps::siteGrave(g, f);
      if (f.graveFlags & party::GF_SITED) {
        g.pl().p = Vec2((float)(f.gx - g.world.ox - 2) * TILE + 8.0f, (float)(f.gy - g.world.oy) * TILE + 10.0f);
        g.pl().face = 2;   // (right: toward it)
      }
    }
    if (!(f.graveFlags & party::GF_SITED)) f.graveFlags |= party::GF_SITED;   // (no open ground near: laid where staged)
    g.party.fallen.push_back(f);
    g.party.graveDirty = true;         // stamped into the map next step (a solid tile)
    return true;
  }
  err = "partyview: unknown " + arg(1);
  return false;
}

bool View::partyViewExpect(Game& g, const std::vector<std::string>& a, std::string& err) {
  auto arg = [&](size_t i) { return i < a.size() ? a[i] : std::string(); };
  const std::string what = lwr(arg(2));
  if (what == "radial") {
    const bool want = lwr(arg(3)) != "closed";
    if (radial_.open != want) err = std::string("expect partyview radial: ") + (radial_.open ? "open" : "closed");
    return err.empty();
  }
  if (what == "hot") {
    const int k = orderWord(lwr(arg(3)));
    if (!radial_.open || radial_.hot != k) err = "expect partyview hot: " + std::to_string(radial_.hot);
    return err.empty();
  }
  if (what == "cards") {
    const int want = std::atoi(arg(3).c_str());
    if ((int)partyCards_.size() != want) err = "expect partyview cards: " + std::to_string(partyCards_.size());
    return err.empty();
  }
  if (what == "deny") { if (partyDenyT_ <= 0 && radial_.denyT <= 0) err = "expect partyview deny: no refusal shown"; return err.empty(); }
  if (what == "banter") { if (banterShow_.empty()) err = "expect partyview banter: none on screen"; return err.empty(); }
  if (what == "alert") { if (alertShow_.empty()) err = "expect partyview alert: none on screen"; return err.empty(); }
  if (what == "tab") { if (g.mode != Mode::Menu || menuTab_ != kPartyTab) err = "expect partyview tab: not on the PARTY page"; return err.empty(); }
  err = "expect partyview: unknown " + arg(2);
  return false;
}

namespace {
bool cmdPartyView(ScriptCtx& c) {
  std::string err;
  if (!c.view.partyViewCommand(c.game, c.a, err) && !err.empty()) c.fail(err);
  return true;
}
EMB_SCRIPT_CMD("partyview", "partyview level|hound|grown|mercenary|select|hold|slide|release|radial|choose|close|page|banter|alert|hp|downt|bindt|mount|grave ...: stage M8 party screens",
               cmdPartyView);
bool expPartyView(ScriptCtx& c) {
  std::string err;
  if (!c.view.partyViewExpect(c.game, c.a, err) && !err.empty()) c.fail(err);
  return true;
}
EMB_SCRIPT_CMD("expect:partyview", "expect partyview radial open|closed | hot ORDER | cards N | deny | banter | alert | tab", expPartyView);
}  // namespace
