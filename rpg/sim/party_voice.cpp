// M8 Fellowship, VOICE lane: who a companion is, read fresh from the living world (rpg/sim/party_voice.h), and the word
// tables the companions' talk and banter share. Pure reads: no RNG draw, nothing saved.
#include "rpg/sim/party_voice.h"

#include <algorithm>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/talk.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"

namespace party {
namespace voice {

const char* jobWord(int job) {
  switch ((life::Job)job) {
    case life::Job::None: case life::Job::Child: case life::Job::Elder: case life::Job::COUNT: return "";
    default: return life::jobName((life::Job)job);
  }
}

std::string article(const std::string& noun) {
  if (noun.empty()) return noun;
  const bool vowel = std::string("AEIOU").find(noun[0]) != std::string::npos;
  return std::string(vowel ? "AN " : "A ") + noun;
}

const char* drinkWord(int d) {
  static const char* const w[] = {"ALE", "MEAD", "WINE", "TEA", "KUMIS", "NECTAR"};
  return d >= 0 && d < 6 ? w[d] : "ALE";
}
const char* stapleWord(int s) {
  static const char* const w[] = {"BREAD", "FISH", "RICE", "FLATBREAD", "MUTTON", "STEW", "FRUIT", "ROOTS", "CHEESE", "MAIZE"};
  return s >= 0 && s < 10 ? w[s] : "BREAD";
}
// how each people builds (cult::Archetype order: Fjordfolk, Highland, Heartland, Imperial, Dune, Steppe, Marsh, Jade,
// River, SunTemple, Sylvan, Starspire)
const char* houseWords(int a) {
  static const char* const w[] = {"LONGHOUSES WITH GRASS ON THE ROOFS", "LOW STONE HOUSES WITH THICK WALLS", "TIMBER HOUSES UNDER THATCH",
                                  "STONE HOUSES WITH COLUMNS AND TILES", "MUD-BRICK HOUSES WITH FLAT ROOFS", "FELT TENTS WE CAN CARRY",
                                  "HOUSES ON STILTS OVER THE WATER", "HOUSES WITH CURLED ROOFS INSIDE WALLED YARDS", "HOUSES THAT FACE THE RIVER",
                                  "PAINTED HOUSES ROUND A TEMPLE", "HOUSES IN THE BRANCHES OF THE OLD TREES", "TALL WHITE TOWERS"};
  return a >= 0 && a < 12 ? w[a] : w[2];
}
const char* landWords(int a) {
  static const char* const w[] = {"THE FJORDS", "THE HIGH GLENS", "THE HEARTLAND", "THE OLD EMPIRE", "THE DUNES", "THE STEPPE",
                                  "THE MARSHES", "THE JADE COUNTRY", "THE RIVERLANDS", "THE SUN COUNTRY", "THE DEEP WOODS", "THE WHITE SPIRES"};
  return a >= 0 && a < 12 ? w[a] : w[2];
}
const char* riteWords(int burial) {
  static const char* const w[] = {"BURIED", "LAID UNDER A CAIRN", "GIVEN TO THE PYRE", "GIVEN TO THE SKY", "GIVEN TO THE SEA"};
  return burial >= 0 && burial < 5 ? w[burial] : w[0];
}

std::string ago(int days) {
  if (days <= 0) return "TODAY";
  if (days == 1) return "YESTERDAY";
  if (days < 7) return speech::countWord(days) + " DAYS AGO";
  if (days < 14) return "LAST WEEK";
  if (days < 30) return speech::countWord(days / 7) + " WEEKS AGO";
  if (days < 60) return "A MONTH AGO";
  if (days < 365) return speech::countWord(days / 30) + " MONTHS AGO";
  if (days < 730) return "A YEAR AGO";
  return speech::countWord(days / 365) + " YEARS AGO";
}

std::string given(const std::string& name) {
  const size_t sp = name.find(' ');
  return sp == std::string::npos ? name : name.substr(0, sp);
}

Bio bio(const Game& g, const Member& m) {
  Bio b;
  b.name = m.name;
  b.female = m.female;
  b.age = m.age;
  b.job = m.job;
  b.trade = jobWord(m.job);
  Game& G = const_cast<Game&>(g);   // (loading a site's record is a cache fill, not a change of state)
  auto siteName = [&](ew::Gid id, int* type) -> std::string {
    if (!id) return std::string();
    const int h = story::siteByIdLoad(G, id);
    if (h < 0 || h >= (int)g.world.sites.size()) return std::string();
    if (type) *type = (int)g.world.sites[(size_t)h].type;
    return g.world.sites[(size_t)h].name;
  };
  b.home = siteName(m.site, &b.homeType);
  if (m.site) {
    const int h = g.world.siteHandle(m.site);
    if (h >= 0 && g.world.sites[(size_t)h].kingdom >= 0 && g.world.sites[(size_t)h].kingdom < (int)g.world.kingdoms.size())
      b.homelandId = g.world.kingdoms[(size_t)g.world.sites[(size_t)h].kingdom].id;
  }
  if (!b.homelandId) b.homelandId = m.stake.kingdom;
  if (b.homelandId) b.homeland = story::kingdomName(g, b.homelandId);
  if (m.culture && g.world.src) {
    const cult::Culture& C = g.world.src->culture(m.culture);
    b.people = C.adjective;
    b.land = C.name;
    b.arch = (int)C.archetype;
    if (!C.faith.names.empty()) b.god = C.faith.names[0];
    b.faith = C.faith.kind;
    b.burial = C.faith.burial;
    b.drink = C.customs.drink;
    b.staple = C.customs.staple[0];
  }
  const life::Census* c = m.site ? g.life.find(m.site) : nullptr;
  if (c && m.res >= 0 && m.res < (int)c->res.size()) {
    const life::Resident& r = c->res[(size_t)m.res];
    auto kin = [&](int idx, const char* he, const char* she) {
      Kin k;
      if (idx < 0 || idx >= (int)c->res.size()) return k;
      const life::Resident& o = c->res[(size_t)idx];
      k.name = o.name; k.female = o.female; k.idx = idx;
      k.word = o.female ? she : he;
      k.dead = (o.flags & life::RF_DEAD) != 0;
      k.away = (o.flags & life::RF_AWAY) != 0 && !(o.flags & life::RF_DEAD);
      return k;
    };
    if (r.spouse >= 0) { Kin k = kin(r.spouse, "HUSBAND", "WIFE"); if (!k.name.empty()) b.household.push_back(k); }
    for (const life::Resident& o : c->res)
      if (o.parent == (int)r.idx && o.idx != r.idx) { Kin k = kin(o.idx, "SON", "DAUGHTER"); if (!k.name.empty()) b.household.push_back(k); }
    if (r.parent >= 0) {
      Kin k = kin(r.parent, "FATHER", "MOTHER");
      if (!k.name.empty()) b.household.push_back(k);
      for (const life::Resident& o : c->res)
        if (o.parent == r.parent && o.idx != r.idx) { Kin s = kin(o.idx, "BROTHER", "SISTER"); if (!s.name.empty()) b.household.push_back(s); }
    }
    for (int f : g.life.friendsOf(*c, r.idx)) {
      if (f < 0 || f >= (int)c->res.size() || f == (int)r.idx) continue;
      Kin k = kin(f, "FRIEND", "FRIEND");
      if (k.dead) continue;
      b.friendOf = k;
      break;
    }
    if (m.stake.kin)
      for (const life::Resident& o : c->res)
        if (life::npcId(c->site, o.idx) == m.stake.kin) {
          b.stakeKin = kin(o.idx, "KINSMAN", "KINSWOMAN");
          for (const Kin& h : b.household) if (h.idx == (int)o.idx) b.stakeKin.word = h.word;
          if (b.stakeKin.word == "KINSMAN" || b.stakeKin.word == "KINSWOMAN") b.stakeKin.word = o.female ? "COUSIN" : "COUSIN";
          break;
        }
  }
  // (a place called for what it is, "TOMB OF IRAR", is spoken with its article: "THE TOMB OF IRAR")
  auto spoken = [](const std::string& n) { return n.find(" OF ") != std::string::npos && n.compare(0, 4, "THE ") != 0 ? "THE " + n : n; };
  if (m.stake.site && m.stake.site != m.site) b.stakeSite = spoken(siteName(m.stake.site, nullptr));
  if (m.stake.ruin) b.stakeRuin = spoken(siteName(m.stake.ruin, nullptr));
  if (m.stake.kingdom) b.stakeKingdom = story::kingdomName(g, m.stake.kingdom);
  return b;
}

// ---------------------------------------------------------------- the FELLOWS lane's sellers and hirelings
std::string pupPitch(const Game& g, int species, int price) {
  (void)g;
  if (species == 1) {
    if (price <= 0)
      return "THE WOLVES THAT DENNED HERE ARE GONE, BUT ONE PUP IS LEFT, SHIVERING IN THE BACK OF THE DEN. IT WON'T LAST A NIGHT ALONE. "
             "RAISE IT AND IT WILL NEVER LEAVE YOUR SIDE.";
    return "A WOLF PUP, ORPHANED IN THE HILLS. MOST FOLK WON'T TOUCH ONE, BUT RAISED BY HAND IT WILL GUARD YOU WITH ITS LIFE. " +
           speech::countWord(price) + " GOLD AND IT'S YOURS.";
  }
  return "MY OLD HOUND WHELPED SIX THIS SPRING AND I CAN'T FEED THEM ALL. THIS ONE IS THE BOLDEST OF THE LITTER. RAISE IT RIGHT AND IT'LL "
         "FOLLOW YOU INTO A FIRE. " + std::to_string(price) + " GOLD.";
}

std::string pupTaken(const Game& g, int species, const std::string& pupName) {
  (void)g;
  const std::string nm = pupName.empty() ? std::string("THE LITTLE ONE") : pupName;
  if (species == 1) return "THERE. KEEP IT WARM AND FEED IT FROM YOUR OWN HAND. A WOLF REMEMBERS WHO FED IT, ALWAYS.";
  return "GO ON THEN, " + nm + ". MIND YOU WALK IT EVERY DAY, AND DON'T LET IT SLEEP ON YOUR BOOTS. IT WILL ANYWAY.";
}

std::string mercTerms(const Game& g, const std::string& who, int wage) {
  (void)g; (void)who;
  return "I FIGHT FOR PAY: " + std::to_string(wage) + " GOLD A DAY, PAID EACH MORNING. MISS A DAY AND I WALK. I DON'T ASK WHERE WE'RE "
         "GOING, AND I DON'T CARE WHY.";
}

}  // namespace voice
}  // namespace party
