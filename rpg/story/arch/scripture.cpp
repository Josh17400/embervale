// M6b "Sagas": archetypes from scripture (close retellings allowed). ARCHETYPES lane. The template markup and role
// conventions: rpg/story/saga.h; the twists these slots take: twists.cpp.
//
//   the_pit         THE BROTHER IN THE PIT          (Joseph; phase A's worked example) a lost son, a bloody coat, a well
//   spent_share     THE CHILD WHO SPENT IT ALL      (the prodigal and the elder brother) the share, the swine, the byre
//   sling_stone     THE SHEPHERD AND THE CHAMPION   (David) a raider's champion calls for single combat every dawn
//   whither_thou    THE ONE WHO WOULD NOT LEAVE     (Ruth) a grieving widow and the stranger who will not go home
//   proud_tower     THE TOWER OF PRIDE              (Babel) the lord's tower to see over every kingdom; the masons' tongues
//   mocked_ark      THE BOAT ON THE HILL            (Noah) a farmer building a boat far from water; the rain
//   out_of_bondage  THE ROAD OUT OF BONDAGE         (the exodus) bond-workers at a quarry camp; the long road; the grumbling
//   runaway_seer    THE ONE WHO RAN FROM THE CALL   (Jonah) a novice who fled rather than warn a hated town; the mercy he resents
//   two_mothers     THE DISPUTED CHILD              (Solomon) two women, one baby, no lord to judge
//   shorn_strength  THE STRONGMAN'S SECRET          (Samson) a town's champion, his vow, and the one who sold it
//   the_censer      THE ONE WHO STOOD BETWEEN       (the plague and the intercessor) a sickness read as judgement
//   such_a_time     THE QUEEN WHO MUST SPEAK        (Esther) a consort who hides she was born in the town a chancellor hates
//   other_side      THE STRANGER ON THE ROAD        (the good Samaritan) a beaten traveller two good people passed by
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kThePit = R"SAGA(
title [[THE BROTHER IN THE PIT|THE SON SOLD SOUTH|WHAT THE BROTHERS BURIED]]
hook npc any
pitch "[[pitch=YOU KEEP WATCHING THE ROAD.|WHO ARE YOU WAITING FOR?|YOU HAVE A MOURNER'S LOOK.]]"
hint "[[~pitch|A TRADER SAID A THING LAST WEEK. I HAVEN'T SLEPT SINCE.|EVERY CARAVAN THAT COMES IN, I WATCH THE FACES. FOOLISH, I KNOW.|A TRADER SAID A THING LAST WEEK. I HAVEN'T SLEPT SINCE.]]"
role giver giver
role home site home
role brother resident child of giver
role far site town near
role lost person male at far
var told 0
slot t1 -> homeward rival wonder deceit
slot t2 -> confront wonder mercy price

stage start
  talk giver
  say "[[years=ELEVEN|TWELVE|NINE]] YEARS AGO MY YOUNGEST, {LOST}, TOOK THE FLOCK UP THE HILL AND NEVER CAME DOWN. {BROTHER} BROUGHT BACK HIS COAT, TORN AND BLOODY. A WOLF, THEY SAID. NOW A TRADER SWEARS HE SAW {LOST} IN {FAR}. ALIVE. <<plea>>"
  opt "I'LL GO TO {FAR} AND LOOK." -> road
  opt "[[WHO FOUND THE COAT?|DID ANYONE SEE THE WOLF?]]" -> coat
  opt "THE DEAD DON'T COME BACK." -> refused

stage coat
  talk giver
  say "{BROTHER} AND THE OTHER HERDERS. THEY CAME DOWN WHITE AS MILK, ALL TALKING AT ONCE. I WAS TOO BROKEN TO ASK WHY A WOLF WOULD TAKE A BOY AND LEAVE EVERY SHEEP. <<doubt>>"
  opt "I'LL FIND OUT WHAT HAPPENED." -> road
  opt "LEAVE IT BURIED." -> refused

stage refused
  end fail
  say "<<farewell>> I'LL GO ON WATCHING THE ROAD, THEN. IT'S WHAT I'M GOOD FOR NOW."
  journal "YOU WOULD NOT LOOK FOR {GIVER}'S LOST SON."
  do remember giver "STILL NO WORD OF MY BOY. NOT THAT YOU WOULD CARE."

stage road
  goal goto far
  then found
  say "HE HAD A CROOKED FRONT TOOTH AND A LAUGH YOU COULD HEAR FROM THE WELL. <<blessing>>"
  journal "{GIVER} OF {HOME} BELIEVES {GIVER.HIS} SON {LOST}, LONG LOST, LIVES IN {FAR}. FIND HIM."

stage found
  talk lost
  say "...{HOME}? I HAVE NOT HEARD THAT NAME SPOKEN IN YEARS. NO WOLF TOOK ME. MY OWN {BROTHER.BROTHER} THREW ME DOWN A DRY WELL AND SOLD ME TO SLAVERS FOR TWENTY SILVER. I KEEP A MERCHANT'S HOUSE HERE NOW. I HAVE DONE WELL."
  opt "YOUR {GIVER.FATHER} WATCHES THE ROAD." -> plead
  opt "THEN WRITE TO {GIVER}." -> letter
  opt "THEN STAY. I'LL TELL NO ONE." -> silence

stage plead
  talk lost
  say "{GIVER.HE} STILL WATCHES THE ROAD? ALL THESE YEARS I TOLD MYSELF NOBODY LOOKED. VERY WELL. I WILL COME. BUT I WILL SEE {BROTHER}'S FACE WHEN I WALK IN, AND THEN I WILL DECIDE WHAT I AM."
  do set told 1
  opt "[[WE'LL GO TOGETHER.|THEN LET'S GO HOME.]]" -> @t1

stage homeward
  goal goto home
  then @t2
  do moves lost home
  journal "{LOST} IS GOING HOME TO {HOME} AFTER [[=years]] YEARS. BE THERE WHEN {BROTHER} SEES HIM."

stage confront
  talk lost
  say "THERE {BROTHER.HE} IS. GREY AT THE TEMPLES NOW. {BROTHER.HE} HASN'T SEEN ME YET. TELL ME, {PLAYER}: DO I EMBRACE {BROTHER.HIM}, OR DO I TELL {GIVER} WHAT WAS DONE AT THE WELL?"
  opt "FORGIVE {BROTHER.HIM}." -> forgiven
  opt "YOUR {GIVER.FATHER} DESERVES THE TRUTH." -> truth
  opt "TEST {BROTHER.HIM} FIRST." -> test

stage test
  talk brother
  say "A STRANGER FROM {FAR} IS TO HANG FOR THEFT, AND YOU TELL ME? WHY... WAIT. THAT FACE. NO. NO, TAKE ME IN HIS PLACE. I GAVE ONE BROTHER TO THE DARK ALREADY. I WILL NOT DO IT TWICE."
  opt "HE IS YOUR BROTHER. {LOST}." -> forgiven
  opt "TELL YOUR {GIVER.FATHER} WHY." -> truth

stage forgiven
  end success
  say "THEY HELD EACH OTHER A LONG TIME, AND {GIVER} WEPT LIKE A CHILD. NOBODY SPOKE OF THE WELL. NOBODY NEEDED TO."
  ?tw_road_omen journal "{LOST} CAME HOME, AND {BROTHER} WAS FORGIVEN. THE THREAD THE SEER SPOKE OF HELD."
  !tw_road_omen journal "{LOST} CAME HOME TO {HOME}, AND {BROTHER} WAS FORGIVEN."
  do reward fair
  do befriend brother
  do remember giver "MY SONS EAT AT ONE TABLE AGAIN. SIT, SIT. THERE IS ALWAYS A PLACE FOR YOU."
  do fact "{LOST} CAME HOME TO {HOME} AFTER YEARS AWAY, AND FORGAVE THE ONE WHO SOLD HIM."
  do mark homecoming

stage truth
  end success
  say "{GIVER} HEARD IT ALL WITHOUT A WORD. THEN {GIVER.HE} TOOK {LOST}'S HAND AND DID NOT LOOK AT {BROTHER} AGAIN. JUSTICE IS A COLD THING TO WARM YOUR HANDS AT."
  journal "{LOST} CAME HOME, AND THE TRUTH OF THE WELL CAME WITH HIM. {BROTHER} SLEEPS ELSEWHERE NOW."
  do reward fair
  do remember brother "YOU. YOU COULD HAVE LET IT LIE. ...NO. YOU COULDN'T. I KNOW."
  do fact "THEY SAY {BROTHER} OF {HOME} ONCE SOLD {BROTHER.HIS} OWN BROTHER, AND THE BROTHER CAME BACK."
  do mark homecoming

stage letter
  goal goto home
  then letter_read
  do give "A LETTER FROM {FAR}" letter
  journal "{LOST} WILL NOT COME HOME, BUT {LOST} HAS WRITTEN. CARRY THE LETTER TO {GIVER} IN {HOME}."

stage letter_read
  talk giver
  say "HIS HAND. THAT IS HIS HAND, I TAUGHT HIM HIS LETTERS. ...HE WRITES THAT HE IS WELL. HE WRITES THAT I SHOULD ASK {BROTHER} ABOUT THE WELL. WHAT WELL? WHAT DOES HE MEAN?"
  opt "ASK {BROTHER}. NOT ME." -> letter_end
  opt "HE IS ALIVE. LET THAT BE ENOUGH." -> letter_end

stage letter_end
  end success
  journal "{GIVER} HAS A LETTER IN {LOST}'S HAND, AND A QUESTION FOR {BROTHER} THAT WILL NOT GO AWAY."
  do take "A LETTER FROM {FAR}"
  do reward small
  do remember giver "I READ IT EVERY NIGHT. HE'S ALIVE. WHATEVER ELSE IS TRUE, HE'S ALIVE."

stage silence
  end fail
  journal "YOU KEPT {LOST}'S SECRET. IN {HOME}, {GIVER} STILL WATCHES THE ROAD."
  do remember lost "YOU KEPT YOUR WORD. I THINK OF {HOME} MORE THAN I SHOULD, NOW."
  do mark kept_silence
)SAGA";

// ---------------------------------------------------------------- the prodigal and the elder brother
const char* const kSpentShare = R"SAGA(
title [[THE CHILD WHO SPENT IT ALL|THE SHARE AND THE SWINE|THE CALF IN THE BYRE]]
hook npc farmer
pitch "[[pitch=WHO IS THE CALF FOR?|A FINE CALF. A FEAST COMING?|YOU'RE FATTENING THAT CALF FOR SOMEONE.]]"
hint "[[~pitch|I KEEP THE BEST CALF IN THE BYRE AND I TELL MYSELF I DON'T KNOW WHY.|MY ELDEST SAYS I'M A FOOL ABOUT THAT CALF. MY ELDEST IS USUALLY RIGHT.|MY ELDEST SAYS I'M A FOOL ABOUT THAT CALF. MY ELDEST IS USUALLY RIGHT.]]"
role giver giver
role home site home
role elder resident child of giver
role far site town near
role lost person [[male|female]] at far
var softened 0
slot t1 -> road world price betrayal deceit
slot t2 -> gate wonder rival return mercy

stage start
  talk giver
  say "TWO SPRINGS AGO MY YOUNGEST, {LOST}, ASKED FOR {LOST.HIS} SHARE OF THE FARM WHILE I STILL BREATHED. I SOLD THE [[field=RIVER|LOWER|EAST]] FIELD TO PAY IT. NOW A DROVER SAYS {LOST} IS IN {FAR}, FEEDING ANOTHER MAN'S PIGS. <<plea>>"
  opt "I'LL FIND {LOST.HIM}." -> @t1
  opt "WHY PAY OUT A FOOL'S SHARE?" -> why
  opt "{LOST.HE} CHOSE {LOST.HIS} ROAD." -> refused

stage why
  talk giver
  say "BECAUSE {LOST.HE} ASKED. A CHILD KEPT BY FORCE IS ONLY A HIRED HAND WHO HATES YOU. {ELDER} HAS NOT FORGIVEN ME FOR IT. {ELDER} WORKED EVERY FURROW OF THAT FIELD AND NEVER ASKED ME FOR A THING."
  opt "THEN I'LL FIND {LOST.HIM}." -> @t1
  opt "{ELDER} HAS A POINT." -> refused

stage refused
  end fail
  say "<<farewell>> THE CALF GOES TO MARKET ON SATURDAY, THEN. I SUPPOSE IT WAS ALWAYS GOING TO."
  journal "YOU WOULD NOT LOOK FOR {GIVER}'S YOUNGEST. THE CALF WENT TO MARKET."
  do remember giver "THE CALF FETCHED A GOOD PRICE. I DIDN'T WANT A GOOD PRICE. NEVER MIND."

stage road
  goal goto far
  then found
  say "{LOST.HE} HUMS WHEN {LOST.HE} THINKS NOBODY'S LISTENING. [[ALWAYS THE SAME TUNE, THE ONE ABOUT THE MILLER'S GOOSE.|SHE GOT IT FROM HER MOTHER. HE GOT IT FROM ME.]]"
  journal "{GIVER} OF {HOME} SOLD A FIELD TO PAY {LOST} A SHARE. {LOST} SPENT IT, AND NOW KEEPS PIGS IN {FAR}. FIND {LOST.HIM}."

stage found
  talk lost
  say "{HOME}? DON'T LOOK AT MY HANDS. THE SHARE WENT ON DICE AND FRIENDS, AND THE FRIENDS WENT WITH IT. I EAT WHAT THE PIGS LEAVE. I'VE A SPEECH READY: I'M NOT FIT TO BE CALLED {GIVER}'S CHILD. TAKE ME ON AS A HIRED HAND."
  opt "{GIVER.HE} KEEPS A CALF FOR YOU." -> softened
  opt "THEN COME HOME AND SAY IT." -> homeward
  opt "STAY. YOU'VE EARNED THE PIGS." -> left

stage softened
  talk lost
  say "A CALF. FOR ME. ...DON'T. I CAN'T WEEP IN FRONT OF THE PIGS; THEY'D NEVER RESPECT ME AGAIN. I'LL COME. BUT I'M STILL SAYING THE SPEECH. I PRACTISED IT ON THE SOW."
  do set softened 1
  opt "SAY IT ALL THE WAY HOME." -> homeward

stage left
  end fail
  say "<<grief:shame>> YOU'RE RIGHT. I EARNED THEM. TELL {GIVER}... NO. TELL {GIVER} NOTHING."
  journal "YOU LEFT {LOST} WITH THE PIGS IN {FAR}. {GIVER} STILL KEEPS A CALF IN THE BYRE."
  do remember lost "THE ONE WHO SAID I'D EARNED THE PIGS. YOU WERE RIGHT. IT DOESN'T HELP."
  do mark left_with_the_swine

