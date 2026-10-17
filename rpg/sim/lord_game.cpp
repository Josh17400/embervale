// M12 "Rise and Fall": the lordship in the game (rpg/sim/lord_ops.h). LORD lane (phase A by the lead; phase B by the
// lane): a village held under a liege (granted to a captain, a charter bought with fame and gold, a ruin resettled by
// its refugees, a conquest by the player's own banner), the day's taxes into the hall's coffers at the steward's rate
// (loyalty drifting with the tax, the works, raids and famine; unrest under 25), a quarter of them owed as tribute every
// 30 days (paid from the coffers, or withheld: the liege's anger, lord_war.cpp), works raised in the village
// (lord_works.cpp stamps them into the window), fief tasks (lord_tasks.cpp), the steward's talk (lord_talk.cpp),
// independence under the player's own heraldry, growth by conquest and resettling, and the crown at 3 settlements.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/audio.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/lord_ops.h"
#include "rpg/sim/service.h"
#include "rpg/sim/service_ops.h"
#include "rpg/sim/talk.h"
#include "rpg/world/source.h"

namespace {
uint16_t dayU16(int day) { return (uint16_t)std::clamp(day, 0, 65535); }

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
// the steward's key and name: a new face of the village's culture (a census resident walks its own life)
uint64_t stewardKey(ew::Gid site, int day) { return ew::mix64(site ^ 0x5737A2D0ull ^ ((uint64_t)(uint32_t)day << 32)) | 1ull; }
}  // namespace

void LordOps::rise(Game& g, RiseEv k, float f, const std::string& s, ew::Gid kingdom) {
  g.events.push_back({Ev::Rise, g.pl().p, (int)k, f, s});
  g.events.back().id = (uint32_t)kingdom;
}

void LordOps::sync(Game& g) {
  realm::Renown& R = g.realm.renown();
  R.lordOf = g.lordship.holdings.empty() ? 0 : g.lordship.holdings[0].site;
  R.realmOf = g.lordship.realm;
  R.liege = g.lordship.realm ? 0 : g.lordship.liege;
}

void LordOps::reset(Game& g) { g.lordship = lord::Lordship(); forgetStamps(g); }
void LordOps::loaded(Game& g) { sync(g); forgetStamps(g); }   // the loaded window is the generator's: stamped afresh

int LordOps::heldSite(const Game& g) {
  const int si = g.curSite;
  if (si < 0 || si >= (int)g.world.sites.size()) return -1;
  return g.lordship.holding(g.world.sites[(size_t)si].id) ? si : -1;
}

int LordOps::nearestHolding(const Game& g) {
  int best = -1;
  float bd = 1e30f;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (size_t i = 0; i < g.lordship.holdings.size(); i++) {
    const int si = g.world.siteHandle(g.lordship.holdings[i].site);
    if (si < 0) continue;
    const Site& s = g.world.sites[(size_t)si];
    const float d = (s.ex - px) * (s.ex - px) + (s.ey - py) * (s.ey - py);
    if (d < bd) { bd = d; best = (int)i; }
  }
  return best;
}

