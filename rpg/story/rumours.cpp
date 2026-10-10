// M4 "Banners": the realm's voice (VISION_PLAN 4.6: events and rumours; owner 15.6.3: wars are foreshadowed by
// rumours, prices, refugees and troop movements). STORY lane.
//
// Every realm event is a rumour. Its reach grows 30 tiles a day from where it happened, and it fades after 30 days (60
// for a fall or a first contact). Who tells it: innkeepers (70 %), travellers (50 %), guards (60 %, war news only) and
// everyone else (25 %), each in their own culture's voice ("WORD CAME UP THE FJORD: ...", "IT IS WHISPERED IN THE TEA
// HOUSES THAT ..."). Hearing one marks the event heard (the News journal), tells the view (Ev::News) and puts a place
// it names on the map as rumoured.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace story {

namespace {
using realm::EvType;

enum Cat { CAT_WAR, CAT_HARD, CAT_GOOD };
Cat catOf(EvType t) {
  switch (t) {
    case EvType::BorderIncident: case EvType::Skirmish: case EvType::WarDeclared: case EvType::SiegeBegun: case EvType::TownTaken:
    case EvType::TownBurned: case EvType::CivilWar: case EvType::KingdomFell: case EvType::TroopsMarching: case EvType::Ruined:
      return CAT_WAR;
    case EvType::FirstContact: case EvType::TradeDeal: case EvType::Peace: case EvType::Succession: case EvType::KingdomRose:
    case EvType::Resettled: case EvType::Festival: case EvType::SiegeBroken:
      return CAT_GOOD;
    default: return CAT_HARD;
  }
}

const char* const kOpen[8][3] = {
    {"", "", ""},
    {"WORD CAME UP THE FJORD:", "THE SKALDS ARE ALREADY SINGING IT:", "HEAR THIS, AND DRINK TO IT OR AGAINST IT:"},
    {"THEY SAY", "MARK MY WORDS,", "THE CARTERS BRING WORD:"},
    {"BY THE SUN'S OPEN EYE,", "THE CARAVANS SAY", "IT IS SCRATCHED ON EVERY WELL-STONE:"},
    {"THE RIDERS BRING IT ON THE WIND:", "FROM CAMP TO CAMP THEY SAY", "THE HERDERS SPEAK OF NOTHING ELSE:"},
    {"DOWN ON THE WATER THEY SAY", "THE EEL-BOATS BROUGHT WORD:", "THE FERRYMEN SWEAR"},
    {"IT IS WHISPERED IN THE TEA HOUSES THAT", "THE MAGISTRATES' RUNNERS SAY", "HONOURED STRANGER, IT IS SAID"},
    {"THE LEAVES CARRY IT:", "SORROW TRAVELS FAST UNDER THE BOUGHS:", "THE SINGERS SAY"},
};
const char* const kClose[8][3] = {
    {"", "", ""},
    {" SHARPEN WHAT YOU CARRY.", " THE GODS ARE COLD THIS YEAR.", " A GOOD SIGN, IF YOU HOLD WITH SIGNS."},
    {" PRAY IT STAYS OVER THERE.", " BREAD WILL COST MORE BY WINTER.", " ABOUT TIME SOMETHING WENT RIGHT."},
    {" MAY THE SAND COVER THEIR TRACKS.", " THE WELLS REMEMBER LEAN YEARS.", " THE SUN SMILES ON SOMEONE."},
    {" THE GRASS WILL BE RED BY SPRING.", " THE HERDS ARE THIN ALREADY.", " GOOD GRAZING AHEAD, THEN."},
    {" THE RIVER CARRIES THE DEAD DOWN FIRST.", " EVEN THE EELS ARE THIN.", " THE TIDE TURNS FOR SOMEONE."},
    {" HEAVEN'S MANDATE IS A FICKLE THING.", " THE GRANARIES ARE COUNTED TWICE NOW.", " AN AUSPICIOUS OMEN."},
    {" THE OLD TREES HAVE SEEN WARS BEFORE.", " EVEN THE ROOTS GO HUNGRY.", " THE STARS ARE KIND THIS SEASON."},
};

const char* numberWord(int n) {
  static const char* w[] = {"NO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN"};
  return n >= 0 && n <= 10 ? w[n] : "MANY";
}

const char* causeOf(realm::WarCause c) {
  switch (c) {
    case realm::WarCause::Famine: return "OVER GRAIN THEY CANNOT GROW";
    case realm::WarCause::BrokenTrade: return "OVER THE BROKEN TRADE";
    case realm::WarCause::BorderDispute: return "OVER THE BORDER VILLAGES";
    case realm::WarCause::Succession: return "OVER WHO SITS THE THRONE";
    case realm::WarCause::Revenge: return "FOR AN OLD WRONG";
    case realm::WarCause::Conquest: return "FOR LAND, PLAIN AND SIMPLE";
    case realm::WarCause::Raiders: return "OVER RAIDS ACROSS THE BORDER";
    default: return "";
  }
}

std::string siteName(const Game& g, ew::Gid id, int32_t gx, int32_t gy) {
  if (!id) return "";
  const int h = g.world.siteHandle(id);
  if (h >= 0) return g.world.sites[(size_t)h].name;
  if (g.world.src)
    for (const ew::SettlementNode& n : g.world.src->settlementsIn(gx - 2, gy - 2, gx + 3, gy + 3, true))
      if (n.id == id) return n.name;
  return "";
}

}  // namespace

