// rpg_test --lord [--seeds A..B]: the M12 LORD lane (village lordship and kingdom-making; VISION_PLAN 4.7, 15.6.4, 15.33),
// per seed in a real endless game, every path through dialogue:
//   granted   enlisted with the start kingdom, raised to Captain, a ruler's I ASK FOR A LORDSHIP: the start village
//   steward   a named Role::Steward stands in the seat; its pages answer (coffers, tax, works, needs, tribute, banner)
//   taxes     60 days at LOW / FAIR / HEAVY: gold within the stated range, LOW < FAIR < HEAVY, loyalty up at LOW, down at HEAVY
//   works     every work raised through RAISE WORKS, done in its days, stamped as props in the loaded village (counted; none
//             on a road, bridge, building or doorstep), the steward and the militiamen standing
//   tribute   paid twice from the coffers, then WITHHELD twice: the ultimatum, the village stands with its lord (the oath to
//             the liege's army released), the liege's tension at Skirmishes and its siege of the seat, broken (the BREAK
//             THE SIEGE fight's strength), the liege's terms accepted at the steward
//   resettled a burned village's refugee: LEAD US HOME (SS_REBUILDING, How::Resettled)
//   conquered the steward's MARCH ON ..., the assault's strength, the village adopted (How::Conquered)
//   crown     3 holdings: KING / QUEEN (RiseEv::Crowned)
//   tick      200 days: the player's realm keeps its ruler, never falls, never splits, never declares a war of its own
//   save      byte-identical round-trip with holdings, works and the steward; the loaded copy stamps the works and
//             stands the steward on its first frame
//   charter   (a fresh game) fame 300 and gold: a ruler's I WOULD BUY A VILLAGE'S CHARTER, the price by prosperity
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service_ops.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

std::map<int, int> g_rise;   // RiseEv -> count seen by stepDays
const char* g_png = nullptr; // --png DIR: the seat's tiles drawn after the works (4 px a tile) to DIR/lord_seed<N>.png

// the seat's tiles: ground, buildings (grey), trees (green), the lord's props (palisade red, walls white, the rest gold)
void dumpSeat(const Game& g, int si, uint64_t seed) {
  if (!g_png || si < 0) return;
  const Site& s = g.world.sites[(size_t)si];
  const Map& M = g.world.over;
  const int x0 = s.r.x - 10, y0 = s.r.y - 10, W = s.r.w + 20, H = s.r.h + 20, P = 4;
  std::vector<uint32_t> px((size_t)W * P * H * P, 0xFF000000u);
  std::map<uint64_t, uint16_t> mine;
  for (const LordOps::WorkTile& t : LordOps::stamped(g)) mine[((uint64_t)(uint32_t)(t.gx - g.world.ox) << 32) | (uint32_t)(t.gy - g.world.oy)] = t.work;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const int mx = x0 + x, my = y0 + y;
      uint32_t c = 0xFF000000u;
      if (M.in(mx, my)) {
        c = groundColor(M.at(mx, my));
        const size_t i = (size_t)my * M.w + mx;
        if (M.bldgAt[i] >= 0) c = 0xFF808080u;
        const int pr = M.prop[i];
        if (pr && art::isTreeProp((art::Prop)(pr - 1))) c = 0xFF206020u;
        auto it = mine.find(((uint64_t)(uint32_t)mx << 32) | (uint32_t)my);
        if (it != mine.end()) c = it->second == lord::LW_PALISADE ? 0xFF2020E0u : it->second == lord::LW_WALLS ? 0xFFFFFFFFu : 0xFF00D0FFu;
      }
      for (int yy = 0; yy < P; yy++)
        for (int xx = 0; xx < P; xx++) px[((size_t)(y * P + yy) * W * P) + x * P + xx] = c;
    }
  char path[512];
  std::snprintf(path, sizeof path, "%s/lord_seed%llu.png", g_png, (unsigned long long)seed);
  writePng(path, W * P, H * P, px);
}

