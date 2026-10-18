// P1b radiant job kind 3: RUN-DOWN (content: jobs_a.md section 3; judge notes resolved). JOBS lane.
//
// Track a fugitive, hear their side, decide. The trail starts with a WITNESS in a neighbouring settlement (never the
// hideout itself: the pitch names the village, the witness reveals the hideout). At the hideout the fugitive waits and
// talks; three ways out in every variant: bring them back (they walk home with you), let them go and report "not found"
// (half pay; each variant's aftermath has its own shape), or take their part and carry what they hand you to the giver
// (who yields to a name the room has heard of, fame 4, or hardens). Six fugitives, six shapes of story:
//   guilty   (guard, lord)   an escaped prisoner, guilty of the purse, who ran to stand at a brother's grave
//   prentice (smith)         a bound apprentice who wants to be seen, with a knife made in secret
//   clerk    (merchant)      a clerk who fled the GIVER, who cooked the books: the player works that out from the lines
//   novice   (priest)        a novice running toward the sick, resting at the old shrine on the way
//   catthief (innkeeper)     a guest wrongly accused of stealing the inn's cat (the cat chose them)
//   sailor   (fisher, coast) a deckhand who ran from a lamp in the fog
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
role trail site village near
{?v_guilty|v_prentice|v_clerk|v_novice
role hide site ruin near
}
{?v_catthief|v_sailor
role hide site cave near
}
role witness person [[male|female]] at trail
role fug person [[=fugsex]] in hide

stage start
  talk giver
  say "[[=reason]] [[=trailline]] [[=ask]] [[I PAY FAIRLY.|THE PAY IS FAIR.]]"
  opt "[[I'LL FIND {FUG.HIM}.|I'LL PICK UP THE TRAIL.|LEAVE IT TO ME.]]" -> witness_go
  opt "[[NOT A HUNT FOR ME.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

stage witness_go
  talk witness
{?v_guilty
  say "A GAUNT ONE RAN PAST HERE, BAREFOOT. RUNNING TO SOMETHING, I'D SAY, NOT FROM IT. THEY ASKED ME THE WAY TO {HIDE}."
}
{?v_prentice
  say "A YOUNG ONE CAME BY, TOOLS CLINKING. SOOT TO THE ELBOWS. A SMITH'S, I'D SAY. TOOK MY BREAD AND LEFT A NAIL FOR IT. ASKED FOR THE ROAD TO {HIDE}."
}
{?v_clerk
  say "A CLERK BEGGED A RIDE ON MY CART TOWARD {HIDE}. INK ON THEIR HANDS, A BAG HELD TIGHT. KEPT LOOKING BACK TOWARD {HOME}."
}
{?v_novice
  say "A GREY ROBE WADED THE FORD. BOUGHT NO FOOD. GAVE ME A BLESSING. ASKED FOR THE ROAD TO {HIDE}, WHERE THE OLD SHRINE IS."
}
{?v_catthief
  say "A STRANGER WENT BY WITH A CAT. THE CAT WALKED. NOBODY CARRIED HER. HAPPIEST CAT I EVER SAW. THEY ASKED FOR THE ROAD TO {HIDE}."
}
{?v_sailor
  say "A SAILOR WADED OFF THE QUAY AT DAWN, SHAKING. KEPT LOOKING AT THE FOG. ASKED ME THE WAY TO {HIDE}."
}
  journal "{FUG} RAN FROM {HOME}. ASK ALONG THE ROAD AT {TRAIL}, {TRAIL.DIR}, WHICH WAY {FUG.HE} WENT."
  opt "[[THANK YOU.|THAT HELPS.|WHICH WAY IS THAT?]]" -> go

stage go
  goal goto hide
  journal "THE TRAIL OF {FUG} LEADS TO {HIDE}, {HIDE.DIR}. FIND {FUG.HIM} AND HEAR {FUG.HIM} OUT."
  then side

