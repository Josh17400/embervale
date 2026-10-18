// P1b radiant job kind 10: THE LONG ROUND (content: jobs_b.md section 5; judge notes resolved). JOBS lane.
//
// A walk round the far houses, a knock at each, and whatever is behind it: three small stories in one trip, each stop a
// person of its own (SITE-TALK at the dwelling), some warm, some funny, one grim. The player is a neighbour, not a hero;
// the one fight is the wolves at a family's door. The round's three stops are drawn per job from eight (by the season,
// the giver's hook and the world) and composed into the script, so the round is its own story each time; the giver's
// report scales with how many stops were HELPED, not only visited.
//   the stops: SNOWED IN (winter), THE STUBBORN (an old man who will not leave), THE EMPTY CHAIR (a neighbour found the
//   keeper dead in her chair: bury her and carry her knitting to her niece, bury it with her, or say a word and leave),
//   WOLVES AT THE DOOR (drive them off, or send for the hunters), THE LIAR (proud, starving: pretend, leave a loaf, or
//   press him and send the town's food), A BIRTH (send for the healer, or stay till morning), THE LETTER (a hermit's
//   letter to his sister in the giver's town, about a goose), THE BEEKEEPER (spring and summer).
// The places are the wayside DWELLINGS (a lone hut, camp or garden; ENGINE lane); until they are cast, the fallback is the
// small settlements round about, the stop's person living at their edge. Judge: no dwelling name in a title or an
// option; one name per journal line; the stop's person introduces themselves against the place's old name; no door,
// larder or arrows a tent may not have; the farmer's "old man who missed market" is always one of the stops; the gale
// and Mist hooks are dropped (no stop paid them off); a pressed liar is fed by the town.
#include <map>
#include <string>
#include <vector>
#include "rpg/sim/world.h"
#include "rpg/story/radiant.h"

