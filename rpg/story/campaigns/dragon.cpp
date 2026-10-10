// M6b "Sagas": THE TONGUE OF THE BEAST (campaign `dragon`): the dragon cult. CAMPAIGNS lane. Ties to the kingdom cell's
// M6 world boss (role `boss`: ASHFANG the dragon, a frost giant, a lich, a wyrm, a behemoth: {WYRM} and {WYRM.KIND} are
// whatever really roams there). Themes from myth and scripture (the idol fed with the children of the town, the
// prophet against the priests of the beast, the slayer of the serpent) and the world sims (raids, tribute, famine).
//
// Hook: a fresh beast raid (a realm event) on a settlement. Among the ashes someone has painted a red eye on the doors
// that did not burn. A cult in the hills, led by one who calls themselves the beast's Tongue, teaches that the beast
// spares those who feed it: grain, gold, and now and then a person. The town's hunter, who lost her family in the raid,
// wants the beast dead. The cult's knife wants the hunter dead. The Tongue is more afraid than anyone.
//   head      the hunter in the ashes; a real resident who lost someone
//   arc 1     THE ASHES                the doors with the red eye | the list in the burned shrine
//   arc 2     THE CULT                 the sermon at the ash camp | the tribute cart
//   arc 3     THE KNIFE                the knife in the night (a betrayal) | the Tongue's fear
//   arc 4     THE LAIR                 the hunter's poisoned offering | the offering cave
//   finale    the last tribute: slay the beast (its hoard's craft saved) | pay the tribute with another town's grain
//             (that town starves) | the Tongue leads it away (the town rebuilds) | the cult sends it on the next town
//             (it burns; a failure)
// Recurring: the hunter, the mourner, the Tongue, the knife; the beast; the cult's camp, the offering cave, the next town.
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title [[THE RED EYE|THE ASH CAMP|THE TONGUE OF THE BEAST]]
hook event beastraid
pitch "WHO PAINTED THAT EYE ON THE DOOR?"
hint "THE SMOKE IS STILL RISING OVER THE ROOFS THAT FELL. ON THE DOORS THAT DIDN'T, SOMEONE HAS PAINTED A RED EYE."
role giver giver
role home site home
role land kingdom home
role wyrm boss
role hunter person female at home
role mourner resident any
role ashcamp site camp near
role tongue person [[male|female]] at ashcamp
role knife foe [[female|male]] at home
role lair site cave near
role town site town near
var fed 0
var trust 0
var poison 0

stage ashes
  talk hunter
  say "{WYRM} CAME DOWN AT DUSK. THE {WYRM.KIND} THEY TELL STORIES ABOUT TO MAKE CHILDREN BEHAVE. I WAS IN THE HILLS, CHECKING SNARES. WHEN I GOT BACK, MY HOUSE WAS A CHIMNEY. NOW LOOK AT THE DOORS THAT STOOD. THE RED EYE."
  journal "{WYRM}, THE {WYRM.KIND}, HAS RAIDED {HOME}. ON THE DOORS THAT DID NOT BURN, SOMEONE PAINTED A RED EYE."
  do hide knife
  opt "WHAT DOES THE EYE MEAN?" -> eye
  opt "I'M SORRY FOR YOUR HOUSE." -> sorry
  opt "BEASTS RAID. TOWNS REBUILD." -> cold

stage sorry
  talk hunter
  say "MY HOUSE. MY HUSBAND. MY BOY, WHO WAS SEVEN AND WANTED TO BE A {WYRM.KIND} WHEN HE GREW UP. DON'T BE SORRY. HELP ME FIND OUT WHO PAINTED THOSE EYES, AND WHY THEIR DOORS ARE STILL STANDING."
  opt "WHAT DOES THE EYE MEAN?" -> eye

stage eye
  talk hunter
  say "IT MEANS SOMEONE KNEW. THE EYE IS THE MARK OF THE ASH CAMP IN THE HILLS: PEOPLE WHO PRAY TO {WYRM} AND FEED IT GRAIN AND GOLD, SO IT PASSES THEM BY. THEIR LEADER CALLS HIMSELF ITS TONGUE. OR HERSELF. NOBODY'S SURE. <<curse:vengeance>>"
  opt "THEN I'LL FIND THE TONGUE." -> @next
  opt "[[SHOW ME THE DOORS.|WHO HERE HAS AN EYE ON THEIR DOOR?]]" -> @next

