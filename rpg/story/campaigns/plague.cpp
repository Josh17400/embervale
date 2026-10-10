// M6b "Sagas": THE PALE CHOIR (campaign `plague`): the plague cult. CAMPAIGNS lane. Themes from scripture and the world
// sims: the plague as judgement and the intercessor who stands between, the hungry year, mercy against purity.
//
// Hook: a fresh famine in a settlement (a realm event). Hunger came with the failed harvest; a fever came after it; and
// a grey-robed choir came with bread at the well, and those who eat their bread do not sicken. The town's healer does
// not believe in coincidences. The truth: the choir's prophet believes every word of the judgement he preaches, and his
// deacon brews the fever in a cistern and pours it in the wells the night before the choir arrives.
//   head      the healer of the hungry town; a real resident of it is already sick
//   arc 1     THE BREAD AT THE WELL     the loaf (what is in it) | the sick house (the choir comes for the dying)
//   arc 2     THE CHOIR'S VILLAGE       the prophet who believes | the cup of the choir (a test of faith)
//   arc 3     THE CISTERN               the vats below the hill | the singer who ran
//   arc 4     THE KNIFE IN THE DARK     the healer's bargain (a betrayal) | the soldiers of the crown (a purge ordered)
//   finale    the choir's village: the cure given freely (a market day again) | the purge (the village burned) | the
//             choir marches on to the next town (it starves; a failure) | the prophet's own judgement (the deacon slain)
// Recurring: the healer, the sick resident, the prophet, the deacon; the ruler of the land; the next town.
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title [[cult=THE PALE CHOIR|THE ASHEN CHORUS|THE CHOIR OF THE LAST HOUR]]
hook event famine
pitch "WHAT'S WRONG WITH THIS TOWN?"
hint "THE BREAD QUEUE AT THE WELL IS LONGER THAN THE ONE AT THE TEMPLE. THAT USED TO BE A JOKE HERE."
role giver giver
role home site home
role land kingdom home
role lord ruler land
role seat capital land
role healer person [[female|male]] at home
role sick resident any
role chapel site village near
role prophet person [[male|female]] at chapel
role cistern site cave near
role deacon foe [[male|female]] at cistern
role next site town near
var faith 0
var proof 0
var healer 1
var slain 0

stage healer1
  talk healer
  say "FIRST THE HARVEST FAILED. THEN THE FEVER: BLACK TONGUE, THREE DAYS, A COUGH LIKE TEARING CLOTH. THEN [[=cult]] CAME: GREY ROBES, A HYMN, BREAD AT THE WELL EVERY DAWN. WHO EATS THEIR BREAD DOESN'T SICKEN. <<doubt>>"
  journal "{HOME} IS HUNGRY, AND NOW IT IS SICK. {HEALER}, ITS HEALER, DOES NOT TRUST THE SINGERS WHO FEED IT."
  opt "WHAT DO YOU NEED?" -> healer2
  opt "MAYBE THEY'RE SIMPLY KIND." -> healer3
  opt "I'M NO PHYSICIAN." -> declined

stage healer3
  talk healer
  say "KIND. THEIR BREAD CURES A FEVER THAT CAME THE SAME WEEK THEY DID. THEIR PROPHET SAYS IT IS {HEALER.GOD}'S JUDGEMENT ON THE HOARDERS, AND THAT THE FED ARE THE FORGIVEN. I'VE BURIED NINE THIS MONTH. NONE OF THEM HOARDED ANYTHING."
  opt "WHAT DO YOU NEED?" -> healer2
  opt "I'M NO PHYSICIAN." -> declined

stage healer2
  talk healer
  say "EYES THAT AREN'T MINE. THE CHOIR KNOWS MY FACE. {SICK}, THE {SICK.JOB}, WOKE WITH THE FEVER TODAY. I CAN KEEP {SICK.HIM} BREATHING FOR A WEEK, NO MORE. FIND OUT WHAT IS IN THAT BREAD, AND WHERE THE FEVER COMES FROM."
  do remember sick "THE HEALER SAYS YOU'RE LOOKING INTO THE FEVER. I CAN'T STOP SHAKING. IS THAT THE FEVER OR THE FEAR?"
  opt "A WEEK. I'LL BE QUICK." -> @next
  opt "[[KEEP {SICK.HIM} BREATHING.|BOIL EVERYTHING. WAIT FOR ME.]]" -> @next

