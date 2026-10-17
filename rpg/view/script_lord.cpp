// M12 LORD lane script commands (rpg/view/script_api.h). Phase A (the lead):
//   lord grant [n]          the player becomes lord of the n-th nearest village (default 0) under its owner
//   lord free [name...]     raise the player's banner: a realm of their own (the name optional)
//   lord collect            the holdings' taxes now (prints the gold)
//   lord tribute            pay what is owed now
//   lord take               the coffers into the purse
//   expect lord holdings <n>    at least n holdings
//   expect lord realm <0|1>     the player's own realm exists
//   expect lord treasury <n>    at least n gold in the coffers
//   expect lord liege <0|1>     the lordship answers to a liege
// Phase B (LORD lane):
//   lord build <work|all> [now]   raise a work in the seat (banner palisade hall granary militia market watchtower walls
//                                 shrine; all = every work but the walls); `now` finishes it at once (the coffers filled)
//   lord tax low|fair|heavy       the seat's tax
//   lord here [dx dy]             stand the player on the seat's green (dx dy: tiles from the heart), facing south
//   lord steward                  stand before the seat's steward and talk to them (the hall's pages)
//   lord withhold                 the steward told to withhold the tribute
//   lord siege                    the liege's siege of the seat now (defiance first if still sworn)
//   lord break                    the BREAK THE SIEGE fight's strength added (14 besiegers slain)
//   lord resettle                 the nearest other village burned, its refugees led home (the RESETTLED path)
//   lord conquer [n]              the n-th nearest village not held taken by the player's banner (a script's conquest)
//   lord march [n]                the steward's MARCH ON the n-th nearest village (the siege; `lord assault` wins it)
//   lord assault                  the player's march gets the assault's strength
//   lord coffers <n>              the coffers set to n gold
//   expect lord props <n>         at least n work props stamped in the window
//   expect lord steward <0|1>     the seat's steward stands in the window
//   expect lord crowned <0|1>     the player's realm is a kingdom (KING / QUEEN)
//   expect lord siege <0|1>       the liege's siege of a holding is being fought
//   expect lord works <n>         at least n works built in the seat
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

int nthVillage(Game& g, int n, bool unheld = false) {
  std::vector<std::pair<float, int>> d;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != SiteType::Village) continue;
    if (unheld && g.lordship.holding(s.id)) continue;
    d.push_back({(s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py), (int)i});
  }
  std::sort(d.begin(), d.end());
  return n >= 0 && n < (int)d.size() ? d[(size_t)n].second : -1;
}

uint16_t workBit(const std::string& w) {
  static const struct { const char* n; uint16_t b; } t[] = {{"banner", lord::LW_BANNER}, {"palisade", lord::LW_PALISADE}, {"hall", lord::LW_HALL},
                                                            {"granary", lord::LW_GRANARY}, {"militia", lord::LW_MILITIA}, {"market", lord::LW_MARKET},
                                                            {"watchtower", lord::LW_WATCHTOWER}, {"tower", lord::LW_WATCHTOWER}, {"walls", lord::LW_WALLS},
                                                            {"shrine", lord::LW_SHRINE}};
  for (const auto& x : t) if (w == x.n) return x.b;
  return 0;
}

const realm::Siege* siegeById(const Game& g, uint32_t id) {
  for (const realm::Siege& s : g.realm.sieges()) if (s.id == id) return &s;
  return nullptr;
}