namespace story {
namespace radiant {

namespace {

enum { S_SNOW, S_STUBBORN, S_CHAIR, S_WOLVES, S_LIAR, S_BIRTH, S_LETTER, S_BEES, S_COUNT };
const char* const kStopNames[S_COUNT] = {"snow", "stubborn", "chair", "wolves", "liar", "birth", "letter", "bees"};

// one stop, written once with %K for its slot and %N for the stage that follows it
const char* const kStopBody[S_COUNT] = {
    // SNOWED IN
    R"DSL(
stage meet%K
  talk p%K
  say "WHO'S THERE? THE SNOW IS UP TO THE ROOF AND I CAN'T DIG OUT. THE NAME ON THIS PLACE IS MY MOTHER'S, AND I'VE NEVER BEEN SO SORRY TO KEEP IT. I HAVE BEEN EATING THE CANDLES."
  journal "{P%K}, AT HOUSE %K, IS SNOWED IN."
  opt "[[DIG HER OUT.|I'LL DIG YOU OUT.]]" -> dig%K
  opt "[[I'LL PASS FOOD THROUGH.|HERE. EAT FIRST.]]" -> food%K

stage dig%K
  talk p%K
  say "THAT TOOK YOU LONG ENOUGH. ...THANK YOU. TELL THEM I'M ALIVE AND OFFENDED."
  journal "YOU DUG {P%K} OUT OF THE SNOW."
  do add helped 1
  opt "[[GLAD TO HELP.|STAY WARM.]]" -> %N

stage food%K
  talk p%K
  say "...CANDLE WAX. THIS IS BETTER. SEND SOMEONE WITH A SPADE, WILL YOU?"
  journal "YOU FED {P%K}. THE TOWN WILL SEND DIGGERS."
  do add helped 1
  do rumour
  opt "[[I'LL TELL THEM.|DIGGERS ARE COMING.]]" -> %N
)DSL",
    // THE STUBBORN
    R"DSL(
stage meet%K
  talk p%K
  say "I HAVE LIVED HERE SIXTY YEARS. IT CARRIES SOMEBODY ELSE'S NAME, BUT IT'S MINE. [[=stubborn]]"
  journal "{P%K}, AT HOUSE %K, REFUSES THE TOWN'S HELP."
  opt "[[I'LL FILL YOUR WOODPILE.|LET ME SPLIT SOME WOOD.]]" -> wood%K
  opt "COME DOWN FOR THE MONTH." check fame 3 -> argued%K else shut%K
  opt "[[I'LL LEAVE YOU BE.|SUIT YOURSELF.]]" -> %N

stage wood%K
  talk p%K
  say "...YOU SPLIT IT SQUARE. GOOD ARMS. NOW GO TELL THEM I AM WELL."
  journal "YOU FILLED {P%K}'S WOODPILE."
  do add helped 1
  opt "[[I WILL.|STAY WARM.]]" -> %N

stage argued%K
  talk p%K
  say "FINE. A MONTH. NOT A DAY MORE. AND I BRING MY OWN CHAIR."
  journal "{P%K} AGREED TO COME DOWN TO {HOME} FOR A MONTH."
  do add helped 1
  do moves p%K home
  opt "[[A MONTH, THEN.|BRING THE CHAIR.]]" -> %N

stage shut%K
  talk p%K
  say "NO. AND SHUT THE GATE ON YOUR WAY OUT."
  journal "{P%K} SHUT YOU OUT."
  opt "[[AS YOU LIKE.|STAY WELL.]]" -> %N
)DSL",
    // THE EMPTY CHAIR
    R"DSL(
stage meet%K
  talk p%K
  say "YOU'RE FROM {HOME}? THEN YOU'RE TOO LATE. I FOUND {DEAD%K} THIS MORNING. SHE SAT DOWN AND DID NOT RISE. THE FIRE IS OUT. KNITTING IN HER LAP, AND A NOTE: FOR MY SISTER'S GIRL."
  journal "{DEAD%K}, AT HOUSE %K, DIED IN HER CHAIR. {P%K} FOUND HER."
  opt "[[BURY HER. TAKE THE KNITTING.|WE BURY HER. THE SCARF GOES HOME.]]" -> buryk%K
  opt "[[BURY HER. LEAVE THE KNITTING.|SHE KEEPS HER WORK.]]" -> buryl%K
  opt "[[SAY A WORD AND LEAVE.|A WORD FOR HER, AND GO.]]" -> word%K

stage buryk%K
  talk p%K
  say "A GRAVE BY THE WALL, THEN, AND THE SCARF FOR {NIECE}, IN {HOME}. SHE'D LIKE THAT. SO WOULD {DEAD%K}."
  journal "YOU BURIED {DEAD%K}. HER HALF-FINISHED SCARF IS FOR {NIECE} IN {HOME}."
  do add helped 1
  do set deliv 1
  do give "A HALF-FINISHED SCARF" cloak
  do deed charity
  do remember giver "I FEARED IT FOR {DEAD%K}. THANK YOU FOR DIGGING."
  opt "[[REST WELL.|I'LL TAKE IT TO HER.]]" -> %N

stage buryl%K
  talk p%K
  say "BURIED WITH HER WORK. GOOD. SHE NEVER LEFT A ROW UNFINISHED BUT THIS ONE."
  journal "YOU BURIED {DEAD%K} WITH HER KNITTING."
  do add helped 1
  do remember giver "{DEAD%K} WAS BURIED WITH HER WORK. GOOD. I'LL SEND FLOWERS."
  opt "[[REST WELL.|SHE'S AT PEACE.]]" -> %N

stage word%K
  talk p%K
  say "A WORD, THEN, AND THE CHAIR. I'LL TELL {HOME} TO SEND A CART."
  journal "YOU SAID A WORD FOR {DEAD%K} AND LEFT HER IN HER CHAIR."
  do deed prayed
  do remember giver "YOU LEFT {DEAD%K} IN HER CHAIR? ...I SEE. A WORD IS SOMETHING. A GRAVE IS MORE."
  opt "[[GOODBYE.|SEND THE CART.]]" -> %N
)DSL",
    // WOLVES AT THE DOOR
    R"DSL(
stage meet%K
  talk p%K
  say "THEY HAVE BEEN OUTSIDE THREE NIGHTS. GREY ONES. THE CHILDREN WON'T SLEEP, AND NEITHER WILL I. WE HAVE NOTHING LEFT TO THROW AT THEM."
  journal "WOLVES CIRCLE {P%K}'S HOME, HOUSE %K OF THE ROUND."
  opt "[[I'LL DRIVE THEM OFF.|STAY INSIDE. I'LL DEAL WITH THEM.]]" -> wolves%K
  opt "[[I'LL SEND HUNTERS FROM {HOME}.|HOLD ON. I'LL SEND HELP.]]" -> send%K

stage wolves%K
  goal kill 2 wolf
  journal "DRIVE OFF THE WOLVES AT {P%K}'S DOOR."
  do ambush wolf 3
  then saved%K

stage saved%K
  talk p%K
  say "THEY'RE GONE? ALL OF THEM? ...THE CHILDREN WILL SLEEP TONIGHT. SO WILL I."
  journal "THE WOLVES ARE GONE FROM HOUSE %K."
  do add helped 1
  opt "[[SLEEP WELL.|BAR THE SHUTTERS ANYWAY.]]" -> %N

stage send%K
  talk p%K
  say "HUNTERS. SOON, I HOPE. ...GO ON, THEN."
  journal "YOU PROMISED {P%K} HUNTERS FROM {HOME}."
  do rumour
  opt "[[SOON.|HOLD ON.]]" -> %N
)DSL",
    // THE LIAR
    R"DSL(