stage side
  talk fug
{?tw_wolf
  say "QUIET! THERE'S A WOLF ASLEEP IN THE BACK. DON'T WAKE IT. [[=sideline]]"
}
{!tw_wolf
  say "[[=sideline]]"
}
  journal "YOU FOUND {FUG} AT {HIDE}. DECIDE WHAT TO DO."
  opt "[[COME BACK WITH ME.|YOU'RE COMING HOME.]]" -> bring
  opt "[[GO. I NEVER FOUND YOU.|RUN. I'LL SAY THE TRAIL WENT COLD.]]" -> letgo
  opt "[[=partopt]]" -> part

stage bring
  goal escort fug to home walk
{?v_guilty
  say "LET ME TELL HIM I'M GOING. ...THERE. I'LL COME."
}
{!v_guilty
  say "[[FINE. I'LL COME.|...ALL RIGHT. LEAD ON.]]"
}
  journal "WALK {FUG} BACK TO {GIVER} IN {HOME}."
  do follow fug
  then back
  else fug_lost

stage back
  talk giver
  say "[[=backline]]"
  journal "TELL {GIVER} THAT {FUG} IS HOME."
  do stay fug
  do moves fug home
  opt "[[HERE {FUG.HE} IS.|AS YOU ASKED.]]" -> brought

stage letgo
  talk giver
  say "[[=letline]]"
  journal "TELL {GIVER} THE TRAIL WENT COLD. {FUG} IS GONE."
  opt "[[THE TRAIL WENT COLD.|NO TRACE AT ALL.]]" -> let_end

stage part
  talk giver
  say "[[=partline]]"
  journal "CARRY [[=token]] TO {GIVER} IN {HOME}, AND SPEAK FOR {FUG}."
  do give "[[=token]]" [[=tokenicon]]
  opt "[[HEAR {FUG.HIM} OUT. READ THIS.|THERE'S MORE TO IT. LOOK.]]" check fame 4 -> yields else hardens

stage brought
  end success
  journal "{FUG} IS BACK IN {HOME}. {GIVER} PAID IN FULL."
  do reward fair 100
  do remember giver "[[=broughtmem]]"
  do remember fug "[[=fugbroughtmem]]"

stage let_end
  end success
  say "[[=letpay]]"
  journal "YOU LET {FUG} GO AND TOLD {GIVER} THE TRAIL WENT COLD. HALF PAY."
  do reward fair 50
  do remember giver "[[=letmem]]"
[[=letfx]]

stage yields
  end success
  say "[[=yieldline]]"
  journal "{GIVER} HEARD {FUG}'S SIDE AND YIELDED. {FUG} IS CLEARED, AND BACK IN {HOME}."
  do take "[[=token]]"
  do reward fair 130
  do moves fug home
  do remember fug "[[=fugseen]]"
  do remember giver "[[=yieldmem]]"
  do fact "{FUG} WAS RUN DOWN FROM {HOME}, AND HEARD, AND CLEARED."

stage hardens
  end success
  say "[[=hardline]]"
  journal "{GIVER} WOULD NOT HEAR {FUG}'S SIDE. YOU WERE PAID LITTLE, AND {FUG} WAS NOT SEEN AGAIN."
  do take "[[=token]]"
  do reward fair 60
  do remember giver "[[=hardmem]]"

stage fug_lost
  end fail
  say "{FUG} DID NOT LIVE TO SEE {HOME} AGAIN. THE ROAD BACK WAS LONGER THAN THE ROAD OUT."
  journal "{FUG} DIED ON THE WAY BACK TO {HOME}."
  do remember giver "{FUG} DIED ON THE ROAD HOME. I WANTED ANSWERS, NOT A GRAVE."

stage decline
  end fail
  journal "YOU TURNED DOWN THE HUNT FOR {FUG}."
)DSL";