stage homeward
  goal goto home
  then @t2
  do moves lost home
  journal "{LOST} IS WALKING HOME TO {HOME}, PRACTISING A SPEECH. BE THERE WHEN {GIVER} SEES {LOST.HIM}."

stage gate
  talk giver
  say "I SAW {LOST.HIM} FROM THE TOP FIELD AND RAN. AT MY AGE. {LOST.HE} STARTED THE SPEECH AND I DIDN'T LET {LOST.HIM} FINISH IT. THE CALF'S ON THE SPIT. ...BUT {ELDER} WON'T COME IN. {ELDER} IS SITTING IN THE BYRE IN THE DARK."
  opt "I'LL TALK TO {ELDER}." -> byre
  opt "LET {ELDER} SULK." -> feast_without

stage byre
  talk elder
  say "[[years=TEN|TWELVE|NINE]] YEARS I'VE PLOUGHED FOR {GIVER}, AND NEVER SO MUCH AS A KID GOAT TO FEAST MY FRIENDS. {LOST} SPENDS THE [[=field]] FIELD ON DICE AND COMES HOME TO THE CALF. TELL ME THAT'S JUST."
  opt "IT ISN'T JUST. IT'S MERCY." -> mercy
  opt "EVERYTHING HERE IS YOURS ALREADY." -> yours
  opt "IT ISN'T. ASK FOR YOUR DUE." -> due

stage mercy
  talk elder
  say "MERCY. EVERYONE IS VERY GENEROUS WITH MERCY WHEN IT'S PAID OUT OF MY FIELD. <<doubt>>"
  opt "COME IN AND EAT ANYWAY." -> feast_all
  opt "THEN STAY OUT HERE." -> feast_without

stage yours
  talk elder
  say "...ALL OF IT. {GIVER} SAYS THAT TOO, AND I NEVER LISTEN. THE LAND, THE HOUSE, THE OLD FOOL. ALL MINE ALREADY. ...I'LL COME IN. I WON'T SMILE. DON'T ASK ME TO SMILE."
  opt "NOBODY WILL ASK." -> feast_all

stage due
  talk elder
  say "MY DUE. YES. I'LL ASK FOR THE DEED TO THE NORTH FIELD TONIGHT, AT THE FEAST, IN FRONT OF EVERYONE. LET'S SEE HOW FAR {GIVER}'S MERCY STRETCHES WHEN IT'S ASKED FOR TO {GIVER.HIS} FACE."
  opt "IT'S YOUR RIGHT." -> split_end

stage feast_all
  end success
  say "THE TWO OF THEM SAT AT ONE TABLE. {ELDER} DID NOT SMILE. AROUND MIDNIGHT {ELDER} PASSED {LOST} THE BREAD WITHOUT BEING ASKED, AND {GIVER} HAD TO GO OUTSIDE FOR A WHILE."
  ?m_hope journal "{LOST} CAME HOME AND BOTH OF {GIVER}'S CHILDREN ATE AT THE FEAST. HOPE, IT SEEMS, CAN BE FATTENED LIKE A CALF."
  !m_hope journal "{LOST} CAME HOME TO {HOME}, AND BOTH OF {GIVER}'S CHILDREN ATE AT THE FEAST."
  do reward fair
  do befriend elder
  do remember giver "BOTH OF THEM AT MY TABLE. BOTH! THEY ARGUE ABOUT FENCES NOW. I'VE NEVER HEARD SUCH A LOVELY SOUND."
  do fact "{GIVER} OF {HOME} KILLED THE FATTED CALF WHEN {LOST} CAME HOME, AND THE ELDEST CAME IN TO THE FEAST."
  do mark prodigal_feast

stage feast_without
  end success
  say "THE FEAST WENT ON LATE. THE BYRE STAYED DARK. ONCE, {GIVER} WENT AND STOOD AT ITS DOOR FOR A LONG TIME, AND CAME BACK WITHOUT A WORD."
  journal "{LOST} CAME HOME. {ELDER} DID NOT COME IN TO THE FEAST, AND HAS NOT SAID {LOST}'S NAME SINCE."
  do reward fair
  do remember elder "YOU LET ME SIT IN THE DARK. EVERYONE DID. I'M STILL SITTING IN IT, SOME NIGHTS."
  do fact "WHEN {LOST} CAME HOME TO {HOME}, THERE WAS A FEAST, AND ONE EMPTY PLACE AT THE TABLE."

stage split_end
  end success
  say "{ELDER} ASKED FOR THE DEED IN FRONT OF EVERYONE, AND {GIVER} GAVE IT WITHOUT A PAUSE. THAT WAS THE WORST PART, {ELDER} SAID LATER. NOT EVEN A PAUSE."
  journal "{LOST} IS HOME. {ELDER} HAS THE NORTH FIELD AND A HOUSE OF {ELDER.HIS} OWN, AND VISITS ON FEAST DAYS."
  do reward small
  do remember elder "MY OWN FIELD, MY OWN ROOF. I'M HAPPIER. I THINK. ASK ME AGAIN IN A YEAR."
  do fact "THE FARM OF {GIVER} IN {HOME} IS TWO FARMS NOW, SINCE THE YOUNGEST CAME HOME AND THE ELDEST ASKED FOR A SHARE."
)SAGA";

// ---------------------------------------------------------------- David and the champion
const char* const kSlingStone = R"SAGA(
title [[THE SHEPHERD AND THE CHAMPION|FIVE SMOOTH STONES|THE CHAMPION AT DAWN]]
hook npc guard
pitch "[[pitch=WHAT'S THAT HORN EVERY DAWN?|WHY IS THE GATE BARRED BY DAY?|YOUR WATCH LOOKS ASHAMED.]]"
hint "[[~pitch|HE BLOWS THAT HORN EVERY DAWN, AND EVERY DAWN WE PRETEND WE'RE DEAF.|DON'T ASK ME ABOUT THE CHAMPION. ASK THE MEN WHO SHOULD HAVE ANSWERED HIM.|DON'T ASK ME ABOUT THE CHAMPION. ASK THE MEN WHO SHOULD HAVE ANSWERED HIM.]]"
role giver giver
role home site home
role kingdom kingdom home
role camp site camp near
role foe foe male at camp
role youth person [[male|female]] at home
var armed 0
slot t1 -> stand mercy identity rival prophecy
slot t2 -> after price world return

stage start
  talk giver
  say "A RAIDER CAMPS AT {CAMP}. EVERY DAWN THEIR CHAMPION, {FOE}, A HEAD TALLER THAN ANY DOOR IN {HOME}, BLOWS A HORN AND CALLS FOR ONE OF US TO FIGHT HIM. WIN, AND THEY LEAVE. LOSE, AND WE PAY THEM FOREVER. [[days=FORTY|THIRTY|TWENTY]] DAWNS NOW."
  opt "THEN I'LL ANSWER THE HORN." -> self
  opt "NOBODY HAS ANSWERED?" -> nobody
  opt "PAY THEM. IT'S CHEAPER." -> refused

stage nobody
  talk giver
  say "ONE. A SHEPHERD CHILD, {YOUTH}, WHO BROUGHT {YOUTH.HIS} BROTHERS BREAD AND HEARD THE HORN. SAYS A SLING KILLED A BEAR THAT TOOK [[A LAMB|TWO EWES|THE BELL-WETHER]]. THE CAPTAINS LAUGHED. I DIDN'T. <<doubt>>"
  opt "LET ME SPEAK TO {YOUTH}." -> youth_talk
  opt "I'LL ANSWER IT MYSELF." -> self

stage refused
  end fail
  say "CHEAPER. YES. EVERYBODY SAYS THAT. <<insult>>"
  journal "YOU WOULD NOT ANSWER {FOE}'S HORN. {HOME} STILL PAYS THE RAIDERS AT {CAMP}."
  do remember giver "STILL PAYING. CHEAPER, YOU SAID. IT DOESN'T FEEL CHEAP."

stage youth_talk
  talk youth
  say "THEY ALL SAY HE'S TOO BIG. THAT'S THE POINT. A BIG MAN IS A BIG MARK. I DON'T NEED A SWORD. I NEED FIVE SMOOTH STONES FROM THE STREAM AND SOMEONE TO STOP THE CAPTAINS STOPPING ME. <<vow>>"
  opt "TAKE MY ARMOUR, AT LEAST." -> armour
  opt "THEN I'LL STAND BEHIND YOU." -> @t1
  opt "NO. THIS ISN'T A CHILD'S FIGHT." -> self

stage armour
  talk youth
  say "...I CAN'T MOVE. I CAN'T EVEN LIFT MY ARM. I'M NOT USED TO IT, {PLAYER}. TAKE IT BACK. I'LL GO AS I AM, OR NOT AT ALL."
  do set armed 1
  opt "AS YOU ARE, THEN." -> @t1
  opt "THEN NOT AT ALL. I'LL GO." -> self

stage stand
  goal goto camp
  then duel
  journal "{YOUTH} OF {HOME} WILL ANSWER {FOE}'S HORN AT {CAMP} WITH A SLING. STAND BEHIND {YOUTH.HIM}."
  say "FIVE STONES. I ONLY MEAN TO USE ONE. THE OTHER FOUR ARE FOR NERVES."

stage self
  goal slay foe
  then after
  journal "ANSWER {FOE}'S HORN AT {CAMP}, {CAMP.DIR} OF {HOME}, AND END THE RAIDERS' CLAIM."

stage duel
  talk youth
  say "HE'S LAUGHING. HE ASKED IF I THINK HE'S A DOG, COMING AT HIM WITH STICKS. ...{PLAYER}. IF I MISS, DON'T LET THEM TAKE MY SLING. IT WAS MY {YOUTH.FATHER}'S. <<vow:fear>>"
  opt "YOU WON'T MISS." -> struck
  opt "STEP BACK. I'LL TAKE HIM." -> self

stage struck
  talk youth
  say "IT'S DOWN. IT'S DOWN! ONE STONE, RIGHT ABOVE THE EYE, LIKE A CROW OFF A FENCE. HE FELL LIKE A TREE. THEY'RE RUNNING, LOOK, ALL OF THEM. I... I THINK I'M GOING TO BE SICK."
  do hide foe
  opt "BE SICK. THEN BE PROUD." -> @t2

stage after
  talk giver
  ?t1 say "{FOE} IS DEAD AND THE RAIDERS ARE GONE FROM {CAMP}. THEY'RE ALREADY SINGING ABOUT IT IN {HOME}, THOUGH THEY CAN'T AGREE WHO DID WHAT. WHO GETS THE CREDIT, {PLAYER}?"
  !t1 say "{FOE} IS DEAD AND THE RAIDERS ARE GONE FROM {CAMP}. {HOME} WILL BE SINGING ABOUT IT BY DARK, AND THEY'LL WANT A NAME FOR THE SONG. WHOSE NAME, {PLAYER}?"
  opt "PUT {YOUTH}'S NAME IN THE SONG." -> youth_end
  opt "MINE. I EARNED IT." -> pride_end
  opt "NO NAME. JUST THE STONE." -> stone_end

stage youth_end
  end success
  say "THEY CARRIED {YOUTH} ON THEIR SHOULDERS THROUGH {HOME}, AND {YOUTH} LOOKED BACK AT YOU OVER THE CROWD THE WHOLE WAY, AS IF TO ASK IF IT WAS ALL RIGHT TO BE SO HAPPY."
  journal "{FOE}'S RAIDERS ARE GONE, AND {HOME} SINGS ABOUT A SHEPHERD WITH A SLING. YOU ARE IN THE LAST VERSE."
  do reward fair
  do rep kingdom 3
  do remember youth "THEY WANT ME TO CAPTAIN THE WATCH. ME! I TOLD THEM I'D RATHER HAVE THE SHEEP. THEY THOUGHT I WAS JOKING."
  do fact "A SHEPHERD OF {HOME} NAMED {YOUTH} KILLED THE RAIDER CHAMPION {FOE} WITH ONE STONE FROM A SLING."
  do mark shepherd_champion

stage pride_end
  end success
  say "THE SONG HAS YOUR NAME IN IT. IT RHYMES BADLY. {YOUTH} WENT BACK TO THE SHEEP AND DOES NOT COME DOWN TO THE TOWN ON FEAST DAYS."
  journal "{FOE} IS DEAD AND {HOME} SINGS YOUR NAME. {YOUTH} WENT BACK TO THE HILLS WITHOUT A WORD."
  do reward fair
  do fame 2
  do remember youth "IT'S A GOOD SONG. THE BIT ABOUT YOU IS VERY LONG. NO, I'M NOT BITTER. THE SHEEP DON'T SING."
  do fact "THEY SING IN {HOME} OF A STRANGER WHO ANSWERED {FOE}'S HORN WHEN NO ONE ELSE WOULD."

stage stone_end
  end success
  say "THE STONE SITS ON THE SILL OF THE GATEHOUSE IN {HOME} NOW, AND CHILDREN TOUCH IT FOR LUCK ON THEIR WAY PAST. NOBODY REMEMBERS WHOSE HAND IT LEFT. THAT, SOMEHOW, IS THE POINT."
  journal "{FOE} IS DEAD. THE STONE SITS ON THE GATEHOUSE SILL IN {HOME}, AND THE SONG HAS NO NAME IN IT."
  do reward fair
  do remember giver "THE CHILDREN TOUCH THE STONE EVERY MORNING. I'VE SEEN GROWN SOLDIERS DO IT TOO, WHEN THEY THINK I'M NOT LOOKING."
  do fact "A SLING-STONE SITS ON THE GATEHOUSE SILL IN {HOME}. IT ENDED THE RAIDERS OF {CAMP}, AND NO ONE SAYS WHOSE IT WAS."
  do mark the_stone_on_the_sill
)SAGA";

// ---------------------------------------------------------------- Ruth
const char* const kWhitherThou = R"SAGA(
title [[THE ONE WHO WOULD NOT LEAVE|WHERE YOU GO|THE GLEANER]]
hook npc priest
pitch "[[pitch=WHO IS THE STRANGER AT THE WELL?|WHO BROUGHT THE GLEANINGS IN?|YOU LOOK WORRIED FOR SOMEONE.]]"
hint "[[~pitch|THERE'S A STRANGER GLEANING AT THE EDGES OF OUR FIELDS. NOBODY KNOWS WHAT TO MAKE OF HER.|THERE'S A STRANGER GLEANING AT THE EDGES OF OUR FIELDS. NOBODY KNOWS WHAT TO MAKE OF HER.|GRIEF IS A HOUSE WITH ONE DOOR. SOMEONE HAS GONE IN AFTER A FRIEND, AND WON'T COME OUT.]]"
role giver giver
role home site home
role widow resident mourner
role loyal person female at home
role kinsman person male at home
role kingdom kingdom home
var spoke 0
slot t1 -> field mercy rival deceit world
slot t2 -> gate prophecy return wonder

