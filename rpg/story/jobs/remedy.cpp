// P1b radiant job kind 7: A CURE FOR THE SICK (content: jobs_b.md section 2; judge notes resolved). JOBS lane.
//
// A race against sickness. A healer needs bundles of a plant that only grows where it is dangerous to go (a cave's
// damp or a ruin's lee, by the season), one bundle a bed. The CLOCK is soft: the tracker counts the days, the journal
// changes at half time and at the deadline, and the turn-in counts the days late: each costs a life (capped at three,
// so every line fits; a farmer's own two children were dosed first and are never among the dead). Early pays a bonus.
// Twists (one in four each, at most one):
//   A  ONE BUNDLE SHORT (the ward givers: a priest's sickroom, a mage's physic ward, a quarantined boat's crew): one
//      bundle rotted; the last three cases (a child, the nurse, an elder, or a boat's boy, cook and bosun) and two doses.
//      Choose who goes without, or boil it weak (a temple-trained hand: background novice). Only when the cure is in
//      time: a late cure has already counted its dead.
//   B  THE HOARDER: a trader with the lord's forest right (a licence to cut wild herbs) got to the patch first and waits
//      there for his cart with full sacks: pay his price (the giver repays it), bargain (a name he has heard of: fame 4),
//      or take it by force (his two hired knives; the healers remember).
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
role patch site [[=sitetype]] near
{?tw_short
role kid person [[male|female]] young at home
role nurse person [[male|female]] at home
role elder person [[male|female]] at home
}
{?tw_hoard
role trader person male in patch
}

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[=clockline]]"
  opt "[[I'LL BRING IT. FAST.|I'M ALREADY GONE.|HOLD ON. I'LL FIND IT.]]" -> [[=first]]
  opt "[[I CAN'T. I'M SORRY.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

{!tw_hoard
stage fetch_a
  goal fetch "[[=bundles]]" in patch
  journal "[[=sick]] ABED IN {HOME} WITH [[=ail]]. BRING [[=bundles]] FROM {PATCH}, {PATCH.DIR}. [[=days]] DAYS."
  deadline [[=half]] -> fetch_b
  then turnin

stage fetch_b
  goal fetch "[[=bundles]]" in patch
  journal "HALF THE DAYS ARE GONE. NONE ARE WELL. BRING [[=bundles]] FROM {PATCH} TO {GIVER}."
  deadline [[=rest]] -> fetch_late
  then turnin

stage fetch_late
  goal fetch "[[=bundles]]" in patch
  journal "THE DAYS ARE GONE. EACH DAY NOW COSTS A LIFE IN {HOME}. BRING [[=bundles]] FROM {PATCH}."
  then turnin
}

