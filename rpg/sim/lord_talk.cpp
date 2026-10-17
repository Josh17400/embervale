// M12 "Rise and Fall": the lordship's talk (rpg/sim/lord_ops.h). LORD lane. Options in [DLG_LORD, DLG_LORD_END).
//   the steward (Role::Steward of a holding; with none named, the elder / innkeeper): THE HALL'S COFFERS (and TAKE), THE
//     TAX (low / fair / heavy: coin against love), RAISE WORKS (gold and days), MUSTER THE MILITIA, HOLD A FEAST, THE
//     VILLAGE'S NEEDS (a fief task), THE TRIBUTE (pay now, withhold, resume), RAISE MY OWN BANNER (a confirm line that says
//     what it will cost), once free: MARCH ON A VILLAGE (a conquest), ACCEPT THE PEACE.
//   rulers (King / Jarl): I ASK FOR A LORDSHIP (a Captain of their army), I WOULD BUY A VILLAGE'S CHARTER (fame 300; the
//     price by the village's prosperity), WE ACCEPT YOUR TERMS (the old liege's peace).
//   refugees (Role::Refugee): LEAD US HOME (their burned / abandoned village resettled under the player), COME AND SETTLE
//     IN ... (a holding's Settlers task).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/audio.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service.h"
#include "rpg/sim/talk.h"
#include "rpg/world/source.h"

namespace {
enum : int {
  K_COFFERS = 1, K_TAKE = 2, K_PAY = 3, K_FREE = 4, K_ASK = 5, K_CHARTER = 6, K_FREE_YES = 7, K_CHARTER_BUY = 8,
  K_TAX = 10, K_TAX_SET = 11, K_BUILD = 20, K_BUILD_DO = 21, K_MUSTER = 30, K_TRIBUTE = 31, K_WITHHOLD = 32, K_RESUME = 33,
  K_MARCH = 40, K_MARCH_DO = 41, K_PEACE = 50, K_FEAST = 51, K_NEEDS = 60, K_NEEDS_DO = 61, K_LEAD = 70, K_SETTLE = 71,
  K_BACK = 90, K_TERMS = 91, K_WARPEACE = 92
};

ew::Gid ownerOfSite(const Game& g, int si) {
  const Site& s = g.world.sites[(size_t)si];
  return s.kingdom >= 0 && s.kingdom < (int)g.world.kingdoms.size() ? g.world.kingdoms[(size_t)s.kingdom].id : 0;
}
std::string realmName(const Game& g, ew::Gid k) {
  if (const realm::KingdomState* K = g.realm.kingdom(k)) return K->name;
  if (g.world.src) if (const ew::KingdomPlan* kp = g.world.src->kingdom(k)) return kp->name;
  return "THE CROWN";
}
std::string siteName(const Game& g, ew::Gid site) {
  const int si = g.world.siteHandle(site);
  return si >= 0 ? g.world.sites[(size_t)si].name : std::string("THE VILLAGE");
}
int actorSite(const Game& g, const Actor& a) {
  int site = a.site;
  if (site < 0 && a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) site = g.world.over.bldgs[(size_t)a.bldg].site;
  return site >= 0 && site < (int)g.world.sites.size() ? site : -1;
}
std::string num(int v) { return std::to_string(v); }
// the MARCH ON page's settlements (DlgOpt::arg indexes it: a site id does not fit an int)
std::vector<ew::Gid>& marchTargets() { static std::vector<ew::Gid> t; return t; }

// the holding the conversation is about: the steward's, or the elder's village
lord::Holding* talkHolding(Game& g) {
  const int ai = LordOps::findActor(g, g.dlg.actor);
  if (ai < 0) return nullptr;
  const Actor& a = g.actors[(size_t)ai];
  for (lord::Holding& h : g.lordship.holdings) {
    const int sa = LordOps::stewardActor(g, h);
    if (sa >= 0 && g.actors[(size_t)sa].id == a.id) return &h;
  }
  const int si = actorSite(g, a);
  return si >= 0 ? g.lordship.holding(g.world.sites[(size_t)si].id) : nullptr;
}

// the kingdom's village nearest the hero that is free to grant (not besieged, not ruined), -1 none
int grantable(Game& g, ew::Gid k) {
  int best = -1;
  float bd = 1e30f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != SiteType::Village || g.lordship.holding(s.id)) continue;
    if (g.realm.ownerOf(s.id, ownerOfSite(g, (int)i)) != k) continue;
    if (const realm::SettlementState* st = g.realm.settlement(s.id)) if (st->flags & (realm::SS_BESIEGED | realm::SS_RUINED | realm::SS_ABANDONED)) continue;
    const float dx = s.ex - g.pl().p.x / TILE, dy = s.ey - g.pl().p.y / TILE;
    const float d2 = dx * dx + dy * dy;
    if (d2 < bd) { bd = d2; best = (int)i; }
  }
  return best;
}