bool cmdLord(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.world.src) { c.fail("lord: no endless world"); return true; }
  const std::string w = c.arg(1);
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  g.realmNoteSites();
  lord::Lordship& L = g.lordship;
  std::string why;
  const int seatSi = L.holdings.empty() ? -1 : g.world.siteHandle(L.holdings[0].site);
  if (w == "grant") {
    const int si = nthVillage(g, std::max(0, std::atoi(c.arg(2).c_str())));
    if (si < 0) { c.fail("lord grant: no village loaded"); return true; }
    if (!LordOps::grant(g, si, lord::How::Granted, 0, why)) c.fail("lord grant: " + why);
  } else if (w == "free") {
    if (!LordOps::independence(g, c.rest(2), why)) c.fail("lord free: " + why);
  } else if (w == "collect") {
    printf("lord: collected %d gold (coffers %d)\n", LordOps::collect(g), L.treasury);
    fflush(stdout);
  } else if (w == "tribute") {
    if (!LordOps::payTribute(g)) c.fail("lord tribute: not paid");
  } else if (w == "take") {
    LordOps::take(g, L.treasury);
  } else if (w == "coffers") {
    L.treasury = std::max(0, std::atoi(c.arg(2).c_str()));
  } else if (w == "build") {
    if (L.holdings.empty()) { c.fail("lord build: no holding"); return true; }
    lord::Holding& h = L.holdings[0];
    const bool now = c.arg(3) == "now" || c.arg(2) == "all";
    std::vector<uint16_t> bits;
    if (c.arg(2) == "all") { for (uint16_t b : lord::BUILD_ORDER) if (b != lord::LW_WALLS) bits.push_back(b); }
    else if (uint16_t b = workBit(c.arg(2))) bits.push_back(b);
    else { c.fail("lord build <work|all> [now]"); return true; }
    for (uint16_t b : bits) {
      if (h.works & b) continue;
      if (h.building && now) { h.buildDone = (uint16_t)std::max(0, g.day); LordOps::daily(g); }   // the work going up finished first
      if (h.works & b) continue;
      L.treasury = std::max(L.treasury, 100000);
      if (!LordOps::startWork(g, h, b, why)) { c.fail("lord build: " + why); return true; }
      if (now) { h.buildDone = (uint16_t)std::max(0, g.day); LordOps::daily(g); }
    }
    LordOps::restamp(g);
  } else if (w == "tax") {
    if (L.holdings.empty()) { c.fail("lord tax: no holding"); return true; }
    const std::string t = c.arg(2);
    LordOps::setTax(g, L.holdings[0], t == "low" ? lord::TAX_LOW : t == "heavy" ? lord::TAX_HEAVY : lord::TAX_FAIR);
  } else if (w == "here") {
    if (seatSi < 0) { c.fail("lord here: no holding loaded"); return true; }
    const Site& s = g.world.sites[(size_t)seatSi];
    const int dx = c.arg(2).empty() ? 0 : std::atoi(c.arg(2).c_str()), dy = c.arg(3).empty() ? 3 : std::atoi(c.arg(3).c_str());
    g.pl().p = LordOps::freeSpot(g, s.ex + dx, s.ey + dy);
  } else if (w == "steward") {
    if (L.holdings.empty()) { c.fail("lord steward: no holding"); return true; }
    const int sa = LordOps::stewardActor(g, L.holdings[0]);
    if (sa < 0) { if (c.waited < 3) return false; c.fail("lord steward: none stands in the seat"); return true; }
    Actor& st = g.actors[(size_t)sa];
    const int tx = (int)std::floor(st.p.x / TILE), ty = (int)std::floor(st.p.y / TILE);
    g.pl().p = LordOps::freeSpot(g, tx, ty + 1);
    LordOps::talkTo(g, g.actors[(size_t)LordOps::stewardActor(g, L.holdings[0])]);
  } else if (w == "withhold") {
    LordOps::withhold(g, true);
  } else if (w == "siege") {
    if (!LordOps::forceWrathSiege(g)) c.fail("lord siege: no siege could be laid");
  } else if (w == "break") {
    const realm::Siege* s = L.siege ? siegeById(g, L.siege) : nullptr;
    if (!s) { c.fail("lord break: no siege of a holding"); return true; }
    g.realm.contribute(s->id, 2, 7.0f);
  } else if (w == "resettle") {
    int burn = -1;
    for (int n = 0; n < 12 && burn < 0; n++) {
      const int si = nthVillage(g, n, true);
      if (si < 0) break;
      const realm::SettlementState* st = g.realm.settlement(g.world.sites[(size_t)si].id);
      if (st && !(st->flags & realm::SS_BESIEGED)) burn = si;
    }
    if (burn < 0) { c.fail("lord resettle: no village to burn"); return true; }
    if (const realm::SettlementState* st = g.realm.settlement(g.world.sites[(size_t)burn].id))
      if (!(st->flags & (realm::SS_BURNED | realm::SS_ABANDONED | realm::SS_RUINED))) g.realm.forceBurn(g.world.sites[(size_t)burn].id, g.day);
    if (!LordOps::resettle(g, burn, why)) c.fail("lord resettle: " + why);
  } else if (w == "conquer") {
    if (!L.realm) { c.fail("lord conquer: raise your banner first"); return true; }
    const int si = nthVillage(g, std::max(0, std::atoi(c.arg(2).c_str())), true);
    if (si < 0) { c.fail("lord conquer: no village"); return true; }
    g.realm.playerTakes(g.world.sites[(size_t)si].id, g.day);
    LordOps::adopt(g);
    g.realmSync();
  } else if (w == "march") {
    const int si = nthVillage(g, std::max(0, std::atoi(c.arg(2).c_str())), true);
    if (si < 0 || !LordOps::march(g, g.world.sites[(size_t)si].id, why)) c.fail("lord march: " + (si < 0 ? std::string("no village") : why));
  } else if (w == "dump") {
    // the stamped props relative to the seat's heart (debugging the layout)
    for (const LordOps::WorkTile& t : LordOps::stamped(g)) {
      const int x = t.gx - g.world.ox - (seatSi >= 0 ? g.world.sites[(size_t)seatSi].ex : 0), y = t.gy - g.world.oy - (seatSi >= 0 ? g.world.sites[(size_t)seatSi].ey : 0);
      if (t.work != lord::LW_PALISADE && t.work != lord::LW_WALLS) printf("lord dump: work %d prop %d at %+d %+d\n", t.work, t.prop - 1, x, y);
    }
    if (seatSi >= 0 && c.arg(2) == "all") {
      const Site& s = g.world.sites[(size_t)seatSi];
      for (int y = -6; y <= 6; y++)
        for (int x = -7; x <= 7; x++) {
          const int p = g.world.over.propAt(s.ex + x, s.ey + y);
          if (p && p - 1 > 21) printf("lord dump: map prop %d at %+d %+d\n", p - 1, x, y);
        }
    }
    fflush(stdout);
  } else if (w == "assault") {
    if (!L.march) { c.fail("lord assault: no march"); return true; }
    g.realm.contribute(L.march, 1, 10.0f);
  } else {
    c.fail("lord grant [n] | free [name] | collect | tribute | take | coffers N | build W [now] | tax L | here | steward | withhold | siege | break | "
           "resettle | conquer [n] | march [n] | assault");
  }
  return true;
}

