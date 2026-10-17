// M12 "Rise and Fall" VIEW lane: the journal's fourth section, STANDING (journalSection 3; realm_hud.cpp hands it over).
// The same list-and-detail page as NEWS and HISTORY (24 px rows on a phone, UP / DOWN), its rows grouped under four
// headers:
//   THE ARMY       the army served (its arms, the rank and its badge, merit toward the next rank, the wage, days
//                  served), then each open order and mission with its place and the day it is due; past armies
//   HOLDINGS       each village held (loyalty, tax, the works built and the one raising, the tribute due and its day,
//                  the hall's coffers)
//   YOUR REALM     the player's own realm once its banner flies free (name, arms, the ruler's title, its settlements)
//   HISTORY        a timeline of the realms that rose and fell round the player (warlords, successors, civil wars,
//                  independence, vassals, alliances, falls, resettled ruins, beasts' raids: heard events or the
//                  player's own), newest first, then the land's old chronicle ("40 WINTERS AGO ...")
// The detail pane draws the picked row: arms and badges, bars, the works, a small timeline strip of the rise and fall.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/sim/lordship.h"
#include "rpg/sim/service.h"
#include "rpg/story/story.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/rise_view.h"
#include "rpg/world/source.h"

namespace {
const Color kGold(0.98f, 0.82f, 0.42f), kText(0.93f, 0.9f, 0.82f), kDim(0.62f, 0.58f, 0.52f), kHead(0.90f, 0.74f, 0.42f);
const Color kGood(0.62f, 0.92f, 0.6f), kBad(1.0f, 0.58f, 0.46f), kWarn(0.98f, 0.80f, 0.45f);
Color colOf(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
Color liftC(Color c) { return Color(c.r * 0.40f + 0.58f, c.g * 0.40f + 0.55f, c.b * 0.40f + 0.48f, c.a); }

std::vector<std::string> wrapW(const std::string& s, int maxChars) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&]() {
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) { lines.push_back(line); line.clear(); }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char c : s) { if (c == ' ') flush(); else word += c; }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}
std::string cutTo(const std::string& s, int chars) { return (int)s.size() <= chars ? s : s.substr(0, (size_t)std::max(1, chars)); }

// a settlement's name: the loaded site's, else its region plan's
std::string siteNameImpl(const Game& g, ew::Gid id) {
  if (!id) return std::string();
  const int h = g.world.siteHandle(id);
  if (h >= 0) return g.world.sites[(size_t)h].name;
  if (g.world.src && ew::idKind(id) == ew::IdKind::Site) {
    const ew::RegionPlan& R = g.world.src->region(ew::idRx(id), ew::idRy(id));
    for (const ew::SitePlan& p : R.sites) if (p.id == id) return p.name;
  }
  return std::string();
}
std::string siteName(const Game& g, ew::Gid id) { return siteNameImpl(g, id); }
std::string realmName(const Game& g, ew::Gid k) {
  const rui::Look L = rui::look(g, k);
  return L.ok ? rui::realmTitle(L, false) : std::string("A FALLEN REALM");
}

