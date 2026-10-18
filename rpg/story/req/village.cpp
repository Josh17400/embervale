// P1b "New Roads Content" (VISION_PLAN 15.35): the ten new village requests (content spec: req_village.md, every final
// judge note of req_village.judge.md resolved). REQUESTS lane. Each script is `family request`, `settle village`, `once
// site` (a request can come up again in another village) and `cooldown 6` (a village raises at most one new request in 6
// days). WHEN a village raises one (its stores, season, flags, the beasts about) is req_offers.cpp's raise policy.
//
// THE VILLAGE REQUEST FRAME as language (dsl.h P1b): the clock is `deadline <days> -> <fail stage>` on the live goal and
// talk stages; the outcomes are `census home ...` on the success and fail endings (mood, stock, price, famine, festival,
// refugees); the turn-in scene with several voices is one say with the other speakers named inline ("{GRAMPS}: ...").
//
//   rq_sourwell     THE SOURED WELL       a rotten well lining; oak planks bought (or worked for) in the next town
//   rq_rabidfold    THE BITTEN FLOCK      a mad wolf bit the lead ram: cull, a full cure, or half a cure; the wolf at night
//   rq_wolfcub      THE LATE LANTERN      a boy nursing a trapped wolf cub in a ruin; bring both home (an escort)
//   rq_ashgranary   ASH IN THE GRANARY    a dare, a festival lamp and the village drunk who put the fire out
//   rq_eatenseed    THE SEED I LOST       the seed corn lost at dice; grain carried through a famine, after dark
//   rq_drunkenbear  THE DRUNKEN BEAR      a bear raiding a dead brewer's last casks before the feast: steel or a mash trail
//   rq_farsister    HIS HALF OF THE STICK a dead man's half of a tally stick, carried to his estranged sister
//   rq_movedstone   THE OLD OATH STONE    a boundary feud, a bronze oath tablet in a ruin, and thirty years of children's dares
//   rq_longdrove    THE LONG DROVE        six sheep to market for a widow's debt; the gate toll; Bramble walks home with you
//   rq_turnipthief  THE TURNIP THIEVES    a burned-out family hiding in a ruin; wages, a blind eye, or the road
//
// Fallbacks (VISION_PLAN 15.35: an item never runs with the wrong place or person): the supply items buy from a merchant
// of the nearest town (`makes` filters are not needed); the bear and the mad wolf come to the village at night (`ambush`),
// so no themed cave is needed; the refugees' burned village is never named (the raise policy checks that war or a raid
// has touched the district).
#include <string>
#include "rpg/story/dsl.h"

