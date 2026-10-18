// P1b radiant job kind 5: THE LAST ROAD HOME (content: jobs_a.md section 5; judge notes resolved). JOBS lane.
//
// Carry the dead to their kin. Two shapes, by the giver's reason: SITE (the body lies at a cave or a ruin that still
// holds what killed them: clear it, lift the remains, a burden that slows the walk) and TOWN (the dead already lie in
// the giver's cellar: the road alone; winter wolves smell the load). The kin's settlement is named in the pitch. The cause
// of death keys the reason and the line at the body (snow, the beasts, battle, the sea, misadventure, illness). At the
// kin's door: grateful kin (A); THE LETTER (B, misadventure only: the dead lost the family's savings at dice and was too
// ashamed to write: a kind lie, or the truth); THE ESTRANGED (C: "we do not know them": leave the body at the temple,
// pay for a grave, or argue with a name the room has heard of). The dog of the first draft is cut (judge: no animal
// follower in the engine); the description tags and the reason that contradicted itself are fixed; B has its own pay line.
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
{!v_illness
role site site [[=sitetype]] near
}
role kintown site [[village|town]] near
role dead person [[male|female]] at home
role kin person [[male|female]] at kintown
{?tw_estranged
role priest person [[male|female]] at kintown
}

stage start
  talk giver
  say "[[=reason]] [[=ask]] [[THE KIN WILL PAY THE CARRIER.|THEY'LL PAY YOU AT THE DOOR.|{KINTOWN} WILL SEE YOU PAID.]]"
  opt "[[I'LL CARRY {DEAD.HIM} HOME.|I'LL TAKE {DEAD.HIM} THE LAST ROAD.|SOMEONE SHOULD. I WILL.]]" -> [[=first]]
  opt "[[I CAN'T. NOT THIS.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

{!v_illness
stage go
  goal goto site
  journal "{DEAD} LIES DEAD AT {SITE}, {SITE.DIR}. BRING {DEAD.HIM} TO {DEAD.HIS} KIN IN {KINTOWN}."
  then clear

stage clear
  goal kill 3 [[=foe]] in site
  journal "WHATEVER KILLED {DEAD} STILL HOLDS {SITE}. CLEAR THE WAY TO THE BODY."
  then lift

stage lift
  goal goto kintown
  journal "CARRY {DEAD}'S BODY TO {DEAD.HIS} KIN IN {KINTOWN}, {KINTOWN.DIR}. THE LOAD IS HEAVY. GO SLOWLY."
  do notice "[[=bodya]]"
  do notice "[[=bodyb]]"
  do give "{DEAD}'S BODY" cloak heavy
  then door
}

{?v_illness
stage carry
  goal goto kintown
  journal "CARRY {DEAD}'S BODY, WRAPPED IN SAILCLOTH, TO {DEAD.HIS} KIN IN {KINTOWN}, {KINTOWN.DIR}. THE LOAD IS HEAVY."
  do give "{DEAD}'S BODY" cloak heavy
  do notice "[[NOT FAR NOW. HOLD ON.|ONE FOOT AFTER ANOTHER.|LONG ROAD. {DEAD} IS PATIENT.]]"
{?s_winter
  do ambush wolf 3
}
  then door
}

{?tw_straight
stage door
  talk kin
  say "YOU BROUGHT {DEAD.HIM} HOME. ...WE'LL BURY {DEAD.HIM} BESIDE KIN. [[=pyre]] TAKE THIS. IT IS ALL WE SAVED, AND IT IS YOURS."
  journal "{DEAD}'S KIN LIVE IN {KINTOWN}. BRING {DEAD.HIM} TO THEIR DOOR."
  opt "[[REST WELL, {DEAD}.|{DEAD.HE} IS HOME NOW.]]" -> home_end
}

{?tw_letter
stage door
  talk kin
  say "YOU BROUGHT {DEAD.HIM} HOME. ...A LETTER IN {DEAD.HIS} FIST, WITH OUR NAME ON IT. DID {DEAD.HE} SAY ANYTHING AT THE END?"
  journal "{DEAD}'S KIN LIVE IN {KINTOWN}. THE LETTER IN {DEAD.HIS} FIST IS ADDRESSED TO THEM. IT IS AN APOLOGY."
  opt "{DEAD.HE} SPOKE OF YOU. ONLY OF YOU." -> kind_lie
  opt "{DEAD.HE} WROTE THIS. READ IT." -> truth
}