stage cold
  end fail
  say "THE HUNTER LOOKS AT YOU LIKE A WOLF LOOKS AT A DOG: ALMOST THE SAME ANIMAL, AND NOT AT ALL. THEN SHE GOES BACK TO DIGGING IN THE ASHES OF HER HOUSE WITH HER BARE HANDS."
  journal "YOU LEFT {HOME} TO ITS ASHES AND ITS RED-EYED DOORS."
  do remember hunter "TOWNS REBUILD. YOU SAID THAT. I'M STILL DIGGING."
)SAGA";

const char* const kFinale = R"SAGA(
stage offering
  talk tongue
  do moves tongue lair
  do moves hunter lair
  ?arc_dr_poison say "THE LAST TRIBUTE IS LAID IN THE CAVE MOUTH: THE CARTS, THE GRAIN, THE GOLD. {WYRM} COMES AT DUSK. THE HUNTER'S POISON IS IN THE GRAIN, OR IT ISN'T. WHATEVER WE DO, WE DO IT NOW. {PLAYER}, I'M LISTENING. FOR ONCE."
  ?arc_dr_hoard say "THE LAST TRIBUTE IS LAID IN THE CAVE MOUTH: THE CARTS, THE GRAIN, THE GOLD. {WYRM} COMES AT DUSK. YOU'VE SEEN THE BONES IN THERE NOW. YOU KNOW WHAT IT EATS WHEN THE GRAIN RUNS OUT. WHAT DO WE DO?"
  opt "WE KILL IT. TONIGHT." -> slay
  opt "PAY IT. WITH {TOWN}'S GRAIN." -> tribute
  opt "LEAD IT AWAY. YOU KNOW HOW." check var trust 2 -> led_away else led_fail
  opt "DO AS YOUR CULT SAYS." -> cult_wins

stage slay
  goal slay wyrm
  then slain
  say "{WYRM} COMES DOWN OUT OF THE DUSK LIKE THE END OF THE WORLD. {HUNTER} DOES NOT RUN. NEITHER DO YOU."
  journal "{WYRM}, THE {WYRM.KIND}, COMES TO {LAIR} FOR ITS TRIBUTE. IT WILL FIND YOU THERE INSTEAD."

stage slain
  end success
  say "IT TOOK ALL NIGHT. AT DAWN {HUNTER} SAT ON THE GREAT COOLING HEAD AND WEPT, FOR {HUNTER.HIS} BOY AND NOT FOR THE BEAST. IN THE CAVE BEHIND, UNDER A CENTURY OF TRIBUTE, LAY THE TOOLS OF SMITHS IT HAD EATEN, AND THEIR SECRETS WITH THEM."
  journal "{WYRM}, THE {WYRM.KIND}, IS DEAD AT {LAIR}. THE RED EYES ARE BEING SCRUBBED FROM THE DOORS OF {HOME}."
  do secret lair
  do reward great
  do fame 6
  do mark dragon_slain
  do remember hunter "I KEPT A TOOTH. I DON'T KNOW WHY. I PUT IT ON MY BOY'S GRAVE. IT LOOKED SMALLER THERE."
  do remember mourner "{HOME} WILL HAVE A FEAST DAY FOR YOU, IF YOU LET IT. MOST OF US WILL CRY THROUGH IT. COME ANYWAY."
  do fact "{WYRM} THE {WYRM.KIND} WAS SLAIN AT THE CAVE OF {LAIR} ON THE NIGHT OF ITS LAST TRIBUTE, BY A STRANGER AND {HUNTER} OF {HOME}."

stage tribute
  talk tongue
  say "{TOWN}'S GRAIN. A WHOLE TOWN'S STORE, TO FEED IT A WHOLE YEAR, AND {HOME} SAFE, AND NO ONE ELSE GOES INTO THE CAVE MOUTH. ...{TOWN} WILL STARVE, YOU KNOW. SLOWLY. NOBODY WILL PAINT AN EYE FOR THEM. ARE YOU SURE?"
  opt "I'M SURE." -> tribute_end
  opt "NO. THEN WE FIGHT." -> slay