namespace story {
namespace dsl {

namespace {

// (one raw literal per script: MSVC caps a literal near 16 KB)
const char* const kVillage[] = {
R"DSL(
script rq_sourwell
title THE SOURED WELL
archetype village request: supply
tier 2
family request
settle village
needs season summer autumn
once site
cooldown 6
gossip "THE WELL TASTES OF RUST AND ROT."
gossip "THEY'RE CARRYING THE RIVER AGAIN."
hook npc villager
pitch "WHY IS EVERYONE CARRYING BUCKETS?"
hint "DON'T DRINK FROM THE WELL, WHATEVER THE OLD MEN TELL YOU. WE'RE ALL CARRYING THE RIVER NOW."
role giver giver
role home site home
role mill site town near
role seller npc merchant at mill
role gramps person male at home

stage start
  talk giver
  say "MY BOY HAS BEEN SICK THREE DAYS. IT'S THE WELL: IT TASTES OF RUST AND ROT. THE OLD LINING HAS GONE SOFT AND THE NAILS BLEED INTO THE WATER. HIS GRANDFATHER {GRAMPS} WON'T BOIL IT. SEVENTY YEARS, HE SAYS. THE FOOL."
  opt "HOW DO YOU MEND A WELL?" -> how
  opt "I'LL SEE THE WELL MENDED." -> go
  opt "DRINK FROM THE RIVER, THEN." -> refuse

stage how
  talk giver
  say "NEW BOARDS, TIGHT AT THE JOINTS. THE GARDENS GET RIVER WATER BY HAND NOW, SO WE GROW LESS AND CARRY LESS TO MARKET. IT ADDS UP. BRING EIGHT GOOD PLANKS FROM {MILL}, {MILL.DIR} OF HERE, AND THE LANE WILL DO THE REST."
  opt "EIGHT PLANKS. I'LL BRING THEM." -> go
  opt "NOT MY WELL, NOT MY TROUBLE." -> refuse

stage go
  goal goto mill
  then buy
  deadline 6 -> sour
  say "TAKE A YOKE IF YOU LIKE. EVERYONE ELSE HAS ONE ON."
  journal "THE WELL OF {HOME} HAS GONE SOUR. FIND {SELLER} IN {MILL} AND BUY EIGHT GOOD PLANKS TO RELINE IT, AND BE BACK WITHIN SIX DAYS."

stage buy
  talk seller
  say "PLANKS FOR A WELL? THEN SEASONED OAK. PINE ROTS IN THE WET, AND THAT'S HOW YOUR WELL WENT BAD IN THE FIRST PLACE, I'D WAGER. EIGHT BOARDS IS FORTY GOLD, AND I'LL ROPE THEM FOR CARRYING."
  opt "HERE'S FORTY GOLD." if gold 40 -> paid
  opt "I'M SHORT OF COIN." -> haggle

stage haggle
  talk seller
  say "SHORT? THEN HAUL LOGS IN MY YARD TILL THIS TIME TOMORROW AND WE'LL CALL IT SQUARE. OR TELL ME WHY A STRANGER CARES WHAT {HOME} DRINKS."
  opt "I'LL HAUL YOUR LOGS." -> labour
  opt "A SICK CHILD DRINKS FROM IT." check level 3 -> gift else labour_grudge

stage labour_grudge
  talk seller
  say "EVERY VILLAGE HAS A SICK CHILD WHEN IT WANTS SOMETHING FOR NOTHING. THE YARD IS ROUND THE BACK. MIND THE SAW."
  opt "ROUND THE BACK, THEN." -> labour

stage gift
  talk seller
  say "...A CHILD. MY OWN GIRL HAD THE GRIPES ONE SUMMER FROM A BAD CISTERN. TAKE THE OAK. PAY ME WHEN YOU'RE NEXT THIS WAY, OR DON'T. JUST DON'T TELL MY WIFE."
  opt "THANK YOU. TRULY." -> carry

stage labour
  goal wait 1
  then carry
  say "THE YARD IS MUD TO THE ANKLE. THE LOGS DON'T CARE ABOUT YOUR BOOTS."
  journal "HAUL LOGS IN THE TIMBER YARD OF {MILL} UNTIL TOMORROW. {SELLER} WILL PAY IN OAK PLANKS FOR THE WELL OF {HOME}."

stage paid
  goal goto home
  then mend
  deadline 4 -> sour
  do gold -40
  do give "EIGHT OAK PLANKS" sheaf heavy
  journal "CARRY THE OAK PLANKS HOME TO {GIVER} IN {HOME}. THEY ARE HEAVY, AND THE WELL WILL NOT WAIT."

stage carry
  goal goto home
  then mend
  deadline 4 -> sour
  do give "EIGHT OAK PLANKS" sheaf heavy
  journal "CARRY THE PLANKS {SELLER} GAVE YOU BACK TO {GIVER} IN {HOME} BEFORE THE SICKNESS SPREADS."

stage mend
  talk giver
  say "OAK! THE WHOLE LANE TURNED OUT TO RELINE IT. {GRAMPS} TOOK THE FIRST DIPPER. {GRAMPS}: COLD AS A BLADE. DON'T TELL THE YOUNG ONES I SAID SO. {GIVER}: I HEARD YOU, OLD MAN, AND YOU'LL BOIL IT ANYWAY. MY BOY HASN'T COUGHED SINCE MORNING."
  opt "DRINK DEEP, ALL OF YOU." -> mended

stage mended
  end success
  say "THE WELL OF {HOME} RUNS COLD AND SWEET. THE BUCKETS GO BACK ON THEIR HOOKS."
  journal "THE WELL OF {HOME} IS RELINED WITH OAK AND RUNS SWEET AGAIN."
  do take "EIGHT OAK PLANKS"
  do reward fair
  do census home mood 10
  do census home price produce 100
  do remember giver "THE WELL RUNS COLD AGAIN. THEY NAME YOU AT IT, YOU KNOW. THE STRANGER'S WELL."
  do remember gramps "COLD AS A BLADE. I'D HAVE DRUNK IT RUSTY ANOTHER SEVENTY YEARS, MIND."
  do fact "THE WELL OF {HOME} WENT SOUR WITH RUST AND ROT UNTIL A TRAVELLER BROUGHT OAK PLANKS FROM {MILL} TO RELINE IT."

stage sour
  end fail
  say "SIX DAYS AND NO PLANKS. THE VILLAGE CARRIES THE RIVER NOW, AND THE TALK OF SICKNESS HAS SPREAD TO THE NEXT LANE."
  journal "NO PLANKS CAME. {HOME} LEARNED TO LIVE WITH THE RIVER."
  do take "EIGHT OAK PLANKS"
  do census home mood -6
  do census home price produce 115
  do remember giver "WE CARRY THE RIVER NOW. MY BOY STILL COUGHS AT NIGHT."
  do fact "THE WELL OF {HOME} STANDS SOUR. THE VILLAGE CARRIES ITS WATER FROM THE RIVER."

stage refuse
  end fail
  journal "YOU WOULD NOT HELP {GIVER}: THE WELL OF {HOME} STAYS SOUR."
  do remember giver "THE RIVER IS A LONG WALK WITH A SICK CHILD ON YOUR HIP."
)DSL",

R"DSL(
script rq_rabidfold
title THE BITTEN FLOCK
archetype village request: a mad beast
tier 2
family request
settle village
needs season spring autumn
once site
cooldown 6
gossip "THERE'S A DEAD DOG UNDER A SACK."
gossip "THE PEN GATE IS CHAINED WITH A YOKE."
hook npc farmer
pitch "WHY THE SPEAR ACROSS YOUR KNEES?"
hint "STAY BACK FROM THE PEN. I MEAN IT. I'VE NOT SLEPT, AND I'VE A SPEAR, AND NEITHER IS A GOOD SIGN."
role giver giver
role home site home
role town site town near
role healer npc priest at town
var dose 0

stage start
  talk giver
  say "BARLEY, MY LEAD RAM, BIT MY DOG. THEN THREE EWES. FOAM ON HIS LIP, AND HE KNEW ME WHEN HE BIT. A MAD WOLF OFF THE HILL BIT HIM FIRST, AND IT'S STILL OUT THERE. FOUR SICK, AND THE DOG DEAD UNDER A SACK."
  opt "PUT THEM DOWN TONIGHT." -> cull
  opt "IS THERE A CURE FOR IT?" -> cure
  opt "I CAN'T HELP YOU WITH THIS." -> refuse

stage cull
  talk giver
  say "AYE. I'LL DO IT MYSELF; I KNOW EACH ONE. BARLEY FIRST, HE'D WANT IT QUICK. YOU FIND THAT WOLF. IT COMES DOWN TO THE PEN AFTER DARK, BOLD AS A DOG, FOR THE SMELL OF THEM."
  opt "I'LL BE AT THE GATE TONIGHT." -> watch

stage cure
  talk giver
  say "THE PRIEST AT {TOWN} KNOWS BITES AND FEVERS. THEY SET MY ARM ONCE. ASK THERE, AND QUICK. I'LL CHAIN THE PEN AND SIT UP WITH THE SPEAR TILL YOU'RE BACK."
  opt "I'LL GO TO {TOWN}." -> to_healer
  opt "NO. BETTER TO PUT THEM DOWN." -> cull

stage to_healer
  goal goto town
  then healer
  deadline 6 -> lost
  say "DON'T STOP FOR ALE. THE FOAM SPREADS."
  journal "FOUR SHEEP OF {GIVER} IN {HOME} WERE BITTEN BY A MAD WOLF. ASK {HEALER} OF {TOWN} FOR A CURE BEFORE IT SPREADS."

stage healer
  talk healer
  say "BITTEN? BY WHAT, AND WHEN? A MAD WOLF, TWO DAYS. THEN FOXGLOVE AND BITTER ROOT, IN A COLD BATH, COLD AS THE WELL GIVES IT. IT SAVES MOST. NOT ALL, I WARN YOU. FORTY GOLD: THE ROOT COSTS ME TOO."
  opt "HERE'S FORTY GOLD." if gold 40 -> dosed
  opt "I HAVE NO GOLD TO GIVE." -> poor

stage poor
  talk healer
  say "NO GOLD. THEN HEAR ME: THAT WOLF WILL BITE AGAIN, AND A CHILD NEXT TIME. KILL IT, AND HALF A POUCH OF ROOT IS YOURS. HALF SAVES TWO, IF YOU CHOOSE WELL. OR THEY ALL GO DOWN."
  opt "I'LL KILL IT. GIVE ME HALF." -> half
  opt "THEN THEY ALL GO DOWN." -> cull_after

stage cull_after
  goal goto home
  then cull
  deadline 3 -> lost
  journal "THERE IS NO CURE YOU CAN PAY FOR. GO BACK TO {GIVER} IN {HOME}: THE FOUR BITTEN SHEEP MUST BE PUT DOWN, AND THE WOLF FOUND."

stage dosed
  goal goto home
  then bathe
  deadline 3 -> lost
  do gold -40
  do give "A POUCH OF BITTER ROOT" herb
  do set dose 2
  say "COLD WATER. NOT WARM, WHATEVER THEY TELL YOU AT HOME."
  journal "HURRY THE BITTER ROOT HOME TO {GIVER} IN {HOME}. A COLD BATH AND A DOSE EACH, THE PRIEST SAYS."

stage half
  goal goto home
  then bathe
  deadline 3 -> lost
  do give "HALF A POUCH OF BITTER ROOT" herb
  do set dose 1
  say "TWO, MIND. CHOOSE THE STRONGEST. AND KILL THAT WOLF."
  journal "TAKE HALF A POUCH OF BITTER ROOT TO {GIVER} IN {HOME}. IT WILL SAVE TWO OF THE FOUR. THE MAD WOLF MUST DIE FOR IT."

stage bathe
  talk giver
  say "BITTER ROOT. RIGHT. BATHE THEM TONIGHT IN WELL WATER, COLD AS IT COMES, AND DOSE EACH ONE. BUT THE WOLF WILL BE BACK FOR THE SMELL OF THEM. WATCH THE GATE WHILE I WORK, OR IT'S ALL FOR NOTHING."
  opt "I'LL WATCH THE GATE." -> watch

stage watch
  goal time 22 4
  then wolf
  deadline 3 -> lost
  say "SIT BY THE GATE. YOU'LL HEAR IT BEFORE YOU SEE IT."
  journal "WAIT BY THE PEN OF {GIVER} IN {HOME} UNTIL DEEP NIGHT. THE MAD WOLF COMES DOWN OFF THE HILL AFTER DARK."

stage wolf
  goal kill 2 wolf
  then dawn
  deadline 2 -> lost
  do ambush wolf 2
  say "THERE. TWO OF THEM. THE ONE WITH THE WET JAW IS THE MAD ONE."
  journal "THE MAD WOLF HAS COME DOWN TO THE PEN WITH ITS MATE. KILL THEM BOTH BEFORE THEY GET IN AMONG THE FLOCK."

stage dawn
  talk giver
  say "BOTH WOLVES DEAD BY THE GATE, AND THE SKY GOING GREY. I'VE NOT LOOKED IN THE PEN YET. I DON'T WANT TO. COME AND LOOK WITH ME."
  opt "LOOK IN THE PEN TOGETHER." if is dose 2 -> end_cure
  opt "LOOK AT THE TWO YOU DOSED." if is dose 1 -> end_half
  opt "HELP BURY THE FOUR." if is dose 0 -> end_cull

stage end_cure
  end success
  say "BARLEY ATE AT DAWN. HE KNEW ME, AND HE DIDN'T BITE. ONE EWE WE LOST IN THE NIGHT. THREE OF FOUR, AND THE WOLF DEAD. THAT'S A BETTER NIGHT THAN ANY I'VE HAD THIS WEEK."
  journal "BARLEY AND TWO EWES OF {GIVER} LIVED THROUGH THE MADNESS. THE WOLF THAT BIT THEM IS DEAD."
  do take "A POUCH OF BITTER ROOT"
  do reward rich
  do census home stock livestock -1
  do census home mood 10
  do rumour
  do remember giver "BARLEY MEETS ME AT THE GATE AGAIN. HE DOESN'T KNOW HE NEARLY DIDN'T. YOU DO."
  do fact "A MAD WOLF BIT THE LEAD RAM OF {HOME}. A TRAVELLER BROUGHT BITTER ROOT FROM {TOWN} AND THREE OF THE FOUR BITTEN SHEEP LIVED."

stage end_half
  end success
  say "BARLEY AND THE YOUNGEST EWE TOOK THE ROOT. BOTH STOOD AT DAWN. THE OTHER TWO I PUT DOWN MYSELF, QUICK AND KIND. TWO OF FOUR. I CAN LIVE WITH TWO."
  journal "HALF A CURE SAVED TWO OF THE FOUR BITTEN SHEEP OF {GIVER}. THE MAD WOLF IS DEAD."
  do take "HALF A POUCH OF BITTER ROOT"
  do reward fair
  do census home stock livestock -2
  do census home mood 6
  do remember giver "TWO OF FOUR. BARLEY'S ONE OF THEM. I CHOSE HIM BEFORE I CHOSE MY BREAKFAST."
  do fact "A MAD WOLF BIT FOUR SHEEP IN {HOME}. HALF A POUCH OF BITTER ROOT FROM {TOWN} SAVED TWO, AND THE WOLF WAS KILLED AT THE PEN GATE."

stage end_cull
  end success
  say "FOUR PUT DOWN AND BURIED DEEP, BARLEY FIRST. THE WOLF TOO. THE REST OF THE FLOCK WILL LIVE, AND THE PEN IS SAFE. I'LL PAY YOU, SINCE I'VE NO WORDS FOR IT."
  journal "{GIVER} PUT DOWN THE FOUR BITTEN SHEEP AND YOU KILLED THE MAD WOLF. THE REST OF THE FLOCK IS SAFE."
  do reward small
  do census home stock livestock -4
  do census home mood 4
  do remember giver "FOUR PUT DOWN. THE REST LIVE. YOU HAD SENSE WHEN I HAD NONE LEFT."
  do fact "A MAD WOLF BIT FOUR SHEEP OF {HOME}. THEY WERE PUT DOWN, AND A TRAVELLER KILLED THE WOLF AT THE PEN GATE."

stage lost
  end fail
  say "THE MAD WOLF GOT INTO THE PEN. THE FLOCK IS HALVED, AND BARLEY'S COLLAR HANGS ON THE GATE."
  journal "THE PEN STANDS EMPTY. BARLEY'S COLLAR HANGS ON THE GATE."
  do take "A POUCH OF BITTER ROOT"
  do take "HALF A POUCH OF BITTER ROOT"
  do census home stock livestock -6
  do census home price meat 120
  do census home mood -8
  do remember giver "THE PEN'S EMPTY. I SIT BY IT ANYWAY. HABIT."
  do fact "A MAD WOLF GOT INTO A PEN IN {HOME} AND HALF THE FLOCK WAS LOST. MEAT COSTS MORE THERE NOW."

stage refuse
  end fail
  journal "YOU WOULD NOT HELP {GIVER} WITH THE BITTEN FLOCK."
  do remember giver "I SAT UP ALONE WITH THE SPEAR. THANKS FOR NOTHING."
)DSL",

R"DSL(
script rq_wolfcub
title THE LATE LANTERN
archetype village request: missing child
tier 2
family request
settle village
needs season autumn winter
once site
cooldown 6
gossip "WET FUR ON THE BOY LATELY."
gossip "THE LANTERNS ARE OUT ON THE TREELINE."
hook npc farmer
pitch "WHO ARE THE LANTERNS FOR?"
hint "IF YOU CAME BY THE HILL ROAD, DID YOU SEE A BOY? TWELVE, MUD TO THE KNEES. NO? THEN I'M SORRY TO TROUBLE YOU."
role giver giver
role home site home
role ruin site ruin near
role boy person male young in ruin

stage start
  talk giver
  say "NINE NIGHTS MY BOY {BOY} HAS COME HOME LATE, WITH WET FUR ON HIS SLEEVES. TONIGHT NOT AT ALL. HIS TRACKS RUN TO {RUIN}, TO THE OLD FOLD THERE. THINGS LIVE IN THAT RUIN. I'VE LOST LAMBS TO WOLVES THIS YEAR AND SET TRAPS ON THE HILL."
  opt "I'LL FIND HIM." -> seek
  opt "WHY WOULD HE GO THERE?" -> why
  opt "THE LANTERNS WILL FIND HIM." -> refuse

stage why
  talk giver
  say "I DON'T KNOW, AND THAT'S THE WORST OF IT. HE'S BEEN TAKING HAM BONES; HIS SISTER SWEARS THE DOG DIDN'T. GO CAREFUL. AND TELL HIM I'M NOT ANGRY. ...I AM. TELL HIM I'M NOT."
  opt "I'LL BRING HIM HOME TO YOU." -> seek

stage seek
  goal enter ruin
  then found
  deadline 3 -> late
  say "THE HUNTERS ARE FOLLOWING THE TRACKS TOO. THEY'RE SLOWER. BE QUICKER."
  journal "{BOY}, A BOY OF TWELVE FROM {HOME}, IS MISSING. HIS TRACKS RUN TO THE OLD FOLD IN {RUIN}. FIND HIM BEFORE THE HUNTERS DO."

stage found
  talk boy
  say "SHH. HE'S ASLEEP, FINALLY. ONE OF THE TRAPS ON THE HILL TOOK HIS LEG; MY {GIVER.FATHER} SET THEM. HIS MOTHER WAS IN ANOTHER. DEAD. I SPLINTED HIM. HE BIT ME. FAIR. CAN HE COME HOME? HE'LL DIE IN HERE."
  opt "WRAP HIM IN YOUR COAT. WE'LL GO." -> carry
  opt "LEAVE THE WOLF. COME HOME." -> leave_cub

stage leave_cub
  talk boy
  say "LEAVE HIM? NINE NIGHTS I'VE FED HIM. HE KNOWS MY WHISTLE. IF I GO, THE HUNTERS FIND HIM, OR THE COLD DOES. I'M NOT GOING WITHOUT HIM."
  opt "ALL RIGHT. BRING HIM." -> carry
  opt "HE'S A WOLF. HE'LL MANAGE." check level 3 -> alone else carry_sulk

stage carry_sulk
  talk boy
  say "YOU DON'T EVEN BELIEVE THAT. I CAN SEE YOU DON'T. I'M BRINGING HIM, AND YOU CAN WALK IN FRONT OR BEHIND."
  opt "IN FRONT, THEN. STAY CLOSE." -> carry

stage carry
  goal escort boy to home walk
  then door
  else boy_lost
  deadline 3 -> late
  do follow boy
  say "HE'S HEAVIER THAN HE LOOKS. SO AM I. GO SLOW."
  journal "WALK {BOY} HOME TO {HOME}, THE WOLF CUB BUNDLED IN HIS COAT. THE RUIN'S CREATURES AND THE HUNTERS ARE BOTH ABOUT."

stage alone
  goal escort boy to home walk
  then door_alone
  else boy_lost
  deadline 3 -> late
  do follow boy
  say "FINE. BUT I'M LEAVING HIM MY SCARF. DON'T LOOK AT ME."
  journal "WALK {BOY} HOME TO {HOME}. HE LEFT THE WOLF CUB IN THE FOLD WITH HIS SCARF, AND HAS NOT SAID A WORD SINCE."

stage door
  talk giver
  say "{BOY}! WHAT'S THAT IN YOUR COAT? ...THAT IS A WOLF. IN MY DOORWAY. {BOY}: ONE OF YOUR TRAPS DID THAT LEG. {GIVER}: ...THE BARN. THE BARN IS ACCEPTABLE. TO BE FAIR, IT IS A SMALL WOLF. WHEN THE LEG MENDS, WE WALK HIM UP THE HILL."
  opt "AND THE TRAPS ON THE HILL?" -> end_barn

stage door_alone
  talk giver
  say "{BOY}! YOU'RE FROZEN. WHERE WERE YOU? ...HE WON'T SAY. HE WALKED STRAIGHT PAST ME TO THE FIRE. WHAT HAPPENED OUT THERE?"
  opt "HE'LL TELL YOU WHEN HE'S READY." -> end_alone

stage end_barn
  end success
  say "THE TRAPS COME DOWN TOMORROW. ALL OF THEM. I'LL FIND ANOTHER WAY TO KEEP THE LAMBS. HERE: HE WHITTLED THIS. HE SAYS YOU SHOULD HAVE IT."
  journal "{BOY} IS HOME, AND A LAME WOLF CUB SLEEPS IN THE BARN OF {GIVER} UNTIL SPRING."
  do give "A WHITTLED WOLF WHISTLE" bone
  do reward fair
  do census home mood 12
  do remember giver "{BOY} KEEPS A WOLF IN THE BARN. UNTIL SPRING, I TELL HIM. HE PRETENDS TO BELIEVE ME."
  do remember boy "HE CAN STAND ON THE LEG NOW. HE STILL BITES. ONLY ME, THOUGH."
  do fact "A BOY OF {HOME} NURSED A TRAPPED WOLF CUB IN THE RUIN OF {RUIN}. HIS PARENT TOOK THE TRAPS OFF THE HILL."

stage end_alone
  end success
  say "HE'S HOME AND WARM, AND I WON'T ASK MORE TONIGHT. THANK YOU FOR BRINGING HIM. HE'LL THANK YOU HIMSELF, ONE DAY. NOT TODAY."
  journal "{BOY} IS HOME, BUT HE LEFT SOMETHING IN THE FOLD AT {RUIN} THAT HE WON'T TALK ABOUT."
  do reward small
  do census home mood 4
  do remember boy "HE HAD MY SCARF. THE HUNTERS BROUGHT IT BACK. I KNOW WHAT THAT MEANS."
  do fact "A BOY OF {HOME} WENT MISSING IN THE RUIN OF {RUIN} AND WAS BROUGHT HOME. HE HAS BARELY SPOKEN SINCE."

stage boy_lost
  end fail
  say "{BOY} NEVER CAME HOME. THE LANTERNS FOUND HIM IN THE BRACKEN, TOO LATE."
  journal "{BOY} DIED ON THE WAY HOME TO {HOME}."
  do census home mood -10
  do remember giver "HE WAS TWELVE. HE WAS FEEDING A WOLF. THAT'S ALL HE WAS DOING."
  do fact "A BOY OF {HOME} DIED ON THE WAY HOME FROM THE RUIN OF {RUIN}."

stage late
  end fail
  say "THE HUNTERS REACHED THE FOLD FIRST. THEY KILLED THE CUB AND BROUGHT {BOY} HOME, FURIOUS AND SILENT."
  journal "THE HUNTERS REACHED THE FOLD AT {RUIN} BEFORE YOU. THE CUB IS DEAD AND {BOY} WON'T SPEAK."
  do census home mood -6
  do remember giver "HE WON'T SPEAK TO ME. HE WON'T SPEAK TO YOU EITHER, IF YOU WERE WONDERING."
  do fact "HUNTERS OF {HOME} KILLED A WOLF CUB IN THE RUIN OF {RUIN}. A BOY HAD BEEN NURSING IT FOR NINE NIGHTS."

stage refuse
  end fail
  journal "YOU LEFT THE SEARCH FOR {BOY} TO THE LANTERNS OF {HOME}."
  do remember giver "THE LANTERNS FOUND HIM. NO THANKS TO YOU."
)DSL",

R"DSL(
script rq_ashgranary
title ASH IN THE GRANARY
archetype village request: a scapegoat
tier 2
family request
settle village
needs season summer autumn
once site
cooldown 6
gossip "THE GRANARY WENT UP ON FEAST NIGHT."
gossip "SOOT ON HIS BOOTS, THEY SAY."
hook npc farmer
pitch "WHAT BURNED HERE?"
hint "MIND THE ASH. HALF THE VILLAGE IS SIFTING IT FOR SEED, AND THE OTHER HALF IS LOOKING FOR SOMEONE TO HANG IT ON."
role giver giver
role home site home
role drunk person male at home
role crone person female at home
role kid person female young at home

stage start
  talk giver
  say "MY GRANARY BURNED ON FEAST NIGHT. THE VILLAGE SAYS {DRUNK} DID IT: SOOT ON HIS BOOTS, DRUNK AS A LORD, ASLEEP IN THE LANE AT DAWN. SINCE HIS WIFE DIED HE DRINKS. I HALF BELIEVE IT. I'D RATHER KNOW."
  opt "I'LL ASK {DRUNK}." -> drunk
  opt "THEN HE DID IT. WHY ASK?" -> doubt
  opt "ASH IS ASH. LET IT LIE." -> refuse

stage doubt
  talk giver
  say "BECAUSE THE EAST BIN DIDN'T BURN, AND SOMEBODY BEAT THE FIRE OUT WITH A WET CLOAK. A MAN WHO SETS A FIRE DOESN'T PUT IT OUT. ASK {DRUNK}. ASK WHO SAW A LIGHT."
  opt "I'LL ASK HIM." -> drunk

stage drunk
  talk drunk
  say "I DIDN'T LIGHT IT. I PUT IT OUT. WET CLOAK, BARE HANDS, THE EAST BIN. THE LAMP LAY ON THE FLOOR, STILL LIT, A FEAST LAMP WITH THE RIBBON ON. NOBODY ASKED ME. THEY SAW SOOT AND DRINK AND THAT WAS ENOUGH."
  deadline 5 -> unasked
  opt "WHO ELSE WAS ABOUT?" -> crone
  opt "SHOW ME YOUR HANDS." -> hands

stage hands
  talk drunk
  say "THERE. BLISTERS, BOTH PALMS. I CAN'T HOLD A CUP STEADY, AND IT'S NOT ONLY THE DRINK. OLD {CRONE} SITS UP AT HER WINDOW ALL NIGHT. ASK HER WHAT SHE SAW."
  opt "I'LL ASK {CRONE}." -> crone

stage crone
  talk crone
  say "A LIGHT WENT INTO THE GRANARY BEFORE THE FIRE. SMALL AND QUICK, LIGHT ON THE FEET, A LAMP SWINGING. THEN SCREAMING, AND THE LIGHT CAME OUT RUNNING WITHOUT THE LAMP. THEN {DRUNK}, SHOUTING, RUNNING IN. I THOUGHT HE'D BURN."
  deadline 5 -> unasked
  opt "SMALL? A CHILD?" -> kid

stage kid
  talk kid
  say "THE BIG ONES DARED ME. A NIGHT IN THE GRANARY, WHERE THE OLD MILLER HANGED HIMSELF. I TOOK A FEAST LAMP SO I WOULDN'T BE SCARED. A RAT RAN OVER MY FOOT. I DROPPED IT. IT WENT UP SO FAST. I RAN. HE DIDN'T RUN."
  deadline 5 -> unasked
  opt "YOU HAVE TO TELL THEM." -> kid_more
  opt "STAY HERE. I'LL SEE TO IT." -> verdict

stage kid_more
  talk kid
  say "THEY'LL THROW ME OUT. OR THEY'LL LOOK AT ME LIKE THEY LOOK AT HIM. I'VE WATCHED THEM SPIT AT HIM FOR TWO DAYS AND SAID NOTHING. I'M WORSE THAN A RAT."
  opt "THEN I'LL SPEAK FOR YOU." -> verdict

stage verdict
  talk giver
  say "WELL? THE VILLAGE MEETS ON THE GREEN TONIGHT TO DECIDE WHAT'S TO BE DONE WITH {DRUNK}. WHAT DO I TELL THEM?"
  deadline 5 -> unasked
  opt "THE WHOLE TRUTH: THE DARE, THE LAMP." -> end_truth
  opt "SAY {DRUNK} LIT IT." -> end_lie
  opt "A FEAST LAMP FELL. NOBODY'S FAULT." -> end_mercy

stage end_truth
  end success
  say "I TOLD THEM ALL OF IT. {KID} STOOD UP BEFORE I'D FINISHED AND SAID THE REST HERSELF. THE BIG ONES WHO DARED HER ARE MUCKING OUT MY BARN TILL HARVEST, AND {DRUNK} HAS HAD MORE HANDS SHAKEN TONIGHT THAN IN TEN YEARS."
  journal "{HOME} KNOWS THE TRUTH ABOUT THE GRANARY FIRE: A CHILD'S DARE, A DROPPED LAMP, AND {DRUNK} WHO PUT IT OUT."
  do reward fair
  do deed innocent
  do census home mood 8
  do census home stock grain 6
  do remember drunk "THEY BUY ME BREAD NOW INSTEAD OF ALE. I'M NOT SURE WHICH I MIND MORE."
  do remember kid "I SAID IT MYSELF IN THE END. MY KNEES WERE SHAKING THE WHOLE TIME."
  do fact "THE GRANARY OF {HOME} BURNED FROM A DROPPED FEAST LAMP. {DRUNK}, BLAMED FOR IT, HAD BEATEN THE FIRE OUT WITH HIS OWN CLOAK."

stage end_lie
  end success
  say "I TOLD THEM. THEY TOOK IT LIKE BREAD. {DRUNK} WAS GONE BY MORNING WITH HIS CLOAK STILL BLACK. {KID} HASN'T SPOKEN A WORD TO ANYONE SINCE."
  journal "{HOME} DROVE {DRUNK} OUT FOR A FIRE HE PUT OUT. {KID} KNOWS, AND SO DO YOU."
  do reward small
  do deed lied
  do deed cruelty
  do census home mood -2
  do hide drunk
  do remember kid "HE PUT IT OUT. YOU KNEW. YOU KNEW AND YOU SAID IT ANYWAY."
  do fact "{DRUNK} WAS DRIVEN OUT OF {HOME} FOR BURNING THE GRANARY. HE HAD PUT THE FIRE OUT."

stage end_mercy
  end success
  say "A FEAST LAMP FELL, I SAID, AND {DRUNK} FOUGHT IT. NOBODY ARGUED. THEN I LOOKED AT {KID} IN HER DOORWAY, AND SHE LOOKED AT ME. WE'LL HAVE A QUIET WORD, HER AND I. THE KIND THAT LASTS."
  journal "{HOME} WAS TOLD A FEAST LAMP FELL AND {DRUNK} FOUGHT THE FIRE. THE GRANARY KEEPER HAD A QUIET WORD WITH {KID}."
  do reward fair
  do deed mercy
  do census home mood 6
  do census home stock grain 6
  do remember giver "A LAMP FELL. THAT'S THE STORY, AND IT'S TRUE AS FAR AS IT GOES. THANK YOU."
  do remember drunk "THEY SAY I FOUGHT IT. I DID. NICE TO HEAR IT SAID."
  do fact "THE GRANARY OF {HOME} BURNED WHEN A FEAST LAMP FELL. {DRUNK} BEAT THE FIRE BACK FROM THE EAST BIN."

stage unasked
  end fail
  say "THE GREEN MET WITHOUT AN ANSWER. THEY DROVE {DRUNK} OUT BY TORCHLIGHT, AND NOBODY ASKED WHO HAD CARRIED THE LAMP."
  journal "NO ONE ASKED WHO CARRIED THE LAMP. {HOME} DROVE {DRUNK} OUT."
  do hide drunk
  do census home mood -5
  do remember giver "THEY DROVE HIM OUT AND I HELD A TORCH. I KEEP SMELLING THE SMOKE."
  do fact "{HOME} DROVE {DRUNK} OUT FOR THE GRANARY FIRE BEFORE ANYONE ASKED WHAT HAPPENED."

stage refuse
  end fail
  journal "YOU LEFT THE GRANARY FIRE OF {HOME} TO THE GOSSIPS."
  do remember giver "ASH IS ASH, YOU SAID. A MAN'S NAME ISN'T, AS IT TURNS OUT."
)DSL",