stage declined
  end fail
  say "{HEALER} NODS AS IF {HEALER.HE} EXPECTED NOTHING ELSE AND GOES BACK TO {HEALER.HIS} BASINS. AT DAWN THE HYMN BEGINS AT THE WELL, AND THE QUEUE IS LONGER THAN YESTERDAY."
  journal "YOU LEFT {HOME} TO ITS FEVER AND ITS SINGERS."
  do remember healer "THE ONE WHO WASN'T A PHYSICIAN. WE'RE DOWN TO HALF THE STREET, IF YOU WONDERED."
)SAGA";

const char* const kFinale = R"SAGA(
stage judgement_road
  goal goto chapel
  then judgement
  do moves prophet chapel
  say "THE HYMN CARRIES ON THE WIND LONG BEFORE THE ROOFS COME IN SIGHT. IT IS A BEAUTIFUL HYMN. THAT IS THE WORST OF IT."
  journal "EVERYTHING ENDS AT {CHAPEL}, WHERE [[=cult]] SINGS. {PROPHET} WILL HEAR WHAT YOU KNOW, ONE WAY OR ANOTHER."

stage judgement
  talk prophet
  ?arc_pl_cistern say "YOU SAY {DEACON} BREWS THE FEVER, AND THAT I HAVE BLESSED A POISONER'S BREAD FOR A YEAR. ...YOUR VIAL STINKS OF THE CISTERN UNDER OUR HILL. I KNOW THAT SMELL. {PROPHET.GOD} FORGIVE ME, I KNOW IT. WHAT WOULD YOU HAVE ME DO?"
  ?arc_pl_ran_singer say "YOU SAY OUR HYMN IS A LIE, AND {DEACON} A POISONER. A JAR WITH MY OWN SEAL ON IT, FULL OF... I HAVE SUNG OVER A THOUSAND FEVERS AND SEEN THEM BREAK AT DAWN. TELL ME WHAT I BLESSED, {PLAYER}. ALL OF IT."
  opt "GIVE THE CURE AWAY. FREELY." check var proof 1 -> cured else unbelieved
  opt "{DEACON} IS DEAD BY MY HAND." if var slain 1 -> reckoning
  opt "JUDGE {DEACON} YOURSELF." if novar slain 1 -> deacon_hunt
  opt "THE CROWN WILL END THIS." -> purge_road
  opt "SING ON. I WAS WRONG." -> choir_wins

stage unbelieved
  talk prophet
  say "WORDS. YOU BRING ME WORDS, AND A FEVER BRINGS ME CORPSES. ...AND YET. {HEALER} SENT YOU, AND {HEALER} HAS NEVER LIED TO ME, NOT EVEN WHEN WE WERE CHILDREN IN {HOME} AND IT WOULD HAVE BEEN KIND. SHOW ME THE CISTERN."
  opt "COME. SEE THE VATS YOURSELF." if novar slain 1 -> deacon_hunt
  opt "COME. SEE {DEACON}'S BODY." if var slain 1 -> reckoning
  opt "THEN THE CROWN WILL SHOW YOU." -> purge_road

stage deacon_hunt
  goal slay deacon
  then reckoning
  do moves prophet cistern
  say "{PROPHET} WALKS BEHIND YOU DOWN INTO THE DARK, SINGING UNDER {PROPHET.HIS} BREATH. THE HYMN STOPS AT THE FIRST VAT."
  journal "{DEACON}, WHO BREWED THE FEVER, WAITS IN {CISTERN}. {PROPHET} HAS SEEN THE VATS. END IT."

stage reckoning
  talk prophet
  say "IT IS DONE. {DEACON} KNELT AT MY SERMONS, YOU KNOW. {DEACON.HE} CRIED. ...I WILL TAKE THE CHOIR TO {HOME} AND TO {NEXT}, AND WE WILL GIVE THE CURE AWAY AT EVERY WELL WE POISONED, AND WE WILL TELL THEM WHY, AND THEY MAY STONE US. THAT IS FAIR."
  opt "LET THEM DECIDE WHAT'S FAIR." -> cured
  opt "NO. DISBAND. GO HOME." -> disbanded

stage cured
  end success
  say "THE CURE WENT OUT FROM EVERY WELL IN A WEEK. {SICK} WALKED TO THE MARKET ON {SICK.HIS} OWN FEET, THIN AS A RAKE, AND BOUGHT A TURNIP JUST TO HOLD IT. {HEALER} WEPT IN THE STREET AND DID NOT CARE WHO SAW."
  journal "THE FEVER IS BROKEN IN {HOME}, AND THE CURE GIVEN FREELY. [[=cult]] SINGS NO MORE OF JUDGEMENT."
  do realm event festival land home
  do befriend sick
  do reward great
  do fame 3
  do mark plague_cured
  do remember healer "THE FIRST MARKET DAY SINCE THE FEVER. I BOUGHT NOTHING. I JUST STOOD THERE AND LISTENED TO PEOPLE HAGGLE."
  do remember sick "I WAS THREE DAYS FROM THE GROUND. NOW I CAN'T STOP EATING. SIT. THERE'S BREAD. IT'S JUST BREAD, I PROMISE."
  do fact "A FEVER WAS BREWED AND POURED IN THE WELLS OF {HOME}, AND {PROPHET} OF [[=cult]] GAVE THE CURE AWAY FOR NOTHING."