stage tribute_end
  end success
  say "THE CARTS WENT OUT FROM {TOWN} UNDER GUARD, FULL, AND CAME BACK EMPTY. {WYRM} DID NOT COME TO {HOME} THAT YEAR, OR THE NEXT. IN {TOWN} THEY ATE THE SEED CORN BY MIDWINTER, AND THEN THE DOGS."
  journal "{HOME} IS SAFE FROM {WYRM}, BOUGHT WITH THE GRAIN OF {TOWN}. {TOWN} IS STARVING."
  do realm famine town
  do reward rich
  do mark dragon_tribute
  do remember hunter "{HOME}'S SAFE. I KNOW WHAT IT COST. I CAN'T LOOK AT A LOAF OF BREAD WITHOUT KNOWING."
  do fact "{HOME} PAYS {WYRM} THE {WYRM.KIND} WITH GRAIN TAKEN FROM {TOWN}, AND {TOWN} STARVES FOR IT."

stage led_fail
  talk tongue
  say "LEAD IT? I'VE NEVER LED IT ANYWHERE. I FED IT AND I PRAYED IT WOULDN'T NOTICE ME. THE WORDS, THE RITES, I MADE THEM UP. ALL OF THEM. ...I'M SORRY. I'M SO SORRY. IT'S COMING. I CAN HEAR IT."
  opt "THEN WE FIGHT." -> slay
  opt "THEN RUN, ALL OF YOU." -> cult_wins

stage led_away
  talk tongue
  say "...THERE'S A WAY. THE OLD TONGUES KNEW IT: YOU STAND IN THE CAVE MOUTH AND YOU SING THE NAME IT HAD BEFORE IT WAS A BEAST, AND IT REMEMBERS BEING YOUNG, AND IT GOES HOME. NORTH, OVER THE MOUNTAINS. IF I SING IT WRONG, IT EATS ME."
  opt "SING IT RIGHT." -> led_end
  opt "NO. WE FIGHT." -> slay

stage led_end
  end success
  say "{TONGUE} STOOD IN THE CAVE MOUTH AND SANG, WHITE TO THE LIPS. {WYRM} LISTENED A LONG TIME. THEN IT LIFTED INTO THE DARK AND WENT NORTH, AND DID NOT COME BACK THAT YEAR. THE ASH CAMP CAME DOWN TO {HOME} AND HELPED REBUILD WHAT THEY HAD WATCHED BURN."
  journal "{WYRM} HAS GONE NORTH, LED AWAY BY {TONGUE}'S SONG. THE CULT HAS COME DOWN TO HELP REBUILD {HOME}."
  do realm event resettled land home
  do reward rich
  do mark dragon_led
  do remember tongue "I SANG IT RIGHT. I DIDN'T KNOW I COULD. I'M STILL SHAKING. I THINK I'LL SHAKE FOR A YEAR."
  do remember hunter "{TONGUE} HELPED BUILD MY NEW HOUSE. I HAVEN'T FORGIVEN. BUT THE ROOF IS STRAIGHT."
  do fact "THE ASH CAMP'S {TONGUE} SANG {WYRM} THE {WYRM.KIND} AWAY TO THE NORTH. THE CULT NOW REBUILDS {HOME}."

stage cult_wins
  end fail
  say "THE TONGUE'S PEOPLE KNELT AND PAINTED A RED EYE ON THE CAVE ROCK, AND {WYRM} CAME, AND TOOK THE GRAIN, AND WAS STILL HUNGRY. IT WENT ON TO {TOWN}. NOBODY THERE HAD PAINTED AN EYE."
  journal "{WYRM} WENT ON FROM {LAIR} TO {TOWN}, AND {TOWN} BURNED. THE ASH CAMP HAS NEW BELIEVERS."
  do realm event townburned land town
  do mark dragon_cult
  do remember hunter "YOU STOOD THERE. YOU STOOD IN THE CAVE MOUTH AND LET THEM KNEEL. I'LL HUNT IT ALONE."
  do fact "{TOWN} WAS BURNED BY {WYRM} THE {WYRM.KIND} THE NIGHT AFTER THE ASH CAMP PAID ITS TRIBUTE."
)SAGA";

// ---- arc 1: the ashes
const char* const kRedDoors = R"SAGA(
stage %doors
  talk mourner
  say "THE EYE? ON MY DOOR? I DIDN'T PAINT IT. I SWEAR. I CAME HOME FROM THE FIELDS AND IT WAS THERE, AND MY HOUSE WAS STANDING, AND NEXT DOOR... NEXT DOOR WAS MY SISTER'S. THERE'S NOTHING THERE NOW BUT THE HEARTH-STONE."
  journal "{MOURNER}, THE {MOURNER.JOB}, HAS A RED EYE ON {MOURNER.HIS} DOOR AND A SISTER UNDER THE ASHES NEXT DOOR."
  opt "WHO HAS BEEN TO YOUR HOUSE LATELY?" -> %who
  opt "I BELIEVE YOU." -> %who