stage meet%K
  talk p%K
  say "NEVER BETTER. YOU SHOULD GO. I DO NOT NEED A THING. ...THE NAME ON THE GATE? MY OLD MASTER'S. I KEEP IT PAINTED."
  journal "{P%K}, AT HOUSE %K, SWEARS ALL IS WELL. THE SHELVES SAY OTHERWISE."
  opt "[[PRETEND TO BELIEVE HIM.|GLAD TO HEAR IT.]]" -> pretend%K
  opt "[[LEAVE A LOAF. SAY NOTHING.|HERE. FOR LATER.]]" -> loaf%K
  opt "YOUR SHELVES ARE BARE." check fame 3 -> press%K else slam%K

stage pretend%K
  talk p%K
  say "GOOD. GOOD. ...GO ON, THEN."
  journal "YOU LEFT {P%K} TO HIS PRIDE."
  do rumour
  opt "[[GOODBYE.|STAY WELL.]]" -> %N

stage loaf%K
  talk p%K
  say "...THIS IS A LOAF. NOT A KINDNESS. I SHALL EAT IT AS A LOAF."
  journal "YOU LEFT {P%K} A LOAF."
  do add helped 1
  opt "[[AS A LOAF, THEN.|ENJOY IT.]]" -> %N

stage press%K
  talk p%K
  say "...THREE WEEKS. I DID NOT WANT TO ASK. I DID NOT KNOW HOW."
  journal "{P%K} BROKE. {HOME} WILL SEND FOOD."
  do add helped 1
  do remember giver "THE OLD MAN ON THE FAR ROUND? WE SEND A BASKET EVERY WEEK NOW. HE CALLS IT A LOAN."
  opt "[[THE TOWN WILL SEND FOOD.|YOU'LL EAT TONIGHT.]]" -> %N

stage slam%K
  talk p%K
  say "BARE? HOW DARE YOU. GOOD DAY."
  journal "{P%K} SLAMMED THE SHUTTER ON YOU."
  opt "[[GOOD DAY.|...]]" -> %N
)DSL",
    // A BIRTH
    R"DSL(
stage meet%K
  talk p%K
  say "THE BABY IS EARLY AND SMALL, AND THERE IS NOBODY HERE. I NEED THE HEALER OR A MIRACLE."
  journal "{P%K}, AT HOUSE %K, IS IN LABOUR ALONE."
  opt "[[I'LL SEND WORD TO THE HEALER.|THE HEALER WILL COME.]]" -> word%K
  opt "[[I'LL STAY AND HELP.|I'M NOT GOING ANYWHERE.]]" -> stay%K

stage word%K
  talk p%K
  say "THEN RUN. AND TELL THEM TO RUN TOO."
  journal "YOU SENT FOR THE HEALER FOR {P%K}."
  do add helped 1
  do rumour
  opt "[[RUNNING.|HOLD ON.]]" -> %N

stage stay%K
  goal time 6 10
  journal "STAY WITH {P%K} THROUGH THE NIGHT."
  then born%K

stage born%K
  talk p%K
  say "A GIRL. SMALL AND LOUD. ...SHE HAS YOUR NAME NOW, YOU KNOW. I DECIDED IN THE NIGHT."
  journal "{P%K}'S CHILD WAS BORN AT DAWN, AND CARRIES YOUR NAME."
  do add helped 1
  do deed charity
  do remember p%K "THE CHILD HAS YOUR NAME. SHE'S LOUD. LIKE YOU, I EXPECT."
  opt "[[SHE'S BEAUTIFUL.|LOOK AT HER.]]" -> %N
)DSL",
    // THE LETTER
    R"DSL(