R"DSL(
script rq_eatenseed
title THE SEED I LOST
archetype village request: supply
tier 2
family request
settle village
needs season spring
once site
cooldown 6
gossip "WHO'S COUNTING THE SEED SACKS?"
gossip "THE PLOUGHS HAVE STOOD IDLE A WEEK."
hook npc villager
pitch "YOU LOOK LIKE YOU'VE SEEN A GHOST."
hint "SOWING IS DAYS OFF. DAYS. AND I'VE CHECKED THAT LOCK FOUR TIMES THIS MORNING. DON'T MIND ME."
role giver giver
role home site home
role town site town near
role factor npc merchant at town

stage start
  talk giver
  say "I KEEP THE VILLAGE SEED. KEPT IT. A DROVER WINTERED AT THE INN WITH A SET OF DICE, AND I BET THE SACKS ONE AT A TIME, THEN ALL OF THEM. HE ROLLED SIX AND LEFT AT THE THAW. THE CHEST IS BARE, AND NOBODY HAS COUNTED IT YET."
  opt "WHAT DO YOU NEED?" -> ask
  opt "TELL THE VILLAGE THE TRUTH." -> truth
  opt "THAT'S YOUR SHAME, NOT MINE." -> refuse

stage truth
  talk giver
  say "THE TRUTH IN SPRING MEANS NO SOWING, AND A VILLAGE THAT HANGS ITS SEED-KEEPER INSTEAD OF PLANTING. THE TRUTH COMES AFTER HARVEST, I SWEAR IT. WHEN THERE'S BREAD ON THE TABLE TO SOFTEN IT."
  opt "AFTER HARVEST, THEN. WHAT DO YOU NEED?" -> ask