std::string agoWords(int d) {
  if (d <= 0) return "THIS VERY DAY";
  if (d == 1) return "YESTERDAY";
  if (d <= 10) return std::string(numberWord(d)) + " DAYS AGO";
  if (d < 14) return "TEN DAYS AGO OR MORE";
  if (d < 18) return "A FORTNIGHT AGO";
  if (d < 25) return "THREE WEEKS AGO";
  if (d < 45) return "A MONTH AGO";
  return "MANY WEEKS AGO";
}

int voiceOf(const Game& g, uint64_t cultureId) {
  if (!cultureId || !g.world.src) return 2;
  switch (g.world.src->culture(cultureId).archetype) {
    case cult::Archetype::Fjordfolk: case cult::Archetype::Highland: return 1;
    case cult::Archetype::Heartland: case cult::Archetype::Imperial: return 2;
    case cult::Archetype::Dune: case cult::Archetype::SunTemple: return 3;
    case cult::Archetype::Steppe: return 4;
    case cult::Archetype::Marsh: case cult::Archetype::River: return 5;
    case cult::Archetype::Jade: return 6;
    case cult::Archetype::Sylvan: case cult::Archetype::Starspire: return 7;
    default: return 2;
  }
}

int rumourLifeDays(EvType t) {
  return t == EvType::TownTaken || t == EvType::KingdomFell || t == EvType::Ruined || t == EvType::FirstContact ? 60 : 30;
}
int rumourReach(int ageDays) { return 24 + 30 * std::max(0, ageDays); }
bool rumourReaches(const realm::WorldEvent& e, int day, int32_t gx, int32_t gy) {
  const int age = day - (int)e.day;
  if (age < 0 || age > rumourLifeDays(e.type)) return false;
  const int64_t dx = gx - e.gx, dy = gy - e.gy, r = rumourReach(age);
  return dx * dx + dy * dy <= r * r;
}
bool warNews(EvType t) {
  switch (t) {
    case EvType::BorderIncident: case EvType::Skirmish: case EvType::WarDeclared: case EvType::SiegeBegun: case EvType::SiegeBroken:
    case EvType::TownTaken: case EvType::TownBurned: case EvType::Peace: case EvType::CivilWar: case EvType::TroopsMarching:
    case EvType::KingdomFell:
      return true;
    default: return false;
  }
}
int speakerChance(int role) {
  switch ((Role)role) {
    case Role::Innkeeper: return 70;
    case Role::Traveller: return 50;
    case Role::Guard: case Role::Soldier: case Role::Captain: return 60;
    case Role::Herald: return 100;
    default: return 25;
  }
}