stage %who
  talk mourner
  say "NOBODY. A TINKER, IN SPRING. SHE SOLD ME A POT AND ASKED IF I WANTED TO BE SAFE. I LAUGHED. SAFE FROM WHAT? ...SHE SAID SHE'D PUT ME ON THE LIST ANYWAY, FOR FREE, BECAUSE I'D BEEN KIND. OH GODS. OH GODS, THE LIST."
  do remember mourner "I KEPT THE POT. I DON'T KNOW WHY. I CAN'T COOK IN IT."
  opt "SOMEONE CHOSE WHO BURNED." -> %chose
stage %chose
  talk hunter
  say "A LIST. THE ASH CAMP SENDS PEDLARS ROUND IN SPRING, AND THE KIND AND THE PAYING GO ON THE LIST, AND THE TONGUE TELLS THE BEAST WHICH DOORS TO PASS. THAT'S WHAT THEY BELIEVE. MY DOOR WASN'T ON IT. NOBODY ASKED ME TO BE KIND."
  opt "OR THE EYES ARE ONLY LUCK." -> %luck
  opt "THEN WE GO TO THE ASH CAMP." -> @next
stage %luck
  talk hunter
  say "LUCK. MAYBE. MAYBE A {WYRM.KIND} CAN'T READ PAINT, AND THE EYES ARE JUST WHERE THE WIND WAS BLOWING. DOES THAT MAKE IT BETTER? SOMEONE STILL SOLD SAFETY DOOR TO DOOR, AND PEOPLE STILL BOUGHT IT."
  opt "NO. IT DOESN'T. THE ASH CAMP." -> @next
)SAGA";

const char* const kShrineList = R"SAGA(
role %shrine site village near
stage %road
  goal goto %shrine
  then %find
  do moves hunter %shrine
  journal "THE RAID STARTED AT {%shrine}, {%shrine.DIR}, THEY SAY. THE HUNTER WANTS TO SEE WHAT IT LEFT."
stage %find
  talk hunter
  say "{%shrine} GOT IT FIRST. EVERYTHING BURNED BUT THIS: A LITTLE STONE SHRINE, RED EYE OVER THE DOOR. INSIDE, A LEDGER. NAMES, HOUSES, WHAT EACH ONE PAID. AND A COLUMN AT THE END, MARKED ONLY WITH A TICK OR A CROSS."
  do give "THE LEDGER OF THE RED EYE" book
  opt "WHAT DO THE CROSSES MEAN?" -> %cross
stage %cross
  talk hunter
  say "THE CROSSES ARE THE ONES WHO BURNED. EVERY ONE. THE TICKS ARE THE DOORS THAT STOOD. {MOURNER} OF {HOME} HAS A TICK. AND HERE, NEAR THE BOTTOM: {TOWN}. A WHOLE TOWN, ONE LINE, AND A CROSS. NEXT IN THE LEDGER."
  opt "THEN {TOWN} IS NEXT." -> %next
stage %next
  talk hunter
  say "UNLESS SOMEONE STOPS IT. THE LEDGER'S SIGNED, AT THE BACK, IN A NEAT HAND: {TONGUE}, OF THE ASH CAMP, SPEAKER FOR {WYRM}. I WANT TO MEET SOMEONE WITH HANDWRITING THAT NEAT. I WANT TO SEE THEIR HANDS."
  do remember hunter "THE LEDGER. I READ IT EVERY NIGHT. I DON'T KNOW WHAT I'M LOOKING FOR. MY BOY'S NAME, MAYBE. HE WASN'T IN IT."
  opt "THE ASH CAMP, THEN." -> @next
)SAGA";

// ---- arc 2: the cult
const char* const kSermon = R"SAGA(
stage %climb
  goal goto ashcamp
  then %sermon
  journal "THE ASH CAMP OF THE RED EYE LIES AT {ASHCAMP}, {ASHCAMP.DIR}. ITS TONGUE PREACHES AT SUNDOWN."
