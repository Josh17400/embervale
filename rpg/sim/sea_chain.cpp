// M10 "Beyond the Mist": the sea-chart chain, first contact and isolation (VISION_PLAN 2.12; rpg/sim/sea_ops.h). SEA lane.
//
// A hidden land is known by a 3-step rumour chain (2.12): an old sailor's tale on a pier (Chain::Sailor: the land is
// rumoured, LF_RUMOURED: the map may show it), its wreck (Chain::Wreck: among the timbers, the drowned captain's log
// names a sea cave), the sea cave's end (the sunken chart: Chain::Chart, LF_CHART: the Mist parts for the boat). The
// steps are one journal quest (QType::Sea, stage 1) whose marker moves to each place. The places are the world's
// (ew::EndlessSource::chartPlaces: a Shipwreck vignette and a sea cave); until the WORLD lane places them, two spots on
// the mainland side of the land's Mist stand in (open water, reached by boat).
//
// FIRST CONTACT: setting foot on a hidden land sets LF_LANDED and its contact day, the realm's kingdoms of that land stop
// being isolated (realm::Realm::contactLand: the FirstContact event and its news), SeaEv::FirstContact ("THE MIST PARTS
// ON ..."), and a quest "TELL <KING> OF THE LAND BEYOND THE MIST" to the nearest mainland capital (stage 2; done in the
// king's hall: LF_WORD, the realms meet: Realm::openContact). The hidden people start wary (playerRep -20); every quest
// done for them raises it by 15 (questDone), so three or four of their jobs make the hero a friend (25).
#include "rpg/sim/sea_ops.h"

#include "engine/audio.h"

#include <algorithm>
#include <cmath>
#include "rpg/sim/game_internal.h"
#include "rpg/world/source.h"

using namespace sea;

namespace {
float distT(int32_t ax, int32_t ay, int32_t bx, int32_t by) { return (float)std::hypot((double)ax - bx, (double)ay - by); }
std::string landName(const ew::LandPlan& L) { return L.name.empty() ? std::string("THE HIDDEN LAND") : L.name; }
// a deep-water tile on the ray from (cx, cy) at angle a, from radius r0 outward (the storm sea only when nothing else)
bool waterOnRay(ew::EndlessSource& src, int32_t cx, int32_t cy, double a, int32_t r0, int32_t& ox, int32_t& oy) {
  int32_t fx = 0, fy = 0;
  bool any = false;
  for (int32_t r = r0; r < r0 + 1200; r += 16) {
    const int32_t x = cx + (int32_t)std::lround(std::cos(a) * r), y = cy + (int32_t)std::lround(std::sin(a) * r);
    const ew::SeaKind k = src.seaAt(x, y);
    if (k == ew::SeaKind::Deep) { ox = x; oy = y; return true; }   // (open water a boat reaches: never a lake)
    if (k == ew::SeaKind::Storm && !any) { fx = x; fy = y; any = true; }
  }
  if (any) { ox = fx; oy = fy; return true; }
  return false;
}
const char* kWreckNames[] = {"THE GREY MAIDEN", "THE SALT CROWN", "THE WIDOW'S HOPE", "THE IRON GULL", "THE LAST LANTERN", "THE NORTHWIND"};
std::string wreckName(uint32_t land) { return kWreckNames[hash32(land * 2654435761u) % 6]; }
}  // namespace

bool SeaOps::chainPlaces(Game& g, uint32_t land, ew::SitePlan& wreck, ew::SitePlan& cave) {
  if (!g.world.src || !land) return false;
  ew::EndlessSource& src = *g.world.src;
  if (src.chartPlaces(land, wreck, cave)) return true;
  // stand-ins until the world places them: on the mainland side of the Mist (toward the origin), the wreck first
  const ew::LandPlan L = src.land(land);
  if (!L.id || !L.hidden) return false;
  const uint32_t h = hash32(land ^ 0x43484152u);
  const double toHome = std::atan2(-(double)L.cy, -(double)L.cx) + (((int)(h & 255) - 128) / 128.0) * 0.45;
  int32_t wx = 0, wy = 0, cx = 0, cy = 0;
  if (!waterOnRay(src, L.cx, L.cy, toHome, L.mistOut + 120, wx, wy)) return false;
  if (!waterOnRay(src, L.cx, L.cy, toHome + ((h >> 8) & 1 ? 0.30 : -0.30), L.mistOut + 380, cx, cy)) { cx = wx; cy = wy; }
  wreck = ew::SitePlan();
  wreck.id = ew::mix64((uint64_t)land * 0x9E3779B97F4A7C15ull ^ 0x5752454Bull);
  wreck.type = SiteType::Vignette;
  wreck.kind = (uint8_t)ew::VignetteKind::Shipwreck;
  wreck.gx = wx; wreck.gy = wy; wreck.ex = wx; wreck.ey = wy; wreck.w = wreck.h = 1;
  wreck.flags = ew::SPF_CHART;
  wreck.chartLand = land;
  wreck.name = wreckName(land);
  cave = wreck;
  cave.id = ew::mix64((uint64_t)land * 0xC2B2AE3D27D4EB4Full ^ 0x43415645ull);
  cave.type = SiteType::Cave;
  cave.kind = 0;
  cave.gx = cx; cave.gy = cy; cave.ex = cx; cave.ey = cy;
  cave.flags = ew::SPF_CHART | ew::SPF_SEACAVE;
  cave.name = "THE DROWNED GROTTO";
  return true;
}