std::string newsLine(const Game& g, const realm::WorldEvent& e, uint64_t voiceCulture) {
  const std::string A = e.a ? kingdomName(g, e.a) : std::string("A FAR REALM");
  const std::string B = e.b ? kingdomName(g, e.b) : std::string("ITS NEIGHBOUR");
  std::string S = siteName(g, e.site, e.gx, e.gy);
  int32_t px = 0, py = 0;
  playerGlobal(g, px, py);
  const float dist = std::hypot((float)(e.gx - px), (float)(e.gy - py));
  if (S.empty()) S = std::string("A TOWN TO THE ") + dirWord(e.gx - px, e.gy - py, false);
  const int age = std::max(0, g.day - (int)e.day);
  const uint32_t h = hash32(e.id * 2654435761u ^ (uint32_t)voiceCulture);
  std::string core;
  switch (e.type) {
    case EvType::FirstContact: core = "ENVOYS OF " + A + " AND " + B + " HAVE MET FOR THE FIRST TIME"; break;
    case EvType::TradeDeal: core = A + " AND " + B + " HAVE SIGNED A TRADE PACT, " + std::string(h & 1 ? "GRAIN FOR IRON" : "SALT FOR TIMBER"); break;
    case EvType::TradeBroken: core = "THE TRADE BETWEEN " + A + " AND " + B + " HAS BROKEN DOWN, AND THE WAGONS STAND EMPTY"; break;
    case EvType::HarvestFailed: core = "THE HARVEST FAILED AROUND " + S + ", BLIGHT IN THE ROOTS"; break;
    case EvType::Famine: core = "HUNGER GRIPS " + S + ". THEY ARE EATING THE SEED CORN"; break;
    case EvType::BorderIncident: core = "BORDER RIDERS OF " + A + " AND " + B + " CROSSED BLADES NEAR " + S; break;
    case EvType::Skirmish: core = "SOLDIERS OF " + A + " AND " + B + " FOUGHT IT OUT NEAR " + S + ". MEN DIED ON BOTH SIDES"; break;
    case EvType::WarDeclared: {
      core = A + " HAS DECLARED WAR ON " + B;
      if (const realm::War* w = g.realm.warBetween(e.a, e.b)) { const std::string c = causeOf(w->cause); if (!c.empty()) core += " " + c; }
      break;
    }
    case EvType::SiegeBegun: core = A + " HAS LAID SIEGE TO " + S + ". NOTHING GOES IN OR OUT"; break;
    case EvType::SiegeBroken: core = "THE SIEGE OF " + S + " WAS BROKEN AND THE CAMP BURNED"; break;
    case EvType::TownTaken: core = S + " HAS FALLEN TO " + A + ". NEW BANNERS FLY OVER IT"; break;
    case EvType::TownBurned: core = S + " WAS PUT TO THE TORCH. THE SMOKE WAS SEEN FOR MILES"; break;
    case EvType::Refugees: core = "REFUGEES CROWD THE ROADS TO " + S + ", CARTS AND CHILDREN AND ALL"; break;
    case EvType::Peace: core = A + " AND " + B + " HAVE MADE PEACE. THE ARMIES ARE GOING HOME"; break;
    case EvType::RulerDied: {
      std::string rn, rt;
      core = g.story.rulerOf(g, e.a, rn, rt) ? "THE " + rt + " OF " + A + " IS DEAD" : "THE RULER OF " + A + " IS DEAD";
      break;
    }
    case EvType::Succession: {
      std::string rn, rt;
      core = g.story.rulerOf(g, e.a, rn, rt) ? A + " HAS A NEW " + rt + ": " + rn : A + " HAS A NEW RULER";
      break;
    }
    case EvType::CivilWar: core = A + " HAS SPLIT IN CIVIL WAR, LORD AGAINST LORD"; break;
    case EvType::KingdomFell: core = A + " IS NO MORE. ITS LAST TOWN HAS FALLEN"; break;
    case EvType::KingdomRose: core = "A NEW REALM HAS RISEN: " + A; break;
    case EvType::Resettled: core = "SETTLERS HAVE GONE BACK TO " + S + " TO REBUILD"; break;
    case EvType::Ruined: core = S + " LIES IN RUINS NOW, EMPTY AS A SKULL"; break;
    case EvType::Festival: core = S + " HOLDS A GREAT FESTIVAL. THE ALE RUNS IN THE STREETS"; break;
    case EvType::TroopsMarching: core = "COLUMNS OF " + A + " SOLDIERS ARE MARCHING ON THE ROADS NEAR " + S; break;
    case EvType::PricesRising: core = "PRICES ARE CLIMBING IN " + S + ": SALT, IRON, BREAD, ALL OF IT"; break;
    // M6 Steel (FOES lane): the beast by name (its species in mag; foes::beastNameNear finds the world boss or named
    // unique of that species that roams there)
    case EvType::BeastRaid: case EvType::BeastSlain: {
      // (M6 fixer r3) BeastSlain packs the rank and who killed it above the species (realm::Realm::beastSlain)
      const bool slain = e.type == EvType::BeastSlain;
      const int mon = slain ? (e.mag & 0xff) : e.mag;
      const int rank = slain ? ((e.mag >> 8) & 7) : (int)foes::Rank::WorldBoss;
      const bool byWanderer = !slain || (e.mag & 0x800) != 0;
      std::string beast = g.world.src ? foes::beastNameNear(*g.world.src, mon, e.gx, e.gy, rank ? rank : -1) : std::string();
      const std::string kind = mon >= 0 && mon < (int)art::Monster::COUNT ? foes::worldBossKind((art::Monster)mon) : "BEAST";
      if (e.type == EvType::BeastRaid) {
        if (beast.empty()) beast = std::string("A GREAT ") + kind;
        static const char* const how[3] = {" CAME DOWN ON ", " FELL ON ", " TORE THROUGH "};
        static const char* const after[3] = {" IN THE NIGHT AND LEFT IT BURNING", ". HALF THE ROOFS ARE GONE", ". THEY ARE STILL COUNTING THE DEAD"};
        core = beast + how[h % 3] + S + after[(h >> 3) % 3];
      } else {
        const std::string where = std::string(" OUT IN THE ") + dirWord(e.gx - px, e.gy - py, false);
        if (byWanderer)
          core = beast.empty() ? "THEY SAY A GREAT BEAST HAS BEEN SLAIN" + where + ", AND BY A WANDERER NO LESS"
                               : beast + " IS DEAD, SLAIN" + where + " BY A WANDERER. THEY'LL SING OF IT FOR YEARS";
        else
          core = beast.empty() ? "THEY SAY A GREAT BEAST HAS BEEN BROUGHT DOWN" + where + ". NOBODY KNOWS WHO DID IT"
                               : beast + " IS DEAD, BROUGHT DOWN" + where + ". HUNTERS, SOLDIERS, NOBODY IS SURE WHO";
      }
      break;
    }
    default: core = std::string(realm::evTypeName(e.type)) + " NEAR " + S; break;
  }
  const std::string when = agoWords(age);
  std::string where;
  if (dist > 90.0f && e.site) where = ", FAR TO THE " + dirWord(e.gx - px, e.gy - py, false);
  const int v = voiceCulture ? std::clamp(voiceOf(g, voiceCulture), 1, 7) : 0;
  if (v == 0) return upper(core + where + ", " + when + ".");
  const std::string open = kOpen[v][h % 3];
  return upper(open + " " + core + where + ", " + when + "." + kClose[v][catOf(e.type)]);
}

