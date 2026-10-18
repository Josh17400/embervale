// P1b radiant job kind 4: THE SURVEY LINE (content: jobs_a.md section 4; judge notes resolved). JOBS lane.
//
// Walk three marks for a mapmaker (a mage's star-line, a lord's border marks, a merchant's road map, a priest's pilgrim
// way; a fisher's beacon-stone survey of the coast; a lord's line of the front in a war). Each stop's mark is READ as
// the player arrives (two lines on the notice strip); the second stop often brings a beast pack or a camp's scout; the
// third has a beat: OCCUPIED (a bandit lookout holds it: drive them off), THE FRESH STONE (a forged boundary or
// waypost, newly planted to cheat a claim), THE HERMIT'S JAR (at stop two, a hermit beside a carved doorpost hands over
// an empty honey jar for the beekeeper of the giver's town, whom the hermit names), or, for the mage's new star only,
// THE FALSE STAR (a goatherd's lantern on a pole). The giver checks the numbers against the marks on the report.
// Judge: the war pool has its third mark (the burned watch-post); every line fits the phone panel.
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/world.h"
#include "rpg/story/radiant.h"

namespace story {
namespace radiant {

namespace {

const char* const kBody = R"DSL(
title [[=title]]
role giver giver
role home site home
role stop1 site ruin near
role stop2 site cave near
role stop3 site ruin near
{?tw_hermit
role hermit person male at stop2
role keeper person [[male|female]] at home
}
{?tw_star
role goatherd person [[male|female]] at stop3
}

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[THE PAY IS GOOD FOR A WALK.|I PAY FOR PROOF, AND I PAY WELL.|YOUR FEET, MY COIN.]]"
  opt "[[I'LL WALK YOUR LINE.|THREE MARKS. DONE.|I'LL BRING YOU NUMBERS.]]" -> leg1
  opt "[[NOT TODAY.|FIND ANOTHER PAIR OF BOOTS.|I'M NO SURVEYOR.]]" -> decline

stage leg1
  goal goto stop1
  journal "WALK {GIVER}'S SURVEY LINE: THREE MARKS. THE FIRST IS AT {STOP1}, {STOP1.DIR}. READ IT."
  then leg2

stage leg2
  goal goto stop2
  journal "THE FIRST MARK IS READ. THE SECOND STANDS AT {STOP2}, {STOP2.DIR}."
  do notice "[[=m1a]]"
  do notice "[[=m1b]]"
{?tw_hermit
  then hermit_talk
}
{!tw_hermit
  then leg3
}

{?tw_hermit
stage hermit_talk
  talk hermit
  say "A CARVED DOORPOST STANDS ALONE, AND YOU SIT BESIDE IT AS IF IT WERE A HOUSE. ...THREE DAYS, THEN THE SEA. AND TAKE THIS EMPTY JAR TO {KEEPER}, WHO KEEPS MY BEES IN {HOME}."
  journal "A HERMIT AT {STOP2} GAVE YOU AN EMPTY HONEY JAR FOR {KEEPER} IN {HOME}."
  do give "AN EMPTY HONEY JAR" potion
  opt "[[I'LL TAKE IT TO {KEEPER}.|THREE DAYS TO THE SEA. NOTED.]]" -> leg3
}

stage leg3
  goal goto stop3
  journal "TWO MARKS READ. THE LAST STANDS AT {STOP3}, {STOP3.DIR}. [[=leg3tail]]"
{!tw_hermit
  do notice "[[=m2a]]"
  do notice "[[=m2b]]"
}
{?tw_hermit
  do notice "A CARVED DOORPOST, ALONE. THE HERMIT."
  do notice "'THREE DAYS, THEN THE SEA.'"
}
  do ambush [[=pack]] [[=packn]]
{?tw_occupied
  then hold
}
{?tw_star
  then star
}
{!tw_occupied|tw_star
  then mark3
}

{?tw_occupied
stage hold
  goal kill 2 bandit
  journal "A BANDIT LOOKOUT HOLDS THE LAST MARK AT {STOP3}. DRIVE THEM OFF AND READ IT."
  do ambush bandit 3
  then mark3
}

{?tw_star
stage star
  talk goatherd
  say "A BRASS LANTERN ON A POLE, SOOT INSIDE, A HUT BESIDE IT, AND YOU, YAWNING IN THE DOOR. ...WHAT? I LIGHT IT FOR THE LAMBS. THEY COME IN TO IT AT NIGHT. WHY?"
  journal "THE MAGE'S NEW STAR, AT {STOP3}, IS A GOATHERD'S LANTERN ON A POLE."
  opt "[[NO REASON. GOOD NIGHT.|A MAGE WILL BE VERY UPSET.]]" -> report
}

{!tw_star
stage mark3
  goal goto stop3
  journal "READ THE LAST MARK AT {STOP3}, THEN REPORT TO {GIVER} IN {HOME}."
  do notice "[[=m3a]]"
  do notice "[[=m3b]]"
{?tw_hermit
  then keeper_talk
}
{!tw_hermit
  then report
}
}

{?tw_hermit
stage keeper_talk
  talk keeper
  say "THE OLD MAN'S JAR AGAIN. HE ALWAYS SENDS IT BACK EMPTY, AND I ALWAYS FILL IT. HERE, ONE FOR YOU. AND THIS: THE OLD MAN SENDS HIS THANKS. HE ALWAYS DOES."
  journal "TAKE THE HERMIT'S JAR TO {KEEPER} IN {HOME}, THEN REPORT TO {GIVER}."
  do take "AN EMPTY HONEY JAR"
  do give "A JAR OF HONEY" potion
  do reward small 30
  do remember keeper "YOU BROUGHT THE OLD MAN'S JAR. HE LIKES YOU, THEN. HE LIKES NOBODY."
  opt "[[TELL HIM I SAID HELLO.|HE SEEMS WELL.]]" -> report
}

stage report
  talk giver
  say "[[=open]] [[=ret]] [[=reactline]]"
  journal "REPORT THE THREE MARKS TO {GIVER} IN {HOME}."
  opt "[[THREE MARKS, AS PROMISED.|HERE ARE YOUR NUMBERS.|THE LINE IS WALKED.]]" -> paid

stage paid
  end success
  say "[[TAKE YOUR FEE. YOU EARNED IT TWICE.|I'LL PAINT YOU INTO THE MARGIN. AND PAY YOU, OF COURSE.|THE LAND IS BIGGER THAN I THOUGHT. HERE.]]"
  journal "{GIVER}'S SURVEY LINE IS WALKED: {STOP1}, {STOP2} AND {STOP3}."
  do reward fair [[=pay]]
  do remember giver "[[=mem]]"
  do fact "A SURVEY LINE FROM {HOME} WAS WALKED: {STOP1}, {STOP2}, {STOP3}."
[[=payfx]]

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S SURVEY LINE."
)DSL";

enum { V_DEFAULT, V_BEACON, V_WAR, V_STAR };
enum { T_NONE, T_OCCUPIED, T_FRESH, T_HERMIT, T_STAR };

struct Mark { const char* a; const char* b; const char* ret; };
const Mark kMarks[] = {
    {"A CAIRN OF RIVER STONES, LEANING.", "A BRASS PIN SITS ON THE TOP.", "A BRASS PIN. A DEAD SURVEYOR'S, THAT."},
    {"THE WAYPOST LIES IN THE GRASS.", "ITS SIGN POINTS TO A DRY WELL.", "A DRY WELL. NOW I KNOW WHY THEY MOVED."},
    {"A DRY WALL RUNS INTO THE HILLS.", "SOMEONE MENDS IT, STONE BY STONE.", "A MENDED WALL. SOMEONE LIVES OUT THERE."},
    {"SIX CARVED RINGS, WORN SMOOTH.", "THE SIXTH POINTS BACK TO {HOME}.", "THE SIXTH RING POINTS HERE? THEN WE'RE OLDER THAN WE THOUGHT."},
    {"A MILESTONE STANDS IN THE BARLEY.", "THE ROAD IT SERVED IS FARMLAND NOW.", "A ROAD LOST TO BARLEY. I'LL DOT IT IN."},
    {"A PILLAR WITH A CROWN CUT IN IT.", "A KINGDOM NAMED ON IT, LONG GONE.", "A DEAD KING'S BORDER. IT IS OURS NOW."},
    // coastal (the beacon survey)
    {"A BELL HALF-BURIED IN THE REEDS.", "THE ROPE IS NEW. THE CLAPPER BRIGHT.", "A NEW ROPE ON A DROWNED BELL? SOMEONE RINGS IT."},
    {"AN IRON CRADLE ON THE HEADLAND.", "EMPTY. BUT THE ASH IN IT IS NEW.", "A FIRE LIT LATELY ON THE HEADLAND. WHO LIT IT?"},
    {"A POST WITH NOTCHES, LOW TO HIGH.", "THE SEA TOOK THE TOP THREE.", "A HIGHER TIDE THAN THE OLD MARKS. NOTE THE DATE."},
    // the war
    {"THE STONE BEARS THE OLD SEAL.", "SPLIT BY FROST DOWN THE MIDDLE.", "SPLIT, BUT STANDING. LAND IS LAND."},
    {"A WATCH-POST, BURNED TO THE KNEE.", "ARROWS IN THE CHAR, POINTING HOME.", "BURNED, AND ARROWS POINTING HOME? THEY'VE BEEN THAT CLOSE."},
};

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  std::vector<int> pool;
  if (c.variant == V_BEACON) pool = {6, 7, 8};
  else if (c.variant == V_WAR) pool = {9, 5, 10};
  else pool = {0, 1, 2, 3, 4, 5};
  // three marks, no repeats
  int m[3];
  for (int k = 0; k < 3; k++) {
    const size_t at = (size_t)d.below((int)pool.size());
    m[k] = pool[at];
    pool.erase(pool.begin() + (std::ptrdiff_t)at);
  }
  P["m1a"] = kMarks[m[0]].a; P["m1b"] = kMarks[m[0]].b;
  P["m2a"] = kMarks[m[1]].a; P["m2b"] = kMarks[m[1]].b;
  if (c.twist == T_FRESH) { P["m3a"] = "THE EARTH ROUND THE STONE IS LOOSE."; P["m3b"] = "THE MOSS IS GLUED ON. A FORGERY."; }
  else if (c.twist == T_STAR) { P["m3a"] = "A BRASS LANTERN ON A POLE. SOOT."; P["m3b"] = "A GOATHERD'S HUT. NO STAR AT ALL."; }
  else { P["m3a"] = kMarks[m[2]].a; P["m3b"] = kMarks[m[2]].b; }
  // the giver's return line: one of the marks read, weighted to stop two (the hermit's doorpost is stop two's mark)
  const int pickRet = c.twist == T_HERMIT ? -1 : (d.below(4) < 2 ? m[1] : d.chance(50) ? m[0] : (c.twist == T_FRESH || c.twist == T_STAR ? m[1] : m[2]));
  P["ret"] = pickRet < 0 ? "THREE DAYS TO THE SEA? THE HERMIT SAID SO? CHARTED." : kMarks[pickRet].ret;
  P["open"] = d.pick({"THREE POINTS. NOW THE MAP IS TRUE.", "LET ME CHECK YOUR NUMBERS AGAINST THE CHART.", "YOU'RE BACK. TELL ME, MARK BY MARK."});
  P["leg3tail"] = c.twist == T_OCCUPIED ? "SOMETHING MOVES UP THERE." : c.twist == T_STAR ? "THE MAGE'S STAR HANGS ABOVE IT AT NIGHT." : "";
  // stop two's encounter: a beast pack (wolves), or a camp's scout in a war
  P["pack"] = c.variant == V_WAR ? "bandit" : "wolf";
  P["packn"] = std::to_string(2 + (c.danger ? 1 : 0) + (c.tier >= 1 ? 1 : 0));
  std::string react, mem = "THE SURVEY LINE YOU WALKED IS ON MY WALL NOW.", fx;
  int pay = 100;
  if (c.twist == T_FRESH) {
    switch ((Role)c.trade) {
      case Role::Jarl: react = "A FORGED STONE? SOMEONE STEALS MY LAND BY PEBBLES."; break;
      case Role::Merchant: react = "A TOLL STONE, PLANTED TO STEER CARTS. CLEVER. I'LL HAVE IT OUT."; break;
      case Role::Priest: react = "A FALSE WAYPOST ON A PILGRIM'S ROAD? SHAMEFUL. IT COMES DOWN TOMORROW."; break;
      default: react = "A FAKE LANDMARK? MY CHARTS WERE WRONG BECAUSE OF A FORGER. I'LL FIND THEM."; break;
    }
    pay = 110;
    fx = "  do rumour";
    mem = "SOMEONE IS MOVING STONES IN THE HILLS. YOU FOUND THE FIRST ONE.";
  } else if (c.twist == T_OCCUPIED) {
    react = "BANDITS SITTING ON MY LAST MARK? AND YOU READ IT ANYWAY. THAT'S WORTH MORE.";
    pay = 125;
  } else if (c.twist == T_STAR) {
    react = "MY LIFE'S WORK. A GOATHERD'S LAMP. ...THE OTHER TWO POINTS WERE TRUE, AT LEAST. I'LL LIVE.";
    mem = "A GOATHERD'S LAMP. I STILL LOOK AT IT SOME NIGHTS. IT'S A VERY GOOD LAMP.";
  } else if (c.variant == V_BEACON) react = "THREE STONES STILL STAND ON THE COAST. THE BOATS WILL KNOW THEIR WAY.";
  else if (c.variant == V_WAR) react = "NOW I KNOW WHERE THE ENEMY'S STONES STAND. AND HOW CLOSE.";
  P["reactline"] = react;
  P["mem"] = mem;
  P["pay"] = std::to_string(pay);
  P["payfx"] = fx;
  // the reason and the title
  std::vector<std::string> R, T = {"THE SURVEY LINE", "THREE MARKS", "WALK THE BOUNDS", "A LINE ON THE MAP", "THE LAND, MEASURED"};
  const std::string first = "{STOP1}";
  if (c.variant == V_STAR) {
    R = {"A NEW STAR HANGS LOW OVER THE HILLS BEYOND {STOP3}. I SEE IT EVERY CLEAR NIGHT, AND IT NEVER MOVES. WALK MY LINE TO IT FROM " + first + " AND SEE."};
    T.push_back("UNDER THE STARS");
  } else if (c.variant == V_BEACON) {
    R = {"THE LAST STORM MOVED THE BEACON-STONES. WALK THE COAST FROM " + first + " AND TELL ME WHICH STILL STAND."};
    T.push_back("THE BEACON STONES");
  } else if (c.variant == V_WAR) {
    R = {"THE WAR HAS MOVED WHAT'S OURS. WALK THE LINE FROM " + first + " AND TELL ME WHERE THE ENEMY'S STONES STAND."};
    T.push_back("CHART THE MARCH");
  } else switch ((Role)c.trade) {
      case Role::Mage: R = {"I MAKE THE MAPS HERE AND I AM TIRED OF GUESSING. WALK THREE POINTS FOR ME, STARTING AT " + first + ".",
                            "THE OLD CHARTS SHOW A MARK AT " + first + " THAT NO ONE ALIVE HAS SEEN. FIND IT, AND TWO MORE BEYOND."}; break;
      case Role::Merchant: R = {"THE TAX-MAN WANTS A ROAD MAP AND MINE IS A GUESS. SEE " + first + " AND THE TWO MARKS BEYOND WITH YOUR OWN EYES.",
                                "I MAKE THE MAPS FOR THE CARTERS AND I AM TIRED OF GUESSING. WALK THREE POINTS FOR ME, STARTING AT " + first + "."}; break;
      case Role::Priest: R = {"THE PILGRIMS NEED A SAFE WAY TO " + first + " AND BEYOND. WALK IT FIRST AND TELL ME WHICH FORD IS DRY."}; break;
      default: R = {"MY KNEES CAN'T WALK THE ROAD ANY MORE. SOMEONE MUST SEE " + first + " AND THE TWO MARKS BEYOND.",
                    "THE BOUNDARY STONES ARE LOST UNDER BRAMBLE. I WANT THEM FOUND, STARTING AT " + first + "."}; break;
    }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"READ ME EACH MARK, AND DON'T LIE ABOUT THE WEATHER.", "STAND BY EACH STONE AND TELL ME WHAT YOU SEE.",
                     "THREE STONES, ONE LINE. DON'T SKIP THE MIDDLE ONE."});
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Mage: out.variant = d.below(4) == 0 ? V_STAR : V_DEFAULT; break;
    case Role::Jarl: out.variant = w.war ? V_WAR : V_DEFAULT; break;
    case Role::Merchant: case Role::Priest: out.variant = V_DEFAULT; break;
    case Role::Fisher: if (!w.coast) return false; out.variant = V_BEACON; break;
    default: return false;
  }
  if (out.variant == V_STAR) { out.twist = T_STAR; return true; }
  // stop three's beat: none 35, occupied 25 (more with danger), fresh 20 (lord, merchant, priest, mage), hermit 20
  const int roll = d.below(100);
  const bool freshOk = w.trade == (int)Role::Jarl || w.trade == (int)Role::Merchant || w.trade == (int)Role::Priest || w.trade == (int)Role::Mage;
  out.twist = T_NONE;
  if (roll < (w.danger ? 40 : 25)) out.twist = T_OCCUPIED;
  else if (roll >= 45 && roll < 65 && freshOk && out.variant == V_DEFAULT) out.twist = T_FRESH;
  else if (roll >= 65 && roll < 85 && out.variant == V_DEFAULT) out.twist = T_HERMIT;
  return true;
}

JobCtx walk(Role t, int variant, int twist, bool war = false, bool coast = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.war = war; c.coast = coast; c.season = 0;
  return c;
}

}  // namespace

const KindDef& kindSurvey() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "survey";
    k.title = "THE SURVEY LINE";
    k.lead = "PAYS SOMEONE TO WALK A SURVEY LINE";
    k.body = kBody;
    k.variants = {"default", "beacon", "war", "star"};
    k.twists = {"none", "occupied", "fresh", "hermit", "star"};
    k.trades = {(int)Role::Mage, (int)Role::Jarl, (int)Role::Merchant, (int)Role::Priest, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Merchant, V_DEFAULT, T_NONE), walk(Role::Priest, V_DEFAULT, T_FRESH), walk(Role::Jarl, V_DEFAULT, T_HERMIT),
               walk(Role::Mage, V_STAR, T_STAR), walk(Role::Jarl, V_WAR, T_OCCUPIED, true), walk(Role::Fisher, V_BEACON, T_NONE, false, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