{?tw_estranged
stage door
  talk kin
  say "WE DO NOT KNOW {DEAD.HIM}. ...WHY? {DEAD.HE} SOLD OUR FATHER'S FIELD. THEN {DEAD.HE} LEFT. WE BURIED AN EMPTY BED TEN YEARS AGO. TAKE {DEAD.HIM} AWAY."
  journal "{DEAD}'S KIN IN {KINTOWN} WILL NOT TAKE {DEAD.HIM}. THE TEMPLE OR A GRAVE, OR ARGUE WITH THEM."
  opt "[[THEN THE TEMPLE WILL TAKE {DEAD.HIM}.|TO THE TEMPLE, THEN.]]" -> temple
  opt "I'LL PAY FOR A GRAVE MYSELF. (40 GOLD)" if gold 40 -> grave
  opt "{DEAD.HE} IS STILL YOUR OWN. LOOK." check fame 3 -> argue_win else temple

stage temple
  talk priest
  say "THE KIN WON'T? IT HAPPENS MORE THAN YOU'D THINK. THE PAUPERS' FUND WILL BURY {DEAD.HIM}, AND PAY YOU WHAT IT CAN FOR THE ROAD."
  journal "BRING {DEAD} TO {PRIEST} AT THE TEMPLE OF {KINTOWN}."
  opt "[[THANK YOU.|SAY THE WORDS FOR {DEAD.HIM}.]]" -> pauper

stage grave
  talk priest
  say "YOU'LL PAY FOR THE GRAVE YOURSELF? FOR A STRANGER? ...THEN {DEAD.HE} GETS A GOOD ONE, BY THE WALL, AND THE TEMPLE PAYS THE CARRIER IN FULL."
  journal "YOU PAID FOR {DEAD}'S GRAVE IN {KINTOWN}."
  do gold -40
  opt "[[A GOOD ONE, BY THE WALL.|THANK YOU.]]" -> grave_end
}

{?tw_straight
stage home_end
  end success
  journal "{DEAD} WAS CARRIED HOME TO {KINTOWN} AND BURIED BESIDE KIN."
  do take "{DEAD}'S BODY"
  do reward fair 110
  do give "A MEAL AT THE KIN'S TABLE" bread
  do remember kin "YOU BROUGHT {DEAD} HOME. WE WILL NOT FORGET YOUR FACE."
  do remember giver "{DEAD} IS HOME, IN {KINTOWN}. THANK YOU."
}

{?tw_letter
stage kind_lie
  end success
  say "THEN IT WAS WORTH THE WAITING. ...WHAT LITTLE IS LEFT IS YOURS. TAKE IT."
  journal "YOU TOLD {DEAD}'S KIN A KIND LIE. THEY WEPT WITH RELIEF, AND BURIED {DEAD.HIM} BESIDE KIN."
  do take "{DEAD}'S BODY"
  do reward fair 100
  do deed lied
  do remember kin "{DEAD} SPOKE OF US AT THE END. THAT IS ENOUGH."

stage truth
  end success
  say "...SORRY. AT LAST. ALL THOSE YEARS, AND IT WAS ONLY DICE AND SHAME. WE CAN LIVE WITH IT. WHAT LITTLE IS LEFT IS YOURS."
  journal "YOU SHOWED {DEAD}'S KIN THE LETTER. THE DEBT OF SHAME IS CLOSED, AND {DEAD} IS BURIED BESIDE KIN."
  do take "{DEAD}'S BODY"
  do reward fair 110
  do remember kin "THE LETTER SAID SORRY. WE NEEDED THAT MORE THAN THE MONEY."
  do fact "A LETTER CARRIED HOME FROM {SITE} CLOSED AN OLD SHAME IN {KINTOWN}."
}