// the village a refugee fled: the camp's host's refugeesFrom, else the nearest burned / abandoned / ruined village
int refugeeHome(Game& g, const Actor& a) {
  g.realmNoteSites();
  const float ax = a.p.x / TILE, ay = a.p.y / TILE;
  float bd = 70.0f * 70.0f;
  int best = -1;
  for (int si : g.world.nearSites) {
    const Site& s = g.world.sites[(size_t)si];
    if (!s.settlement()) continue;
    const realm::SettlementState* st = g.realm.settlement(s.id);
    if (!st || !(st->flags & realm::SS_REFUGEES) || !st->refugeesFrom) continue;
    const float d = (s.ex - ax) * (s.ex - ax) + (s.ey - ay) * (s.ey - ay);
    if (d < bd && g.world.siteHandle(st->refugeesFrom) >= 0) { bd = d; best = g.world.siteHandle(st->refugeesFrom); }
  }
  if (best >= 0) return best;
  bd = 240.0f * 240.0f;
  for (size_t i = 0; i < g.world.sites.size(); i++) {
    const Site& s = g.world.sites[i];
    if (s.type != SiteType::Village) continue;
    const realm::SettlementState* st = g.realm.settlement(s.id);
    if (!st || !(st->flags & (realm::SS_BURNED | realm::SS_ABANDONED | realm::SS_RUINED))) continue;
    const float d = (s.ex - ax) * (s.ex - ax) + (s.ey - ay) * (s.ey - ay);
    if (d < bd) { bd = d; best = (int)i; }
  }
  return best;
}

// the need the steward speaks of first (Task::None: all is well)
lord::Task needOf(Game& g, const lord::Holding& h) {
  if (h.flags & lord::HF_UNREST) return lord::Task::Unrest;
  if (g.realm.siegeAt(h.site)) return lord::Task::Raid;
  const realm::SettlementState* st = g.realm.settlement(h.site);
  if (!st) return lord::Task::None;
  for (const realm::Camp& c : g.realm.camps()) {
    if (c.flags & (realm::CF_CLEARED | realm::CF_WARLORD)) continue;
    const int si = g.world.siteHandle(c.site);
    if (si >= 0 && g.world.sites[(size_t)si].cleared) continue;
    const int64_t dx = c.gx - st->gx, dy = c.gy - st->gy;
    if (dx * dx + dy * dy < 140LL * 140LL) return lord::Task::Camp;
  }
  if (st->popPct < 70) return lord::Task::Settlers;
  return lord::Task::None;
}
const char* needLine(lord::Task t) {
  switch (t) {
    case lord::Task::Unrest: return "THE PEOPLE GRUMBLE AT THE TAX, MY LORD. A FEAST, OR A LIGHTER HAND, WOULD QUIET THEM.";
    case lord::Task::Raid: return "SOLDIERS CAMP OUTSIDE OUR FENCES. WE NEED YOUR SWORD WITH THE MILITIA.";
    case lord::Task::Camp: return "BANDITS FROM THE CAMP IN THE HILLS STEAL FROM OUR FIELDS. BREAK THEM AND WE'LL SLEEP AGAIN.";
    case lord::Task::Settlers: return "HALF THE HOUSES STAND EMPTY. REFUGEES ON THE ROADS WOULD FIND A HOME HERE, IF YOU SENT THEM.";
    default: return "ALL IS WELL, MY LORD. THE FIELDS ARE TENDED AND THE ROOFS ARE SOUND.";
  }
}