void stepDays(Game& g, int days) {
  for (int d = 0; d < days; d++) {
    g.day++;
    for (int i = 0; i < 3; i++) {
      g.update(SIM_DT, Input());
      for (const Event& e : g.events) if (e.type == Ev::Rise) g_rise[e.a]++;
      g.events.clear();
    }
  }
}
void frames(Game& g, int n) {
  for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
}
int opt(const Game& g, const std::string& prefix) {
  for (size_t i = 0; i < g.dlg.opts.size(); i++) if (g.dlg.opts[i].label.rfind(prefix, 0) == 0) return (int)i;
  return -1;
}
bool pick(Game& g, const std::string& prefix) {
  const int o = opt(g, prefix);
  if (o < 0) return false;
  g.dialogueChoose(o);
  return true;
}
std::string opts(const Game& g) {
  std::string s;
  for (const DlgOpt& o : g.dlg.opts) s += "[" + o.label + "] ";
  return s;
}
// a person standing beside the player (a ruler of `site`, a refugee)
Actor& person(Game& g, Role r, int site, const char* name) {
  Actor a;
  a.id = LordOps::nextId(g);
  a.npc = true; a.human = true; a.faction = Faction::Town;
  a.p = g.pl().p + Vec2(14, 0); a.home = a.goal = a.p;
  a.site = site; a.fromMap = false; a.slot = -1;
  a.maxHp = a.hp = 40; a.radius = 5;
  a.role = r; a.name = name;
  g.actors.push_back(a);
  return g.actors.back();
}
void drop(Game& g, int id) {
  for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].id == id) { g.actors.erase(g.actors.begin() + (std::ptrdiff_t)i); return; }
}
void prepare(Game& g, uint64_t seed) {
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  frames(g, 30);
  int steps = 0;
  while (g.realm.focusPending() && steps < 3000) { g.update(SIM_DT, Input()); g.events.clear(); steps++; }
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  g.realm.focusNow(*g.world.src, gx, gy, g.day);
  g.realmNoteSites();
}
bool onRoadOrDoor(const Game& g, int32_t gx, int32_t gy) {
  const Map& M = g.world.over;
  const int x = gx - g.world.ox, y = gy - g.world.oy;
  if (!M.in(x, y)) return false;
  const Ground gr = M.at(x, y);
  if (gr == Ground::Road || gr == Ground::Bridge) return true;
  if (M.bldgAt[(size_t)y * M.w + x] >= 0) return true;
  for (int k = 1; k <= 2; k++)
    if (M.in(x, y - k)) {
      const int b = M.bldgAt[(size_t)(y - k) * M.w + x];
      if (b >= 0 && x == M.bldgs[(size_t)b].doorX() && y - k == M.bldgs[(size_t)b].doorY()) return true;
    }
  return false;
}