stage disbanded
  end success
  say "[[=cult]] PUT AWAY ITS GREY ROBES. {PROPHET} WENT EAST ALONE, BAREFOOT, TO DO WHATEVER PENANCE {PROPHET.HE} COULD FIND. THE CURE WENT OUT THROUGH {HEALER}'S HANDS INSTEAD, SLOWER, AND WITH NOBODY SINGING."
  journal "THE FEVER IS BROKEN IN {HOME}, [[=cult]] IS DISBANDED, AND {PROPHET} HAS GONE INTO THE EAST ALONE."
  do realm event festival land home
  do befriend sick
  do reward rich
  do mark plague_cured
  do remember healer "I MISS THE HYMN, IF YOU CAN BELIEVE IT. NOT THE WORDS. THE TUNE."
  do fact "[[=cult]] IS NO MORE. THEY SAY ITS PROPHET WALKS BAREFOOT FROM WELL TO WELL, ASKING FORGIVENESS."

stage purge_road
  goal goto seat
  then purge
  journal "TAKE WHAT YOU KNOW OF [[=cult]] TO {LORD} AT {SEAT}. THE CROWN HAS SOLDIERS, AND NO PATIENCE FOR CULTS."

stage purge
  talk lord
  say "A POISONER'S CULT, ON MY ROADS, IN A HUNGRY YEAR? GOOD. YOU DID WELL TO BRING IT TO THE CROWN. MY SOLDIERS RIDE FOR {CHAPEL} TONIGHT. THEY KNOW WHAT TO DO WITH CULTS. DON'T WATCH IF YOU'VE A SOFT STOMACH."
  opt "SPARE THOSE WHO ONLY SANG." -> purge_end
  opt "BURN IT ALL." -> purge_end

stage purge_end
  end success
  say "THE SMOKE FROM {CHAPEL} WAS SEEN IN {HOME}. THE FEVER STOPPED THAT WEEK, BECAUSE NOBODY POURED IT ANY MORE. {HEALER} DID NOT SPEAK TO YOU AGAIN, AND {SICK} LIVED, AND THAT IS ALL OF IT."
  journal "{CHAPEL} WAS BURNED BY THE CROWN WITH [[=cult]] INSIDE IT. THE FEVER IS OVER IN {HOME}."
  do realm event townburned land chapel
  do rep land 6
  do reward rich
  do mark plague_purged
  do remember healer "THEY SANG UNTIL THE ROOF FELL, THEY SAY. I WANTED THE FEVER STOPPED. I DIDN'T WANT THAT."
  do fact "THE {LORD.TITLE}'S SOLDIERS BURNED {CHAPEL} AND THE SINGERS OF [[=cult]] IN IT. THE FEVER ENDED THAT SAME WEEK."

stage choir_wins
  end fail
  say "{PROPHET} EMBRACED YOU AND CALLED YOU BROTHER IN THE HYMN. THE CHOIR TOOK THE ROAD TO {NEXT} AT DAWN, AND THE NIGHT BEFORE THEY ARRIVED, SOMEONE POURED SOMETHING INTO {NEXT}'S WELLS."
  journal "[[=cult]] MARCHES ON {NEXT}, AND THE FEVER WALKS AHEAD OF IT. YOU LET IT GO."
  do realm famine next
  do mark plague_choir
  do remember healer "THEY'RE SINGING IN {NEXT} NOW. YOU KNOW WHAT THAT MEANS. YOU KNEW WHAT IT MEANT THEN, TOO."
  do fact "THE FEVER FOLLOWED [[=cult]] FROM {HOME} TO {NEXT}, WHERE THEY ARE EATING THE SEED CORN NOW."
)SAGA";

// ---- arc 1: the bread at the well
const char* const kLoaf = R"SAGA(
stage %dawn
  goal wait 1
  then %well
  say "BEFORE DAWN THE GREY ROBES COME UP FROM THE ROAD WITH BASKETS ON THEIR HEADS, SINGING. THE QUEUE IS ALREADY WAITING."
  journal "WATCH [[=cult]] AT THE WELL OF {HOME} AT DAWN. GET ONE OF THEIR LOAVES."