stage meet%K
  talk p%K
  say "I AM FINE. GO AWAY. ...WAIT. TAKE THIS TO MY SISTER, {SIS}, IN {HOME}. TELL HER I WAS RIGHT ABOUT THE GOOSE."
  journal "{P%K}, A HERMIT AT HOUSE %K, HAS A LETTER FOR {SIS} IN {HOME}."
  opt "[[I'LL TAKE THE LETTER.|GIVE IT HERE.]]" -> take%K
  opt "[[WHAT ABOUT THE GOOSE?|TELL ME ABOUT THE GOOSE.]]" -> goose%K

stage take%K
  talk p%K
  say "GOOD. AND DON'T READ IT."
  journal "CARRY {P%K}'S LETTER TO {SIS} IN {HOME}."
  do add helped 1
  do set deliv 1
  do give "A HERMIT'S LETTER" letter
  opt "[[I WON'T.|ON MY WORD.]]" -> %N

stage goose%K
  talk p%K
  say "SHE BOUGHT A GOOSE AT THE FAIR. I SAID IT WAS A GANDER. TWENTY SPRINGS AND NOT ONE EGG. I WAS RIGHT. SAY SO, LOUDLY. ...HERE, I'VE ADDED A LINE: COME AND EAT. I HAVE A NEW GOOSE."
  journal "CARRY {P%K}'S LETTER, AND ITS NEW LINE, TO {SIS} IN {HOME}."
  do add helped 1
  do set deliv 1
  do set goose 1
  do give "A HERMIT'S LETTER" letter
  opt "[[LOUDLY. GOT IT.|A NEW GOOSE. I'LL TELL HER.]]" -> %N
)DSL",
    // THE BEEKEEPER
    R"DSL(
stage meet%K
  talk p%K
  say "DON'T MOVE! DON'T BREATHE! MY BEES ARE ON YOUR HAT. THE QUEEN IS NOT ON ME. WHERE IS SHE?"
  journal "{P%K}, AT HOUSE %K, IS UP TO HER ANKLES IN HER OWN SWARM."
  opt "[[STAND VERY STILL.|...]]" -> still%K
  opt "[[FIND THE QUEEN.|LET ME LOOK FOR HER.]]" -> queen%K
  opt "[[SWAT THEM.|GET OFF ME!]]" -> swat%K

stage still%K
  talk p%K
  say "BRAVE. OR ASLEEP. EITHER HELPS. ...THERE, THEY'RE GOING HOME. HAVE A JAR, FOR STANDING STILL."
  journal "THE BEES WENT HOME. {P%K} GAVE YOU HONEY."
  do add helped 1
  do give "A JAR OF HONEY" potion
  opt "[[THANK YOU.|ANY TIME. NOT REALLY.]]" -> %N

stage queen%K
  talk p%K
  say "IN MY APRON POCKET? HOW DID SHE GET THERE? I DIDN'T PUT HER THERE. ...THANK YOU. TAKE A JAR."
  journal "YOU FOUND THE QUEEN. {P%K} GAVE YOU HONEY."
  do add helped 1
  do give "A JAR OF HONEY" potion
  do rumour
  opt "[[THANK YOU.|SHE'S A WANDERER.]]" -> %N

stage swat%K
  talk p%K
  say "WELL. THAT WAS A WAY TO DO IT. THEY'LL BE BACK, ANGRIER. ...ARE YOU STUNG? COME HERE."
  journal "YOU SWATTED THE BEES. {P%K} WAS KIND ABOUT IT."
  opt "[[OW.|SORRY.]]" -> %N
)DSL"};

const char* const kHead = R"DSL(
title [[=title]]
role giver giver
role home site home
)DSL";

const char* const kTail = R"DSL(
var helped 0
var deliv 0
var goose 0

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[THE PAY IS SMALL. THE THANKS ARE NOT.|I'LL PAY FOR YOUR BOOTS.|COME BACK AND TELL ME, AND I'LL PAY.]]"
  opt "[[I'LL WALK THE ROUND.|I'LL KNOCK AT EVERY DOOR.|I'LL LOOK IN ON THEM.]]" -> go1
  opt "[[NOT TODAY.|FIND SOMEONE ELSE.|I'M NO NEIGHBOUR.]]" -> decline

stage report
  talk giver
  say "[[YOU'RE BACK. TELL ME, HOUSE BY HOUSE.|BACK FROM THE ROUND? SIT DOWN. TELL ME ALL.|WELL? WHO IS STILL BREATHING OUT THERE?]]"
  journal "TELL {GIVER} IN {HOME} WHAT YOU FOUND ON THE ROUND."