stage start
  talk giver
  say "{WIDOW} BURIED A SPOUSE AND [[sons=BOTH SONS|TWO SONS|THREE SONS]] IN ONE BAD YEAR. ONE SON'S WIDOW, {LOYAL}, IS A STRANGER HERE, FROM OVER THE BORDER. {WIDOW} BEGGED HER TO GO HOME TO HER OWN PEOPLE. SHE WON'T. NOW THEY'RE BOTH STARVING. <<plea>>"
  opt "WHAT CAN I DO?" -> what
  opt "SEND THE STRANGER HOME." -> send
  opt "A TOWN SHOULD FEED ITS OWN." -> refused

stage refused
  end fail
  say "IT SHOULD. IT DOESN'T. THAT'S WHY I ASKED YOU. <<farewell>>"
  journal "YOU LEFT {WIDOW} AND {LOYAL} TO THE CHARITY OF {HOME}, WHICH IS THIN THIS YEAR."
  do remember giver "{WIDOW} AND THE STRANGER STILL GLEAN THE FIELD EDGES. NOBODY HELPED. I INCLUDE MYSELF."

stage send
  talk loyal
  say "GO HOME? {WIDOW} SAID THE SAME, AND I SAID THIS: WHERE YOU GO I WILL GO, AND WHERE YOU LODGE I WILL LODGE. YOUR PEOPLE ARE MY PEOPLE, AND {GIVER.GOD} IS MY GOD. WHERE YOU DIE, I WILL BE BURIED. DON'T ASK ME AGAIN."
  opt "THEN I'LL HELP YOU STAY." -> what
  opt "LOYALTY WON'T FEED YOU." -> send2

stage send2
  talk loyal
  say "NO. BUT IT WILL GET ME UP BEFORE DAWN TO GLEAN, WHICH IS MORE THAN HUNGER EVER DID. <<refusal>>"
  opt "THEN LET ME HELP." -> what
  opt "AS YOU WISH. I'LL GO." -> walk_away

stage walk_away
  end fail
  journal "{LOYAL} WOULD NOT LEAVE {WIDOW}, AND YOU WOULD NOT HELP THEM STAY. THEY GLEAN THE FIELD EDGES OF {HOME} STILL."
  do remember loyal "STILL HERE. STILL GLEANING. I TOLD YOU NOT TO ASK ME AGAIN, AND YOU DIDN'T. THANK YOU FOR THAT, AT LEAST."
  do mark left_the_gleaners

stage what
  talk giver
  say "{KINSMAN} OWNS THE BEST BARLEY IN {HOME} AND IS KIN TO {WIDOW}'S DEAD. BY OLD CUSTOM HE MAY REDEEM THEIR LAND AND TAKE THEM UNDER HIS ROOF. HE IS A GOOD MAN, AND A SHY ONE, AND HE HAS NOT BEEN ASKED."
  opt "THEN I'LL ASK HIM." -> @t1
  opt "I'LL GLEAN WITH {LOYAL} FIRST." -> glean

stage glean
  goal wait 1
  then @t1
  journal "YOU GLEANED THE EDGES OF {KINSMAN}'S BARLEY WITH {LOYAL} FROM DAWN TO DUSK. THE REAPERS LEFT MORE THAN THEY HAD TO. SOMEONE TOLD THEM TO."
  do set spoke 1

stage field
  talk kinsman
  ?m_devotion say "[[THE STRANGER WHO GLEANS MY EDGES?|{WIDOW}'S SON'S WIDOW?]] I TOLD MY REAPERS TO DROP HANDFULS ON PURPOSE. I HEARD WHAT SHE DID, STAYING. I'VE NEVER SEEN DEVOTION LIKE IT. I'M... NOT GOOD AT ASKING THINGS."
  !m_devotion say "[[THE STRANGER WHO GLEANS MY EDGES?|{WIDOW}'S SON'S WIDOW?]] I TOLD MY REAPERS TO DROP HANDFULS ON PURPOSE. I HEARD WHAT SHE DID, STAYING WHEN SHE COULD HAVE GONE. I'M... NOT GOOD AT ASKING THINGS."
  opt "REDEEM THEIR LAND. TAKE THEM IN." -> redeem
  opt "WHY HAVEN'T YOU ALREADY?" -> nearer
  opt "I'LL PAY THEIR DEBTS MYSELF." check gold 40 -> paid else nearer

stage nearer
  talk kinsman
  say "THERE IS A NEARER KINSMAN THAN ME, AND HE HAS THE FIRST RIGHT. HE WANTS THE LAND. HE DOESN'T WANT TWO WIDOWS AND A STRANGER WITH IT. I'LL ASK HIM AT THE GATE, IN FRONT OF THE ELDERS. IF HE REFUSES, IT'S MINE TO DO."
  opt "AT THE GATE, THEN." -> @t2

stage gate
  talk kinsman
  say "HE TOOK OFF HIS SANDAL AND HANDED IT TO ME, IN FRONT OF THE ELDERS. THAT'S THE OLD WAY OF SAYING: YOU HAVE IT. I HAVE IT. THE LAND, THE NAME, ALL OF IT. ...I'M TERRIFIED, {PLAYER}. TELL ME I'M DOING RIGHT."
  opt "YOU'RE DOING RIGHT." -> redeem
  opt "ASK {LOYAL} WHAT SHE WANTS." -> ask_her

stage ask_her
  talk loyal
  say "WHAT I WANT? NOBODY HAS ASKED ME THAT SINCE I CROSSED THE BORDER. ...I WANT {WIDOW} FED. I WANT A ROOF. AND {KINSMAN} LEFT THE BARLEY FOR ME WHEN HE THOUGHT NOBODY SAW. YES. TELL HIM YES."
  opt "I'LL TELL HIM." -> redeem

stage redeem
  end success
  say "THE ELDERS AT THE GATE BLESSED THE HOUSE. {WIDOW} LAUGHED FOR THE FIRST TIME IN A YEAR, AND SAID SHE HAD GONE AWAY FULL AND COME BACK EMPTY, AND NOW SHE WAS FULL AGAIN."
  journal "{KINSMAN} REDEEMED THE LAND OF {WIDOW}'S DEAD AND TOOK {LOYAL} AND {WIDOW} UNDER HIS ROOF IN {HOME}."
  do reward fair
  do befriend widow
  do remember loyal "MY CHILD WILL BE BORN IN {HOME}, AMONG MY PEOPLE. MY PEOPLE. I SAID IT AND NOW IT'S TRUE."
  do fact "{LOYAL}, A STRANGER FROM OVER THE BORDER, WOULD NOT LEAVE {WIDOW} IN HER GRIEF. NOW THEY ARE BOTH OF {KINSMAN}'S HOUSE."
  do mark the_loyal_stranger

stage paid
  end success
  do gold -40
  say "YOU PAID THEIR DEBTS, AND THEY KEPT THEIR LAND, AND THEIR OWN ROOF. {KINSMAN} STILL LEAVES HANDFULS OF BARLEY ON PURPOSE, AND STILL HAS NOT ASKED."
  journal "YOU PAID THE DEBTS OF {WIDOW}'S HOUSE. {LOYAL} AND {WIDOW} KEEP THEIR OWN ROOF, TOGETHER, IN {HOME}."
  do reward small
  do befriend widow
  do remember loyal "OUR OWN ROOF. I PATCHED IT MYSELF. {KINSMAN} BRINGS BARLEY AND STANDS IN THE DOOR LIKE A HERON. ONE DAY HE'LL ASK."
  do fact "A STRANGER PAID THE DEBTS OF {WIDOW}'S HOUSE IN {HOME}, SO TWO WIDOWS COULD KEEP THEIR OWN ROOF."
)SAGA";