const realm::WorldEvent* pickRumour(const Game& g, int32_t gx, int32_t gy, bool warOnly, uint64_t salt) {
  const realm::WorldEvent* best = nullptr;
  int bestScore = -1;
  for (const realm::WorldEvent& e : g.realm.events()) {
    if (warOnly && !warNews(e.type)) continue;
    if (!rumourReaches(e, g.day, gx, gy)) continue;
    // unheard news first, then the freshest; ties broken by the speaker (two innkeepers need not tell the same)
    const int score = (e.heard ? 0 : 100000) + std::max(0, 1000 - (g.day - (int)e.day) * 10) + (int)(hash32((uint32_t)salt ^ e.id) % 7);
    if (score > bestScore) { bestScore = score; best = &e; }
  }
  return best;
}

std::string hearEvent(Game& g, const realm::WorldEvent& e, uint64_t voiceCulture) {
  const std::string line = newsLine(g, e, voiceCulture);
  const uint32_t id = e.id;
  const ew::Gid site = e.site;
  g.realm.markHeard(id);
  Event ev;
  ev.type = Ev::News; ev.p = g.pl().p; ev.a = (int)id; ev.f = 1; ev.s = newsLine(g, e, 0);
  g.events.push_back(ev);
  if (site) {
    const int h = g.world.siteHandle(site);
    if (h >= 0 && !g.world.sites[(size_t)h].discovered) g.world.sites[(size_t)h].rumoured = true;
  }
  return line;
}