stage ask
  talk giver
  say "SEED. THERE'S FAMINE ALL ROUND, AND NO CARAVAN WILL CARRY GRAIN IN A FAMINE. A TRAVELLER ON FOOT CAN. BRING SIX SACKS FROM {TOWN}. COME AFTER DARK AND LEAVE THEM AT MY BARN. NO ONE ASKS WHAT COMES IN AT NIGHT."
  opt "SIX SACKS, AFTER DARK." -> go

stage go
  goal goto town
  then buy
  deadline 6 -> fallow
  say "HERE. EVERY COIN I HAVE LEFT. IT ISN'T ENOUGH. I KNOW."
  do gold 15
  journal "THE SEED CHEST OF {HOME} IS EMPTY AND SOWING IS DAYS OFF. BUY SIX SACKS OF SEED GRAIN FROM {FACTOR} IN {TOWN}."

stage buy
  talk factor
  say "SEED GRAIN, IN A FAMINE YEAR? EVERYONE WANTS IT AND NOBODY WILL CART IT. SIX SACKS IS SIXTY GOLD, AND THAT'S CHEAP FOR WHAT IT IS. CARRY IT YOURSELF AND IT'S YOUR BACK, NOT MY WAGON."
  opt "SIXTY GOLD. DONE." if gold 60 -> bought
  opt "IT'S FOR SOWING, NOT EATING." check level 3 -> cheap else dear
  opt "I'LL RETURN WHEN I HAVE IT." -> wait_coin

stage cheap
  talk factor
  say "FOR SOWING? THEN IT COMES BACK TENFOLD AND I'VE SOLD TO A VILLAGE THAT WILL BUY FROM ME FOR TWENTY YEARS. THIRTY GOLD, AND TELL THEM WHERE THE SEED CAME FROM."
  opt "THIRTY GOLD. DONE." if gold 30 -> bought_cheap
  opt "HOLD IT. I'LL FETCH THE THIRTY." -> wait_coin

stage dear
  talk factor
  say "EVERY SACK IS FOR SOWING UNTIL SOMEONE'S HUNGRY. SIXTY, OR GO AND SEE IF THE NEXT TOWN IS SOFTER."
  opt "SIXTY, THEN. HERE." if gold 60 -> bought
  opt "KEEP THE SACKS. I'LL BE BACK." -> wait_coin

stage wait_coin
  goal wait 1
  then buy_again
  deadline 4 -> fallow
  journal "FIND THE COIN FOR SIX SACKS OF SEED GRAIN, THEN SEE {FACTOR} IN {TOWN} AGAIN. THE SOWING WILL NOT WAIT LONG."

stage buy_again
  talk factor
  say "BACK AGAIN? THE PRICE HASN'T MOVED, AND THE SEED HASN'T EITHER. COIN ON THE COUNTER, OR GO HOME AND TELL THEM THE TRUTH."
  opt "SIXTY GOLD. HERE." if gold 60 -> bought
  opt "THIRTY, AND IT'S FOR SOWING." if gold 30 check level 4 -> bought_cheap else no_seed
  opt "I CAN'T RAISE IT." -> no_seed

stage no_seed
  end fail
  say "YOU WALK BACK TO {HOME} WITH EMPTY HANDS. THE KEEPER SEES YOUR FACE AND DOESN'T ASK. AT THE MOOT THEY OPEN THE CHEST: NO SEED, ONLY A SET OF DICE."
  journal "YOU COULD NOT BUY THE SEED. THE CHEST OF {HOME} WAS OPENED AT THE MOOT: EMPTY, BUT FOR THE DICE."
  do census home famine 14
  do census home mood -10
  do remember giver "YOU TRIED. THE CHEST WAS OPENED ANYWAY. I DON'T KEEP THE KEY NOW."
  do fact "THE SEED CHEST OF {HOME} WAS FOUND EMPTY AT THE MOOT, WITH A DROVER'S DICE IN IT. THE FIELDS LIE FALLOW THIS YEAR."

stage bought
  goal goto home
  then barn
  deadline 4 -> fallow
  do gold -60
  do give "SIX SACKS OF SEED GRAIN" seeds heavy
  journal "CARRY THE SIX SACKS OF SEED GRAIN BACK TO {HOME}. THEY ARE HEAVY. THE KEEPER WANTS THEM AT THE BARN, AFTER DARK."

stage bought_cheap
  goal goto home
  then barn
  deadline 4 -> fallow
  do gold -30
  do give "SIX SACKS OF SEED GRAIN" seeds heavy
  journal "CARRY THE SEED GRAIN {FACTOR} SOLD CHEAP BACK TO {HOME}. LEAVE IT AT THE KEEPER'S BARN AFTER DARK."

stage barn
  goal time 21 3
  then count
  deadline 3 -> fallow
  journal "WAIT UNTIL DARK, THEN LEAVE THE SEED AT THE BARN OF {GIVER} IN {HOME}, WHERE NO ONE WILL SEE THE SACKS COME IN."

stage count
  talk giver
  say "COUNT THEM WITH ME. ONE, TWO... ALL SIX. I'LL TELL THEM MY KIN IN THE NEXT VALLEY SENT SEED. THE DICE ARE STILL LOCKED IN THE CHEST WHERE I PUT THEM, TO SHAME ME EVERY TIME I LIFTED THE LID. THEY GO IN THE FIRE TONIGHT."
  opt "AND AFTER HARVEST?" -> end_sown

stage end_sown
  end success
  say "AFTER HARVEST I TELL THE GREEN, ALL OF IT. SEE IF I DON'T. TONIGHT I PLANT."
  journal "{HOME} SOWED ON TIME WITH SEED YOU CARRIED THROUGH THE FAMINE. THE KEEPER SAYS KIN SENT IT."
  do take "SIX SACKS OF SEED GRAIN"
  do reward fair
  do census home stock grain 6
  do census home mood 12
  do remember giver "THE KEEPER'S KIN SENT SEED, THEY SAY. WHAT KIN? NO ONE ASKS. I'LL TELL THEM AFTER HARVEST."
  do fact "{HOME} SOWED ITS FIELDS IN A FAMINE SPRING WITH SEED THE KEEPER SAYS KIN SENT. A TRAVELLER CARRIED IT FROM {TOWN} BY NIGHT."

