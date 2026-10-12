// M8 Fellowship, VOICE lane: what companions say on the road (rpg/sim/party.h party::banterStep, deedLine, cryLine,
// epitaph, aboutLine; rpg/sim/party_voice.h the triggers). VISION_PLAN 9: template lines triggered by context, at most
// one line every BANTER_GAP_SECS, the best trigger crossing a culture border ("These Qasri build houses like bread
// ovens. Cool inside, though."); then weather, night, a hard fight, a wonder, low health, approval and trust, news of a
// war involving their homeland, the places they know, and each other. Cries and deed reactions are said by the FELLOWS
// lane's alerts and deed handling (exempt from the gap). Every line is first person, whole sentences, at most two rows
// of the phone panel, built from the speaker's real history and their people's ways (rpg/sim/party_voice.cpp).
//
// The choice inside a trigger is a hash of who, the day and the trigger (no RNG draw); a line said lately (the talk
// memory's recent lines, within a day) or the last line said is passed over for the next one.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/biomes.h"
#include "rpg/world/source.h"

namespace party {

namespace {

using voice::Bio;
using voice::Kin;
using voice::Trig;

constexpr size_t BANTER_MAX = (size_t)speech::PANEL_CHARS * 2;   // party.h: a banter line holds two rows of the panel

uint32_t mixh(uint32_t a, uint32_t b) { return hash32(a * 0x9E3779B1u ^ hash32(b + 0x7F4A7C15u)); }

// replace every {KEY} in t
std::string subst(std::string t, const char* key, const std::string& v) {
  const std::string k = std::string("{") + key + "}";
  for (size_t p = t.find(k); p != std::string::npos; p = t.find(k, p + v.size())) t.replace(p, k.size(), v);
  return t;
}
bool unfilled(const std::string& t) { return t.find('{') != std::string::npos; }

// the first of `opts` (from a hash-chosen start) that is filled, fits and has not been said lately
std::string choose(const Game& g, const std::vector<std::string>& opts, uint32_t h, size_t maxLen = BANTER_MAX) {
  if (opts.empty()) return std::string();
  const std::string fallback;
  for (size_t k = 0; k < opts.size(); k++) {
    const std::string& s = opts[(h + k) % opts.size()];
    if (s.empty() || unfilled(s) || s.size() > maxLen || speech::wrapLines(s, speech::PANEL_CHARS).size() > 2) continue;
    const uint32_t lh = speech::lineHash(s);
    if (s == g.party.lastBanter || g.talkMem.saidRecently(lh, g.day, 1)) continue;   // (never twice in a row, or in a day)
    return s;
  }
  (void)fallback;
  return std::string();
}

const Kin* closestKin(const Bio& b) {
  for (const Kin& k : b.household) if (!k.dead && !k.away) return &k;
  return nullptr;
}
std::string homeOr(const Bio& b, const char* dflt = "HOME") { return b.home.empty() ? std::string(dflt) : b.home; }
std::string godOr(const Bio& b) { return b.god.empty() ? std::string("THE GODS") : b.god; }

bool sameFamily(uint64_t a, uint64_t b) { return a && b && cult::familyOf(a) == cult::familyOf(b); }

// how a people's houses strike someone who did not grow up in them (cult::Archetype order)
const char* const kHouses[12][2] = {
    {"THE {O} PUT GRASS ON THEIR ROOFS. I SUPPOSE IT KEEPS THE WIND OUT.", "LONGHOUSES WITH TURF ON TOP. A GOAT COULD GRAZE ON THESE {O} ROOFS."},
    {"THE {O} BUILD LOW AND THICK, LIKE THEY'RE HIDING FROM THE SKY.", "STONE ON STONE AND NOT A SCRAP OF PAINT. THE {O} DON'T WASTE A THING."},
    {"THATCH AND TIMBER EVERYWHERE. THE {O} DO LIKE THEIR COMFORTS.", "THE {O} BUILD LIKE A PICTURE IN A STORYBOOK. ALL THATCH AND ROSES."},
    {"THE {O} BUILD IN STONE AS IF THEY MEAN TO LIVE FOREVER.", "COLUMNS ON A COWSHED. ONLY THE {O} WOULD DO THAT."},
    {"THESE {O} BUILD HOUSES LIKE BREAD OVENS. COOL INSIDE, THOUGH.", "FLAT ROOFS AND MUD WALLS. THE {O} SLEEP ON TOP OF THEIR HOUSES, I HEAR."},
    {"THE {O} FOLD UP THEIR HOUSES AND CARRY THEM OFF. HANDY, IF IT DOESN'T RAIN.", "FELT TENTS AS FAR AS I CAN SEE. THE {O} NEVER STAY ANYWHERE LONG."},
    {"THE {O} BUILD ON STILTS OVER THE WATER. I'D NEVER SLEEP FOR THE CREAKING.", "EVERY {O} HOUSE HAS ITS FEET IN THE WATER. MINE WOULD ROT IN A WEEK."},
    {"THE {O} CURL THEIR ROOFS UP AT THE CORNERS, AS IF THE HOUSES WANT TO FLY.", "WALLS ROUND EVERY YARD. THE {O} DO LIKE TO KNOW WHO'S COMING."},
    {"EVERY {O} DOOR FACES THE RIVER. I SUPPOSE THE RIVER IS THEIR ROAD.", "THE {O} BUILD AS CLOSE TO THE WATER AS THEY DARE. BRAVE, OR FOOLISH."},
    {"THE {O} PAINT THEIR WALLS LIKE A SUNSET. YOU COULD SEE THEM FROM A MILE OFF.", "A TEMPLE IN THE MIDDLE AND EVERY HOUSE TURNED TO FACE IT. THE {O} MEAN IT."},
    {"THE {O} LIVE UP IN THE TREES. PRETTY, BUT I'D FALL OUT OF BED.", "HOUSES GROWN OUT OF LIVING WOOD. THE {O} MUST TALK TO THEIR TREES."},
    {"THE {O} BUILD THEIR TOWERS SO TALL THEY MUST GET DIZZY.", "WHITE STONE EVERYWHERE. THE {O} MUST SCRUB IT EVERY MORNING."}};
// ... and what they would say of home in the same breath
const char* const kHomeWay[12] = {
    "AT HOME WE BUILD LONGHOUSES AND KEEP THE FIRE IN THE MIDDLE.", "AT HOME WE BUILD IN STONE AGAINST THE WIND.",
    "AT HOME IT'S TIMBER AND THATCH, AND A GARDEN OUT THE BACK.", "AT HOME WE BUILD IN STONE, AND WE BUILD IT TO LAST.",
    "AT HOME WE BUILD THICK WALLS AGAINST THE SUN.", "AT HOME WE CARRY OUR HOUSES WITH US.",
    "AT HOME WE BUILD ON STILTS, AND THE WATER SINGS US TO SLEEP.", "AT HOME EVERY HOUSE HAS ITS OWN WALLED YARD.",
    "AT HOME EVERY DOOR OPENS ONTO THE RIVER.", "AT HOME WE PAINT OUR WALLS FOR THE SUN.",
    "AT HOME WE LIVE AMONG THE BRANCHES.", "AT HOME WE BUILD TOWARDS THE STARS."};

// the speaker's people and the land's: a border line (the VISION_PLAN 9 bar)
std::vector<std::string> borderLines(const Game& g, const Member& m, const Bio& b, uint64_t land) {
  std::vector<std::string> v;
  if (!g.world.src || !land) return v;
  const cult::Culture& O = g.world.src->culture(land);
  const int oa = (int)O.archetype;
  if (O.adjective.empty() || oa < 0 || oa >= 12) return v;
  const std::string adj = O.adjective;
  for (const char* t : kHouses[oa]) v.push_back(subst(t, "O", adj));
  // with home in the same breath (a different way of building only)
  if (b.arch != oa && b.arch >= 0 && b.arch < 12) {
    std::string t = subst(kHouses[oa][0], "O", adj);
    const std::string both = t + " " + kHomeWay[b.arch];
    if (speech::wrapLines(both, speech::PANEL_CHARS).size() <= 2) v.push_back(both);
  }
  // their drink and their gods (only when these differ from the speaker's own)
  if (O.customs.drink != b.drink) {
    v.push_back("SO THE " + adj + " DRINK " + voice::drinkWord(O.customs.drink) + " HERE. WHERE I COME FROM IT'S " + voice::drinkWord(b.drink) +
                ", AND PLENTY OF IT.");
    if (m.p.kind > 0) v.push_back("THE " + adj + " DRINK " + voice::drinkWord(O.customs.drink) + ". I'LL TRY IT, IF ONLY TO BE POLITE.");
  }
  if (!O.faith.names.empty() && !b.god.empty() && O.faith.names[0] != b.god) {
    if (m.p.pious > 0) v.push_back("THE " + adj + " PRAY TO " + O.faith.names[0] + " HERE. I'LL KEEP TO " + b.god + ", IF IT'S ALL THE SAME.");
    else v.push_back("THE " + adj + " SWEAR BY " + O.faith.names[0] + " HERE. AT LEAST THE CURSES SOUND NEW.");
  }
  if (O.customs.staple[0] != b.staple)
    v.push_back("I SMELL " + std::string(voice::stapleWord(O.customs.staple[0])) + " COOKING. THE " + adj + " MUST EAT IT WITH EVERYTHING.");
  return v;
}

// the weather lines, by what the speaker's own country is like
std::vector<std::string> weatherLines(const Bio& b, const Member& m, Trig t) {
  const bool hot = b.arch == (int)cult::Archetype::Dune || b.arch == (int)cult::Archetype::SunTemple || b.arch == (int)cult::Archetype::Steppe;
  const bool cold = b.arch == (int)cult::Archetype::Fjordfolk || b.arch == (int)cult::Archetype::Highland;
  const bool wet = b.arch == (int)cult::Archetype::Marsh || b.arch == (int)cult::Archetype::River;
  const std::string home = homeOr(b);
  std::vector<std::string> v;
  switch (t) {
    case Trig::Rain:
      v.push_back("THIS RAIN GETS INTO EVERYTHING. MY BOOTS HAVEN'T BEEN DRY IN DAYS.");
      v.push_back("RAIN AGAIN. IN " + home + " WE'D BE INDOORS BY THE FIRE BY NOW.");
      // (more than two that anyone can say: the day's rain lines are shared by the whole party, and a wet day used up
      // two of them before the next one opened their mouth)
      v.push_back("I'M SOAKED TO THE SKIN. THE NEXT ROOF WE PASS, I'M STANDING UNDER IT.");
      v.push_back("LISTEN TO IT COME DOWN. THE ROAD WILL BE MUD UP TO OUR ANKLES BY NIGHTFALL.");
      v.push_back("KEEP YOUR BOWSTRING DRY, IF YOU HAVE ONE. THIS RAIN ISN'T STOPPING ANY TIME SOON.");
      if (hot) v.push_back("WATER FALLING OUT OF THE SKY, AND NOBODY HERE EVEN LOOKS UP. I'LL NEVER GET USED TO IT.");
      if (wet) v.push_back("A LITTLE RAIN NEVER HURT ANYONE. WHERE I COME FROM, THIS IS A DRY DAY.");
      if (m.p.brave < 0) v.push_back("I CAN'T HEAR A THING OVER THIS RAIN. ANYTHING COULD BE CREEPING UP ON US.");
      break;
    case Trig::Snow:
      v.push_back("SNOW, AND MORE SNOW. MY FINGERS STOPPED TALKING TO ME AN HOUR AGO.");
      if (hot) v.push_back("SO THIS IS SNOW. IT'S PRETTIER IN THE STORIES, AND A GREAT DEAL WARMER.");
      if (cold) v.push_back("PROPER SNOW AT LAST. NOW IT FEELS LIKE HOME.");
      v.push_back("KEEP MOVING. IF WE STOP IN THIS COLD, WE WON'T START AGAIN.");
      break;
    case Trig::Heat:
      v.push_back("THIS HEAT! IN " + home + " WE'D BE SITTING IN THE SHADE BY NOW.");
      if (hot) v.push_back("HOT? THIS IS A MILD SPRING DAY WHERE I COME FROM. YOU SHOULD SEE OUR SUMMERS.");
      if (cold) v.push_back("I'M MELTING. MY PEOPLE WERE MADE FOR SNOW, NOT FOR THIS.");
      v.push_back("MY WATERSKIN IS HALF EMPTY ALREADY. LET'S FIND A WELL BEFORE NOON.");
      break;
    case Trig::Sand:
      v.push_back("SAND IN MY TEETH, SAND IN MY BOOTS. HOW DOES ANYONE LIVE OUT HERE?");
      if (hot) v.push_back("A SAND WIND. COVER YOUR FACE AND KEEP WALKING, IT PASSES.");
      break;
    case Trig::Fog:
      v.push_back("I CAN'T SEE MY OWN HAND IN THIS FOG. STAY CLOSE.");
      v.push_back("THIS MIST GETS INTO YOUR BONES. I DON'T LIKE WHAT IT HIDES.");
      break;
    case Trig::Ash:
      v.push_back("THE AIR TASTES OF ASH. WHATEVER BURNED HERE IS STILL BURNING.");
      v.push_back("ASH IN THE WIND, LIKE A FIRE THAT NEVER GOES OUT. LET'S NOT STAY.");
      break;
    case Trig::Eerie:
      v.push_back("SOMETHING ABOUT THIS PLACE MAKES MY SKIN CRAWL.");
      if (m.p.pious > 0) v.push_back(godOr(b) + " KEEP US. THIS PLACE IS WRONG, AND I CAN FEEL IT.");
      break;
    default: break;
  }
  return v;
}

// what they did at home, in their own words (the job's sentence; {HOME} filled)
std::string tradeSentence(const Bio& b) {
  const std::string home = homeOr(b, "MY VILLAGE");
  switch ((life::Job)b.job) {
    case life::Job::Farmer: return "I'VE WORKED THE FIELDS OUTSIDE " + home + " SINCE I COULD HOLD A HOE.";
    case life::Job::Fisher: return "I'VE FISHED THE WATERS ROUND " + home + " ALL MY LIFE.";
    case life::Job::Miner: return "I'VE SPENT HALF MY LIFE DOWN THE PITS BY " + home + ".";
    case life::Job::Woodcutter: return "I'VE FELLED TREES IN THE WOODS ROUND " + home + " SINCE I WAS YOUNG.";
    case life::Job::Hunter: return "I'VE HUNTED THE HILLS ROUND " + home + " SINCE I WAS SMALL.";
    case life::Job::Herder: return "I'VE KEPT FLOCKS ON THE PASTURES BY " + home + " FOR YEARS.";
    case life::Job::Smith: return "I'VE KEPT A FORGE IN " + home + " FOR YEARS, AND MY HANDS KNOW STEEL.";
    case life::Job::Merchant: return "I'VE TRADED IN " + home + " MOST OF MY LIFE, AND I KNOW A FAIR PRICE.";
    case life::Job::Baker: return "I'VE BAKED THE BREAD IN " + home + " SINCE BEFORE THE SUN CAME UP, EVERY DAY.";
    case life::Job::Tailor: return "I'VE SEWN HALF THE CLOTHES IN " + home + ".";
    case life::Job::Stablehand: return "I'VE MUCKED OUT STABLES IN " + home + " SINCE I WAS SMALL. I'M GOOD WITH HORSES.";
    case life::Job::Innkeeper: return "I'VE KEPT AN INN IN " + home + ", AND I'VE HEARD EVERY STORY THERE IS.";
    case life::Job::Server: return "I'VE CARRIED ALE AND STEW IN " + home + " FOR YEARS.";
    case life::Job::Bard: return "I'VE SUNG FOR MY SUPPER IN EVERY HALL OF " + home + ".";
    case life::Job::Scholar: return "I'VE READ EVERY BOOK IN " + home + ", SOME OF THEM TWICE.";
    case life::Job::Priest: return "I'VE TENDED THE SHRINE IN " + home + " AND HEARD MORE CONFESSIONS THAN I CAN COUNT.";
    case life::Job::Guard: return "I'VE STOOD WATCH AT " + home + "'S GATE FOR YEARS.";
    case life::Job::Noble: return "I WAS BORN TO A GOOD HOUSE IN " + home + ", AND IT BORED ME HALF TO DEATH.";
    case life::Job::Servant: return "I'VE SERVED IN A GREAT HOUSE IN " + home + " SINCE I WAS SMALL.";
    case life::Job::Labourer: return "I'VE HAULED AND DUG FOR ANYONE IN " + home + " WHO WOULD PAY ME.";
    case life::Job::Lamplighter: return "I'VE LIT THE LAMPS OF " + home + " EVERY EVENING FOR YEARS.";
    case life::Job::Beggar: return "I'VE BEGGED ON THE STREETS OF " + home + " LONGER THAN I LIKE TO SAY.";
    default: return b.home.empty() ? std::string() : "I'VE LIVED IN " + b.home + " ALL MY LIFE.";
  }
}

// the stake: what drives them on (the Hook), in one or two sentences
std::string stakeSentence(const Game& g, const Member& m, const Bio& b) {
  const int days = g.day - m.stake.day;
  const std::string when = voice::ago(std::max(1, days));
  const std::string home = homeOr(b, "MY VILLAGE");
  const uint32_t h = m.stake.seed;
  switch (m.stake.hook) {
    case Hook::BurnedHome: {
      const std::string where = b.stakeSite.empty() ? home : b.stakeSite;
      return "RAIDERS BURNED " + where + " " + when + ". I STILL SMELL THE SMOKE IN MY SLEEP, AND I MEAN TO SEE IT ANSWERED.";
    }
    case Hook::LostKin: {
      const Kin& k = b.stakeKin;
      if (k.name.empty()) return "SOMEONE OF MY BLOOD WENT MISSING " + when + ". NOBODY HAS LOOKED HARDER FOR THEM THAN I HAVE.";
      if (k.dead) return "MY " + k.word + " " + k.name + " DIED " + when + ", AND NOBODY HAS EVER TOLD ME HOW.";
      return "MY " + k.word + " " + k.name + " WENT MISSING " + when + ". NOBODY HAS LOOKED HARDER THAN I HAVE.";
    }
    case Hook::War:   // (a veteran when brave, a deserter when not: party_recruit.cpp stakeFor)
      if (m.p.brave >= 0) return "I MARCHED FOR " + (b.stakeKingdom.empty() ? std::string("MY KING") : b.stakeKingdom) + " IN THE LAST WAR. SOME NIGHTS I'M STILL MARCHING.";
      return "I RAN FROM THE LEVY WHEN " + (b.stakeKingdom.empty() ? std::string("THE KING") : b.stakeKingdom) + " WENT TO WAR. I'M NOT PROUD OF IT.";
    case Hook::Revenge: {
      const Kin* k = closestKin(b);
      (void)k;
      return h % 2 ? std::string("A BEAST TORE MY FATHER APART ON THE ROAD HOME, ") + when + ". ONE DAY I'LL FIND IT."
                   : std::string("A BANDIT CHIEF KILLED SOMEONE I LOVED ") + when + ". I'VE NEVER FORGOTTEN THE FACE.";
    }
    case Hook::Heirloom: {
      static const char* const what[] = {"MY GRANDFATHER'S SWORD", "MY MOTHER'S SILVER RING", "OUR FAMILY'S HORN", "MY GRANDMOTHER'S BROOCH"};
      const std::string it = what[h % 4];
      return it + " WAS LOST IN " + (b.stakeRuin.empty() ? std::string("AN OLD RUIN") : b.stakeRuin) + ". I'D LIKE TO BRING IT HOME.";
    }
    case Hook::Debt: return "I OWE MORE MONEY THAN I CAN EARN AT HOME. THE ROAD PAYS BETTER, IF IT DOESN'T KILL ME FIRST.";
    case Hook::Exile:
      return "I CAN'T GO BACK TO " + (b.stakeKingdom.empty() ? std::string("MY OWN COUNTRY") : b.stakeKingdom) + ". THEY MADE VERY SURE OF THAT.";
    case Hook::Faith: return "I SWORE A VOW TO " + godOr(b) + " TO WALK THE ROADS UNTIL I'VE DONE SOME GOOD IN THE WORLD.";
    case Hook::Ambition: return "I WANT MY NAME IN A SONG BEFORE I'M OLD. " + home + " WAS NEVER GOING TO GIVE ME ONE.";
    case Hook::Love: {
      const Kin& k = b.stakeKin;
      if (!k.name.empty() && !k.dead) return "SOMEONE IS WAITING FOR ME. MY SWEETHEART " + k.name + " DOESN'T KNOW I'M COMING BACK A HERO.";
      return "THERE'S SOMEONE I LOVE IN ANOTHER TOWN. ONE DAY I'LL HAVE ENOUGH TO GO TO THEM.";
    }
    default: return std::string();
  }
}

// the household sentence: who they left behind, introduced by what they are
std::string kinSentence(const Bio& b) {
  const uint32_t h = speech::lineHash(b.name);
  for (const Kin& k : b.household) {
    const std::string who = "MY " + k.word + " " + k.name;
    if (k.dead) return h % 2 ? "I BURIED " + who + " THERE, AND I VISIT WHEN I CAN." : who + " IS BURIED THERE. I STILL TALK TO " + (k.female ? "HER" : "HIM") + " SOMETIMES.";
    if (k.away) continue;
    if (k.word == "WIFE" || k.word == "HUSBAND")
      return h % 2 ? who + " KEEPS THE HOUSE WHILE I'M GONE, AND WORRIES." : who + " DIDN'T WANT ME TO GO. I PROMISED TO COME BACK.";
    if (k.word == "SON" || k.word == "DAUGHTER")
      return h % 2 ? who + " IS LOOKING AFTER THINGS UNTIL I'M BACK." : who + " THINKS I'M OFF HAVING ADVENTURES. I SUPPOSE I AM.";
    if (k.word == "MOTHER" || k.word == "FATHER")
      return h % 2 ? who + " STILL LIVES THERE, AND THINKS I'M A FOOL." : who + " MADE ME PROMISE TO EAT PROPERLY. I HAVEN'T.";
    return who + " WILL BE MISSING ME.";
  }
  if (!b.friendOf.name.empty()) {
    const std::string f = "MY FRIEND " + b.friendOf.name;
    switch (h % 3) {
      case 0: return f + " WILL BE MISSING ME AT THE INN.";
      case 1: return f + " THINKS I'VE LOST MY MIND, GOING OFF LIKE THIS.";
      default: return f + " SWORE TO KEEP MY SEAT WARM UNTIL I'M BACK.";
    }
  }
  return "NOBODY THERE WILL MISS ME MUCH, IF I'M HONEST.";
}

std::string joinAll(const std::vector<std::string>& parts, size_t maxLen) {
  std::string s;
  for (const std::string& p : parts) {
    if (p.empty()) continue;
    const std::string t = s.empty() ? p : s + " " + p;
    if (t.size() <= maxLen && speech::fitsPanel(t)) s = t;
  }
  return s;
}

// the per-member counters and the trigger state banterStep keeps (runtime only)
struct BanterState {
  const Game* g = nullptr;
  uint64_t seed = 0;
  float lastTime = 0;          // the game's clock at the last step (a clock that went back: another game, or a load)
  uint64_t land = 0;           // the culture of the land the player last stood on
  int lastSite = -2;           // the settlement the player was last in (-1 none)
  float landT = 0;             // seconds to the next look at the land
  bool fighting = false;
  float calm = 0;              // seconds without a hostile engaged near
  float minPl = 1;             // the player's lowest health fraction in the fight
  int fightDowned = -1;        // a member downed in it
  float lowT = 0;              // the low-health warning's cool-down
  int weatherSlot = -1;        // the last weather slot remarked (day * 8 + hour / 3)
  int nightDay = -1;
  std::string wonder;          // a wonder found, waiting to be remarked
  float wonderT = 0;
  int replyBy = -1;            // an exchange's answer: who, what, when
  std::string reply;
  float replyT = 0;
  float musingT = 0;
};
BanterState& st() { static BanterState s; return s; }
int g_counts[(int)Trig::COUNT] = {};

}  // namespace

namespace voice {

const char* trigName(Trig t) {
  static const char* const n[] = {"BORDER", "BORDER HOME", "HOME", "STAKE SITE", "STAKE RUIN", "WONDER", "HARD FIGHT", "LOW HEALTH", "WAR",
                                  "PRAISE", "TRUST", "RAIN", "SNOW", "HEAT", "SAND", "FOG", "ASH", "EERIE", "NIGHT", "MUSING", "PAIR", "HOUND"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Trig::COUNT, "a name for every trigger");
  return (int)t < (int)Trig::COUNT ? n[(int)t] : "";
}
int banterCount(Trig t) { return (int)t < (int)Trig::COUNT ? g_counts[(int)t] : 0; }
void banterResetCounts() { for (int& c : g_counts) c = 0; }

Trig weatherNow(const Game& g) {
  if (g.inside || !g.world.endless) return Trig::COUNT;
  const int slot = g.day * 8 + (int)(g.hour / 3);
  const uint32_t w = hash32((uint32_t)slot ^ (uint32_t)g.seed) % 100;
  const float hr = g.hour;
  const bool midday = hr >= 10.5f && hr <= 16.5f;
  const int tx = (int)std::floor(g.pl().p.x / 16), ty = (int)std::floor(g.pl().p.y / 16);
  switch (ecoInfo(g.world.over.ecoAt(tx, ty)).sky) {
    case Sky::Temperate: return w < 22 ? Trig::Rain : Trig::COUNT;
    case Sky::Showery: return w < 45 ? Trig::Rain : Trig::COUNT;
    case Sky::Misty: return Trig::Fog;
    case Sky::Dry: return w < 5 ? Trig::Rain : midday ? Trig::Heat : Trig::COUNT;
    case Sky::Arid: return w < 24 ? Trig::Sand : midday ? Trig::Heat : Trig::COUNT;
    case Sky::Snowy: case Sky::Blizzard: return w < 55 ? Trig::Snow : Trig::COUNT;
    case Sky::Monsoon: return w < 60 ? Trig::Rain : midday ? Trig::Heat : Trig::COUNT;
    case Sky::Ashfall: return Trig::Ash;
    case Sky::Eerie: return Trig::Eerie;
    default: return Trig::COUNT;
  }
}

std::string banterLine(const Game& g, int mi, Trig t, uint64_t arg, const std::string& name, uint32_t salt) {
  if (mi < 0 || mi >= (int)g.party.members.size()) return std::string();
  const Member& m = g.party.members[(size_t)mi];
  if (m.voc == Vocation::Hound) return std::string();
  const Bio b = bio(g, m);
  const uint32_t h = mixh(m.id * 131u + (uint32_t)t, salt ^ (uint32_t)g.day * 2654435761u);
  const std::string home = homeOr(b);
  std::vector<std::string> v;
  switch (t) {
    case Trig::Border: v = borderLines(g, m, b, arg); break;
    case Trig::BorderHome:
      if (b.land.empty()) break;
      v.push_back(b.land + " AGAIN. I COULD SMELL THE " + std::string(voice::stapleWord(b.staple)) + " COOKING FROM THE LAST HILL.");
      v.push_back("BACK AMONG MY OWN PEOPLE. EVERYTHING HERE LOOKS RIGHT TO ME.");
      v.push_back("HOME GROUND. EVEN THE BIRDS SOUND FAMILIAR.");
      break;
    case Trig::Home: {
      const Kin* k = closestKin(b);
      if (b.home.empty()) break;
      v.push_back("SO THIS IS " + b.home + " AGAIN. IT LOOKS SMALLER THAN I REMEMBER.");
      if (k) v.push_back(b.home + " AT LAST. MY " + k->word + " " + k->name + " WILL HAVE THE KETTLE ON, IF I KNOW THEM.");
      v.push_back("HOME AT LAST. I DIDN'T THINK I'D MISS THE OLD PLACE, BUT I DID.");
      break;
    }
    case Trig::StakeSite:
      if (b.stakeSite.empty() && m.stake.hook != Hook::BurnedHome) break;
      if (m.stake.hook == Hook::BurnedHome) {
        const std::string where = b.stakeSite.empty() ? home : b.stakeSite;
        v.push_back("THIS IS " + where + ". THIS IS WHERE THEY CAME WITH THE TORCHES.");
        v.push_back("BACK IN " + where + ". I SWORE I WOULDN'T COME BACK HERE UNTIL IT WAS ANSWERED.");
      } else if (!b.stakeSite.empty()) {
        v.push_back("THIS IS " + b.stakeSite + ". THIS IS WHERE IT ALL WENT WRONG FOR US.");
      }
      break;
    case Trig::StakeRuin:
      if (b.stakeRuin.empty()) break;
      v.push_back("THAT'S " + b.stakeRuin + ". SOMEWHERE IN THERE IS WHAT MY FAMILY LOST.");
      v.push_back("SO THERE IT IS, " + b.stakeRuin + ". MY GRANDMOTHER WOULD SPIT WHEN SHE SAID THAT NAME.");
      break;
    case Trig::Wonder:
      if (name.empty()) break;
      v.push_back("SO THAT'S " + name + ". THE OLD STORIES DIDN'T DO IT JUSTICE.");
      v.push_back("WAIT UNTIL I TELL THEM ABOUT " + name + " IN " + home + ". NOBODY WILL BELIEVE ME.");
      if (m.p.pious > 0) v.push_back("ONLY " + godOr(b) + " COULD HAVE MADE SOMETHING LIKE " + name + ".");
      break;
    case Trig::HardFight:
      if (arg == 1) {
        v.push_back("YOU GOT ME BACK ON MY FEET. I WON'T FORGET THAT.");
        v.push_back("I THOUGHT THAT WAS THE END OF ME. THANK YOU FOR NOT LEAVING ME THERE.");
      } else {
        v.push_back("THAT WAS TOO CLOSE. I THOUGHT I'D LOST YOU FOR A MOMENT.");
        if (m.p.brave > 0) v.push_back("NOW THAT WAS A FIGHT! MY HEART'S STILL POUNDING.");
        if (m.voc == Vocation::Healer) v.push_back("HOLD STILL AND LET ME LOOK AT THOSE CUTS BEFORE WE GO ON.");
        if (m.p.brave < 0) v.push_back("MY HANDS ARE STILL SHAKING. GIVE ME A MOMENT.");
        v.push_back("WE WON, BUT ONLY JUST. LET'S BE SMARTER NEXT TIME.");
      }
      break;
    case Trig::LowHealth:
      v.push_back("YOU'RE BLEEDING BADLY! DRINK SOMETHING, QUICKLY!");
      v.push_back("GET BACK, YOU'RE HURT! I'LL HOLD THEM!");
      if (m.voc == Vocation::Healer) v.push_back("STAY ON YOUR FEET! I'M COMING TO YOU!");
      break;
    case Trig::War: {
      if (b.homeland.empty() || (ew::Gid)arg == b.homelandId) break;
      const std::string other = arg ? story::kingdomName(g, (ew::Gid)arg) : std::string("ITS NEIGHBOURS");
      v.push_back("WORD ON THE ROAD IS THAT " + b.homeland + " IS AT WAR WITH " + other + ". MY PEOPLE ARE RIGHT IN THE WAY OF IT.");
      const Kin* k = closestKin(b);
      if (k && (k->word == "SON" || k->word == "HUSBAND" || k->word == "BROTHER" || k->word == "FATHER"))
        v.push_back(b.homeland + " IS AT WAR. MY " + k->word + " " + k->name + " WILL BE CALLED TO THE LEVY. I KNOW IT.");
      if (m.stake.hook == Hook::War) v.push_back(b.homeland + " AT WAR AGAIN. I SWORE I'D NEVER MARCH AGAIN. I MAY NOT HAVE A CHOICE.");
      break;
    }
    case Trig::Praise:
      v.push_back("I'VE BEEN THINKING. I'M GLAD I CAME WITH YOU, MORE THAN I EXPECTED TO BE.");
      v.push_back("YOU KNOW, WHEN I LEFT " + home + " I DIDN'T THINK I'D FIND ANYONE WORTH FOLLOWING. I WAS WRONG.");
      break;
    case Trig::Trust:
      v.push_back("I TRUST YOU NOW. I DON'T SAY THAT LIGHTLY, AND I DON'T SAY IT OFTEN.");
      v.push_back("WHATEVER COMES, I'VE GOT YOUR BACK. I THOUGHT YOU SHOULD HEAR ME SAY IT.");
      break;
    case Trig::Rain: case Trig::Snow: case Trig::Heat: case Trig::Sand: case Trig::Fog: case Trig::Ash: case Trig::Eerie:
      v = weatherLines(b, m, t);
      break;
    case Trig::Night:
      v.push_back("THE DARK COMES DOWN FAST OUT HERE. STAY CLOSE, AND KEEP YOUR BLADE LOOSE.");
      if (m.p.brave > 0) v.push_back("A FINE NIGHT FOR WALKING. THE WOLVES CAN COME IF THEY LIKE.");
      if (m.p.pious > 0) v.push_back(godOr(b) + " KEEP US TILL MORNING. THE ROAD IS NO PLACE TO BE AFTER DARK.");
      v.push_back("WE COULD MAKE CAMP, YOU KNOW. MY FEET WOULD THANK YOU FOR IT.");
      v.push_back("LISTEN TO THAT. EVERY SOUND CARRIES AT NIGHT.");
      break;
    case Trig::Musing: {
      const Kin* k = closestKin(b);
      if (k) v.push_back("I WONDER WHAT MY " + k->word + " " + k->name + " IS DOING RIGHT NOW. PROBABLY CURSING ME FOR LEAVING.");
      switch ((life::Job)b.job) {
        case life::Job::Smith: v.push_back("I MISS THE FORGE. THE ROAD HAS NO ANVIL, AND MY ARMS FORGET THE WEIGHT."); break;
        case life::Job::Baker: v.push_back("I'D GIVE A LOT FOR A LOAF STRAIGHT OUT OF MY OWN OVEN."); break;
        case life::Job::Farmer: case life::Job::Herder: v.push_back("THE HARVEST WILL BE IN AT HOME BY NOW. I HOPE THEY MANAGED WITHOUT ME."); break;
        case life::Job::Fisher: v.push_back("I MISS THE SOUND OF THE WATER UNDER A BOAT. THE ROAD IS TOO QUIET."); break;
        case life::Job::Hunter: v.push_back("FRESH TRACKS HERE. DEER, AND NOT LONG AGO. IF WE WEREN'T BUSY I'D FOLLOW THEM."); break;
        case life::Job::Bard: v.push_back("I'M MAKING A SONG ABOUT YOU. DON'T WORRY, YOU COME OUT OF IT WELL. MOSTLY."); break;
        case life::Job::Priest: v.push_back("I SAY A PRAYER FOR US EVERY MORNING. I THOUGHT YOU SHOULD KNOW."); break;
        case life::Job::Innkeeper: case life::Job::Server: v.push_back("I KEEP LOOKING AT EVERY INN WE PASS AND COUNTING WHAT THEY'RE DOING WRONG."); break;
        case life::Job::Merchant: v.push_back("YOU KNOW YOU'RE OVERPAYING FOR ARROWS? NEXT TIME, LET ME DO THE TALKING."); break;
        default: break;
      }
      if (!b.people.empty()) v.push_back("THE " + b.people + " HAVE A SAYING: A ROAD SHARED IS HALF AS LONG. I NEVER BELIEVED IT UNTIL NOW.");
      if (m.stake.hook != Hook::None) v.push_back("SOME DAYS I THINK ABOUT WHY I REALLY CAME. THEN I THINK ABOUT SOMETHING ELSE.");
      break;
    }
    case Trig::Hound: {
      const std::string hn = name.empty() ? std::string("THAT HOUND") : name;
      v.push_back("GOOD DOG, " + hn + ". YOU'RE THE ONLY ONE HERE WHO NEVER COMPLAINS.");
      v.push_back(hn + " HAS BEEN EYEING MY SUPPER SINCE NOON. DON'T THINK I HAVEN'T NOTICED.");
      v.push_back(hn + " CAUGHT A RABBIT AGAIN. AT LEAST ONE OF US EATS WELL TONIGHT.");
      break;
    }
    default: break;
  }
  return choose(g, v, h);
}

bool exchange(const Game& g, int ai, int bi, uint32_t salt, std::string& la, std::string& lb) {
  la.clear(); lb.clear();
  if (ai < 0 || bi < 0 || ai == bi || ai >= (int)g.party.members.size() || bi >= (int)g.party.members.size()) return false;
  const Member& A = g.party.members[(size_t)ai];
  const Member& B = g.party.members[(size_t)bi];
  if (A.voc == Vocation::Hound || B.voc == Vocation::Hound) return false;
  const Bio a = bio(g, A), b = bio(g, B);
  const std::string bn = voice::given(B.name), an = voice::given(A.name);
  struct Pair { std::string a, b; };
  std::vector<Pair> v;
  // their peoples (only where they differ)
  if (!sameFamily(A.culture, B.culture) && !b.people.empty()) {
    if (a.drink != b.drink)
      v.push_back({bn + ", IS IT TRUE THE " + b.people + " DRINK " + voice::drinkWord(b.drink) + " WITH THEIR BREAKFAST?",
                   B.p.kind >= 0 ? "ONLY ON GOOD DAYS, " + an + ". YOU SHOULD TRY IT SOMETIME." : "BETTER THAN THE SWILL YOUR PEOPLE DRINK, " + an + "."});
    if (a.arch != b.arch)
      v.push_back({bn + ", DID YOU REALLY GROW UP IN ONE OF THOSE " + voice::houseWords(b.arch) + "?",
                   "I DID. AND I SLEPT BETTER THERE THAN I EVER HAVE ON THIS ROAD."});
    if (!b.god.empty() && a.god != b.god)
      v.push_back({bn + ", WHO DO YOU PRAY TO WHEN THINGS GO WRONG?", "TO " + b.god + ". AND WHEN THINGS GO RIGHT, I THANK " + b.god + " TOO."});
  }
  // their natures
  if (A.p.brave - B.p.brave >= 2) v.push_back({bn + ", YOU HANG BACK IN EVERY FIGHT.", "SOMEONE HAS TO BE ALIVE AFTERWARDS TO DRAG YOU OUT, " + an + "."});
  if (A.p.pious - B.p.pious >= 2) v.push_back({bn + ", YOU COULD SAY A PRAYER NOW AND THEN. IT WOULDN'T KILL YOU.", "NEITHER WOULD A LITTLE QUIET, " + an + "."});
  if (A.p.lawful - B.p.lawful >= 2) v.push_back({bn + ", I SAW YOU POCKET THAT SPOON AT THE LAST INN.", "IT WAS A VERY GOOD SPOON."});
  if (A.p.kind - B.p.kind >= 2) v.push_back({bn + ", YOU COULD BE GENTLER WITH THE FOLK WE MEET.", "AND YOU COULD BE LESS SOFT, " + an + ". WE'LL SEE WHICH OF US LIVES LONGER."});
  if (std::abs(A.p.kind - B.p.kind) <= 1 && std::abs(A.p.brave - B.p.brave) <= 1)
    v.push_back({bn + ", I'M GLAD YOU'RE WITH US. I SLEEP BETTER KNOWING YOU'RE ON WATCH.", "AND I'M GLAD IT'S YOU AT MY BACK, " + an + "."});
  // their trades
  if (!b.trade.empty() && !b.home.empty())
    v.push_back({bn + ", WHAT DID YOU DO BEFORE ALL THIS?", "I WAS " + voice::article(b.trade) + " IN " + b.home + ". HARDER WORK THAN THIS, SOME DAYS."});
  if (B.kills > A.kills + 4) v.push_back({bn + ", YOU'RE KEEPING COUNT OF YOUR KILLS, AREN'T YOU?", "SOMEBODY HAS TO. YOU'RE FALLING BEHIND, " + an + "."});
  if (v.empty()) return false;
  const uint32_t h = mixh(A.id * 977u + B.id, salt ^ (uint32_t)g.day * 2654435761u);
  for (size_t k = 0; k < v.size(); k++) {
    const Pair& p = v[(h + k) % v.size()];
    if (p.a.size() > BANTER_MAX || p.b.size() > BANTER_MAX) continue;
    if (p.a == g.party.lastBanter || g.talkMem.saidRecently(speech::lineHash(p.a), g.day, 1)) continue;
    la = p.a; lb = p.b;
    return true;
  }
  return false;
}

}  // namespace voice

// ================================================================ banterStep
void banterStep(Game& g, float dt) {
  BanterState& S = st();
  // (a new Game of the same seed is often built where the last one stood: the address alone kept the old game's state,
  // and two plays of the same seed talked differently)
  if (S.g != &g || S.seed != g.seed || g.time < S.lastTime) { S = BanterState(); S.g = &g; S.seed = g.seed; }
  S.lastTime = g.time;
  if (g.mode != Mode::Play || g.travelling()) return;
  Party& P = g.party;
  // who can talk (people with the player), and the animal
  std::vector<int> people;
  int hound = -1;
  for (size_t i = 0; i < P.members.size(); i++) {
    const Member& m = P.members[i];
    if (m.state != State::With) continue;
    if (m.voc == Vocation::Hound) hound = (int)i; else people.push_back((int)i);
  }
  if (people.empty()) return;
  // an exchange's answer comes a few seconds after the line it answers
  if (S.replyBy >= 0) {
    S.replyT -= dt;
    if (S.replyT <= 0) {
      if (S.replyBy < (int)P.members.size() && P.members[(size_t)S.replyBy].state == State::With) g.partySay(S.replyBy, S.reply);
      S.replyBy = -1;
    }
    return;
  }
  const uint32_t salt = (uint32_t)(g.time * 7.0f) ^ (uint32_t)P.stats.banter * 0x9E37u;
  auto say = [&](int mi, Trig t, const std::string& line) {
    if (line.empty()) return false;
    g.partySay(mi, line);
    g_counts[(int)t]++;
    return true;
  };
  // the speaker for a trigger: the one it matters to most, else the first who has a line for it
  auto speak = [&](Trig t, uint64_t arg, const std::string& name, const std::vector<int>& who) {
    for (int mi : who) {
      const std::string l = voice::banterLine(g, mi, t, arg, name, salt);
      if (!l.empty()) return say(mi, t, l);
    }
    return false;
  };
  const Actor& pl = g.pl();
  // ---- the fight: tracked every step (its end is a trigger)
  {
    bool engaged = false;
    for (size_t k = 1; k < g.actors.size() && !engaged; k++) {
      const Actor& e = g.actors[k];
      if (e.hostile && e.aggro && e.st != AState::Dead && len2(e.p - pl.p) < (14.0f * TILE) * (14.0f * TILE)) engaged = true;
    }
    const float frac = pl.maxHp > 0 ? pl.hp / pl.maxHp : 1.0f;
    if (engaged) {
      if (!S.fighting) { S.fighting = true; S.minPl = 1; S.fightDowned = -1; }
      S.calm = 0;
      S.minPl = std::min(S.minPl, frac);
      for (size_t i = 0; i < P.members.size(); i++) if (P.members[i].state == State::Downed) S.fightDowned = (int)i;
      // the player badly hurt: a warning (exempt from the gap; its own cool-down)
      S.lowT -= dt;
      if (frac < 0.3f && S.lowT <= 0) {
        S.lowT = 45.0f;
        speak(Trig::LowHealth, 0, "", people);
      }
      return;   // (no chatter in a fight)
    }
    if (S.fighting) {
      S.calm += dt;
      if (S.calm < 4.0f) return;
      S.fighting = false;
      const bool hard = S.minPl < 0.5f || S.fightDowned >= 0;
      if (hard && P.banterT >= 20.0f) {
        if (S.fightDowned >= 0 && S.fightDowned < (int)P.members.size() && P.members[(size_t)S.fightDowned].state == State::With &&
            P.members[(size_t)S.fightDowned].voc != Vocation::Hound) {
          if (say(S.fightDowned, Trig::HardFight, voice::banterLine(g, S.fightDowned, Trig::HardFight, 1, "", salt))) return;
        }
        if (speak(Trig::HardFight, 0, "", people)) return;
      }
    }
  }
  // ---- a wonder found this step (Ev::Discover f 2) waits a minute for its line
  for (const Event& e : g.events) if (e.type == Ev::Discover && e.f >= 2.0f && !e.s.empty()) { S.wonder = e.s; S.wonderT = 60.0f; }
  if (!S.wonder.empty()) {
    S.wonderT -= dt;
    if (S.wonderT <= 0) S.wonder.clear();
    else if (P.banterT >= 8.0f) {
      const std::string w = S.wonder;
      S.wonder.clear();
      if (speak(Trig::Wonder, 0, w, people)) return;
    }
  }
  // ---- places: the land's people, home, the stake's places (looked at once a second)
  S.landT -= dt;
  bool placeTurn = false;
  if (S.landT <= 0 && !g.inside && g.world.src) {
    S.landT = 1.0f;
    placeTurn = true;
  }
  if (P.banterT < BANTER_GAP_SECS) {
    // (a border is remembered as crossed even while the gap runs, so the line comes the first time the gap allows)
    return;
  }
  if (placeTurn) {
    const int32_t gx = g.world.ox + (int32_t)std::floor(pl.p.x / TILE), gy = g.world.oy + (int32_t)std::floor(pl.p.y / TILE);
    const uint64_t land = g.world.src->cultureAt(gx, gy);
    const bool newLand = land && !sameFamily(land, S.land);
    const uint64_t prevLand = S.land;
    if (land) S.land = land;
    if (newLand && prevLand) {
      // into the land of another people: the one whose own people it is not, who has not remarked on them before
      for (int mi : people) {
        Member& m = P.members[(size_t)mi];
        if (sameFamily(m.culture, land)) {
          if (!sameFamily(prevLand, m.culture) && say(mi, Trig::BorderHome, voice::banterLine(g, mi, Trig::BorderHome, 0, "", salt))) return;
          continue;
        }
        const uint32_t bit = voice::TB_BORDER0 << (hash32((uint32_t)(cult::familyOf(land) >> 32) ^ (uint32_t)cult::familyOf(land)) % 16u);
        if (m.talkBits & bit) continue;
        const std::string l = voice::banterLine(g, mi, Trig::Border, land, "", salt);
        if (l.empty()) continue;
        m.talkBits |= bit;
        if (say(mi, Trig::Border, l)) return;
      }
    }
    // home, the stake's site and ruin
    const int si = g.curSite;
    if (si != S.lastSite) {
      const int was = S.lastSite;
      S.lastSite = si;
      if (si >= 0 && si < (int)g.world.sites.size() && was != -2) {
        const ew::Gid sid = g.world.sites[(size_t)si].id;
        for (int mi : people) {
          const Member& m = P.members[(size_t)mi];
          if (m.site == sid && say(mi, Trig::Home, voice::banterLine(g, mi, Trig::Home, 0, "", salt))) return;
          if (m.stake.site == sid && m.stake.site != m.site && say(mi, Trig::StakeSite, voice::banterLine(g, mi, Trig::StakeSite, 0, "", salt))) return;
        }
      }
    }
    for (int mi : people) {
      Member& m = P.members[(size_t)mi];
      if (!m.stake.ruin) continue;
      const int rh = g.world.siteHandle(m.stake.ruin);
      if (rh < 0) continue;
      const Site& R = g.world.sites[(size_t)rh];
      const int64_t dx = (int64_t)(g.world.ox + R.ex) - gx, dy = (int64_t)(g.world.oy + R.ey) - gy;
      if (dx * dx + dy * dy < 40 * 40 && !(m.talkBits & voice::TB_RUIN)) {
        m.talkBits |= voice::TB_RUIN;
        if (say(mi, Trig::StakeRuin, voice::banterLine(g, mi, Trig::StakeRuin, 0, "", salt))) return;
      }
    }
  }
  // ---- their homeland's war (each war remarked once: talkBits TB_WAR0.. by the war's id)
  for (int mi : people) {
    Member& m = P.members[(size_t)mi];
    const Bio b = voice::bio(g, m);
    if (!b.homelandId) continue;
    for (const realm::War& w : g.realm.wars()) {
      if (w.endDay || (w.attacker != b.homelandId && w.defender != b.homelandId)) continue;
      const uint32_t bit = voice::TB_WAR0 << (w.id % 3u);
      if (m.talkBits & bit) continue;
      m.talkBits |= bit;
      const ew::Gid other = w.attacker == b.homelandId ? w.defender : w.attacker;
      if (say(mi, Trig::War, voice::banterLine(g, mi, Trig::War, other, "", salt))) return;
    }
  }
  // ---- approval and trust (said once each; said again only after they fell well back)
  for (int mi : people) {
    Member& m = P.members[(size_t)mi];
    if (m.approval < 30) m.talkBits &= ~voice::TB_PRAISE;
    if (m.approval >= APPROVAL_QUEST && !(m.talkBits & voice::TB_PRAISE)) {
      m.talkBits |= voice::TB_PRAISE;
      if (say(mi, Trig::Praise, voice::banterLine(g, mi, Trig::Praise, 0, "", salt))) return;
    }
    if (m.bond >= SKILL_BOND[1] && !(m.talkBits & voice::TB_TRUST)) {
      m.talkBits |= voice::TB_TRUST;
      if (say(mi, Trig::Trust, voice::banterLine(g, mi, Trig::Trust, 0, "", salt))) return;
    }
  }
  // ---- the weather on screen (once per three-hour slot)
  {
    const Trig w = voice::weatherNow(g);
    const int slot = g.day * 8 + (int)(g.hour / 3);
    if (w != Trig::COUNT && slot != S.weatherSlot) {
      S.weatherSlot = slot;
      if (speak(w, 0, "", people)) return;
    }
  }
  // ---- nightfall on the road (once a night)
  if (g.isNight() && g.curSite < 0 && !g.inside && S.nightDay != g.day) {
    S.nightDay = g.day;
    if (speak(Trig::Night, 0, "", people)) return;
  }
  // ---- each other: two people with the player, or the hound
  S.musingT += dt;
  if (S.musingT < BANTER_GAP_SECS * 2) return;
  S.musingT = 0;
  if (people.size() >= 2) {
    const size_t ia = (size_t)(salt % people.size());
    const int a = people[ia];
    const int b = people[(ia + 1) % people.size()];
    std::string la, lb;
    if (a != b && voice::exchange(g, a, b, salt, la, lb)) {
      g.partySay(a, la);
      g_counts[(int)Trig::Pair]++;
      S.replyBy = b; S.reply = lb; S.replyT = 3.5f;
      return;
    }
  }
  if (hound >= 0 && (salt & 3u) == 0) {
    const int mi = people[(size_t)(salt % people.size())];
    if (say(mi, Trig::Hound, voice::banterLine(g, mi, Trig::Hound, 0, voice::given(P.members[(size_t)hound].name), salt))) return;
  }
  speak(Trig::Musing, 0, "", std::vector<int>{people[(size_t)(salt % people.size())]});
}

// ================================================================ deeds
namespace {
struct DeedWords { const char* good[2]; const char* bad[2]; };
// (Deed order) what a companion says when a deed moves them: approving, disapproving
const DeedWords kDeed[(int)Deed::COUNT] = {
    {{"YOU DIDN'T HAVE TO HELP THEM. I'M GLAD YOU DID.", "THAT'S ONE MORE FAMILY WHO'LL SLEEP EASIER TONIGHT."},
     {"ALL THAT TROUBLE FOR STRANGERS? I HOPE THE PAY WAS WORTH IT.", "WE COULD HAVE BEEN ON OUR WAY DAYS AGO."}},
    {{"SOME JOBS AREN'T WORTH FINISHING. I WON'T LOSE ANY SLEEP OVER IT.", "WELL, NOBODY CAN SAY WE DIDN'T TRY."},
     {"WE GAVE OUR WORD AND WE DIDN'T KEEP IT. THAT SITS BADLY WITH ME.", "SOMEONE WAS COUNTING ON US, AND WE LET THEM DOWN."}},
    {{"YOU BROUGHT THEM HOME ALIVE. I'LL REMEMBER THE LOOK ON THEIR FAMILY'S FACES.", "ALIVE AND HOME. THAT'S THE BEST DAY'S WORK I'VE SEEN IN YEARS."},
     {"WE RISKED OUR NECKS FOR ONE STRANGER. NEXT TIME, ASK ME FIRST.", "ALL THAT DANGER FOR ONE PERSON. I HOPE THEY'RE WORTH IT."}},
    {{"THOSE PEOPLE LOST EVERYTHING. YOU GAVE THEM A LITTLE BACK.", "THE WAR TOOK THEIR HOMES. YOU GAVE THEM A ROOF. THAT MATTERS."},
     {"WE CAN'T FEED EVERY FAMILY THE WAR SPITS OUT.", "THEY'LL EXPECT THE SAME FROM EVERY STRANGER NOW."}},
    {{"THAT WAS KIND. NOBODY WILL SING ABOUT IT, BUT IT WAS KIND.", "YOU GAVE WITHOUT BEING ASKED. I LIKE THAT ABOUT YOU."},
     {"THAT COIN WOULD HAVE BOUGHT US ARROWS.", "YOU CAN'T BUY THE WHOLE WORLD SUPPER, YOU KNOW."}},
    {{"YOU MAKE FRIENDS EASILY. I LIKE THAT ABOUT YOU.", "A FRIEND IN EVERY TOWN. THAT'S WORTH MORE THAN GOLD ON THE ROAD."},
     {"YOU TRUST PEOPLE TOO QUICKLY. ONE DAY IT WILL COST YOU.", "FRIENDS ARE EASY TO MAKE. THEY'RE HARDER TO KEEP."}},
    {{"YOU LET THEM LIVE. THAT TAKES MORE COURAGE THAN PEOPLE THINK.", "MERCY. I WASN'T SURE YOU HAD IT IN YOU. I'M GLAD YOU DO."},
     {"YOU LET THEM WALK AWAY. THEY'LL BE BACK, AND WE'LL PAY FOR IT.", "MERCY IS A FINE THING UNTIL THE ONE YOU SPARED COMES BACK WITH FRIENDS."}},
    {{"HARSH, BUT THEY WON'T TRY THAT AGAIN.", "SOMETIMES IT TAKES A HARD HAND. THEY'LL REMEMBER THIS."},
     {"THERE WAS NO NEED FOR THAT. NONE AT ALL.", "I DIDN'T COME ON THIS ROAD TO WATCH THAT."}},
    {{"THEY WERE IN THE WAY. I SUPPOSE THAT'S ONE WAY TO CLEAR A ROAD.", "WELL. NOBODY WILL CROSS US NOW."},
     {"THEY WEREN'T EVEN ARMED! WHAT HAVE YOU DONE?", "THAT WAS SOMEBODY'S CHILD. I'LL SEE IT EVERY TIME I CLOSE MY EYES."}},
    {{"ONE LESS BOOT ON THE NECKS OF HONEST FOLK.", "THE WATCH HAD IT COMING. THEY ALWAYS DO."},
     {"YOU KILLED A MAN OF THE WATCH. THEY'LL HANG US BOTH FOR THAT.", "THAT GUARD WAS ONLY DOING THE JOB. NOW THEY'RE DEAD, AND WE'RE OUTLAWS."}},
    {{"NEATLY DONE. NOBODY SAW A THING.", "THEY'LL NEVER MISS IT. AND IF THEY DO, WE'RE LONG GONE."},
     {"THAT WASN'T OURS TO TAKE.", "SO WE'RE THIEVES NOW. I DIDN'T SIGN ON FOR THAT."}},
    {{"THE ROAD'S SAFER FOR THAT. GOOD.", "THOSE BANDITS WON'T BOTHER ANYONE AGAIN."},
     {"THEY WERE DESPERATE, NOT WICKED. SOME OF THEM, ANYWAY.", "HALF OF THEM WERE HUNGRY FARMERS WITH SPEARS. I TAKE NO JOY IN IT."}},
    {{"THAT THING WON'T TAKE ANY MORE SHEEP, OR SHEPHERDS EITHER.", "THE FOLK HERE CAN SLEEP AGAIN. WELL FOUGHT."},
     {"IT WAS ONLY DEFENDING ITS OWN GROUND.", "WE WALKED INTO ITS HOME AND KILLED IT. THAT'S HOW I SEE IT."}},
    {{"THEY'LL SING ABOUT THIS ONE DAY. I WAS THERE, AND THEY'LL SING ABOUT IT!", "I NEVER THOUGHT I'D SEE ONE OF THOSE FALL. NOT IN MY LIFE."},
     {"WE NEARLY DIED FOR THAT. WAS IT WORTH IT?", "GLORY IS A POOR TRADE FOR A GRAVE."}},
    {{"THEN WE'RE SOLDIERS NOW. I CAN LIVE WITH THAT.", "A SIDE AT LAST. I NEVER COULD ABIDE SITTING ON A FENCE."},
     {"WAR IS NO GAME FOR STRANGERS LIKE US. WE SHOULD HAVE KEPT OUT OF IT.", "YOU'VE TAKEN A SIDE. I HOPE YOU KNOW WHAT IT WILL COST."}},
    {{"WE HELD THEM OFF. I'VE NEVER BEEN SO TIRED OR SO PROUD.", "THE TOWN STILL STANDS, AND WE'RE WHY. REMEMBER THAT."},
     {"THAT WASN'T OUR FIGHT, AND WE BLED FOR IT ANYWAY.", "WE COULD HAVE DIED FOR A TOWN THAT DOESN'T KNOW OUR NAMES."}},
    {{"I'VE ALWAYS WONDERED WHAT'S DOWN IN PLACES LIKE THIS.", "OLD STONES AND OLD SECRETS. NOW THIS IS AN ADVENTURE."},
     {"THE DEAD SHOULD BE LEFT ALONE. I DON'T LIKE THIS PLACE.", "NOTHING GOOD EVER CAME UP OUT OF A HOLE LIKE THIS."}},
    {{"NOW THIS IS MORE LIKE IT. A WARM FIRE AND SOMETHING IN MY CUP.", "A GOOD MEAL AND A SOFT BENCH. I'D FOLLOW YOU ANYWHERE FOR THIS."},
     {"WE'VE BETTER THINGS TO SPEND OUR COIN ON THAN DRINK.", "ANOTHER INN. ANOTHER EVENING WASTED."}},
    {{"IT DOES ME GOOD TO SEE YOU PRAY.", "THE GODS LISTEN TO THOSE WHO ASK. I'M GLAD YOU ASKED."},
     {"PRAYERS DON'T SHARPEN BLADES.", "ALL THAT KNEELING. THE GODS HAVE NEVER DONE A THING FOR ME."}},
    {{"A LITTLE GOLD SAVES A LOT OF BLOOD.", "PAID AND DONE. THAT'S HOW SENSIBLE PEOPLE SETTLE THINGS."},
     {"PAYING OFF CROOKS ONLY MAKES MORE OF THEM.", "SO THAT'S HOW IT IS. ANYTHING CAN BE BOUGHT, IF THE PRICE IS RIGHT."}},
    {{"A GOOD STORY. I NEARLY BELIEVED IT MYSELF.", "THEY'LL NEVER KNOW. SOMETIMES THE TRUTH ONLY GETS IN THE WAY."},
     {"THEY TRUSTED YOU, AND YOU LIED TO THEIR FACE.", "HOW DO I KNOW YOU DON'T LIE TO ME LIKE THAT?"}},
    {{"OATHS ARE FOR FOOLS AND PRIESTS.", "A PROMISE MADE IN A CORNER IS NOT WORTH KEEPING."},
     {"YOU GAVE YOUR WORD. WHAT IS YOUR WORD WORTH NOW?", "AN OATH IS AN OATH. WHERE I COME FROM, WE KEEP THEM."}},
    {{"THE DEAD DON'T NEED GOLD.", "IT WAS ONLY GATHERING DUST DOWN THERE."},
     {"ROBBING THE DEAD? HAVE YOU NO SHAME AT ALL?", "THAT WAS SOMEBODY'S GRAVE. I'LL PRAY THEY FORGIVE US."}},
    {{"", ""}, {"YOU LEFT ME THERE. YOU JUST LEFT ME.", "I WAS BLEEDING ON THE GROUND AND YOU RAN. I WON'T FORGET IT."}},
};
}  // namespace

std::string deedLine(const Game& g, const Member& m, Deed d, int delta) {
  if (m.voc == Vocation::Hound || (int)d < 0 || (int)d >= (int)Deed::COUNT || std::abs(delta) < 4) return std::string();
  const Bio b = voice::bio(g, m);
  const bool good = delta > 0;
  const uint32_t h = mixh(m.id * 0x51u + (uint32_t)d, (uint32_t)g.day ^ (uint32_t)std::abs(delta) * 13u);
  std::vector<std::string> v;
  for (const char* s : good ? kDeed[(int)d].good : kDeed[(int)d].bad) if (s && *s) v.push_back(s);
  // what it was about (the memory FELLOWS just added): a war against their own people, a deed in their home town
  ew::Gid about = 0;
  if (!m.mem.empty() && m.mem.back().deed == d && m.mem.back().day == g.day) about = m.mem.back().about;
  if (d == Deed::JoinedWar && !good && about && about == b.homelandId)
    v = {"YOU'VE TAKEN UP ARMS AGAINST MY OWN PEOPLE. AGAINST " + b.homeland + ". HOW COULD YOU?",
         "THOSE ARE MY KIN YOU'VE SWORN TO FIGHT. I DON'T KNOW IF I CAN STAND BESIDE YOU NOW."};
  if ((d == Deed::KilledInnocent || d == Deed::Stole || d == Deed::KilledGuard) && !good && about && about == m.site && !b.home.empty())
    v = {"THAT WAS " + b.home + ". THAT WAS MY HOME, AND MY PEOPLE. HOW COULD YOU DO THAT THERE?"};
  if (d == Deed::DefendedTown && good && about && about == m.site && !b.home.empty())
    v = {"YOU SAVED " + b.home + ". MY HOME. I'LL NEVER BE ABLE TO REPAY THAT."};
  std::string line = choose(g, v, h);
  // a strong feeling about a moral deed, said in their people's way: the god's name for the devout
  const bool moral = d == Deed::Mercy || d == Deed::Cruelty || d == Deed::KilledInnocent || d == Deed::Charity || d == Deed::Rescued ||
                     d == Deed::HelpedRefugees || d == Deed::Lied || d == Deed::Oathbreak || d == Deed::DefiledGrave || d == Deed::Stole;
  if (!line.empty() && moral && std::abs(delta) >= 9 && m.p.pious > 0 && !b.god.empty()) {
    const std::string tail = good ? " " + b.god + " SAW THAT, AND SMILED." : " MAY " + b.god + " FORGIVE US BOTH.";
    if (speech::wrapLines(line + tail, speech::PANEL_CHARS).size() <= 2) line += tail;
  }
  return line;
}

// ================================================================ cries
namespace {
const char* const kSkillCry[(int)Skill::COUNT] = {
    "I'VE GOT THE KNACK OF IT NOW. WHEN THEY COME, I'LL DRAW THEM ALL ONTO ME.",
    "STAND BEHIND MY SHIELD WHEN IT GETS BAD. I'VE LEARNED TO HOLD A WALL ALONE.",
    "IF I FALL, I'LL GET UP ONCE MORE. I PROMISE YOU THAT.",
    "I CAN PIN THEM IN PLACE NOW, ONE SHOT TO THE LEG. WATCH FOR IT.",
    "GIVE ME A CROWD OF THEM AND A MOMENT, AND I'LL RAIN ARROWS ON THE LOT.",
    "MY EYE HAS NEVER BEEN SHARPER. I COULD SPLIT A REED AT A HUNDRED PACES.",
    "THE FIRE COMES WHEN I CALL IT NOW. STAND CLEAR WHEN IT DOES.",
    "I CAN PUT A WARD ON YOU. IT WON'T STOP EVERYTHING, BUT IT HELPS.",
    "I'VE FOUND SOMETHING BIGGER IN ME. A STORM. KEEP YOUR HEAD DOWN WHEN I LET IT OUT.",
    "MY HANDS KNOW YOUR WOUNDS NOW. I CAN CLOSE THEM FASTER.",
    "BURNS, POISON, SLOWNESS IN THE BLOOD. I CAN DRAW THEM ALL OUT OF YOU NOW.",
    "IF ONE OF US GOES DOWN, I CAN REACH THEM FROM ACROSS THE FIELD. TRUST ME.",
    "I'LL GET BEHIND THEM WHILE YOU KEEP THEIR EYES. THEY'LL NEVER SEE ME.",
    "A LITTLE SMOKE AND I'M GONE. HANDY, WHEN THINGS GO BADLY.",
    "WHEN ONE OF THEM IS HURT AND SLOW, LEAVE IT TO ME. ONE CUT IS ALL I NEED.",
    "", "", ""};
}  // namespace

std::string cryLine(const Game& g, const Member& m, int kind) {
  if (m.voc == Vocation::Hound) return std::string();
  const Bio b = voice::bio(g, m);
  const uint32_t h = mixh(m.id * 0x2Bu + (uint32_t)kind, (uint32_t)g.day * 31u + m.downs * 7u + m.revives);
  std::vector<std::string> v;
  switch (kind) {
    case 1:
      v = {"I'M HURT! I CAN'T TAKE MUCH MORE OF THIS!", "I'M BLEEDING! COVER ME, I NEED A MOMENT!"};
      if (m.p.brave > 0) v.push_back("THEY'VE CUT ME, BUT I'M STILL STANDING! KEEP FIGHTING!");
      if (m.p.brave < 0) v.push_back("I CAN'T DO THIS MUCH LONGER! HELP ME!");
      break;
    case 2:
      v = {"I'M DOWN! HELP ME!", "I CAN'T GET UP! DON'T LEAVE ME HERE!"};
      if (!b.home.empty()) v.push_back("I'M DOWN! I DON'T WANT TO DIE SO FAR FROM " + b.home + "!");
      break;
    case 3:
      v = {"THANK YOU. I THOUGHT THAT WAS THE END OF ME.", "YOU CAME BACK FOR ME. I WON'T FORGET IT."};
      if (m.p.pious > 0) v.push_back(godOr(b) + " BE THANKED, AND YOU WITH THEM. I'M ALL RIGHT.");
      if (m.p.brave > 0) v.push_back("I'M UP! LET'S FINISH THIS!");
      break;
    case 5:
      switch (m.origin) {
        case Origin::Mercenary: v = {"PAID BY THE DAY, AND I FIGHT LIKE IT. LEAD ON.", "YOUR COIN, MY BLADE. LET'S GO."}; break;
        case Origin::Rescued: v = {"YOU CAME INTO THE DARK FOR ME. THE LEAST I CAN DO IS WALK BESIDE YOU.", "I OWE YOU MY LIFE. I MEAN TO PAY IT BACK."}; break;
        case Origin::Saga: v = {"I'VE SWORN TO SEE THIS THROUGH WITH YOU. LEAD THE WAY.", "WHERE YOU GO, I GO. THAT WAS THE OATH."}; break;
        case Origin::Animal: break;
        default:
          v = {"I'LL FETCH MY PACK. LEAD THE WAY.", "RIGHT. LET'S SEE WHAT'S OUT THERE."};
          if (!b.home.empty()) v.push_back("GOODBYE, " + b.home + ". I'LL BE BACK WITH STORIES.");
          break;
      }
      break;
    case 6:
      v = {"I'M DONE WITH YOU. I'M GOING HOME.", "I CAN'T WALK BESIDE YOU ANY MORE. DON'T FOLLOW ME."};
      if (!b.home.empty()) v.push_back("I'M GOING BACK TO " + b.home + ". IF YOU WANT ME BACK, YOU KNOW WHAT YOU OWE ME.");
      break;
    case 7:
      v = {"I DON'T LIKE WHAT YOU'RE TURNING INTO.", "THIS ISN'T WHAT I LEFT HOME FOR. SOMETHING HAS TO CHANGE."};
      if (m.p.lawful > 0) v.push_back("I KEEP TO THE LAW. LATELY YOU DON'T, AND IT'S WEARING ON ME.");
      if (m.p.kind > 0) v.push_back("YOU USED TO BE KINDER. WHAT HAPPENED TO YOU?");
      break;
    case 8: {
      const uint8_t un = unlockedSkills(m);
      int top = -1;
      for (int k = 2; k >= 0; k--) if (un & (1u << k)) { top = k; break; }
      if (top >= 0) {
        const Skill s = skillsOf(m.voc)[(size_t)top];
        if ((int)s < (int)Skill::COUNT && kSkillCry[(int)s][0]) v.push_back(kSkillCry[(int)s]);
      }
      if (v.empty()) v.push_back("I'VE LEARNED SOMETHING ON THIS ROAD. NEXT FIGHT, WATCH ME.");
      break;
    }
    default: break;
  }
  if (v.empty()) return std::string();
  return v[(size_t)(h % v.size())];
}

// ================================================================ the grave
std::string epitaph(const Game& g, const Fallen& f) {
  int burial = 0;
  std::string god, home;
  if (f.culture && g.world.src) {
    const cult::Culture& C = g.world.src->culture(f.culture);
    burial = C.faith.burial;
    if (!C.faith.names.empty()) god = C.faith.names[0];
  }
  if (f.site) {
    const int h = g.world.siteHandle(f.site);
    if (h >= 0) home = g.world.sites[(size_t)h].name;
  }
  const std::string who = home.empty() ? f.name : f.name + " OF " + home;
  const std::string span = f.days < 7 ? "A FEW DAYS" : f.days < 30 ? "MANY DAYS" : f.days < 90 ? "A SEASON" : "A YEAR AND MORE";
  const uint32_t h = hash32(f.seed ^ 0xE91Au);
  std::string mid;
  switch (h % 3) {
    case 0: mid = "WHO WALKED " + span + " AT A FRIEND'S SIDE"; break;
    case 1: mid = f.kills >= 10 ? std::string("WHO STOOD IN THE LINE AND DID NOT BREAK") : std::string("WHO KEPT FAITH ON A HARD ROAD"); break;
    default: mid = "BELOVED, BRAVE, AND GONE TOO SOON"; break;
  }
  // (the last line ends the sentence the stone tells: the grave's dialogue shows the lines one under another)
  switch (burial) {
    case 1: return "A STONE FOR " + who + " / " + mid + " / LET THE CAIRN STAND WHILE THE HILLS STAND.";
    case 2: return who + " / " + mid + " / WENT UP IN FIRE, AND WAS NOT AFRAID.";
    case 3: return who + " / " + mid + " / GIVEN TO THE WIND AND THE HIGH BIRDS.";
    case 4: return who + " / " + mid + " / THE SEA KEEPS THEM NOW.";
    default: return "HERE LIES " + who + " / " + mid + (god.empty() ? std::string(" / AND DID NOT TURN BACK.") : " / " + god + " KEEP THEM.");
  }
}

// ================================================================ their own story
std::string aboutLine(const Game& g, const Member& m) {
  if (m.voc == Vocation::Hound) return std::string();
  const Bio b = voice::bio(g, m);
  std::vector<std::string> parts;
  if (m.npc && m.res >= 0) {
    parts.push_back(tradeSentence(b));
    parts.push_back(kinSentence(b));
    parts.push_back(stakeSentence(g, m, b));
  } else {
    const std::string land = b.people.empty() ? std::string("THESE LANDS") : std::string(voice::landWords(b.arch));
    switch (m.origin) {
      case Origin::Mercenary: parts.push_back("I'VE SOLD MY SWORD UP AND DOWN " + land + " SINCE I WAS SIXTEEN."); break;
      case Origin::Rescued: parts.push_back("YOU FOUND ME WHEN NOBODY ELSE WAS LOOKING, AND I DON'T FORGET A THING LIKE THAT."); break;
      case Origin::Saga: parts.push_back("I'VE SWORN TO SEE THIS ROAD THROUGH WITH YOU, WHEREVER IT GOES."); break;
      default: parts.push_back("I GREW UP IN " + land + ", AND I'VE BEEN ON THE ROAD EVER SINCE."); break;
    }
    parts.push_back(stakeSentence(g, m, b));
  }
  return joinAll(parts, (size_t)speech::PANEL_CHARS * speech::PANEL_LINES);
}

}  // namespace party

// (the shared word tables for the talk: rpg/sim/party_talk.cpp reads these through the VOICE header)
namespace party {
namespace voice {
std::string tradeLine(const Bio& b) { return tradeSentence(b); }
std::string kinLine(const Bio& b) { return kinSentence(b); }
std::string stakeLine(const Game& g, const Member& m, const Bio& b) { return stakeSentence(g, m, b); }
}  // namespace voice
}  // namespace party