[[=delivopt]]
  opt "[[EVERY DOOR ANSWERED.|ALL OF THEM, HELPED.]]" if is helped 3 [[=notdeliv]]-> end_all
  opt "[[ALL BUT ONE.|ALL BUT ONE HOUSE.]]" if is helped 2 [[=notdeliv]]-> end_most
  opt "[[SOME. NOT ALL.|ONE OF THEM, AT LEAST.]]" if is helped 1 [[=notdeliv]]-> end_some
  opt "[[I WENT. THAT'S ALL.|I KNOCKED. LITTLE MORE.]]" if is helped 0 [[=notdeliv]]-> end_none

{?v_chair|tw_chair|x_chair|v_letter|tw_letter|x_letter
stage report_b
  talk giver
  say "[[AND THE REST OF THE ROUND?|SO. THE REST OF IT?]]"
  journal "TELL {GIVER} IN {HOME} WHAT YOU FOUND ON THE ROUND."
  opt "[[EVERY DOOR ANSWERED.|ALL OF THEM, HELPED.]]" if is helped 3 -> end_all
  opt "[[ALL BUT ONE.|ALL BUT ONE HOUSE.]]" if is helped 2 -> end_most
  opt "[[SOME. NOT ALL.|ONE OF THEM, AT LEAST.]]" if is helped 1 -> end_some
  opt "[[I WENT. THAT'S ALL.|I KNOCKED. LITTLE MORE.]]" if is helped 0 -> end_none
}

stage end_all
  end success
  say "[[=all]]"
  journal "THE LONG ROUND IS WALKED. EVERY HOUSE WAS HELPED."
  do reward fair 125
  do fame 1
  do rumour
  do remember giver "A FRIEND OF THE FAR HOUSES. THAT'S WHAT THEY CALL YOU OUT THERE."
  do census home mood 3
  do census home stock bread 2

stage end_most
  end success
  say "[[=most]]"
  journal "THE LONG ROUND IS WALKED. ALL BUT ONE HOUSE WAS HELPED."
  do reward fair 100
  do remember giver "THE ROUND YOU WALKED. THE FAR HOUSES ASK AFTER YOU."
  do census home mood 2
  do census home stock bread 1

stage end_some
  end success
  say "[[=some]]"
  journal "THE LONG ROUND IS WALKED. SOME OF THE HOUSES WERE HELPED."
  do reward fair 80
  do remember giver "THE ROUND YOU WALKED. SOME HELP IS BETTER THAN NONE."

stage end_none
  end success
  say "[[=none]]"
  journal "THE LONG ROUND IS WALKED. YOU KNOCKED, AND LITTLE MORE."
  do reward fair 60
  do remember giver "YOU WALKED THE ROUND. THEY KNOW WE LOOK, AT LEAST."

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S LONG ROUND."
)DSL";

const char* const kNiece = R"DSL(
stage niece
  talk niece
  say "AUNT {DEADW}'S LAST SCARF? FOR ME? ...SHE NEVER SAID. I NEVER ASKED. I WILL FINISH THE SCARF MYSELF."
  journal "CARRY {DEADW}'S HALF-FINISHED SCARF TO {NIECE} IN {HOME}."
  do take "A HALF-FINISHED SCARF"
  do xp 20
  do rumour
  do remember niece "I WEAR THE SCARF EVERY DAY. I FINISHED IT CROOKED."
  opt "[[SHE'D LIKE THAT.|IT SUITS YOU.]]" -> report_b
)DSL";

const char* const kSister = R"DSL(
stage sister
  talk sis
  say "FROM MY BROTHER? ...A GANDER! TWENTY SPRINGS, NO EGGS. I NAMED HER PIPPIN. SHE WAS A HE. TELL HIM HE WAS RIGHT. ONCE."
  journal "CARRY THE HERMIT'S LETTER TO {SIS} IN {HOME}."
  do take "A HERMIT'S LETTER"
  do remember sis "MY BROTHER WAS RIGHT ABOUT THE GOOSE. DON'T TELL HIM I SAID SO."
  opt "[[ONCE.|I'LL TELL HIM.]]" -> report_b