// the rise and fall the HISTORY timeline keeps (the realm's events of these kinds)
bool riseType(realm::EvType t) {
  using E = realm::EvType;
  switch (t) {
    case E::WarlordRose: case E::WarbandRaid: case E::SuccessorRose: case E::Reunified: case E::AllianceMade: case E::AllyJoined:
    case E::Vassalised: case E::TributeRefused: case E::Independence: case E::FiefGranted: case E::Enlisted: case E::Promoted:
    case E::TermsCarried: case E::BossWoke: case E::Rebuilt: case E::KingdomFell: case E::KingdomRose: case E::CivilWar:
    case E::Succession: case E::TownTaken: case E::TownBurned: case E::Resettled: case E::Ruined: case E::BeastRaid: case E::Peace:
    case E::WarDeclared:
      return true;
    default: return false;
  }
}
Color riseColour(realm::EvType t) {
  using E = realm::EvType;
  switch (t) {
    case E::WarlordRose: case E::WarbandRaid: case E::KingdomFell: case E::CivilWar: case E::TownTaken: case E::TownBurned:
    case E::Ruined: case E::BeastRaid: case E::BossWoke: case E::TributeRefused: case E::WarDeclared: return kBad;
    case E::SuccessorRose: case E::Reunified: case E::Independence: case E::KingdomRose: case E::FiefGranted: case E::Promoted:
    case E::Enlisted: return kGold;
    case E::AllianceMade: case E::AllyJoined: case E::Peace: case E::Resettled: case E::Rebuilt: case E::TermsCarried: return kGood;
    default: return kWarn;
  }
}
// the timeline's short words for a rise-and-fall event ("A WARLORD ROSE - RAVENMOOR")
std::string riseTitle(const Game& g, const realm::WorldEvent& e) {
  using E = realm::EvType;
  std::string t;
  switch (e.type) {
    case E::WarlordRose: t = "A WARLORD ROSE"; break;
    case E::WarbandRaid: t = "A WARBAND RAIDED"; break;
    case E::SuccessorRose: t = "A SUCCESSOR CROWNED"; break;
    case E::Reunified: t = "A REALM REUNITED"; break;
    case E::AllianceMade: t = "AN ALLIANCE"; break;
    case E::AllyJoined: t = "AN ALLY MARCHED"; break;
    case E::Vassalised: t = "A VASSAL BENT THE KNEE"; break;
    case E::TributeRefused: t = "TRIBUTE REFUSED"; break;
    case E::Independence: t = "INDEPENDENCE"; break;
    case E::FiefGranted: t = "YOUR LORDSHIP"; break;
    case E::Enlisted: t = "YOU ENLISTED"; break;
    case E::Promoted: t = "YOU ROSE IN RANK"; break;
    case E::TermsCarried: t = e.mag == 0 ? "THE TERMS REFUSED" : e.mag == 2 ? "AN ALLIANCE SWORN" : e.mag == 3 ? "TRIBUTE AGREED" : "PEACE SIGNED"; break;
    case E::BossWoke: t = "A BEAST WOKE"; break;
    case E::Rebuilt: t = "A RUIN LIVES AGAIN"; break;
    case E::BeastRaid: t = "A BEAST RAIDED"; break;
    default: t = realm::evTypeName(e.type); break;
  }
  std::string place = siteName(g, e.site);
  if (place.empty() && e.a) { const rui::Look k = rui::look(g, e.a); if (k.ok) place = k.name; }
  return place.empty() ? t : t + " - " + place;
}

// one row of the page
enum RowKind { R_HEAD, R_ARMY, R_ORDER, R_MISSION, R_PAST, R_HOLDING, R_REALM, R_EVENT, R_CHRONICLE, R_NONE };
struct Row { RowKind kind; int idx; std::string title; Color col; };

std::vector<const realm::WorldEvent*> riseEvents(const Game& g) {
  std::vector<const realm::WorldEvent*> v;
  const ew::Gid mine = g.realm.playerRealm(), sworn = g.service.sworn;
  for (const realm::WorldEvent& e : g.realm.events()) {
    if (!riseType(e.type)) continue;
    const bool own = (mine && (e.a == mine || e.b == mine)) || e.type == realm::EvType::Enlisted || e.type == realm::EvType::Promoted ||
                     e.type == realm::EvType::FiefGranted || (sworn && (e.type == realm::EvType::KingdomFell || e.type == realm::EvType::CivilWar) && (e.a == sworn || e.b == sworn));
    if (!e.heard && !own) continue;
    v.push_back(&e);
  }
  std::stable_sort(v.begin(), v.end(), [](const realm::WorldEvent* a, const realm::WorldEvent* b) { return a->id > b->id; });
  if (v.size() > 60) v.resize(60);
  return v;
}
// the kingdom whose old chronicle the page closes on: the player's own, else the sworn army's, else the land's
ew::Gid chronicleRealm(const Game& g) {
  if (g.realm.playerRealm()) return g.realm.playerRealm();
  if (g.service.sworn) return g.service.sworn;
  if (!g.world.src) return 0;
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  return rui::landOwnerAt(g, gx, gy);
}