bool LordOps::grant(Game& g, int site, lord::How how, ew::Gid liege, std::string& why) {
  if (site < 0 || site >= (int)g.world.sites.size() || !g.world.sites[(size_t)site].settlement()) { why = "THAT IS NO SETTLEMENT."; return false; }
  const Site& s = g.world.sites[(size_t)site];
  lord::Lordship& L = g.lordship;
  // villages until the crown; a king may hold a town (a city stays a realm's own seat)
  if (s.type != SiteType::Village && !(L.crowned && s.type == SiteType::Town)) { why = "ONLY A VILLAGE CAN BE HELD BY A LORD WITHOUT A CROWN."; return false; }
  if (L.holding(s.id)) { why = "IT IS ALREADY YOURS."; return false; }
  g.realmNoteSites();
  if (L.realm) liege = L.realm;
  else if (!liege) liege = g.realm.ownerOf(s.id, ownerOfSite(g, site));
  // a sworn lord's new land answers to the same liege (a wildlands village joins the liege's realm with its lord)
  if (!L.realm && L.liege && !liege) liege = L.liege;
  // one liege at a time: a lord sworn to one crown takes no land from another
  if (!L.realm && L.liege && liege && liege != L.liege) { why = "YOU ARE SWORN TO " + realmName(g, L.liege) + ". ONE LIEGE AT A TIME."; return false; }
  if (!g.realm.grantFief(s.id, L.realm ? 0 : liege, (uint8_t)how, g.day)) { why = "THE REALM WILL NOT HAVE IT."; return false; }
  lord::Holding h;
  h.site = s.id; h.how = how; h.sinceDay = dayU16(g.day); h.lastTaxDay = g.day;
  h.steward = stewardKey(s.id, g.day);
  h.taskDay = dayU16(g.day);
  // a ruin led home starts poor and grateful; a conquest sullen
  if (how == lord::How::Resettled) h.loyalty = 75;
  else if (how == lord::How::Conquered) h.loyalty = 35;
  L.holdings.push_back(h);
  if (!L.realm) {
    L.liege = liege;
    if (!L.tributeDay) L.tributeDay = dayU16(g.day + lord::TRIBUTE_DAYS);
  }
  if (!L.weekDay) L.weekDay = dayU16(g.day + 7);
  sync(g);
  g.realmSync();
  if (L.realm) g.realm.playerTakes(s.id, g.day);   // the crown's title follows the land
  static const char* how3[] = {"", "BY THE CROWN'S GRANT", "BY CHARTER", "AT ITS PEOPLE'S ASKING", "BY CONQUEST", "BY FOUNDING"};
  const std::string line = "YOU ARE LORD OF " + s.name + " " + how3[(int)how] +
                           (L.realm ? std::string(".") : liege ? ", SWORN TO " + realmName(g, liege) + "." : ", ANSWERING TO NO CROWN.");
  g.say(line);
  rise(g, RiseEv::FiefGranted, (float)how, line, L.realm ? L.realm : liege);
  g.sfx((int)Sfx::LevelUp, g.pl().p);
  crownCheck(g);
  restamp(g);
  return true;
}

bool LordOps::resettle(Game& g, int site, std::string& why) {
  if (site < 0 || site >= (int)g.world.sites.size()) { why = "WE DON'T KNOW THE WAY."; return false; }
  const Site& s = g.world.sites[(size_t)site];
  g.realmNoteSites();
  const realm::SettlementState* st = g.realm.settlement(s.id);
  if (!st || !(st->flags & (realm::SS_BURNED | realm::SS_ABANDONED | realm::SS_RUINED))) { why = "OUR HOME STILL STANDS. WE'LL GO BACK WHEN IT IS SAFE."; return false; }
  if (st->flags & realm::SS_BESIEGED) { why = "SOLDIERS STILL CAMP ROUND IT."; return false; }
  lord::Lordship& L = g.lordship;
  if (L.holding(s.id)) { why = "IT IS ALREADY YOURS."; return false; }
  // (M12 fixer) the holding rules first (grant's own): a burned town is no lord's to lead home without a crown, and the
  // place is not resettled for anyone when it cannot be held
  if (s.type != SiteType::Village && !(L.crowned && s.type == SiteType::Town)) { why = "A TOWN NEEDS A CROWN'S MEN TO REBUILD IT. WE WILL WAIT FOR ITS OWN LORDS."; return false; }
  // who the rebuilt village answers to: the player's own realm, the liege the player is sworn to, or whoever held it
  ew::Gid by = L.realm ? L.realm : L.liege ? L.liege : st->owner;
  g.realm.forceResettle(s.id, by, g.day);
  if (!grant(g, site, lord::How::Resettled, by, why)) return false;
  return true;
}