int SeaOps::seaQuest(Game& g, int kind, uint32_t land, const std::string& title, int32_t tgx, int32_t tgy, ew::Gid target) {
  for (const Quest& q : g.quests)   // (one of a kind per land at a time)
    if (q.type == QType::Sea && q.stage == kind && q.state != QState::Done && (kind == 3 || (uint32_t)q.giverId == land)) return -1;
  Quest q;
  q.type = QType::Sea;
  q.stage = kind;
  q.title = title;
  q.giverId = land;
  q.targetId = target;
  q.hasPos = tgx != 0 || tgy != 0;
  q.tgx = tgx; q.tgy = tgy;
  q.gold = kind == 1 ? 400 : kind == 2 ? 600 : 0;
  q.xp = kind == 1 ? 300 : kind == 2 ? 400 : 0;
  q.need = 1;
  g.acceptQuest(q);
  return g.quests.empty() ? -1 : g.quests.back().id;
}

void SeaOps::seaQuestStep(Game& g, int kind, uint32_t land, int32_t tgx, int32_t tgy, const std::string& line, bool done) {
  for (Quest& q : g.quests) {
    if (q.type != QType::Sea || q.stage != kind || q.state != QState::Active) continue;
    if (kind == 3) {   // a fishmonger's order: line is the fish caught
      if (q.subject != line) continue;
      q.have++;
      if (q.have >= q.need) {
        q.state = QState::Complete;
        const std::string m = "REWARD READY: RETURN TO " + q.giverName;
        g.emit(Ev::QuestUpdate, g.pl().p, q.id, 2, m);
        g.say(m);
      } else g.emit(Ev::Text, g.pl().p + Vec2(0, -26), (int)rgba(255, 220, 120), 0, std::to_string(q.have) + "/" + std::to_string(q.need));
      continue;
    }
    if ((uint32_t)q.giverId != land) continue;
    q.have++;
    q.subject = line;
    q.hasPos = tgx != 0 || tgy != 0;
    q.tgx = tgx; q.tgy = tgy;
    if (done) { q.state = QState::Complete; g.completeQuest(q); }
    else g.emit(Ev::QuestUpdate, g.pl().p, q.id, 1, q.title + ": " + line);
  }
}

void SeaOps::tellOfLand(Game& g, uint32_t land, const std::string& teller) {
  LandRecord& r = g.sea.landMut(land);
  if (r.chain >= Chain::Sailor) return;
  r.chain = Chain::Sailor;
  r.flags |= LF_RUMOURED;
  ew::SitePlan wreck, cave;
  const bool places = chainPlaces(g, land, wreck, cave);
  const ew::LandPlan L = g.world.src->land(land);
  ev(g, SeaEv::ChartStep, g.pl().p, 1, land, "A LAND BEYOND THE MIST: " + landName(L));
  const int q = seaQuest(g, 1, land, "THE LAND BEYOND THE MIST", places ? wreck.ex : 0, places ? wreck.ey : 0, places ? wreck.id : 0);
  for (Quest& qq : g.quests)
    if (qq.id == q) {
      qq.giverName = teller;
      qq.subject = "FIND THE WRECK OF " + (places ? wreck.name : std::string("THE OLD SHIP"));
      qq.desc = teller + " TOLD OF A LAND BEYOND THE MIST. HIS CAPTAIN'S SHIP LIES WRECKED ON THE WAY THERE.";
    }
}