{?tw_estranged
stage argue_win
  end success
  say "...{DEAD.HIS} MOTHER WOULD HAVE WEPT. ALL RIGHT. BRING {DEAD.HIM} IN. FIND A SPADE. AND EAT WITH US AFTER."
  journal "{DEAD}'S KIN TOOK {DEAD.HIM} IN AT LAST, AND BURIED {DEAD.HIM} BESIDE THE FATHER WHOSE FIELD {DEAD.HE} SOLD."
  do take "{DEAD}'S BODY"
  do reward fair 110
  do give "A MEAL AT THE KIN'S TABLE" bread
  do remember kin "YOU WOULDN'T LET US TURN {DEAD.HIM} AWAY. ...GOOD."

stage pauper
  end success
  journal "{DEAD} WAS BURIED FROM THE PAUPERS' FUND AT THE TEMPLE OF {KINTOWN}. THE KIN DID NOT COME."
  do take "{DEAD}'S BODY"
  do reward fair 75
  do remember priest "A PAUPER'S GRAVE, BUT A GRAVE. YOU DID RIGHT TO BRING {DEAD.HIM}."

stage grave_end
  end success
  journal "YOU PAID FOR {DEAD}'S GRAVE. THE TEMPLE OF {KINTOWN} PAID THE CARRIER IN FULL."
  do take "{DEAD}'S BODY"
  do reward fair 110
  do remember priest "A STRANGER PAID FOR A STRANGER'S GRAVE. I WROTE IT IN THE BOOK."
  do fact "A STRANGER PAID FOR {DEAD}'S GRAVE IN {KINTOWN}."
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S LAST ROAD."
)DSL";