stage fallow
  end fail
  say "THE SOWING DAYS PASSED. AT THE MOOT THEY OPENED THE CHEST: NO SEED, ONLY A SET OF DICE. {GIVER} HANDED OVER THE KEY WITHOUT A WORD."
  journal "THE CHEST WAS OPENED AT THE MOOT. IT WAS EMPTY. THE DICE WERE NOT."
  do take "SIX SACKS OF SEED GRAIN"
  do census home famine 14
  do census home mood -10
  do remember giver "I DON'T KEEP ANYTHING NOW. NOT THE SEED, NOT THE KEY, NOT MUCH OF MY NAME."
  do fact "THE SEED CHEST OF {HOME} WAS FOUND EMPTY AT THE MOOT, WITH A DROVER'S DICE IN IT. THE FIELDS LIE FALLOW."

stage refuse
  end fail
  journal "YOU LEFT THE SEED-KEEPER OF {HOME} TO THEIR SHAME."
  do remember giver "YOU KNOW, AND YOU WALKED OFF. I CHECK THE LOCK AND THINK OF YOU."
)DSL",

R"DSL(
script rq_drunkenbear
title THE DRUNKEN BEAR
archetype village request: a beast in the cellar
tier 2
family request
settle village
once site
cooldown 6
gossip "THREE CASKS. NOT A DROP LEFT."
gossip "PAW PRINTS IN THE LANE, ALL IN LOOPS."
hook npc innkeeper
pitch "WHAT HAPPENED TO YOUR CELLAR DOOR?"
hint "THE FEAST IS DAYS OFF AND MY CELLAR DOOR IS MATCHWOOD. IF YOU HEAR SNORING TONIGHT, IT ISN'T ME."
role giver giver
role home site home

stage start
  talk giver
  say "THREE CASKS OF MY FATHER'S LAST BREW, BREWED THE AUTUMN BEFORE HE DIED. I PROMISED HIM THE FEAST WOULD POUR IT. LAST NIGHT A BEAR BROKE THE CELLAR DOOR, STOVE IN ONE CASK AND DRANK THE LANE DRY. HE'LL BE BACK TONIGHT FOR THE OTHER TWO."
  opt "I'LL BE WAITING FOR HIM." -> plan
  opt "WHY NOT MOVE THE CASKS?" -> why
  opt "BEARS ARE NOT MY TRADE." -> refuse

stage why
  talk giver
  say "EACH ONE WEIGHS MORE THAN A PONY, AND THE CELLAR'S THE ONLY COOL PLACE IN THE VILLAGE. AND HE'D FOLLOW THE SMELL ANYWHERE NOW. HE'S LEARNED WHAT A CASK IS. THAT'S THE TROUBLE WITH CLEVER BEARS."
  opt "THEN WE DEAL WITH THE BEAR." -> plan

stage plan
  talk giver
  say "HE COMES AFTER DARK, NOSE FIRST, AND HE'S THE SIZE OF A CART. YOU CAN MEET HIM WITH STEEL; IF HE FALLS, HIS HIDE IS YOURS. OR I'VE A BUCKET OF SOUR MASH HE'D CROSS A RIVER FOR. LEAD HIM OFF AND LET HIM SLEEP IT OFF ON THE HILL."
  opt "I'LL MEET HIM WITH STEEL." -> watch_fight
  opt "GIVE ME THE MASH." -> watch_lure

stage watch_fight
  goal time 21 3
  then bear
  deadline 4 -> dry
  say "I'LL BE UPSTAIRS. WITH THE LADDER PULLED UP."
  journal "A BEAR WITH A TASTE FOR BREW RAIDS THE CELLAR OF {GIVER} IN {HOME}. WAIT FOR HIM AFTER DARK, BEFORE THE FEAST."

stage watch_lure
  goal time 21 3
  then lure
  deadline 4 -> dry
  do give "A BUCKET OF SOUR MASH" veg
  say "DON'T SPILL IT ON YOURSELF. HE'LL TAKE YOU FOR DESSERT."
  journal "WAIT FOR THE BEAR AFTER DARK AT THE CELLAR OF {GIVER} IN {HOME}, WITH A BUCKET OF SOUR MASH TO LEAD HIM AWAY."

stage lure
  talk giver
  say "THERE. HEAR HIM? SNUFFLING AT THE DOOR LIKE A PIG AT A TROUGH. HE'S SEEN THE BUCKET. HE'S SITTING DOWN. I THINK HE'S WAITING FOR YOU TO POUR."
  opt "WALK SLOW. LEAD HIM UP THE HILL." check level 3 -> lured else spooked

stage spooked
  talk giver
  say "HE'S UP ON HIS HIND LEGS! HE WANTS THE BUCKET AND HE WANTS IT NOW. DROP IT AND DRAW!"
  opt "DROP THE BUCKET AND DRAW." -> bear

stage bear
  goal kill 1 bear
  then fought
  deadline 2 -> dry
  do take "A BUCKET OF SOUR MASH"
  do ambush bear 1
  say "HERE HE COMES. MIND THE CLAWS."
  journal "THE BEAR HAS COME FOR THE CASKS OF {GIVER}. STAND BETWEEN HIM AND THE CELLAR."

stage fought
  talk giver
  say "IS HE...? HE IS. THE SIZE OF HIM. NOT A DROP SPILLED FROM THE OTHER TWO. TAKE HIS HIDE, AS I SAID. AND COME TO THE FEAST: THE FIRST CUP IS YOURS, AND THE SECOND, AND I'LL STOP COUNTING AFTER THAT."
  opt "POUR YOUR FATHER'S BREW." -> end_fight

stage lured
  end success
  say "HE FOLLOWED THE MASH TO THE TOP OF THE HILL, ATE THE BUCKET TOO, AND SNORED THERE TILL NOON. NOT A DROP SPILLED. TASTE IT: BEARS KNOW GOOD BREW. DAD WOULD HAVE LAUGHED HIMSELF SICK."
  journal "THE BEAR SLEPT IT OFF ON THE HILL. THE FEAST OF {HOME} POURED THE LAST BREW OF {GIVER}'S FATHER."
  do take "A BUCKET OF SOUR MASH"
  do reward fair
  do census home festival
  do census home mood 15
  do remember giver "THEY POUR THE BEAR'S CUP NOW. ONE CUP LEFT ON THE HILL EACH FEAST. HE'S NEVER COME BACK FOR IT."
  do fact "A BEAR RAIDED THE INN CELLAR OF {HOME} BEFORE THE FEAST. A TRAVELLER LED IT OFF WITH SOUR MASH AND THE LAST CASKS WERE SAVED."

stage end_fight
  end success
  say "THE FEAST POURED HIS LAST BREW, AND I TOLD THE STORY TWICE BEFORE THE FIRST CASK WAS DRY. DAD WOULD HAVE LAUGHED AT ALL OF IT."
  journal "THE BEAR IS DEAD. THE FEAST OF {HOME} POURED THE LAST BREW OF {GIVER}'S FATHER."
  do give "A BEAR HIDE" pelt
  do reward fair
  do census home festival
  do census home mood 15
  do remember giver "A BEAR'S HIDE OVER YOUR SHOULDERS AND MY FATHER'S BREW IN YOUR CUP. THAT'S A FEAST NIGHT."
  do fact "A BEAR BROKE INTO THE INN CELLAR OF {HOME} BEFORE THE FEAST. A TRAVELLER KILLED IT AT THE CELLAR DOOR."

stage dry
  end fail
  say "HE CAME BACK, AND NOBODY WAS WAITING. THE LAST TWO CASKS ARE STAVES IN THE LANE. THE FEAST POURED WATER, AND IT WAS A MEAN NIGHT."
  journal "THE FEAST OF {HOME} POURED WATER. NO LAUGHS."
  do take "A BUCKET OF SOUR MASH"
  do census home mood -6
  do remember giver "MY FATHER'S LAST BREW WENT DOWN A BEAR'S THROAT. I PROMISED HIM. I PROMISED."
  do fact "A BEAR DRANK THE LAST CASKS OF THE INN OF {HOME} BEFORE THE FEAST. THE FEAST POURED WATER."

stage refuse
  end fail
  journal "YOU LEFT THE INNKEEPER OF {HOME} TO THE BEAR."
  do remember giver "NOT YOUR TRADE. NOR MINE, AS IT TURNS OUT."
)DSL",

R"DSL(
script rq_farsister
title HIS HALF OF THE STICK
archetype village request: a last message
tier 2
family request
settle village
needs season autumn winter
once site
cooldown 6
gossip "TWENTY YEARS AND NOT A WORD."
gossip "BLACK CLOTH ON THE DOOR BY THE FORD."
hook npc villager
pitch "WHO IS THE BLACK CLOTH FOR?"
hint "THEY'RE CUTTING THE NAME INTO THE MARKER NOW. THE RITES CAN WAIT A FEW DAYS. SOME THINGS CAN'T WAIT ANY LONGER."
role giver giver
role home site home
role town site town near
role dead person male at home
role widow person female at home
role sister person female at town

stage start
  talk giver
  do hide dead
  say "OLD {DEAD} DIED THREE DAYS AGO, MY NEIGHBOUR FORTY YEARS. HIS SISTER {SISTER} DOESN'T KNOW. THEY HAVEN'T SPOKEN IN TWENTY. HIS WIDOW {WIDOW} WON'T ASK ANYONE TO TELL HER, SO I'M ASKING YOU. SHE LIVES IN {TOWN}."
  opt "WHY DIDN'T THEY SPEAK?" -> why
  opt "I'LL SPEAK TO HIS WIDOW." -> widow
  opt "IT'S NOT MY PLACE." -> refuse

stage why
  talk giver
  say "THEIR FATHER LEFT THE FARM TO HIM ALONE. HE MEANT TO PAY HER HALF OUT AND NEVER COULD, AND HE WAS TOO PROUD TO SAY SO. SHE WALKED OUT OF THE READING OF THE WILL AND NEVER CAME BACK. NEITHER ONE EVER WROTE."
  opt "I'LL SPEAK TO HIS WIDOW." -> widow

stage widow
  talk widow
  say "SO THE NEIGHBOUR SENT YOU. GOOD. PRIDE, ALL OF IT. BUT EVERY WINTER HE CUT A NOTCH IN THIS. THEIR FATHER SPLIT A TALLY STICK BETWEEN THEM WHEN THEY WERE SMALL. THIS IS HIS HALF. TAKE IT TO {SISTER}. THE RITES WAIT FIVE DAYS, NO LONGER."
  opt "FIVE DAYS. I'LL TAKE IT." -> carry

stage carry
  goal goto town
  then sister
  deadline 5 -> buried
  do give "HALF A TALLY STICK" key
  say "DON'T SAY SORRY FOR HIM. HE'D HATE IT. JUST GIVE IT TO HER."
  journal "{DEAD} OF {HOME} IS DEAD. CARRY HIS HALF OF A TALLY STICK TO HIS SISTER {SISTER} IN {TOWN}. THE BURIAL WAITS FIVE DAYS."