void SeaOps::firstContact(Game& g, uint32_t land) {
  LandRecord& r = g.sea.landMut(land);
  if (r.flags & LF_LANDED) return;
  r.flags |= LF_LANDED | LF_RUMOURED;
  r.contactDay = g.day;
  g.realm.contactLand(land, g.day, true);
  const ew::LandPlan L = g.world.src->land(land);
  const std::string line = "THE MIST PARTS ON " + landName(L);
  ev(g, SeaEv::FirstContact, g.pl().p, 1, land, line);
  g.emit(Ev::Notice, g.pl().p, (int)rgba(200, 230, 255), 0, line);
  snd(g, (int)Sfx::Discover, g.pl().p, 0.8f, 1.0f);
  g.say(line + ": A LAND NOBODY KNEW");
  g.gainXp(250);
  g.realm.renown().fame += 50;
  // the chain is over (whatever step it stood at: the hero found another way in)
  for (Quest& q : g.quests)
    if (q.type == QType::Sea && q.stage == 1 && (uint32_t)q.giverId == land && q.state == QState::Active) { q.state = QState::Complete; g.completeQuest(q); }
  // a few seconds on (the landing's moment first): a quest to tell the nearest mainland king (wordQuest)
  g.sea.live.wordLand = land;
  g.sea.live.wordT = 4.0f;
}

namespace {
// the quest to carry the word home: the nearest mainland king among the kingdoms met so far (their seats)
void wordQuest(Game& g, uint32_t land) {
  const ew::LandPlan L = g.world.src->land(land);
  int best = -1;
  double bd = 1e18;
  const int32_t hx = SeaOps::heroGx(g), hy = SeaOps::heroGy(g);
  for (int i = 0; i < (int)g.world.kingdoms.size(); i++) {
    const Kingdom& K = g.world.kingdoms[(size_t)i];
    if (!K.capitalId) continue;
    const ew::KingdomPlan* kp = g.world.src->kingdom(K.id);
    if (!kp || kp->hiddenLand) continue;
    const double d = std::hypot((double)K.gx - hx, (double)K.gy - hy);
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0) return;
  const Kingdom& K = g.world.kingdoms[(size_t)best];
  std::string king = "THE KING";
  if (const realm::KingdomState* ks = g.realm.kingdom(K.id)) {
    if (!ks->ruler.name.empty()) king = (ks->ruler.title.empty() ? std::string("KING") : ks->ruler.title) + " " + ks->ruler.name;
  }
  // the journal's banner holds about 28 letters: the fullest title that fits
  std::string title;
  for (const std::string& t : {"TELL " + king + " OF THE LAND BEYOND THE MIST", "TELL " + king + " OF " + landName(L), "TELL " + king + " OF THE MIST",
                               "TELL " + K.name + " OF THE MIST", std::string("WORD FROM BEYOND THE MIST")})
    if (title.empty() && t.size() <= 28) title = t;
  const int q = SeaOps::seaQuest(g, 2, land, title, K.gx, K.gy, K.capitalId);
  for (Quest& qq : g.quests)
    if (qq.id == q) {
      qq.subject = "BRING WORD TO " + K.name;
      qq.desc = "NOBODY ON THE MAINLAND KNOWS OF " + landName(L) + ". CARRY THE WORD TO " + king + " OF THE LAND BEYOND THE MIST.";
      qq.giverName = king;
    }
}
}  // namespace

void SeaOps::isolationSync(Game& g) {
  // kingdoms of a land already landed on that came into focus later (instantiated isolated) are opened quietly
  std::vector<uint32_t> open;
  for (const realm::KingdomState& k : g.realm.kingdoms()) {
    if (!k.isolated || !k.hiddenLand) continue;
    const LandRecord* r = g.sea.land(k.hiddenLand);
    if (r && (r->flags & LF_LANDED) && std::find(open.begin(), open.end(), k.hiddenLand) == open.end()) open.push_back(k.hiddenLand);
  }
  for (uint32_t land : open) g.realm.contactLand(land, g.day, false);
}