enum { V_GUILTY, V_PRENTICE, V_CLERK, V_NOVICE, V_CAT, V_SAILOR };

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  // the cat thief's guest and the sailor may be either; the novice and the apprentice too
  P["fugsex"] = d.chance(50) ? "female" : "male";
  P["trailline"] = d.pick({"A HERDER NEAR {TRAIL} SAW {FUG.HIM} PASS.", "THE TRAIL STARTS AT {TRAIL}, {TRAIL.DIR}.",
                           "THEY SAW {FUG.HIM} AT {TRAIL}."});
  std::vector<std::string> R, T = {"RUN-DOWN", "THE RUNAWAY", "FIND THE RUNAWAY", "WALKED OUT", "THE EMPTY BUNK", "THE COLD TRAIL", "BRING THEM BACK"};
  std::vector<std::string> asks = {"FIND {FUG.HIM}. HEAR {FUG.HIM} OUT. THEN DO WHAT YOU THINK IS RIGHT.", "BRING {FUG.HIM} BACK, OR TELL ME WHY NOT.",
                                   "I NEED TO KNOW WHERE {FUG.HE} WENT, AND WHY."};
  switch (c.variant) {
    case V_GUILTY:
      R = {"{FUG} SLIPPED THE CELL WHILE THE KEY WAS AT SUPPER. FIND {FUG.HIM}.",
           "{FUG} STOLE A PURSE AND OWED A SEASON IN THE PIT. WITH TWO MONTHS LEFT, {FUG.HE} IS GONE. I WANT TO KNOW WHY."};
      if (c.season == 3) R.push_back("{FUG} BROKE OUT IN THE FIRST SNOW, WITH NO COAT. A RUNNER NEEDS SHELTER IN THIS WEATHER, AND I NEED {FUG.HIM} BACK.");
      P["sideline"] = "I DID IT. I TOOK THE PURSE. BUT MY BROTHER DIED IN MY SECOND WEEK, AND THEY WOULD NOT LET ME BURY HIM. HE LIES HERE, IN THE OLD CAIRN. GIVE ME ONE NIGHT. THEN I GO BACK.";
      P["partopt"] = "I'LL SPEAK FOR YOU.";
      P["token"] = "A DEAD BROTHER'S GRAVE-RING"; P["tokenicon"] = "ring";
      P["backline"] = "BACK IN BY DUSK. THANK YOU. ...A BROTHER IN THAT CAIRN, AND NOBODY TOLD ME? I DIDN'T KNOW.";
      P["letline"] = "NO TRACE? THE WOODS ARE BIG. WELL. YOU TRIED, AND I PAY FOR TRYING.";
      P["letpay"] = "HALF THE FEE, FOR A COLD TRAIL. IT'S FAIR.";
      P["letmem"] = "THE PRISONER WALKED BACK IN AT DAWN. NOBODY ASKED. NOBODY HAD TO.";
      P["letfx"] = "  do deed mercy";
      P["partline"] = "A RING? WHOSE RING? ...SPEAK, THEN. WHY SHOULD A THIEF HAVE MORE MERCY THAN THE LAW GIVES?";
      P["yieldline"] = "A GRAVE WITH NO MOURNER. HM. A SEASON OF WALL-MENDING AT THE BURIAL GROUND, THEN. NOT THE PIT. TELL {FUG.HIM}.";
      P["yieldmem"] = "{FUG} MENDS THE BURIAL-GROUND WALL NOW. MENDS IT WELL, TOO.";
      P["fugseen"] = "I'LL MEND IT WELL. THE WALL, I MEAN. HE'D HAVE LAUGHED.";
      P["hardline"] = "A SAD STORY. STILL A THIEF. HERE IS WHAT THE TRAIL WAS WORTH.";
      P["hardmem"] = "A SAD STORY. STILL A THIEF. I DON'T REGRET IT.";
      P["broughtmem"] = "{FUG} IS BACK IN THE CELL. QUIET. ASKED ME TO THANK YOU.";
      P["fugbroughtmem"] = "YOU LET ME SAY GOODBYE FIRST. THAT WAS ENOUGH.";
      break;
    case V_PRENTICE:
      R = {"{FUG} CAME HERE A CHILD AND LEARNED THE BELLOWS. LAST NIGHT {FUG.HE} TOOK A TOOL BELT AND LEFT A NOTE I CAN'T READ."};
      if (c.season == 2) R.push_back("THE HARVEST RUSH IS ON, AND {FUG} HAS WALKED OUT OF MY FORGE. I DON'T PAY FOR SULKS.");
      P["sideline"] = "FOUR YEARS ON THE BELLOWS. NO HAMMER. NOT ONCE. I MADE THIS KNIFE IN SECRET, AT NIGHT. NOBODY EVER ASKED IF I COULD. I'LL CARRY COAL. I WON'T BE A BROOM.";
      P["partopt"] = "LET ME SHOW YOUR MASTER THAT KNIFE.";
      P["token"] = "THE APPRENTICE'S KNIFE"; P["tokenicon"] = "dagger";
      P["backline"] = "BACK TO THE BELLOWS. THAT'S LEARNING. ...SULKS ARE NOT.";
      P["letline"] = "NOT A TRACK? GOOD RIDDANCE. HALF THE FEE, AND DON'T EXPECT A DRINK.";
      P["letpay"] = "HALF, FOR A HUNT THAT FOUND NOTHING.";
      P["letmem"] = "A BENT NAIL TURNED UP AT MY DOOR. THEN A HOOK. A GOOD HOOK, TOO. I DON'T KNOW WHO.";
      P["letfx"] = "  do give \"A SMITH'S HOOK\" key";
      P["partline"] = "A KNIFE? FROM THE ONE WHO PUMPS MY BELLOWS? LET ME SEE IT.";
      P["yieldline"] = "THAT EDGE. I DID NOT TEACH THAT. ...THE BOND IS DONE. {FUG.HE} CAN WORK THE FORGE FOR WAGES, AND HAMMER WHAT {FUG.HE} LIKES.";
      P["yieldmem"] = "MY OLD APPRENTICE WORKS THE SECOND ANVIL NOW. FOR WAGES. GOOD WORK, TOO.";
      P["fugseen"] = "I HAVE A HAMMER NOW. AND A BENCH. THANK YOU.";
      P["hardline"] = "A KNIFE IS NOT A SWORD. NO. HERE'S A LITTLE FOR THE WALK.";
      P["hardmem"] = "A KNIFE IS NOT A SWORD. MY OLD APPRENTICE WILL LEARN THAT, SOMEWHERE ELSE.";
      P["broughtmem"] = "MY APPRENTICE IS BACK ON THE BELLOWS. SULKING. FINE.";
      P["fugbroughtmem"] = "BACK ON THE BELLOWS. ONE DAY THEY'LL ASK.";
      break;
    case V_CLERK:
      R = {"{FUG} KEPT MY BOOKS FOUR YEARS. THE TAX-MAN COMES THIS WEEK AND {FUG.HE} IS GONE. FIND {FUG.HIM} BEFORE HE ASKS."};
      if (c.season == 0) R.push_back("{FUG} TOOK THE LEDGERS AND LEFT THE COIN. I WANT THE LEDGERS BACK. NOTHING ELSE MATTERS.");
      asks = {"BRING {FUG.HIM} BACK, OR TELL ME WHY NOT."};
      P["sideline"] = "THE BOOKS WERE FALSE BEFORE I CAME. I WAS TOLD TO WRITE THEM THAT WAY, TO CHEAT THE TAX-MAN. I COPIED THE TRUE PAGES. HERE. I RAN WHEN HE CAME ASKING.";
      P["partopt"] = "GIVE ME THE TRUE PAGES.";
      P["token"] = "THE TRUE PAGES OF THE BOOKS"; P["tokenicon"] = "scroll";
      P["backline"] = "THE BOOKS WILL TELL. WELL DONE. VERY WELL DONE. THE CLERK AND I WILL TALK, IN PRIVATE.";
      P["letline"] = "NO TRACE? WELL. ...GOOD. I MEAN, A PITY. HALF, THEN.";
      P["letpay"] = "HALF THE FEE. AND YOU DIDN'T LOOK VERY HARD, DID YOU?";
      P["letmem"] = "YOU DIDN'T LOOK VERY HARD FOR THAT CLERK, DID YOU? I WONDER WHY.";
      P["letfx"] = "  do rumour";
      P["partline"] = "PAGES? WHAT PAGES? ...LET ME SEE THOSE.";
      P["yieldline"] = "HOW MUCH DID YOU READ? ...NO. HOW MUCH DO I OWE? THE TAX-MAN WILL HAVE IT BY MORNING. THE CLERK KEEPS THE POST.";
      P["yieldmem"] = "THE TAX-MAN HAS HIS PAGES. THE CLERK KEEPS MY BOOKS TRUE NOW. I SLEEP WORSE AND BETTER.";
      P["fugseen"] = "I DO THE BOOKS TRUE NOW. EVERY PAGE.";
      P["hardline"] = "A PAGE IS PAPER. WHO BELIEVES A RUNAWAY? TAKE THIS AND FORGET IT.";
      P["hardmem"] = "PAPER. RUNAWAYS. NOBODY BELIEVES THEM. REMEMBER THAT.";
      P["broughtmem"] = "THE CLERK LEFT AGAIN. A SHAME. I'LL FIND ANOTHER, A QUIETER ONE.";
      P["fugbroughtmem"] = "YOU BROUGHT ME BACK TO HIM. YOU KNOW WHAT HE IS NOW.";
      break;
    case V_NOVICE:
      R = {"{FUG} WAS A NOVICE AND HAS LEFT THE VOWS BEHIND. I DO NOT WANT {FUG.HIM} PUNISHED. I WANT {FUG.HIM} ASKED WHY.",
           "THE CLOISTER HAS NINE NOVICES AND EIGHT FILLED THEIR PLACES THIS MORNING. {FUG} IS THE NINTH, AND THE BED IS MADE."};
      asks = {"FIND {FUG.HIM}. HEAR {FUG.HIM} OUT. THEN DO WHAT YOU THINK IS RIGHT.", "I NEED TO KNOW WHERE {FUG.HE} WENT, AND WHY."};
      P["sideline"] = c.tier == 0 ? "I STOPPED HEARING THE GODS. SORRY. THE FARMS DOWN THE VALLEY HAVE FEVER, AND I WANTED TO NURSE THE SICK. I ONLY STOPPED HERE TO REST. I CAN'T SING. I CAN CARRY WATER."
                                  : "I STOPPED HEARING THE GODS. SORRY. THE LOWER STREETS HAVE THE FEVER, AND I WANTED TO NURSE THE SICK. I ONLY STOPPED HERE TO REST. I CAN'T SING. I CAN CARRY WATER.";
      P["partopt"] = "WRITE IT DOWN. I'LL CARRY IT.";
      P["token"] = "A NOVICE'S NOTE"; P["tokenicon"] = "letter";
      P["backline"] = "THE DOOR WAS OPEN. IT STILL IS. COME IN, CHILD. WE'LL TALK.";
      P["letline"] = "NO TRACE. I'LL LEAVE THE DOOR OPEN, ALL THE SAME.";
      P["letpay"] = "HALF, FOR YOUR FEET. THE GODS WILL KNOW THE REST.";
      P["letmem"] = "THEY SAY A NOVICE NURSES THE SICK DOWN THE VALLEY. CARRIES WATER. SINGS BADLY. I WONDER.";
      P["letfx"] = "  do rumour";
      P["partline"] = "A NOTE, IN THAT HAND? READ IT TO ME. MY EYES ARE OLD.";
      P["yieldline"] = "I ASKED THE WRONG QUESTION. NOT WHY THEY LEFT. WHERE THEY WERE GOING. LET {FUG} TEND THE SICK. WITH OUR BLESSING.";
      P["yieldmem"] = "OUR NOVICE KEEPS THE SICKROOM NOW, WITH A BASIN AND NO HYMNS. IT IS A KIND OF PRAYER.";
      P["fugseen"] = "I KEEP THE SICKROOM NOW. WATER, NOT HYMNS. THANK YOU.";
      P["hardline"] = "THE VOWS ARE NOT A MENU. I'M SORRY FOR IT. TAKE THIS FOR YOUR TROUBLE.";
      P["hardmem"] = "THE VOWS ARE NOT A MENU. I STAND BY THAT. MOSTLY.";
      P["broughtmem"] = "THE NOVICE IS HOME AGAIN, AND QUIET. TOO QUIET.";
      P["fugbroughtmem"] = "BACK IN THE CHOIR. I STILL CAN'T SING.";
      break;
    case V_CAT:
      R = {"A LODGER LEFT BEFORE DAWN WITH MY CAT AND NO COIN. THAT CAT KEPT MY CELLAR CLEAR NINE YEARS.",
           "MY CELLAR HAS RATS AGAIN. MY CAT IS GONE. SO IS THE GUEST IN ROOM THREE. DRAW YOUR OWN CONCLUSION."};
      T.push_back("THE CAT THIEF");
      P["sideline"] = "SHE FOLLOWED ME. I SWEAR. I'D NEVER STEAL A CAT. SHE STOLE ME. AND I PAID FOR THE ROOM WITH A RING. THE STABLE BOY KEPT IT, AND SCRAWLED ME THIS RECEIPT.";
      P["partopt"] = "GIVE ME THE BOY'S RECEIPT.";
      P["token"] = "THE STABLE BOY'S RECEIPT"; P["tokenicon"] = "letter";
      P["backline"] = "THERE SHE IS. LOOK AT HER. SPOILT. ...AND THE BOY SAYS YOU PAID WITH A RING, GUEST? THEN WE'RE SQUARE. SIT DOWN.";
      P["letline"] = "NO CAT, NO GUEST? PITY. THE RATS SEND THEIR THANKS.";
      P["letpay"] = "HALF THE FEE. THE RATS ARE DRINKING THE REST.";
      P["letmem"] = "A TIN MOUSE TURNED UP ON THE SILL OF ROOM THREE. FOR YOU, I THINK.";
      P["letfx"] = "  do give \"A TIN MOUSE\" gem";
      P["partline"] = "A RECEIPT? IN THE STABLE BOY'S HAND? ...LET ME SEE THAT.";
      P["yieldline"] = "THE STABLE BOY. I'LL HEAR OF THIS. ...THE CAT CHOSE. I WON'T ARGUE WITH A CAT. THE GUEST CAN STAY, AND SHE STAYS WITH THEM.";
      P["yieldmem"] = "THE GUEST AND THE CAT SIT BY MY FIRE EVERY NIGHT. THE BOY MUCKS OUT. THE RATS ARE GONE.";
      P["fugseen"] = "SHE SLEEPS ON MY FEET. WHAT CAN I DO?";
      P["hardline"] = "A RECEIPT IS PAPER. ...KEEP THE CAT, THEN, IF SHE WON'T COME. THERE'S YOUR LITTLE FEE.";
      P["hardmem"] = "A RECEIPT IS PAPER. THE CAT NEVER CAME BACK. I MISS HER.";
      P["broughtmem"] = "THE CAT IS HOME. THE GUEST IS PAID UP. THE BOY IS SORRY. GOOD.";
      P["fugbroughtmem"] = "SHE FOLLOWED ME HOME AGAIN. SHE ALWAYS WILL.";
      break;
    default:   // V_SAILOR
      R = {"{FUG} SIGNED ON FOR THE NEXT RUN, TOOK THE BOND MONEY AND WAS GONE BEFORE THE TIDE. WE SAIL SHORT-HANDED WITHOUT {FUG.HIM}.",
           "MY MATE SAYS {FUG} WAS AFRAID OF A FOG. I SAY A SAILOR WHO RUNS FROM FOG IS NO SAILOR. FIND {FUG.HIM}."};
      T.push_back("JUMPED SHIP");
      P["sideline"] = "I HEARD MY NAME OUT IN THE FOG. THE OTHERS SAW NOTHING. I SAW A LAMP. MOVING. NO HAND HOLDING IT. I'LL PAY THE BOND BACK. NOT ON THAT BOAT.";
      P["partopt"] = "DRAW ME THAT LAMP. I'LL SHOW THEM.";
      P["token"] = "A CHARCOAL DRAWING OF A LAMP"; P["tokenicon"] = "scroll";
      P["backline"] = "ONE MORE HAND. THE TIDE WON'T WAIT. GET YOUR GEAR, AND NO MORE TALK OF LAMPS.";
      P["letline"] = "NO TRACE? THE SEA TAKES ALL, AND SO DOES THE SHORE. HALF THE FEE.";
      P["letpay"] = "HALF. WE SAIL SHORT A HAND.";
      P["letmem"] = "WE SAILED SHORT A HAND, AND THE FOG CAME DOWN TWICE. NOBODY SAW A LAMP. NOBODY LOOKED.";
      P["letfx"] = "  do census home mood -2";
      P["partline"] = "A DRAWING? OF A LAMP? ...SHOW ME.";
      P["yieldline"] = "A LAMP ON THE WATER. WE LOSE BOATS. ...NOT TODAY. THE BOAT WAITS FOR THE FOG TO LIFT. {FUG} STAYS ASHORE.";
      P["yieldmem"] = "THE BOAT THAT WAITED CAME BACK. THE NEXT ONE OUT DIDN'T WAIT, AND DIDN'T.";
      P["fugseen"] = "I MEND NETS ASHORE NOW. I DON'T LOOK AT THE FOG.";
      P["hardline"] = "A DRAWING IS NOT A MAST. HERE'S YOUR FEE, WHAT'S LEFT OF IT.";
      P["hardmem"] = "A DRAWING IS NOT A MAST. ...THE FOG WAS THICK LAST NIGHT.";
      P["broughtmem"] = "THE RUNAWAY IS ABOARD AGAIN. WON'T LOOK AT THE FOG. WORKS LIKE TWO.";
      P["fugbroughtmem"] = "BACK ON THE BOAT. I KEEP MY EYES ON THE ROPES.";
      break;
  }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = asks[(size_t)d.below((int)asks.size())];
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Guard: case Role::Jarl: out.variant = V_GUILTY; break;
    case Role::Smith: out.variant = V_PRENTICE; break;
    case Role::Merchant: out.variant = V_CLERK; break;
    case Role::Priest: out.variant = V_NOVICE; break;
    case Role::Innkeeper: out.variant = V_CAT; break;
    case Role::Fisher: if (!w.coast) return false; out.variant = V_SAILOR; break;
    default: return false;
  }
  out.twist = d.below(8) == 0 ? 1 : 0;   // the rare beat: a wolf asleep in the hideout
  return true;
}

JobCtx walk(Role t, int variant, int twist, int season, int tier = 0, bool coast = false) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.season = season; c.tier = tier; c.coast = coast;
  return c;
}

}  // namespace

const KindDef& kindRunDown() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "rundown";
    k.title = "RUN-DOWN";
    k.lead = "WANTS A RUNAWAY FOUND";
    k.body = kBody;
    k.variants = {"guilty", "prentice", "clerk", "novice", "catthief", "sailor"};
    k.twists = {"none", "wolf"};
    k.trades = {(int)Role::Guard, (int)Role::Jarl, (int)Role::Smith, (int)Role::Merchant, (int)Role::Priest, (int)Role::Innkeeper, (int)Role::Fisher};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Jarl, V_GUILTY, 0, 3), walk(Role::Smith, V_PRENTICE, 1, 2), walk(Role::Merchant, V_CLERK, 0, 0, 1),
               walk(Role::Priest, V_NOVICE, 0, 1), walk(Role::Innkeeper, V_CAT, 0, 1), walk(Role::Fisher, V_SAILOR, 1, 2, 0, true)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