stage sister
  talk sister
  say "HE DIED? THE STUBBORN OLD MULE. ...HIS HALF. HE KEPT IT. HOLD ON. MINE'S UNDER THE BED. HERE. THEY FIT. TWENTY NOTCHES ON HIS. I CUT TWENTY TOO. HE OWED ME A LETTER, NOT THE FARM. HE NEVER UNDERSTOOD THAT."
  opt "THE RITES ARE IN FIVE DAYS." -> end_comes
  opt "HE WAS SORRY. HE JUST NEVER SAID." -> end_sorry

stage end_comes
  end success
  say "THEN I'LL WALK TONIGHT. I'LL BE THERE. TELL HER... NO. I'LL TELL HER MYSELF. HERE, FOR YOUR TROUBLE, AND YOUR BOOTS."
  journal "{SISTER} WALKED TO {HOME} WITH HER HALF OF THE STICK. SHE STOOD AT HER BROTHER'S GRAVE."
  do take "HALF A TALLY STICK"
  do reward small
  do census home mood 8
  do remember widow "SHE CAME. SHE STAYED AT HIS GRAVE ALL NIGHT, THE TWO HALVES IN HER HANDS."
  do remember sister "BOTH HALVES ARE BURIED WITH HIM. THE ACCOUNT IS SQUARE."
  do fact "{SISTER} OF {TOWN} CAME TO HER BROTHER {DEAD}'S BURIAL IN {HOME} AFTER TWENTY YEARS OF SILENCE."

stage end_sorry
  end success
  say "SORRY? HE NEVER SAID IT TO A DOG IN HIS LIFE. ...BUT HE CUT THE NOTCHES. THAT'S HOW HE SAID IT. I'LL WALK TONIGHT. HERE, FOR YOUR BOOTS."
  journal "{SISTER} WALKED TO {HOME} FOR HER BROTHER'S BURIAL, THE TALLY STICK WHOLE AGAIN."
  do take "HALF A TALLY STICK"
  do reward small
  do census home mood 8
  do remember widow "SHE CAME, AND SHE SAID YOU TOLD HER HE WAS SORRY. HE WAS. NICE THAT SOMEONE SAID IT."
  do remember sister "THE NOTCHES WERE HIS SORRY. I UNDERSTAND THAT NOW. TOO LATE, BUT I DO."
  do fact "{SISTER} OF {TOWN} MADE PEACE WITH HER DEAD BROTHER {DEAD} OF {HOME} OVER A TALLY STICK SPLIT IN CHILDHOOD."

stage buried
  end fail
  say "THE RITES COULDN'T WAIT. {DEAD} WAS BURIED WITHOUT HIS SISTER, AND HIS HALF OF THE STICK IS STILL IN YOUR PACK."
  journal "{DEAD} WAS BURIED WITHOUT HIS SISTER."
  do census home mood -4
  do remember widow "HE WAS BURIED WITHOUT HER. YOU HAD FIVE DAYS."
  do fact "{DEAD} OF {HOME} WAS BURIED WITHOUT HIS SISTER. SHE STILL DOES NOT KNOW HE IS DEAD."

stage refuse
  end fail
  journal "YOU WOULD NOT CARRY WORD OF {DEAD}'S DEATH TO HIS SISTER."
  do remember giver "NOT YOUR PLACE. NO ONE'S, IT SEEMS. THAT'S HOW TWENTY YEARS GO BY."
)DSL",

R"DSL(
script rq_movedstone
title THE OLD OATH STONE
archetype village request: a boundary feud
tier 2
family request
settle village
needs season spring autumn
once site
cooldown 6
gossip "THAT STONE WAS A FOOT TO THE LEFT."
gossip "A RAKE IS NOT A SPEAR. TELL HIM."
hook npc villager
pitch "WHY ARE THOSE TWO SHOUTING?"
hint "IF YOU HEAR SHOUTING BY THE NORTH WALL, PAY IT NO MIND. OR PAY IT A GREAT DEAL. I HAVEN'T DECIDED WHICH."
role giver giver
role home site home
role ruin site ruin near
role hill person male at home
role low person female at home

stage start
  talk giver
  say "{HILL} OF THE HILL FARM AND {LOW} OF THE LOW FARM: NO PEACE ALL SEASON, AND THEY BOTH COME TO ME. A STANDING STONE MARKS THE LINE BETWEEN THEM, AND IT MOVES. ONE SAYS IT CREEPS DOWNHILL, THE OTHER THAT IT NEVER MOVED AN INCH. SOMETHING MOVES IT. NOT WEATHER."
  opt "WHAT DOES THE LAW SAY?" -> law
  opt "I'LL HEAR THEM BOTH FIRST." -> hill
  opt "MOVE THE STONE AND BE DONE." -> refuse

stage law
  talk giver
  say "THE FIRST SETTLERS CAST THE LINE IN BRONZE: AN OATH TABLET, LAID IN THEIR CAIRN IN THE OLD HALL AT {RUIN}. NOBODY HAS READ IT IN A HUNDRED YEARS. I CAN READ THE OLD LETTERS. FETCH IT, AND I'LL READ IT ON THE GREEN WHERE BOTH OF THEM CAN HEAR."
  opt "I'LL FETCH THE TABLET." -> fetch
  opt "LET ME HEAR THEM FIRST." -> hill

stage hill
  talk hill
  say "A FOOT TO THE LEFT LAST MONTH, AND A FOOT BEFORE THAT. IT CREEPS INTO MY GRAZING LIKE DAMP. MY GRANDFATHER PLOUGHED THAT STRIP FIFTY YEARS. {LOW} PUSHES IT IN THE NIGHT. I'VE NEVER SEEN HER DO IT. THAT'S HOW CLEVER SHE IS."
  opt "AND WHAT DOES {LOW} SAY?" -> low

stage low
  talk low
  say "{HILL} SAYS IT MOVES? IT HASN'T MOVED AN INCH. EXCEPT WHEN IT HAS, AND THEN IT'S HIS DOING, CREEPING IT DOWNHILL INTO MY CLOVER. MY MOTHER FED HER COWS ON THAT STRIP. READ THE BRONZE. I'LL ABIDE BY IT. HE WON'T."
  opt "THEN I'LL FETCH THE BRONZE." -> fetch

stage fetch
  goal fetch "THE OATH TABLET" in ruin
  then green
  deadline 8 -> blows
  say "IT'S HEAVY, THEY SAY. BRONZE AND THE SETTLERS' WORDS ON IT."
  journal "FETCH THE SETTLERS' BRONZE OATH TABLET FROM THE CAIRN IN THE RUIN OF {RUIN}. {GIVER} WILL READ IT ON THE GREEN."

stage green
  goal goto home
  then reading
  deadline 4 -> blows
  journal "CARRY THE OATH TABLET BACK TO {HOME} BEFORE THE FEUD BETWEEN {HILL} AND {LOW} COMES TO BLOWS."

stage reading
  talk giver
  say "I'VE READ IT. THE STRIP IS COMMON GRAZING; THE STONE MARKS ITS EDGE. {HILL}: COMMON? MY GRANDFATHER PLOUGHED IT! {LOW}: AND MY MOTHER GRAZED IT, AND WE WERE BOTH WRONG. {GIVER}: THERE IS MORE. THE CHILDREN HAVE DARED EACH OTHER TO SHOVE THAT STONE FOR THIRTY YEARS."
  opt "READ THEM THE LAST LINE." -> end_common
  opt "TELL THEM IT FAVOURS {HILL}." -> end_hill
  opt "TELL THEM IT FAVOURS {LOW}." -> end_low

stage end_common
  end success
  say "{HILL}: THE LITTLE DEVILS. I MOVED IT TWICE MYSELF, AS A BOY. {LOW}: I KNOW. I MOVED IT BACK. {GIVER}: THEY'RE LAUGHING. BOTH OF THEM. I'LL HAVE THE TABLET SET IN THE STONE ITSELF, WHERE NO CHILD CAN SHIFT IT."
  journal "THE OATH TABLET WAS READ ON THE GREEN OF {HOME}. THE STRIP IS COMMON GRAZING, AND THE FEUD IS OVER."
  do take "THE OATH TABLET"
  do reward fair
  do census home mood 10
  do census home price livestock 90
  do remember hill "COMMON GROUND. I GRAZE IT ON MONDAYS, SHE GRAZES IT ON THURSDAYS. WE ARGUE ABOUT SUNDAYS."
  do remember low "THE OATH STONE MARKS COMMON GROUND. I NEVER THOUGHT I'D BE GLAD OF IT."
  do fact "THE SETTLERS' OATH TABLET WAS READ ON THE GREEN OF {HOME}. THE STRIP BETWEEN THE HILL AND LOW FARMS IS COMMON GRAZING."

stage end_hill
  end success
  say "I READ IT AS YOU SAID, {GIVER.GOD} FORGIVE ME. {HILL} HAS HIS STRIP AND A SMILE LIKE A FOX. {LOW} WALKED OFF WITHOUT A WORD. THE STONE WON'T MOVE AGAIN. SHE WILL, I THINK."
  journal "THE OATH TABLET WAS READ FALSELY ON THE GREEN OF {HOME}. {HILL} HAS THE STRIP. {LOW} DOES NOT FORGIVE."
  do take "THE OATH TABLET"
  do reward small
  do deed lied
  do census home mood -2
  do remember low "THE BRONZE SAID WHAT THEY WANTED IT TO SAY. I KNOW WHO WANTED WHAT."
  do fact "{GIVER} OF {HOME} READ THE SETTLERS' OATH TABLET IN FAVOUR OF THE HILL FARM. THE LOW FARM CALLS IT A LIE."

stage end_low
  end success
  say "I READ IT AS YOU SAID, AND MAY {GIVER.GOD} FORGIVE ME. {LOW} HAS HER CLOVER, AND {HILL} SAYS THE BRONZE WAS FORGED BY HER GREAT-GRANDMOTHER. HE'S HALF RIGHT ABOUT SOMETHING, FOR ONCE."
  journal "THE OATH TABLET WAS READ FALSELY ON THE GREEN OF {HOME}. {LOW} HAS THE STRIP. {HILL} SWEARS VENGEANCE."
  do take "THE OATH TABLET"
  do reward small
  do deed lied
  do census home mood -2
  do remember hill "FORGED BRONZE. I'D SWEAR TO IT. NOBODY ELSE WILL, BUT I WOULD."
  do fact "{GIVER} OF {HOME} READ THE SETTLERS' OATH TABLET IN FAVOUR OF THE LOW FARM. THE HILL FARM CALLS IT A FORGERY."

stage blows
  end fail
  say "NO BRONZE CAME. {HILL} AND {LOW} CAME TO BLOWS AT THE WALL, RAKE AGAINST HOE, AND THE WHOLE VILLAGE TOOK SIDES."
  journal "THE FEUD CAME TO BLOWS AT THE WALL."
  do take "THE OATH TABLET"
  do census home mood -6
  do remember giver "THEY FOUGHT AT THE WALL. I STOOD BETWEEN THEM AND GOT A RAKE IN THE SHIN FOR MY TROUBLE."
  do fact "THE FEUD BETWEEN THE HILL AND LOW FARMS OF {HOME} CAME TO BLOWS OVER A STANDING STONE."