bool LordOps::independence(Game& g, const std::string& name, std::string& why) {
  lord::Lordship& L = g.lordship;
  if (L.holdings.empty()) { why = "YOU HOLD NO LAND TO RULE FROM."; return false; }
  if (L.realm) { why = "YOUR BANNER ALREADY FLIES FREE."; return false; }
  const ew::Gid liege = L.liege;
  // sworn to the liege's army: the oath ends first (released with honour, or a deserter), with a clear line
  releaseService(g);
  const Appearance& A = g.app;
  // the realm takes the seat's name unless the player names it (not the old liege's word: realm_player.cpp)
  const std::string rname = !name.empty() ? name : siteName(g, L.holdings[0].site);
  const ew::Gid rid = g.realm.declareIndependence(rname, A.name.empty() ? std::string("THE LORD") : A.name, A.female, A.heraldry.field,
                                                  A.heraldry.charge, A.heraldry.emblem, g.day);
  if (!rid) { why = "THE REALM WOULD NOT LET IT BE."; return false; }
  for (size_t i = 1; i < L.holdings.size(); i++) g.realm.playerTakes(L.holdings[i].site, g.day);
  g.realm.playerTakes(L.holdings[0].site, g.day);   // the ruler's title (LORD / LADY, the crown at 3)
  L.realm = rid;
  L.realmName = rname;
  L.independenceDay = dayU16(g.day);
  L.tributeDue = 0;
  L.taxSinceTribute = 0;
  L.withhold = 0;
  L.missed = 0;
  L.liege = 0;
  // the old liege's anger takes its first step in a fortnight or so (lord_war.cpp)
  L.wrath = 1;
  L.wrathDay = dayU16(g.day + 12 + (int)(ew::mix64(rid ^ (uint64_t)g.day) % 8));
  sync(g);
  g.realmSync();
  const std::string line = "YOUR BANNER FLIES FREE: " + L.realmName + " DEFIES " + realmName(g, liege) + ".";
  g.say(line);
  rise(g, RiseEv::Independence, 1, line, rid);
  crownCheck(g);
  restamp(g);
  return true;
}

int LordOps::collect(Game& g) {
  int total = 0;
  lord::Lordship& L = g.lordship;
  for (lord::Holding& h : L.holdings) {
    if (h.lastTaxDay >= g.day) continue;
    const int days = std::min(30, g.day - std::max(0, h.lastTaxDay));
    h.lastTaxDay = g.day;
    const realm::SettlementState* st = g.realm.settlement(h.site);
    const int pop = st ? st->popPct : 100, pros = st ? st->prosperity : 50;
    const bool famine = st && (st->flags & realm::SS_FAMINE);
    const bool raided = st && (st->flags & (realm::SS_BESIEGED | realm::SS_BURNED));
    if (famine) h.flags |= lord::HF_FAMINE; else h.flags &= (uint8_t)~lord::HF_FAMINE;
    // a raid, a siege or a burning: the people's faith in their lord's protection is shaken once (the militia and the
    // palisade soften it)
    if (raided && (h.raidDay == 0 || g.day - (int)h.raidDay > 20)) {
      const int hit = 15 - ((h.works & lord::LW_MILITIA) ? 5 : 0) - ((h.works & (lord::LW_PALISADE | lord::LW_WALLS)) ? 4 : 0);
      h.loyalty = (uint8_t)std::max(0, (int)h.loyalty - hit);
      h.raidDay = dayU16(g.day);
    }
    for (int d = 0; d < days; d++) {
      total += lord::taxPerDay(h, pop, pros);
      const int want = lord::loyaltyTarget(h);
      int l = h.loyalty;
      l += l < want ? 1 : l > want ? -1 : 0;
      if (famine && !(h.works & lord::LW_GRANARY)) l -= 1;   // a granary carries them through
      h.loyalty = (uint8_t)std::clamp(l, 0, 100);
      if (h.loyalty < 25) h.flags |= lord::HF_UNREST;
      else if (h.loyalty >= 35) h.flags &= (uint8_t)~lord::HF_UNREST;
    }
  }
  if (total > 0) {
    L.treasury += total;
    L.weekTax += total;
    if (!L.realm && L.liege) {
      L.taxSinceTribute += total;
      L.tributeDue = L.taxSinceTribute / 4;
    }
  }
  return total;
}