stage %sermon
  talk tongue
  say "SIT. EVERYONE IS WELCOME AT SUNDOWN. LISTEN: {WYRM} IS NOT A BEAST. IT IS A WEATHER, LIKE WINTER. YOU DON'T FIGHT WINTER. YOU STORE GRAIN. YOU PAY IT ITS DUE, AND IT PASSES. WE ARE NOT MONSTERS. WE ARE THE ONES WHO ARE STILL ALIVE."
  opt "AND THE ONES ON THE LEDGER WHO BURNED?" -> %ledger
  opt "TELL ME MORE." -> %more
stage %more
  talk tongue
  say "MORE? WE FEED IT AT THE CAVE OF {LAIR}: GRAIN AT EVERY NEW MOON, GOLD AT MIDSUMMER. IN THE BAD YEARS IT WANTS MORE THAN GRAIN. IN THE BAD YEARS SOMEONE WALKS INTO THE CAVE MOUTH. WE DRAW LOTS. I HAVE NEVER RIGGED THE LOTS."
  do add fed 1
  opt "HAVE YOU EVER DRAWN ONE?" -> %ledger
stage %ledger
  talk tongue
  say "...YOU HAVE SHARP EYES. YES. I SPEAK FOR IT, SO I DON'T DRAW. THAT'S THE RULE. I DIDN'T MAKE IT. I INHERITED IT. MY MOTHER WAS TONGUE BEFORE ME, AND SHE WALKED INTO THE CAVE MOUTH WHEN THE GRAIN RAN SHORT. SHE SAID IT WAS AN HONOUR."
  opt "YOU DON'T BELIEVE THAT." -> %doubt
  opt "IT SOUNDS LIKE MURDER." -> %murder
stage %doubt
  talk tongue
  say "I BELIEVE THAT {HOME} STILL HAS HALF ITS DOORS. I BELIEVE {TOWN} HAS ALL OF THEM. THAT'S WHAT I BELIEVE. EVERYTHING ELSE I SAY AT SUNDOWN BECAUSE THEY NEED ME TO. ...WHY AM I TELLING YOU THIS?"
  do add trust 1
  do remember tongue "I TOLD YOU SOMETHING AT SUNDOWN THAT I'VE NEVER TOLD ANYONE. I'M STILL DECIDING WHETHER TO REGRET IT."
  opt "BECAUSE YOU'RE TIRED OF IT." -> @next
stage %murder
  talk tongue
  say "IT IS MURDER. OF ONE, INSTEAD OF A HUNDRED. YOU THINK I DON'T KNOW? I DO THE SUM EVERY NIGHT. IT ALWAYS COMES OUT THE SAME. COME BACK WHEN YOU HAVE A BETTER SUM, STRANGER. UNTIL THEN, KEEP YOUR VOICE DOWN AT MY FIRE."
  opt "I'LL FIND A BETTER SUM." -> @next
)SAGA";

const char* const kTributeCart = R"SAGA(
stage %road
  talk hunter
  say "A CART'S COMING DOWN THE HILL ROAD FROM THE ASH CAMP TONIGHT, TO {LAIR}. THE NEW MOON TRIBUTE. I'VE WATCHED THEM THREE TIMES. TWO DRIVERS, ONE GUARD. THIS TIME THERE'S SOMETHING ELSE IN IT. SOMETHING THAT MOVES."
  journal "THE ASH CAMP'S TRIBUTE CART GOES TO {LAIR} AT THE NEW MOON. {HUNTER} SAYS SOMETHING IN IT MOVES."
  opt "WE STOP THE CART." -> %stop
  opt "WE FOLLOW IT." -> %follow
stage %stop
  goal kill 2 bandit
  then %bound
  say "THE CART CREAKS DOWN OUT OF THE DARK, LANTERNS SWINGING. THE GUARD SEES YOU AND REACHES FOR A HORN."
  journal "STOP THE TRIBUTE CART ON THE HILL ROAD TO {LAIR} BEFORE IT REACHES THE CAVE."
stage %bound
  talk mourner
  do moves mourner home
  say "...YOU. YOU FROM {HOME}. THEY CAME FOR ME LAST NIGHT. THEY SAID MY TICK WAS A DEBT, NOT A GIFT, AND THE DEBT WAS DUE. I DREW THE LOT. THEY SAID I DREW IT. I DON'T REMEMBER DRAWING ANYTHING."
  do add trust 1
  do remember mourner "I WAS IN THE CART. I WAS TIED IN THE CART WITH THE GRAIN, AND YOU OPENED IT. I'LL NEVER BE ABLE TO REPAY THAT."
  do befriend mourner
  opt "YOU'RE SAFE NOW. GO HOME." -> @next