enum { V_SNOW, V_BEASTS, V_BATTLE, V_SEA, V_MISADV, V_ILLNESS };
enum { T_STRAIGHT, T_LETTER, T_ESTRANGED };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const bool town = c.variant == V_ILLNESS;
  P["first"] = town ? "carry" : "go";
  P["sitetype"] = (c.variant == V_SNOW || c.variant == V_BEASTS || c.variant == V_SEA) ? "cave" : "ruin";
  P["foe"] = P["sitetype"] == "ruin" ? "undead" : "any";
  P["pyre"] = d.pick({"A PYRE AT DUSK, AND EAT WITH US.", "THE WHOLE STREET WILL COME.", ""});
  std::vector<std::string> R, T = {"BRING THEM HOME", "THE LAST ROAD HOME", "THE LONG WAY HOME", "HOME AT LAST"};
  switch (c.variant) {
    case V_SNOW:
      R = {"THE SNOW TOOK {DEAD} IN THE PASS. WE FOUND {DEAD.HIS} PACK AT THE MOUTH OF {SITE}. THE THINGS INSIDE WON'T LET US IN. {DEAD.HIS} PEOPLE ARE IN {KINTOWN}."};
      T.push_back("WHAT THE SNOW KEPT");
      P["bodya"] = "STIFF WITH FROST. THIN AS A RAKE.";
      P["bodyb"] = "ICE IN THE LASHES. LIKE SLEEP.";
      break;
    case V_BEASTS:
      R = {"{DEAD} WENT INTO {SITE} AND WHATEVER LAIRS THERE FOUND {DEAD.HIM}. {DEAD.HIS} KIN IN {KINTOWN} MUST BE TOLD, AND WHAT IS LEFT BROUGHT."};
      P["bodya"] = "CLAW SIGN ALL AROUND. WRAP IT TIGHT.";
      P["bodyb"] = "THE CLOAK HELD. IT'LL BE A SHROUD.";
      break;
    case V_BATTLE:
      if (c.trade == (int)Role::Jarl) R = {"{DEAD} WAS SWORN TO MY HALL AND DIED IN MY SERVICE AT {SITE}. {DEAD.HE} DESERVES A HOMECOMING IN {KINTOWN}."};
      else R = {"A PATROL WENT TO {SITE} AND {DEAD} DID NOT RETURN. I WON'T LEAVE THAT BODY TO THE CROWS. {DEAD.HIS} PEOPLE ARE IN {KINTOWN}."};
      if (c.war) R.push_back("A SOLDIER FELL AT {SITE} WHEN THE RAIDERS HIT. NO KIN HERE. THE ROLL NAMES {DEAD}, AND SAYS {DEAD.HIS} PEOPLE ARE IN {KINTOWN}.");
      T.push_back("HOME ON A SHIELD");
      P["bodya"] = "THE HAND STILL HOLDS THE SWORD.";
      P["bodyb"] = "HELM DENTED. BOOTS STILL LACED.";
      break;
    case V_SEA:
      R = {"THE SEA GAVE {DEAD} BACK AT {SITE}, WRAPPED IN WEED AND NO BREATH. THE CREW'S ROLL SAYS {KINTOWN}. SOMEONE MUST CARRY {DEAD.HIM} THERE."};
      T.push_back("THE SEA GAVE BACK");
      P["bodya"] = "SALT-WHITE AND HEAVY WITH WEED.";
      P["bodyb"] = "A SHELL IN THE POCKET. LUCK, ONCE.";
      break;
    case V_MISADV:
      R = {"{DEAD} DIED AT {SITE}. NO ONE HERE HAS THE STOMACH TO FETCH {DEAD.HIM}. {DEAD.HIS} PEOPLE WAIT IN {KINTOWN}.",
           "{DEAD} FOLLOWED A RUMOUR TO {SITE} AND PAID FOR IT. THE FAMILY IN {KINTOWN} HAS WRITTEN TWICE. I WILL SEND {DEAD.HIM} HOME INSTEAD OF A LIE."};
      P["bodya"] = c.twist == T_LETTER ? "A LETTER IN THE FIST. NEVER SENT." : "A SPRIG OF HEATHER, PRESSED IN A COAT.";
      P["bodyb"] = c.twist == T_LETTER ? "IT IS ADDRESSED TO KIN IN {KINTOWN}." : "WRAP IT. CARRY IT. GENTLY.";
      break;
    default:   // illness (the TOWN shape)
      if (c.trade == (int)Role::Fisher) R = {"THE FEVER TOOK {DEAD} AT THE DOCKS. THE ROLL SAYS {KINTOWN}. THE BOAT SAILS WITHOUT {DEAD.HIM}. WILL YOU CARRY {DEAD.HIM} THE LAST ROAD?"};
      else {
        R = {"{DEAD} CAME HERE ILL AND DID NOT WAKE. THE BODY LIES IN MY CELLAR. {DEAD.HIS} PEOPLE LIVE IN {KINTOWN}, AND I WON'T BURY {DEAD.HIM} AMONG STRANGERS.",
             "A TRAVELLER FELL SICK ON THE ROAD AND DIED ON MY BEST BED. {DEAD} HAD A NAME CARVED INSIDE {DEAD.HIS} BELT: {KINTOWN}. THE BODY WAITS IN THE CELLAR."};
        T.push_back("THE CELLAR GUEST");
      }
      P["bodya"] = ""; P["bodyb"] = "";
      break;
  }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"BRING {DEAD.HIM} TO {DEAD.HIS} PEOPLE IN {KINTOWN}.", "CARRY {DEAD.HIM} THE LAST ROAD, TO {KINTOWN}.",
                     "GIVE {DEAD.HIM} THE FUNERAL {DEAD.HE} DESERVES."});
  P["title"] = T[(size_t)d.below((int)T.size())];
  (void)town;
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Priest: out.variant = V_MISADV; break;
    case Role::Innkeeper: out.variant = V_ILLNESS; break;
    case Role::Hunter: out.variant = w.season == 3 ? V_SNOW : V_BEASTS; break;
    case Role::Guard: case Role::Jarl: out.variant = V_BATTLE; break;
    case Role::Fisher: if (!w.coast) return false; out.variant = w.port && d.chance(40) ? V_ILLNESS : V_SEA; break;
    default: return false;
  }
  // SITE: A 55, B 15 (misadventure only, else A), C 30; TOWN: A 70, C 30
  const int roll = d.below(100);
  out.twist = T_STRAIGHT;
  if (out.variant == V_ILLNESS) { if (roll >= 70) out.twist = T_ESTRANGED; }
  else if (roll >= 70) out.twist = T_ESTRANGED;
  else if (roll >= 55 && out.variant == V_MISADV) out.twist = T_LETTER;
  return true;
}

JobCtx walk(Role t, int variant, int twist, int season, bool coast = false, bool war = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.season = season; c.coast = coast; c.war = war;
  return c;
}

}  // namespace

const KindDef& kindHomeward() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "homeward";
    k.title = "THE LAST ROAD HOME";
    k.lead = "NEEDS SOMEONE TO CARRY THE DEAD HOME";
    k.body = kBody;
    k.variants = {"snow", "beasts", "battle", "sea", "misadv", "illness"};
    k.twists = {"straight", "letter", "estranged"};
    k.trades = {(int)Role::Priest, (int)Role::Innkeeper, (int)Role::Hunter, (int)Role::Guard, (int)Role::Jarl, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Priest, V_MISADV, T_LETTER, 1), walk(Role::Innkeeper, V_ILLNESS, T_ESTRANGED, 3), walk(Role::Hunter, V_SNOW, T_STRAIGHT, 3),
               walk(Role::Jarl, V_BATTLE, T_ESTRANGED, 2, false, true), walk(Role::Fisher, V_SEA, T_STRAIGHT, 0, true), walk(Role::Hunter, V_BEASTS, T_STRAIGHT, 1)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