// ---------------------------------------------------------------- Babel
const char* const kProudTower = R"SAGA(
title [[THE TOWER OF PRIDE|THE TOWER TO SEE ALL KINGDOMS|THE MASONS' TONGUES]]
hook npc smith
pitch "[[pitch=WHO ARE ALL THOSE CHISELS FOR?|YOU'RE FORGING A LOT OF CHISELS.|WHERE ARE THE MASONS GOING?]]"
hint "[[~pitch|I HAVE MADE MORE CHISELS THIS YEAR THAN IN TEN BEFORE IT. FOR A TOWER. ONE TOWER.|THE MASONS COME FOR TOOLS AND GO BACK GREY. THEY DON'T TALK ABOUT IT.|THE MASONS COME FOR TOOLS AND GO BACK GREY. THEY DON'T TALK ABOUT IT.]]"
role giver giver
role home site home
role kingdom kingdom home
role capital capital kingdom
role lord ruler kingdom
role master person [[male|female]] at capital
var cracks 0
slot t1 -> road betrayal price rival deceit
slot t2 -> audience prophecy wonder world

stage start
  talk giver
  say "{KINGDOM.LORD} IS BUILDING A TOWER AT {CAPITAL} TO SEE OVER EVERY KINGDOM THERE IS. [[floors=FORTY|SIXTY|FIFTY]] FLOORS, THEY SAY, AND ADDING. MY NEPHEW CUTS STONE THERE. HIS LAST LETTER SAID THE FOUNDATIONS SING AT NIGHT. STONE SHOULDN'T SING."
  opt "I'LL GO AND LOOK AT IT." -> @t1
  opt "WHAT DOES THE LORD WANT TO SEE?" -> see
  opt "TOWERS AREN'T MY TRADE." -> refused

stage see
  talk giver
  say "EVERYTHING. EVERY ARMY BEFORE IT MARCHES, EVERY HARVEST BEFORE IT'S IN. A LORD WHO SEES EVERYTHING NEEDS NOBODY. <<proverb>>"
  opt "I'LL GO TO {CAPITAL}." -> @t1
  opt "LET THE LORD HAVE HIS TOWER." -> refused

stage refused
  end fail
  say "NO. NOT MINE EITHER. I JUST MAKE THE CHISELS. <<farewell>>"
  journal "YOU LEFT {KINGDOM.LORD}'S TOWER TO ITS BUILDERS."
  do remember giver "STILL MAKING CHISELS. MY NEPHEW STOPPED WRITING. I'M SURE IT'S NOTHING."

stage road
  goal goto capital
  then scaffold
  journal "{KINGDOM.LORD}'S TOWER AT {CAPITAL} RISES FLOOR ON FLOOR. A MASON'S LETTER SAYS ITS FOUNDATIONS SING. GO AND SEE."

stage scaffold
  talk master
  say "YOU'RE NOT A MASON. GOOD. THE MASONS DON'T SPEAK TO EACH OTHER NOW. THE NORTH GANG AND THE RIVER GANG EACH SAY THE OTHER CUT THE STONE SHORT. NOBODY USES THE SAME WORD FOR A CUBIT ANY MORE. AND THE CRACKS RUN UP THROUGH [[=floors]] FLOORS."
  opt "TELL THE LORD TO STOP." -> plead
  opt "SHOW ME THE CRACKS." -> cracks
  opt "FINISH IT. FAST." -> finish

stage cracks
  talk master
  say "HERE. YOU CAN PUT YOUR HAND IN THIS ONE. LAST WEEK IT WAS A HAIR. LISTEN: THAT HUM IS THE STONE ARGUING WITH ITSELF. THE LORD SAYS ADD TEN FLOORS AND THE WEIGHT WILL SETTLE IT. <<warning>>"
  do set cracks 1
  opt "THEN I'LL TALK TO THE LORD." -> plead
  opt "THEN BRING THE MASONS DOWN." -> down

stage plead
  goal goto capital
  then @t2
  journal "TELL {LORD} AT {CAPITAL} THAT THE TOWER IS CRACKING, AND WHY THE MASONS NO LONGER UNDERSTAND EACH OTHER."

stage audience
  talk lord
  say "YOU WANT ME TO STOP MY TOWER? FROM THE TOP OF IT I WILL SEE EVERY ARMY BEFORE IT MARCHES. NO ONE WILL EVER SURPRISE {KINGDOM} AGAIN. GIVE ME ONE REASON THAT ISN'T FEAR."
  opt "A TOWER CAN'T SEE INTO HEARTS." check level 4 -> listened else angered
  opt "IT WILL FALL, AND KILL HUNDREDS." -> angered
  opt "THEN BUILD IT. I WAS WRONG." -> finish

stage listened
  talk lord
  say "...NO. IT CAN'T. MY FATHER HAD A HUNDRED SPIES AND HIS OWN STEWARD POISONED HIM. <<doubt>> VERY WELL. THE TOWER STOPS AT THE FLOOR WHERE IT STANDS. AND THE MASONS GO HOME."
  opt "LET THEM GO HOME." -> scattered_end

stage angered
  talk lord
  say "FALL? MY TOWER? GET OUT OF MY SIGHT. ...GUARDS. SEE THE TRAVELLER TO THE GATE. GENTLY. FOR NOW. <<threat>>"
  opt "BRING THE MASONS DOWN MYSELF." -> down
  opt "THEN I'VE DONE WHAT I COULD." -> fell_end

stage down
  talk master
  say "TONIGHT? WITHOUT LEAVE? THEY'LL HANG ME. ...THEY'LL HANG ME ANYWAY WHEN IT COMES DOWN. ALL RIGHT. I'LL TELL THE NORTH GANG IN THEIR WORDS AND THE RIVER GANG IN THEIRS, AND THEY CAN HATE EACH OTHER ON THE ROAD HOME."
  opt "GO. ALL OF YOU." -> empty_end

stage finish
  talk master
  say "FAST. YES. THAT'S WHAT EVERYONE SAYS. FAST. <<doubt>> I'LL SEE YOU AT THE TOP, IF THERE IS ONE."
  opt "AT THE TOP." -> fell_end

stage scattered_end
  end success
  say "THE MASONS WENT HOME TO THEIR OWN VALLEYS, EACH GANG WITH ITS OWN WORD FOR A CUBIT, AND THE HALF-TOWER STANDS OVER {CAPITAL}. NOBODY CLIMBS IT. CROWS NEST IN ITS WINDOWS."
  journal "{KINGDOM.LORD} STOPPED THE TOWER AT {CAPITAL}. IT STANDS UNFINISHED, AND THE MASONS ARE HOME."
  do reward rich
  do rep kingdom 3
  do remember giver "MY NEPHEW'S HOME. HE TALKS FUNNY NOW, ALL RIVER-GANG WORDS. I DON'T CARE. HE'S HOME."
  do fact "THE TOWER OF {CAPITAL} WAS LEFT UNFINISHED WHEN {KINGDOM.LORD} WAS TOLD A TOWER CANNOT SEE INTO HEARTS."
  do mark tower_stopped

stage empty_end
  end success
  say "BY DAWN THE SCAFFOLDS WERE EMPTY. BY NOON THE TOWER GROANED AND LEANED. BY DUSK IT WAS A HILL OF RUBBLE, AND NOT ONE MASON WAS UNDER IT. THE LORD HAS NOT FORGIVEN YOU, AND NEVER WILL."
  journal "THE TOWER AT {CAPITAL} FELL, BUT ITS MASONS WERE ALREADY ON THE ROAD HOME. {KINGDOM.LORD} HAS NOT FORGIVEN YOU."
  do reward fair
  do rep kingdom -5
  do remember giver "THEY SAY IT FELL LIKE A DRUNK MAN SITTING DOWN. AND MY NEPHEW WASN'T UNDER IT. BLESS YOU. DON'T GO TO {CAPITAL} FOR A WHILE."
  do fact "THE GREAT TOWER OF {CAPITAL} FELL IN A DAY, BUT THE MASONS HAD GONE HOME THE NIGHT BEFORE."

stage fell_end
  end fail
  say "THEY ADDED TEN FLOORS IN A MONTH. ON THE ELEVENTH, THE SINGING STOPPED, AND THEN THE TOWER CAME DOWN INTO THE MASONS' CAMP. THEY ARE STILL DIGGING."
  journal "{KINGDOM.LORD}'S TOWER FELL ON ITS OWN BUILDERS AT {CAPITAL}."
  do remember giver "THEY'RE STILL DIGGING AT {CAPITAL}. I'VE STOPPED MAKING CHISELS. I MAKE SPADES NOW."
  do fact "THE GREAT TOWER OF {CAPITAL} FELL ON ITS MASONS. THE STONE OF IT LIES THERE STILL, AND NOBODY WILL BUILD WITH IT."
)SAGA";

// ---------------------------------------------------------------- Noah
const char* const kMockedArk = R"SAGA(
title [[THE BOAT ON THE HILL|THE FOOL'S ARK|FORTY DAYS]]
hook npc farmer
pitch "[[pitch=WHY IS THERE A BOAT ON YOUR HILL?|IS THAT A BOAT ON THE HILL?|YOU'RE BUILDING A WHAT?]]"
hint "[[~pitch|GO ON, LAUGH. EVERYONE ELSE DOES. THEN COME BACK WHEN YOU'VE STOPPED.|THEY THROW STONES AT THE HULL AT NIGHT. THE HULL DOESN'T MIND. I DO.|GO ON, LAUGH. EVERYONE ELSE DOES. THEN COME BACK WHEN YOU'VE STOPPED.]]"
role giver giver
role home site home
role cave site cave near
role mocker resident any
role kin resident kin of giver
var helped 0
slot t1 -> pitch_back price world betrayal rival
slot t2 -> rain wonder prophecy mercy

stage start
  talk giver
  say "I DREAMED THE SAME DREAM [[nights=SEVEN|NINE|FORTY]] NIGHTS: THE RIVER OVER THE MILL, THE MILL OVER THE TREES. SO I'M BUILDING A BOAT ON THE HIGHEST HILL IN {HOME}. I'M SHORT OF PITCH. THERE'S PITCH IN {CAVE}, AND NOBODY WILL FETCH IT FOR A MADMAN."
  opt "I'LL FETCH YOUR PITCH." -> fetch
  opt "HOW SURE ARE YOU?" -> sure
  opt "IT HASN'T RAINED IN WEEKS." -> mock

stage sure
  talk giver
  say "SURE? I'M NOT SURE OF ANYTHING. I'M SURE OF THE DREAM. I'D RATHER BE A FOOL WITH A BOAT THAN A WISE MAN UNDER WATER. <<proverb>>"
  opt "THEN I'LL FETCH THE PITCH." -> fetch
  opt "YOU'RE A FOOL WITH A BOAT." -> mock

stage mock
  talk mocker
  say "HA! YOU'VE MET OUR SHIPWRIGHT, THEN. THE CHILDREN SAIL TWIGS IN THE HORSE-TROUGH AND CALL THEM BY HIS NAME. HE'S HARMLESS. MAD AS A HARE, BUT HARMLESS. <<proverb>>"
  opt "HE MIGHT BE RIGHT." -> fetch
  opt "MAD AS A HARE." -> mocked_end

stage mocked_end
  end fail
  journal "YOU LAUGHED WITH {MOCKER} AT {GIVER}'S BOAT ON THE HILL. IT HASN'T RAINED. YET."
  do remember giver "YOU LAUGHED TOO. EVERYONE LAUGHS. I'M STILL SHORT OF PITCH."
  do remember mocker "THE BOAT ON THE HILL! WE LAUGHED, DIDN'T WE? ...IT'S BEEN CLOUDY ALL WEEK. FUNNY, THAT."
  do mark laughed_at_the_ark

stage fetch
  goal fetch "A POT OF BLACK PITCH" in cave
  then @t1
  journal "{GIVER} IS BUILDING A BOAT ON A HILL IN {HOME} AND NEEDS PITCH. THERE IS PITCH TO BE HAD IN {CAVE}, {CAVE.DIR}."

stage pitch_back
  talk giver
  say "PITCH! ENOUGH FOR THE WHOLE KEEL. NOW I NEED HANDS, AND {KIN} WON'T EVEN COME UP THE HILL. SAYS I SHAMED THE FAMILY. <<grief>>"
  do set helped 1
  opt "I'LL TALK TO {KIN}." -> kin_talk
  opt "I'LL TAR THE HULL MYSELF." -> tar

stage kin_talk
  talk kin
  say "UP THE HILL? AND BE LAUGHED AT LIKE {GIVER}? THE BAKER WON'T SELL TO US. THE PRIEST PRAYS FOR {GIVER.HIM} BY NAME, OUT LOUD. DO YOU KNOW WHAT THAT'S LIKE?"
  opt "I KNOW WHAT DROWNING'S LIKE." -> kin_comes
  opt "THEN STAY DOWN HERE." -> tar

stage kin_comes
  talk kin
  say "...YOU REALLY THINK IT'LL RAIN. YOU, A STRANGER. ALL RIGHT. I'LL CARRY PLANKS. I WON'T SING THE HAULING SONG, THOUGH. {GIVER} ALWAYS SINGS IT FLAT."
  do befriend kin
  opt "THEN SING IT SHARP." -> tar

stage tar
  goal wait 2
  then @t2
  journal "YOU TARRED THE HULL OF {GIVER}'S BOAT FROM KEEL TO RAIL. ON THE SECOND EVENING THE SKY OVER {HOME} WENT THE COLOUR OF A BRUISE."

stage rain
  talk giver
  say "<<omen>> IT'S COMING. THE RIVER'S OVER THE FORD ALREADY. THE LOWER FARMS, THE MILL, THE CHILDREN BY THE WATER. {PLAYER}, THE BOAT HOLDS THIRTY. WHO DO WE BRING UP THE HILL?"
  opt "EVERYONE WHO'LL COME." -> all_end
  opt "THE ONES WHO MOCKED YOU TOO." -> all_end
  opt "YOUR OWN FIRST." -> own_end

stage all_end
  end success
  say "IT RAINED FOR [[=nights]] DAYS. THE RIVER TOOK THE MILL AND THE LOWER FARMS, AND STOPPED THREE FEET BELOW THE KEEL. THIRTY PEOPLE AND A GOAT CAME DOWN THE HILL AFTER, AND NOBODY LAUGHS ABOUT THE BOAT NOW."
  journal "THE FLOOD CAME TO {HOME}. {GIVER}'S BOAT ON THE HILL HELD THIRTY, AND {MOCKER} WAS ONE OF THEM."
  do reward fair
  do remember mocker "I WAS IN THE BOAT. ME. I LAUGHED LOUDEST AND HE PULLED ME IN BY THE COLLAR. I'VE NOT LAUGHED AT ANYONE SINCE."
  do remember giver "THE BOAT'S STILL UP THERE. THE CHILDREN PLAY IN IT. I LET THEM. EVERY BOAT SHOULD HAVE CHILDREN IN IT."
  do fact "WHEN THE RIVER ROSE OVER THE MILL OF {HOME}, {GIVER}'S BOAT ON THE HILL CARRIED THIRTY SOULS ABOVE THE WATER."
  do mark ark_saved_thirty

stage own_end
  end success
  say "IT RAINED FOR [[=nights]] DAYS. THE BOAT HELD {GIVER}'S HOUSEHOLD, DRY AND SAFE. BELOW, ON THE ROOFS, PEOPLE SHOUTED FOR A WHILE, AND THEN THEY STOPPED. THE BOAT HAD ROOM. NOBODY SAYS SO OUT LOUD."
  journal "THE FLOOD CAME TO {HOME}. {GIVER}'S HOUSEHOLD RODE IT OUT IN THE BOAT. MANY OTHERS DID NOT."
  do reward small
  do remember giver "WE HAD ROOM. I HEAR THEM AT NIGHT. I DREAMED THE FLOOD, AND I DIDN'T DREAM THAT, AND IT'S THAT I SEE."
  do fact "{GIVER} OF {HOME} DREAMED THE FLOOD AND BUILT A BOAT, AND SAVED HIS OWN, AND THE BOAT HAD ROOM."
)SAGA";

// ---------------------------------------------------------------- the exodus
const char* const kOutOfBondage = R"SAGA(
title [[THE ROAD OUT OF BONDAGE|LET THEM GO|THE QUARRY CAMP]]
hook npc any
pitch "[[pitch=WHO DO YOU KEEP LOOKING FOR?|IS SOMEONE OF YOURS AWAY?|YOU KEEP COUNTING ON YOUR FINGERS.]]"
hint "[[~pitch|THEY SAID IT WAS A YEAR'S WORK FOR A YEAR'S DEBT. THAT WAS FOUR YEARS AGO.|THEY SAID IT WAS A YEAR'S WORK FOR A YEAR'S DEBT. THAT WAS FOUR YEARS AGO.|TWENTY-THREE. I SAY IT EVERY MORNING. TWENTY-THREE. SO I DON'T FORGET A SINGLE ONE.]]"
role giver giver
role home site home
role camp site camp near
role foe foe male at camp
role leader person [[male|female]] at camp
var bond 0
slot t1 -> wilderness betrayal price world
slot t2 -> grumbling world wonder return

stage start
  talk giver
  say "[[count=TWENTY-THREE|NINETEEN|THIRTY-ONE]] OF OUR PEOPLE WENT TO WORK OFF A DEBT IN THE QUARRY AT {CAMP}. A YEAR, THEY WERE TOLD. IT'S BEEN FOUR. NOW {FOE} SAYS THE DEBT GROWS FASTER THAN THEY CAN CUT STONE. THEY'LL DIE THERE. <<plea>>"
  opt "THEN I'LL BRING THEM OUT." -> go
  opt "IS THE DEBT HONEST?" -> debt
  opt "A DEBT IS A DEBT." -> refused

stage debt
  talk giver
  say "THEY CHARGE FOR THE BREAD, THE BLANKETS, THE PICKS THEY BREAK, AND THE STONE THEY BREAK THEM ON. MY [[SISTER|BROTHER|DAUGHTER]] OWES MORE NOW THAN THE DAY SHE WENT IN. <<curse>>"
  opt "I'LL BRING THEM OUT." -> go
  opt "THE LAW IS ON {FOE}'S SIDE." -> refused

stage refused
  end fail
  say "THE LAW. YES. THE LAW IS ALWAYS ON SOMEBODY'S SIDE. <<farewell>>"
  journal "YOU WOULD NOT GO TO THE QUARRY AT {CAMP}. [[=count]] OF {GIVER}'S PEOPLE ARE STILL CUTTING STONE."
  do remember giver "[[=count]]. I STILL COUNT THEM EVERY MORNING. IT'S [[=count]] TODAY. ASK ME AGAIN NEXT YEAR."

stage go
  goal goto camp
  then quarry
  journal "[[=count]] OF {HOME}'S PEOPLE ARE HELD BY A DEBT THAT ONLY GROWS AT THE QUARRY OF {CAMP}. BRING THEM OUT."

stage quarry
  talk leader
  say "YOU'RE FROM {HOME}? THEY SENT SOMEONE? ...KEEP YOUR VOICE DOWN. {FOE} HAS THE LEDGER AND THE DOGS. SOME OF US WANT TO RUN TONIGHT. SOME ARE TOO AFRAID. WHAT DO WE DO?"
  opt "I'LL FACE {FOE}. YOU GET READY." -> face
  opt "I'LL BUY YOUR BOND. ALL OF IT." check gold 40 -> bought else face
  opt "RUN TONIGHT. I'LL HOLD THE DOGS." -> run

stage face
  goal slay foe
  then freed
  journal "{FOE} HOLDS THE LEDGER OF THE QUARRY AT {CAMP}. END HIS HOLD ON THE PEOPLE OF {HOME}."

stage bought
  talk leader
  do gold -40
  do set bond 1
  say "HE TOOK IT. HE TORE OUT THE PAGE AND HANDED IT TO ME, AND LAUGHED, AND SAID WE'D BE BACK. MAYBE WE WILL. NOT TODAY."
  opt "NOT TODAY. WALK." -> @t1

stage run
  goal kill 4 bandit in camp
  then freed
  journal "THE PEOPLE OF {HOME} ARE RUNNING FROM {CAMP} BY NIGHT. HOLD THE QUARRY GUARDS WHILE THEY GO."

stage freed
  talk leader
  say "THE LEDGER'S ASH. THE GATE'S OPEN. LOOK AT THEM, {PLAYER}, STANDING AT THE GATE LIKE IT MIGHT BITE. THEY'VE FORGOTTEN HOW TO WALK THROUGH A DOOR WITHOUT BEING TOLD."
  opt "WALK THROUGH IT. ALL OF YOU." -> @t1

stage wilderness
  goal goto home
  then @t2
  do moves leader home
  journal "LEAD THE FREED QUARRY-WORKERS HOME TO {HOME} ACROSS THE WILD COUNTRY. THERE IS LITTLE FOOD, AND LESS PATIENCE."

stage grumbling
  talk leader
  say "<<doubt>> HALF OF THEM WANT TO TURN BACK. THEY SAY AT LEAST AT THE QUARRY THERE WAS BREAD AND MEAT, SALTED. THEY'RE HUNGRY AND THEY'RE FRIGHTENED OF BEING FREE. I DON'T KNOW WHAT TO TELL THEM."
  opt "TELL THEM WHAT THE BREAD COST." -> onward
  opt "LET THE ONES WHO WANT GO BACK." -> split
  opt "SHARE OUT ALL I HAVE." -> share

stage onward
  talk leader
  say "...THEY'RE QUIET NOW. ONE OF THE OLD ONES STARTED A SONG THEY SANG BEFORE THE QUARRY. THE YOUNG ONES DIDN'T KNOW THE WORDS. THEY'RE LEARNING."
  opt "SING IT ALL THE WAY HOME." -> home_end

stage share
  talk leader
  say "IT WAS NOT ENOUGH FOR ALL OF US. IT WAS ENOUGH FOR TONIGHT. THAT'S WHAT THEY'LL REMEMBER. NOT THE QUARRY. TONIGHT. <<thanks>>"
  do gold -10
  opt "TONIGHT, THEN. TOMORROW, HOME." -> home_end

stage split
  talk leader
  say "[[back=FIVE|SIX|FOUR]] OF THEM WENT BACK. I WATCHED THEM OUT OF SIGHT. {PLAYER}... THE ONE IN THE BLUE SHAWL WAS MY COUSIN. <<grief>>"
  opt "THEY CHOSE." -> split_end

stage home_end
  end success
  say "THEY CAME INTO {HOME} AT DUSK, [[=count]] OF THEM, THIN AS RAKES, SINGING. {GIVER} COUNTED THEM AT THE GATE OUT LOUD, ONE BY ONE, AND GOT EVERY ONE RIGHT."
  journal "YOU BROUGHT [[=count]] PEOPLE OUT OF THE QUARRY AT {CAMP} AND HOME TO {HOME}. NOT ONE WAS LEFT BEHIND."
  do reward rich
  do remember giver "[[=count]]. I SAID IT AT THE GATE AND THEN I DIDN'T NEED TO SAY IT ANY MORE. I'VE FORGOTTEN HOW TO COUNT. I DON'T MISS IT."
  do remember leader "WE KEEP A FEAST FOR THE NIGHT WE CAME OUT. WE EAT STANDING UP, WITH OUR SHOES ON. THE CHILDREN ASK WHY. WE TELL THEM."
  do fact "THE PEOPLE OF {HOME} HELD IN DEBT AT THE QUARRY OF {CAMP} CAME HOME, AND KEEP A FEAST FOR THE NIGHT THEY WALKED OUT."
  do mark led_out_of_bondage

stage split_end
  end success
  say "THEY CAME INTO {HOME} AT DUSK, FEWER THAN WENT OUT. {GIVER} COUNTED THEM AT THE GATE, AND STOPPED SHORT, AND COUNTED AGAIN, AND THEN JUST HELD THE ONES WHO WERE THERE."
  journal "MOST OF {HOME}'S PEOPLE CAME HOME FROM {CAMP}. [[=back]] TURNED BACK TO THE QUARRY ON THE ROAD, AND ARE THERE STILL."
  do reward fair
  do remember giver "I COUNT AGAIN NOW. A SMALLER NUMBER. IT'S STILL A NUMBER."
  do fact "SOME OF {HOME}'S PEOPLE WALKED HOME FROM THE QUARRY OF {CAMP}. SOME TURNED BACK, AFRAID OF THE OPEN ROAD."
)SAGA";

// ---------------------------------------------------------------- Jonah
const char* const kRunawaySeer = R"SAGA(
title [[THE ONE WHO RAN FROM THE CALL|THE RELUCTANT PROPHET|THE GOURD AND THE TOWN]]
hook npc priest
pitch "[[pitch=YOUR NOVICE ISN'T AT PRAYERS?|WHERE'S THE YOUNG ONE TODAY?|YOU'VE AN EMPTY STOOL THERE.]]"
hint "[[~pitch|THE STOOL BY THE ALTAR IS EMPTY. IT WASN'T, A WEEK AGO.|THE STOOL BY THE ALTAR IS EMPTY. IT WASN'T, A WEEK AGO.|I HAD A NOVICE WHO HEARD THE GOD SPEAK. NOW I HAVE A NOVICE WHO RAN AWAY.]]"
role giver giver
role home site home
role cave site cave near
role far site town near
role seer person [[male|female]] at cave
role elder person [[male|female]] at far
var dragged 0
slot t1 -> to_far identity mercy price betrayal
slot t2 -> sulk wonder prophecy world

stage start
  talk giver
  say "MY NOVICE {SEER} WOKE ME AT MIDNIGHT. {GIVER.GOD} HAD SAID: GO TO {FAR}, TELL THEM THEIR CRUELTY HAS COME UP BEFORE HEAVEN. {SEER} HATES {FAR}, AS WE ALL DO, AND BY DAWN HAD RUN THE OTHER WAY, TO {CAVE}. <<plea>>"
  opt "I'LL BRING {SEER.HIM} BACK." -> seek
  opt "WHY DO YOU HATE {FAR}?" -> why
  opt "LET {SEER.HIM} RUN." -> refused

stage why
  talk giver
  say "[[forty=FORTY|SIXTY|THIRTY]] YEARS AGO THEIR RAIDERS BURNED OUR GRANARY, AND WE STILL TELL IT. {SEER} WOULD RATHER SEE {FAR} BURN THAN WARN IT. I UNDERSTAND THAT. I DON'T THINK {GIVER.GOD} DOES."
  opt "THEN I'LL FIND {SEER.HIM}." -> seek
  opt "PERHAPS THEY DESERVE IT." -> refused

stage refused
  end fail
  say "PERHAPS. THAT'S NOT FOR US TO SAY. <<saying>>"
  journal "YOU LET {SEER} HIDE FROM THE CALL. NO ONE WARNED {FAR}."
  do remember giver "{SEER} CAME BACK IN THE END, HALF-STARVED, AND WON'T SPEAK OF IT. NOR WILL I."

stage seek
  goal enter cave
  then wolves
  journal "{GIVER}'S NOVICE {SEER} RAN FROM A CALL TO WARN {FAR} AND IS HIDING IN {CAVE}, {CAVE.DIR}. BRING {SEER.HIM} OUT."

stage wolves
  goal kill 4 wolf in cave
  then found
  journal "SOMETHING IN THE DARK OF {CAVE} IS HUNGRIER THAN {SEER} IS AFRAID. CLEAR THE WOLVES FROM THE CAVE."

stage found
  talk seer
  say "THREE DAYS IN THE BELLY OF THIS DARK, WITH THE WOLVES CIRCLING. I PRAYED. I DIDN'T MEAN TO. IT JUST CAME OUT. ...FINE. FINE! I'LL GO TO {FAR}. I'LL SHOUT IT IN THEIR STREETS. AND I HOPE THEY DON'T LISTEN."
  opt "THEN GO. I'LL WALK WITH YOU." -> @t1
  opt "YOU HOPE THEY DIE?" -> hope

stage hope
  talk seer
  say "THEY BURNED OUR BREAD! MY GRANDMOTHER STARVED THAT WINTER! ...BUT THE CHILDREN IN {FAR} NOW DIDN'T BURN ANYTHING. I KNOW. I KNOW. THAT'S WHY I RAN. <<grief>>"
  do set dragged 1
  opt "WALK WITH ME TO {FAR}." -> @t1

stage to_far
  goal goto far
  then warned
  do moves seer far
  journal "{SEER} WILL CRY THE WARNING IN THE STREETS OF {FAR}, WHETHER {SEER.HE} LIKES IT OR NOT. GO WITH {SEER.HIM}."

stage warned
  talk elder
  say "A PRIEST OF {HOME} IN OUR STREETS, SHOUTING THAT WE ARE CRUEL? ...WE ARE. WE KNOW IT. THE WHOLE TOWN IS PUTTING ON SACKCLOTH, EVEN THE MAGISTRATE, EVEN THE GOATS. TELL {SEER.HIM} WE HEARD."
  opt "I'LL TELL {SEER.HIM}." -> @t2

stage sulk
  talk seer
  say "THEY LISTENED. OF COURSE THEY LISTENED. I KNEW THEY WOULD; I KNEW THE GOD WOULD FORGIVE THEM. THAT'S WHY I RAN. I'M SITTING ON THIS HILL TILL SOMETHING BURNS, {PLAYER}, AND NOTHING WILL."
  opt "WOULD YOU RATHER THEY DIED?" -> lesson
  opt "THEY'RE CHILDREN, LIKE YOURS." -> lesson
  opt "SIT, THEN. I'M GOING HOME." -> sulk_end

stage lesson
  talk seer
  say "...THERE WAS A VINE ON THIS HILL THAT SHADED ME, AND IT WITHERED THIS MORNING, AND I WEPT FOR IT. A VINE. AND I'D HAVE WATCHED [[=forty]] THOUSAND PEOPLE BURN AND CALLED IT JUSTICE. <<apology>>"
  opt "COME HOME, {SEER}." -> mercy_end

stage mercy_end
  end success
  say "{SEER} WALKED HOME WITHOUT A WORD AND SAT ON THE STOOL BY THE ALTAR. A WEEK LATER A CART CAME FROM {FAR} WITH GRAIN FOR {HOME}'S GRANARY, AND NO MESSAGE. NONE WAS NEEDED."
  journal "{FAR} HEARD THE WARNING AND TURNED FROM ITS CRUELTY. {SEER} CAME HOME TO {HOME}, AND LEARNED TO BEAR IT."
  do reward fair
  do remember giver "A CART OF GRAIN FROM {FAR}. FROM {FAR}! [[=forty]] YEARS. {SEER} UNLOADED IT, AND WEPT, AND WOULDN'T SAY WHY."
  do remember seer "I'M STILL ANGRY. I'M ANGRY AND I'M GLAD. I DIDN'T KNOW YOU COULD BE BOTH."
  do fact "A NOVICE OF {HOME} WARNED {FAR} OF HEAVEN'S ANGER. {FAR} REPENTED, AND SENT GRAIN TO {HOME} AFTER [[=forty]] YEARS OF HATE."
  do mark ninefold_mercy

stage sulk_end
  end success
  say "{SEER} SAT ON THE HILL ABOVE {FAR} FOR A WEEK, WAITING FOR FIRE. NONE CAME. THEN {SEER} WENT AWAY, NOT HOME, AND {GIVER} HAS AN EMPTY STOOL STILL."
  journal "{FAR} HEARD THE WARNING AND REPENTED. {SEER} COULD NOT FORGIVE THEM FOR IT, AND DID NOT COME HOME."
  do reward small
  do remember giver "{FAR} WAS SPARED. {SEER} WASN'T, NOT REALLY. I PRAY FOR {SEER.HIM} WHERE THE STOOL IS."
  do fact "{FAR} WAS WARNED OF ITS CRUELTY BY A NOVICE OF {HOME}, AND TURNED FROM IT. THE NOVICE NEVER FORGAVE THEM FOR LISTENING."
)SAGA";