// ---------------------------------------------------------------- (M6b CAMPAIGNS lane) campaign news and omens
namespace {

// what the roads say while a campaign runs in a kingdom (its cast filled in; {CULT} is the campaign's composed title)
struct CampaignNews { const char* plan; const char* lines[3]; };
const CampaignNews kCampaignNews[] = {
    {"burden", {"THEY SAY A STRANGER CARRIES SOMETHING OUT OF {HOME} THAT MAKES DOGS HOWL. SINCE THEN {LAND} AND {RIVAL} HAVE BEEN COUNTING SPEARS.",
                "THE DEAD OF {HOME} WALK THE ROADS AT NIGHT NOW, LOOKING FOR SOMETHING THEY LOST. MIND THE FORDS AFTER DARK.",
                "A SCHOLAR CALLED {SCHOLAR} WAS SEEN ON THE ROAD TO {FORGE}, TALKING TO THE AIR AND WRITING DOWN WHAT IT SAID BACK."}},
    {"plague", {"{CULT} SINGS AT THE WELL OF {HOME} EVERY DAWN. THE QUEUE FOR THEIR BREAD IS LONGER THAN THE ONE FOR THE TEMPLE.",
                "BLACK TONGUES IN {HOME}, AND {HEALER} THE HEALER HAS NOT SLEPT IN A WEEK. THEY SAY THE FEVER WALKS AHEAD OF A HYMN.",
                "SOMEONE WATCHES THE WELLS OF {NEXT} AT NIGHT, THEY SAY. NOBODY KNOWS WHO. NOBODY DRINKS BEFORE NOON THERE NOW."}},
    {"fae", {"SIX GONE FROM {HOME} SINCE MIDSUMMER, AND {POSTER} STILL ASKING EVERYONE ON THE ROAD IF THEY'VE SEEN {TAKEN}.",
             "LIGHTS UNDER THE HILL AT {HILL} EVERY NIGHT NOW, AND A FIDDLE PLAYED LIKE IT'S UNDER WATER. KEEP IRON ABOUT YOU.",
             "THE OLD STONES OF {STONES} WERE WARM AT DAWN, THE SHEPHERDS SAY. THE GRASS INSIDE THE RING IS GREEN IN WINTER."}},
    {"rebellion", {"{LORD}'S TITHE-TAKER, {TAKER}, HANGED A MILLER IN {HAMLET}. THE WIDOW ISN'T QUIET, AND THE VALLEY IS LISTENING.",
                   "THE GUARD OF {HOME} WHISPERS IN THE BARRACKS. THEY SAY CAPTAIN {CAPTAIN} HAS NOT SMILED SINCE THE TITHE WAS CRIED.",
                   "A SACK IN THREE AND A SON IN EVERY HOUSE, FOR THE WAR ON {RIVAL}. THE CARTS GO OUT EMPTY AND NEVER COME HOME EMPTY."}},
    {"dragon", {"{WYRM} BURNED HALF OF {HOME}, AND THE DOORS WITH THE RED EYE STILL STAND. PEOPLE ARE ASKING HOW YOU GET AN EYE.",
                "THE ASH CAMP'S TONGUE PREACHES AT SUNDOWN: FEED THE BEAST AND IT PASSES YOU BY. MORE GO UP THE HILL EVERY WEEK.",
                "THEY SAY {TOWN} IS IN SOMEONE'S LEDGER NOW, ONE LINE, AND A CROSS BESIDE IT. NOBODY IN {TOWN} HAS SEEN THE LEDGER."}},
};

// the omen of a campaign that has not begun yet, where its world facts hold ({K}: the kingdom)
struct CampaignOmen { const char* plan; const char* lines[2]; };
const CampaignOmen kCampaignOmens[] = {
    {"burden", {"THE BARROW-DEAD HAVE BEEN SEEN WALKING IN THE HILLS OF {K}, AND NOBODY WILL ADMIT TO OPENING A TOMB.",
                "A KING OF THE OLD TIME WAS BURIED WITH SOMETHING HE WOULD NOT LET GO OF, THEY SAY. SOMEWHERE IN {K}. DON'T DIG."}},
    {"plague", {"GREY ROBES ON THE ROADS OF {K}, SINGING. WHEREVER THE CHOIR WALKS, THEY SAY, A FEVER WALKED THE WEEK BEFORE.",
                "BREAD FOR NOTHING AT THE WELLS, IN A HUNGRY YEAR. MY MOTHER SAID NOTHING IS EVER FOR NOTHING. {K} WILL LEARN IT."}},
    {"fae", {"LIGHTS ON THE HILLS OF {K} AT NIGHT, AND MUSIC. KEEP IRON IN YOUR POCKET AFTER DARK, MY GRANDMOTHER USED TO SAY.",
             "A CHILD CAME HOME FROM THE HILL WITH GRASS IN HER HAIR AND SAID SHE'D BEEN GONE AN HOUR. IT WAS A WEEK."}},
    {"rebellion", {"THE CROWN'S TITHE-TAKERS ARE ON THE ROADS OF {K} WITH EMPTY CARTS AGAIN. THEY NEVER GO HOME EMPTY.",
                   "ANOTHER DECREE FROM THE PALACE OF {K}. THE HERALD READS THEM SLOWER EVERY TIME, LIKE SOMEONE HOPING TO BE INTERRUPTED."}},
    {"dragon", {"SOMEONE HAS BEEN PAINTING A RED EYE ON DOORS IN THE VILLAGES OF {K}. NOBODY WILL SAY WHO, OR WHAT IT'S FOR.",
                "PEDLARS IN {K} ARE SELLING SAFETY DOOR TO DOOR, THEY SAY. SAFE FROM WHAT, YOU ASK. THEY JUST POINT AT THE SKY."}},
};

std::string replaceAll(std::string s, const std::string& a, const std::string& b) {
  for (size_t p = s.find(a); p != std::string::npos; p = s.find(a, p + b.size())) s.replace(p, a.size(), b);
  return s;
}

std::string campaignLine(const Game& g, ew::Gid kingdom, int v, uint32_t h) {
  // a campaign running in this kingdom: its news
  for (const Instance& in : g.story.running()) {
    if (in.done || in.script.find("~@") == std::string::npos) continue;
    const Binding* land = bindingOf(in, "land");
    if (!land || land->id != kingdom) continue;
    saga::Spec s;
    if (!saga::parseSpecId(in.script, s)) continue;
    for (const CampaignNews& n : kCampaignNews) {
      if (s.arch != n.plan) continue;
      std::string line = n.lines[h % 3];
      if (line.find("{CULT}") != std::string::npos) {
        const dsl::Script* sc = saga::script(in.script);
        line = replaceAll(line, "{CULT}", sc ? sc->title : std::string("THE CHOIR"));
      }
      line = fillText(g, in, line);
      if (line.find('{') != std::string::npos) return "";   // (a role the cast does not have)
      return upper(std::string(kOpen[v][h % 3]) + " " + line);
    }
  }
  // an omen: one in three asks, of a campaign not yet told in this world whose world facts hold in the kingdom
  if ((h >> 4) % 3 != 0) return "";
  const realm::KingdomState* K = g.realm.kingdom(kingdom);
  if (!K || K->fallen) return "";
  const size_t n = sizeof(kCampaignOmens) / sizeof(kCampaignOmens[0]);
  const CampaignOmen& o = kCampaignOmens[(h >> 7) % n];
  const std::string pre = "saga" + std::to_string(saga::SAGA_GEN_VER) + "~@" + o.plan + "~";
  for (const auto& kv : g.story.played_) if (kv.first.compare(0, pre.size(), pre) == 0) return "";
  bool fits = true;
  if (std::string(o.plan) == "plague") fits = K->food < 0.5f;   // the hungry year
  if (std::string(o.plan) == "rebellion") { std::string rn, rt; fits = g.story.rulerOf(g, kingdom, rn, rt); }
  if (std::string(o.plan) == "dragon") {
    fits = false;
    for (const realm::WorldEvent& e : g.realm.events()) if (e.type == EvType::BeastRaid && (e.a == kingdom || e.b == kingdom)) fits = true;
  }
  if (!fits) return "";
  return upper(replaceAll(o.lines[(h >> 9) & 1], "{K}", K->name) + kClose[v][CAT_HARD]);
}

}  // namespace