int lordSeed(uint64_t seed) {
  int bad = 0;
  auto fail = [&](const std::string& what) { out("FAIL: lord seed %llu: %s\n", (unsigned long long)seed, what.c_str()); bad++; };
  g_rise.clear();
  Game g(seed);
  prepare(g, seed);
  const Site& sv = g.world.sites[(size_t)g.world.startSite];
  const ew::Gid home = sv.kingdom >= 0 ? g.world.kingdoms[(size_t)sv.kingdom].id : 0;
  if (!home) { out("  lord seed %llu: the start village has no kingdom (skipped)\n", (unsigned long long)seed); return 0; }
  std::string why;
  int paths = 0;

  // ---- GRANTED: a captain of the start kingdom asks its ruler
  if (!ServiceOps::enlist(g, home, why)) fail("enlist: " + why);
  ServiceOps::promote(g);
  ServiceOps::promote(g);
  if (g.service.rank != service::Rank::Captain) fail("not a captain");
  {
    Actor& r = person(g, Role::Jarl, g.world.startSite, "THE JARL");
    const int rid = r.id;
    LordOps::talkTo(g, r);
    if (!pick(g, "I ASK FOR A LORDSHIP")) fail("the ruler offers no lordship to a captain: " + opts(g));
    g.closeDialogue();
    drop(g, rid);
  }
  g.mode = Mode::Play;
  if (g.lordship.holdings.size() != 1 || g.lordship.holdings[0].how != lord::How::Granted) { fail("the grant gave no holding"); return bad; }
  paths++;
  const ew::Gid seat = g.lordship.holdings[0].site;
  const int seatSi = g.world.siteHandle(seat);
  int perDay[3] = {0, 0, 0};

  // ---- the steward
  frames(g, 20);
  int sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
  if (sa < 0) fail("no steward stands in the seat");
  else {
    const Actor& st = g.actors[(size_t)sa];
    if (st.role != Role::Steward || st.name.empty()) fail("the steward has no role or name");
    LordOps::talkTo(g, g.actors[(size_t)sa]);
    for (const char* o : {"THE HALL'S COFFERS", "THE TAX", "RAISE WORKS", "THE VILLAGE'S NEEDS", "THE TRIBUTE", "RAISE MY OWN BANNER"})
      if (opt(g, o) < 0) fail(std::string("the steward lacks ") + o + ": " + opts(g));
    if (!pick(g, "THE HALL'S COFFERS") || g.dlg.text.find("COFFERS HOLD") == std::string::npos) fail("COFFERS: " + g.dlg.text);
    pick(g, "BACK");
    if (!pick(g, "RAISE MY OWN BANNER") || g.dlg.text.find("ARE YOU SURE") == std::string::npos || g.dlg.text.find("OATH") == std::string::npos)
      fail("the banner's confirm line does not say what it costs: " + g.dlg.text);
    if (opt(g, "YES. RAISE IT.") < 0 || opt(g, "NOT YET") < 0) fail("the banner's confirm options");
    pick(g, "NOT YET");
    if (g.lordship.realm) fail("NOT YET raised the banner");
    // ---- taxes over 60 days at each level (the collect rule alone: the day is put back after)
    const int d0 = g.day, t0 = g.lordship.treasury;
    lord::Holding& h = g.lordship.holdings[0];
    const char* lv[3] = {"A LOW TAX", "A FAIR TAX", "A HEAVY TAX"};
    int gold[3], loy[3];
    for (int k = 0; k < 3; k++) {
      pick(g, "BACK");
      if (opt(g, "THE TAX") < 0) { LordOps::talkTo(g, g.actors[(size_t)LordOps::stewardActor(g, g.lordship.holdings[0])]); }
      if (!pick(g, "THE TAX") || !pick(g, lv[k])) fail(std::string("tax page: ") + lv[k] + " " + opts(g));
      g.lordship.holdings[0].loyalty = 60;
      g.lordship.holdings[0].lastTaxDay = g.day;
      gold[k] = 0;
      for (int d = 0; d < 60; d++) { g.day++; gold[k] += LordOps::collect(g); }
      loy[k] = g.lordship.holdings[0].loyalty;
      perDay[k] = gold[k] / 60;
    }
    g.closeDialogue();
    g.mode = Mode::Play;
    g.day = d0;
    g.lordship.treasury = t0;
    g.lordship.taxSinceTribute = 0;
    g.lordship.tributeDue = 0;
    g.lordship.weekTax = 0;
    h.lastTaxDay = d0;
    h.loyalty = 60;
    h.taxPct = lord::TAX_FAIR;
    out("  lord seed %llu: 60 days of taxes LOW %d (loyalty %d) FAIR %d (%d) HEAVY %d (%d)\n", (unsigned long long)seed, gold[0], loy[0], gold[1], loy[1], gold[2], loy[2]);
    if (!(gold[0] < gold[1] && gold[1] < gold[2])) fail("taxes do not grow LOW < FAIR < HEAVY");
    if (gold[0] < 40 || gold[0] > 260 || gold[1] < 100 || gold[1] > 420 || gold[2] < 220 || gold[2] > 840) fail("60 days of taxes outside the stated ranges (40-260 / 100-420 / 220-840)");
    if (!(loy[0] > 60 && loy[2] < 60 && loy[1] > loy[2])) fail("loyalty does not rise at LOW and fall at HEAVY");
  }

  // ---- works, through RAISE WORKS, each done in its days (the tribute falls due twice meanwhile: paid from the coffers)
  g.lordship.treasury = 60000;
  int built = 0;
  for (uint16_t bit : lord::BUILD_ORDER) {
    const lord::WorkInfo* w = lord::workInfo(bit);
    sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
    if (sa < 0) { frames(g, 20); sa = LordOps::stewardActor(g, g.lordship.holdings[0]); }
    if (sa < 0) { fail("the steward went missing"); break; }
    LordOps::talkTo(g, g.actors[(size_t)sa]);
    if (!pick(g, "RAISE WORKS") || !pick(g, std::string("RAISE ") + w->name)) { fail(std::string("cannot order ") + w->name + ": " + opts(g)); g.closeDialogue(); g.mode = Mode::Play; continue; }
    g.closeDialogue();
    g.mode = Mode::Play;
    if (g.lordship.holdings[0].building != bit) { fail(std::string("not building ") + w->name); continue; }
    if (w->days > 1) { stepDays(g, w->days - 1); if (g.lordship.holdings[0].works & bit) fail(std::string(w->name) + " finished early"); }
    stepDays(g, 1);
    if (!(g.lordship.holdings[0].works & bit)) fail(std::string(w->name) + " not done in its days");
    else built++;
    if (bit == lord::LW_HALL) dumpSeat(g, seatSi, seed);   // every work but the walls
    if (bit == lord::LW_PALISADE) {
      int stakes = 0;
      for (const LordOps::WorkTile& t : LordOps::stamped(g)) stakes += t.prop == (int)art::Prop::Palisade + 1;
      out("  lord seed %llu: palisade %d stakes round %s (%dx%d)\n", (unsigned long long)seed, stakes, g.world.sites[(size_t)seatSi].name.c_str(),
          g.world.sites[(size_t)seatSi].r.w, g.world.sites[(size_t)seatSi].r.h);
      if (stakes < 12) fail("the palisade has under 12 stakes");
    }
  }
  frames(g, 4);
  {
    std::map<uint16_t, int> per;
    int onBad = 0, total = 0;
    for (const LordOps::WorkTile& t : LordOps::stamped(g)) { per[t.work]++; total++; onBad += onRoadOrDoor(g, t.gx, t.gy); }
    std::string list;
    for (uint16_t bit : lord::BUILD_ORDER) {
      list += std::string(lord::workInfo(bit)->name) + " " + std::to_string(per[bit]) + ", ";
      if (!per[bit] && !(bit == lord::LW_PALISADE && per[lord::LW_WALLS])) fail(std::string("no props for ") + lord::workInfo(bit)->name + " in the loaded village");
    }
    out("  lord seed %llu: %d works built, %d props: %s%d on a road / door / building\n", (unsigned long long)seed, built, total, list.c_str(), onBad);
    if (onBad) fail("work props on a road, door or building tile");
    int militia = 0;
    for (const Actor& a : g.actors) militia += a.militia && !a.fromMap && a.slot >= 2100000;
    if (militia < 1) {
      int lordActors = 0;
      for (const Actor& a : g.actors) lordActors += !a.fromMap && a.slot >= 2100000;
      fail("no militiamen stand by their yard (militia " + std::to_string(g.lordship.holdings[0].militia) + ", lord actors " + std::to_string(lordActors) +
           ", inside " + std::to_string((int)g.inside) + ")");
    }
  }
  int paid = 0;
  for (const realm::WorldEvent& e : g.realm.events()) paid += e.type == realm::EvType::TributePaid;
  if (paid < 2) {
    stepDays(g, std::max(1, (int)g.lordship.tributeDay - g.day + 1));
    paid = 0;
    for (const realm::WorldEvent& e : g.realm.events()) paid += e.type == realm::EvType::TributePaid;
  }
  if (paid < 2) fail("tribute was not paid twice (" + std::to_string(paid) + ")");

  // ---- tribute WITHHELD twice: the liege's wrath, the village stands, the siege of the seat
  sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
  if (sa >= 0) {
    LordOps::talkTo(g, g.actors[(size_t)sa]);
    if (!pick(g, "THE TRIBUTE") || !pick(g, "WITHHOLD THE TRIBUTE")) fail("cannot withhold: " + opts(g));
    g.closeDialogue();
    g.mode = Mode::Play;
  }
  int refusedN = 0;
  for (int guard = 0; guard < 80 && g.lordship.missed < 2 && !g.lordship.realm; guard++) stepDays(g, 1);
  for (const realm::WorldEvent& e : g.realm.events()) refusedN += e.type == realm::EvType::TributeRefused;
  if (g.lordship.missed < 2 || refusedN < 2) fail("tribute not withheld twice");
  if (g.lordship.wrath < 1 || !g.lordship.wrathDay) fail("no ultimatum after two withheld tributes");
  const bool swornBefore = g.service.enlisted();
  for (int guard = 0; guard < 14 && !g.lordship.realm; guard++) stepDays(g, 1);
  const ew::Gid mine = g.lordship.realm;
  if (!mine) { fail("the ultimatum ran out without the village standing with its lord"); return bad; }
  if (swornBefore && g.service.enlisted()) fail("independence did not release the oath to the liege's army");
  const realm::Relation* rel = g.realm.relation(home, mine);
  if (!rel || (rel->tension < realm::Tension::Skirmishes && rel->state != realm::Rel::War)) fail("the liege's tension did not rise to Skirmishes");
  const realm::Siege* sg = g.realm.siegeAt(seat);
  if (!sg || sg->attacker != home) fail("no siege of the seat by the liege");
  else {
    // the BREAK THE SIEGE fight: 14 besiegers slain at the militia's side
    g.realm.contribute(sg->id, 2, 0.5f * 14);
    const uint32_t sid = sg->id;
    for (int guard = 0; guard < 12 && g.lordship.siege; guard++) stepDays(g, 1);
    const realm::Siege* s2 = nullptr;
    for (const realm::Siege& s : g.realm.sieges()) if (s.id == sid) s2 = &s;
    if (!s2 || !s2->over || s2->attackerWon) fail("the siege of the seat was not broken");
    if (!g.lordship.holding(seat)) fail("the seat was lost after breaking the siege");
    if (!g.lordship.peaceOffer) fail("no peace offer after the broken siege");
  }
  // the liege's terms, at the steward
  sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
  if (sa < 0) { frames(g, 20); sa = LordOps::stewardActor(g, g.lordship.holdings[0]); }
  if (sa >= 0) {
    LordOps::talkTo(g, g.actors[(size_t)sa]);
    if (!pick(g, "ACCEPT ")) fail("no ACCEPT ... PEACE at the steward: " + opts(g));
    g.closeDialogue();
    g.mode = Mode::Play;
    if (g.realm.atWar(home, mine)) fail("still at war with the old liege after its peace");
  }
  {
    const realm::KingdomState* P = g.realm.kingdom(mine);
    if (P && P->gov == 5) fail("the player's realm took the liege's REPUBLIC");   // cult::Government::MerchantRepublic
    if (P && P->ruler.title != "LORD" && P->ruler.title != "LADY") fail("the new realm's ruler is no LORD / LADY: " + P->ruler.title);
  }

  // ---- RESETTLED: a burned village's refugee asks to be led home
  {
    int burn = -1;
    float bd = 1e30f;
    const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
    for (size_t si = 0; si < g.world.sites.size(); si++) {
      const Site& s = g.world.sites[si];
      if (s.type != SiteType::Village || g.lordship.holding(s.id)) continue;
      const realm::SettlementState* st = g.realm.settlement(s.id);
      if (!st || (st->flags & realm::SS_BESIEGED)) continue;
      const float d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
      if (d < bd) { bd = d; burn = (int)si; }
    }
    if (burn < 0) fail("no village near to burn for the resettling");
    else {
      g.realm.forceBurn(g.world.sites[(size_t)burn].id, g.day);
      g.realm.renown().fame = std::max(g.realm.renown().fame, 50);
      Actor& r = person(g, Role::Refugee, -1, "A REFUGEE");
      const int rid = r.id;
      r.p = Vec2(g.world.sites[(size_t)burn].ex * (float)TILE, (g.world.sites[(size_t)burn].ey + 2) * (float)TILE);
      LordOps::talkTo(g, r);
      const size_t before = g.lordship.holdings.size();
      if (!pick(g, "LEAD US HOME")) fail("the refugee does not ask to be led home: " + opts(g));
      g.closeDialogue();
      g.mode = Mode::Play;
      drop(g, rid);
      const realm::SettlementState* st = g.realm.settlement(g.world.sites[(size_t)burn].id);
      if (g.lordship.holdings.size() != before + 1 || g.lordship.holdings.back().how != lord::How::Resettled) fail("LEAD US HOME gave no resettled holding");
      else paths++;
      if (!st || !(st->flags & realm::SS_REBUILDING) || st->owner != mine) fail("the resettled ruin is not rebuilding under the player's banner");
    }
  }

  // ---- CONQUERED: the steward's MARCH ON ...
  {
    const size_t before = g.lordship.holdings.size();
    for (int attempt = 0; attempt < 2 && g.lordship.holdings.size() == before; attempt++) {
      sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
      if (sa < 0) { frames(g, 20); sa = LordOps::stewardActor(g, g.lordship.holdings[0]); }
      if (sa < 0) { fail("no steward for the march"); break; }
      LordOps::talkTo(g, g.actors[(size_t)sa]);
      if (!pick(g, "MARCH ON A VILLAGE")) { fail("no MARCH ON A VILLAGE: " + opts(g)); break; }
      // a village of a third realm first (the old liege is at peace with us)
      const realm::KingdomState* P = g.realm.kingdom(mine);
      const std::string oldName = P && g.realm.kingdom(P->parent) ? g.realm.kingdom(P->parent)->name : "";
      int o = -1;
      for (size_t i = 0; i < g.dlg.opts.size() && o < 0; i++)
        if (g.dlg.opts[i].label.rfind("MARCH ON ", 0) == 0 && (oldName.empty() || g.dlg.opts[i].label.find("(" + oldName + ")") == std::string::npos)) o = (int)i;
      if (o < 0) o = opt(g, "MARCH ON ");
      if (o < 0) { fail("nothing to march on: " + opts(g)); g.closeDialogue(); g.mode = Mode::Play; break; }
      g.dialogueChoose(o);
      g.closeDialogue();
      g.mode = Mode::Play;
      if (!g.lordship.march) { stepDays(g, 61); continue; }   // a peace sworn with that realm: wait a season
      // the assault: the player's banner's strength (kills at the walls)
      g.realm.contribute(g.lordship.march, 1, 10.0f);
      for (int guard = 0; guard < 12 && g.lordship.march; guard++) stepDays(g, 1);
    }
    if (g.lordship.holdings.size() < before + 1 || g.lordship.holdings.back().how != lord::How::Conquered) fail("the march took no village");
    else paths++;
  }

  // ---- the crown
  {
    const realm::KingdomState* P = g.realm.kingdom(mine);
    if ((int)g.lordship.holdings.size() >= lord::CROWN_HOLDINGS) {
      if (!g.lordship.crowned || !P || (P->ruler.title != "KING" && P->ruler.title != "QUEEN")) fail("3 holdings, no KING / QUEEN");
      if (!g_rise[(int)RiseEv::Crowned]) fail("no Crowned rise line");
    } else {
      fail("fewer than 3 holdings for the crown (" + std::to_string(g.lordship.holdings.size()) + ")");
    }
    // peace with whoever the march fought (the steward's MAKE PEACE WITH ...), so the tick check is the realm's own
    for (int k = 0; k < 3; k++) {
      sa = LordOps::stewardActor(g, g.lordship.holdings[0]);
      if (sa < 0) break;
      LordOps::talkTo(g, g.actors[(size_t)sa]);
      const bool any = pick(g, "MAKE PEACE WITH ");
      g.closeDialogue();
      g.mode = Mode::Play;
      if (!any) break;
    }
  }

  // ---- 200 days: the tick leaves the player's realm alone
  {
    const realm::KingdomState* P = g.realm.kingdom(mine);
    const std::string ruler = P ? P->ruler.name : "";
    for (int k = 0; k < 20; k++) {
      const size_t n0 = g.lordship.holdings.size();
      stepDays(g, 10);
      if (g.lordship.holdings.size() < n0) {
        out("  lord seed %llu: day %d: a holding was lost in the 200 days (now %zu); recent realm events:\n", (unsigned long long)seed, g.day, g.lordship.holdings.size());
        const auto& ev = g.realm.events();
        for (size_t e = ev.size() > 6 ? ev.size() - 6 : 0; e < ev.size(); e++)
          out("    %s a %llx b %llx site %llx day %d\n", realm::evTypeName(ev[e].type), (unsigned long long)ev[e].a, (unsigned long long)ev[e].b, (unsigned long long)ev[e].site, (int)ev[e].day);
      }
    }
    P = g.realm.kingdom(mine);
    if (!P || P->ruler.name != ruler || P->fallen || !P->player || P->tier != realm::Tier::Active) fail("the tick changed the player's ruler, felled the realm or let it go dormant");
    for (const realm::War& w : g.realm.wars()) if (w.attacker == mine && !w.forced) fail("the tick declared a war for the player's realm");
    for (const realm::KingdomState& k : g.realm.kingdoms()) if (k.parent == mine && k.origin == realm::KO_REBEL) fail("the player's realm split");
  }

  // ---- the save
  std::vector<uint8_t> a, b;
  g.serialize(a);
  Game h(seed);
  if (!h.deserialize(a)) fail("the save did not load");
  else {
    h.serialize(b);
    if (a != b) fail("the save does not round-trip byte-identically");
    bool same = h.lordship.holdings.size() == g.lordship.holdings.size() && h.lordship.crowned == g.lordship.crowned && h.lordship.treasury == g.lordship.treasury;
    for (size_t i = 0; same && i < h.lordship.holdings.size(); i++)
      same = h.lordship.holdings[i].works == g.lordship.holdings[i].works && h.lordship.holdings[i].steward == g.lordship.holdings[i].steward &&
             h.lordship.holdings[i].how == g.lordship.holdings[i].how;
    if (!same) fail("the loaded lordship differs (holdings, works, steward)");
    h.mode = Mode::Play;
    h.update(SIM_DT, Input());
    if (g.lordship.holding(seat) && g.lordship.holding(seat)->works) {
      int seen = 0;
      for (const LordOps::WorkTile& t : LordOps::stamped(h)) seen += t.work != 0;
      if (!seen && g.world.over.in(g.world.sites[(size_t)seatSi].ex, g.world.sites[(size_t)seatSi].ey)) fail("the loaded game shows no works on its first frame");
    }
  }

  // ---- CHARTER (a fresh game): fame and gold buy a village from its ruler
  {
    Game c(seed);
    prepare(c, seed);
    c.realm.renown().fame = lord::CHARTER_FAME + 10;
    c.gold = 5000;
    Actor& r = person(c, Role::Jarl, c.world.startSite, "THE JARL");
    LordOps::talkTo(c, r);
    const int gold0 = c.gold;
    if (!pick(c, "I WOULD BUY A VILLAGE'S CHARTER")) fail("the ruler sells no charter at fame 300: " + opts(c));
    else if (!pick(c, "BUY THE CHARTER OF ")) fail("no BUY THE CHARTER: " + c.dlg.text);
    c.closeDialogue();
    if (c.lordship.holdings.size() != 1 || c.lordship.holdings[0].how != lord::How::Charter) fail("the charter gave no holding");
    else {
      paths++;
      const int paidGold = gold0 - c.gold;
      out("  lord seed %llu: the charter cost %d gold\n", (unsigned long long)seed, paidGold);
      if (paidGold < 1500 || paidGold > 3500) fail("the charter's price is outside 1500..3500");
      // unrest: a heavy hand, the steward's need, a Fief quest, a feast that calms it
      lord::Holding& ch = c.lordship.holdings[0];
      ch.taxPct = lord::TAX_HEAVY;
      ch.loyalty = 20;
      ch.lastTaxDay = c.day;
      c.day += 1;
      LordOps::collect(c);
      c.mode = Mode::Play;
      frames(c, 20);
      const int csa = LordOps::stewardActor(c, c.lordship.holdings[0]);
      if (!(c.lordship.holdings[0].flags & lord::HF_UNREST)) fail("loyalty under 25 is no unrest");
      else if (csa < 0) fail("no steward in the charter village");
      else {
        LordOps::talkTo(c, c.actors[(size_t)csa]);
        if (!pick(c, "THE VILLAGE'S NEEDS") || !pick(c, "I WILL SEE TO IT")) fail("the steward gives no unrest task: " + c.dlg.text + " " + opts(c));
        bool quest = false;
        for (const Quest& q : c.quests) quest |= q.type == QType::Fief && q.state == QState::Active && lord::taskOf(q.flags) == lord::Task::Unrest;
        if (!quest) fail("no Fief quest for the unrest");
        c.lordship.treasury += 500;
        pick(c, "BACK");
        if (!pick(c, "HOLD A FEAST")) fail("no HOLD A FEAST: " + opts(c));
        c.closeDialogue();
        c.mode = Mode::Play;
        frames(c, 3);
        bool done = false;
        for (const Quest& q : c.quests) done |= q.type == QType::Fief && q.state == QState::Done && !(q.flags & QF_FAILED);
        if (!done || (c.lordship.holdings[0].flags & lord::HF_UNREST)) fail("the feast did not calm the unrest (the Fief quest still open)");
      }
      // the seat lost: the lordship ends gracefully
      const ew::Gid cs = c.lordship.holdings[0].site;
      ew::Gid other = 0;
      for (const realm::KingdomState& k : c.realm.kingdoms()) if (!k.fallen && k.id != c.lordship.liege && !k.isolated) { other = k.id; break; }
      if (other) {
        c.realm.forceOwner(cs, other, c.day);
        frames(c, 3);
        if (!c.lordship.holdings.empty() || c.realm.renown().lordOf || c.lordship.liege) fail("losing the seat did not end the lordship");
      }
    }
  }

  out("lord seed %llu: paths %d/4, holdings %zu, works %d, taxes/day %d/%d/%d, realm %s (%s), tributes paid %d refused %d, save %zu B: %s\n",
      (unsigned long long)seed, paths, g.lordship.holdings.size(), built, perDay[0], perDay[1], perDay[2], g.lordship.realmName.c_str(),
      g.realm.kingdom(mine) ? g.realm.kingdom(mine)->ruler.title.c_str() : "?", paid, refusedN, a.size(), bad ? "FAIL" : "ok");
  return bad;
}

int lordCmd(int argc, char** argv) {
  uint64_t A = 1, B = 3;
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    else if (!strcmp(argv[i], "--png") && i + 1 < argc) g_png = argv[++i];
  int bad = 0;
  for (uint64_t s = A; s <= B; s++) bad += lordSeed(s);
  out("lord: %d failures\n", bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--lord", "M12 lordship: the four paths, the steward, taxes, works, tribute and the liege's siege, the crown, the save [--seeds A..B]", lordCmd);