stage refuse
  end fail
  journal "YOU LEFT {GIVER} OF {HOME} TO THE BOUNDARY FEUD."
  do remember giver "MOVE THE STONE, YOU SAID. THEY'VE BEEN DOING THAT FOR THIRTY YEARS."
)DSL",

R"DSL(
script rq_longdrove
title THE LONG DROVE
archetype village request: a drove to market
tier 2
family request
settle village
needs season autumn
once site
cooldown 6
gossip "THE HAY MAN WANTS HIS COIN BY SNOW."
gossip "THAT BELL WAS THE OLD DROVER'S."
hook npc farmer
pitch "THAT'S A FINE BELL ON THAT EWE."
hint "SIX SHEEP, ONE DEBT, ONE OLD MAN SHOUTING THROUGH A SHUTTER. IF YOU KNOW THE ROAD TO MARKET, I MAY HAVE WORK FOR YOU."
role giver giver
role home site home
role town site town near
role lad person male young at home
role toll person male at town
role buyer npc merchant at town
var sheep 6
var bramble 1

stage start
  talk giver
  say "THE HAY FAILED THIS SUMMER, SO THE FLOCK CAN'T WINTER WHOLE, AND OUR DEBT TO THE HAY MAN FALLS DUE AT FIRST SNOW. SIX HEAD SOLD IN {TOWN} CLEARS BOTH. MY BOY'S GRANDFATHER DROVE SHEEP FORTY YEARS. HE'S LAID UP WITH A BROKEN LEG."
  opt "I'LL DRIVE THEM FOR YOU." -> who
  opt "CAN'T YOU HIRE A DROVER?" -> hire
  opt "SHEEP ARE NOT MY TRADE." -> refuse

stage hire
  talk giver
  say "EVERY DROVER IN THE DALE IS ON THE ROAD THIS MONTH, AND THE OLD MAN WON'T LET A STRANGER TOUCH BRAMBLE, THE LEAD EWE WITH HIS BELL. YOU'RE A STRANGER TOO. BUT YOU'RE A STRANGER WHO ASKED."
  opt "THEN I'LL DRIVE THEM." -> who

stage who
  talk giver
  say "MY BOY {LAD} WALKS WITH YOU; BRAMBLE KNOWS HIM. SIX SHEEP TO {TOWN}, AND BRAMBLE COMES HOME AFTER, SHE'S NOT FOR SALE. TAKE THE OLD MAN'S DROVER'S TAG. THE GATE FOLK KNOW IT. AND MIND HER AT THE GATE: SHE BOLTS AT NOISE."
  opt "WE'LL BE BACK BEFORE THE SNOW." -> drove

stage drove
  goal escort lad to town drove
  then gate
  else lad_lost
  deadline 5 -> late
  do give "THE OLD DROVER'S TAG" key
  do follow lad
  say "BRAMBLE LEADS. THE REST FOLLOW HER BELL. KEEP THE BOY CLOSE."
  journal "DRIVE SIX SHEEP OF {GIVER} TO MARKET IN {TOWN} WITH {LAD}. BRAMBLE THE BELL-EWE LEADS AND COMES HOME. MARKET CLOSES IN FIVE DAYS."

stage gate
  talk toll
  say "HOLD THERE. LIVESTOCK THROUGH THE GATE OF {TOWN} PAYS TOLL: A SHEEP OR TEN GOLD, YOUR CHOICE AND MY PLEASURE. AND KEEP THAT BELL-EWE STILL, SHE'S EYEING MY CART."
  opt "HERE'S TEN GOLD." if gold 10 -> clatter
  opt "TAKE A SHEEP, THEN." -> give_sheep
  opt "SHOW HIM THE DROVER'S TAG." if have "THE OLD DROVER'S TAG" -> tag

stage give_sheep
  talk toll
  say "A FAT ONE, NOT THE BELL-EWE. THAT ONE. MUCH OBLIGED. GO ON THROUGH, AND MIND THE CART."
  do add sheep -1
  opt "THROUGH WE GO." -> clatter

stage tag
  talk toll
  say "THAT'S HIS TAG. THE OLD MAN'S. HE'S ALIVE? BROKE HIS LEG? HA! THAT'S BRAMBLE, THEN. SHE BIT ME IN THIRTY-ONE. PASS, AND FREE. HE OWES ME ONE, AND NOW I OWE HIM TWO. TELL HIM."
  opt "I'LL TELL HIM." -> clatter

stage clatter
  talk lad
  say "THE CART! THE CARTER DROPPED A BARREL AND BRAMBLE'S BOLTED BACK DOWN THE ROAD, BELL AND ALL! IF SHE GETS TO THE WOODS WE'LL NEVER CATCH HER. WHAT DO WE DO?"
  opt "WALK AFTER HER SLOWLY. CALL HER." check level 2 -> caught else lost_ewe
  opt "LET HER GO. SELL THE OTHERS." -> lost_ewe

stage caught
  talk lad
  say "YOU WALKED RIGHT UP AND SHE JUST STOOD THERE! GRANDDAD SAYS NEVER RUN AT A SHEEP, AND YOU DIDN'T. SHE'S SHAKING BUT SHE'S HERE."
  opt "NOW TO MARKET." -> market

stage lost_ewe
  talk lad
  say "SHE'S GONE INTO THE WOODS. HER BELL'S STOPPED. GRANDDAD WILL ASK. WHAT DO I TELL HIM?"
  do set bramble 0
  opt "WE'LL THINK OF SOMETHING." -> market

stage market
  talk buyer
  say "SHEEP? A RIVAL SOLD TWENTY CHEAP THIS MORNING, SO I OFFER THE LOW PRICE, AND IT'S FAIR. ...WAIT. THAT BOY IS THE OLD DROVER'S GRANDSON. I KNOW THAT NOSE. HOW MANY HAVE YOU BROUGHT ME?"
  opt "ALL SIX. HIS PRICE FOR HIS FLOCK." if is sheep 6 -> sold_six
  opt "FIVE. THE GATE HAD ONE." if is sheep 5 -> sold_five

stage sold_six
  goal escort lad to home drove
  then home_six
  else lad_lost
  deadline 4 -> late_home
  do ambush wolf 2
  do follow lad
  say "SIX FOR THE OLD MAN'S PRICE. TAKE HIS COIN HOME. AND WATCH THE ROAD, WOLVES HAVE BEEN AT THE STRAGGLERS."
  journal "TAKE {LAD} AND THE PRICE OF SIX SHEEP HOME TO {GIVER} IN {HOME}. WOLVES FOLLOW THE ROAD OUT OF {TOWN}."

stage sold_five
  goal escort lad to home drove
  then home_five
  else lad_lost
  deadline 4 -> late_home
  do ambush wolf 2
  do follow lad
  say "FIVE, AT THE LOW PRICE BUT HONEST. TAKE IT HOME. AND WATCH THE ROAD, WOLVES HAVE BEEN AT THE STRAGGLERS."
  journal "TAKE {LAD} AND THE PRICE OF FIVE SHEEP HOME TO {GIVER} IN {HOME}. WOLVES FOLLOW THE ROAD OUT OF {TOWN}."

stage home_six
  talk giver
  say "{LAD}! AND THE COIN, ALL OF IT? THE DEBT'S PAID TONIGHT, THEN. LISTEN: THE OLD MAN'S HEARD THE BELL."
  opt "BRAMBLE WALKED HOME WITH US." if is bramble 1 -> end_bell
  opt "BRAMBLE BOLTED AT THE GATE." if is bramble 0 -> end_nobell

stage home_five
  talk giver
  say "{LAD}! FIVE SOLD? THE GATE TOOK ONE, DID IT. IT'S NEAR ENOUGH; THE HAY MAN WILL TAKE THE REST IN WOOL. LISTEN, THE OLD MAN'S CALLING."
  opt "BRAMBLE WALKED HOME WITH US." if is bramble 1 -> end_bell_five
  opt "BRAMBLE BOLTED AT THE GATE." if is bramble 0 -> end_nobell

stage end_bell
  end success
  say "I'VE HUNG THE BELL BY HIS SHUTTERED WINDOW. FROM INSIDE, THE OLD MAN'S VOICE: FORTY YEARS. NEVER LOST HER. THE DEBT'S PAID, AND HE'LL SLEEP TONIGHT. SO WILL I. TAKE THE CLOAK; IT WAS HIS DROVING CLOAK."
  journal "SIX SHEEP SOLD IN {TOWN} AT THE OLD DROVER'S PRICE. THE HAY DEBT OF {GIVER} IS PAID, AND BRAMBLE'S BELL RINGS AT HOME."
  do take "THE OLD DROVER'S TAG"
  do give "A WOOLLEN DROVER'S CLOAK" cloak
  do reward fair 120
  do census home mood 8
  do census home price livestock 90
  do remember giver "BRAMBLE'S BELL RINGS AGAIN. THE OLD MAN COUNTS THE FLOCK BY IT FROM HIS BED."
  do fact "A TRAVELLER DROVE SIX SHEEP OF {HOME} TO MARKET IN {TOWN} AND PAID A FARM'S HAY DEBT. THE OLD DROVER'S BELL-EWE CAME HOME."

stage end_bell_five
  end success
  say "THE BELL BY THE WINDOW AGAIN. THE OLD MAN CALLED OUT TO ASK IF ALL SIX SOLD. I SAID FIVE AND A TOLL, AND HE SWORE AT THE GATE FOR A FULL MINUTE. THAT'S HOW I KNOW HE'S MENDING."
  journal "FIVE SHEEP SOLD IN {TOWN}. MOST OF THE HAY DEBT OF {GIVER} IS PAID, AND BRAMBLE CAME HOME."
  do take "THE OLD DROVER'S TAG"
  do give "A WOOLLEN DROVER'S CLOAK" cloak
  do reward fair 100
  do census home mood 6
  do census home price livestock 95
  do remember giver "FIVE AND A TOLL. HE STILL SWEARS ABOUT THE GATE. BRAMBLE'S BELL RINGS, THOUGH."
  do fact "A TRAVELLER DROVE SHEEP OF {HOME} TO MARKET IN {TOWN}. MOST OF A FARM'S HAY DEBT WAS PAID, AND THE BELL-EWE CAME HOME."

stage end_nobell
  end success
  say "THE DEBT'S PAID, OR NEAR IT. HE ASKED WHERE HER BELL WAS. I SAID RESTING. HE KNOWS. HE'S NOT A FOOL, HE'S JUST OLD. THANK YOU. TRULY. IT WASN'T YOUR FAULT."
  journal "THE SHEEP OF {GIVER} WERE SOLD IN {TOWN} AND THE DEBT IS PAID. BRAMBLE WAS LOST AT THE GATE."
  do take "THE OLD DROVER'S TAG"
  do reward fair 80
  do census home mood 3
  do census home price livestock 95
  do remember giver "HE ASKS AFTER HER BELL SOME NIGHTS. I SAY RESTING. WE BOTH KNOW."
  do fact "A TRAVELLER DROVE SHEEP OF {HOME} TO MARKET IN {TOWN}. THE OLD DROVER'S BELL-EWE BOLTED AT THE GATE AND WAS LOST."