stage %well
  talk healer
  say "HERE. A LOAF, STILL WARM. I CUT IT OPEN AND LOOK: WILLOW-BARK, FEVERFEW, SOMETHING BITTER I DON'T KNOW. IT'S A GOOD CURE, {PLAYER}. A BETTER ONE THAN MINE. THEY KNEW THE FEVER BEFORE IT CAME. HOW?"
  opt "THEY KNEW WHAT TO COOK FOR IT." -> %ledger
  opt "EAT IT. TEST IT ON ME." -> %eaten
stage %eaten
  talk healer
  say "YOU'RE EITHER BRAVE OR A FOOL. ...WELL? NOTHING? NO, WAIT. YOUR TONGUE. IT'S GONE GREY, NOT BLACK. THE BREAD DOESN'T STOP THE FEVER: IT MAKES YOU THE CHOIR'S. EVERYONE WHO EATS IT GOES GREY-TONGUED. LIKE A BRAND."
  do add faith 1
  opt "THEN THEY'RE MARKING THEIR FLOCK." -> %ledger
stage %ledger
  talk sick
  say "THEY ASKED MY NAME AT THE WELL. WROTE IT IN A BOOK. ASKED WHO ELSE LIVES IN MY HOUSE, AND IF ANY OF US HAD EVER SPOKEN AGAINST THE HYMN. I SAID NO. I DIDN'T WANT TO BE HUNGRY ANY MORE. WAS THAT WRONG?"
  opt "NO. HUNGER ISN'T A SIN." -> @next
  opt "WHO ELSE DID THEY WRITE DOWN?" -> @next
)SAGA";

const char* const kSickHouse = R"SAGA(
stage %nursing
  talk healer
  say "THE SICK HOUSE. TWELVE BEDS, NINETEEN SICK. HOLD {SICK}'S HEAD WHILE I GET THE BROTH DOWN. ...LISTEN. HEAR IT? THE HYMN. THEY COME AT DUSK FOR THE ONES WHO WON'T LAST THE NIGHT, TO SING THEM HOME, THEY SAY. THEY'VE COME FOR {SICK}."
  journal "IN THE SICK HOUSE OF {HOME}, [[=cult]] HAS COME AT DUSK FOR THE DYING. FOR {SICK}."
  do moves prophet home
  opt "NOBODY TAKES {SICK.HIM}." -> %door
  opt "LET THEM SING. MAYBE IT HELPS." -> %taken
stage %door
  talk prophet
  say "PEACE, FRIEND. WE TAKE THE DYING TO {CHAPEL}, WHERE THE AIR IS CLEAN AND THE HYMN NEVER STOPS. NONE WHO GO THERE DIE CURSING. YOU WOULD KEEP {SICK.HIM} HERE, IN THIS STINK, TO SPITE ME?"
  opt "{SICK.HE} STAYS WHERE I CAN SEE." -> %stay
  opt "FINE. BUT I'M COMING WITH YOU." -> %taken
stage %stay
  talk healer
  say "{PROPHET} LEFT WITHOUT A WORD. {PROPHET.HE} LOOKED SORRY FOR US. ...{SICK} IS STILL BREATHING. HOLD THE CUP, I'LL HOLD {SICK.HIM}. IF WE GET {SICK.HIM} TO MORNING, I'LL BELIEVE IN SOMETHING AGAIN."
  do set healer 1
  do moves prophet chapel
  do remember sick "YOU WOULDN'T LET THEM TAKE ME. I HEARD YOU. I HEARD EVERYTHING, EVEN WHEN I COULDN'T OPEN MY EYES."
  opt "WE'LL GET {SICK.HIM} TO MORNING." -> @next
stage %taken
  talk healer
  say "THEN GO WITH THEM, AND WATCH. NOBODY HAS EVER COME BACK FROM {CHAPEL} TO TELL ME WHAT HAPPENS THERE. {SICK.HE} WAS MY NEIGHBOUR TWENTY YEARS. <<grief>>"
  do moves sick chapel
  do moves prophet chapel
  do add faith 1
  opt "I'LL BRING {SICK.HIM} BACK." -> @next
)SAGA";

// ---- arc 2: the choir's village
const char* const kTrueProphet = R"SAGA(
stage %road
  goal goto chapel
  then %sermon
  journal "[[=cult]] LIVES AT {CHAPEL}, {CHAPEL.DIR}. GO AND HEAR THEIR PROPHET, {PROPHET}."