stage sister_g
  talk sis
  say "A GANDER! AND A NEW GOOSE? HA! ...I WILL GO AT FEAST DAY. SIT DOWN, YOU'RE EATING WITH ME. HOT, AND NO ARGUING."
  journal "CARRY THE HERMIT'S LETTER TO {SIS} IN {HOME}."
  do take "A HERMIT'S LETTER"
  do give "A HOT MEAL FROM {SIS}" meat
  do remember sis "A NEW GOOSE! I'M GOING AT FEAST DAY. I'LL TAKE A CAKE."
  opt "[[THANK YOU.|HE'LL BE GLAD.]]" -> report_b
)DSL";

std::string subst(std::string s, const std::string& from, const std::string& to) {
  for (size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + to.size())) s.replace(p, from.size(), to);
  return s;
}

// the whole template: the head, the stops' roles, every stop in every slot (blocks keep the drawn ones), the tail
const char* buildBody() {
  static std::string B;
  if (!B.empty()) return B.c_str();
  B = kHead;
  // the places: dwellings (ENGINE lane), else the small settlements round about (fb)
  for (int k = 1; k <= 3; k++) {
    const std::string K = std::to_string(k);
    B += "{!fb\nrole st" + K + " site dwelling near\n}\n{?fb\nrole st" + K + " site village near\n}\n";
  }
  // the people: each slot's person, of the sex its stop needs (the chair's speaker is the neighbour who found her)
  static const char* slotFlag[3] = {"v_", "tw_", "x_"};
  static const char* sexOf[S_COUNT] = {"female", "male", "[[male|female]]", "female", "male", "female", "male", "female"};
  for (int k = 1; k <= 3; k++) {
    const std::string K = std::to_string(k);
    for (int s = 0; s < S_COUNT; s++) {
      B += std::string("{?") + slotFlag[k - 1] + kStopNames[s] + "\n";
      B += "role p" + K + " person " + sexOf[s] + " at st" + K + "\n";
      if (s == S_CHAIR) B += "role dead" + K + " person female at st" + K + "\n";
      B += "}\n";
    }
  }
  // the deliveries' receivers (at most one delivery stop a round: prep)
  B += "{?v_chair|tw_chair|x_chair\nrole niece person female at home\n}\n";
  B += "{?v_letter|tw_letter|x_letter\nrole sis person female at home\n}\n";
  B += kTail;
  // the walk to each stop, then the stop
  for (int k = 1; k <= 3; k++) {
    const std::string K = std::to_string(k), N = k < 3 ? "go" + std::to_string(k + 1) : "report";
    B += "\nstage go" + K + "\n  goal goto st" + K + "\n  journal \"THE ROUND, HOUSE " + K + " OF 3, {ST" + K + ".DIR}. KNOCK, AND SEE WHO ANSWERS.\"\n  then meet" + K + "\n";
    for (int s = 0; s < S_COUNT; s++) {
      B += std::string("{?") + slotFlag[k - 1] + kStopNames[s] + "\n";
      B += subst(subst(kStopBody[s], "%K", K), "%N", N);
      B += "}\n";
    }
  }
  B += "{?v_chair|tw_chair|x_chair\n";
  B += kNiece;
  B += "}\n{?v_letter|tw_letter|x_letter\n";
  B += kSister;
  B += "}\n";
  return B.c_str();
}