void mainPage(Game& g, lord::Holding& h, bool bye) {
  Dialogue& d = g.dlg;
  const lord::Lordship& L = g.lordship;
  d.opts.clear();
  d.opts.push_back({"THE HALL'S COFFERS", DLG_LORD + K_COFFERS, 0});
  d.opts.push_back({std::string("THE TAX (") + lord::taxWord(h.taxPct) + ")", DLG_LORD + K_TAX, 0});
  d.opts.push_back({h.building ? std::string("THE WORKS (") + (lord::workInfo(h.building) ? lord::workInfo(h.building)->name : "") + " GOING UP)" : std::string("RAISE WORKS"),
                    DLG_LORD + K_BUILD, 0});
  if (h.works & lord::LW_MILITIA) d.opts.push_back({"MUSTER THE MILITIA (" + num(h.militia) + ")", DLG_LORD + K_MUSTER, 0});
  if (h.loyalty < 50 || (h.flags & lord::HF_UNREST)) d.opts.push_back({"HOLD A FEAST (" + num(lord::FEAST_GOLD) + " GOLD)", DLG_LORD + K_FEAST, 0});
  d.opts.push_back({"THE VILLAGE'S NEEDS", DLG_LORD + K_NEEDS, 0});
  if (!L.realm && L.liege) {
    d.opts.push_back({"THE TRIBUTE", DLG_LORD + K_TRIBUTE, 0});
    d.opts.push_back({"RAISE MY OWN BANNER", DLG_LORD + K_FREE, 0});
  }
  if (L.realm) {
    d.opts.push_back({"MARCH ON A VILLAGE", DLG_LORD + K_MARCH, 0});
    const realm::KingdomState* P = g.realm.kingdom(L.realm);
    const ew::Gid old = P ? P->parent : 0;
    if (L.peaceOffer) d.opts.push_back({"ACCEPT " + realmName(g, old) + "'S PEACE", DLG_LORD + K_PEACE, 0});
    // another war of the player's realm, three weeks old and going our way (or the foe worn out): terms can be had
    for (const realm::War& w : g.realm.wars()) {
      if (w.endDay || (w.attacker != L.realm && w.defender != L.realm)) continue;
      const ew::Gid foe = w.attacker == L.realm ? w.defender : w.attacker;
      if (foe == old && L.peaceOffer) continue;
      const realm::KingdomState* F = g.realm.kingdom(foe);
      const float ours = w.attacker == L.realm ? w.score : -w.score;
      if (g.day - (int)w.startDay >= 20 && (ours >= 0.1f || (F && F->exhaustion >= 0.3f)))
        d.opts.push_back({"MAKE PEACE WITH " + realmName(g, foe), DLG_LORD + K_WARPEACE, (int)w.id});
    }
  }
  if (bye) d.opts.push_back({"FAREWELL", 0, 0});   // (the opening's FAREWELL is the game's own)
}
void backPage(Game& g) {
  g.dlg.opts.clear();
  g.dlg.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
  g.dlg.opts.push_back({"FAREWELL", 0, 0});
}
}  // namespace

