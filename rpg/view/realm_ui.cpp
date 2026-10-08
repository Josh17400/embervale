// M4 "Banners" VIEW lane: realm facts in words and colours for the HUD, the journal and the map (realm_ui.h).
#include "rpg/view/realm_ui.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/world/source.h"

namespace rui {

const char* realmWord(uint8_t government) {
  switch ((cult::Government)government) {
    case cult::Government::Monarchy: return "KINGDOM";
    case cult::Government::Jarldom: return "JARLDOM";
    case cult::Government::Khanate: return "KHAGANATE";
    case cult::Government::ClanElders: return "CLANLANDS";
    case cult::Government::Theocracy: return "THEOCRACY";
    case cult::Government::MerchantRepublic: return "REPUBLIC";
    case cult::Government::HighCouncil: return "HIGH REALM";
    default: return "KINGDOM";
  }
}

Look look(const Game& g, ew::Gid id) {
  Look k;
  if (!id) return k;
  k.id = id;
  auto it = g.world.kingdomById.find(id);
  if (it != g.world.kingdomById.end() && it->second >= 0 && it->second < (int)g.world.kingdoms.size()) {
    const Kingdom& w = g.world.kingdoms[(size_t)it->second];
    k.ok = true; k.handle = it->second; k.name = w.name; k.color = w.color; k.color2 = w.color2; k.emblem = w.emblem;
    k.culture = g.world.cultureOfKingdom(it->second);
    if (k.culture) k.gov = (uint8_t)cult::societyOf(*k.culture).government;
  }
  if (const realm::KingdomState* K = g.realm.kingdom(id)) {
    k.ok = true;
    if (!K->name.empty()) k.name = K->name;
    if (K->color) { k.color = K->color; k.color2 = K->color2; k.emblem = K->emblem; }
    k.gov = K->gov;
    if (!K->ruler.name.empty()) k.ruler = (K->ruler.title.empty() ? std::string() : K->ruler.title + " ") + K->ruler.name;
  }
  if (!k.ok && g.world.src) {
    if (const ew::KingdomPlan* kp = g.world.src->kingdom(id)) {
      k.ok = true; k.name = kp->name; k.color = kp->color; k.color2 = kp->color2; k.emblem = kp->emblem;
    }
  }
  return k;
}

std::string realmTitle(const Look& k, bool the) {
  if (!k.ok) return the ? "THE WILDLANDS" : "WILDLANDS";
  return std::string(the ? "THE " : "") + realmWord(k.gov) + " OF " + k.name;
}

void arms(const Look& k, cult::Heraldry& out) {
  out = cult::Heraldry();
  if (k.culture && !k.culture->heraldry.empty()) {
    out = k.culture->heraldry;
    // a realm-made kingdom (rebels, a new dynasty) flies its own colours over its culture's grammar
    if (k.color && k.color != out.field) { out.field = k.color; out.charge = k.color2 ? k.color2 : out.charge; }
    return;
  }
  out.field = k.color ? k.color : rgba(150, 40, 44);
  out.charge = k.color2 ? k.color2 : rgba(236, 210, 120);
  out.emblem = k.emblem;
}

std::string stateLine(const Game& g, ew::Gid site, uint32_t* col) {
  const realm::SettlementState* st = g.realm.settlement(site);
  auto put = [&](uint32_t c) { if (col) *col = c; };
  if (!st) return std::string();
  const uint16_t f = st->flags;
  auto nameOf = [&](ew::Gid k) { const Look l = look(g, k); return l.ok ? l.name : std::string("RAIDERS"); };
  if (f & realm::SS_BESIEGED) {
    const realm::Siege* s = g.realm.siegeAt(site);
    put(rgba(255, 118, 92));   // (fixer M4 r1) brighter: dark red read poorly on the translucent band at 1x
    return "BESIEGED BY " + nameOf(s ? s->attacker : 0);
  }
  if (f & realm::SS_RUINED) { put(rgba(170, 160, 150)); return "IN RUINS"; }
  if (f & realm::SS_BURNED) { put(rgba(236, 120, 60)); return "BURNED"; }
  if (f & realm::SS_ABANDONED) { put(rgba(170, 160, 150)); return "ABANDONED"; }
  if (f & realm::SS_OCCUPIED) { put(rgba(236, 170, 80)); return "OCCUPIED BY " + nameOf(st->owner); }
  if (f & realm::SS_FAMINE) { put(rgba(226, 190, 90)); return "HUNGRY"; }
  if (f & realm::SS_UNREST) { put(rgba(226, 150, 90)); return "IN UNREST"; }
  if (f & realm::SS_REBUILDING) { put(rgba(200, 190, 150)); return "REBUILDING"; }
  if (f & realm::SS_REFUGEES) { put(rgba(200, 190, 150)); return "SHELTERS REFUGEES"; }
  return std::string();
}

std::vector<const realm::SettlementState*> knownStates(const Game& g) {
  std::vector<const realm::SettlementState*> out;
  std::unordered_set<ew::Gid> seen;
  auto add = [&](ew::Gid s) {
    if (!s || !seen.insert(s).second) return;
    if (const realm::SettlementState* st = g.realm.settlement(s)) out.push_back(st);
  };
  for (const Site& s : g.world.sites) if (s.settlement()) add(s.id);
  for (const realm::KingdomState& k : g.realm.kingdoms()) for (ew::Gid s : k.settlements) add(s);
  for (const realm::Siege& s : g.realm.sieges()) add(s.site);
  for (const realm::WorldEvent& e : g.realm.events()) add(e.site);
  return out;
}

uint64_t signature(const Game& g) {
  uint64_t h = 0x9E3779B97F4A7C15ull ^ g.realm.kingdoms().size();
  auto mx = [&](uint64_t v) { h = ew::mix64(h ^ v); };
  for (const realm::KingdomState& k : g.realm.kingdoms()) {
    mx(k.id); mx(k.fallen ? 1 : 0);
    for (ew::Gid s : k.settlements) {
      mx(s);
      if (const realm::SettlementState* st = g.realm.settlement(s)) mx(st->home);
    }
  }
  for (const realm::Siege& s : g.realm.sieges()) mx(s.site ^ (s.over ? 7 : 3));
  return h;
}

std::string daysAgo(int today, int day) {
  const int d = std::max(0, today - day);
  if (d == 0) return "TODAY";
  if (d == 1) return "YESTERDAY";
  return std::to_string(d) + " DAYS AGO";
}

ew::Gid landOwnerAt(const Game& g, int32_t gx, int32_t gy) {
  if (!g.world.src) return 0;
  return g.realm.landOwner(g.world.src->kingdomAt(gx, gy), gx, gy);
}

}  // namespace rui