enum { V_OFFICE, V_CARE, V_NEIGHBOUR };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const int stops[3] = {c.variant, c.twist, c.extra};
  const Role t = (Role)c.trade;
  const int voice = (t == Role::Jarl || t == Role::Guard || t == Role::Fisher) ? V_OFFICE : t == Role::Priest ? V_CARE : V_NEIGHBOUR;
  // the delivery (the chair's knitting or the hermit's letter) offered at the report
  int deliv = -1, dslot = 0;
  for (int k = 0; k < 3; k++) if (stops[k] == S_CHAIR || stops[k] == S_LETTER) { deliv = stops[k]; dslot = k + 1; }
  if (deliv == S_CHAIR) {
    P["delivopt"] = "  opt \"HER NIECE SHOULD HAVE THIS SCARF.\" if is deliv 1 -> niece";
    P["notdeliv"] = "if novar deliv 1 ";
  } else if (deliv == S_LETTER) {
    P["delivopt"] = "  opt \"I CARRY A LETTER FOR {SIS}.\" if is deliv 1 if novar goose 1 -> sister\n  opt \"I CARRY A LETTER FOR {SIS}.\" if is goose 1 -> sister_g";
    P["notdeliv"] = "if novar deliv 1 ";
  } else { P["delivopt"] = ""; P["notdeliv"] = ""; }
  P["delivstages"] = "";
  // the niece's aunt: the chair slot's dead keeper
  if (deliv == S_CHAIR) P["delivstages"] = "";
  (void)dslot;
  // the stubborn's second line, by the world
  P["stubborn"] = c.season == 3 ? "I WILL NOT LEAVE FOR A BAD WINTER." : c.war ? "I HAVE SEEN THREE WARS. I STAY." : "I WILL NOT LEAVE NOW. NOT FOR YOU, NOT FOR ANYONE.";
  std::vector<std::string> R;
  switch (t) {
    case Role::Jarl: R = {c.war ? "THE ARMIES WALKED THROUGH THE OUTER FARMS TWICE. NOBODY HAS COUNTED WHAT IS LEFT OUT THERE."
                                : "THE SNOWS CLOSED THE HILL TRACKS A MONTH AGO. I WANT TO KNOW WHO IS STILL BREATHING UP THERE."}; break;
    case Role::Priest: R = {"I BURY THE ONES WHO COME TO ME. IT IS THE ONES WHO NEVER COME THAT KEEP ME AWAKE AT NIGHT."}; break;
    case Role::Guard: R = {"THE WATCH PATROLS THE WALLS. NOBODY PATROLS THE LONELY HOUSES, AND THEY ARE THE FIRST THE RAIDERS HIT."}; break;
    case Role::Farmer: R = {"THE OLD MAN AT THE END OF THE TRACK NEVER MISSED A MARKET DAY. HE HAS MISSED SIX. LOOK IN ON HIM, AND THE OTHERS."}; break;
    case Role::Innkeeper: R = {"A CARTER SAYS THE CHIMNEYS PAST THE FAR HOUSES HAVE GONE COLD. HE MAY BE WRONG. HE IS ALSO DRUNK."}; break;
    case Role::Fisher: R = {"THE OUTLYING HOUSES ON THE SHORE HAVE NOT SENT A BOAT IN SINCE THE STORMS. SOMEBODY SHOULD WALK OUT AND SEE."}; break;
    default: R = {"MY OLD NEIGHBOURS LIVE ON THE FAR SLOPES AND SELDOM ASK FOR HELP. THEY ARE OWED A VISIT, AND WOULD DENY IT."}; break;
  }
  if (c.season == 3 && t != Role::Jarl) R.push_back("THE HUNGER IS WORST WHERE THE LAND IS THIN. SOMEBODY SHOULD LOOK BEFORE IT BECOMES A LIST.");
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"VISIT EACH HOUSE AND BRING BACK THE NEWS.", "KNOCK AT EVERY DOOR AND COUNT THE SMOKE.", "LOOK IN ON THEM AND DO WHAT YOU CAN."});
  switch (voice) {
    case V_OFFICE:
      P["all"] = "THE COUNT IS WHOLE. IT IS RARE. I WILL ENTER IT IN THE BOOKS.";
      P["most"] = "ONE DOOR OPEN. THE REST ARE KNOWN. A GOOD COUNT.";
      P["some"] = "A FAIR REPORT. GOOD ENOUGH FOR NOW.";
      P["none"] = "A WALK, AT LEAST. THEY KNOW WE LOOK.";
      break;
    case V_CARE:
      P["all"] = "EVERY DOOR ANSWERED. BLESS YOU. I WILL SLEEP TONIGHT FOR ONCE.";
      P["most"] = "ALL BUT ONE. I WILL GO TO THAT ONE MYSELF.";
      P["some"] = "SOME HELP IS BETTER THAN NONE. THANK YOU.";
      P["none"] = "YOU WENT. THAT COUNTS FOR SOMETHING.";
      break;
    default:
      P["all"] = "ALL OF THEM? GO ON, TELL ME ALL OF IT. SIT. FROM THE START.";
      P["most"] = "ALMOST ALL. THAT IS A GOOD ROUND.";
      P["some"] = "HALF IS HALF. THE REST WILL KEEP.";
      P["none"] = "...WELL. AT LEAST YOU WENT.";
      break;
  }
  std::vector<std::string> T = {"THE LONG ROUND", "STILL OUT THERE", "A KNOCK AT EACH DOOR", "THE FAR HOUSES", "THE LAST HOUSES", "COUNT THE SMOKE",
                                "THE SILENT HOMESTEADS"};
  if (c.season == 3) T.push_back("AFTER THE SNOW");
  P["title"] = T[(size_t)d.below((int)T.size())];
}