bool expLord(ScriptCtx& c) {
  const Game& g = c.game;
  const lord::Lordship& L = g.lordship;
  const std::string what = c.arg(2);
  const int v = std::atoi(c.arg(3).c_str());
  if (what == "holdings") { if ((int)L.holdings.size() < v) c.fail("expect lord holdings: got " + std::to_string(L.holdings.size())); }
  else if (what == "realm") { if ((L.realm != 0) != (v != 0)) c.fail("expect lord realm " + c.arg(3)); }
  else if (what == "treasury") { if (L.treasury < v) c.fail("expect lord treasury: got " + std::to_string(L.treasury)); }
  else if (what == "liege") { if ((L.liege != 0) != (v != 0)) c.fail("expect lord liege " + c.arg(3)); }
  else if (what == "props") {
    const int n = (int)LordOps::stamped(g).size();
    if (n < v) c.fail("expect lord props: got " + std::to_string(n));
  } else if (what == "steward") {
    const bool has = !L.holdings.empty() && LordOps::stewardActor(g, L.holdings[0]) >= 0;
    if (has != (v != 0)) c.fail("expect lord steward " + c.arg(3));
  } else if (what == "crowned") { if ((L.crowned != 0) != (v != 0)) c.fail("expect lord crowned " + c.arg(3)); }
  else if (what == "siege") {
    const realm::Siege* s = L.siege ? siegeById(g, L.siege) : nullptr;
    if ((s && !s->over) != (v != 0)) c.fail("expect lord siege " + c.arg(3));
  } else if (what == "works") {
    int n = 0;
    if (!L.holdings.empty()) for (uint16_t b = 1; b; b = (uint16_t)(b << 1)) n += (L.holdings[0].works & b) ? 1 : 0;
    if (n < v) c.fail("expect lord works: got " + std::to_string(n));
  } else c.fail("expect lord holdings|realm|treasury|liege|props|steward|crowned|siege|works <n>");
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("lord", "lord grant|free|collect|tribute|take|coffers|build|tax|here|steward|withhold|siege|break|resettle|conquer|march|assault: the player's lordship (M12)", cmdLord);
EMB_SCRIPT_CMD("expect:lord", "expect lord holdings|realm|treasury|liege|props|steward|crowned|siege|works <n>", expLord);