{?tw_hoard
stage go
  goal goto patch
  journal "[[=sick]] ABED IN {HOME} WITH [[=ail]]. BRING [[=bundles]] FROM {PATCH}, {PATCH.DIR}. [[=days]] DAYS."
  deadline [[=daysn]] -> go_late
  then hoarder

stage go_late
  goal goto patch
  journal "THE DAYS ARE GONE. EACH DAY NOW COSTS A LIFE IN {HOME}. GET THE [[=herb]] FROM {PATCH}."
  then hoarder

stage hoarder
  talk trader
  say "THE AIR IS FREE. [[=herb]] IS NOT. MY KNIVES CLEARED THIS DEN LAST WEEK AND I PICKED IT BARE. I'M WAITING ON MY CART, AND I HAVE THE LORD'S FOREST RIGHT TO CUT IT. [[=price]] GOLD FOR THE LOT."
  journal "A TRADER AT {PATCH} PICKED THE [[=herb]] FIRST, AND WANTS [[=price]] GOLD FOR IT."
  opt "PAY HIS PRICE. ([[=price]] GOLD)" if gold [[=price]] -> paid_him
  opt "[[A TOWN IS DYING. HALF THAT.|HALF. AND YOUR NAME STAYS CLEAN.]]" check fame 4 -> bargained else full
  opt "[[TAKE IT BY FORCE.|I'M TAKING IT.]]" -> force

stage paid_him
  talk trader
  say "A PLEASURE. [[=bundles]], COUNTED."
  journal "YOU PAID THE TRADER. CARRY THE [[=herb]] TO {GIVER} IN {HOME}."
  do gold -[[=price]]
  do give "[[=bundles]]" herb
  opt "[[THE TOWN THANKS YOU. I DON'T.|COUNTED. GOODBYE.]]" -> turnin_paid

stage bargained
  talk trader
  say "...ONE EXTRA BUNDLE. FOR HALF. DO NOT TELL ANYONE. I WON'T BE THE MAN WHO LET A TOWN DIE."
  journal "THE TRADER TOOK HALF HIS PRICE. CARRY THE [[=herb]] TO {GIVER} IN {HOME}."
  do gold -[[=halfprice]]
  do give "[[=bundles]]" herb
  opt "[[YOUR SECRET IS SAFE.|THANK YOU.]]" -> turnin_bargain

stage full
  talk trader
  say "A NAME I NEVER HEARD, ASKING HALF? NO. THE FULL PRICE, OR WALK."
  journal "THE TRADER AT {PATCH} WON'T BARGAIN."
  opt "PAY HIS PRICE. ([[=price]] GOLD)" if gold [[=price]] -> paid_him
  opt "[[THEN BY FORCE.|I'M TAKING IT.]]" -> force

stage force
  goal slay trader
  say "I WILL REMEMBER YOUR FACE."
  journal "THE TRADER AND HIS HIRED KNIVES WON'T GIVE UP THE [[=herb]]. TAKE IT."
  do hostile trader
  do ambush bandit 2
  then forced

stage forced
  goal goto patch
  journal "THE [[=herb]] IS YOURS. CARRY IT TO {GIVER} IN {HOME}."
  do give "[[=bundles]]" herb
  do deed stole
  then turnin_force

stage turnin_paid
  talk giver
  say "YOU PAID HIM? TAKE EVERY COIN BACK, FROM THE TOWN'S PURSE. NOBODY LOSES BY SAVING THE SICK."
  journal "BRING THE [[=herb]] TO {GIVER} IN {HOME}."
  do gold [[=price]]
[[=turnopts]]

stage turnin_bargain
  talk giver
  say "HALF THE PRICE? THE TOWN OWES YOU. HERE IS WHAT YOU PAID, AND A QUARTER OF WHAT YOU SAVED US."
  journal "BRING THE [[=herb]] TO {GIVER} IN {HOME}."
  do gold [[=halfprice]]
  do gold [[=tip]]
[[=turnopts]]

stage turnin_force
  talk giver
  say "THE BUNDLES ARE HERE. THE BLOOD TOO. ...THE HEALERS WILL REMEMBER HOW THIS WAS GOT."
  journal "BRING THE [[=herb]] TO {GIVER} IN {HOME}."
  do remember giver "THE HERBS CAME WITH BLOOD ON THEM. THE HEALERS REMEMBER."
[[=turnopts]]
}

{!tw_hoard
stage turnin
  talk giver
  say "YOU'RE BACK. GIVE IT HERE, QUICKLY. LET ME COUNT."
  journal "BRING THE [[=herb]] TO {GIVER} IN {HOME}."
[[=turnopts]]
}

{?tw_short
stage short
  talk giver
  say "ONE BUNDLE ROTTED IN THE SACK. THREE LEFT TO TREAT, TWO DOSES: [[=trio]] I CANNOT CHOOSE. YOU DECIDE."
  journal "ONE BUNDLE ROTTED. THREE OF {GIVER}'S SICK, AND TWO DOSES. SOMEONE MUST CHOOSE."
  opt "{ELDER} GOES WITHOUT." -> old_end
  opt "{NURSE} GOES WITHOUT." -> nurse_end
  opt "{KID} GOES WITHOUT." -> kid_end
  opt "BOIL IT WEAK, FOR ALL THREE." check bg novice -> weak_end else weak_fail
}

{!tw_short
stage early
  end success
  say "ALREADY? THEN THEY STAY WELL. [[=intime]] A BONUS, FOR THE SPEED."
  journal "THE [[=herb]] REACHED {HOME} EARLY. ALL [[=sick]] OF THE SICK WILL LIVE."
  do take "[[=bundles]]"
  do reward fair 125
  do deed charity
  do remember giver "[[=mem]]"
  do census home mood 4
  do census home stock produce 2

stage intime
  end success
  say "[[=intime]]"
  journal "THE [[=herb]] REACHED {HOME} IN TIME. ALL [[=sick]] OF THE SICK WILL LIVE."
  do take "[[=bundles]]"
  do reward fair 100
  do deed charity
  do remember giver "[[=mem]]"
  do census home mood 3
  do census home stock produce 2
}