// ---------------------------------------------------------------- Solomon
const char* const kTwoMothers = R"SAGA(
title [[THE DISPUTED CHILD|TWO MOTHERS AND A CRADLE|THE JUDGEMENT AT THE GATE]]
hook npc guard
pitch "[[pitch=WHAT'S ALL THAT SHOUTING?|WHO'S BEING JUDGED TODAY?|WHY IS THE CROWD AT THE GATE?]]"
hint "[[~pitch|THEY'VE BEEN SCREAMING AT EACH OTHER SINCE DAWN. THE BABY'S THE ONLY ONE WHO'S STOPPED.|TWO WOMEN, ONE BABY, NO LORD WITHIN THREE DAYS' RIDE. AND I'M ONLY A GATE-GUARD.|THEY'VE BEEN SCREAMING AT EACH OTHER SINCE DAWN. THE BABY'S THE ONLY ONE WHO'S STOPPED.]]"
role giver giver
role home site home
role first person female at home
role second person female at home
role far site village near
role midwife person female at far
var proof 0
slot t1 -> hearing deceit rival betrayal world
slot t2 -> verdict mercy wonder identity price

stage start
  talk giver
  say "TWO WOMEN SHARE A HOUSE. EACH HAD A CHILD THE SAME WEEK. IN THE NIGHT ONE CHILD DIED. NOW {FIRST} AND {SECOND} EACH SWEAR THE LIVING ONE IS HERS. THE LORD'S JUDGE IS AWAY. THE TOWN SAYS YOU MUST DECIDE, BEING NOBODY'S COUSIN. <<plea>>"
  opt "I'LL HEAR THEM BOTH." -> @t1
  opt "WHO DELIVERED THE CHILDREN?" -> who
  opt "THIS ISN'T MINE TO JUDGE." -> refused

stage refused
  end fail
  say "NO. IT'S NOBODY'S. THAT'S THE TROUBLE. <<doubt>>"
  journal "YOU WOULD NOT JUDGE BETWEEN {FIRST} AND {SECOND}. THE CHILD STAYS WITH WHOEVER HOLDS IT TIGHTEST."
  do remember giver "THEY'RE STILL FIGHTING OVER THE CHILD. SOMEONE WILL HAVE TO DECIDE. IT WON'T BE YOU, I SUPPOSE."

stage who
  talk giver
  say "{MIDWIFE}, THE MIDWIFE OF {FAR}. SHE'D KNOW EACH CHILD BY THE MARKS. BUT SHE'S ALREADY GONE HOME, AND IT'S A HALF-DAY'S WALK."
  opt "THEN I'LL WALK IT." -> to_midwife
  opt "I'LL HEAR THE WOMEN FIRST." -> @t1

stage to_midwife
  goal goto far
  then midwife_talk
  journal "THE MIDWIFE {MIDWIFE} OF {FAR} DELIVERED BOTH CHILDREN IN {HOME}. ASK HER WHAT SHE SAW."

stage midwife_talk
  talk midwife
  say "BOTH, YES. {FIRST}'S CHILD HAD A STRAWBERRY MARK BEHIND THE LEFT EAR, SMALL AS A LENTIL. {SECOND}'S HAD NONE. SHE'D LOST ONE BEFORE, {SECOND}. TWO WINTERS AGO. SHE NEVER STOPPED HEARING IT CRY."
  do set proof 1
  opt "THEN I KNOW WHOSE IT IS." -> @t1

stage hearing
  talk first
  ?m_grief say "<<grief>> HE'S MINE. I KNOW HIS CRY FROM THE NEXT STREET. SHE ROLLED ON HER OWN IN THE NIGHT AND SWAPPED THEM WHILE I SLEPT. I WOKE AND THE CHILD AT MY BREAST WAS DEAD AND IT WASN'T HIM."
  !m_grief say "HE'S MINE. I KNOW HIS CRY FROM THE NEXT STREET. SHE ROLLED ON HER OWN IN THE NIGHT AND SWAPPED THEM WHILE I SLEPT. I WOKE AND THE CHILD AT MY BREAST WAS DEAD, AND IT WASN'T HIM. ASK HER. LOOK AT HER FACE."
  opt "NOW I'LL HEAR {SECOND}." -> other