stage lad_lost
  end fail
  say "{LAD} NEVER CAME HOME. THE SHEEP SCATTERED, AND THE HAY MAN TOOK THE BEST FIELD AT FIRST SNOW."
  journal "{LAD} WAS KILLED ON THE ROAD. THE FLOCK WAS LOST."
  do take "THE OLD DROVER'S TAG"
  do census home stock livestock -6
  do census home price meat 115
  do census home mood -8
  do remember giver "I SENT MY BOY WITH YOU. I SENT HIM WITH YOU."
  do fact "A BOY OF {HOME} DIED ON THE DROVE ROAD TO {TOWN}. HIS FAMILY LOST THE FLOCK AND THE BEST FIELD."

stage late
  end fail
  say "THE MARKET WINDOW CLOSED. THE SHEEP WALKED BACK TO THE PEN, AND THE FIRST SNOW CAME, AND THE HAY MAN WITH IT."
  journal "THE FIRST SNOW CAME. THE HAY MAN TOO."
  do take "THE OLD DROVER'S TAG"
  do census home price livestock 75
  do census home mood -3
  do remember giver "HALF THE FLOCK TO THE HAY MAN, CHEAP. WE'LL WINTER. JUST."
  do fact "A FARM OF {HOME} MISSED THE AUTUMN MARKET AND SOLD HALF HER FLOCK CHEAP TO PAY A HAY DEBT."

stage late_home
  end fail
  say "THE COIN CAME HOME TOO LATE: THE HAY MAN HAD ALREADY TAKEN HALF THE FLOCK IN LIEU, AND WOULDN'T GIVE IT BACK."
  journal "THE SHEEP SOLD, BUT THE COIN REACHED {HOME} AFTER THE SNOW AND THE HAY MAN."
  do take "THE OLD DROVER'S TAG"
  do census home price livestock 80
  do census home mood -2
  do remember giver "THE COIN CAME. THE HAY MAN CAME FIRST. THAT'S THE WAY OF IT, I SUPPOSE."
  do fact "A FARM OF {HOME} SOLD ITS SHEEP IN {TOWN}, BUT THE HAY MAN TOOK HALF THE FLOCK BEFORE THE COIN CAME HOME."

stage refuse
  end fail
  journal "YOU WOULD NOT DRIVE THE SHEEP OF {GIVER} TO MARKET."
  do remember giver "THE HAY MAN WON'T WAIT, AND NEITHER WOULD YOU."
)DSL",

R"DSL(
script rq_turnipthief
title THE TURNIP THIEVES
archetype village request: refugees
tier 2
family request
settle village
needs season summer autumn
once site
cooldown 6
gossip "WHO TWISTS THE TOPS OFF TURNIPS?"
gossip "SOMEONE DROPPED A WOODEN HORSE."
hook npc villager
pitch "WHY SLEEP IN THE FIELD WITH A SHOVEL?"
hint "IF YOU SEE ANYONE IN MY TURNIPS AFTER DARK, SHOUT. I'VE GOT DOGS. I'D RATHER NOT USE THEM. I'D RATHER NOT."
role giver giver
role home site home
role ruin site ruin near
role mother person female at ruin

stage start
  talk giver
  say "TURNIPS GONE, THREE NIGHTS RUNNING. ONE ROW, TOPS TWISTED OFF, NOT DUG. SMALL FEET, NO BIGGER THAN A CHILD'S. AND THIS IN THE ROW: A CARVED WOODEN HORSE. THE TRACKS RUN TO {RUIN}. I HAVE DOGS. I'D RATHER NOT."
  opt "I'LL FOLLOW THE TRACKS." -> follow
  opt "LOOSE THE DOGS, THEN." -> dogs
  opt "A FEW TURNIPS. LET IT GO." -> refuse

stage dogs
  talk giver
  say "AND SET DOGS ON A CHILD? I'M NOT THAT SORT. NOT YET. BUT ANOTHER WEEK OF THIS AND I MIGHT BE. GO AND SEE WHO IT IS. PLEASE. BEFORE I FIND OUT WHAT SORT I AM."
  opt "I'LL GO. KEEP THE DOGS IN." -> follow

stage follow
  goal goto ruin
  then mother
  deadline 3 -> loosed
  do give "A CARVED WOODEN HORSE" bone
  say "TAKE THE HORSE. WHOEVER LOST IT WILL WANT IT BACK."
  journal "SOMEONE SMALL PULLS TURNIPS BY NIGHT IN {HOME}. FOLLOW THE TRACKS TO {RUIN} BEFORE {GIVER} LOOSES THE DOGS."

stage mother
  talk mother
  say "TAKE ME, NOT THEM. THEY OBEYED ME. I'M {MOTHER}. OUR VILLAGE BURNED; WE RAN WITH NOTHING. MY ANKLE BROKE ON THE ROAD, SO I SENT MY THREE. ONE ROW. PULL, DON'T DIG. I TAUGHT THEM. THE LITTLEST HASN'T SPOKEN SINCE. YOU HAVE HIS HORSE."
  opt "GIVE THE HORSE BACK." -> horse

stage horse
  talk mother
  say "HE'S HOLDING IT LIKE IT'S MADE OF GOLD. THANK YOU. I'LL WORK ANY ROW IF THE FARMER LETS ME, ONCE THIS ANKLE BEARS ME. TELL {GIVER.HIM} THAT. SAY WHATEVER YOU MUST, BUT SAY THAT."
  do take "A CARVED WOODEN HORSE"
  opt "I'LL TALK TO HIM." -> verdict

stage verdict
  talk giver
  say "WELL? WHO'S BEEN IN MY TURNIPS? AND DON'T TELL ME FOXES. FOXES DON'T TWIST."
  deadline 3 -> loosed
  opt "A FAMILY. I'LL PAY FOR THE CROP." if gold 30 -> wages
  opt "PEDLAR'S CHILDREN. MOVING ON." -> blind
  opt "TELL THEM TO GO. I'LL DO IT." -> drive

stage wages
  talk giver
  say "A MOTHER AND THREE, BURNED OUT, IN THAT RUIN? AND YOU'D PAY FOR TURNIPS THAT AREN'T YOURS. ...PUT YOUR PURSE AWAY. NO. TAKE IT BACK OUT. FINE. THEN SHE WORKS MY ROWS FOR WAGES WHEN THAT ANKLE MENDS."
  opt "AND THE LITTLEST?" -> end_wages

stage blind
  talk giver
  say "PEDLARS. WITH CHILDREN. WHO TWIST TURNIPS. ...HM. I'LL LEAVE THE EAST ROW UNWATCHED A WHILE, THEN. PEDLARS GET HUNGRY. DON'T TELL ME ANY MORE THAN THAT."
  opt "I WON'T." -> end_blind

stage drive
  goal goto ruin
  then send_off
  journal "TELL THE FAMILY HIDING IN {RUIN} THAT THEY MUST LEAVE THE FIELDS OF {HOME} ALONE, AND GO."

stage send_off
  talk mother
  say "GO? WHERE? ...NO. DON'T ANSWER. I KNOW WHAT IT IS TO HAVE NO ANSWER. WE'LL BE GONE BY DARK. I'LL CARRY HIM IF I MUST, ANKLE OR NO."
  opt "I'M SORRY." -> end_drive

stage end_wages
  end success
  say "LET THE LITTLEST KEEP THE HORSE, AND TELL HIM THE TURNIPS ARE ON ME. HERE. NO, THIS ISN'T FOR THE TURNIPS. IT'S FOR SPARING ME THE DOGS."
  journal "{MOTHER} AND HER THREE CHILDREN, BURNED OUT OF THEIR VILLAGE, LIVE IN {HOME} NOW. SHE WILL WORK THE ROWS FOR WAGES."
  do gold -30
  do reward small
  do deed charity
  do census home refugees 4
  do census home mood 6
  do remember giver "SHE WORKS A ROW FASTER THAN I DO, ANKLE AND ALL. THE LITTLEST SPEAKS NOW. MOSTLY ABOUT HORSES."
  do remember mother "WE HAVE A ROOF. I DON'T KNOW HOW TO THANK YOU, SO I WON'T TRY."
  do fact "A FAMILY BURNED OUT OF THEIR VILLAGE FOUND A HOME IN {HOME}, WORKING A FARMER'S ROWS FOR WAGES."

stage end_blind
  end success
  say "PEDLARS, I SAID, AND THAT'S WHAT IT WAS. HERE, FOR YOUR TROUBLE. I SLEPT INSIDE LAST NIGHT. FIRST TIME IN A WEEK."
  journal "{GIVER} LEAVES THE EAST ROW UNWATCHED. THE FAMILY IN {RUIN} WILL MOVE ON WHEN {MOTHER} CAN WALK."
  do reward small
  do deed lied
  do remember mother "THE EAST ROW IS NEVER WATCHED. I DON'T ASK WHY. WE MOVE ON AT THE NEW MOON."
  do fact "A GROWER OF {HOME} LEAVES ONE ROW OF TURNIPS UNWATCHED FOR A FAMILY {GIVER.HE} PRETENDS ARE PEDLARS."

stage end_drive
  end success
  say "THEY'RE GONE, YOU SAY? GOOD. THAT'S... GOOD. HERE. TAKE IT. I DON'T FEEL AS GOOD ABOUT IT AS I THOUGHT I WOULD."
  journal "YOU SENT {MOTHER} AND HER CHILDREN OUT OF {RUIN}. THE TURNIPS OF {GIVER} ARE SAFE."
  do reward small
  do deed cruelty
  do hide mother
  do remember giver "THE TURNIPS ARE SAFE. I KEEP LOOKING AT THE EAST ROW ANYWAY."
  do fact "A BURNED-OUT FAMILY WAS SENT AWAY FROM THE RUIN OF {RUIN} NEAR {HOME} FOR PULLING TURNIPS."

stage loosed
  end fail
  say "ON THE THIRD NIGHT {GIVER} LET THE DOGS LOOSE. A CHILD WAS BITTEN, AND THE FAMILY FLED INTO THE WOODS. THE VILLAGE TALKS OF NOTHING ELSE."
  journal "{GIVER} SAID NO DOGS. THEN LOOSED THEM."
  do take "A CARVED WOODEN HORSE"
  do hide mother
  do census home mood -8
  do remember giver "I SAID I WASN'T THAT SORT. I WAS WRONG. I HEARD THE CHILD SCREAM."
  do fact "A GROWER OF {HOME} SET DOGS ON A BURNED-OUT FAMILY PULLING TURNIPS BY NIGHT. A CHILD WAS BITTEN."

stage refuse
  end fail
  journal "YOU LEFT {GIVER} ALONE WITH {GIVER.HIS} DOGS AND TURNIPS."
  do remember giver "LET IT GO, YOU SAID. TELL THAT TO MY DOGS."
)DSL",
};

}  // namespace

const char* requestsVillageSource() {
  static const std::string joined = [] {
    std::string s;
    for (const char* p : kVillage) s += p;
    return s;
  }();
  return joined.c_str();
}

}  // namespace dsl
}  // namespace story