stage late1
  end success
  say "[[=late1]]"
  journal "THE [[=herb]] CAME LATE TO {HOME}. ONE OF THE SICK DIED. THE REST WILL LIVE."
  do take "[[=bundles]]"
  do reward fair 50
  do deed charity
  do remember giver "ONE OF THEM DIED BEFORE YOU CAME. THE REST LIVE. I THINK OF BOTH."
  do census home mood -2
  do census home stock produce 1

stage late2
  end success
  say "[[=late2]]"
  journal "THE [[=herb]] CAME LATE TO {HOME}. TWO OF THE SICK DIED. THE REST WILL LIVE."
  do take "[[=bundles]]"
  do reward fair 50
  do deed charity
  do remember giver "TWO DIED BEFORE YOU CAME. THE REST LIVE. I THINK OF BOTH."
  do census home mood -3
  do census home stock produce 1

{!x_cap2|x_cavecap2
stage late3
  end success
  say "[[=late3]]"
  journal "THE [[=herb]] CAME LATE TO {HOME}. THREE OF THE SICK DIED. THE REST WILL LIVE."
  do take "[[=bundles]]"
  do reward fair 50
  do deed charity
  do remember giver "THREE DIED BEFORE YOU CAME. THE REST LIVE. I THINK OF ALL OF THEM."
  do census home mood -4
  do census home stock produce 1
}

{?tw_short
stage old_end
  end success
  say "{ELDER} SAID: GIVE IT TO THE YOUNG. {ELDER.HE} DIED TWO DAYS ON, EASY, WITH A BLESSING FOR YOU. WE WILL BURY {ELDER.HIM} WITH HONOUR."
  journal "{ELDER} WENT WITHOUT, AND DIED EASY. {KID} AND {NURSE} LIVE."
  do take "[[=bundles]]"
  do reward fair 100
  do xp 20
  do deed charity
  do remember giver "{ELDER} CHOSE IT TOO, IN THE END. GIVE IT TO THE YOUNG, {ELDER.HE} SAID."
  do census home mood 2
  do census home stock produce 1

stage nurse_end
  end success
  say "{NURSE} DIED THE NEXT DAY. {KID} AND {ELDER} LIVE, BUT [[=nurseloss]] AND TWO MORE FELL THIS WEEK. WE LIVE, BUT NOT WELL."
  journal "{NURSE} WENT WITHOUT, AND DIED. WITHOUT {NURSE.HIM}, TWO MORE FELL SICK."
  do take "[[=bundles]]"
  do reward fair 100
  do deed charity
  do rumour
  do remember giver "THE WARD HAS NO ONE TO KEEP IT NOW. WE MANAGE."
  do census home mood -2
  do census home stock produce 1

stage kid_end
  end success
  say "{NURSE} LIVES. {ELDER} LIVES. {KID} DID NOT. ...GO."
  journal "{KID} WENT WITHOUT, AND DIED. {NURSE} LIVED, AND SAVES A DOZEN MORE THROUGH THE SEASON. {GIVER} WILL NOT LOOK AT YOU."
  do take "[[=bundles]]"
  do reward fair 80
  do rumour
  do remember giver "...I DON'T WANT TO TALK TO YOU."
  do census home mood -3
  do census home stock produce 1

stage weak_end
  end success
  say "A WEAK BREW. A STRONG WILL. THEY LIVE, ALL THREE, WEAK BUT ALIVE. THE WARD IS CALLING IT A MIRACLE."
  journal "YOU STRETCHED THE LAST GOOD BUNDLE INTO A THIN BREW. ALL THREE LIVE."
  do take "[[=bundles]]"
  do reward fair 100
  do xp 60
  do fame 1
  do deed charity
  do remember giver "THE WEAK BREW. ALL THREE LIVED. I STILL DON'T KNOW HOW."
  do census home mood 4
  do census home stock produce 1

stage weak_fail
  end success
  say "{ELDER} DIED. {KID} SHAKES AGAIN, AND WILL BE SICK A WEEK. IT WAS TOO THIN. YOU TRIED."
  journal "THE WEAK BREW WAS TOO THIN. {ELDER} DIED, AND {KID} RELAPSED."
  do take "[[=bundles]]"
  do reward fair 80
  do remember giver "THE BREW WAS TOO THIN. WE TRIED. THAT'S ALL."
  do census home mood -1
  do census home stock produce 1
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S SICK."
)DSL";