stage %follow
  goal goto lair
  then %watch
  journal "FOLLOW THE TRIBUTE CART TO {LAIR}, {LAIR.DIR}, AND SEE WHAT THE ASH CAMP FEEDS {WYRM}."
stage %watch
  talk hunter
  do moves hunter lair
  say "THEY LEFT THE GRAIN IN THE CAVE MOUTH. AND THEY LEFT {MOURNER}, TIED, AND WALKED AWAY SINGING. I CUT {MOURNER.HIM} LOOSE BEFORE DUSK. IT CAME AN HOUR LATER, ATE THE GRAIN, AND SNIFFED AT THE ROPES FOR A LONG TIME."
  do add fed 1
  do moves mourner home
  opt "WE SHOULD HAVE STOPPED IT EARLIER." -> @next
  opt "NOW WE KNOW WHAT IT EATS." -> @next
)SAGA";

// ---- arc 3: the knife
const char* const kKnifeNight = R"SAGA(
stage %night
  goal slay knife
  then %after
  do show knife
  say "A SOUND AT THE WINDOW OF THE HOUSE WHERE {HUNTER} SLEEPS. A SHAPE WITH A KNIFE AND A RED EYE PAINTED ON ITS PALM."
  journal "THE ASH CAMP HAS SENT ITS KNIFE, {KNIFE}, TO {HOME} BY NIGHT, FOR {HUNTER}. STOP {KNIFE.HIM}."
stage %after
  talk hunter
  say "ANOTHER INCH AND I'D HAVE BEEN WITH MY BOY. ...{KNIFE.HE} HAD A NOTE IN {KNIFE.HIS} BOOT. IN A NEAT HAND. THE TONGUE'S HAND, FROM THE LEDGER. AN ORDER TO KILL ME. AND UNDER IT, CROSSED OUT, ANOTHER LINE: DON'T. PLEASE DON'T."
  opt "THE TONGUE CHANGED ITS MIND." -> %mind
  opt "THE TONGUE WANTS YOU DEAD." -> %dead
stage %mind
  talk hunter
  say "OR CHANGED IT BACK. A PERSON WHO WRITES DON'T AND THEN SENDS THE KNIFE ANYWAY IS A PERSON WHO'S FRIGHTENED OF SOMETHING WORSE THAN ME. I WANT TO KNOW WHAT. AND THEN I STILL MIGHT KILL THEM."
  do add trust 1
  opt "LET'S ASK THEM." -> @next
stage %dead
  talk hunter
  say "THEN THE TONGUE CAN COME AND DO IT. LET'S SEE IF THOSE NEAT HANDS SHAKE. ...NO. YOU'RE RIGHT TO LOOK AT ME LIKE THAT. IF I GO IN ANGRY, I DIE ANGRY. I'LL GO IN COLD."
  opt "COLD, THEN." -> @next
)SAGA";

const char* const kTongueFear = R"SAGA(
stage %summons
  talk mourner
  say "A MESSAGE FOR YOU. A BOY FROM THE ASH CAMP BROUGHT IT, SHAKING. THE TONGUE WANTS TO MEET YOU. ALONE. AT THE CAMP, AT NOON, WHEN EVERYONE'S IN THE FIELDS. IT SAYS: I CAN'T DO THE SUM ANY MORE."
  journal "THE TONGUE OF THE ASH CAMP HAS SENT FOR YOU. ALONE, AT NOON. THE MESSAGE SAYS: I CAN'T DO THE SUM ANY MORE."
  opt "I'LL GO." -> %go
stage %go
  goal goto ashcamp
  then %fear
  do moves tongue ashcamp
  journal "MEET THE TONGUE AT THE ASH CAMP, {ASHCAMP.DIR}, AT NOON. ALONE."
stage %fear
  talk tongue
  say "IT'S GETTING HUNGRIER. EVERY YEAR MORE GRAIN, MORE GOLD, MORE LOTS. MY MOTHER FED IT ONCE A SEASON. I FEED IT EVERY MONTH, AND IT STILL BURNED HALF OF {HOME}. THE SUM DOESN'T WORK. IT NEVER WORKED. I'M SO TIRED."
  opt "THEN STOP FEEDING IT." -> %stop
  opt "THEN HELP ME KILL IT." -> %kill