bool LordOps::payTribute(Game& g) {
  lord::Lordship& L = g.lordship;
  if (L.realm || !L.liege) return false;
  const int due = L.tributeDue;
  if (due <= 0) return true;
  if (L.treasury < due) {
    L.missed++;
    g.realm.refuseTribute(0, L.liege, g.day, false);
    rise(g, RiseEv::TributeRefused, (float)due, "THE COFFERS COULD NOT MEET THE TRIBUTE TO " + realmName(g, L.liege) + ".", L.liege);
    refused(g);
    return false;
  }
  L.treasury -= due;
  L.tributeDue = 0;
  L.taxSinceTribute = 0;
  L.missed = 0;
  if (L.wrath == 1 && !L.realm) { L.wrath = 0; L.wrathDay = 0; }   // the ultimatum met
  g.realm.payTribute(0, L.liege, due / 100.0f, g.day);
  rise(g, RiseEv::Tribute, (float)due, "TRIBUTE PAID: " + std::to_string(due) + " GOLD TO " + realmName(g, L.liege) + ".", L.liege);
  return true;
}

void LordOps::withhold(Game& g, bool on) { g.lordship.withhold = on ? 1 : 0; }

int LordOps::take(Game& g, int gold) {
  const int n = std::clamp(gold, 0, g.lordship.treasury);
  g.lordship.treasury -= n;
  if (n > 0) g.giveGold(n);
  return n;
}

// ---------------------------------------------------------------- the hall's actions
bool LordOps::startWork(Game& g, lord::Holding& h, uint16_t bit, std::string& why) {
  const lord::WorkInfo* w = lord::workInfo(bit);
  if (!w) { why = "WE DON'T KNOW HOW TO BUILD THAT."; return false; }
  if (h.works & bit) { why = "IT IS ALREADY RAISED, MY LORD."; return false; }
  if (h.building) { why = "THE WORKERS ARE STILL ON " + std::string(lord::workInfo(h.building) ? lord::workInfo(h.building)->name : "THE LAST WORK") + "."; return false; }
  const uint16_t need = lord::workNeeds(bit);
  if ((h.works & need) != need) { why = "FIRST THE PALISADE AND THE WATCHTOWER, MY LORD. STONE GOES WHERE TIMBER STOOD."; return false; }
  lord::Lordship& L = g.lordship;
  if (L.treasury + g.gold < w->gold) { why = "IT WILL COST " + std::to_string(w->gold) + " GOLD. THE COFFERS AND YOUR PURSE HOLD " + std::to_string(L.treasury + g.gold) + "."; return false; }
  const int fromCoffers = std::min(L.treasury, w->gold);
  L.treasury -= fromCoffers;
  g.gold -= w->gold - fromCoffers;
  h.building = bit;
  h.buildDone = dayU16(g.day + w->days);
  g.sfx((int)Sfx::Coin, g.pl().p);
  restamp(g);   // the scaffold goes up
  return true;
}

void LordOps::setTax(Game& g, lord::Holding& h, uint8_t pct) {
  (void)g;
  h.taxPct = (uint8_t)std::clamp<int>(pct, 0, 30);
}

bool LordOps::muster(Game& g, lord::Holding& h, std::string& why) {
  if (!(h.works & lord::LW_MILITIA)) { why = "WE HAVE NO MILITIA TO MUSTER. RAISE ONE FIRST."; return false; }
  if (h.militia >= lord::MILITIA_MAX) { why = "EVERY ABLE HAND IS ALREADY UNDER ARMS."; return false; }
  if (g.lordship.treasury < lord::MUSTER_GOLD) { why = "ARMS AND PAY FOR TWO MORE COST " + std::to_string(lord::MUSTER_GOLD) + " GOLD FROM THE COFFERS."; return false; }
  g.lordship.treasury -= lord::MUSTER_GOLD;
  h.militia = (uint8_t)std::min(lord::MILITIA_MAX, h.militia + 2);
  // a siege of this holding being fought: they take the walls at once
  if (const realm::Siege* sg = g.realm.siegeAt(h.site)) g.realm.contribute(sg->id, sg->attacker == g.lordship.realm ? 1 : 2, 1.0f);
  restamp(g);
  return true;
}

