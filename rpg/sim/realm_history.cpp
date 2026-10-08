// M4 "Banners": the history pre-roll and the ruin records (VISION_PLAN 4.2, owner 15.3; rpg/sim/realm.h). REALM lane.
// Text only, zero simulation: 40 to 200 years of backstory per kingdom from hashed templates over its neighbours'
// genesis names (wars with names, dynasties, plagues, floods, hungry years, extinct realms it rose from), and the true
// record behind every ruin: its old name in the culture's phonology, the realm that built it (extinct or living), its
// last lord, when it was founded and how and when it fell, who destroyed it, and 4-8 clue lines for the STORY lane to
// place (inscriptions, journals, graves, murals, a toppled statue, a letter). Pure functions of the seed and the
// generator's plans, memoised, never saved. Settlements the live sim ruins get records from Realm::simRuins.
#include <algorithm>
#include <cstdio>
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/realm.h"
#include "rpg/world/source.h"

namespace realm {
namespace {

uint64_t H(uint64_t a, uint64_t b) { return ew::mix64(a ^ ew::mix64(b + 0x9E3779B97F4A7C15ull)); }
template <size_t N> const char* pick(const char* const (&a)[N], uint64_t h) { return a[h % N]; }
std::string num(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return b; }

const char* const ADJ[] = {"SALT", "IRON", "BROKEN", "SILVER", "RED", "BITTER", "LONG", "GREY", "BURNING", "HOLLOW",
                           "WINTER", "GOLDEN", "BLACK", "WEEPING", "THREE", "WHITE"};
const char* const NOUN[] = {"CROWN", "FORD", "HARVEST", "BRIDGE", "BANNER", "OATH", "FIELDS", "SPEAR", "GRANARY", "RIVER",
                            "STAG", "TOWER", "WELL", "ROAD", "HILLS", "MILL"};
const char* const PLAGUE[] = {"GREY", "WEEPING", "RED", "SWEATING", "SPOTTED", "BLACK"};
const char* const EPITHET[] = {"THE PALE", "THE BOLD", "THE OLD", "THE CRUEL", "THE PIOUS", "STONEHAND", "THE LAME",
                               "THE YOUNG", "THE WISE", "IRONSIDE", "THE GENTLE", "THE BLIND"};

std::string fallPhrase(FallCause c, uint64_t h) {
  switch (c) {
    case FallCause::War: { const char* const p[] = {"WAR", "LONG WAR", "SACK", "BURNING"}; return std::string("THE ") + pick(p, h); }
    case FallCause::Plague: return std::string("THE ") + pick(PLAGUE, h) + " PLAGUE";
    case FallCause::Flood: return "THE GREAT FLOOD";
    case FallCause::Famine: return "THE HUNGRY YEARS";
    case FallCause::Collapse: { const char* const p[] = {"THE COLLAPSE", "THE YEARS OF NO KING", "THE TAX REVOLTS"}; return pick(p, h); }
    case FallCause::Dragon: return "THE DRAGONFIRE";
    case FallCause::Curse: return "THE CURSE";
    default: return "THE DARK YEARS";
  }
}
FallCause causeFor(uint64_t h) {
  const int r = (int)(h % 100);
  if (r < 34) return FallCause::War;
  if (r < 49) return FallCause::Plague;
  if (r < 59) return FallCause::Flood;
  if (r < 73) return FallCause::Famine;
  if (r < 85) return FallCause::Collapse;
  if (r < 93) return FallCause::Dragon;
  return FallCause::Curse;
}

// an extinct realm of a kingdom's land (the history pre-roll's "ashes it rose from")
struct Past {
  std::string name;            // "OSKVAR"
  int foundedAgo = 0, fellAgo = 0;
  FallCause cause = FallCause::War;
  std::string lastRuler;       // "QUEEN ASGERD THE PALE"
};
std::vector<Past> pastRealms(ew::EndlessSource& src, uint64_t seed, Gid kid, uint64_t culture, int age) {
  const cult::Culture& C = src.culture(culture);
  const cult::Society S = cult::societyOf(C);
  std::vector<Past> out;
  const uint64_t h = H(seed ^ 0x50415354ull, kid);
  const int n = 1 + (int)(h % 2);
  int ago = age;
  for (int i = 0; i < n; i++) {
    const uint64_t hi = H(h, (uint64_t)i + 1);
    Past p;
    p.name = cult::kingdomName(C, (uint32_t)(hi >> 7));
    p.fellAgo = ago + (int)(hi % 25);
    p.foundedAgo = p.fellAgo + 60 + (int)((hi >> 12) % 240);
    p.cause = causeFor(hi >> 20);
    const bool f = ((hi >> 30) & 3) == 0;
    p.lastRuler = std::string(f ? "QUEEN" : (S.rulerTitle ? S.rulerTitle : "KING")) + " " + cult::personName(C, (uint32_t)(hi >> 33), f) +
                  " " + pick(EPITHET, hi >> 40);
    out.push_back(p);
    ago = p.foundedAgo;
  }
  return out;
}

}  // namespace

void Realm::warmHistory(ew::EndlessSource& src, Gid id) const {
  if (hist_.count(id)) return;
  const KingdomState* K = kingdom(id);
  if (!K || K->rebel) return;
  const cult::Culture& C = src.culture(K->culture);
  const cult::Society S = cult::societyOf(C);
  const int age = std::max(40, -K->foundedDay / YEAR);
  const uint64_t h = H(seed_ ^ 0x48495354ull, id);
  std::vector<HistoryEntry> L;
  // its neighbours by their genesis names
  std::vector<std::string> nb;
  const int32_t kx = ew::idRx(id), ky = ew::idRy(id);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (!dx && !dy) continue;
      const Gid o = src.kingdomOfCell(kx + dx, ky + dy);
      if (const ew::KingdomPlan* kp = o ? src.kingdom(o) : nullptr) nb.push_back(kp->name);
    }
  const std::vector<Past> past = pastRealms(src, seed_, id, K->culture, age);
  for (size_t i = past.size(); i-- > 0;) {
    const Past& p = past[i];
    L.push_back({p.foundedAgo, "THE KINGDOM OF " + p.name + " WAS FOUNDED " + num(p.foundedAgo) + " WINTERS AGO."});
    L.push_back({p.fellAgo, "THE KINGDOM OF " + p.name + " ENDED IN " + fallPhrase(p.cause, h >> 3) + ", " + num(p.fellAgo) +
                                " WINTERS AGO. ITS LAST RULER WAS " + p.lastRuler + "."});
  }
  const std::string founder = cult::personName(C, (uint32_t)(h >> 9), false);
  const std::string house0 = "HOUSE " + cult::personName(C, (uint32_t)(h >> 21) ^ 0x484F5553u, false);
  L.push_back({age, founder + " OF " + house0 + " FOUNDED " + K->name + " " + num(age) + " WINTERS AGO" +
                        (past.empty() ? std::string(".") : " ON THE ASHES OF " + past[0].name + ".")});
  // the middle years
  const int n = 2 + (int)(h % 4);
  bool dynasty = false;
  for (int i = 0; i < n; i++) {
    const uint64_t e = H(h, 100 + (uint64_t)i);
    const int y = std::max(2, (int)(age - 5 - (int)((e >> 8) % (uint64_t)std::max(1, age - 8))));
    const int kind = (int)(e % 8);
    std::string t;
    const std::string nbName = nb.empty() ? std::string("THE HILL TRIBES") : nb[(e >> 16) % nb.size()];
    switch (kind) {
      case 0: case 1: {
        const char* const out[] = {"IT ENDED IN A BITTER TRUCE.", "THE BORDER FORTS CHANGED HANDS.", "BOTH REALMS BLED FOR NOTHING.",
                                   "IT WAS WON AT THE FORD.", "THE HARVEST BURNED THAT YEAR."};
        t = "THE WAR OF THE " + std::string(pick(ADJ, e >> 20)) + " " + pick(NOUN, e >> 28) + " AGAINST " + nbName + ", " + num(y) +
            " WINTERS AGO. " + pick(out, e >> 36);
        break;
      }
      case 2: t = std::string("THE ") + pick(PLAGUE, e >> 20) + " PLAGUE TOOK ONE IN FIVE, " + num(y) + " WINTERS AGO."; break;
      case 3: t = "THE GREAT FLOOD DROWNED THE LOWLAND FARMS, " + num(y) + " WINTERS AGO."; break;
      case 4: t = "THE HUNGRY YEARS: THREE HARVESTS FAILED IN A ROW, " + num(y) + " WINTERS AGO."; break;
      case 5:
        if (!dynasty && K->ruler.house != house0) {
          dynasty = true;
          t = house0 + " DIED OUT. " + K->ruler.house + " TOOK THE SEAT, " + num(y) + " WINTERS AGO.";
        } else {
          t = "THE GOLDEN YEARS UNDER " + std::string(K->ruler.title) + " " + cult::personName(C, (uint32_t)(e >> 24), false) +
              ": NEW ROADS AND A GREAT MARKET, " + num(y) + " WINTERS AGO.";
        }
        break;
      case 6:
        t = "A PEACE WAS SWORN WITH " + nbName + " AND SEALED WITH A MARRIAGE, " + num(y) + " WINTERS AGO.";
        break;
      default:
        t = "THE TOWN OF " + cult::placeName(C, (uint32_t)(e >> 24)) + " WAS LOST TO " + fallPhrase(causeFor(e >> 40), e >> 44) + ", " +
            num(y) + " WINTERS AGO. ITS RUINS STILL STAND.";
        break;
    }
    L.push_back({y, t});
  }
  std::stable_sort(L.begin(), L.end(), [](const HistoryEntry& a, const HistoryEntry& b) { return a.yearsAgo > b.yearsAgo; });
  L.push_back({0, std::string(S.rulerTitle ? S.rulerTitle : "KING") + " " + K->ruler.name + " OF " + K->ruler.house + " RULES " +
                      K->name + " FROM " + (S.seatTitle ? S.seatTitle : "THE PALACE") + "."});
  hist_[id] = std::move(L);
}