stage %stop
  talk tongue
  say "STOP? IF I STOP, IT COMES FOR THE CAMP FIRST. FOR THE CHILDREN WHO WERE BORN UP HERE AND NEVER KNEW ANYTHING ELSE. ...UNLESS THERE'S A WAY I DON'T KNOW. MY MOTHER SANG SOMETHING, AT THE CAVE. I WAS TOO YOUNG TO LEARN IT."
  do add trust 1
  opt "WE'LL FIND THE SONG." -> @next
stage %kill
  talk tongue
  say "KILL IT. YOU AND THE HUNTER AND WHAT, A SPEAR? ...I'LL TELL YOU WHEN IT COMES TO THE CAVE. EXACTLY WHEN. THAT'S ALL I CAN GIVE. IF YOU FAIL, IT WILL KNOW WHO TOLD YOU. IT ALWAYS KNOWS."
  do add trust 2
  do remember tongue "I TOLD YOU WHEN IT COMES. IF YOU'RE HEARING THIS AND IT'S STILL ALIVE, RUN."
  opt "THANK YOU, TONGUE." -> @next
)SAGA";

// ---- arc 4: the lair
const char* const kPoison = R"SAGA(
stage %plan
  talk hunter
  say "WOLFSBANE, HEMLOCK, AND THE BLACK MUSHROOM THAT GROWS UNDER THE YEWS. A CARTLOAD. MIXED INTO THE TRIBUTE GRAIN, IT WON'T KILL A {WYRM.KIND}. NOTHING KILLS A {WYRM.KIND}. BUT IT'LL MAKE IT SLOW. SLOW ENOUGH FOR A SPEAR IN THE EYE."
  journal "{HUNTER} MEANS TO POISON THE ASH CAMP'S TRIBUTE GRAIN WITH WOLFSBANE AND HEMLOCK, TO MAKE {WYRM} SLOW."
  opt "AND IF THE CULT FINDS OUT?" -> %cult
  opt "I'LL GATHER THE HERBS." -> %gather
stage %cult
  talk hunter
  say "THEN THE CULT FEEDS IT ONE OF US INSTEAD, PROBABLY ME. THE TONGUE HAS TO KNOW. THE TONGUE HAS TO LET THE POISONED CARTS THROUGH. ...WHICH MEANS TRUSTING THE NEAT HANDS. I HATE IT. DO IT ANYWAY."
  do add trust 1
  opt "I'LL GATHER THE HERBS." -> %gather
stage %gather
  goal wait 1
  then %mixed
  say "A DAY OF CUTTING, GLOVED AND MASKED, IN THE DAMP UNDER THE YEWS. YOUR HANDS GO NUMB. THE SMELL GETS INTO EVERYTHING."
  journal "GATHER WOLFSBANE, HEMLOCK AND THE BLACK MUSHROOM, ENOUGH TO POISON A CARTLOAD OF GRAIN."
stage %mixed
  talk hunter
  do moves hunter lair
  say "IT'S IN THE GRAIN. THE CARTS ARE IN THE CAVE MOUTH. THE TONGUE IS THERE TOO, PALE AS MILK. DUSK IN AN HOUR. ...WHATEVER HAPPENS, I WANT YOU TO KNOW: MY BOY WOULD HAVE LIKED YOU. HE LIKED ANYONE WHO WASN'T AFRAID."
  do set poison 1
  opt "I AM AFRAID." -> @next
  opt "HE'D HAVE LIKED YOU TOO." -> @next
)SAGA";

const char* const kHoard = R"SAGA(
stage %enter
  goal enter lair
  then %bones
  do moves hunter lair
  journal "THE CAVE AT {LAIR}, {LAIR.DIR}, WHERE THE ASH CAMP LEAVES ITS TRIBUTE. GO IN WHILE {WYRM} IS AWAY."
stage %bones
  talk hunter
  say "LOOK. THE GRAIN'S ROTTED, UNEATEN. IT DOESN'T EAT GRAIN. IT NEVER DID. THE GOLD'S HEAPED LIKE A BED. AND THE BONES... THE CULT'S LOTS. THEY THOUGHT THEY WERE FEEDING IT WHEAT. THEY WERE FEEDING IT THEIR OWN."
  opt "THE TONGUE HAS TO SEE THIS." -> %show
  opt "THEN WE END IT HERE." -> %smith