std::vector<Row> buildRows(const Game& g, std::vector<const realm::WorldEvent*>& evs, std::vector<realm::HistoryEntry>& chron) {
  std::vector<Row> rows;
  const service::Service& S = g.service;
  rows.push_back({R_HEAD, 0, "THE ARMY", kHead});
  if (S.enlisted()) {
    rows.push_back({R_ARMY, 0, std::string(service::rankName(S.rank)) + " - " + rui::look(g, S.sworn).name, kText});
    for (size_t i = 0; i < S.orders.size(); i++)
      if (S.orders[i].state == service::OS_OPEN)
        rows.push_back({R_ORDER, (int)i, "ORDER: " + std::string(service::orderName(S.orders[i].kind)) + " " + siteName(g, S.orders[i].site), kWarn});
    for (size_t i = 0; i < S.missions.size(); i++)
      if (S.missions[i].state == service::OS_OPEN)
        rows.push_back({R_MISSION, (int)i, "ENVOY: " + std::string(service::missionName(S.missions[i].kind)) + " " + rui::look(g, S.missions[i].to).name, kWarn});
  } else rows.push_back({R_NONE, 0, "SWORN TO NO ARMY", kDim});
  for (size_t i = 0; i < S.past.size() && i < 4; i++) {
    const service::Past& p = S.past[S.past.size() - 1 - i];
    const char* how = p.how == service::PAST_DESERTED ? "DESERTED" : p.how == service::PAST_DISBANDED ? "DISBANDED" : "DISCHARGED";
    rows.push_back({R_PAST, (int)(S.past.size() - 1 - i), std::string(how) + " - " + rui::look(g, p.kingdom).name, kDim});
  }
  rows.push_back({R_HEAD, 1, "HOLDINGS", kHead});
  if (g.lordship.holdings.empty()) rows.push_back({R_NONE, 1, "NO LAND OF YOUR OWN", kDim});
  for (size_t i = 0; i < g.lordship.holdings.size(); i++) {
    const lord::Holding& h = g.lordship.holdings[i];
    std::string n = siteName(g, h.site);
    if (n.empty()) n = "YOUR VILLAGE";
    rows.push_back({R_HOLDING, (int)i, n + (i == 0 ? " (SEAT)" : ""), h.loyalty < 25 ? kBad : kText});
  }
  if (g.realm.playerRealm()) {
    rows.push_back({R_HEAD, 2, "YOUR REALM", kHead});
    rows.push_back({R_REALM, 0, realmName(g, g.realm.playerRealm()), kGold});
  }
  rows.push_back({R_HEAD, 3, "HISTORY", kHead});
  evs = riseEvents(g);
  for (size_t i = 0; i < evs.size(); i++) rows.push_back({R_EVENT, (int)i, riseTitle(g, *evs[i]), riseColour(evs[i]->type)});
  const ew::Gid ck = chronicleRealm(g);
  chron = ck ? g.realm.history(ck) : std::vector<realm::HistoryEntry>();
  std::reverse(chron.begin(), chron.end());   // the newest of the old days first
  if (chron.size() > 12) chron.resize(12);
  for (size_t i = 0; i < chron.size(); i++)
    rows.push_back({R_CHRONICLE, (int)i, chron[i].yearsAgo <= 0 ? std::string("THIS YEAR") : chron[i].yearsAgo == 1 ? std::string("A WINTER AGO") : std::to_string(chron[i].yearsAgo) + " WINTERS AGO", Color(0.86f, 0.80f, 1.0f)});
  if (evs.empty() && chron.empty()) rows.push_back({R_NONE, 3, "NO TALES HEARD YET", kDim});
  return rows;
}