enum { V_PRIEST, V_MAGE, V_JARL, V_INN, V_FARMER, V_VILLAGER, V_HUNTER, V_HARBOUR };
enum { T_NONE, T_SHORT, T_HOARD };

const char* countWord(int n) {
  static const char* w[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE"};
  return n >= 0 && n <= 12 ? w[n] : "MANY";
}

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const bool cave = (c.extra & 1) != 0;
  P["sitetype"] = cave ? "cave" : "ruin";
  static const char* ails[4][2] = {{"THE SPRING AGUE", "THE SPRING AGUE"}, {"THE SWEATS", "BOG FEVER"}, {"THE RED SPOTS", "THE SHAKES"},
                                   {"THE WINTER COUGH", "THE WINTER COUGH"}};
  const std::string ail = ails[c.season & 3][d.below(2)];
  static const char* herbs[4][2] = {{"WILLOW BARK", "WILD GARLIC"}, {"FEVERFEW", "YARROW"}, {"ELDERBERRY", "ROSEHIP"}, {"WINTERGREEN", "PINE TIPS"}};
  static const char* caveHerbs[3] = {"BLACK MOSS", "PALE LICHEN", "MOONCAP"};
  const std::string herb = cave ? caveHerbs[d.below(3)] : herbs[c.season & 3][d.below(2)];
  int n = 4 + 2 * c.tier + d.below(3);
  if (n > 12) n = 12;
  const int days = (c.season == 3 ? 4 : c.season == 1 ? 7 : 5) + (cave ? 1 : 0);
  const int half = days / 2;
  P["ail"] = ail; P["herb"] = herb; P["sick"] = countWord(n); P["days"] = countWord(days); P["daysn"] = std::to_string(days);
  P["bundles"] = std::string(countWord(n)) + " BUNDLES OF " + herb;
  P["half"] = std::to_string(half); P["rest"] = std::to_string(days - half);
  const int price = n * 8;
  P["price"] = std::to_string(price); P["halfprice"] = std::to_string(price / 2); P["tip"] = std::to_string(price / 8);
  P["first"] = c.twist == T_HOARD ? "go" : "fetch_a";
  // the dead are capped at three (two for a farmer's lane in a village: the giver's own two are dosed first)
  const bool farmer = c.variant == V_FARMER;
  const int cap = (c.extra & 2) ? 2 : 3;
  (void)farmer;
  // the turn-in: exactly one option shows, by the days since the story began (early / in time / one, two, three late)
  const bool shortTw = c.twist == T_SHORT;
  std::string o;
  o += "  opt \"" + std::string(countWord(n)) + " BUNDLES, AND QUICKLY.\" if nodays " + std::to_string(half) + " -> " + (shortTw ? "short" : "early") + "\n";
  o += "  opt \"" + std::string(countWord(n)) + " BUNDLES, AS YOU ASKED.\" if days " + std::to_string(half) + " if nodays " + std::to_string(days) + " -> " +
       (shortTw ? "short" : "intime") + "\n";
  o += "  opt \"I CAME AS FAST AS I COULD.\" if days " + std::to_string(days) + " if nodays " + std::to_string(days + 1) + " -> late1\n";
  if (cap >= 3) {
    o += "  opt \"I CAME AS FAST AS I COULD.\" if days " + std::to_string(days + 1) + " if nodays " + std::to_string(days + 2) + " -> late2\n";
    o += "  opt \"I CAME AS FAST AS I COULD.\" if days " + std::to_string(days + 2) + " -> late3\n";
  } else {
    o += "  opt \"I CAME AS FAST AS I COULD.\" if days " + std::to_string(days + 1) + " -> late2\n";
  }
  P["turnopts"] = o;
  // the giver's voice: in time, late (the dead named first, then the rest)
  std::string in, l1, l2, l3, reason;
  std::vector<std::string> R;
  switch (c.variant) {
    case V_PRIEST:
      in = d.pick({"THE SICKROOM HAS STOPPED PRAYING. THEY ARE ASKING FOR BREAD. BLESS YOU.", "TEA IS ON. THEY ARE SITTING UP. THE BEST SOUND ALL WEEK: A COMPLAINT."});
      l1 = "I SAID THE WORDS FOR ONE OF THEM. COME. THE REST WILL LIVE. THAT IS SOMETHING.";
      R = {"THE SICKROOM BEHIND THE CHAPEL HAS " + std::string(countWord(n)) + " IN IT. PRAYER HELPS THEM SLEEP. " + herb + " HELPS THEM WAKE."};
      break;
    case V_MAGE:
      in = "FRESH " + herb + ". THE TONIC WORKS. I ALMOST LIKE YOU FOR THIS.";
      l1 = herb + " IS FRESH. ONE OF THEM IS NOT. I MADE WHAT I COULD OF IT.";
      R = {"I CAN MAKE THE TONIC. I CANNOT MAKE " + herb + ". IT GROWS ONLY AT {PATCH}, WHERE THE EARTH IS OLD AND SPOILED."};
      break;
    case V_JARL:
      in = "A WELL PEOPLE ARE A KEPT PEOPLE. {HOME} IS IN YOUR DEBT. I NOTE IT.";
      l1 = "LATE. ONE OF THEM DIED. SAY NOTHING. THE REST WILL LIVE. THAT IS ALL.";
      R = {"A LEADER WHO CANNOT KEEP THE PEOPLE WELL DOES NOT KEEP THEM LONG. " + ail + " IS IN {HOME}. " + herb + " IS AT {PATCH}."};
      break;
    case V_INN:
      in = "I HAVE TWO CUSTOMERS FEELING BRAVE ALREADY. THE FEVER BROKE BY MORNING.";
      l1 = "ONE OF THEM DIED UNDER MY ROOF. ...SIT. WE ARE ALL TIRED.";
      R = {"A DROVER BROUGHT " + ail + " IN WITH HIM. HALF MY ROOMS ARE COUGHING AND THE OTHER HALF ARE LEAVING. I NEED " + herb + "."};
      break;
    case V_FARMER:
      in = "MY TWO ARE UP, EATING AND ARGUING. THE FIELD CAN WAIT. THEY CANNOT. THEY DIDN'T HAVE TO.";
      l1 = "MY TWO LIVE. THE LANE LOST ONE. TAKE THE GOLD. I OWE YOU MORE THAN THIS.";
      R = {std::string(countWord(n)) + " ARE ABED ALONG THE LANE WITH " + ail + ", TWO OF MY CHILDREN AMONG THEM. THE HEALER SAYS " + herb + "."};
      break;
    case V_HUNTER:
      in = "YOU KNEW THE WAY IN, THEN. THE LANE IS UP AND CURSING ME ALREADY.";
      l1 = "ONE OF THEM DIED WHILE YOU WERE OUT THERE. I SHOULD HAVE GONE WITH YOU.";
      R = {"I TRAP AND I SKIN AND I CANNOT NURSE. " + ail + " IS IN THE LANE, AND THE ONLY " + herb + " LEFT GROWS AT {PATCH}, WHERE I KNOW WHAT LIVES."};
      break;
    case V_HARBOUR:
      in = "THE HARBOUR OPENS TOMORROW. THE CREW LIVES. SO DOES TRADE. THANK YOU.";
      l1 = "ONE OF THEM DIED IN QUARANTINE. THE REST ARE FIT. THE HARBOUR OPENS.";
      R = {"A BOAT CAME IN WITH " + ail + " BELOW DECKS. THE HARBOUR STAYS SHUT UNTIL WE HAVE " + herb + "."};
      break;
    default:   // villager
      in = "MY NEIGHBOURS ARE AWAKE. THANK YOU. I WILL BAKE FOR A WEEK.";
      l1 = "ONE OF THEM DID NOT WAKE. THE REST DID. THANK YOU. I WILL REMEMBER.";
      R = {"THE WET AIR BROUGHT " + ail + " UP FROM THE MARSH. " + std::string(countWord(n)) + " ARE ABED, AND IT IS A RACE THAT RUNS FAST."};
      break;
  }
  if (c.war || c.season == 3) R.push_back("THE REFUGEES CAME IN WEAK, AND WEAK FOLK CATCH " + ail + ". WE HAVE BLANKETS. WE HAVE NO " + herb + ".");
  // the second and third deaths, by count (the count word is the only change: every line fits)
  auto more = [&](const char* w) {
    std::string s = l1;
    const size_t p = s.find("ONE");
    if (p != std::string::npos) s = s.substr(0, p) + w + s.substr(p + 3);
    return s;
  };
  l2 = more("TWO"); l3 = more("THREE");
  P["intime"] = in; P["late1"] = l1; P["late2"] = l2; P["late3"] = l3;
  P["mem"] = "THE " + herb + " YOU BROUGHT. THEY'RE ALL WELL NOW.";
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"BRING ME THE BUNDLES, FAST.", "GATHER IT AND RUN.", "PICK IT. THE SICK DO NOT WAIT.", "ONE BUNDLE FOR EACH BED."});
  P["clockline"] = std::string(countWord(days)) + " DAYS. EVERY DAY AFTER THAT COSTS A LIFE.";
  // twist A's three: a ward's (priest, mage) or a boat's (harbour)
  P["trio"] = c.variant == V_HARBOUR ? "{KID} THE BOAT'S BOY, {NURSE} THE COOK, {ELDER} THE OLD BOSUN."
                                     : "{KID}, A CHILD OF SIX. {NURSE}, WHO KEEPS THE WARD. {ELDER}, EIGHTY WINTERS.";
  P["nurseloss"] = c.variant == V_HARBOUR ? "NOBODY COOKS FOR THE SICK NOW," : "THE WARD HAS NO ONE TO KEEP IT,";
  std::vector<std::string> T = {"A CURE FOR THE SICK", "THE SICK CANNOT WAIT", "NOT ONE MORE GRAVE", "MEDICINE IN THE DARK", herb + ", AND FAST"};
  if (c.variant == V_PRIEST || c.variant == V_MAGE) T.push_back("THE HEALER'S LIST");
  if (c.variant == V_PRIEST || c.variant == V_INN || c.variant == V_VILLAGER) T.push_back("THE SICKROOM IS FULL");
  if (c.variant == V_PRIEST || c.variant == V_HARBOUR) T.push_back("THE LONG WARD");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Priest: out.variant = V_PRIEST; break;
    case Role::Mage: out.variant = V_MAGE; break;
    case Role::Jarl: out.variant = V_JARL; break;
    case Role::Innkeeper: out.variant = V_INN; break;
    case Role::Farmer: out.variant = V_FARMER; break;
    case Role::Villager: out.variant = V_VILLAGER; break;
    case Role::Hunter: out.variant = V_HUNTER; break;
    case Role::Fisher: if (!w.port) return false; out.variant = V_HARBOUR; break;
    default: return false;
  }
  out.extra = d.chance(45) ? 1 : 0;   // the patch: a cave's damp (any season) or a ruin's lee
  const int roll = d.below(4);
  const bool ward = out.variant == V_PRIEST || out.variant == V_MAGE || out.variant == V_HARBOUR;
  out.twist = roll == 0 && ward ? T_SHORT : roll == 1 ? T_HOARD : T_NONE;
  // a farmer's village lane with few abed caps the dead at two (the giver's own are dosed first)
  if (out.variant == V_FARMER && w.tier == 0) out.extra |= 2;
  return true;
}

JobCtx walk(Role t, int variant, int twist, int season, int extra, int tier = 0, bool port = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.season = season; c.extra = extra; c.tier = tier; c.port = port;
  return c;
}

}  // namespace

const KindDef& kindRemedy() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "remedy";
    k.title = "A CURE FOR THE SICK";
    k.lead = "NEEDS MEDICINE FOR THE SICK, FAST";
    k.body = kBody;
    k.variants = {"priest", "mage", "jarl", "innkeeper", "farmer", "villager", "hunter", "harbour"};
    k.twists = {"none", "short", "hoard"};
    k.extras = {"ruin", "cave", "cap2", "cavecap2"};
    k.trades = {(int)Role::Priest, (int)Role::Mage, (int)Role::Jarl, (int)Role::Innkeeper, (int)Role::Farmer, (int)Role::Villager, (int)Role::Hunter, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Priest, V_PRIEST, T_SHORT, 3, 0), walk(Role::Farmer, V_FARMER, T_NONE, 1, 3), walk(Role::Innkeeper, V_INN, T_HOARD, 2, 1, 1),
               walk(Role::Fisher, V_HARBOUR, T_SHORT, 0, 1, 1, true), walk(Role::Hunter, V_HUNTER, T_NONE, 2, 0, 2)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