std::vector<HistoryEntry> Realm::history(Gid kingdom) const {
  if (src_ && src_->seed() == seed_) warmHistory(*src_, kingdom);
  auto it = hist_.find(kingdom);
  return it == hist_.end() ? std::vector<HistoryEntry>() : it->second;
}

std::vector<std::string> Realm::chronicle(Gid kingdom) const {
  std::vector<std::string> out;
  const KingdomState* k = this->kingdom(kingdom);
  if (!k) return out;
  auto kname = [&](Gid g) { const KingdomState* o = this->kingdom(g); return o ? o->name : std::string("A FOREIGN REALM"); };
  if (k->rebel) {
    out.push_back(k->name + " WAS BORN OF A REVOLT AGAINST " + kname(k->parent) + " ON DAY " + num(std::max(0, k->foundedDay)) + ".");
  } else {
    const std::vector<HistoryEntry> h = history(kingdom);
    for (size_t i = 0; i + 1 < h.size(); i++) out.push_back(h[i].text);   // the last line is the genesis ruler: see below
  }
  // its deeds since the game began
  for (const War& w : wars_) {
    if (w.attacker != kingdom && w.defender != kingdom) continue;
    const Gid other = w.attacker == kingdom ? w.defender : w.attacker;
    std::string t = w.name + ": " + (w.attacker == kingdom ? "AGAINST " : "DEFENDING FROM ") + kname(other) + " OVER " +
                    warCauseName(w.cause) + ", FROM DAY " + num(w.startDay);
    t += w.endDay ? " TO DAY " + num(w.endDay) + "." : ". IT RAGES STILL.";
    out.push_back(t);
  }
  if (k->fallen) out.push_back(k->name + " IS NO MORE.");
  else out.push_back("THE " + std::string(k->ruler.title) + " " + k->ruler.name + " OF " + k->ruler.house + " RULES " + k->name + ".");
  return out;
}