bool LordOps::feast(Game& g, lord::Holding& h, std::string& why) {
  if (h.flags & lord::HF_FEAST) { why = "THEY STILL TALK OF THE LAST FEAST, MY LORD."; return false; }
  if (g.lordship.treasury < lord::FEAST_GOLD) { why = "A FEAST FOR THE VILLAGE COSTS " + std::to_string(lord::FEAST_GOLD) + " GOLD FROM THE COFFERS."; return false; }
  g.lordship.treasury -= lord::FEAST_GOLD;
  h.loyalty = (uint8_t)std::min(100, h.loyalty + 15);
  if (h.loyalty >= 25) h.flags &= (uint8_t)~lord::HF_UNREST;
  h.flags |= lord::HF_FEAST;
  return true;
}

bool LordOps::march(Game& g, ew::Gid site, std::string& why) {
  lord::Lordship& L = g.lordship;
  if (!L.realm) { why = "A SWORN LORD MAKES NO WARS OF HIS OWN."; return false; }
  if (L.march) { why = "OUR MEN ARE ALREADY IN THE FIELD."; return false; }
  g.realmNoteSites();
  const realm::SettlementState* st = g.realm.settlement(site);
  if (!st) { why = "WE KNOW NOTHING OF THAT PLACE."; return false; }
  if (st->type == (uint8_t)SiteType::City) { why = "A CITY'S WALLS ARE BEYOND US."; return false; }
  if (st->type == (uint8_t)SiteType::Town && !L.crowned) { why = "A TOWN NEEDS A KING'S ARMY, MY LORD."; return false; }
  if (st->owner == L.realm) { why = "IT FLIES OUR BANNER ALREADY."; return false; }
  if (st->flags & realm::SS_BESIEGED) { why = "ANOTHER ARMY CAMPS ROUND IT."; return false; }
  const realm::KingdomState* P = g.realm.kingdom(L.realm);
  if (L.peaceDay && P && st->owner == P->parent && g.day - (int)L.peaceDay < 60) { why = "WE SWORE PEACE WITH THEM, MY LORD. GIVE IT A SEASON."; return false; }
  if (const realm::KingdomState* O = g.realm.kingdom(st->owner)) if (O->isolated) { why = "NO ROAD WE KNOW LEADS THERE."; return false; }
  const uint32_t id = g.realm.forceSiege(site, L.realm, g.day);
  if (!id) { why = "THE MARCH CAME TO NOTHING."; return false; }
  L.march = id;
  // the levies march with the banner
  int levy = 0;
  for (const lord::Holding& h : L.holdings) levy += h.militia;
  if (levy) g.realm.contribute(id, 1, 0.5f * levy);
  g.say("YOUR BANNER MARCHES. JOIN THE ASSAULT AT ITS CAMP.");
  g.realmSync();
  return true;
}

bool LordOps::acceptPeace(Game& g) {
  lord::Lordship& L = g.lordship;
  const realm::KingdomState* P = g.realm.kingdom(L.realm);
  if (!L.realm || !P || !L.peaceOffer) return false;
  const ew::Gid old = P->parent;
  // the ladder down first, then the treaty (Realm::endWar sets a year's truce; forceTension would clear it)
  const bool war = g.realm.atWar(old, L.realm);
  if (!war) g.realm.forceTension(old, L.realm, realm::Tension::Calm, realm::WarCause::Succession, g.day);
  g.realm.forcePeace(old, L.realm, g.day);
  L.peaceOffer = 0;
  L.peaceDay = dayU16(g.day);
  L.wrath = 0;
  L.wrathDay = 0;
  const std::string line = "PEACE WITH " + realmName(g, old) + ": THEY OWN " + L.realmName + " FREE.";
  g.say(line);
  rise(g, RiseEv::MissionDone, 1, line, L.realm);
  return true;
}