void SeaOps::chainStep(Game& g, float dt) {
  Sea::Live& L = g.sea.live;
  if (L.wordLand && (L.wordT -= dt) <= 0) { const uint32_t land = L.wordLand; L.wordLand = 0; if (g.world.src) wordQuest(g, land); }
  if ((L.chainT -= dt) > 0) return;
  L.chainT = 0.5f;
  if (!g.world.src || !g.world.endless) return;
  isolationSync(g);
  if (g.travelling()) return;
  int32_t hx = 0, hy = 0;
  {
    int x = 0, y = 0;
    gsim::overworldTile(g, x, y);
    hx = g.world.ox + x; hy = g.world.oy + y;
  }
  // first contact: the hero's feet on a hidden land
  if (!g.sea.atSea() && !g.inside && !L.swimming) {
    const uint32_t land = g.world.src->landAt(hx, hy);
    if (land && land != (uint32_t)L.landedLand) {
      L.landedLand = (int32_t)land;
      const ew::LandPlan P = g.world.src->land(land);
      if (P.hidden) firstContact(g, land);
    }
  }
  // the chain's places
  for (size_t i = 0; i < g.sea.lands.size(); i++) {
    const LandRecord rec = g.sea.lands[i];
    if (rec.chain != Chain::Sailor && rec.chain != Chain::Wreck) continue;
    ew::SitePlan wreck, cave;
    if (!chainPlaces(g, rec.land, wreck, cave)) continue;
    if (rec.chain == Chain::Sailor && distT(hx, hy, wreck.ex, wreck.ey) < 10.0f) {
      LandRecord& r = g.sea.landMut(rec.land);
      r.chain = Chain::Wreck;
      ev(g, SeaEv::ChartStep, g.pl().p, 2, rec.land, "THE CAPTAIN'S LOG");
      snd(g, (int)Sfx::Chest, g.pl().p, 0.8f);
      g.say("AMONG THE TIMBERS OF " + wreck.name + ": THE CAPTAIN'S LOG. IT NAMES " + cave.name);
      seaQuestStep(g, 1, rec.land, cave.ex, cave.ey, "FIND THE CHART IN " + cave.name, false);
      g.gainXp(80);
      continue;
    }
    if (rec.chain == Chain::Wreck) {
      bool found = false;
      const int ci = g.world.siteHandle(cave.id);
      if (g.inside && g.subSite >= 0 && ci >= 0 && g.subSite == ci) {
        // a real sea cave: the sunken chart lies at its end (the cave cleared, or its depths reached)
        const Site& S = g.world.sites[(size_t)ci];
        const Vec2 entry((float)g.sub.w * 0.5f * TILE, (float)(g.sub.h - 4) * TILE);
        found = S.cleared || len(g.pl().p - entry) > 0.55f * (float)std::max(g.sub.w, g.sub.h) * TILE;
      } else if (ci < 0 || g.world.sites[(size_t)ci].type != SiteType::Cave) {
        found = !g.inside && distT(hx, hy, cave.ex, cave.ey) < 10.0f;   // (a stand-in: the grotto's mouth on the water)
      }
      if (found) {
        LandRecord& r = g.sea.landMut(rec.land);
        r.chain = Chain::Chart;
        r.flags |= LF_CHART | LF_RUMOURED;
        const ew::LandPlan P = g.world.src->land(rec.land);
        ev(g, SeaEv::ChartStep, g.pl().p, 3, rec.land, "THE SEA-CHART OF " + landName(P));
        snd(g, (int)Sfx::QuestDone, g.pl().p, 1.0f);
        g.say("THE SUNKEN CHART: THE WAY THROUGH THE MIST TO " + landName(P));
        Item chart;
        chart.kind = ItemKind::Quest; chart.name = "SEA-CHART OF " + landName(P); chart.icon = art::Icon::Letter;
        chart.tint = rgba(170, 200, 210); chart.value = 0; chart.questId = -1;
        g.addItem(chart, false);
        seaQuestStep(g, 1, rec.land, P.cx, P.cy, "SAIL THROUGH THE MIST TO " + landName(P), false);
      }
    }
  }
}

void SeaOps::questDone(Game& g, const Quest& q) {
  // a hidden land's people warm to the stranger who helps them (wary -20; three or four jobs: Friendly 25)
  if (q.giverSite >= 0 && q.giverSite < (int)g.world.sites.size()) {
    const Site& s = g.world.sites[(size_t)q.giverSite];
    if (s.kingdom >= 0 && s.kingdom < (int)g.world.kingdoms.size()) {
      const ew::Gid kid = g.world.kingdoms[(size_t)s.kingdom].id;
      const realm::KingdomState* k = g.realm.kingdom(kid);
      if (k && k->hiddenLand) g.realm.addRep(kid, 15);
    }
  }
  // a fishmonger's order: the fish handed over
  if (q.type == QType::Sea && q.stage == 3) {
    int left = q.need;
    for (int i = (int)g.inv.size() - 1; i >= 0 && left > 0; i--) {
      Item& it = g.inv[(size_t)i];
      if (it.kind != ItemKind::Food || it.name != q.subject) continue;
      const int take = std::min(left, it.count);
      left -= take;
      it.count -= take;
      if (it.count <= 0) g.dropItem(i);
    }
  }
}