RuinRecord Realm::ruin(ew::EndlessSource& src, Gid site) {
  src_ = &src;
  auto memo = ruins_.find(site);
  if (memo != ruins_.end()) return memo->second;
  RuinRecord r;
  r.site = site;
  const uint64_t h = H(seed_ ^ 0x5255494Eull, site);
  // ---- a settlement the live sim ruined
  for (const SimRuin& sr : simRuins_) {
    if (sr.site != site) continue;
    const SettlementState* s = settlement(site);
    const KingdomState* home = kingdom(sr.home);
    const KingdomState* owner = kingdom(sr.owner);
    const KingdomState* dest = kingdom(sr.destroyer);
    const uint64_t cul = home ? home->culture : (owner ? owner->culture : src.cultureAt(s ? s->gx : 0, s ? s->gy : 0));
    const cult::Culture& C = src.culture(cul);
    const cult::Society S = cult::societyOf(C);
    r.valid = true;
    r.simulated = true;
    r.culture = cul;
    r.builtByCulture = C.adjective;
    if (s)
      for (const ew::SettlementNode& n : src.settlementsIn(s->gx - 1, s->gy - 1, s->gx + 2, s->gy + 2, true))
        if (n.id == site) r.oldName = n.name;
    if (r.oldName.empty()) r.oldName = cult::placeName(C, (uint32_t)h);
    r.builtBy = home ? "THE KINGDOM OF " + home->name : "THE FREE FOLK OF THE WILDLANDS";
    r.extinct = home ? home->fallen : true;
    r.builtByKingdom = home && !home->fallen ? home->id : 0;
    r.lastLord = std::string(S.lordTitle ? S.lordTitle : "LORD") + " " + cult::personName(C, (uint32_t)(h >> 9), (h & 3) == 0);
    r.foundedYearsAgo = home ? std::max(1, -home->foundedDay / YEAR) : 30 + (int)(h % 120);
    r.fellYearsAgo = std::max(0, (day_ - (int)sr.day) / YEAR);
    r.cause = sr.cause;
    r.destroyer = dest && !dest->fallen ? dest->id : 0;
    r.destroyerName = dest ? dest->name : std::string();
    r.clues.push_back("NOTICE: BY ORDER OF " + r.lastLord + ", ALL GRAIN IS TO BE BROUGHT TO THE HALL.");
    if (sr.cause == FallCause::War && dest)
      r.clues.push_back("JOURNAL: THE BANNERS OF " + dest->name + " ON THE ROAD AT DAWN. WE RAN WITH WHAT WE COULD CARRY.");
    else
      r.clues.push_back("JOURNAL: THE LAST SACK OF GRAIN IS GONE. THE CHILDREN EAT BARK. TOMORROW WE LEAVE " + r.oldName + ".");
    r.clues.push_back("GRAVE: HERE LIE THE PEOPLE OF " + r.oldName + ", WHO DID NOT LIVE TO SEE THE SPRING.");
    r.clues.push_back("INSCRIPTION: " + r.oldName + ", A FREE TOWN OF " + (home ? home->name : std::string("THE WILDLANDS")) + ".");
    ruins_[site] = r;
    return r;
  }
  // ---- a ruin from the pre-play history (the generator's SiteType::Ruin)
  if (ew::idKind(site) != ew::IdKind::Site) { ruins_[site] = r; return r; }
  const ew::RegionPlan& rp = src.region(ew::idRx(site), ew::idRy(site));
  const ew::SitePlan* sp = nullptr;
  for (const ew::SitePlan& p : rp.sites) if (p.id == site) { sp = &p; break; }
  if (!sp || sp->type != SiteType::Ruin) { ruins_[site] = r; return r; }
  const ew::SitePlan plan = *sp;   // the region reference is only valid until the next region() call
  const uint64_t cul = plan.culture ? plan.culture : src.cultureAt(plan.ex, plan.ey);
  const cult::Culture& C = src.culture(cul);
  const cult::Society S = cult::societyOf(C);
  r.valid = true;
  r.culture = cul;
  r.builtByCulture = C.adjective;
  r.oldName = cult::placeName(C, (uint32_t)(h >> 5));
  // the living realm whose land it lies in (or the nearest seat), and the extinct realms before it
  Gid living = src.kingdomAt(plan.ex, plan.ey);
  if (!living) living = src.kingdomOfCell(ew::EndlessSource::kcellOf(plan.ex), ew::EndlessSource::kcellOf(plan.ey));
  const ew::KingdomPlan* lp = living ? src.kingdom(living) : nullptr;
  const KingdomState* lk = living ? kingdom(living) : nullptr;
  const int livingAge = lk ? std::max(40, -lk->foundedDay / YEAR) : 40 + (int)(ew::mix64(living ^ seed_) % 161);   // == genesis
  const std::vector<Past> past = lp ? pastRealms(src, seed_, living, lp->culture, livingAge) : std::vector<Past>();
  const bool byLiving = lp && (h % 100) < 30;
  if (byLiving) {
    r.builtBy = "THE KINGDOM OF " + lp->name;
    r.extinct = false;
    r.builtByKingdom = living;
    r.foundedYearsAgo = std::max(10, livingAge - (int)((h >> 12) % (uint64_t)std::max(1, livingAge / 2)));
    r.fellYearsAgo = 3 + (int)((h >> 20) % (uint64_t)std::max(1, r.foundedYearsAgo - 5));
  } else if (!past.empty()) {
    const Past& p = past[(h >> 8) % past.size()];
    r.builtBy = "THE KINGDOM OF " + p.name;
    r.extinct = true;
    r.foundedYearsAgo = p.foundedAgo - (int)((h >> 12) % 40);
    r.fellYearsAgo = p.fellAgo + (int)((h >> 20) % 10);
    if (r.foundedYearsAgo <= r.fellYearsAgo) r.foundedYearsAgo = r.fellYearsAgo + 30;
  } else {
    r.builtBy = "THE KINGDOM OF " + cult::kingdomName(C, (uint32_t)(h >> 14));
    r.extinct = true;
    r.fellYearsAgo = 60 + (int)((h >> 20) % 300);
    r.foundedYearsAgo = r.fellYearsAgo + 50 + (int)((h >> 30) % 200);
  }
  r.cause = causeFor(h >> 28);
  if (!byLiving && !past.empty() && (h >> 36) % 3 == 0) r.cause = past[(h >> 8) % past.size()].cause;   // fell with its realm
  // a destroyer: a living neighbour when the war is recent enough for it to have been there
  if (r.cause == FallCause::War) {
    const int32_t kx = ew::EndlessSource::kcellOf(plan.ex), ky = ew::EndlessSource::kcellOf(plan.ey);
    const int dx = (int)((h >> 40) % 3) - 1, dy = (int)((h >> 44) % 3) - 1;
    const Gid o = src.kingdomOfCell(kx + dx, ky + dy);
    const ew::KingdomPlan* op = o && o != living ? src.kingdom(o) : nullptr;
    const int oAge = 40 + (int)(ew::mix64(o ^ seed_) % 161);
    if (op && oAge > r.fellYearsAgo) { r.destroyer = o; r.destroyerName = op->name; }
    else r.destroyerName = "THE " + std::string(pick(ADJ, h >> 48)) + " HOST";
  }
  const bool fem = ((h >> 50) & 3) == 0;
  const std::string lordTitle = fem ? "LADY" : (S.lordTitle ? S.lordTitle : "LORD");
  r.lastLord = lordTitle + " " + cult::personName(C, (uint32_t)(h >> 33), fem) + " " + pick(EPITHET, h >> 52);
  // the clues (each begins with what it is written on: STORY places them on that prop)
  const std::string fell = num(r.fellYearsAgo) + " WINTERS AGO";
  const uint64_t c = H(h, 7);
  std::vector<std::string>& cl = r.clues;
  cl.push_back("INSCRIPTION: " + r.lastLord + " RAISED THESE WALLS FOR " + r.builtBy + ".");
  cl.push_back("STATUE: " + r.lastLord + ", LAST " + lordTitle + " OF " + r.oldName + ".");
  switch (r.cause) {
    case FallCause::War:
      cl.push_back("JOURNAL: THE BANNERS OF " + r.destroyerName + " ARE ON THE RIDGE. WE HAVE NO MORE ARROWS.");
      cl.push_back("GRAVE: HERE LIE THE DEFENDERS OF " + r.oldName + ". THEY HELD THE GATE THREE DAYS.");
      break;
    case FallCause::Plague:
      cl.push_back("JOURNAL: THE SICKNESS HAS REACHED THE MILL. THE PRIESTS HAVE SHUT THE TEMPLE DOORS.");
      cl.push_back("GRAVE: A PIT OF MANY, UNNAMED. SOMEONE HAS SCRATCHED: FORGIVE US.");
      break;
    case FallCause::Flood:
      cl.push_back("JOURNAL: THE RIVER ROSE AGAIN IN THE NIGHT. THE LOWER STREETS ARE GONE.");
      cl.push_back("MURAL: A GREAT WAVE OVER THE ROOFS OF " + r.oldName + ", AND A LORD WHO WOULD NOT LEAVE.");
      break;
    case FallCause::Famine:
      cl.push_back("JOURNAL: THE THIRD HARVEST HAS FAILED. THE LORD'S GRANARY IS LOCKED AND GUARDED.");
      cl.push_back("GRAVE: " + cult::personName(C, (uint32_t)(c >> 8), true) + ", AGED SIX. SHE WAS ALWAYS HUNGRY.");
      break;
    case FallCause::Collapse:
      cl.push_back("JOURNAL: NO TAX CART HAS COME FROM THE CAPITAL IN TWO YEARS. THE ROADS ARE EMPTY. WE ARE FORGOTTEN.");
      cl.push_back("LETTER: COME HOME TO " + (lp ? lp->name : std::string("THE LOWLANDS")) + ", BROTHER. THERE IS NOTHING LEFT THERE.");
      break;
    case FallCause::Dragon:
      cl.push_back("JOURNAL: FIRE FROM THE SKY. THE ROOFS BURN LIKE STRAW. IT CIRCLES STILL.");
      cl.push_back("MURAL: A WINGED SHADOW OVER " + r.oldName + ", AND THE LORD'S SPEAR BROKEN.");
      break;
    case FallCause::Curse:
      cl.push_back("JOURNAL: THE WELL WHISPERS AT NIGHT. THE CHILDREN SAY THE OLD LORD WALKS THE WALL.");
      cl.push_back("INSCRIPTION: LET NONE DISTURB WHAT SLEEPS BENEATH " + r.oldName + ".");
      break;
    default: break;
  }
  if ((c >> 20) % 2) cl.push_back("GRAVE: " + r.lastLord + ". " + r.oldName + " FELL " + fell + ".");
  if ((c >> 24) % 3 != 0) cl.push_back("MURAL: " + r.lastLord + " CROWNED BENEATH THE ARMS OF " + r.builtBy + ".");
  if ((c >> 28) % 2) cl.push_back("JOURNAL: " + r.oldName + " WAS FOUNDED " + num(r.foundedYearsAgo) + " WINTERS AGO, THEY SAY. I DOUBT WE SEE ANOTHER.");
  if ((c >> 32) % 3 == 0 && r.extinct) cl.push_back("COIN: STAMPED WITH THE CROWN OF " + r.builtBy + ", A REALM NO ONE REMEMBERS.");
  if (cl.size() > 8) cl.resize(8);
  ruins_[site] = r;
  return r;
}

}  // namespace realm