stage other
  talk second
  say "LIES. HE'S MINE. LOOK AT HIM SMILE AT ME. SHE'S THE ONE WHO ROLLED IN HER SLEEP. SHE'S MAD WITH IT. ...DON'T LOOK AT ME LIKE THAT. HE'S MINE."
  opt "THE MIDWIFE SAYS OTHERWISE." if var proof 1 -> @t2
  opt "BRING ME A KNIFE." -> knife
  opt "GIVE HIM TO {SECOND}." -> wrong_end

stage knife
  talk giver
  say "A KNIFE? FOR... YOU CAN'T. YOU WOULDN'T. ...{FIRST} IS ON HER KNEES. SHE'S SCREAMING FOR YOU TO GIVE HIM TO {SECOND}, ALIVE, AND LET HER NEVER SEE HIM AGAIN. AND {SECOND} SAYS: DIVIDE HIM, THEN, AND LET NEITHER HAVE HIM."
  opt "NOW I KNOW." -> @t2

stage verdict
  talk second
  say "...YES. ALL RIGHT. HE ISN'T MINE. MINE IS IN THE GROUND, AND THE GROUND IS COLD, AND I COULDN'T... TWICE, {PLAYER}. TWICE IN TWO WINTERS. WHAT WILL YOU DO WITH ME?"
  opt "GO TO THE LORD'S JUDGE." -> law_end
  opt "SHE NEEDS MERCY, NOT A CELL." -> mercy_end

stage law_end
  end success
  say "THE CHILD WENT HOME IN {FIRST}'S ARMS. {SECOND} WENT TO THE LORD'S JUDGE IN THE SPRING, AND THE JUDGE SAID YOUR VERDICT WAS WISE, AND HIS OWN WAS HARD."
  journal "YOU JUDGED BETWEEN TWO MOTHERS IN {HOME}. THE LIVING CHILD IS WITH {FIRST}. {SECOND} ANSWERED TO THE LORD'S LAW."
  do reward fair
  do remember first "HE'S WALKING NOW. HE WALKS LIKE A DRUNK SAILOR. I KEEP TELLING HIM ABOUT YOU. HE DOESN'T CARE. HE WILL."
  do fact "THEY SAY A STRANGER JUDGED BETWEEN TWO MOTHERS IN {HOME}, AND FOUND THE TRUE ONE BY THE ONE WHO WOULD GIVE HER CHILD UP."
  do mark wise_judgement

stage mercy_end
  end success
  say "THE CHILD WENT HOME IN {FIRST}'S ARMS. {SECOND} WAS NOT TAKEN TO THE JUDGE. IN THE AUTUMN, {FIRST} WAS SEEN LETTING HER HOLD THE BABY AT THE WELL, JUST FOR A MOMENT. NOBODY KNOWS WHAT TO SAY ABOUT THAT."
  journal "YOU JUDGED BETWEEN TWO MOTHERS IN {HOME}. THE LIVING CHILD IS WITH {FIRST}. {SECOND} WAS SHOWN MERCY."
  do reward fair
  do remember second "SHE LETS ME HOLD HIM SOMETIMES. I DON'T DESERVE IT. I HOLD HIM VERY CAREFULLY, AND THEN I GIVE HIM BACK."
  do fact "A STRANGER JUDGED BETWEEN TWO MOTHERS IN {HOME}, AND GAVE THE LIVING CHILD TO THE TRUE ONE, AND MERCY TO THE OTHER."
  do mark wise_judgement

stage wrong_end
  end fail
  say "{SECOND} WALKED AWAY WITH THE CHILD, HOLDING HIM TOO TIGHT. {FIRST} DID NOT SCREAM. THAT WAS THE WORST PART. SHE JUST SAT DOWN IN THE ROAD."
  journal "YOU GAVE THE CHILD TO {SECOND}. {FIRST} SITS BY THE ROAD IN {HOME}, AND WILL NOT SAY YOUR NAME."
  do remember first "YOU. YOU GAVE MY SON AWAY. I HAVE NOTHING ELSE TO SAY TO YOU. GET AWAY FROM MY DOOR."
  do fact "A STRANGER JUDGED BETWEEN TWO MOTHERS IN {HOME}, AND {HOME} STILL ARGUES OVER WHETHER THE STRANGER JUDGED RIGHT."
)SAGA";

// ---------------------------------------------------------------- Samson
const char* const kShornStrength = R"SAGA(
title [[THE STRONGMAN'S SECRET|THE SEVEN LOCKS|THE PILLARS OF THE HALL]]
hook npc innkeeper
pitch "[[pitch=WHERE'S YOUR CHAMPION TONIGHT?|WHO USED TO SIT IN THAT CORNER?|WHY IS THE BIG CHAIR EMPTY?]]"
hint "[[~pitch|THAT CHAIR IS OUR CHAMPION'S. NOBODY ELSE WILL SIT IN IT. IT'S BEEN EMPTY SINCE THE THAW.|THE BANDITS DIDN'T COME FOR TEN YEARS, BECAUSE OF ONE MAN. NOW THEY COME EVERY MONTH.|THE BANDITS DIDN'T COME FOR TEN YEARS, BECAUSE OF ONE MAN. NOW THEY COME EVERY MONTH.]]"
role giver giver
role home site home
role camp site camp near
role champ person male at camp
role lover person female at home
role foe foe male at camp
var warned 0
slot t1 -> go betrayal deceit mercy rival
slot t2 -> pillars price wonder world

stage start
  talk giver
  say "{CHAMP} KEPT {HOME} SAFE TEN YEARS. STRONG AS AN OX, AND SWORN BY HIS MOTHER TO A VOW: NO BLADE ON HIS HAIR. HE TOLD ONE PERSON WHY, {LOVER}, AND {LOVER} TOLD {FOE} OF {CAMP} FOR [[price=ELEVEN HUNDRED|TWELVE HUNDRED|A THOUSAND]] SILVER."
  opt "WHERE IS {CHAMP} NOW?" -> where
  opt "I'LL SPEAK WITH {LOVER}." -> lover_talk
  opt "A FOOL'S SECRET. LET IT GO." -> refused

stage refused
  end fail
  say "A FOOL'S SECRET. HE WAS A FOOL FOR HER. WE'RE ALL FOOLS FOR SOMEONE. <<farewell>>"
  journal "YOU LEFT {CHAMP} TO THE BANDITS OF {CAMP}."
  do remember giver "THE BIG CHAIR'S STILL EMPTY. THE BANDITS STILL COME. NOBODY TALKS ABOUT {CHAMP} NOW."

stage where
  talk giver
  say "THEY CAME WHILE HE SLEPT WITH HIS HEAD IN {LOVER}'S LAP. SHAVED HIM AND TOOK HIM WEAK AS A KITTEN TO {CAMP}. THEY'VE BLINDED HIM, THEY SAY, AND SET HIM TO TURN THE MILLSTONE LIKE A MULE. <<curse>>"
  opt "THEN I'LL GET HIM OUT." -> @t1
  opt "FIRST, {LOVER}." -> lover_talk

stage lover_talk
  talk lover
  say "THEY SAID THEY ONLY WANTED TO KNOW. FOUR TIMES HE LIED TO ME: GREEN ROPES, NEW ROPES, A LOOM. THE FOURTH TIME HE TOLD ME TRUE, BECAUSE I WEPT. I TOOK THE SILVER. I'VE NOT SPENT ONE COIN OF IT. <<apology>>"
  opt "THEN HELP ME SAVE HIM." -> helps
  opt "YOU'LL ANSWER FOR IT." -> answers
  opt "WHY DID YOU DO IT?" -> why

stage why
  talk lover
  say "MY BROTHERS OWED {FOE}. EVERY ONE OF THEM. IT WAS HIM OR MY FAMILY, AND HE WAS SO STRONG, I THOUGHT HE'D BREAK FREE THE WAY HE ALWAYS DID. HE ALWAYS DID. <<grief>>"
  opt "THEN HELP ME SAVE HIM." -> helps
  opt "YOU'LL ANSWER FOR IT." -> answers

stage helps
  talk lover
  say "THE SILVER. ALL OF IT. GIVE IT TO WHOEVER WILL OPEN THE GATE OF {CAMP}. AND TELL HIM... NO. DON'T TELL HIM ANYTHING. HE'LL KNOW MY VOICE IF HE NEEDS TO."
  do set warned 1
  do remember lover "I GAVE IT ALL BACK. IT DOESN'T UNDO ANYTHING. I GO TO THE TEMPLE EVERY DAY. I DON'T PRAY. I JUST SIT."
  opt "I'LL TAKE IT." -> @t1

stage answers
  talk lover
  say "I WILL. I ALREADY DO. EVERY NIGHT. <<grief:shame>>"
  do fact "{LOVER} OF {HOME} SOLD THE SECRET OF {CHAMP}'S STRENGTH TO THE BANDITS OF {CAMP}, AND THE WHOLE TOWN KNOWS IT."
  opt "GOOD." -> @t1

stage go
  goal goto camp
  then camp_gate
  journal "{CHAMP}, {HOME}'S SHORN CHAMPION, TURNS A MILLSTONE BLIND IN THE BANDIT CAMP OF {CAMP}. GET HIM OUT."

stage camp_gate
  talk champ
  say "WHO'S THERE? YOU SMELL OF {HOME}. ...MY HAIR IS GROWING BACK. I CAN FEEL IT ON MY NECK. TONIGHT THEY FEAST, AND THEY'LL BRING ME OUT TO LAUGH AT. PUT MY HANDS ON THE TWO MIDDLE PILLARS OF THEIR HALL. THAT'S ALL I ASK."
  opt "NO. WE GO OUT THE BACK, NOW." -> escape
  opt "I'LL PUT YOUR HANDS THERE." -> @t2

stage escape
  goal slay foe
  then freed_end
  journal "GET {CHAMP} OUT OF {CAMP} ALIVE. {FOE} STANDS BETWEEN YOU AND THE GATE."

stage pillars
  talk champ
  say "<<oath>> GO NOW. RUN, AND DON'T LOOK BACK, AND WHEN YOU HEAR IT COME DOWN, KNOW I WAS STRONG ONE MORE TIME. TELL {HOME} THAT. TELL {LOVER}... TELL HER I KNOW SHE WEPT."
  opt "{CHAMP}, NO. COME WITH ME." -> escape
  opt "I'LL TELL THEM." -> pillars_end

stage freed_end
  end success
  say "YOU LED {CHAMP} HOME BLIND, A HAND ON YOUR SHOULDER, AND {HOME} CAME OUT TO MEET HIM. HE SITS IN THE BIG CHAIR AGAIN. HE IS NOT AS STRONG AS HE WAS. HE IS KINDER."
  journal "{CHAMP} IS HOME IN {HOME}, BLIND, HIS HAIR GROWING. {FOE} OF {CAMP} IS DEAD."
  do reward rich
  do remember giver "{CHAMP} SITS IN THE BIG CHAIR AND TELLS THE CHILDREN STORIES. HE DOESN'T LIFT OXEN NOW. HE DOESN'T NEED TO."
  do fact "{CHAMP} OF {HOME}, SHORN AND BLINDED BY THE BANDITS OF {CAMP}, WAS BROUGHT HOME ALIVE, AND {FOE} WAS SLAIN."
  do mark brought_the_strongman_home

stage pillars_end
  end success
  say "YOU WERE A FIELD AWAY WHEN THE HALL OF {CAMP} CAME DOWN WITH A SOUND LIKE THE SKY BREAKING. {FOE} AND ALL HIS FEAST WERE UNDER IT. SO WAS {CHAMP}. THE BANDITS NEVER CAME TO {HOME} AGAIN."
  journal "{CHAMP} PULLED DOWN THE HALL OF {CAMP} ON HIMSELF AND ALL HIS CAPTORS. {HOME} IS SAFE. THE BIG CHAIR IS EMPTY."
  do hide foe
  do reward fair
  do remember giver "NOBODY SITS IN THE BIG CHAIR. NOBODY EVER WILL. WE PUT FLOWERS ON IT IN SPRING."
  do remember lover "HE SAID HE KNEW I WEPT? ...THANK YOU FOR TELLING ME. I DON'T KNOW IF THAT'S WORSE OR BETTER. BOTH."
  do fact "{CHAMP} OF {HOME} PULLED DOWN THE HALL OF {CAMP} WITH HIS LAST STRENGTH, AND KILLED MORE BANDITS DYING THAN HE EVER DID LIVING."
  do mark pillars_of_the_hall
)SAGA";

// ---------------------------------------------------------------- the plague and the intercessor
const char* const kTheCenser = R"SAGA(
title [[THE ONE WHO STOOD BETWEEN|THE CENSER IN THE STREET|THE SWEATING SICKNESS]]
hook npc priest
pitch "[[pitch=WHY ARE THE DOORS CHALKED?|WHAT ARE THE WHITE CROSSES FOR?|WHY IS THE TEMPLE FULL?]]"
hint "[[~pitch|THEY SAY IT'S JUDGEMENT. THEY'RE ASKING ME FOR WHAT. I DON'T KNOW WHAT.|THEY SAY IT'S JUDGEMENT. THEY'RE ASKING ME FOR WHAT. I DON'T KNOW WHAT.|CHALK ON THE DOOR MEANS SICKNESS IN THE HOUSE. COUNT THE CHALK MARKS ON YOUR WAY IN. I'VE STOPPED.]]"
role giver giver
role home site home
role ruin ruin near
role sick resident any
role elder person [[male|female]] at home
var knew 0
slot t1 -> street mercy price deceit betrayal
slot t2 -> reckoning wonder prophecy world

stage start
  talk giver
  say "A SWEATING SICKNESS IN {HOME}. [[houses=NINE|ELEVEN|SEVEN]] HOUSES CHALKED. {SICK} IS IN THE WORST OF IT. THEY SAY {GIVER.GOD} IS ANGRY. I'D CALL THAT FOOLISH, BUT THE OLD ONES WHISPER ABOUT SOMETHING THIS TOWN DID AT {RUIN}, LONG AGO. <<urgency>>"
  opt "WHAT DID THE TOWN DO?" -> elder_talk
  opt "WHAT CAN BE DONE NOW?" -> censer
  opt "SICKNESS ISN'T JUDGEMENT." -> refused

stage refused
  end fail
  say "NO. PROBABLY NOT. BUT A FRIGHTENED TOWN NEEDS SOMETHING TO DO WITH ITS HANDS. <<farewell>>"
  journal "YOU LEFT {HOME} TO ITS SICKNESS AND ITS WHISPERS."
  do remember giver "THE CHALK IS WASHING OFF THE DOORS NOW. NOT ALL OF THEM. I BURIED [[=houses]] PEOPLE. I KEEP THEIR NAMES."

stage elder_talk
  talk elder
  say "WHEN I WAS SMALL, PLAGUE CAME TO {RUIN.OLD}, AND ITS PEOPLE CAME HERE BEGGING. OUR GRANDFATHERS BARRED THE GATE AND THREW STONES. THEY DIED OUTSIDE OUR WALLS. NOBODY BURIED THEM. NOBODY SAID THEIR NAMES. <<grief>>"
  do set knew 1
  opt "THEN SOMEONE SHOULD SAY THEM." -> to_ruin
  opt "THE DEAD DON'T SEND FEVERS." -> censer

stage to_ruin
  goal use grave in ruin
  then @t1
  journal "THE PEOPLE OF {RUIN.OLD} DIED OUTSIDE {HOME}'S BARRED GATE. GO TO {RUIN} AND READ THEIR NAMES FROM THE GRAVES."

stage censer
  talk giver
  say "THE OLD BOOKS SAY THAT WHEN A PLAGUE WALKED, A PRIEST TOOK FIRE FROM THE ALTAR AND STOOD IN THE STREET BETWEEN THE LIVING AND THE DEAD, AND IT STOPPED. I'M TOO OLD TO STAND THAT LONG. AND I'M AFRAID."
  opt "THEN I'LL STAND THERE." -> @t1
  opt "FIND HERBS, NOT OMENS." -> herbs

stage herbs
  goal fetch "A BUNDLE OF FEVERFEW" in ruin
  then herbs_back
  journal "FEVERFEW GROWS WILD IN THE BROKEN COURTS OF {RUIN}. FETCH ENOUGH TO BREAK THE FEVER IN {HOME}."

stage herbs_back
  talk sick
  say "...COOLER. I'M COOLER. THE BITTER TEA. EVERYONE IN THE STREET IS DRINKING IT, AND CURSING, AND COOLER. <<relief>>"
  opt "DRINK IT ALL." -> herb_end

stage street
  talk sick
  ?t1 say "YOU'RE STILL HERE. EVERYONE ELSE WENT INDOORS. YOU STOOD IN THE STREET WITH THE PRIEST'S FIRE ALL NIGHT. THE FEVER BROKE AN HOUR AGO. I THOUGHT I DREAMED YOU."
  !t1 say "YOU'RE STILL HERE. EVERYONE ELSE WENT INDOORS. YOU STOOD IN THE STREET ALL NIGHT. THE FEVER BROKE AN HOUR AGO, AND I THOUGHT I DREAMED YOU, STANDING THERE WITH THE FIRE."
  opt "TELL THE TOWN TO WASH THE DOORS." -> @t2

stage reckoning
  talk giver
  say "IT'S PASSING. THE CHALK IS COMING OFF THE DOORS. NOW THE TOWN WANTS TO KNOW WHAT IT WAS, {PLAYER}. A JUDGEMENT? A FEVER? I'LL SAY WHAT YOU TELL ME TO SAY FROM THE STEPS."
  opt "SAY THE NAMES OF THE DEAD." if var knew 1 -> names_end
  opt "SAY IT WAS A FEVER, AND IT'S GONE." -> fever_end
  opt "SAY {GIVER.GOD} WAS ANGRY." -> fear_end

stage names_end
  end success
  say "FROM THE TEMPLE STEPS {GIVER} READ THE NAMES OF {RUIN.OLD}'S DEAD, EVERY ONE, AND {HOME} STOOD IN THE SQUARE AND LISTENED. THEN THEY BUILT A GATE THAT STANDS OPEN."
  journal "THE SICKNESS LEFT {HOME}. THE TOWN NAMED THE DEAD OF {RUIN.OLD} WHOM ITS GRANDFATHERS LEFT OUTSIDE THE WALLS."
  do reward fair
  do remember elder "THEY READ THE NAMES. MY OWN GRANDFATHER THREW STONES. I'VE CARRIED THAT. NOW THE WHOLE TOWN CARRIES IT, AND IT'S LIGHTER."
  do fact "{HOME} ONCE BARRED ITS GATE AGAINST THE PLAGUE-STRICKEN OF {RUIN.OLD}. NOW IT READS THEIR NAMES EVERY YEAR, AND KEEPS ITS GATE OPEN."
  do mark named_the_dead

stage fever_end
  end success
  say "THE TOWN WASHED ITS DOORS AND OPENED ITS SHUTTERS AND WENT BACK TO ITS QUARRELS. NOBODY SPOKE OF JUDGEMENT. THAT WAS A MERCY OF A KIND, AND {GIVER} SAID SO, AND DIDN'T SOUND SURE."
  journal "THE SICKNESS LEFT {HOME}. IT WAS A FEVER, THEY SAY NOW, AND ONLY A FEVER."
  do reward fair
  do remember sick "I WAS IN THE WORST OF IT AND I LIVED. THE PRIEST SAYS IT WAS A FEVER. I SAY IT WAS YOU IN THE STREET."
  do fact "A SWEATING SICKNESS CAME TO {HOME} AND WENT. SOME SAY A STRANGER STOOD IN THE STREET ALL NIGHT WITH FIRE, AND IT TURNED."

stage herb_end
  end success
  say "THE BITTER TEA WENT ROUND {HOME} AND THE FEVER WENT OUT OF IT. THE OLD ONES STILL WHISPER ABOUT JUDGEMENT. THE YOUNG ONES PLANT FEVERFEW IN THEIR WINDOW BOXES."
  journal "FEVERFEW FROM {RUIN} BROKE THE SICKNESS IN {HOME}. THE TOWN STILL DOES NOT KNOW IF IT WAS A JUDGEMENT."
  do reward fair
  do remember sick "BITTER AS BILE, THAT TEA. I DRINK IT EVERY SPRING NOW. CAN'T BE TOO CAREFUL."
  do fact "FEVERFEW FROM THE COURTS OF {RUIN} BROKE A SWEATING SICKNESS IN {HOME}. IT GROWS IN EVERY WINDOW BOX THERE NOW."

stage fear_end
  end fail
  say "{GIVER} SAID IT FROM THE STEPS, AND {HOME} BELIEVED IT. NOW THEY LOOK FOR WHO ANGERED THE GOD, AND THEY ARE LOOKING HARDEST AT THE POOR, AND THE STRANGERS."
  journal "THE SICKNESS PASSED, BUT {HOME} IS HUNTING FOR SOMEONE TO BLAME FOR {GIVER.GOD}'S ANGER."
  do remember giver "I SAID WHAT YOU TOLD ME. THEY THREW STONES AT A TINKER YESTERDAY. I'VE BEEN AFRAID BEFORE. THIS IS WORSE."
  do fact "AFTER THE SICKNESS, {HOME} WAS TOLD IT WAS {GIVER.GOD}'S ANGER, AND STRANGERS ARE NOT WELCOME THERE NOW."
)SAGA";