stage %show
  talk tongue
  do moves tongue lair
  say "...MOTHER. THAT'S HER RING. ON THAT HAND. THERE. IT DIDN'T EAT THE GRAIN. A HUNDRED YEARS OF GRAIN, ROTTING IN THE DARK, AND WE STARVED FOR IT, AND IT ONLY EVER WANTED... I'M GOING TO BE SICK."
  do add trust 2
  do remember tongue "I SAW MY MOTHER'S RING IN THE CAVE. I THINK ABOUT IT EVERY TIME ANYONE SAYS THE WORD HONOUR."
  opt "NO MORE LOTS. EVER." -> @next
stage %smith
  talk hunter
  say "WAIT. LOOK HERE, UNDER THE GOLD. HAMMERS, TONGS, A SMITH'S MARKED ANVIL. IT ATE A FORGE ONCE, AND THE SMITH'S SECRETS ROTTED WITH HIM. THESE MARKS ARE THE OLD {HOME} SMITHS' MARKS. SOMEONE IN {HOME} WILL KNOW THEM."
  do add fed 1
  opt "WE'LL TAKE THEM BACK. AFTER." -> @next
)SAGA";

}  // namespace

void addDragonArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::World;
  a.tier = 3;
  a.needs = N_BOSS | N_EVENT | N_CAMP | N_CAVE | N_TOWN;

  a.id = "dr_red_doors"; a.name = "THE DOORS WITH THE RED EYE";
  a.themes = TH_GRIEF | TH_GREED | TH_JUDGEMENT; a.body = kRedDoors; v.push_back(a);
  a.id = "dr_shrine_list"; a.name = "THE LEDGER OF THE RED EYE";
  a.themes = TH_JUDGEMENT | TH_DOOM | TH_GRIEF; a.body = kShrineList; v.push_back(a);
  a.source = Source::Scripture;
  a.id = "dr_sermon"; a.name = "THE SERMON AT SUNDOWN";
  a.themes = TH_FAITH | TH_TEMPTATION | TH_SACRIFICE; a.body = kSermon; v.push_back(a);
  a.source = Source::World;
  a.id = "dr_tribute_cart"; a.name = "THE TRIBUTE CART";
  a.themes = TH_SACRIFICE | TH_MERCY | TH_COURAGE; a.body = kTributeCart; v.push_back(a);
  a.id = "dr_knife_night"; a.name = "THE KNIFE IN THE NIGHT";
  a.themes = TH_BETRAYAL | TH_VENGEANCE | TH_COURAGE; a.body = kKnifeNight; v.push_back(a);
  a.source = Source::Myth;
  a.id = "dr_tongue_fear"; a.name = "THE TONGUE'S FEAR";
  a.themes = TH_REDEMPTION | TH_DOOM | TH_FAITH; a.body = kTongueFear; v.push_back(a);
  a.source = Source::World;
  a.id = "dr_poison"; a.name = "THE POISONED TRIBUTE";
  a.themes = TH_VENGEANCE | TH_TRICKERY | TH_COURAGE; a.body = kPoison; v.push_back(a);
  a.source = Source::Myth;
  a.id = "dr_hoard"; a.name = "THE BONES IN THE HOARD";
  a.themes = TH_GREED | TH_GRIEF | TH_JUDGEMENT; a.body = kHoard; v.push_back(a);
}

CampaignPlan dragonPlan() {
  CampaignPlan p;
  p.id = "dragon";
  p.name = "THE TONGUE OF THE BEAST";
  p.source = Source::Myth;
  p.themes = TH_SACRIFICE | TH_FAITH | TH_VENGEANCE | TH_COURAGE | TH_GRIEF;
  p.needs = N_BOSS | N_EVENT | N_KINGDOM | N_CAMP | N_CAVE | N_TOWN;
  p.head = kHead;
  p.arcs = {ArcSlot{{"dr_red_doors", "dr_shrine_list"}}, ArcSlot{{"dr_sermon", "dr_tribute_cart"}},
            ArcSlot{{"dr_knife_night", "dr_tongue_fear"}}, ArcSlot{{"dr_poison", "dr_hoard"}}};
  p.finale = kFinale;
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