stage %sermon
  talk prophet
  say "YOU CAME TO SPY. EVERYONE DOES, AT FIRST. LOOK, THEN. LOOK AT MY HOUSE OF THE DYING: CLEAN STRAW, FRESH WATER, NO ONE ALONE. THE HUNGRY YEAR IS {PROPHET.GOD}'S JUDGEMENT, AND WE ARE THE MERCY THAT COMES AFTER. WHICH PART OFFENDS YOU?"
  opt "WHERE DOES YOUR BREAD COME FROM?" -> %bread
  opt "WHO TOLD YOU THE FEVER WAS COMING?" -> %warning
stage %bread
  talk prophet
  say "FROM {DEACON}, MY DEACON, WHO KEEPS OUR STORES IN THE OLD CISTERN UNDER THE HILL. HARD, BUT FAITHFUL. {DEACON.HE} WAS A BAKER BEFORE THE HYMN FOUND {DEACON.HIM}. {DEACON.HE} WEEPS AT EVERY FUNERAL. YOU'D LIKE {DEACON.HIM}."
  opt "I'D LIKE TO MEET HIM." -> %invite
stage %warning
  talk prophet
  say "TOLD ME? THE HYMN TOLD ME. A VISION, {PLAYER}: A TOWN, A WELL, A BLACK TONGUE. I WAKE, AND {DEACON} IS ALREADY BAKING, AND WE WALK. WE HAVE NEVER ARRIVED TOO LATE. NOT ONCE. ISN'T THAT A MIRACLE?"
  do add proof 1
  opt "...IT'S SOMETHING." -> %invite
stage %invite
  talk prophet
  say "STAY A WHILE. CARRY BREAD WITH US TO {NEXT} WHEN WE GO. YOU HAVE A KIND FACE UNDER ALL THAT SUSPICION. THE HYMN WOULD SUIT YOU. <<blessing:devotion>>"
  opt "I'LL THINK ON IT." -> @next
  opt "I'LL CARRY BREAD." -> %joined
stage %joined
  talk prophet
  say "THEN WELCOME. WEAR GREY TOMORROW. WE WALK FOR {NEXT} WHEN THE VISION COMES."
  do add faith 1
  opt "[[TOMORROW, THEN.|I'LL BE READY.]]" -> @next
)SAGA";

const char* const kChoirCup = R"SAGA(
role %singer person [[female|male]] at chapel
stage %road
  goal goto chapel
  then %cup
  journal "[[=cult]] LIVES AT {CHAPEL}, {CHAPEL.DIR}. THEY LET ANY STRANGER IN. FEW LEAVE THE SAME."
stage %cup
  talk %singer
  say "NEWCOMERS DRINK FROM THE CUP. THAT'S ALL. ONE SIP, AND THE HYMN KNOWS YOU, AND YOU'RE FAMILY. I DRANK THREE WINTERS AGO, THE YEAR THE FLOOD TOOK MY HOUSE. I HAVEN'T BEEN HUNGRY SINCE. DRINK, AND {PROPHET} WILL SEE YOU."
  opt "I'LL DRINK." -> %drunk
  opt "WHAT'S IN IT?" -> %what
  opt "I'LL NOT DRINK." -> %refuse
stage %what
  talk %singer
  say "WHAT'S IN... WATER. FROM THE CISTERN. AND THE BITTER HERB THAT MAKES YOUR TONGUE GREY, SO WE KNOW OUR OWN. {DEACON} MIXES IT. NO ONE ELSE IS ALLOWED DOWN THERE. NO ONE ASKS WHY, EITHER. ...WHY ARE YOU LOOKING AT ME LIKE THAT?"
  do add proof 1
  opt "HAVE YOU EVER BEEN DOWN THERE?" -> %refuse
stage %drunk
  talk %singer
  say "THERE. YOUR TONGUE'S GREY ALREADY. WELCOME HOME. ...YOU'LL DREAM TONIGHT. EVERYONE DOES, THE FIRST TIME. A WELL, AND A FACE YOU KNOW AT THE BOTTOM OF IT. DON'T BE AFRAID. IT'S ONLY THE HYMN LEARNING YOUR NAME."
  do add faith 1
  do remember %singer "WE DRANK FROM THE SAME CUP. WHATEVER YOU THINK OF US NOW, THAT WAS REAL."
  opt "WHOSE FACE?" -> @next
  opt "I'M NOT AFRAID." -> @next