// ---------------------------------------------------------------- Esther
const char* const kSuchATime = R"SAGA(
title [[THE QUEEN WHO MUST SPEAK|FOR SUCH A TIME AS THIS|THE CHANCELLOR'S DECREE]]
hook npc merchant
pitch "[[pitch=WHAT'S THAT NOTICE ON THE GATE?|WHY IS THE MARKET SO QUIET?|WHAT DOES THE CROWN WANT NOW?]]"
hint "[[~pitch|THE CROWN'S MAN NAILED A DECREE TO THE GATE AND NOBODY WILL READ IT ALOUD. I CAN'T READ.|THEY SAY THE CHANCELLOR HAS A LONG MEMORY AND A SHORT LIST, AND THIS TOWN IS ON IT.|THE CROWN'S MAN NAILED A DECREE TO THE GATE AND NOBODY WILL READ IT ALOUD. I CAN'T READ.]]"
role giver giver
role home site home
role kingdom kingdom home
role capital capital kingdom
role lord ruler kingdom
role consort person female at capital
role chancellor person [[male|female]] at capital
var proof 0
slot t1 -> road betrayal deceit rival price
slot t2 -> banquet wonder mercy world

stage start
  talk giver
  say "{CHANCELLOR}, CHANCELLOR TO {KINGDOM.LORD}, HAS A DECREE: ON THE [[day=THIRTEENTH|NINTH|TWELFTH]] DAY OF NEXT MONTH THE LANDS OF {HOME} GO TO THE CROWN, AND WE GO WHERE WE'RE SENT. WHY? BECAUSE I WOULDN'T BOW TO {CHANCELLOR.HIM} IN THE STREET. <<grief:fear>>"
  opt "WHO CAN STOP IT?" -> who
  opt "WHY WOULDN'T YOU BOW?" -> bow
  opt "YOU SHOULD HAVE BOWED." -> refused

stage bow
  talk giver
  say "I BOW TO {GIVER.GOD} AND TO THE CROWN. NOT TO A CLERK WITH A CHAIN AROUND {CHANCELLOR.HIS} NECK. PRIDE, MAYBE. IT'S THE ONLY THING THEY HAVEN'T TAXED. <<boast>>"
  opt "WHO CAN STOP THE DECREE?" -> who
  opt "PRIDE WILL COST {HOME} ITS LAND." -> refused

stage refused
  end fail
  say "YES. IT WILL. AND YOU WON'T HELP. NOBODY DOES, WHEN IT'S THE CROWN. <<farewell>>"
  journal "YOU WOULD NOT CARRY {GIVER}'S PLEA TO {CAPITAL}. THE DECREE STANDS."
  do remember giver "WE'RE PACKING. THE DECREE STANDS. SOME OF US WILL GO TO THE HILLS. SOME OF US WILL BOW, NOW. I WON'T."

stage who
  talk giver
  say "MY [[NIECE|WARD|COUSIN'S GIRL]], {CONSORT}. TAKEN INTO {KINGDOM.LORD}'S HOUSE AT {CAPITAL} AS CONSORT. NOBODY THERE KNOWS SHE WAS BORN IN {HOME}. I TOLD HER TO HIDE IT. NOW I NEED HER TO SAY IT. CARRY MY WORDS TO HER."
  opt "I'LL CARRY THEM." -> @t1
  opt "AND IF SPEAKING KILLS HER?" -> risk

stage risk
  talk giver
  say "THEN SHE DIES SPEAKING, AND THE REST OF US DIE ANYWAY. WHO KNOWS IF SHE CAME TO THAT HOUSE FOR ANY REASON BUT THIS? TELL HER THAT. TELL HER: FOR SUCH A TIME AS THIS. <<vow>>"
  opt "I'LL TELL HER." -> @t1

stage road
  goal goto capital
  then court
  journal "THE LANDS OF {HOME} ARE FORFEIT BY {CHANCELLOR}'S DECREE. CARRY {GIVER}'S WORDS TO {CONSORT}, CONSORT TO {KINGDOM.LORD}, AT {CAPITAL}."

stage court
  talk consort
  say "{HOME}. NOBODY HAS SAID THAT WORD TO ME IN FOUR YEARS. ...YOU DON'T UNDERSTAND THIS HOUSE. WHOEVER GOES TO THE LORD UNSUMMONED DIES, UNLESS THE LORD LIFTS A HAND. I HAVEN'T BEEN SUMMONED IN [[days=THIRTY|FORTY|TWENTY]] DAYS."
  opt "FOR SUCH A TIME AS THIS." -> resolve
  opt "THEN I'LL GO IN YOUR PLACE." -> instead
  opt "BRING PROOF OF {CHANCELLOR}'S LIE." -> proof

stage proof
  talk chancellor
  say "A TRAVELLER FROM... WHERE DID YOU SAY? {HOME}. HOW INTERESTING. YOU'LL WANT TO SEE THE DECREE'S SEAL, I EXPECT. IT'S GENUINE. EVERYTHING I DO IS GENUINE. <<threat>>"
  opt "AND THE GRUDGE BEHIND IT?" check level 3 -> slipped else resolve
  opt "I'VE SEEN ENOUGH." -> resolve

stage slipped
  talk chancellor
  say "GRUDGE? THAT STIFF-NECKED... AH. YOU HEARD THAT. I SHOULD WATCH MY TEMPER. SO SHOULD YOU, IN THESE HALLS. THEY'RE VERY LONG HALLS, AND THE STAIRS ARE STEEP."
  do set proof 1
  opt "I'LL MIND THE STAIRS." -> resolve

stage instead
  talk consort
  say "YOU'D DIE AT THE DOOR AND CHANGE NOTHING. NO. IF IT'S DEATH, IT'S MINE. FAST WITH ME THREE DAYS. TELL {GIVER} TO MAKE ALL {HOME} FAST. AND THEN I'LL GO IN. AND IF I PERISH, I PERISH."
  opt "THREE DAYS, THEN." -> @t2

stage resolve
  talk consort
  say "...I'VE BEEN SAFE SO LONG I FORGOT IT WAS A KIND OF PRISON. THREE DAYS' FAST. THEN I'LL GO IN, AND I'LL INVITE THE LORD AND {CHANCELLOR} TO A BANQUET. AND THERE I'LL SPEAK. AND IF I PERISH, I PERISH."
  opt "I'LL BE AT THE BANQUET." -> @t2

stage banquet
  talk lord
  say "MY CONSORT SAYS SHE IS OF {HOME}. THAT {CHANCELLOR}'S DECREE WOULD SELL HER OWN PEOPLE FOR A SLIGHT IN THE STREET. YOU CARRIED HER WORDS HERE, STRANGER. IS IT TRUE?"
  opt "{CHANCELLOR} ADMITTED THE GRUDGE." if var proof 1 -> justice_end
  opt "IT'S TRUE. LIFT THE DECREE." check level 4 -> justice_end else mercy_end
  opt "SHE SPEAKS FOR HERSELF." -> mercy_end

stage justice_end
  end success
  say "{KINGDOM.LORD} TORE THE DECREE IN HALF AT THE TABLE. {CHANCELLOR} WAS STRIPPED OF THE CHAIN BEFORE THE SWEET COURSE. {CONSORT} DID NOT LOOK AWAY ONCE."
  journal "THE DECREE AGAINST {HOME} IS TORN UP. {CHANCELLOR} HAS FALLEN, AND {CONSORT} SPOKE AND LIVED."
  do reward rich
  do rep kingdom 4
  do remember giver "THE [[=day]] DAY CAME AND WE HELD A FEAST INSTEAD. WE'LL HOLD IT EVERY YEAR. YOU'RE INVITED. YOU'RE ALWAYS INVITED."
  do fact "{CONSORT}, CONSORT TO {KINGDOM.LORD}, REVEALED SHE WAS BORN IN {HOME}, AND SAVED IT FROM {CHANCELLOR}'S DECREE."
  do mark such_a_time

stage mercy_end
  end success
  say "THE LORD COULD NOT UNDO A SEALED DECREE, BUT WROTE A SECOND ONE: {HOME} MAY KEEP WHAT IT CAN HOLD. IT IS NOT JUSTICE. IT IS A DOOR LEFT OPEN. {CHANCELLOR} STILL WEARS THE CHAIN."
  journal "{CONSORT} SPOKE FOR {HOME} AND LIVED. THE DECREE STANDS, BUT A SECOND ONE LETS {HOME} KEEP WHAT IT CAN DEFEND."
  do reward fair
  do rep kingdom 1
  do remember giver "WE KEPT THE LAND. MOST OF IT. {CHANCELLOR} STILL WATCHES US. I STILL DON'T BOW. NOW I DON'T BOW WITH A SPEAR IN MY HAND."
  do fact "{CONSORT}, CONSORT TO {KINGDOM.LORD}, SPOKE FOR {HOME} AT A BANQUET AND WAS NOT KILLED FOR IT. THE CHANCELLOR STILL WEARS THE CHAIN."
)SAGA";

// ---------------------------------------------------------------- the good Samaritan
const char* const kOtherSide = R"SAGA(
title [[THE STRANGER ON THE ROAD|THE OTHER SIDE OF THE ROAD|WHO IS MY NEIGHBOUR]]
hook npc innkeeper
pitch "[[pitch=WHO'S THE BANDAGED MAN UPSTAIRS?|WHY ARE YOU BOILING RAGS?|WHO BROUGHT THAT MULE IN?]]"
hint "[[~pitch|I'VE A MAN UPSTAIRS WHO CAN'T TELL ME HIS NAME. THE ONE WHO BROUGHT HIM LEFT SILVER AND RODE ON.|TWO GOOD PEOPLE PASSED HIM ON THE ROAD TODAY. I KNOW THEM BOTH. I'LL NEVER LOOK AT THEM THE SAME.|I'VE A MAN UPSTAIRS WHO CAN'T TELL ME HIS NAME. THE ONE WHO BROUGHT HIM LEFT SILVER AND RODE ON.]]"
role giver giver
role home site home
role camp site camp near
role victim person male at home
role tinker person [[male|female]] at home
role kingdom kingdom home
var paid 0
slot t1 -> waking betrayal deceit rival world
slot t2 -> farewell mercy identity return

stage start
  talk giver
  say "ROBBED AND BEATEN ON THE ROAD BY {CAMP}. THE PRIEST WENT BY ON THE FAR SIDE. A SERGEANT OF {KINGDOM} WENT BY ON THE FAR SIDE. A TINKER FROM OVER THE BORDER, {TINKER}, CARRIED HIM HERE ON A MULE AND PAID FOR TWO NIGHTS. <<doubt>>"
  opt "HOW IS HE?" -> how
  opt "WHERE ARE THE ROBBERS?" -> robbers
  opt "NOT MY AFFAIR." -> refused

stage refused
  end fail
  say "NO. THAT'S WHAT THE PRIEST SAID TOO. <<insult>>"
  journal "YOU PASSED BY ON THE FAR SIDE, LIKE THE OTHERS."
  do remember giver "THE TINKER CAME BACK FOR HIM, YOU KNOW. A FOREIGNER. NOT ONE OF US. NOT YOU."
  do mark passed_by

stage how
  talk giver
  say "FEVERISH. RIBS BROKEN. TWO NIGHTS' SILVER DOESN'T BUY A WEEK'S MENDING, AND I'M NOT A RICH HOUSE. {TINKER} SAID {TINKER.HE}'D PAY THE REST ON THE WAY BACK. I BELIEVE IT. I SHOULDN'T, BUT I DO."
  opt "I'LL PAY HIS KEEP TILL THEN." check gold 20 -> pay else robbers
  opt "I'LL GET HIS PURSE BACK." -> robbers

stage pay
  talk giver
  do gold -20
  do set paid 1
  say "TWENTY. FOR A MAN YOU NEVER MET. ...YOU AND THE TINKER WOULD GET ON. NEITHER OF YOU SEEMS TO KNOW WHAT'S GOOD FOR YOU. <<blessing>>"
  opt "AND THE ROBBERS?" -> robbers

stage robbers
  goal kill 4 bandit in camp
  then @t1
  journal "THE ROBBERS WHO BEAT A TRAVELLER NEAR-DEAD ON THE ROAD HIDE AT {CAMP}, {CAMP.DIR} OF {HOME}."

stage waking
  talk victim
  say "...WHERE... AN INN? WHO BROUGHT ME? I REMEMBER THE PRIEST'S FACE. HE LOOKED RIGHT AT ME. THEN HE LOOKED AT THE OTHER SIDE OF THE ROAD, AS IF SOMETHING WAS VERY INTERESTING THERE."
  opt "A TINKER FROM OVER THE BORDER." -> told
  opt "DOES IT MATTER WHO?" -> told

stage told
  talk victim
  say "FROM OVER THE BORDER? ...MY FATHER FOUGHT THEIR KIND. I'VE SPAT AT THEIR KIND IN THE MARKET. <<grief:shame>> AND A PRIEST OF MY OWN PEOPLE WALKED BY."
  opt "SO WHO WAS YOUR NEIGHBOUR?" -> @t2

stage farewell
  talk tinker
  say "BACK FOR HIM, AS I SAID. HE'S UP? GOOD. NO, DON'T THANK ME. I DID WHAT ANYBODY WOULD. ...WELL. WHAT ANYBODY SHOULD."
  opt "INTRODUCE YOURSELVES." -> neighbours_end
  ?t2 opt "HE HAS SOMETHING TO SAY TO YOU." -> neighbours_end
  opt "{TINKER.HE} WAS PAID. LET'S GO." -> cold_end

stage neighbours_end
  end success
  say "{VICTIM} TRIED TO STAND, AND COULDN'T, AND TOOK {TINKER}'S HAND INSTEAD AND DID NOT LET GO FOR A LONG TIME. NEITHER OF THEM SAID A WORD. THE INN WAS VERY QUIET."
  ?m_shame journal "{VICTIM} HAS FOUND A NEIGHBOUR IN A STRANGER FROM OVER THE BORDER, AND IS ASHAMED OF EVERY STONE HE THREW."
  !m_shame journal "{VICTIM} HAS FOUND A NEIGHBOUR IN A STRANGER FROM OVER THE BORDER. {HOME} IS STILL TALKING ABOUT IT."
  do reward fair
  do remember tinker "HE WRITES TO ME. A MAN WHO SPAT AT MY PEOPLE. LONG LETTERS. BAD SPELLING. I KEEP THEM ALL."
  do fact "A TINKER FROM OVER THE BORDER SAVED {VICTIM} ON THE ROAD BY {CAMP}, AFTER A PRIEST AND A SERGEANT PASSED HIM BY."
  do mark good_neighbour

stage cold_end
  end success
  say "{TINKER} SHRUGGED AND LOADED THE MULE. {VICTIM} WATCHED {TINKER.HIM} GO FROM THE WINDOW, AND SAID NOTHING, AND THE INNKEEPER SAID NOTHING, AND THE SILVER SAT ON THE TABLE."
  journal "THE ROBBERS OF {CAMP} ARE DEALT WITH. {VICTIM} IS MENDING. {TINKER} RODE ON WITHOUT THANKS."
  do reward small
  do remember giver "THE TINKER STOPS HERE ON EVERY CROSSING NOW. I GIVE {TINKER.HIM} THE GOOD ROOM. YOU WEREN'T KIND TO {TINKER.HIM}. I NOTICED."
  do fact "THE ROBBERS WHO HAUNTED THE ROAD BY {CAMP} ARE GONE, AND A TINKER FROM OVER THE BORDER IS WELCOME AT THE INN OF {HOME}."
)SAGA";