struct Geo { float lx, lw, div, dx, dw, pitch, listBottom; int rows; };
Geo geoOf(bool touch, float top) {
  Geo G;
  G.pitch = touch ? 24.0f : 12.0f;
  G.lx = 18; G.lw = std::floor(200 + std::max(0, Pix::W - 480) * 0.4f);
  G.div = G.lx + G.lw + 8; G.dx = G.div + 10; G.dw = (float)Pix::W - 20 - G.dx;
  G.listBottom = (float)Pix::H - (touch ? 50.0f : 26.0f);
  G.rows = std::max(3, (int)((G.listBottom - top - 6) / G.pitch));
  return G;
}
}  // namespace

std::string RiseViewOps::siteName(const Game& g, ew::Gid id) { return siteNameImpl(g, id); }

// ---------------------------------------------------------------- the page
void RiseViewOps::drawStanding(View& v, Game& g, float top) {
  Pix& P = pix(v);
  const bool tch = touch(v);
  const Geo G = geoOf(tch, top);
  std::vector<const realm::WorldEvent*> evs;
  std::vector<realm::HistoryEntry> chron;
  const std::vector<Row> rows = buildRows(g, evs, chron);
  const int n = (int)rows.size();
  int& sel = v.menuSel_;
  int& scroll = v.menuScroll_;
  sel = std::clamp(sel, 0, std::max(0, n - 1));
  if (sel < scroll) scroll = sel;
  if (sel >= scroll + G.rows) scroll = sel - G.rows + 1;
  scroll = std::clamp(scroll, 0, std::max(0, n - G.rows));
  st(v).stats.standingRows = n;
  // ---- the list: headers as gold rules, rows under them indented, the picked one lit
  for (int r = 0; r < G.rows && scroll + r < n; r++) {
    const int i = scroll + r;
    const Row& R = rows[(size_t)i];
    const float rowTop = top + 4 + r * G.pitch, yy = rowTop + std::floor((G.pitch - 8) / 2);
    if (i == sel) P.rect(G.lx, rowTop, G.lw, G.pitch - (tch ? 1 : 0), Color(0.3f, 0.22f, 0.12f, 0.8f));
    else if (tch && (r & 1) && R.kind != R_HEAD) P.rect(G.lx, rowTop, G.lw, G.pitch - 1, Color(0.16f, 0.12f, 0.08f, 0.35f));
    if (R.kind == R_HEAD) {
      P.text(G.lx + 4, yy, R.title, 1, R.col);
      const float rx = G.lx + 8 + P.textW(R.title, 1);
      P.rect(rx, yy + 3, std::max(0.0f, G.lx + G.lw - 4 - rx), 1, Color(R.col.r, R.col.g, R.col.b, 0.45f));
      continue;
    }
    const int fit = std::max(4, (int)((G.lw - 18) / 6));
    float tx = G.lx + 10;
    if (R.kind == R_EVENT) {   // a timeline tick: a coloured pip on a thin vertical line
      P.rect(G.lx + 6, rowTop, 1, G.pitch, Color(0.45f, 0.36f, 0.22f, 0.8f));
      P.rect(G.lx + 5, yy + 2, 3, 3, R.col);
    }
    P.text(tx, yy, cutTo(R.title, fit), 1, R.col);
  }
  if (n > G.rows) {
    const std::string cnt = std::to_string(scroll + 1) + "-" + std::to_string(std::min(n, scroll + G.rows)) + " OF " + std::to_string(n);
    if (tch) {
      button(v, G.lx, (float)Pix::H - 46, 52, 24, "UP", false);
      button(v, G.lx + G.lw - 52, (float)Pix::H - 46, 52, 24, "DOWN", false);
      P.text(G.lx + G.lw / 2, (float)Pix::H - 38, cnt, 1, kDim, 1);
    } else P.text(G.lx + G.lw / 2, (float)Pix::H - 18, cnt, 1, kDim, 1);
  }
  P.rect(G.div, top - v.journalStripH() + 8, 1, Pix::H - (top - v.journalStripH()) - 24, Color(0.4f, 0.32f, 0.2f));
  if (n == 0) return;
  // ---- the detail pane
  const Row& R = rows[(size_t)sel];
  const int per = std::max(10, (int)(G.dw / 6));
  float y = top - v.journalStripH() + 12;
  const float yMax = (float)Pix::H - 24;
  auto line = [&](const std::string& s, Color c) {
    for (const std::string& l : wrapW(s, per)) { if (y > yMax) return; P.text(G.dx, y, l, 1, c); y += 10; }
  };
  auto bar = [&](float x, float yy, float w, float k, Color c) {
    P.rect(x - 1, yy - 1, w + 2, 6, Color(0.05f, 0.04f, 0.06f, 0.9f));
    P.rect(x, yy, w, 4, Color(c.r * 0.25f, c.g * 0.25f, c.b * 0.25f, 0.95f));
    P.rect(x, yy, std::floor(w * std::clamp(k, 0.0f, 1.0f)), 4, c);
    P.rect(x, yy, std::floor(w * std::clamp(k, 0.0f, 1.0f)), 1, Color(std::min(1.0f, c.r + 0.3f), std::min(1.0f, c.g + 0.3f), std::min(1.0f, c.b + 0.3f)));
  };
  auto realmLine = [&](ew::Gid k, int size) {   // the arms and the realm's title in its colour
    if (!k || y > yMax - 10) return;
    cult::Heraldry h;
    uint32_t kc = 0;
    armsOf(g, k, h, &kc);
    const Tex& sh = armsTex(v, h, size);
    P.blit(sh, G.dx, y - 2);
    const std::string t = realmName(g, k);
    for (const std::string& l : wrapW(t, (int)((G.dw - size - 6) / 6))) { P.text(G.dx + size + 5, y + (size > 14 ? 3 : 1), l, 1, liftC(colOf(kc))); y += 10; }
    y = std::max(y, y + (size > 14 ? (float)size - 8 : 4.0f));
  };
  const service::Service& S = g.service;
  switch (R.kind) {
    case R_HEAD: case R_NONE: {
      static const char* kAbout[4] = {
          "CAPTAINS AND THE TOWN WATCH TAKE RECRUITS. A RECRUIT EARNS A WAGE EACH WEEK; MERIT IN THE FIELD RAISES THEM TO SERGEANT, CAPTAIN AND MARSHAL.",
          "A LIEGE GRANTS A VILLAGE TO ITS CAPTAINS; A RULER SELLS A CHARTER TO THE FAMOUS. A LORD COLLECTS TAXES, RAISES WORKS AND OWES TRIBUTE EVERY 30 DAYS.",
          "THE REALM YOU RULE. NO CROWN BUT YOURS.",
          "THE REALMS THAT ROSE AND FELL AROUND YOU: WARLORDS GROWN FROM BANDIT CAMPS, SUCCESSORS OF FALLEN CROWNS, ALLIANCES, VASSALS AND RESETTLED RUINS."};
      const int h = R.kind == R_HEAD ? R.idx : std::clamp(R.idx, 0, 3);
      P.text(G.dx, y, rows[(size_t)sel].kind == R_HEAD ? R.title : rows[(size_t)sel].title, 1, kHead);
      y += 14;
      line(kAbout[h], kDim);
      if (h == 0 && S.enlisted()) { y += 6; realmLine(S.sworn, 16); }
      break;
    }
    case R_ARMY: {
      cult::Heraldry h;
      uint32_t kc = 0;
      armsOf(g, S.sworn, h, &kc);
      // the badge large beside the rank's name, the army's arms and title under it
      const Tex& bd = badgeTex(v, (int)S.rank, h.field, h.charge, 22);
      P.blit(bd, G.dx, y - 2);
      P.text(G.dx + bd.w + 6, y + 2, service::rankName(S.rank), 1, kGold);
      P.text(G.dx + bd.w + 6, y + 13, "WAGE " + std::to_string(service::wageOf(S.rank)) + " GOLD A WEEK", 1, kText);
      y += (float)bd.h + 4;
      realmLine(S.sworn, 12);
      y += 4;
      const int need = service::meritToRise(S.rank);
      if (need > 0) {
        P.text(G.dx, y, "MERIT " + std::to_string(S.merit) + " / " + std::to_string(need), 1, kDim);
        bar(G.dx + 90, y + 2, std::max(40.0f, G.dw - 96), (float)S.merit / (float)need, Color(0.92f, 0.70f, 0.28f));
        y += 11;
        P.text(G.dx, y, std::string("NEXT: ") + service::rankName((service::Rank)((int)S.rank + 1)), 1, kDim);
        y += 12;
      } else { P.text(G.dx, y, "THE HIGHEST RANK OF THE ARMY", 1, kGold); y += 12; }
      line("SERVED " + std::to_string(std::max(0, g.day - (int)S.enlistDay)) + " DAYS. ORDERS CARRIED OUT " + std::to_string(S.ordersDone) +
               (S.ordersFailed ? ", FAILED " + std::to_string(S.ordersFailed) : std::string()) + ". ENVOY'S ERRANDS " + std::to_string(S.missionsDone) + ".",
           kText);
      if (service::squadOf(S.rank) > 0) line(std::to_string(service::squadOf(S.rank)) + " SOLDIERS MARCH WITH YOU ON AN ORDER.", kText);
      break;
    }
    case R_ORDER: case R_MISSION: {
      const bool ord = R.kind == R_ORDER;
      const uint16_t due = ord ? S.orders[(size_t)R.idx].dueDay : S.missions[(size_t)R.idx].dueDay;
      const ew::Gid site = ord ? S.orders[(size_t)R.idx].site : S.missions[(size_t)R.idx].toSite;
      P.text(G.dx, y, ord ? "CAMPAIGN ORDERS" : "AN ENVOY'S ERRAND", 1, kHead);
      y += 12;
      line(R.title.substr(R.title.find(':') + 2), kText);
      y += 4;
      const std::string place = siteName(g, site);
      if (!place.empty()) line("AT " + place + ".", kDim);
      const int left = (int)due - g.day;
      line(left > 1 ? "DUE IN " + std::to_string(left) + " DAYS." : left == 1 ? "DUE TOMORROW." : left == 0 ? "DUE TODAY." : "OVERDUE.", left < 2 ? kWarn : kDim);
      y += 6;
      if (ord) { line("+" + std::to_string(S.orders[(size_t)R.idx].merit) + " MERIT WHEN DONE.", kGold); realmLine(S.sworn, 12); }
      else {
        P.text(G.dx, y, "FROM", 1, kDim); y += 11; realmLine(S.missions[(size_t)R.idx].from, 12);
        y += 2; P.text(G.dx, y, "TO", 1, kDim); y += 11; realmLine(S.missions[(size_t)R.idx].to, 12);
      }
      break;
    }
    case R_PAST: {
      const service::Past& p = S.past[(size_t)R.idx];
      P.text(G.dx, y, R.title.substr(0, R.title.find(" -")), 1, p.how == service::PAST_DESERTED ? kBad : kHead);
      y += 12;
      realmLine(p.kingdom, 12);
      line(std::string("YOUR RANK: ") + service::rankName(p.rank) + ". " + rui::daysAgo(g.day, p.day) + ".", kDim);
      if (p.how == service::PAST_DESERTED) line("THAT ARMY WILL NOT TAKE YOU BACK.", kBad);
      break;
    }
    case R_HOLDING: {
      const lord::Holding& h = g.lordship.holdings[(size_t)R.idx];
      P.text(G.dx, y, R.title, 1, kGold);
      y += 11;
      static const char* kHow[] = {"", "GRANTED BY YOUR LIEGE", "BOUGHT BY CHARTER", "RESETTLED FROM RUIN", "TAKEN BY YOUR BANNER", "FOUNDED"};
      line(std::string(kHow[std::min<int>((int)h.how, 5)]) + ", " + rui::daysAgo(g.day, h.sinceDay) + ".", kDim);
      y += 3;
      P.text(G.dx, y, "LOYALTY", 1, kDim);
      bar(G.dx + 54, y + 2, std::max(40.0f, G.dw - 90), h.loyalty / 100.0f, h.loyalty < 25 ? Color(0.9f, 0.3f, 0.25f) : Color(0.42f, 0.78f, 0.4f));
      P.text(G.dx + G.dw - 30, y, std::to_string(h.loyalty), 1, h.loyalty < 25 ? kBad : kText);
      y += 11;
      P.text(G.dx, y, "TAX " + std::to_string(h.taxPct) + "%", 1, kText);
      P.text(G.dx + 70, y, "COFFERS " + std::to_string(g.lordship.treasury), 1, kGold);
      y += 12;
      // the works: built in gold, the one raising with its days left, the rest dim
      std::string built, raising;
      for (uint16_t bit = 1; bit && bit <= lord::LW_SHRINE; bit = (uint16_t)(bit << 1)) {
        const lord::WorkInfo* w = lord::workInfo(bit);
        if (!w) continue;
        if (h.works & bit) built += (built.empty() ? "" : ", ") + std::string(w->name);
        if (h.building == bit) raising = std::string(w->name) + " - " + std::to_string(std::max(0, (int)h.buildDone - g.day)) + " DAYS LEFT";
      }
      line("WORKS: " + (built.empty() ? std::string("NONE YET") : built), built.empty() ? kDim : kText);
      if (!raising.empty()) line("RAISING: " + raising, kWarn);
      if (h.militia) line("MILITIA: " + std::to_string(h.militia) + " WHO ANSWER YOUR CALL", kText);
      y += 4;
      if (g.lordship.liege && !g.lordship.realm) {
        const int left = (int)g.lordship.tributeDay - g.day;
        line("TRIBUTE DUE: " + std::to_string(g.lordship.tributeDue) + " GOLD " + (left > 0 ? "IN " + std::to_string(left) + " DAYS" : std::string("NOW")), left <= 3 ? kWarn : kText);
        if (g.lordship.missed) line(std::to_string(g.lordship.missed) + " TRIBUTE" + (g.lordship.missed > 1 ? "S" : "") + " WITHHELD.", kBad);
        y += 2;
        P.text(G.dx, y, "SWORN TO", 1, kDim); y += 11;
        realmLine(g.lordship.liege, 12);
      } else if (g.lordship.realm) line("YOU ANSWER TO NO CROWN.", kGold);
      break;
    }
    case R_REALM: {
      const ew::Gid k = g.realm.playerRealm();
      const realm::KingdomState* K = g.realm.kingdom(k);
      cult::Heraldry h;
      uint32_t kc = 0;
      armsOf(g, k, h, &kc);
      const Tex& sh = armsTex(v, h, 24);
      P.blit(sh, G.dx, y - 2);
      const rui::Look L = rui::look(g, k);
      float ty = y;
      for (const std::string& l : wrapW(rui::realmTitle(L, true), (int)((G.dw - 30) / 6))) { P.text(G.dx + 30, ty + 1, l, 1, liftC(colOf(kc))); ty += 10; }
      if (!L.ruler.empty()) { P.text(G.dx + 30, ty + 1, L.ruler, 1, kGold); ty += 10; }
      y = std::max(y + 26, ty + 4);
      if (K) {
        line("FREE SINCE " + rui::daysAgo(g.day, g.lordship.independenceDay) + ". " + std::to_string(K->settlements.size()) + " SETTLEMENT" + (K->settlements.size() == 1 ? "" : "S") + ":", kText);
        for (ew::Gid s : K->settlements) { std::string nm = siteName(g, s); if (!nm.empty()) line("  " + nm, kDim); }
      }
      break;
    }
    case R_EVENT: {
      const realm::WorldEvent& e = *evs[(size_t)R.idx];
      line(R.title, R.col);
      P.text(G.dx, y, rui::daysAgo(g.day, e.day), 1, kDim);
      y += 13;
      // a small timeline strip: every rise-and-fall event on the page as a tick in the order they came (oldest at the
      // left), the picked one lit, the first and last days under its ends
      if (evs.size() > 1) {
        const float tw = G.dw - 8;
        const int n2 = (int)evs.size();
        P.rect(G.dx, y + 3, tw, 1, Color(0.45f, 0.36f, 0.22f));
        for (int qi = 0; qi < n2; qi++) {
          const realm::WorldEvent* q = evs[(size_t)(n2 - 1 - qi)];
          const float x = G.dx + std::floor(tw * (float)qi / (float)(n2 - 1));
          const Color qc = riseColour(q->type);
          P.rect(x, y + 1, 1, 5, Color(qc.r, qc.g, qc.b, q == &e ? 1.0f : 0.55f));
          if (q == &e) { P.rect(x - 1, y, 3, 7, qc); P.rect(x, y - 1, 1, 9, Color(1, 1, 0.9f)); }
        }
        P.text(G.dx, y + 9, "DAY " + std::to_string(evs.back()->day), 1, kDim);
        P.text(G.dx + tw, y + 9, "DAY " + std::to_string(evs.front()->day), 1, kDim, 2);
        y += 22;
      }
      line(story::newsLine(g, e, 0), kText);
      y += 5;
      for (ew::Gid kid : {e.a, e.b}) {
        if (!kid || (kid == e.b && e.b == e.a)) continue;
        if (!rui::look(g, kid).ok) continue;
        realmLine(kid, 12);
        y += 3;
      }
      break;
    }
    case R_CHRONICLE: {
      const realm::HistoryEntry& hEnt = chron[(size_t)R.idx];
      P.text(G.dx, y, R.title, 1, Color(0.86f, 0.80f, 1.0f));
      y += 13;
      line(hEnt.text, kText);
      y += 6;
      realmLine(chronicleRealm(g), 12);
      break;
    }
    default: break;
  }
}