// ---------------------------------------------------------------- the day
void LordOps::adopt(Game& g) {
  lord::Lordship& L = g.lordship;
  if (!L.realm) return;
  const realm::KingdomState* K = g.realm.kingdom(L.realm);
  if (!K) return;
  std::vector<ew::Gid> fresh;
  for (ew::Gid s : K->settlements) if (!L.holding(s)) fresh.push_back(s);
  for (ew::Gid s : fresh) {
    lord::Holding h;
    h.site = s; h.how = lord::How::Conquered; h.sinceDay = dayU16(g.day); h.lastTaxDay = g.day;
    h.loyalty = 35; h.steward = stewardKey(s, g.day); h.taskDay = dayU16(g.day);
    L.holdings.push_back(h);
    const std::string line = siteName(g, s) + " IS YOURS BY CONQUEST.";
    g.say(line);
    rise(g, RiseEv::FiefGranted, (float)lord::How::Conquered, line, L.realm);
    g.realm.playerTakes(s, g.day);
    restamp(g);
  }
  if (!fresh.empty()) { sync(g); crownCheck(g); }
}

void LordOps::crownCheck(Game& g) {
  lord::Lordship& L = g.lordship;
  if (!L.realm || L.holdings.empty()) return;
  g.realm.playerTakes(L.holdings[0].site, g.day);   // the title follows the land
  const bool king = (int)L.holdings.size() >= lord::CROWN_HOLDINGS;
  if (king && !L.crowned) {
    L.crowned = 1;
    const realm::KingdomState* K = g.realm.kingdom(L.realm);
    const std::string title = K ? K->ruler.title : std::string(g.app.female ? "QUEEN" : "KING");
    const std::string line = "THE CROWN IS YOURS: " + title + " OF " + L.realmName + ".";
    g.say(line);
    rise(g, RiseEv::Crowned, (float)L.holdings.size(), line, L.realm);
    if (const realm::SettlementState* st = g.realm.settlement(L.holdings[0].site))
      g.realm.forceEvent(realm::EvType::KingdomRose, L.realm, 0, L.holdings[0].site, st->gx, st->gy, g.day);   // the news
    g.sfx((int)Sfx::LevelUp, g.pl().p);
  } else if (!king && L.crowned) {
    L.crowned = 0;   // the crown follows the land (the title went back to LORD / LADY)
  }
}

void LordOps::daily(Game& g) {
  lord::Lordship& L = g.lordship;
  // works finished
  for (lord::Holding& h : L.holdings) {
    if (!h.building || g.day < (int)h.buildDone) continue;
    const lord::WorkInfo* w = lord::workInfo(h.building);
    h.works |= h.building;
    if (h.building == lord::LW_MILITIA) h.militia = (uint8_t)std::max<int>(h.militia, lord::MILITIA_WORK);
    h.building = 0;
    const std::string nm = w ? w->name : "THE WORK";
    const bool one = nm.rfind("A ", 0) == 0 || nm.rfind("THE ", 0) == 0;
    g.say("THE BUILDERS ARE DONE: " + nm + (one ? " NOW STANDS IN " : " NOW STAND IN ") + siteName(g, h.site) + ".");
    restamp(g);
  }
  // the weekly summary of the taxes (one line a week, never a toast a day)
  if (g.day >= (int)L.weekDay) {
    if (L.weekTax > 0) {
      const std::string line = "THE WEEK'S TAXES: " + std::to_string(L.weekTax) + " GOLD INTO THE HALL'S COFFERS (" + std::to_string(L.treasury) + ").";
      rise(g, RiseEv::Tax, (float)L.weekTax, line, L.realm ? L.realm : L.liege);
    }
    L.weekTax = 0;
    L.weekDay = dayU16(g.day + 7);
    if ((g.day / 7) % 2 == 0) for (lord::Holding& h : L.holdings) h.flags &= (uint8_t)~lord::HF_FEAST;   // a feast a fortnight
  }
}