stage %refuse
  talk %singer
  say "THEN GO. QUICKLY, AND DON'T TELL ANYONE I... {DEACON} IS WATCHING FROM THE CISTERN DOOR. {DEACON.HE} DOESN'T LIKE PEOPLE WHO ASK. THE LAST ONE WHO ASKED WENT DOWN THE HILL WITH A BLACK TONGUE. <<warning>>"
  do remember %singer "YOU DIDN'T DRINK. I THINK ABOUT THAT. I THINK ABOUT IT A LOT."
  opt "COME WITH ME." -> @next
  opt "I'LL BE BACK FOR YOU." -> @next
)SAGA";

// ---- arc 3: the cistern
const char* const kCistern = R"SAGA(
stage %descend
  goal enter cistern
  then %vats
  do moves healer cistern
  journal "{DEACON} KEEPS THE CHOIR'S STORES IN {CISTERN}, AND LETS NO ONE DOWN. GO DOWN."
stage %vats
  talk healer
  say "{HEALER} CAME AFTER YOU, OF COURSE. ...LOOK AT THESE VATS. BLIGHTED RYE, RAT-MEAT, WELL-WATER BROUGHT FROM {HOME} IN SEALED JARS. IT'S THE FEVER. THEY BREW IT. THEY POUR IT, AND THE NEXT DAY THEY ARRIVE SINGING WITH THE CURE."
  do give "A VIAL OF THE GREY WATER" scroll
  do set proof 1
  opt "WE TAKE THIS TO {PROPHET}." -> %leave
  opt "WE WAIT FOR {DEACON}." -> %ambush
stage %ambush
  goal slay deacon
  then %after
  say "THE CISTERN DOOR CREAKS. {DEACON} COMES DOWN THE STEPS WITH A LANTERN AND A JAR, HUMMING THE HYMN."
  journal "{DEACON}, WHO BREWS THE FEVER, IS COMING DOWN INTO {CISTERN}. BE WAITING."
stage %after
  talk healer
  say "{DEACON.HE} BEGGED. DID YOU HEAR? NOT FOR {DEACON.HIS} LIFE: FOR THE CHOIR. {DEACON.HE} SAID {PROPHET} DOESN'T KNOW. {DEACON.HE} SAID SOMEONE HAS TO MAKE THE MIRACLES, OR THE FAITHFUL STOP COMING. ...I BELIEVE {DEACON.HIM}. THAT'S THE WORST PART."
  do add proof 1
  do set slain 1
  opt "THEN {PROPHET} HAS TO HEAR IT." -> @next
stage %leave
  goal wait 1
  then @next
  say "YOU CLIMB OUT INTO THE DARK. THE HYMN IS STILL GOING IN {CHAPEL}, SOFT AND SWEET, AND {HEALER} WILL NOT STOP SHIVERING."
  journal "YOU HAVE A VIAL OF THE GREY WATER FROM THE VATS UNDER {CISTERN}. IT IS PROOF, IF {PROPHET} WILL SEE IT."
)SAGA";

const char* const kRanSinger = R"SAGA(
role %runaway person [[male|female]] at next
stage %word
  talk healer
  say "SOMEONE KNOCKED AT MIDNIGHT. A GREY ROBE, A TONGUE AS GREY AS ASH, RUN FROM THE CHOIR AND TOO FRIGHTENED TO STAY. THEY'RE GOING TO {NEXT} NEXT, IT SAID, AND SOMEONE ALWAYS GOES AHEAD OF THEM. BY NIGHT. TO THE WELLS."
  journal "A RUNAWAY OF [[=cult]] SAYS SOMEONE GOES AHEAD OF THE CHOIR TO {NEXT}, BY NIGHT, TO THE WELLS."
  opt "THEN I'LL BE AT {NEXT}'S WELLS." -> %next
stage %next
  goal goto next
  then %meet
  journal "GET TO {NEXT} BEFORE [[=cult]] DOES, AND WATCH ITS WELLS BY NIGHT."
stage %meet
  talk %runaway
  say "YOU'RE THE ONE {HEALER} SENT. I'M {%runaway}. I CARRIED THE JARS FOR {DEACON}, TWO YEARS. I THOUGHT THEY WERE HOLY WATER. THEN I SMELLED ONE. THEY'RE POURING TONIGHT, THE WELL BY THE TANNERS'. THERE'LL BE THREE OF THEM."
  opt "THEN WE STOP THEM." -> %stop
  opt "WE FOLLOW THEM HOME." -> %follow
stage %stop
  goal kill 3 bandit
  then %caught
  say "MIDNIGHT, THE TANNERS' WELL. THREE SHAPES IN GREY, WITH JARS."
  journal "STOP THE POURERS AT THE TANNERS' WELL OF {NEXT}. THEY WILL NOT COME QUIETLY."