struct Def {
  const char* id;
  const char* name;
  uint32_t themes;
  uint32_t needs;
  uint16_t motives;
  uint32_t twists;
  const char* body;
};

uint16_t M(Motive a) { return motiveBit(a); }

}  // namespace

void addScripture(std::vector<Archetype>& v) {
  const Def defs[] = {
      {"the_pit", "THE BROTHER IN THE PIT", TH_BETRAYAL | TH_KINSHIP | TH_MERCY | TH_HOMECOMING | TH_REDEMPTION, N_TOWN,
       (uint16_t)(M(Motive::Grief) | M(Motive::Hope) | M(Motive::Love)), TF_RIVAL | TF_WONDER | TF_DECEIT | TF_MERCY | TF_PRICE, kThePit},
      {"spent_share", "THE CHILD WHO SPENT IT ALL", TH_REDEMPTION | TH_KINSHIP | TH_MERCY | TH_HOMECOMING | TH_GREED, N_TOWN,
       (uint16_t)(M(Motive::Love) | M(Motive::Hope) | M(Motive::Grief) | M(Motive::Shame)),
       TF_WORLD | TF_PRICE | TF_BETRAYAL | TF_DECEIT | TF_WONDER | TF_RIVAL | TF_RETURN | TF_MERCY, kSpentShare},
      {"sling_stone", "THE SHEPHERD AND THE CHAMPION", TH_COURAGE | TH_FAITH | TH_PRIDE | TH_WAR, N_CAMP | N_KINGDOM,
       (uint16_t)(M(Motive::Shame) | M(Motive::Duty) | M(Motive::Fear) | M(Motive::Pride)),
       TF_MERCY | TF_IDENTITY | TF_RIVAL | TF_PROPHECY | TF_PRICE | TF_WORLD | TF_RETURN, kSlingStone},
      {"whither_thou", "THE ONE WHO WOULD NOT LEAVE", TH_LOYALTY | TH_LOVE | TH_GRIEF | TH_KINSHIP | TH_HOSPITALITY, N_GRIEF | N_KINGDOM,
       (uint16_t)(M(Motive::Love) | M(Motive::Devotion) | M(Motive::Grief) | M(Motive::Duty)),
       TF_MERCY | TF_RIVAL | TF_DECEIT | TF_WORLD | TF_PROPHECY | TF_RETURN | TF_WONDER, kWhitherThou},
      {"proud_tower", "THE TOWER OF PRIDE", TH_PRIDE | TH_JUDGEMENT | TH_KINGSHIP | TH_DOOM, N_KINGDOM | N_CAPITAL,
       (uint16_t)(M(Motive::Fear) | M(Motive::Love) | M(Motive::Duty) | M(Motive::Grief)),
       TF_BETRAYAL | TF_PRICE | TF_RIVAL | TF_DECEIT | TF_PROPHECY | TF_WONDER | TF_WORLD, kProudTower},
      {"mocked_ark", "THE BOAT ON THE HILL", TH_FAITH | TH_JUDGEMENT | TH_DOOM | TH_MERCY | TH_PROPHECY, N_CAVE,
       (uint16_t)(M(Motive::Devotion) | M(Motive::Fear) | M(Motive::Hope) | M(Motive::Duty)),
       TF_PRICE | TF_WORLD | TF_BETRAYAL | TF_RIVAL | TF_WONDER | TF_PROPHECY | TF_MERCY, kMockedArk},
      {"out_of_bondage", "THE ROAD OUT OF BONDAGE", TH_FREEDOM | TH_FAITH | TH_HUNGER | TH_COURAGE | TH_KINSHIP, N_CAMP,
       (uint16_t)(M(Motive::Grief) | M(Motive::Love) | M(Motive::Vengeance) | M(Motive::Hope)),
       TF_BETRAYAL | TF_PRICE | TF_WORLD | TF_WONDER | TF_RETURN, kOutOfBondage},
      {"runaway_seer", "THE ONE WHO RAN FROM THE CALL", TH_FAITH | TH_MERCY | TH_PROPHECY | TH_VENGEANCE | TH_JUDGEMENT, N_CAVE | N_TOWN,
       (uint16_t)(M(Motive::Duty) | M(Motive::Devotion) | M(Motive::Fear) | M(Motive::Shame)),
       TF_IDENTITY | TF_MERCY | TF_PRICE | TF_BETRAYAL | TF_WONDER | TF_PROPHECY | TF_WORLD, kRunawaySeer},
      {"two_mothers", "THE DISPUTED CHILD", TH_JUDGEMENT | TH_LOVE | TH_GRIEF | TH_MERCY, N_VILLAGE,
       (uint16_t)(M(Motive::Duty) | M(Motive::Fear) | M(Motive::Grief)),
       TF_DECEIT | TF_RIVAL | TF_BETRAYAL | TF_WORLD | TF_MERCY | TF_WONDER | TF_IDENTITY | TF_PRICE, kTwoMothers},
      {"shorn_strength", "THE STRONGMAN'S SECRET", TH_BETRAYAL | TH_LOVE | TH_SACRIFICE | TH_PRIDE | TH_REDEMPTION, N_CAMP,
       (uint16_t)(M(Motive::Grief) | M(Motive::Vengeance) | M(Motive::Love) | M(Motive::Duty)),
       TF_BETRAYAL | TF_DECEIT | TF_MERCY | TF_RIVAL | TF_PRICE | TF_WONDER | TF_WORLD, kShornStrength},
      {"the_censer", "THE ONE WHO STOOD BETWEEN", TH_JUDGEMENT | TH_SACRIFICE | TH_FAITH | TH_GRIEF | TH_MERCY, N_RUIN,
       (uint16_t)(M(Motive::Fear) | M(Motive::Devotion) | M(Motive::Duty) | M(Motive::Shame)),
       TF_MERCY | TF_PRICE | TF_DECEIT | TF_BETRAYAL | TF_WONDER | TF_PROPHECY | TF_WORLD, kTheCenser},
      {"such_a_time", "THE QUEEN WHO MUST SPEAK", TH_COURAGE | TH_KINSHIP | TH_KINGSHIP | TH_PRIDE | TH_JUDGEMENT, N_KINGDOM | N_CAPITAL,
       (uint16_t)(M(Motive::Fear) | M(Motive::Pride) | M(Motive::Love) | M(Motive::Duty)),
       TF_BETRAYAL | TF_DECEIT | TF_RIVAL | TF_PRICE | TF_WONDER | TF_MERCY | TF_WORLD, kSuchATime},
      {"other_side", "THE STRANGER ON THE ROAD", TH_MERCY | TH_HOSPITALITY | TH_FRIENDSHIP | TH_JUDGEMENT, N_CAMP | N_KINGDOM,
       (uint16_t)(M(Motive::Duty) | M(Motive::Shame) | M(Motive::Hope) | M(Motive::Devotion)),
       TF_BETRAYAL | TF_DECEIT | TF_RIVAL | TF_WORLD | TF_MERCY | TF_IDENTITY | TF_RETURN, kOtherSide},
  };
  for (const Def& d : defs) {
    Archetype a;
    a.id = d.id;
    a.name = d.name;
    a.source = Source::Scripture;
    a.themes = d.themes;
    a.needs = d.needs;
    a.tier = 2;
    a.motives = d.motives;
    a.twists = d.twists;
    a.body = d.body;
    v.push_back(a);
  }
}

}  // namespace arch
}  // namespace saga
}  // namespace story