// ---------------------------------------------------------------- hooks
void LordOps::step(Game& g, float dt) {
  (void)dt;
  lord::Lordship& L = g.lordship;
  if (L.holdings.empty()) { stampStep(g); return; }
  // a holding the realm gave to someone else (a siege, a liege's anger) is lost
  bool lostAny = false;
  for (size_t i = 0; i < L.holdings.size();) {
    const realm::SettlementState* st = g.realm.settlement(L.holdings[i].site);
    // (a liege-less holding, a wildlands village resettled, is held while no realm owns it: a warlord's banner over it
    // is a loss like any other)
    const bool lost = st && ((L.realm && st->owner != L.realm) || (!L.realm && st->owner != L.liege));
    if (lost) {
      const ew::Gid site = L.holdings[i].site;
      for (Quest& q : g.quests)
        if (q.type == QType::Fief && q.targetId == site && q.state != QState::Done) { q.state = QState::Done; q.flags |= QF_FAILED; }
      g.realm.revokeFief(site, g.day);
      L.holdings.erase(L.holdings.begin() + (std::ptrdiff_t)i);
      g.say(siteName(g, site) + " IS LOST TO YOU.");
      restamp(g);
      lostAny = true;
    } else {
      i++;
    }
  }
  if (L.holdings.empty()) {
    // the lordship ends gracefully: the hall's coffers go with the seat, the realm's name lives in the chronicle
    g.say(L.realm ? "YOUR LAST HOLDING HAS FALLEN. THE BANNER OF " + L.realmName + " IS FURLED." : "YOUR LORDSHIP IS ENDED.");
    if (L.realm) g.realm.playerRealmEnds(g.day);   // the realm falls with its last land: its wars end (no zombie realm)
    const uint32_t next = L.nextTask;
    L = lord::Lordship();
    L.nextTask = next;
    sync(g);
    g.realmSync();
    stampStep(g);
    return;
  }
  if (lostAny) { sync(g); crownCheck(g); }   // the seat may have moved; the crown follows the land
  adopt(g);
  collect(g);
  daily(g);
  if (!L.realm && L.liege && g.day >= (int)L.tributeDay) {
    if (L.withhold && L.tributeDue > 0) {
      L.missed++;
      g.realm.refuseTribute(0, L.liege, g.day);
      rise(g, RiseEv::TributeRefused, (float)L.tributeDue, "YOU WITHHOLD THE TRIBUTE FROM " + realmName(g, L.liege) + ".", L.liege);
      refused(g);
    } else {
      payTribute(g);
    }
    L.tributeDay = dayU16(g.day + lord::TRIBUTE_DAYS);
  }
  wrathStep(g);
  tasksStep(g);
  stampStep(g);
}

void LordOps::talk(Game& g, Actor& a) {
  lord::Lordship& L = g.lordship;
  // the steward of a holding (or, with none named, the elder / innkeeper)
  for (lord::Holding& h : L.holdings) {
    const int sa = stewardActor(g, h);
    if (sa >= 0 && g.actors[(size_t)sa].id == a.id) { stewardTalk(g, a, h); return; }
  }
  int si = a.site;
  if (si < 0 && a.bldg >= 0 && a.bldg < (int)g.world.over.bldgs.size()) si = g.world.over.bldgs[(size_t)a.bldg].site;
  if (si >= (int)g.world.sites.size()) si = -1;
  if (si >= 0 && (a.role == Role::Jarl || a.role == Role::Innkeeper)) {
    if (lord::Holding* h = L.holding(g.world.sites[(size_t)si].id)) { if (!h->steward) { stewardTalk(g, a, *h); return; } }
  }
  if (a.role == Role::Refugee) { refugeeTalk(g, a); return; }
  if ((a.role == Role::King || a.role == Role::Jarl) && si >= 0) rulerTalk(g, a, si);
}

bool LordOps::choose(Game& g, const DlgOpt& o) { return talkChoose(g, o); }

void LordOps::questDone(Game& g, const Quest& q) {
  if (q.type != QType::Fief) return;
  lord::Holding* h = g.lordship.holding(q.targetId);
  if (!h) return;
  h->loyalty = (uint8_t)std::min(100, h->loyalty + 10);
  if (h->loyalty >= 25) h->flags &= (uint8_t)~lord::HF_UNREST;
  if (h->task == (uint32_t)q.stage) h->task = 0;
  g.lordship.treasury = std::max(0, g.lordship.treasury - q.gold);   // the reward came from the coffers
}