std::string foreshadowLine(const Game& g, ew::Gid kingdom, uint64_t voiceCulture, uint64_t salt) {
  if (!kingdom) return "";
  const int v = std::clamp(voiceOf(g, voiceCulture), 1, 7);
  // (M6b) a campaign underway in this kingdom is the talk of the roads half the time; its omen, now and then
  {
    const uint32_t hc = hash32((uint32_t)salt ^ 0xCA3Bu);
    if (hc & 1) {
      const std::string c = campaignLine(g, kingdom, v, hc >> 1);
      if (!c.empty()) return c;
    }
  }
  const realm::Relation* worst = nullptr;
  for (const realm::Relation& r : g.realm.relations())
    if ((r.a == kingdom || r.b == kingdom) && r.state != realm::Rel::War && r.tension != realm::Tension::Calm && (!worst || r.tension > worst->tension))
      worst = &r;
  const uint32_t h = hash32((uint32_t)salt);
  if (worst) {
    const std::string other = kingdomName(g, worst->a == kingdom ? worst->b : worst->a);
    switch (worst->tension) {
      case realm::Tension::Strained:
        return upper(std::string(kOpen[v][h % 3]) + " THE " + other + " MERCHANTS HAVE STOPPED COMING. PRICES ARE UP A THIRD AND NOBODY WILL SAY WHY.");
      case realm::Tension::TradeBroken:
        return upper("NO GRAIN FROM " + other + " SINCE THE DEAL BROKE. BREAD COSTS DOUBLE, AND THE MILLER WATERS HIS FLOUR." + kClose[v][CAT_HARD]);
      case realm::Tension::BorderTension:
        return upper("TROOPS ON EVERY ROAD TO THE " + other + " BORDER. THEY SEARCH THE WAGONS AND TAKE WHAT THEY LIKE." + kClose[v][CAT_WAR]);
      case realm::Tension::Skirmishes:
        return upper("REFUGEES FROM THE BORDER VILLAGES SLEEP IN THE BARNS NOW. IT WILL COME TO WAR WITH " + other + ", YOU WATCH." + kClose[v][CAT_WAR]);
      default: break;
    }
  }
  if (const realm::KingdomState* K = g.realm.kingdom(kingdom))
    if (K->food < 0.3f) return upper("THE GRANARIES ARE LOW ACROSS " + K->name + ". THE " + K->ruler.title + "'S MEN COUNT EVERY SACK." + kClose[v][CAT_HARD]);
  return "";
}

}  // namespace story