void LordOps::stewardTalk(Game& g, Actor& a, lord::Holding& h) {
  (void)a;
  Dialogue& d = g.dlg;
  const std::string where = siteName(g, h.site);
  std::string line = "MY LORD. ";
  if (const realm::Siege* sg = g.realm.siegeAt(h.site)) line += realmName(g, sg->attacker) + "'S SOLDIERS CAMP OUTSIDE " + where + ". WE ARE READY.";
  else if (h.flags & lord::HF_UNREST) line += "THE PEOPLE GRUMBLE. THE TAX SITS HEAVY ON " + where + ".";
  else if (h.building && lord::workInfo(h.building)) line += std::string(lord::workInfo(h.building)->name) + " GOES UP: " + num(std::max(0, (int)h.buildDone - g.day)) + " DAYS MORE.";
  else if (h.loyalty >= 70) line += where + " LOVES ITS LORD. WHAT WOULD YOU HAVE OF US?";
  else line += "WHAT WOULD YOU HAVE OF US TODAY?";
  // the steward's pages replace the generic options (the hall is all business)
  const std::vector<DlgOpt> keep = d.opts;
  mainPage(g, h, false);
  for (const DlgOpt& o : keep) if (!(o.action >= DLG_LORD && o.action < DLG_LORD_END)) d.opts.push_back(o);
  speech::propose(d, speech::P_SERVICE + 5, speech::Topic::Service, line);
}

void LordOps::rulerTalk(Game& g, Actor& a, int si) {
  (void)a;
  Dialogue& d = g.dlg;
  const lord::Lordship& L = g.lordship;
  const ew::Gid k = g.realm.ownerOf(g.world.sites[(size_t)si].id, ownerOfSite(g, si));
  if (!k) return;
  // the old liege, at odds with the player's realm: its terms
  if (L.realm && L.peaceOffer) {
    const realm::KingdomState* P = g.realm.kingdom(L.realm);
    if (P && P->parent == k) { d.opts.push_back({"WE ACCEPT YOUR TERMS OF PEACE", DLG_LORD + K_TERMS, 0}); return; }
  }
  if (L.realm) return;
  if (L.liege && L.liege != k) return;   // one liege at a time
  const int cap = g.service.rank >= service::Rank::Marshal ? 2 : 1;   // a captain one village, a marshal two
  if (g.service.sworn == k && (int)g.service.rank >= lord::GRANT_RANK && (int)L.holdings.size() < cap)
    d.opts.push_back({"I ASK FOR A LORDSHIP", DLG_LORD + K_ASK, 0});
  else if (g.realm.renown().fame >= lord::CHARTER_FAME && g.realm.rep(k) >= 0)
    d.opts.push_back({"I WOULD BUY A VILLAGE'S CHARTER", DLG_LORD + K_CHARTER, 0});
}

void LordOps::refugeeTalk(Game& g, Actor& a) {
  Dialogue& d = g.dlg;
  const int home = refugeeHome(g, a);
  if (home >= 0 && !g.lordship.holding(g.world.sites[(size_t)home].id)) {
    const realm::SettlementState* st = g.realm.settlement(g.world.sites[(size_t)home].id);
    if (st && !(st->flags & realm::SS_BESIEGED) && g.realm.renown().fame >= 20) {
      d.opts.push_back({"LEAD US HOME", DLG_LORD + K_LEAD, home});
      speech::propose(d, speech::P_NEED + 1, speech::Topic::Need,
                      "THEY SAY YOU ARE SOMEONE THE ROADS KNOW. " + g.world.sites[(size_t)home].name + " COULD RISE AGAIN, IF SOMEONE STOOD FOR US. WILL YOU LEAD US HOME?");
    }
  }
  // a holding that wants settlers
  for (size_t i = 0; i < g.lordship.holdings.size(); i++) {
    const lord::Holding& h = g.lordship.holdings[i];
    bool want = false;
    for (const Quest& q : g.quests)
      want |= q.type == QType::Fief && q.stage == (int)h.task && q.state == QState::Active && lord::taskOf(q.flags) == lord::Task::Settlers;
    if (want) { d.opts.push_back({"COME AND SETTLE IN " + siteName(g, h.site), DLG_LORD + K_SETTLE, (int)i}); break; }
  }
}