// the niece's aunt is the chair slot's dead keeper: the template's {DEADW} -> {DEAD<k>}
bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  const Role t = (Role)w.trade;
  if (t != Role::Priest && t != Role::Guard && t != Role::Jarl && t != Role::Innkeeper && t != Role::Farmer && t != Role::Villager && t != Role::Fisher) return false;
  // the stops: weights by the season and the world; the farmer's hook forces an old man (stubborn, liar or letter)
  int wgt[S_COUNT] = {0, 10, 8, 10, 10, 8, 10, 0};
  if (w.season == 3) { wgt[S_SNOW] = 24; wgt[S_STUBBORN] = 16; wgt[S_CHAIR] = 14; }
  if (w.season <= 1) wgt[S_BEES] = 16;
  if (w.war) { wgt[S_WOLVES] = 16; wgt[S_LIAR] = 16; }
  int pick[3];
  bool delivery = false;
  for (int k = 0; k < 3; k++) {
    int tot = 0;
    for (int s = 0; s < S_COUNT; s++) {
      bool used = false;
      for (int j = 0; j < k; j++) used |= pick[j] == s;
      if (used || ((s == S_CHAIR || s == S_LETTER) && delivery)) continue;
      tot += wgt[s];
    }
    int roll = d.below(tot > 0 ? tot : 1);
    pick[k] = S_STUBBORN;
    for (int s = 0; s < S_COUNT; s++) {
      bool used = false;
      for (int j = 0; j < k; j++) used |= pick[j] == s;
      if (used || ((s == S_CHAIR || s == S_LETTER) && delivery) || !wgt[s]) continue;
      if (roll < wgt[s]) { pick[k] = s; break; }
      roll -= wgt[s];
    }
    if (pick[k] == S_CHAIR || pick[k] == S_LETTER) delivery = true;
  }
  if (t == Role::Farmer) {
    bool old = false;
    for (int k = 0; k < 3; k++) old |= pick[k] == S_STUBBORN || pick[k] == S_LIAR || pick[k] == S_LETTER;
    if (!old) pick[2] = pick[0] == S_LIAR || pick[1] == S_LIAR ? S_STUBBORN : S_LIAR;
  }
  out.variant = pick[0]; out.twist = pick[1]; out.extra = pick[2];
  return true;
}

JobCtx walk(Role t, int a, int b, int c3, int season, bool fb = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = a; c.twist = b; c.extra = c3; c.season = season; c.fallback = fb;
  return c;
}

}  // namespace

const KindDef& kindRounds() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "rounds";
    k.title = "THE LONG ROUND";
    k.lead = "WANTS THE FAR HOUSES LOOKED IN ON";
    // the niece's aunt: whichever slot drew the chair
    static std::string body;
    body = buildBody();
    // {DEADW} is the chair's dead keeper; the chair may be in any slot: each slot's niece line is kept by its block
    {
      const std::string niece = kNiece;
      std::string withSlots;
      static const char* slotFlag[3] = {"v_chair", "tw_chair", "x_chair"};
      for (int s = 0; s < 3; s++) withSlots += std::string("{?") + slotFlag[s] + "\n" + subst(niece, "{DEADW}", "{DEAD" + std::to_string(s + 1) + "}") + "}\n";
      body = subst(body, kNiece, withSlots);
    }
    k.body = body.c_str();
    k.variants = {kStopNames[0], kStopNames[1], kStopNames[2], kStopNames[3], kStopNames[4], kStopNames[5], kStopNames[6], kStopNames[7]};
    k.twists = k.variants;
    k.extras = k.variants;
    k.trades = {(int)Role::Priest, (int)Role::Guard, (int)Role::Jarl, (int)Role::Innkeeper, (int)Role::Farmer, (int)Role::Villager, (int)Role::Fisher};
    k.fallback = true;
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Priest, S_SNOW, S_CHAIR, S_STUBBORN, 3), walk(Role::Farmer, S_WOLVES, S_LETTER, S_LIAR, 1),
               walk(Role::Jarl, S_BIRTH, S_BEES, S_CHAIR, 0), walk(Role::Innkeeper, S_LIAR, S_STUBBORN, S_LETTER, 2),
               walk(Role::Villager, S_BEES, S_WOLVES, S_BIRTH, 1)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