bool RiseViewOps::standingTap(View& v, Game& g, Vec2 p, float top) {
  const bool tch = touch(v);
  const Geo G = geoOf(tch, top);
  std::vector<const realm::WorldEvent*> evs;
  std::vector<realm::HistoryEntry> chron;
  const int n = (int)buildRows(g, evs, chron).size();
  int& sel = v.menuSel_;
  int& scroll = v.menuScroll_;
  for (int r = 0; r < G.rows && scroll + r < n; r++)
    if (p.x >= G.lx && p.x < G.lx + G.lw && p.y >= top + 4 + r * G.pitch && p.y < top + 4 + (r + 1) * G.pitch) {
      sel = scroll + r;
      audio(v).play(Sfx::MenuMove);
      return true;
    }
  if (tch && n > G.rows) {
    const float by = (float)Pix::H - 46;
    if (p.y >= by - 2 && p.y < by + 26 && p.x >= G.lx && p.x < G.lx + 52) { sel = std::max(0, sel - G.rows); scroll = std::max(0, scroll - G.rows); return true; }
    if (p.y >= by - 2 && p.y < by + 26 && p.x >= G.lx + G.lw - 52 && p.x < G.lx + G.lw) {
      scroll = std::min(std::max(0, n - G.rows), scroll + G.rows);
      sel = std::min(n - 1, std::max(sel + G.rows, scroll));
      return true;
    }
  }
  return false;
}

void RiseViewOps::standingKey(View& v, Game& g, int key) { (void)v; (void)g; (void)key; }