stage %caught
  talk %runaway
  say "THE JARS. LOOK, THEY'RE SEALED WITH THE CHOIR'S MARK. {PROPHET} BLESSES EVERY SEAL WITH {PROPHET.HIS} OWN HAND AT DAWN, AND NEVER ASKS WHAT'S INSIDE. ...I WANT TO TELL {PROPHET.HIM}. I WANT TO SEE {PROPHET.HIS} FACE."
  do set proof 1
  do give "A SEALED JAR OF THE CHOIR'S" scroll
  do remember %runaway "WE STOPPED THEM AT THE TANNERS' WELL. NOBODY IN {NEXT} KNOWS HOW CLOSE IT CAME."
  opt "YOU WILL. COME WITH ME." -> @next
stage %follow
  goal wait 1
  then @next
  say "THE POURERS DO THEIR WORK AND SLIP AWAY UP THE ROAD TO {CHAPEL}. BY MORNING HALF THE TANNERS' STREET HAS BLACK TONGUES."
  do add faith 1
  journal "YOU WATCHED THE POURERS WORK AT {NEXT}, AND FOLLOWED THEM BACK TO {CHAPEL}. {NEXT} WILL PAY FOR WHAT YOU LEARNED."
)SAGA";

// ---- arc 4: the knife in the dark
const char* const kHealerBargain = R"SAGA(
stage %back
  goal goto home
  then %night
  do moves healer home
  journal "{HEALER} HAS SENT WORD: COME BACK TO {HOME}, ALONE, AFTER DARK. {HEALER.HE} HAS SOMETHING TO CONFESS."
stage %night
  talk healer
  say "SIT DOWN. I HAVE TO TELL YOU SOMETHING BEFORE YOU HEAR IT ELSEWHERE. THE CROWN PAYS FOR NAMES. CULTISTS, HOARDERS. I'VE BEEN SENDING THEM NAMES FROM THE CHOIR'S BOOK SINCE THE FIRST DEATH. SOME OF THEM WERE ONLY HUNGRY."
  journal "{HEALER} HAS BEEN SELLING NAMES TO THE {LORD.TITLE}'S MEN. SOME OF THE NAMED WERE ONLY HUNGRY."
  opt "WHY?" -> %why
  opt "YOU'RE NO BETTER THAN {DEACON}." -> %cold
stage %why
  talk healer
  say "BECAUSE THE CROWN PAYS IN GRAIN, AND MY SICK HOUSE EATS. BECAUSE I HATED THEM FOR BEING RIGHT ABOUT THE CURE WHEN I WASN'T. ...{SICK}'S NAME IS IN THAT BOOK, {PLAYER}. I KNOW BECAUSE I PUT IT THERE, AND I CAN'T TAKE IT BACK OUT."
  opt "THEN WE GET {SICK.HIM} CLEAR." -> %mend
  opt "YOU'LL ANSWER FOR IT, AFTER." -> %cold
stage %mend
  talk healer
  say "YES. YES. I'LL TAKE {SICK.HIM} TO MY SISTER'S FARM TONIGHT. AND WHEN THIS IS DONE I'LL STAND IN THE SQUARE AND SAY WHAT I DID, AND {HOME} CAN DO WHAT IT LIKES WITH ME. <<apology>>"
  do set healer 1
  do remember healer "I TOLD THE SQUARE WHAT I DID. THEY LET ME KEEP HEALING. I DON'T KNOW IF THAT'S MERCY OR NEED."
  opt "GO. QUICKLY." -> @next
stage %cold
  talk healer
  say "...NO. I'M NOT. I'LL KEEP DOING THE WORK, BECAUSE THE WORK DOESN'T CARE WHAT I AM. BUT I WON'T ASK YOU TO TRUST ME AGAIN. ONLY TO FINISH THIS."
  do set healer 0
  do remember healer "YOU WERE RIGHT ABOUT ME. I STILL DON'T LIKE YOU FOR IT."
  opt "I'LL FINISH IT." -> @next
)SAGA";

const char* const kCrownSoldiers = R"SAGA(
stage %summons
  goal goto home
  then %orders
  do moves healer home
  journal "SOLDIERS OF THE {LORD.TITLE} HAVE COME TO {HOME}. THEIR CAPTAIN IS ASKING FOR YOU BY NAME."
stage %orders
  talk healer
  say "THEY CAME AT NOON. A CAPTAIN WITH A WRIT IN {LORD}'S OWN HAND: [[=cult]] IS TO BE PUT DOWN, AND THEY WANT A GUIDE TO {CHAPEL} WHO KNOWS THE BACK PATHS. YOU. I TOLD THEM ABOUT YOU. I'M SORRY."
  opt "WHAT DOES THE WRIT SAY, EXACTLY?" -> %writ
  opt "I WON'T GUIDE BUTCHERS." -> %refused