bool LordOps::talkChoose(Game& g, const DlgOpt& o) {
  const int act = o.action - DLG_LORD;
  Dialogue& d = g.dlg;
  lord::Lordship& L = g.lordship;
  auto bye = [&] { d.opts.clear(); d.opts.push_back({"FAREWELL", 0, 0}); };
  lord::Holding* h = talkHolding(g);
  std::string why;
  switch (act) {
    case K_BACK:
      if (h) mainPage(g, *h, true); else bye();
      return true;
    case K_COFFERS: {
      if (!h) { bye(); return true; }
      const realm::SettlementState* st = g.realm.settlement(h->site);
      const int perDay = lord::taxPerDay(*h, st ? st->popPct : 100, st ? st->prosperity : 50);
      std::string t = "THE COFFERS HOLD " + num(L.treasury) + " GOLD. " + siteName(g, h->site) + " BRINGS ABOUT " + num(perDay * 7) +
                      " A WEEK AT A " + lord::taxWord(h->taxPct) + " TAX; THE PEOPLE'S LOVE STANDS AT " + num(h->loyalty) + ".";
      if (!L.realm && L.liege)
        t += " " + num(L.tributeDue) + " IS OWED TO " + realmName(g, L.liege) + " IN " + num(std::max(0, (int)L.tributeDay - g.day)) + " DAYS" +
             (L.withhold ? ", AND YOU BID ME WITHHOLD IT." : ".");
      else if (L.realm) t += " WE ANSWER TO NO CROWN BUT YOURS.";
      d.text = t;
      d.opts.clear();
      if (L.treasury > 0) d.opts.push_back({"TAKE WHAT THE COFFERS HOLD", DLG_LORD + K_TAKE, 0});
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      d.opts.push_back({"FAREWELL", 0, 0});
      return true;
    }
    case K_TAKE: {
      const int n = take(g, L.treasury);
      d.text = "HERE, MY LORD: " + num(n) + " GOLD.";
      g.sfx((int)Sfx::Coin, g.pl().p);
      backPage(g);
      return true;
    }
    case K_TAX: {
      if (!h) { bye(); return true; }
      d.text = "THE TAX STANDS " + std::string(lord::taxWord(h->taxPct)) + ". A LIGHT HAND WINS THEIR LOVE; A HEAVY ONE FILLS THE COFFERS AND EMPTIES THEIR HEARTS.";
      d.opts.clear();
      d.opts.push_back({"A LOW TAX (A TWENTIETH)", DLG_LORD + K_TAX_SET, lord::TAX_LOW});
      d.opts.push_back({"A FAIR TAX (A TENTH)", DLG_LORD + K_TAX_SET, lord::TAX_FAIR});
      d.opts.push_back({"A HEAVY TAX (A FIFTH)", DLG_LORD + K_TAX_SET, lord::TAX_HEAVY});
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      return true;
    }
    case K_TAX_SET:
      if (!h) { bye(); return true; }
      setTax(g, *h, (uint8_t)o.arg);
      d.text = o.arg <= lord::TAX_LOW ? "A LIGHT HAND. THEY'LL BLESS YOUR NAME, MY LORD." : o.arg >= lord::TAX_HEAVY ? "AS YOU COMMAND. THEY WON'T LIKE IT." : "A FAIR TENTH. NO ONE CAN SAY IT ISN'T JUST.";
      backPage(g);
      return true;
    case K_BUILD: {
      if (!h) { bye(); return true; }
      d.opts.clear();
      if (h->building) {
        const lord::WorkInfo* w = lord::workInfo(h->building);
        d.text = std::string(w ? w->name : "THE WORK") + " GOES UP: " + num(std::max(0, (int)h->buildDone - g.day)) + " DAYS MORE, MY LORD.";
      } else {
        d.text = "WHAT SHALL WE RAISE? THE COFFERS PAY FIRST, YOUR PURSE THE REST (" + num(L.treasury) + " IN THE COFFERS).";
        for (uint16_t bit : lord::BUILD_ORDER) {
          if (h->works & bit) continue;
          const uint16_t need = lord::workNeeds(bit);
          if ((h->works & need) != need) continue;
          const lord::WorkInfo* w = lord::workInfo(bit);
          if (w) d.opts.push_back({std::string("RAISE ") + w->name + " (" + num(w->gold) + " GOLD, " + num(w->days) + (w->days == 1 ? " DAY)" : " DAYS)"), DLG_LORD + K_BUILD_DO, (int)bit});
        }
        if (d.opts.empty()) d.text = "EVERY WORK WE KNOW STANDS IN " + siteName(g, h->site) + ", MY LORD.";
      }
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      return true;
    }
    case K_BUILD_DO:
      if (!h) { bye(); return true; }
      if (startWork(g, *h, (uint16_t)o.arg, why)) {
        const lord::WorkInfo* w = lord::workInfo((uint16_t)o.arg);
        d.text = "THE WORKERS START AT DAWN. " + std::string(w ? w->name : "IT") + " WILL STAND IN " + num(w ? w->days : 1) + (w && w->days == 1 ? " DAY." : " DAYS.");
      } else {
        d.text = why;
      }
      backPage(g);
      return true;
    case K_MUSTER:
      if (!h) { bye(); return true; }
      d.text = muster(g, *h, why) ? "TWO MORE TAKE UP SPEAR AND SHIELD. " + num(h->militia) + " STAND READY, MY LORD." : why;
      backPage(g);
      return true;
    case K_FEAST:
      if (!h) { bye(); return true; }
      d.text = feast(g, *h, why) ? "TABLES IN THE GREEN, A BULLOCK ON THE SPIT. THEY'LL DRINK YOUR HEALTH TONIGHT." : why;
      backPage(g);
      return true;
    case K_NEEDS: {
      if (!h) { bye(); return true; }
      const lord::Task t = needOf(g, *h);
      d.text = needLine(t);
      d.opts.clear();
      bool open = false;
      for (const Quest& q : g.quests) open |= q.type == QType::Fief && q.stage == (int)h->task && h->task && q.state == QState::Active;
      if (open) d.text += " AND YOU HAVE ALREADY GIVEN YOUR WORD ON ONE THING.";
      else if (t != lord::Task::None) d.opts.push_back({"I WILL SEE TO IT", DLG_LORD + K_NEEDS_DO, (int)t});
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      return true;
    }
    case K_NEEDS_DO:
      if (!h) { bye(); return true; }
      d.text = giveTask(g, *h, (lord::Task)o.arg) ? "THANK YOU, MY LORD. THE VILLAGE WILL KNOW OF IT." : "IT SEEMS THE MATTER HAS PASSED, MY LORD.";
      backPage(g);
      return true;
    case K_TRIBUTE:
      d.text = L.tributeDue > 0 ? num(L.tributeDue) + " GOLD IS OWED TO " + realmName(g, L.liege) + " IN " + num(std::max(0, (int)L.tributeDay - g.day)) + " DAYS." +
                                      (L.missed ? " WE HAVE MISSED " + num(L.missed) + " ALREADY." : "")
                                : "NOTHING IS OWED YET, MY LORD.";
      d.opts.clear();
      if (L.tributeDue > 0) d.opts.push_back({"PAY THE TRIBUTE NOW", DLG_LORD + K_PAY, 0});
      if (!L.withhold) d.opts.push_back({"WITHHOLD THE TRIBUTE", DLG_LORD + K_WITHHOLD, 0});
      else d.opts.push_back({"PAY THE TRIBUTE AGAIN WHEN IT FALLS DUE", DLG_LORD + K_RESUME, 0});
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      return true;
    case K_PAY:
      d.text = payTribute(g) ? "IT IS PAID. THE CROWN'S MEN WILL LEAVE US BE." : "THE COFFERS ARE TOO THIN, MY LORD.";
      if (L.withhold) withhold(g, false);
      backPage(g);
      return true;
    case K_WITHHOLD:
      withhold(g, true);
      d.text = "THEN NOT A COPPER GOES TO " + realmName(g, L.liege) + ". THEY WILL NOTICE, MY LORD.";
      backPage(g);
      return true;
    case K_RESUME:
      withhold(g, false);
      d.text = "AS YOU WISH. THE TRIBUTE GOES WHEN IT FALLS DUE.";
      backPage(g);
      return true;
    case K_FREE:
      d.text = "RAISE YOUR OWN BANNER? " + freeCost(g) + " ARE YOU SURE, MY LORD?";
      d.opts.clear();
      d.opts.push_back({"YES. RAISE IT.", DLG_LORD + K_FREE_YES, 0});
      d.opts.push_back({"NOT YET", DLG_LORD + K_BACK, 0});
      return true;
    case K_FREE_YES:
      d.text = independence(g, "", why) ? "THEN LET THEM COME. " + siteName(g, L.holdings.empty() ? 0 : L.holdings[0].site) + " STANDS WITH YOU." : why;
      bye();
      return true;
    case K_MARCH: {
      d.opts.clear();
      // the nearest settlements not under the player's banner the lattice knows (villages; towns once crowned), up to 4
      marchTargets().clear();
      if (g.world.src) {
        const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
        std::vector<std::pair<int64_t, size_t>> c;
        const std::vector<ew::SettlementNode> nodes = g.world.src->settlementsIn(px - 900, py - 900, px + 900, py + 900, true);
        for (size_t i = 0; i < nodes.size(); i++) {
          const ew::SettlementNode& n = nodes[i];
          if (!(n.type == SiteType::Village || (L.crowned && n.type == SiteType::Town)) || L.holding(n.id)) continue;
          const realm::SettlementState* st = g.realm.settlement(n.id);
          if (!st || st->owner == L.realm || (st->flags & (realm::SS_BESIEGED | realm::SS_RUINED | realm::SS_ABANDONED | realm::SS_BURNED))) continue;
          if (const realm::KingdomState* O = g.realm.kingdom(st->owner)) if (O->isolated) continue;
          c.push_back({(int64_t)(n.x - px) * (n.x - px) + (int64_t)(n.y - py) * (n.y - py), i});
        }
        std::sort(c.begin(), c.end());
        for (size_t i = 0; i < c.size() && i < 4; i++) {
          const ew::SettlementNode& n = nodes[c[i].second];
          marchTargets().push_back(n.id);
          d.opts.push_back({"MARCH ON " + n.name + " (" + realmName(g, g.realm.settlement(n.id)->owner) + ")", DLG_LORD + K_MARCH_DO, (int)marchTargets().size() - 1});
        }
      }
      d.text = L.march ? "OUR MEN ARE ALREADY IN THE FIELD, MY LORD." : d.opts.empty() ? "THERE IS NOTHING NEAR ENOUGH TO TAKE." : "WHERE SHALL THE BANNER GO?";
      if (L.march) d.opts.clear();
      d.opts.push_back({"BACK", DLG_LORD + K_BACK, 0});
      return true;
    }
    case K_MARCH_DO: {
      const ew::Gid t = o.arg >= 0 && o.arg < (int)marchTargets().size() ? marchTargets()[(size_t)o.arg] : 0;
      d.text = march(g, t, why) ? "THE MILITIA FORM UP. MEET THEM AT THE CAMP BEFORE ITS FENCES AND TAKE IT." : why;
      bye();
      return true;
    }
    case K_PEACE: case K_TERMS:
      d.text = acceptPeace(g) ? "THE TERMS ARE SEALED. LET THE FIELDS BE PLOUGHED AGAIN." : "THERE IS NO OFFER ON THE TABLE.";
      bye();
      return true;
    case K_WARPEACE: {
      ew::Gid foe = 0;
      for (const realm::War& w : g.realm.wars())
        if ((int)w.id == o.arg && !w.endDay) foe = w.attacker == L.realm ? w.defender : w.attacker;
      if (foe) {
        g.realm.forcePeace(foe, L.realm, g.day);
        d.text = "OUR ENVOY RIDES TO " + realmName(g, foe) + " WITH TERMS. THE WAR IS OVER, MY LORD.";
        rise(g, RiseEv::MissionDone, 1, "PEACE WITH " + realmName(g, foe) + ".", L.realm);
      } else {
        d.text = "THAT WAR IS ALREADY OVER.";
      }
      bye();
      return true;
    }
    case K_ASK: case K_CHARTER: case K_CHARTER_BUY: {
      const int ai = findActor(g, d.actor);
      const int si = ai >= 0 ? actorSite(g, g.actors[(size_t)ai]) : -1;
      const ew::Gid k = si >= 0 ? g.realm.ownerOf(g.world.sites[(size_t)si].id, ownerOfSite(g, si)) : 0;
      const int v = act == K_CHARTER_BUY ? o.arg : k ? grantable(g, k) : -1;
      why = "WE HAVE NO VILLAGE TO GIVE.";
      if (v >= 0 && v < (int)g.world.sites.size()) {
        const Site& vs = g.world.sites[(size_t)v];
        const realm::SettlementState* st = g.realm.settlement(vs.id);
        const int price = lord::charterPriceFor((uint8_t)vs.type, st ? st->prosperity : 50, st ? st->popPct : 100);
        if (act == K_CHARTER) {
          d.text = "THE CHARTER OF " + vs.name + " IS FOR SALE: " + num(price) + " GOLD, AND ITS TRIBUTE TO THE CROWN EVERY MONTH.";
          d.opts.clear();
          if (g.gold >= price) d.opts.push_back({"BUY THE CHARTER OF " + vs.name, DLG_LORD + K_CHARTER_BUY, v});
          else d.text += " COME BACK WHEN YOUR PURSE IS HEAVIER.";
          d.opts.push_back({"NOT TODAY", 0, 0});
          return true;
        }
        if (act == K_CHARTER_BUY) {
          if (g.gold < price) { d.text = "A CHARTER COSTS " + num(price) + " GOLD. COME BACK WHEN YOU HAVE IT."; bye(); return true; }
          if (grant(g, v, lord::How::Charter, k, why)) {
            g.gold -= price;
            g.sfx((int)Sfx::Buy, g.pl().p);
            d.text = "THE CHARTER OF " + vs.name + " IS YOURS. YOUR STEWARD WAITS ON THE GREEN. RULE IT WELL.";
            bye();
            return true;
          }
        } else if (grant(g, v, lord::How::Granted, k, why)) {
          d.text = "YOU HAVE SERVED US WELL, CAPTAIN. TAKE " + vs.name + ", AND KEEP IT FOR THE CROWN. YOUR STEWARD WAITS THERE.";
          bye();
          return true;
        }
      }
      d.text = why;
      bye();
      return true;
    }
    case K_LEAD: {
      if (resettle(g, o.arg, why)) {
        d.text = "THEN WE GO HOME! " + g.world.sites[(size_t)o.arg].name + " WILL RISE FROM THE ASHES, MY LORD. WE'LL NOT FORGET WHO STOOD FOR US.";
        g.realm.renown().fame = std::min(1000000, g.realm.renown().fame + 15);
      } else {
        d.text = why;
      }
      bye();
      return true;
    }
    case K_SETTLE: {
      if (o.arg < 0 || o.arg >= (int)L.holdings.size()) { bye(); return true; }
      lord::Holding& hh = L.holdings[(size_t)o.arg];
      const std::string where = siteName(g, hh.site);
      if (hh.task) finishTask(g, hh.task);
      d.text = "A ROOF AND A FIELD IN " + where + "? WE'LL GO AT FIRST LIGHT. THANK YOU, MY LORD.";
      bye();
      return true;
    }
    default: return false;
  }
}