stage %writ
  talk healer
  say "PUT DOWN. THAT'S ALL IT SAYS. NOT THE PROPHET, NOT THE DEACON, NOT THE GUILTY. [[=cult]]. THERE ARE CHILDREN IN {CHAPEL}, {PLAYER}. THERE ARE PEOPLE FROM THIS TOWN WHO ONLY WANTED BREAD."
  opt "THEN I GET THERE FIRST." -> %refused
  opt "I'LL GUIDE THEM. AND STAY THEIR HANDS." -> %guide
stage %guide
  goal wait 1
  then @next
  do rep land 2
  say "THE SOLDIERS CAMP OUTSIDE {HOME}, SHARPENING. THE CAPTAIN SAYS THEY MARCH WHEN YOU SAY. SHE SAYS IT LIKE SHE MEANS SOONER."
  journal "THE {LORD.TITLE}'S SOLDIERS WAIT FOR YOU TO LEAD THEM TO {CHAPEL}. THE WRIT SAYS PUT DOWN. NOTHING ELSE."
stage %refused
  goal wait 1
  then @next
  do rep land -3
  say "THE CAPTAIN SPITS AND SAYS THEY'LL FIND THE WAY THEMSELVES, IN A WEEK OR SO. A WEEK. THAT IS HOW LONG YOU HAVE."
  journal "THE {LORD.TITLE}'S SOLDIERS WILL FIND {CHAPEL} WITHOUT YOU WITHIN A WEEK. GET THERE FIRST."
)SAGA";

}  // namespace

void addPlagueArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::Scripture;
  a.tier = 3;
  a.needs = N_FAMINE | N_KINGDOM | N_TOWN;

  a.id = "pl_loaf"; a.name = "THE LOAF FROM THE WELL";
  a.themes = TH_HUNGER | TH_FAITH | TH_TRICKERY; a.body = kLoaf; v.push_back(a);
  a.id = "pl_sick_house"; a.name = "THE SICK HOUSE AT DUSK";
  a.themes = TH_MERCY | TH_GRIEF | TH_FAITH; a.body = kSickHouse; v.push_back(a);
  a.id = "pl_true_prophet"; a.name = "THE PROPHET WHO BELIEVES";
  a.themes = TH_FAITH | TH_PROPHECY | TH_JUDGEMENT; a.body = kTrueProphet; v.push_back(a);
  a.id = "pl_choir_cup"; a.name = "THE CUP OF THE CHOIR";
  a.themes = TH_FAITH | TH_TEMPTATION | TH_HOSPITALITY; a.body = kChoirCup; v.push_back(a);
  a.source = Source::World;
  a.id = "pl_cistern"; a.name = "THE VATS BELOW THE HILL";
  a.themes = TH_JUDGEMENT | TH_TRICKERY | TH_COURAGE; a.body = kCistern; v.push_back(a);
  a.id = "pl_ran_singer"; a.name = "THE SINGER WHO RAN";
  a.themes = TH_REDEMPTION | TH_COURAGE | TH_FREEDOM; a.body = kRanSinger; v.push_back(a);
  a.id = "pl_healer_bargain"; a.name = "THE HEALER'S BARGAIN";
  a.themes = TH_BETRAYAL | TH_GREED | TH_REDEMPTION; a.body = kHealerBargain; v.push_back(a);
  a.id = "pl_crown_soldiers"; a.name = "THE WRIT OF THE CROWN";
  a.themes = TH_JUDGEMENT | TH_MERCY | TH_WAR; a.body = kCrownSoldiers; v.push_back(a);
}

CampaignPlan plaguePlan() {
  CampaignPlan p;
  p.id = "plague";
  p.name = "THE PALE CHOIR";
  p.source = Source::Scripture;
  p.themes = TH_JUDGEMENT | TH_MERCY | TH_FAITH | TH_HUNGER | TH_BETRAYAL;
  p.needs = N_FAMINE | N_KINGDOM | N_CAPITAL | N_TOWN | N_EVENT;
  p.head = kHead;
  p.arcs = {ArcSlot{{"pl_loaf", "pl_sick_house"}}, ArcSlot{{"pl_true_prophet", "pl_choir_cup"}},
            ArcSlot{{"pl_cistern", "pl_ran_singer"}}, ArcSlot{{"pl_healer_bargain", "pl_crown_soldiers"}}};
  p.finale = kFinale;
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
