// M8 Fellowship: the companions' personal quests (VISION_PLAN 9 "personal quests come from the hook", 15.26; owner M8
// notes: generated through the M6b saga composer, cast from the companion's real history). VOICE lane. The template
// markup and role conventions: rpg/story/saga.h; the companion hook and its stake roles: rpg/story/dsl.h.
//
// Every archetype is `hook companion <stake> [<fact>]`: the giver is the companion (their own voice, first person),
// `home` their home settlement, and roles marked `stake` are bound from their history (party::Stake): the kin of the
// census, the named foe, the kingdom, the ruin, the place it happened. Each has a moral choice whose consequences last:
// the companion's approval and bond (`approve`, `bond`), their fate (`fate home|part`), the world's memory (`fact`,
// `remember`), and the deeds bus (`deed`) that every companion with the player weighs by their own nature.
//
//   ashes_answered   THE ASHES ANSWERED          burned home: the raiders' camp and their chief, who yields
//   ashes_rebuilt    THE ROOF RAISED AGAIN       burned home: timber, hands and the house to rebuild, and who it is for
//   kin_refuge       THE FACE AMONG THE REFUGEES lost kin (alive): found in a far town with a new life of their own
//   kin_taken        THE ONES THEY CARRIED OFF   lost kin (alive): held at a bandit camp; brought out, they may come along
//   kin_grave        WHAT BECAME OF THEM         lost kin (dead): the truth of how they died, and whether to tell it
//   old_company      THE LAST OF THE COMPANY     war (veteran): the captain who left the company to die
//   levy_roll        THE NAME ON THE ROLL        war (deserter): the sergeant who comes with the deserters' roll
//   beast_reckoning  THE BEAST ON THE HOME ROAD  revenge: the named beast that killed their kin
//   chief_reckoning  THE FACE I NEVER FORGOT     revenge: the bandit chief, grown old
//   heirloom_ruin    WHAT THE RUIN KEPT          heirloom: the family's treasure in the ruin, and its other claimant
//   debt_called      THE DEBT CALLED IN          debt: the moneylender's collectors, and the moneylender's ledger
//   exile_return     THE ROAD BACK               exile: a pardon begged at the court that cast them out
//   pilgrim_vow      THE VOW ON THE ROAD         faith: a vow to a far shrine, and a dying man on the way
//   song_renown      A NAME IN THE SONGS         ambition: a deed of renown, and who the song is about
//   sweetheart       THE ONE WHO WAITED          love: the sweetheart in another town, and what they want now
#include <vector>
#include "rpg/story/arch/arch_tables.h"

namespace story {
namespace saga {
namespace arch {

namespace {

const char* const kAshesAnswered = R"SAGA(
title [[THE ASHES ANSWERED|SMOKE OVER THE VILLAGE|THE RAIDERS' CAMP]]
hook companion burned
pitch "[[YOU'VE BEEN QUIET SINCE THE LAST TOWN.|SOMETHING IS ON YOUR MIND.|YOU KEEP LOOKING AT THE SMOKE.]]"
hint "I'VE NEVER TOLD YOU ALL OF IT, HAVE I? ABOUT THE NIGHT THE RAIDERS CAME. I THINK I'M READY TO, IF YOU'LL HEAR IT."
role giver giver
role home site home
role burned site village near stake
role camp site camp near
role chief person [[male|female]] at camp
slot t1 -> camp_go world wonder price
slot t2 -> chief_talk mercy

stage start
  talk giver
  say "RAIDERS BURNED {BURNED}. I WAS THERE, AND I STILL SMELL THE SMOKE IN MY SLEEP. THE SAME BAND IS CAMPED AT {CAMP} NOW, {CAMP.DIR} OF HERE, AND THEIR CHIEF IS CALLED {CHIEF}. I WANT TO LOOK THAT FACE IN THE EYE. WILL YOU COME WITH ME?"
  opt "THEN WE GO TOGETHER." -> @t1
  opt "VENGEANCE WON'T MEND IT." -> doubt
  opt "LET IT LIE." -> refused

stage doubt
  talk giver
  say "NO, IT WON'T MEND ANYTHING. BUT THEY'LL DO IT AGAIN TO SOMEONE ELSE, AND I'LL HAVE KNOWN WHERE THEY WERE AND SAID NOTHING. <<vow:vengeance>>"
  opt "ALL RIGHT. WE'LL GO." -> @t1
  opt "I SAID LET IT LIE." -> refused

stage refused
  end fail
  say "THEN I'LL CARRY IT A WHILE LONGER. I'VE CARRIED IT THIS FAR. <<farewell>>"
  journal "{GIVER} ASKED YOU TO FACE THE RAIDERS WHO BURNED {BURNED}. YOU WOULD NOT."
  do approve -10
  do remember giver "I ASKED YOU ONCE ABOUT {CAMP}. I WON'T ASK AGAIN."

stage camp_go
  goal kill 4 bandit in camp
  then @t2
  journal "THE RAIDERS WHO BURNED {BURNED} ARE CAMPED AT {CAMP}, {CAMP.DIR}. BREAK THEM, WITH {GIVER} AT YOUR SIDE."

stage chief_talk
  talk chief
  say "ENOUGH! I YIELD! YES, MY BAND BURNED {BURNED}. THE LORD'S MEN BURNED OUR OWN FARMS THE WINTER BEFORE, AND WE WERE STARVING. THAT'S NO EXCUSE, AND I KNOW IT ISN'T. DO WHAT YOU CAME TO DO."
  opt "{GIVER}, IT'S YOUR CHOICE." -> giver_choice
  opt "SWEAR NEVER TO RAID AGAIN, AND GO." -> spared
  opt "YOU'LL ANSWER TO {HOME} FOR IT." -> taken

stage giver_choice
  talk giver
  say "I'VE DREAMED OF THIS FOR YEARS. AND NOW THERE'S ONLY A HUNGRY, FRIGHTENED FOOL IN THE MUD. I DON'T KNOW WHAT I WANT ANY MORE. YOU DECIDE, AND I'LL ABIDE BY IT."
  opt "LET THEM LIVE." -> spared
  opt "END IT." -> killed

stage spared
  end success
  say "{CHIEF} SWORE ON A HANDFUL OF ASH FROM THE FIRE AND WALKED AWAY INTO THE HILLS. {GIVER} WATCHED UNTIL THE LAST OF THEM WAS GONE, AND THAT NIGHT, FOR THE FIRST TIME IN YEARS, SLEPT UNTIL MORNING."
  journal "YOU BROKE THE RAIDERS AT {CAMP} AND SPARED {CHIEF}, WHO SWORE NEVER TO RAID AGAIN."
  do deed mercy
  do approve 4
  do bond 10
  do hide chief
  do reward fair
  do fact "THE RAIDERS WHO BURNED {BURNED} WERE BROKEN AT {CAMP} BY {GIVER} AND A TRAVELLER. THEIR CHIEF WAS SPARED."

stage killed
  end success
  say "IT WAS QUICK. {GIVER} STOOD OVER THE BODY A LONG TIME WITHOUT A WORD, THEN WIPED THE BLADE ON THE GRASS AND TURNED BACK TO THE ROAD."
  journal "YOU BROKE THE RAIDERS AT {CAMP}, AND {CHIEF}, WHO BURNED {BURNED}, DIED THERE ON THEIR KNEES."
  do deed cruelty
  do approve 8
  do bond 6
  do hide chief
  do reward fair
  do fact "THE CHIEF OF THE RAIDERS WHO BURNED {BURNED} DIED AT {CAMP}, ON THEIR KNEES."

stage taken
  goal goto home
  then judged
  journal "{CHIEF} YIELDED AT {CAMP}. BRING THEM TO {HOME} TO ANSWER FOR {BURNED}."

stage judged
  end success
  say "THE ELDERS OF {HOME} HEARD {CHIEF} OUT, AND THE SENTENCE WAS HARD LABOUR UNTIL {BURNED} WAS BUILT AGAIN. {GIVER} BROUGHT THEM WATER ON THE FIRST DAY, AND DID NOT SAY WHY."
  journal "{CHIEF} ANSWERED TO {HOME} FOR BURNING {BURNED}: HARD LABOUR UNTIL IT IS BUILT AGAIN."
  do deed bandits
  do approve 6
  do bond 8
  do hide chief
  do reward fair
  do fact "THE RAIDER CHIEF WHO BURNED {BURNED} WAS JUDGED IN {HOME} AND SET TO REBUILD WHAT THEY BURNED."
)SAGA";

const char* const kAshesRebuilt = R"SAGA(
title [[THE ROOF RAISED AGAIN|NEW TIMBER, OLD ASHES|WHAT THE FIRE LEFT]]
hook companion burned
pitch "[[YOU KEEP TALKING ABOUT HOME.|WHAT WOULD YOU DO, IF YOU COULD GO BACK?|YOU'VE BEEN DRAWING HOUSES IN THE DIRT.]]"
hint "I KEEP THINKING ABOUT THE HOUSES THAT BURNED. NOT THE RAIDERS. THE HOUSES. SOMEONE OUGHT TO BUILD THEM AGAIN, AND I THINK IT SHOULD BE ME."
role giver giver
role home site home
role burned site village near stake
role mill site town near
role timber npc merchant at mill
role widow person female at burned
var paid 0
slot t1 -> back_go world price wonder

stage start
  talk giver
  say "HALF OF {BURNED} IS STILL BLACK TIMBER AND NETTLES. I COULD RAISE A ROOF OR TWO, IF I HAD SEASONED WOOD. THE MERCHANT IN {MILL} SELLS IT, {MILL.DIR} OF HERE, [[BUT NOT CHEAP|AT A PRICE|FOR A FAIR SUM]]. WILL YOU HELP ME DO IT?"
  opt "WE'LL BUY THE TIMBER TOGETHER." -> mill_go
  opt "BUILD IT WITH YOUR OWN HANDS?" -> hands
  opt "THE ROAD IS OUR HOME NOW." -> refused

stage hands
  talk giver
  say "WITH MY OWN HANDS, AND WITH ANYONE ELSE WHO'LL LIFT A BEAM. I'M NO CARPENTER, BUT I KNOW WHICH END OF A HAMMER GOES DOWN. I'D RATHER BE BUILDING SOMETHING THAN ONLY EVER BREAKING THINGS. <<proverb>>"
  opt "THEN LET'S FETCH THE WOOD." -> mill_go
  opt "NOT NOW." -> refused

stage refused
  end fail
  say "MAYBE YOU'RE RIGHT. MAYBE THERE'S NOTHING THERE TO GO BACK TO. <<farewell>>"
  journal "{GIVER} WANTED TO REBUILD THE BURNED HOUSES OF {BURNED}. YOU TOLD THEM THE ROAD WAS HOME NOW."
  do approve -6
  do remember giver "THE NETTLES WILL HAVE THE WHOLE STREET BY NOW."

stage mill_go
  goal goto mill
  then timber_talk
  journal "{GIVER} MEANS TO REBUILD {BURNED}. SEASONED TIMBER CAN BE BOUGHT FROM A MERCHANT IN {MILL}, {MILL.DIR}."

stage timber_talk
  talk timber
  say "TIMBER FOR {BURNED}? I REMEMBER THE FIRE. THE SMOKE CAME ALL THE WAY HERE. FORTY SILVER FOR A CARTLOAD, AND I'LL SEND IT ON MY OWN CART. OR WORK A DAY IN MY YARD AND HAVE IT FOR HALF."
  opt "HERE'S FORTY SILVER." check gold 40 -> bought else short
  opt "WE'LL WORK THE DAY." -> work

stage short
  talk timber
  say "THAT'S NOT FORTY. THAT'S NOT TWENTY. A DAY IN THE YARD, THEN, AND I'LL CALL IT FAIR."
  opt "A DAY IN THE YARD, THEN." -> work

stage bought
  talk timber
  say "DONE. THE CART GOES AT DAWN. TELL THEM IN {BURNED} TO STACK IT OFF THE GROUND, OR IT'LL WARP."
  do gold -40
  do set paid 1
  opt "WE'LL MEET IT THERE." -> @t1

stage work
  goal wait 1
  then @t1
  journal "A DAY HAULING LOGS IN A TIMBER YARD IN {MILL}, FOR A CARTLOAD OF SEASONED WOOD FOR {BURNED}."

stage back_go
  goal goto burned
  then widow_talk
  journal "THE TIMBER IS ON ITS WAY TO {BURNED}. MEET IT THERE WITH {GIVER}."

stage widow_talk
  talk widow
  say "YOU'RE THE ONES WITH THE TIMBER? I LOST MY HUSBAND AND MY HOUSE THE SAME NIGHT. I'VE SLEPT IN THE BYRE SINCE. I WON'T BEG. BUT IF ONE ROOF IS GOING UP, I'D ASK YOU TO THINK OF WHO SLEEPS UNDER IT."
  opt "THE FIRST ROOF IS YOURS." -> widow_end
  opt "{GIVER}, IT'S YOUR FAMILY'S HOUSE." -> giver_house

stage giver_house
  talk giver
  say "IT WAS MY FAMILY'S HOUSE. BUT NOBODY OF MINE WILL SLEEP IN IT AGAIN, AND SHE'S BEEN SLEEPING WITH THE COWS. I THINK I KNOW WHAT MY MOTHER WOULD SAY."
  opt "THEN SAY IT." -> widow_end
  opt "STAY AND LIVE IN IT YOURSELF." -> stay_end

stage widow_end
  end success
  say "THE ROOF WENT UP IN FOUR DAYS, WITH HALF OF {BURNED} PULLING ON THE ROPES. {WIDOW} CRIED WHEN THE LAST SHINGLE WENT ON, AND THEN MADE EVERYONE SOUP. IT WAS TERRIBLE SOUP. NOBODY SAID SO."
  journal "THE FIRST NEW ROOF IN {BURNED} SHELTERS {WIDOW}, WHO LOST EVERYTHING IN THE FIRE."
  do deed charity
  do approve 10
  do bond 10
  do reward fair
  do remember widow "THE ROOF DOESN'T LEAK. I KEEP WAITING FOR IT TO LEAK. IT DOESN'T."
  do fact "THE FIRST HOUSE REBUILT IN {BURNED} AFTER THE FIRE WAS RAISED BY {GIVER} AND A TRAVELLER, FOR A WIDOW."

stage stay_end
  end success
  say "I'LL STAY A WHILE, THEN. SOMEONE HAS TO BUILD THE NEXT ONE, AND THE ONE AFTER THAT. COME AND FIND ME WHEN THE ROAD NEEDS ME. I'LL KNOW YOUR KNOCK."
  journal "{GIVER} STAYED IN {BURNED} TO REBUILD IT, HOUSE BY HOUSE. THEY WILL COME WHEN YOU ASK."
  do approve 12
  do bond 6
  do reward small
  do fate home
  do fact "{GIVER} CAME HOME TO {BURNED} AFTER THE FIRE AND STARTED BUILDING IT AGAIN, ONE ROOF AT A TIME."
)SAGA";

const char* const kKinRefuge = R"SAGA(
title [[THE FACE AMONG THE REFUGEES|NEWS FROM THE CAMPS|THE ONE WHO WALKED AWAY]]
hook companion lostkin kinalive
pitch "[[YOU WATCH EVERY STRANGER'S FACE.|WHO ARE YOU LOOKING FOR?|YOU ASKED THAT MERCHANT SOMETHING.]]"
hint "A TRADER AT THE LAST INN SAID SOMETHING THAT HASN'T LEFT MY HEAD SINCE. I THINK I KNOW WHERE ONE OF MY OWN HAS GONE."
role giver giver
role home site home
role refuge site town far
role kin person [[male|female]] at refuge stake
slot t1 -> refuge_go world wonder price
slot t2 -> kin_talk mercy

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} WENT MISSING, AND I'VE ASKED EVERY TRAVELLER SINCE. A TRADER SAYS SOMEONE WHO FITS THE TELLING IS LIVING AMONG THE REFUGEES IN {REFUGE}, A LONG WAY {REFUGE.DIR}. IT'S FAR, AND IT MAY NOT BE {KIN.HIM}. I HAVE TO KNOW."
  opt "THEN WE GO TO {REFUGE}." -> @t1
  opt "WHAT IF IT ISN'T THEM?" -> doubt
  opt "IT'S TOO FAR." -> refused

stage doubt
  talk giver
  say "THEN I'LL HAVE LOOKED, AND I'LL STOP WONDERING EVERY NIGHT. BUT IF IT IS {KIN.HIM}, AND I DIDN'T GO, I COULDN'T LIVE WITH IT. <<proverb>>"
  opt "WE'LL GO AND SEE." -> @t1
  opt "I'M SORRY. NOT NOW." -> refused

stage refused
  end fail
  say "I UNDERSTAND. IT'S A LONG ROAD FOR A RUMOUR. <<farewell>>"
  journal "{GIVER} HEARD THAT {KIN} MIGHT BE ALIVE IN {REFUGE}. YOU DID NOT GO."
  do approve -8
  do remember giver "I STILL THINK ABOUT {REFUGE}. EVERY NIGHT."

stage refuge_go
  goal goto refuge
  then @t2
  journal "{GIVER}'S {KIN.KIN} {KIN} MAY BE LIVING AMONG THE REFUGEES IN {REFUGE}, FAR {REFUGE.DIR}. ASK AT THE DYE VATS BY THE WATER."

stage kin_talk
  talk kin
  say "...{GIVER}? IT CAN'T BE. I THOUGHT EVERYONE AT HOME WAS DEAD OR GONE. I HAVE A CHILD NOW, AND WORK, AND A ROOF. I CAN'T GO BACK TO THAT PLACE. PLEASE DON'T ASK ME TO."
  opt "COME HOME. YOUR FAMILY NEEDS YOU." -> pressed
  opt "THEN STAY. WE'RE GLAD YOU LIVE." -> let_be
  opt "{GIVER}, SAY SOMETHING." -> giver_says

stage giver_says
  talk giver
  say "I CAME ALL THIS WAY TO BRING YOU HOME, {KIN}. BUT LOOK AT YOU. YOU'RE ALIVE, AND YOU'RE NOT AFRAID ANY MORE. MAYBE THAT'S ENOUGH FOR ME."
  opt "IT'S ENOUGH." -> let_be
  opt "NO. THEY SHOULD COME HOME." -> pressed

stage pressed
  end success
  say "{KIN} PACKED IN SILENCE AND CAME, WITH THE CHILD ON {KIN.HIS} HIP. THE WHOLE LONG ROAD HOME {KIN.HE} SAID NOTHING, AND IN {HOME} {KIN.HE} STILL SAYS LITTLE. BUT {KIN.HE} IS HOME, AND {GIVER} SLEEPS LIKE A STONE."
  journal "YOU BROUGHT {KIN} HOME TO {HOME} FROM {REFUGE}, THOUGH {KIN.HE} DID NOT WANT TO COME."
  do deed cruelty
  do approve 6
  do bond 4
  do hide kin
  do reward fair
  do fact "{KIN}, LONG THOUGHT LOST, CAME HOME TO {HOME} FROM THE REFUGEE STREETS OF {REFUGE}, UNWILLING."

stage let_be
  end success
  say "YOU STAYED THREE DAYS. {GIVER} HELD THE CHILD AND LEARNED ITS NAME AND SANG IT THE OLD SONGS FROM HOME. WHEN WE LEFT, {KIN} WALKED WITH YOU BOTH TO THE EDGE OF TOWN AND SAID, COME BACK. {GIVER} PROMISED."
  journal "YOU FOUND {KIN} ALIVE IN {REFUGE}, WITH A CHILD AND A NEW LIFE, AND LEFT {KIN.HIM} TO LIVE IT."
  do deed mercy
  do approve 10
  do bond 12
  do hide kin
  do reward small
  do remember giver "I HAVE A NIECE. OR A NEPHEW. I'M NOT SURE IT MATTERS. THEY LAUGHED AT ME."
  do fact "{GIVER} OF {HOME} FOUND {KIN}, LONG LOST, ALIVE AMONG THE REFUGEES OF {REFUGE}, AND LEFT THEM IN PEACE."
)SAGA";

const char* const kKinTaken = R"SAGA(
title [[THE ONES THEY CARRIED OFF|THE CAGES AT THE CAMP|CHAINS IN THE HILLS]]
hook companion lostkin kinalive
pitch "[[YOU FLINCH WHEN YOU HEAR CHAINS.|WHAT DID THAT BANDIT SAY TO YOU?|YOU'VE SHARPENED THAT BLADE ALL NIGHT.]]"
hint "THE LAST BANDIT WE PUT DOWN WAS WEARING A RING I KNOW. I KNOW IT, BECAUSE I MADE IT. IT WAS A GIFT FOR SOMEONE OF MY BLOOD."
role giver giver
role home site home
role camp site camp near
role kin person [[male|female]] at camp stake
slot t1 -> camp_go world price
slot t2 -> free_talk mercy

stage start
  talk giver
  say "THAT RING BELONGED TO MY {KIN.KIN} {KIN}, WHO WAS TAKEN ON THE ROAD. BANDITS CARRY PEOPLE OFF TO SELL, AND THE ONES WHO WORE THAT RING CAMP AT {CAMP}, {CAMP.DIR} OF HERE. IF {KIN.HE}'S ALIVE, {KIN.HE}'S THERE. I'M GOING, WITH YOU OR WITHOUT YOU."
  opt "WITH ME. WE GO NOW." -> @t1
  opt "WAIT. WE NEED A PLAN." -> plan
  opt "IT'S A TRAP, AND I WON'T." -> refused

stage plan
  talk giver
  say "A PLAN? FINE. THEY KEEP THE CAPTIVES IN THE MIDDLE, WHERE THE GUARDS CAN SEE THEM. WE HIT THEM HARD, WE CUT THE ROPES, WE RUN. THAT'S MY PLAN. DO YOU HAVE A BETTER ONE? <<proverb>>"
  opt "NO. THAT ONE WILL DO." -> @t1
  opt "NO. AND I WON'T GO." -> refused

stage refused
  end fail
  say "THEN I'LL GO ALONE, ONE DAY. DON'T TRY TO STOP ME WHEN I DO. <<farewell>>"
  journal "{GIVER} BELIEVES {KIN} IS HELD CAPTIVE AT {CAMP}. YOU WOULD NOT GO."
  do approve -12
  do remember giver "EVERY NIGHT I THINK ABOUT THE ROPES AT {CAMP}."

stage camp_go
  goal kill 4 bandit in camp
  then @t2
  journal "{GIVER}'S {KIN.KIN} {KIN} MAY BE HELD CAPTIVE BY THE BANDITS AT {CAMP}, {CAMP.DIR}. BREAK THEM AND CUT THE ROPES."

stage free_talk
  talk kin
  say "{GIVER}! YOU CAME! I KNEW SOMEONE WOULD, I KEPT TELLING MYSELF. THEY WERE GOING TO SELL ME AT THE FAIR. ...LET ME COME WITH YOU. I CAN'T GO HOME AND SIT BY THE FIRE AFTER THIS. I'D RATHER FIGHT."
  opt "THEN COME WITH US." -> joined
  opt "GO HOME. YOU'VE SUFFERED ENOUGH." -> sent_home

stage joined
  end success
  say "THEN I'M WITH YOU. I'LL TRY NOT TO SLOW YOU DOWN. AND I'LL TRY NOT TO SHAKE WHEN I HEAR A CHAIN."
  journal "YOU BROKE THE BANDITS AT {CAMP} AND FREED {KIN}, WHO CHOSE TO TRAVEL WITH YOU."
  do deed rescued
  do recruit kin rescued
  do approve 12
  do bond 15
  do reward fair
  do fact "THE BANDITS OF {CAMP} WERE BROKEN AND THEIR CAPTIVES FREED. {KIN} OF {HOME} WAS AMONG THEM."

stage sent_home
  end success
  say "{KIN} WENT HOME TO {HOME} ON A CARTER'S WAGON, WRAPPED IN {GIVER}'S CLOAK. {GIVER} STOOD IN THE ROAD WAVING LONG AFTER THE WAGON WAS GONE, AND THEN CAME BACK GRINNING LIKE A FOOL."
  journal "YOU BROKE THE BANDITS AT {CAMP}, FREED {KIN} AND SENT {KIN.HIM} HOME TO {HOME}."
  do deed rescued
  do approve 10
  do bond 12
  do hide kin
  do reward fair
  do fact "THE BANDITS OF {CAMP} WERE BROKEN AND THEIR CAPTIVES FREED. {KIN} WENT HOME TO {HOME}."
)SAGA";

const char* const kKinGrave = R"SAGA(
title [[WHAT BECAME OF THEM|THE LAST ROAD THEY WALKED|THE PRIEST WHO BURIED THEM]]
hook companion lostkin kindead
pitch "[[YOU NEVER SAY HOW THEY DIED.|WHO WAS IT YOU LOST?|YOU WENT QUIET AT THAT GRAVE.]]"
hint "NOBODY EVER TOLD MY FAMILY HOW IT HAPPENED. WE HAD A BODY AND NO STORY. I'VE HEARD OF SOMEONE WHO MIGHT KNOW, AND I CAN'T STOP THINKING ABOUT IT."
role giver giver
role home site home
role where site village near
role kin person [[male|female]] at where stake
role priest npc priest at where
slot t1 -> where_go wonder world

stage start
  talk giver
  say "MY {KIN.KIN} {KIN} DIED ON THE ROAD, NEAR {WHERE}, {WHERE.DIR} OF HERE. THEY BROUGHT {KIN.HIM} HOME TO {HOME}, BUT NOBODY EVER SAID HOW IT HAPPENED. THE PRIEST AT {WHERE} SAID THE LAST RITES. THE PRIEST WOULD KNOW."
  opt "THEN WE ASK THE PRIEST." -> @t1
  opt "SOME THINGS ARE BETTER NOT KNOWN." -> doubt
  opt "LET THE DEAD REST." -> refused

stage doubt
  talk giver
  say "MAYBE. BUT NOT KNOWING HAS A WEIGHT TOO, AND I'VE CARRIED IT LONG ENOUGH. WHATEVER THE TRUTH IS, I'D RATHER HOLD IT THAN WONDER. <<proverb>>"
  opt "THEN WE'LL FIND OUT." -> @t1
  opt "I WON'T DIG UP THE PAST." -> refused

stage refused
  end fail
  say "ALL RIGHT. THE DEAD WILL KEEP. THEY'RE GOOD AT THAT. <<farewell>>"
  journal "{GIVER} WANTED TO LEARN HOW {KIN} DIED NEAR {WHERE}. YOU LEFT THE PAST ALONE."
  do approve -6
  do remember giver "I DREAMED ABOUT {WHERE} AGAIN."

stage where_go
  goal goto where
  then priest_talk
  journal "{GIVER}'S {KIN.KIN} {KIN} DIED ON THE ROAD NEAR {WHERE}, {WHERE.DIR}. THE PRIEST THERE SAID THE RITES AND MAY KNOW HOW."

stage priest_talk
  talk priest
  say "{KIN}? I REMEMBER. {KIN.HE} DIED STEALING FROM A MERCHANT'S CART, AND THE GUARD'S ARROW WAS QUICK. {KIN.HE} HAD BREAD IN BOTH HANDS. THE FAMILY WAS TOLD IT WAS A FEVER. I THOUGHT IT KINDER. WAS I WRONG?"
  opt "{GIVER} DESERVES THE TRUTH." -> truth
  opt "IT WAS A FEVER. LEAVE IT THERE." -> lie

stage truth
  talk giver
  say "BREAD. {KIN.HE} DIED FOR BREAD, WHILE WE WERE HUNGRY AT HOME. {KIN.HE} WAS STEALING IT FOR US. ...I'M GLAD I KNOW. IT HURTS, BUT IT'S THE TRUTH, AND IT'S {KIN.HIS} OWN."
  opt "WHAT WILL YOU TELL YOUR FAMILY?" -> told

stage told
  end success
  say "THE TRUTH. ALL OF IT. THAT {KIN} WAS HUNGRY AND BRAVE AND FOOLISH, AND THAT {KIN.HE} DIED THINKING OF US. THANK YOU FOR COMING ALL THIS WAY WITH ME. I MEAN IT."
  journal "YOU LEARNED HOW {KIN} DIED NEAR {WHERE}: STEALING BREAD FOR {KIN.HIS} FAMILY. {GIVER} WILL TELL THEM."
  do approve 10
  do bond 12
  do reward small
  do fact "{KIN} OF {HOME} DIED NEAR {WHERE} STEALING BREAD FOR A HUNGRY FAMILY. THE FAMILY KNOWS NOW."

stage lie
  end success
  say "A FEVER, THEN. {GIVER} LISTENED TO THE PRIEST'S KIND STORY, NODDED, AND THANKED HIM, AND NEVER LEARNED WHAT YOU KNOW. SOME NIGHTS THAT SITS HEAVY ON YOU."
  journal "THE PRIEST AT {WHERE} TOLD {GIVER} THAT {KIN} DIED OF A FEVER. YOU KNOW BETTER, AND SAID NOTHING."
  do deed lied
  do approve 4
  do reward small
  do remember giver "A FEVER. AT LEAST IT WAS QUICK, THE PRIEST SAID. I'M GLAD OF THAT."
)SAGA";

const char* const kOldCompany = R"SAGA(
title [[THE LAST OF THE COMPANY|THE CAPTAIN'S HOUSE|WHO SOUNDED THE RETREAT]]
hook companion war veteran
pitch "[[YOU STILL MARCH IN YOUR SLEEP.|WHAT HAPPENED IN THE WAR?|YOU STARED AT THAT BANNER.]]"
hint "THE MAN WHO LED MY COMPANY INTO THE LAST WAR IS LIVING IN COMFORT NOW. I HEARD IT AT THE INN. FORTY OF US WENT IN WITH HIM. SIX CAME OUT."
role giver giver
role home site home
role land kingdom home stake
role seat site town near
role captain person [[male|female]] at seat
slot t1 -> seat_go world price wonder
slot t2 -> captain_talk mercy

stage start
  talk giver
  say "I MARCHED FOR {LAND} UNDER A CAPTAIN CALLED {CAPTAIN}. AT THE FORD, {CAPTAIN.HE} SOUNDED THE RETREAT AND RODE AWAY, AND LEFT FORTY OF US TO HOLD THE BANK. SIX OF US LIVED. {CAPTAIN.HE} LIVES IN {SEAT} NOW, {SEAT.DIR}, ON A PENSION. I WANT TO ASK {CAPTAIN.HIM} WHY."
  opt "THEN WE'LL ASK TOGETHER." -> @t1
  opt "AND IF THE ANSWER IS POOR?" -> doubt
  opt "THE WAR IS OVER." -> refused

stage doubt
  talk giver
  say "THEN I'LL KNOW WHAT KIND OF MAN I FOLLOWED. I WON'T DRAW STEEL IN A TOWN STREET, IF THAT'S YOUR FEAR. I ONLY WANT TO HEAR IT SAID TO MY FACE. <<proverb>>"
  opt "ALL RIGHT. WE GO." -> @t1
  opt "LET IT REST." -> refused

stage refused
  end fail
  say "OVER FOR YOU, MAYBE. NOT FOR THE THIRTY-FOUR WHO STAYED AT THE FORD. <<farewell>>"
  journal "{GIVER} WANTED TO FACE {CAPTAIN}, WHO LEFT {GIVER.HIS} COMPANY TO DIE. YOU WOULD NOT GO."
  do approve -8
  do remember giver "THIRTY-FOUR NAMES. I SAY THEM SOMETIMES WHEN I CAN'T SLEEP."

stage seat_go
  goal goto seat
  then @t2
  journal "{CAPTAIN}, WHO LEFT {GIVER}'S COMPANY TO DIE AT THE FORD, LIVES IN {SEAT}, {SEAT.DIR}."

stage captain_talk
  talk captain
  say "I KNOW YOUR FACE. THE FORD. ...I HAD ORDERS. THE LORD'S HOST WAS FALLING BACK, AND SOMEONE HAD TO HOLD THE CROSSING OR WE ALL DIED. I CHOSE YOU. I'VE SEEN YOUR FACES EVERY NIGHT SINCE. DOES THAT HELP?"
  opt "{GIVER}, IS IT ENOUGH?" -> giver_hears
  opt "YOU'LL SAY IT IN THE SQUARE." -> shamed

stage giver_hears
  talk giver
  say "ORDERS. SO WE WERE A WALL, NOT MEN. ...I THINK I KNEW THAT, IN THE WATER, WITH THE ARROWS COMING. I JUST NEEDED TO HEAR IT. I'M DONE HERE, I THINK."
  opt "THEN WE GO, AND LEAVE IT." -> forgiven
  opt "NO. THE TOWN SHOULD KNOW." -> shamed

stage forgiven
  end success
  say "{GIVER} TURNED AND WALKED OUT OF {SEAT} WITHOUT LOOKING BACK. THAT NIGHT {GIVER} SAID THE THIRTY-FOUR NAMES ALOUD BY THE FIRE, ONE BY ONE, AND THEN SLEPT, AND DID NOT MARCH IN {GIVER.HIS} SLEEP."
  journal "{CAPTAIN} TOLD {GIVER} THE TRUTH OF THE FORD. {GIVER} HEARD IT AND LET IT GO."
  do deed mercy
  do approve 10
  do bond 12
  do hide captain
  do reward small
  do fact "{CAPTAIN} OF {SEAT}, WHO HELD THE FORD WITH FORTY MEN AND LOST THIRTY-FOUR, WAS VISITED BY ONE OF THE SIX."

stage shamed
  end success
  say "{CAPTAIN} STOOD IN THE SQUARE OF {SEAT} AND TOLD THE STORY OF THE FORD TO ANYONE WHO WOULD LISTEN. THE PENSION STOPPED THAT WINTER. {GIVER} WAS NOT SURE, AFTERWARDS, WHETHER IT HAD FELT LIKE JUSTICE."
  journal "{CAPTAIN} CONFESSED THE TRUTH OF THE FORD IN THE SQUARE OF {SEAT}, AND LOST THE LORD'S PENSION."
  do deed cruelty
  do approve 6
  do bond 6
  do hide captain
  do reward small
  do fact "{CAPTAIN} OF {SEAT} CONFESSED IN THE SQUARE THAT THIRTY-FOUR SOLDIERS WERE LEFT TO DIE AT A FORD ON ORDERS."
)SAGA";

const char* const kLevyRoll = R"SAGA(
title [[THE NAME ON THE ROLL|THE DESERTERS' LIST|THE SERGEANT'S BOOK]]
hook companion war deserter
pitch "[[WHY DID YOU HIDE FROM THOSE SOLDIERS?|YOU WENT PALE WHEN THAT HORN BLEW.|WHAT ARE YOU RUNNING FROM?]]"
hint "THERE'S A SERGEANT ASKING AFTER ME ON THE ROAD. I HEARD MY NAME AT THE WELL. I THINK IT'S TIME I TOLD YOU WHY."
role giver giver
role home site home
role land kingdom home stake
role sergeant person [[male|female]] at home
role clerk npc guard at home

stage start
  talk giver
  say "I RAN FROM THE LEVY OF {LAND}. THE NIGHT BEFORE THE BATTLE I PUT DOWN MY SPEAR AND WALKED INTO THE DARK, AND I'VE NEVER BEEN BACK. A SERGEANT CALLED {SERGEANT} HAS COME TO {HOME} WITH THE ROLL OF DESERTERS, AND MY NAME IS ON IT."
  opt "THEN WE'LL TALK TO THE SERGEANT." -> home_go
  opt "WHY DID YOU RUN?" -> why
  opt "THEN HAND YOURSELF IN." -> hand_in

stage why
  talk giver
  say "BECAUSE I WAS SIXTEEN AND SO FRIGHTENED I COULDN'T HOLD THE SPEAR STILL. BECAUSE THE LORD WE FOUGHT FOR HAD BURNED OUR NEIGHBOURS' FIELDS THE YEAR BEFORE. CHOOSE WHICHEVER REASON YOU LIKE. THEY'RE BOTH TRUE. <<proverb>>"
  opt "THEN WE'LL TALK TO THE SERGEANT." -> home_go
  opt "YOU SHOULD STILL ANSWER FOR IT." -> hand_in

stage hand_in
  end success
  say "YOU'RE RIGHT. I'VE RUN LONG ENOUGH. I'LL GIVE MYSELF UP TO {SERGEANT} AND SERVE WHAT THEY GIVE ME. DON'T COME LOOKING FOR ME FOR A WHILE. AND THANK YOU. I MEAN THAT, EVEN NOW."
  journal "{GIVER} GAVE {GIVER.HIM}SELF UP TO THE LEVY OF {LAND} TO ANSWER FOR DESERTING."
  do approve -4
  do bond 4
  do fate home
  do fact "{GIVER} OF {HOME}, WHO RAN FROM THE LEVY OF {LAND}, CAME BACK AND GAVE {GIVER.HIM}SELF UP."

stage home_go
  goal goto home
  then sergeant_talk
  journal "A SERGEANT OF {LAND} HAS COME TO {HOME} WITH THE ROLL OF DESERTERS, AND {GIVER}'S NAME IS ON IT."

stage sergeant_talk
  talk sergeant
  say "THE ROLL SAYS {GIVER} OF {HOME}, DESERTED BEFORE THE BATTLE. THE PUNISHMENT IS THE LASH AND FIVE YEARS ON THE BORDER FORTS. UNLESS SOMEONE SPEAKS FOR {GIVER.HIM}, OR SOMEONE MAKES IT WORTH MY WHILE TO LOSE A PAGE."
  opt "I SPEAK FOR {GIVER.HIM}." check fame 2 -> spoken else refused_speech
  opt "HOW MUCH FOR THE PAGE?" -> bribe
  opt "LET {GIVER.HIM} SERVE THE TIME." -> served

stage refused_speech
  talk sergeant
  say "AND WHO ARE YOU? NOBODY I'VE HEARD OF. YOUR WORD WON'T MOVE A CLERK, FRIEND. IT'S THE PAGE OR THE FORTS."
  opt "HOW MUCH FOR THE PAGE?" -> bribe
  opt "THEN THE FORTS." -> served

stage bribe
  talk sergeant
  say "THIRTY SILVER, AND THE PAGE GOES IN THE FIRE, AND I NEVER CAME TO {HOME}."
  opt "HERE. BURN IT." check gold 30 -> bribed else served_poor
  opt "NO. I WON'T BUY IT." -> served

stage served_poor
  talk sergeant
  say "THAT'S NOT THIRTY. THEN IT'S THE FORTS, AND I'M SORRY FOR IT."
  opt "SO BE IT." -> served

stage spoken
  end success
  say "{SERGEANT} LOOKED AT YOU A LONG TIME, THEN STRUCK THE LINE THROUGH {GIVER}'S NAME AND WROTE BESIDE IT: SERVED SINCE, WITH HONOUR. {GIVER} DIDN'T SPEAK FOR AN HOUR AFTERWARDS, AND THEN COULDN'T STOP."
  journal "YOU SPOKE FOR {GIVER}, AND THE SERGEANT OF {LAND} STRUCK {GIVER.HIS} NAME FROM THE ROLL OF DESERTERS."
  do approve 14
  do bond 14
  do hide sergeant
  do reward small
  do fact "{GIVER} OF {HOME} WAS STRUCK FROM THE LEVY'S ROLL OF DESERTERS ON THE WORD OF A TRAVELLER."

stage bribed
  end success
  say "THE PAGE CURLED IN THE INN'S FIRE AND {SERGEANT} RODE OUT BEFORE DAWN. {GIVER} IS FREE. BUT {GIVER} KNOWS WHAT IT COST, AND HOW, AND LOOKS AT YOU A LITTLE DIFFERENTLY NOW."
  journal "YOU BRIBED THE SERGEANT OF {LAND}, AND {GIVER}'S NAME WENT INTO THE FIRE WITH THE PAGE."
  do gold -30
  do deed bribed
  do approve 8
  do bond 8
  do hide sergeant
  do reward small

stage served
  end success
  say "{GIVER} WENT WITH {SERGEANT} TO SERVE OUT THE SENTENCE IN THE NORTH. AT THE EDGE OF {HOME}, {GIVER} LOOKED BACK ONCE, AS IF HOPING YOU WOULD SAY SOMETHING ELSE."
  journal "{GIVER} WAS TAKEN BY THE SERGEANT OF {LAND} TO SERVE OUT A DESERTER'S SENTENCE."
  do approve -20
  do fate home
  do hide sergeant
  do fact "{GIVER} OF {HOME} WAS TAKEN FOR DESERTING THE LEVY OF {LAND} AND SENT TO THE BORDER FORTS."
)SAGA";

const char* const kBeastReckoning = R"SAGA(
title [[THE BEAST ON THE HOME ROAD|WHAT KILLED MY FATHER|THE TRACKS I KNOW]]
hook companion revenge
pitch "[[YOU KNOW THAT BEAST, DON'T YOU?|WHAT NAME DID THEY HAVE FOR IT?|WHAT DID THE HUNTERS SAY?]]"
hint "THE HUNTERS AT THE INN WERE TALKING ABOUT A BEAST. THEY HAD A NAME FOR IT. I KNOW THAT NAME, AND I'VE HATED IT EVERY DAY SINCE."
role giver giver
role home site home
role foe beast near stake
slot t1 -> hunt world wonder price
slot t2 -> den_talk mercy

stage start
  talk giver
  say "{FOE}, THE WORST OF THE {FOE.KIND} {FOE.DIR} OF HERE, KILLED MY FATHER ON THE HOME ROAD. I SAW WHAT WAS LEFT. THE HUNTERS SAY IT'S STILL OUT THERE, [[OLD AND CUNNING|SCARRED AND CUNNING|OLDER NOW, AND CLEVERER]]. I WANT IT DEAD, AND I WANT TO BE THERE WHEN IT DIES."
  opt "THEN WE HUNT IT." -> @t1
  opt "IT'S ONLY A BEAST." -> doubt
  opt "TOO DANGEROUS. NO." -> refused

stage doubt
  talk giver
  say "ONLY A BEAST. I KNOW. IT DOESN'T HATE ME BACK. BUT IT HAS KILLED SINCE, AND IT WILL KILL AGAIN, AND I'M THE ONE WHO KNOWS HOW IT HUNTS. WILL YOU COME? <<proverb>>"
  opt "I'LL COME." -> @t1
  opt "NO. LET THE HUNTERS DEAL WITH IT." -> refused

stage refused
  end fail
  say "THE HUNTERS HAVE BEEN DEALING WITH IT FOR TWENTY YEARS. FINE. <<farewell>>"
  journal "{GIVER} WANTED TO HUNT {FOE}, WHICH KILLED {GIVER.HIS} FATHER. YOU WOULD NOT."
  do approve -8
  do remember giver "I HEARD IT HOWL LAST NIGHT. OR I DREAMED IT."

stage hunt
  goal slay foe
  then @t2
  journal "{FOE}, WHICH KILLED {GIVER}'S FATHER, STILL HUNTS {FOE.DIR}. HUNT IT DOWN, TOGETHER."

stage den_talk
  talk giver
  say "IT'S DEAD. IT'S REALLY DEAD. I THOUGHT I'D FEEL SOMETHING BIGGER. ...THERE'S A TOOTH HERE AS LONG AS MY HAND. MY MOTHER WOULD WANT TO SEE IT, OR TO SEE IT BURNED. I DON'T KNOW WHICH."
  opt "TAKE IT HOME TO YOUR MOTHER." -> home_go
  opt "BURN IT. LET IT BE OVER." -> burned
  opt "KEEP IT. YOU EARNED IT." -> kept

stage home_go
  goal goto home
  then given
  journal "{FOE} IS DEAD. {GIVER} CARRIES ITS TOOTH HOME TO {HOME}, FOR {GIVER.HIS} MOTHER."

stage given
  end success
  say "{GIVER}'S MOTHER TURNED THE TOOTH OVER IN HER HANDS FOR A LONG TIME, THEN PUT IT ON THE SHELF BESIDE HER HUSBAND'S PIPE, AND MADE THEM BOTH SUPPER, AND TALKED ABOUT HIM ALL NIGHT FOR THE FIRST TIME IN YEARS."
  journal "YOU AND {GIVER} KILLED {FOE}. ITS TOOTH SITS ON A SHELF IN {HOME}, BESIDE A DEAD MAN'S PIPE."
  do deed beast
  do approve 12
  do bond 14
  do reward fair
  do fact "{FOE}, WHICH HUNTED {FOE.DIR} FOR TWENTY YEARS, WAS KILLED BY {GIVER} OF {HOME}, WHOSE FATHER IT KILLED."

stage burned
  end success
  say "THERE. IT CRACKED IN THE FIRE LIKE ANY OLD BONE. IT'S OVER. I DON'T HATE IT ANY MORE. I DON'T THINK I HATE ANYTHING. IT'S A STRANGE FEELING. THANK YOU."
  journal "YOU AND {GIVER} KILLED {FOE} AND BURNED ITS TOOTH, AND {GIVER} LET THE OLD HATE GO."
  do deed beast
  do approve 10
  do bond 12
  do reward fair
  do fact "{FOE}, WHICH HUNTED {FOE.DIR} FOR TWENTY YEARS, WAS KILLED BY {GIVER} OF {HOME} AND A TRAVELLER."

stage kept
  end success
  say "I'LL WEAR IT ON A CORD. NOT AS A TROPHY. AS A REMINDER THAT THE THING I WAS AFRAID OF ALL MY LIFE IS DEAD, AND I'M NOT."
  journal "YOU AND {GIVER} KILLED {FOE}. {GIVER} WEARS ITS TOOTH ON A CORD."
  do deed beast
  do approve 8
  do bond 10
  do reward fair
  do fact "{GIVER} OF {HOME} WEARS THE TOOTH OF {FOE}, WHICH KILLED {GIVER.HIS} FATHER, ON A CORD ROUND {GIVER.HIS} NECK."
)SAGA";

const char* const kChiefReckoning = R"SAGA(
title [[THE FACE I NEVER FORGOT|THE OLD CHIEF|THE RECKONING AT THE CAMP]]
hook companion revenge
pitch "[[YOU'VE GONE VERY QUIET.|WHO WAS THAT YOU SAW ON THE ROAD?|YOU'RE GRIPPING THAT HILT TOO TIGHT.]]"
hint "I SAW A FACE IN THE LAST TOWN. OLDER, GREYER, BUT I'D KNOW IT ANYWHERE. I'VE SEEN IT EVERY NIGHT SINCE I WAS TWELVE."
role giver giver
role home site home
role camp site camp near
role chief person [[male|female]] at camp
slot t2 -> chief_talk mercy

stage start
  talk giver
  say "WHEN I WAS TWELVE, A BANDIT CHIEF CALLED {CHIEF} HELD UP OUR CART ON THE ROAD TO {HOME} AND KILLED MY BROTHER FOR HIS BOOTS. {CHIEF.HE} STILL RUNS A BAND OUT OF {CAMP}, {CAMP.DIR} OF HERE. I'VE WAITED LONG ENOUGH."
  opt "THEN WE END THE BAND." -> camp_go
  opt "WHAT WILL YOU DO, WHEN YOU FIND THEM?" -> what
  opt "NOT LIKE THIS." -> refused

stage what
  talk giver
  say "I DON'T KNOW. I'VE IMAGINED IT A HUNDRED WAYS. MOST OF THEM END WITH {CHIEF.HIM} DEAD. SOME END WITH {CHIEF.HIM} BEGGING. ONE OR TWO END WITH ME WALKING AWAY. I SUPPOSE I'LL FIND OUT. <<proverb>>"
  opt "THEN LET'S FIND OUT." -> camp_go
  opt "NO. NOT LIKE THIS." -> refused

stage refused
  end fail
  say "NOT LIKE THIS. THEN HOW? NEVER MIND. FORGET I ASKED. <<farewell>>"
  journal "{GIVER} WANTED TO FIND {CHIEF}, WHO KILLED {GIVER.HIS} BROTHER. YOU REFUSED."
  do approve -10
  do remember giver "{CHIEF} IS STILL OUT THERE."

stage camp_go
  goal kill 5 bandit in camp
  then @t2
  journal "{CHIEF}, WHO KILLED {GIVER}'S BROTHER, RUNS A BANDIT BAND OUT OF {CAMP}, {CAMP.DIR}. BREAK IT."

stage chief_talk
  talk chief
  say "ALL RIGHT, ALL RIGHT, IT'S OVER, I'M DONE. WHO ARE YOU? WHAT DO YOU WANT FROM AN OLD MAN WITH A BAD LEG? ...YOU. I KNOW THAT LOOK. THE CART ON THE ROAD. THE BOY AND THE BOOTS."
  opt "{GIVER}, THIS IS YOURS." -> giver_choice
  opt "YOU'LL HANG FOR THAT BOY." -> hanged

stage giver_choice
  talk giver
  say "LOOK AT YOU. YOU'RE JUST OLD. I THOUGHT YOU'D BE A MONSTER, AND YOU'RE JUST AN OLD THIEF WHO LIMPS. ...I CAN'T DO IT. OR I CAN, AND I DON'T WANT TO BE THE KIND OF PERSON WHO WOULD. TELL ME WHAT TO DO."
  opt "WALK AWAY. YOU'RE BETTER THAN THAT." -> walked
  opt "DO IT. FOR YOUR BROTHER." -> killed

stage walked
  end success
  say "{GIVER} SPAT IN THE DIRT AT {CHIEF}'S FEET AND TURNED AWAY. ON THE ROAD, A MILE ON, {GIVER} STOPPED AND WEPT, AND THEN LAUGHED, AND THEN WALKED ON LIGHTER THAN BEFORE."
  journal "YOU BROKE {CHIEF}'S BAND AT {CAMP}. {GIVER} FOUND THE OLD CHIEF AND WALKED AWAY."
  do deed mercy
  do approve 8
  do bond 12
  do hide chief
  do reward fair
  do fact "THE BAND OF {CHIEF} AT {CAMP} WAS BROKEN. THE OLD CHIEF WAS LEFT ALIVE BY THE ONE WHO HAD MOST RIGHT TO KILL HIM."

stage killed
  end success
  say "{GIVER} DID IT, AND IT WAS DONE, AND THERE WAS NOTHING AFTERWARDS BUT THE WIND OVER THE CAMP AND THE FLIES COMING. {GIVER} DIDN'T SPEAK UNTIL THE NEXT TOWN, AND DIDN'T SAY WHETHER IT HELPED."
  journal "YOU BROKE {CHIEF}'S BAND AT {CAMP}, AND {GIVER} KILLED THE OLD CHIEF WHO KILLED {GIVER.HIS} BROTHER."
  do deed cruelty
  do approve 6
  do bond 6
  do hide chief
  do reward fair
  do fact "{CHIEF}, AN OLD BANDIT CHIEF OF {CAMP}, WAS KILLED BY THE BROTHER OF A BOY HE MURDERED YEARS BEFORE."

stage hanged
  end success
  say "THEY HANGED {CHIEF} AT THE CROSSROADS BY {HOME}, AND {GIVER} STOOD IN THE CROWD AND WATCHED, AND WENT HOME AFTERWARDS AND SLEPT FOR A DAY AND A NIGHT."
  journal "YOU BROKE {CHIEF}'S BAND AT {CAMP} AND HANDED THE OLD CHIEF TO THE JUSTICE OF {HOME}."
  do deed bandits
  do approve 8
  do bond 8
  do hide chief
  do reward fair
  do fact "{CHIEF}, AN OLD BANDIT CHIEF OF {CAMP}, WAS HANGED AT THE CROSSROADS BY {HOME} FOR A MURDER ON THE ROAD."
)SAGA";

const char* const kHeirloomRuin = R"SAGA(
title [[WHAT THE RUIN KEPT|THE SWORD IN THE DARK|MY GRANDFATHER'S BLADE]]
hook companion heirloom
pitch "[[YOU KEEP STARING AT THAT RUIN.|WHAT DID YOUR FAMILY LOSE?|YOU KNOW THESE OLD STONES, DON'T YOU?]]"
hint "THERE'S SOMETHING OF MY FAMILY'S IN A RUIN NOT FAR FROM HERE. I'VE NEVER HAD THE NERVE TO GO DOWN THERE ALONE. WITH YOU, I MIGHT."
role giver giver
role home site home
role ruin ruin stake
role rival person [[male|female]] at home
slot t1 -> ruin_go world wonder price

stage start
  talk giver
  say "MY GRANDFATHER CARRIED A SWORD WITH OUR NAME ON THE BLADE. HE TOOK IT DOWN INTO {RUIN}, {RUIN.DIR} OF HERE, LOOKING FOR TREASURE, AND NEVER CAME OUT. THE SWORD IS STILL DOWN THERE WITH HIS BONES. I WANT TO BRING THEM BOTH HOME."
  opt "THEN WE GO DOWN TOGETHER." -> @t1
  opt "WHY NOW, AFTER ALL THESE YEARS?" -> why
  opt "LEAVE THE DEAD THEIR DARK." -> refused

stage why
  talk giver
  say "BECAUSE NOW I HAVE SOMEONE TO GO WITH. BECAUSE MY MOTHER IS GETTING OLD AND SHE STILL LAYS A PLACE FOR HIM ON HIS NAME DAY. BECAUSE I'M TIRED OF BEING AFRAID OF A HOLE IN THE GROUND. <<proverb>>"
  opt "GOOD REASONS. WE GO." -> @t1
  opt "NO. LEAVE THEM BE." -> refused

stage refused
  end fail
  say "THE DARK CAN KEEP HIM A WHILE LONGER, THEN. <<farewell>>"
  journal "{GIVER} WANTED TO BRING {GIVER.HIS} GRANDFATHER'S SWORD AND BONES OUT OF {RUIN}. YOU WOULD NOT GO."
  do approve -6
  do remember giver "MY MOTHER LAID HIS PLACE AT THE TABLE AGAIN."

stage ruin_go
  goal fetch "A SWORD WITH A FAMILY NAME" in ruin
  then rival_talk
  journal "{GIVER}'S GRANDFATHER DIED IN {RUIN}, {RUIN.DIR}, WITH THE FAMILY SWORD. FIND IT AND BRING IT OUT."

stage rival_talk
  talk rival
  say "THAT SWORD. I KNOW IT. MY FATHER WENT INTO {RUIN} WITH {GIVER}'S GRANDFATHER, AND ONLY MY FATHER CAME OUT. HE SAID THE OLD MAN SOLD HIM THAT BLADE FOR A SACK OF GRAIN THE WINTER BEFORE. IT'S OURS."
  opt "{GIVER}, IS THAT TRUE?" -> giver_hears

stage giver_hears
  talk giver
  say "GRAIN. IT WAS A HUNGRY WINTER THAT YEAR. MY MOTHER SAYS SO. ...IT COULD BE TRUE. GODS, IT COULD BE TRUE. WHAT DO WE DO?"
  opt "GIVE IT TO THEM. A SALE IS A SALE." -> given
  opt "SHARE THE GRAVE, AND THE SWORD." -> shared
  opt "KEEP IT. IT'S YOUR NAME." -> kept

stage given
  end success
  say "{RIVAL} TOOK THE SWORD IN BOTH HANDS AND CRIED. {GIVER} KEPT ONLY THE BONES, AND BURIED THEM IN {HOME} WITH THE WHOLE FAMILY STANDING ROUND, AND THAT WAS ENOUGH IN THE END. MORE THAN ENOUGH."
  journal "YOU BROUGHT {GIVER}'S GRANDFATHER OUT OF {RUIN}. HIS SWORD WENT TO {RIVAL}, WHOSE FATHER HAD BOUGHT IT."
  do approve 4
  do bond 10
  do take "A SWORD WITH A FAMILY NAME"
  do reward fair
  do remember rival "IT HANGS OVER MY HEARTH. I TELL EVERYONE WHOSE NAME IS ON IT, AND WHY IT'S HERE."
  do fact "THE OLD SWORD OF {GIVER}'S HOUSE CAME OUT OF {RUIN} AND HANGS IN ANOTHER FAMILY'S HALL, BY RIGHT OF A HUNGRY WINTER."

stage shared
  end success
  say "THEY BURIED BOTH FAMILIES' DEAD SIDE BY SIDE IN {HOME}, WITH THE SWORD LAID ACROSS THE TWO GRAVES. {GIVER} AND {RIVAL} DRANK TOGETHER AFTERWARDS, AND BY THE END OF THE NIGHT THEY WERE LAUGHING."
  journal "{GIVER}'S GRANDFATHER CAME OUT OF {RUIN}. HIS SWORD LIES ACROSS TWO GRAVES IN {HOME}, SHARED BY TWO FAMILIES."
  do deed befriended
  do approve 10
  do bond 12
  do take "A SWORD WITH A FAMILY NAME"
  do reward fair
  do fact "IN {HOME} AN OLD SWORD FROM {RUIN} LIES ACROSS TWO GRAVES OF TWO FAMILIES, WHO NOW KEEP THE FEAST DAYS TOGETHER."

stage kept
  end success
  say "IT'S MINE. IT HAS MY NAME ON IT, WHATEVER WAS SOLD IN A HUNGRY WINTER. I'LL CARRY IT. IT FEELS HEAVIER THAN I THOUGHT IT WOULD."
  journal "{GIVER} CARRIES {GIVER.HIS} GRANDFATHER'S SWORD OUT OF {RUIN}, THOUGH {RIVAL} SAYS IT WAS SOLD."
  do deed stole
  do approve 8
  do bond 6
  do take "A SWORD WITH A FAMILY NAME"
  do reward fair
  do remember rival "THEY TOOK IT. MY FATHER'S SWORD. BOUGHT AND PAID FOR."
)SAGA";

const char* const kDebtCalled = R"SAGA(
title [[THE DEBT CALLED IN|THE LEDGER IN THE BACK ROOM|WHAT I OWE]]
hook companion debt
pitch "[[WHO WERE THOSE MEN WATCHING US?|YOU HID YOUR PURSE AGAIN.|SOMETHING IS WORRYING YOU.]]"
hint "THERE WERE TWO MEN AT THE INN WHO KNEW MY NAME. THEY WORK FOR SOMEONE I OWE. I SHOULD HAVE TOLD YOU BEFORE NOW."
role giver giver
role home site home
role market site town near
role lender person [[male|female]] at market
slot t1 -> market_go world price

stage start
  talk giver
  say "I BORROWED MONEY WHEN MY FAMILY'S ROOF FELL IN. A MONEYLENDER IN {MARKET} CALLED {LENDER}, {MARKET.DIR} OF HERE. I'VE PAID IT BACK TWICE OVER, BUT THE INTEREST KEEPS GROWING. NOW THEY'VE SENT MEN. THEY'LL GO TO MY FAMILY NEXT."
  opt "THEN WE'LL GO AND SEE THEM." -> @t1
  opt "HOW MUCH DO YOU OWE?" -> how
  opt "YOUR DEBTS ARE YOUR OWN." -> refused

stage how
  talk giver
  say "BY {LENDER}'S BOOKS, FORTY SILVER. BY ANY HONEST RECKONING, NOTHING AT ALL. BUT HONEST RECKONING DOESN'T SEND MEN TO YOUR MOTHER'S DOOR. <<proverb>>"
  opt "THEN WE GO TO {MARKET}." -> @t1
  opt "I CAN'T HELP YOU." -> refused

stage refused
  end fail
  say "NO. YOU'RE RIGHT. THEY'RE MY DEBTS. I'LL MANAGE. <<farewell>>"
  journal "{GIVER} OWES MONEY TO {LENDER} OF {MARKET}. YOU WOULD NOT HELP."
  do approve -10
  do remember giver "MY MOTHER WROTE. THE MEN CAME AGAIN."

stage market_go
  goal goto market
  then lender_talk
  journal "{LENDER}, A MONEYLENDER OF {MARKET}, {MARKET.DIR}, IS HOUNDING {GIVER} AND {GIVER.HIS} FAMILY FOR A DEBT LONG PAID."

stage lender_talk
  talk lender
  say "{GIVER}! AND A FRIEND. HOW NICE. THE LEDGER SAYS FORTY SILVER, AND THE LEDGER IS NEVER WRONG. PAY IT, AND MY MEN FORGET YOUR MOTHER'S HOUSE. DON'T, AND THEY REMEMBER IT VERY WELL."
  opt "HERE. FORTY. WE'RE DONE." check gold 40 -> paid else cannot
  opt "SHOW ME THIS LEDGER." -> ledger
  opt "TOUCH THEIR FAMILY AND YOU'LL BLEED." -> threatened

stage cannot
  talk lender
  say "THAT ISN'T FORTY SILVER. THAT ISN'T EVEN TWENTY. THEN SHOW ME SOMETHING ELSE, OR GET OUT."
  opt "SHOW ME THIS LEDGER, THEN." -> ledger
  opt "TOUCH THEIR FAMILY AND YOU'LL BLEED." -> threatened

stage ledger
  talk lender
  say "...FINE, LOOK. THERE. AND THERE. ...THOSE ARE ROUNDING ERRORS. EVERY LENDER IN {MARKET} ROUNDS. ALL RIGHT. ALL RIGHT! I'LL STRIKE THE DEBT, IF YOU DON'T SHOW THAT PAGE TO THE REEVE."
  opt "STRIKE IT, AND WE'LL KEEP QUIET." -> struck
  opt "THE REEVE SEES IT ANYWAY." -> exposed

stage paid
  end success
  say "{LENDER} COUNTED THE SILVER TWICE AND STRUCK THE LINE THROUGH {GIVER}'S NAME. OUTSIDE, {GIVER} HUGGED YOU HARD ENOUGH TO HURT, AND SWORE TO PAY BACK EVERY COIN, WITH NO INTEREST AT ALL."
  journal "YOU PAID {GIVER}'S DEBT TO {LENDER} OF {MARKET}, FORTY SILVER, AND THE MEN WILL TROUBLE {GIVER.HIS} FAMILY NO MORE."
  do gold -40
  do deed charity
  do approve 14
  do bond 10
  do reward small
  do remember giver "I'M SAVING. EVERY COIN. YOU'LL SEE."

stage struck
  end success
  say "THE LINE WENT THROUGH {GIVER}'S NAME, AND {LENDER} SMILED A THIN SMILE AND SHOWED YOU OUT. THE MEN NEVER CAME TO {HOME} AGAIN. NOR DID ANYONE ELSE'S DEBT GET ANY SMALLER IN {MARKET}."
  journal "YOU FOUND {LENDER}'S CROOKED LEDGER AND STRUCK {GIVER}'S DEBT, AND KEPT THE SECRET."
  do deed bribed
  do approve 8
  do bond 8
  do reward small
  do remember lender "YOU AGAIN. I REMEMBER YOU. I REMEMBER EVERYONE WHO'S SEEN MY BOOKS."

stage exposed
  end success
  say "THE REEVE OF {MARKET} READ THE LEDGER WITH A FACE LIKE THUNDER, AND {LENDER}'S DOOR WAS NAILED SHUT BY THE END OF THE WEEK. HALF THE TOWN'S DEBTS WERE WIPED CLEAN. {GIVER} BOUGHT EVERYONE IN THE INN A DRINK WITH MONEY THEY DIDN'T HAVE."
  journal "YOU SHOWED {LENDER}'S CROOKED LEDGER TO THE REEVE OF {MARKET}. HALF THE TOWN'S DEBTS WERE STRUCK."
  do approve 14
  do bond 12
  do hide lender
  do reward fair
  do fact "A MONEYLENDER OF {MARKET} WAS FOUND KEEPING A CROOKED LEDGER, AND HALF THE TOWN'S DEBTS WERE STRUCK."

stage threatened
  end success
  say "{LENDER} WENT WHITE AND STRUCK THE DEBT WITH A SHAKING HAND. {GIVER} WALKED OUT FREE, AND QUIET, AND THAT NIGHT ADMITTED TO NOT LIKING THE LOOK ON YOUR FACE IN THERE, OR THAT IT HAD WORKED."
  journal "YOU FRIGHTENED {LENDER} OF {MARKET} INTO STRIKING {GIVER}'S DEBT."
  do deed cruelty
  do approve 4
  do bond 4
  do reward small
  do remember lender "THAT ONE. THE ONE WITH THE EYES. KEEP THEM AWAY FROM ME."
)SAGA";

const char* const kExileReturn = R"SAGA(
title [[THE ROAD BACK|THE PARDON|THE GATES THAT SHUT]]
hook companion exile
pitch "[[YOU NEVER SPEAK OF YOUR HOMELAND.|WHY CAN'T YOU GO HOME?|YOU TURNED AWAY FROM THAT BANNER.]]"
hint "I'VE NOT SET FOOT IN MY OWN COUNTRY SINCE THEY SENT ME OUT OF IT. I'VE BEEN THINKING THAT IF I STOOD BEFORE THE THRONE AGAIN, WITH SOMEONE BESIDE ME, I MIGHT ASK TO COME HOME."
role giver giver
role home site home
role land kingdom home stake
role seat capital land
role lord ruler land
slot t1 -> seat_go world wonder price

stage start
  talk giver
  say "{LAND} CAST ME OUT. I SPOKE AGAINST THE TAXES IN A HUNGRY YEAR, IN THE WRONG ROOM, TO THE WRONG PEOPLE. IT HAS BEEN [[YEARS|SEVEN WINTERS|TOO LONG]]. {LORD} HOLDS COURT IN {SEAT}, {SEAT.DIR} OF HERE. WILL YOU STAND BESIDE ME WHILE I ASK FOR A PARDON?"
  opt "I'LL STAND BESIDE YOU." -> @t1
  opt "WHAT IF THE ANSWER IS NO?" -> doubt
  opt "THE ROAD IS HOME ENOUGH." -> refused

stage doubt
  talk giver
  say "THEN I'LL HAVE ASKED, AND I'LL STOP WONDERING WHETHER I SHOULD HAVE. AND IF THEY CHAIN ME, WELL, YOU'LL KNOW WHERE TO FIND ME. <<proverb>>"
  opt "THEN WE GO TO {SEAT}." -> @t1
  opt "NO. IT'S TOO DANGEROUS." -> refused

stage refused
  end fail
  say "HOME ENOUGH. MAYBE IT IS. IT DOESN'T FEEL LIKE IT TONIGHT. <<farewell>>"
  journal "{GIVER} WANTED TO ASK {LORD} FOR A PARDON IN {SEAT}. YOU WOULD NOT GO."
  do approve -8
  do remember giver "I DREAMED IN MY OWN LANGUAGE LAST NIGHT."

stage seat_go
  goal goto seat
  then lord_talk
  journal "{GIVER} MEANS TO ASK {LORD} OF {LAND} FOR A PARDON. GO TO THE SEAT OF POWER IN {SEAT}, {SEAT.DIR}."

stage lord_talk
  talk lord
  say "THE EXILE {GIVER}, IN MY HALL, WITH A SWORD-HAND BESIDE THEM. BOLD. THEY SPOKE AGAINST THE CROWN. WHY SHOULD I LET THEM HOME, TRAVELLER? SPEAK FOR THEM, IF YOU THINK YOU CAN."
  opt "THEY'VE PAID ENOUGH FOR WORDS." check fame 2 -> pardoned else refused_court
  opt "THEY WERE RIGHT ABOUT THE TAXES." -> defiant
  opt "WHAT WOULD A PARDON COST?" -> price

stage refused_court
  talk lord
  say "WORDS FROM A STRANGER NOBODY KNOWS. NO. THE EXILE STANDS."
  opt "WHAT WOULD A PARDON COST, THEN?" -> price
  opt "THEN WE LEAVE YOUR HALL." -> denied

stage price
  talk lord
  say "THE CROWN'S COFFERS ARE NOT WHAT THEY WERE. FORTY SILVER TO THE TREASURY, AND I'LL FIND IT IN MY HEART."
  opt "FORTY SILVER, THEN." check gold 40 -> bought else denied
  opt "A PARDON SHOULDN'T BE BOUGHT." -> denied

stage pardoned
  end success
  say "{LORD} STRUCK THE SENTENCE WITH A WAVE OF THE HAND, AND {GIVER} KNELT, AND STOOD, AND WALKED OUT INTO {SEAT} A CITIZEN AGAIN. THAT NIGHT {GIVER} SANG THE OLD SONGS OF {LAND} IN THE INN UNTIL THE INNKEEPER BEGGED FOR QUIET."
  journal "ON YOUR WORD, {LORD} PARDONED {GIVER}, WHO MAY GO HOME TO {LAND} AGAIN."
  do approve 15
  do bond 14
  do reward small
  do fact "{GIVER}, EXILED FROM {LAND} FOR SPEAKING AGAINST THE TAXES, WAS PARDONED IN {SEAT} ON A TRAVELLER'S WORD."

stage bought
  end success
  say "THE TREASURER TOOK THE SILVER AND A CLERK WROTE THE PARDON. {GIVER} HELD IT LIKE A LIVE COAL ALL THE WAY OUT OF THE HALL, A PARDON BOUGHT AND PAID FOR, AND COULD NOT SAY WHETHER TO BE GLAD OF IT."
  journal "YOU BOUGHT {GIVER}'S PARDON FROM {LORD} OF {LAND} FOR FORTY SILVER."
  do gold -40
  do deed bribed
  do approve 8
  do bond 8
  do reward small
  do fact "THE EXILE {GIVER} WAS PARDONED IN {SEAT}. THE TREASURY OF {LAND} WAS FORTY SILVER RICHER THAT DAY."

stage defiant
  end success
  say "{LORD} LAUGHED, AND THE COURT LAUGHED, AND THE GUARDS WALKED YOU BOTH TO THE GATE. {GIVER} IS STILL AN EXILE. BUT ALL THE WAY DOWN THE HILL, {GIVER} KEPT LOOKING AT YOU AS IF YOU HAD DONE SOMETHING WONDERFUL."
  journal "YOU TOLD {LORD} THAT {GIVER} WAS RIGHT. {GIVER} IS STILL AN EXILE, AND PROUD OF YOU."
  do approve 12
  do bond 15
  do rep land -2
  do reward small
  do fact "A TRAVELLER TOLD {LORD} OF {LAND} TO THEIR FACE, IN THEIR OWN HALL, THAT THE TAXES OF THE HUNGRY YEAR WERE WRONG."

stage denied
  end fail
  say "THE ANSWER WAS NO. {GIVER} BOWED TO THE THRONE, AS THE OLD COURTESY ASKS, AND WALKED OUT OF {SEAT} STRANGELY LIGHTER, AS IF THE ASKING HAD BEEN THE THING THAT MATTERED."
  journal "{LORD} WOULD NOT PARDON {GIVER}. {GIVER} IS STILL AN EXILE FROM {LAND}."
  do approve 2
  do bond 4
  do remember giver "I ASKED. THAT'S SOMETHING."
)SAGA";

const char* const kPilgrimVow = R"SAGA(
title [[THE VOW ON THE ROAD|THE PILGRIM'S PROMISE|A CANDLE FOR THE SHRINE]]
hook companion faith
pitch "[[YOU PRAY LONGER THAN YOU USED TO.|WHAT DID YOU PROMISE YOUR GOD?|YOU'VE BEEN COUNTING THE DAYS.]]"
hint "WHEN I LEFT HOME I MADE A VOW, AND I'VE BEEN PUTTING IT OFF. THERE'S A SHRINE I SWORE TO REACH, AND THE DAYS ARE RUNNING SHORT."
role giver giver
role home site home
role shrine site town far
role priest npc priest at shrine
role pilgrim person [[male|female]] at home
slot t1 -> shrine_go wonder world

stage start
  talk giver
  say "I SWORE BY {GIVER.GOD} TO LIGHT A CANDLE IN THE TEMPLE AT {SHRINE} BEFORE THE YEAR TURNS. IT'S A LONG WAY {SHRINE.DIR}. I SWORE IT WHEN MY MOTHER WAS SICK, AND SHE LIVED, AND I'VE OWED IT EVER SINCE. WILL YOU WALK IT WITH ME?"
  opt "WE'LL WALK IT TOGETHER." -> pilgrim_talk
  opt "WOULD THE GODS REALLY MIND?" -> doubt
  opt "WE'VE NO TIME FOR SHRINES." -> refused

stage doubt
  talk giver
  say "THE GODS? MAYBE NOT. BUT I WOULD. A VOW IS A VOW, OR NOTHING ANYONE SAYS MEANS ANYTHING. <<saying>>"
  opt "THEN WE'LL KEEP IT." -> pilgrim_talk
  opt "I STILL SAY NO." -> refused

stage refused
  end fail
  say "THEN I'LL GO ALONE, WHEN I CAN. A VOW DOESN'T STOP BEING A VOW BECAUSE IT'S INCONVENIENT. <<farewell>>"
  journal "{GIVER} SWORE TO LIGHT A CANDLE AT {SHRINE}. YOU WOULD NOT GO."
  do approve -10
  do remember giver "I LIT A CANDLE AT A WAYSIDE SHRINE INSTEAD. IT ISN'T THE SAME."

stage pilgrim_talk
  talk pilgrim
  say "YOU'RE TAKING THE PILGRIM ROAD TO {SHRINE}? THEN TAKE THIS FOR ME. MY HUSBAND DIED BEFORE HE COULD GO, AND HE PROMISED HIS MOTHER. IT'S ONLY A BIT OF RIBBON. TIE IT AT THE DOOR FOR HIM."
  do give "A WIDOW'S RIBBON" amulet
  opt "WE'LL TIE IT FOR HIM." -> @t1

stage shrine_go
  goal goto shrine
  then priest_talk
  journal "{GIVER} SWORE TO LIGHT A CANDLE IN THE TEMPLE AT {SHRINE}, FAR {SHRINE.DIR}. A WIDOW'S RIBBON GOES WITH YOU."

stage priest_talk
  talk priest
  say "PILGRIMS. WELCOME. THE CANDLES ARE A COPPER EACH. ...A RIBBON TOO? FOR A DEAD MAN'S VOW? THEN YOU CARRY TWO PROMISES. THE OLD WAY SAYS A PILGRIM WHO CARRIES ANOTHER'S VOW MAY ASK ONE BLESSING. WHAT WILL IT BE?"
  opt "BLESS {GIVER}'S MOTHER." -> blessed
  opt "BLESS THE WIDOW WHO SENT IT." -> widow
  opt "BLESS THE ROAD AHEAD OF US." -> road

stage blessed
  end success
  say "THE CANDLE WAS LIT, THE RIBBON TIED AT THE DOOR, AND {GIVER}'S MOTHER BLESSED FROM ALL THAT WAY OFF. {GIVER} CAME OUT INTO THE SUN LOOKING LIKE SOMEONE WHO HAD PUT DOWN A SACK OF STONES."
  journal "{GIVER} KEPT THE VOW AT {SHRINE}, AND THE PRIEST BLESSED {GIVER.HIS} MOTHER."
  do deed prayed
  do take "A WIDOW'S RIBBON"
  do approve 10
  do bond 10
  do reward small
  do fact "A PILGRIM OF {HOME} KEPT A VOW AT {SHRINE}, AND TIED A DEAD MAN'S RIBBON AT THE TEMPLE DOOR FOR HIS WIDOW."

stage widow
  end success
  say "THE PRIEST BLESSED THE WIDOW WHO SENT THE RIBBON, AND WROTE IT IN THE TEMPLE'S BOOK. {GIVER} LIT THE CANDLE OF THE VOW AND SAID, OUTSIDE, THAT IT WAS THE RIGHT BLESSING TO ASK FOR."
  journal "{GIVER} KEPT THE VOW AT {SHRINE}, AND YOU ASKED THE TEMPLE'S BLESSING FOR THE WIDOW WHO SENT HER RIBBON."
  do deed charity
  do take "A WIDOW'S RIBBON"
  do approve 12
  do bond 12
  do reward small
  do remember pilgrim "THEY TIED IT FOR HIM. AND THEY ASKED A BLESSING FOR ME, THE PRIEST WROTE. FOR ME."

stage road
  end success
  say "THE PRIEST BLESSED THE ROAD AHEAD, AND {GIVER} LIT THE CANDLE OF THE VOW, AND TIED THE WIDOW'S RIBBON AT THE DOOR. THE VOW WAS KEPT, AND THE DAYS NO LONGER NEEDED COUNTING."
  journal "{GIVER} KEPT THE VOW AT {SHRINE}, AND THE PRIEST BLESSED THE ROAD AHEAD."
  do deed prayed
  do take "A WIDOW'S RIBBON"
  do approve 8
  do bond 8
  do reward small
  do fact "A PILGRIM OF {HOME} KEPT A VOW AT {SHRINE} AND TIED A DEAD MAN'S RIBBON AT THE TEMPLE DOOR."
)SAGA";

const char* const kSongRenown = R"SAGA(
title [[A NAME IN THE SONGS|THE DEED OF RENOWN|WHAT THE BARD WILL SING]]
hook companion ambition
pitch "[[YOU WANT SOMETHING. WHAT IS IT?|WHY DO YOU ASK EVERY BARD WE MEET?|YOU'RE RESTLESS TONIGHT.]]"
hint "EVERY BARD IN EVERY INN SINGS ABOUT SOMEONE ELSE. I WANT ONE SONG. ONE. I THINK I KNOW HOW TO EARN IT, BUT I CAN'T DO IT ALONE."
role giver giver
role home site home
role camp site camp near
role bard npc innkeeper at home
slot t1 -> camp_go world price wonder

stage start
  talk giver
  say "THERE'S A BANDIT HOLD AT {CAMP}, {CAMP.DIR} OF HERE. EVERY CARAVAN THAT PASSES IT PAYS IN BLOOD OR SILVER. IF WE BREAK IT, THEY'LL SING ABOUT IT IN {HOME} [[FOR A GENERATION|FOR A HUNDRED YEARS|AS LONG AS THERE ARE INNS]]. I KNOW HOW THAT SOUNDS. I DON'T CARE. WILL YOU COME?"
  opt "LET'S EARN YOUR SONG." -> @t1
  opt "WHY DOES A SONG MATTER SO?" -> why
  opt "GLORY GETS PEOPLE KILLED." -> refused

stage why
  talk giver
  say "BECAUSE NOBODY IN MY FAMILY HAS EVER BEEN ANYTHING BUT TIRED. BECAUSE WHEN I'M DEAD I WANT SOMEONE, SOMEWHERE, TO SAY MY NAME AND MEAN SOMETHING BY IT. IS THAT SO STRANGE? <<proverb>>"
  opt "NO. LET'S GO." -> @t1
  opt "IT'S NOT WORTH DYING FOR." -> refused

stage refused
  end fail
  say "MAYBE NOT. MAYBE THE SONGS ARE ALL LIES ANYWAY. <<farewell>>"
  journal "{GIVER} WANTED A DEED OF RENOWN AT {CAMP}. YOU REFUSED."
  do approve -8
  do remember giver "THE BARD AT THE LAST INN SANG ABOUT SOMEONE ELSE AGAIN."

stage camp_go
  goal kill 6 bandit in camp
  then home_go
  journal "{GIVER} WANTS A DEED WORTH A SONG: BREAK THE BANDIT HOLD AT {CAMP}, {CAMP.DIR}."

stage home_go
  goal goto home
  then bard_talk
  journal "THE HOLD AT {CAMP} IS BROKEN. TELL IT IN {HOME}."

stage bard_talk
  talk bard
  say "THE HOLD AT {CAMP}, BROKEN? BY THE TWO OF YOU? THE CARAVANS WILL STOP PAYING TOLL IN BLOOD. THIS IS A SONG IF EVER I HEARD ONE. NOW, WHO STRUCK THE BLOW THAT BROKE THEM? A SONG NEEDS ONE HERO."
  opt "{GIVER} DID. SING OF {GIVER}." -> giver_song
  opt "I DID. SING OF ME." -> player_song
  opt "BOTH OF US. ONE SONG, TWO NAMES." -> both_song

stage giver_song
  end success
  say "THE SONG OF {GIVER} OF {HOME} WAS SUNG THAT NIGHT, AND THE NEXT, AND IN TOWNS {GIVER} HAD NEVER SEEN. {GIVER} PRETENDED NOT TO LISTEN EVERY TIME AND KNEW EVERY WORD BY THE THIRD NIGHT."
  journal "YOU GAVE {GIVER} THE CREDIT FOR BREAKING {CAMP}. THE SONG OF {GIVER} OF {HOME} IS SUNG IN THE INNS."
  do deed bandits
  do approve 15
  do bond 15
  do reward fair
  do fact "THEY SING OF {GIVER} OF {HOME}, WHO BROKE THE BANDIT HOLD AT {CAMP}."

stage player_song
  end success
  say "THE SONG WAS ABOUT YOU, AND IT WAS A GOOD SONG. {GIVER} CLAPPED WITH EVERYONE ELSE AND SMILED, AND IT WAS NOT A REAL SMILE, AND YOU BOTH KNEW IT."
  journal "YOU TOOK THE CREDIT FOR BREAKING {CAMP}. THE SONG IS ABOUT YOU."
  do deed bandits
  do fame 1
  do approve -10
  do reward fair
  do fact "THEY SING OF A TRAVELLER WHO BROKE THE BANDIT HOLD AT {CAMP}."

stage both_song
  end success
  say "THE BARD GRUMBLED THAT SONGS DON'T WORK THAT WAY, AND THEN MADE IT WORK. TWO NAMES, ONE CHORUS. {GIVER} SANG IT LOUDEST OF ANYONE IN THE ROOM, AND OFF KEY."
  journal "THE SONG OF {CAMP} HAS TWO NAMES IN ITS CHORUS: YOURS, AND {GIVER}'S."
  do deed bandits
  do approve 12
  do bond 12
  do reward fair
  do fact "THEY SING OF {GIVER} OF {HOME} AND A TRAVELLER, WHO BROKE THE BANDIT HOLD AT {CAMP} TOGETHER."
)SAGA";

const char* const kSweetheart = R"SAGA(
title [[THE ONE WHO WAITED|A LETTER NEVER SENT|THE HOUSE BY THE WELL]]
hook companion love
pitch "[[WHO DO YOU WRITE TO AT NIGHT?|YOU SMILED AT NOTHING JUST NOW.|THERE'S SOMEONE, ISN'T THERE?]]"
hint "THERE'S SOMEONE I PROMISED TO COME BACK FOR. I'VE BEEN ON THE ROAD SO LONG I'M AFRAID TO FIND OUT IF THEY STILL WAIT."
role giver giver
role home site home
role town site town near
role love person [[male|female]] at town stake
role suitor person [[male|female]] at town
slot t1 -> town_go world wonder

stage start
  talk giver
  say "MY SWEETHEART {LOVE} LIVES IN {TOWN}, {TOWN.DIR} OF HERE. I SAID I'D COME BACK WITH ENOUGH MONEY FOR A HOUSE, AND THAT WAS [[TWO WINTERS|A YEAR AND MORE|THREE SUMMERS]] AGO. I HAVE THE MONEY NOW, NEARLY. I'M FRIGHTENED. WILL YOU COME WITH ME?"
  opt "I'LL COME. TAKE HEART." -> @t1
  opt "WHAT ARE YOU AFRAID OF?" -> afraid
  opt "THE ROAD NEEDS YOU MORE." -> refused

stage afraid
  talk giver
  say "THAT {LOVE.HE} DIDN'T WAIT. THAT {LOVE.HE} DID, AND I'VE CHANGED TOO MUCH TO BE WHAT {LOVE.HE} WAITED FOR. THAT I'LL HAVE TO CHOOSE BETWEEN A HOUSE AND THE ROAD. <<proverb>>"
  opt "THEN LET'S FIND OUT." -> @t1
  opt "THEN DON'T GO." -> refused

stage refused
  end fail
  say "MORE THAN {LOVE.HE} DOES? MAYBE. I HOPE YOU'RE RIGHT. <<farewell>>"
  journal "{GIVER} WANTED TO GO BACK FOR {LOVE} IN {TOWN}. YOU SAID THE ROAD NEEDED THEM MORE."
  do approve -12
  do remember giver "I STARTED A LETTER TO {TOWN} AGAIN. I DIDN'T FINISH IT."

stage town_go
  goal goto town
  then suitor_talk
  journal "{GIVER}'S SWEETHEART {LOVE} LIVES IN {TOWN}, {TOWN.DIR}. GO THERE WITH {GIVER}."

stage suitor_talk
  talk suitor
  say "LOOKING FOR {LOVE}? I'M TO MARRY {LOVE.HIM} AT MIDSUMMER. OR I WAS, BEFORE YOU WALKED IN. YOU'RE THE ONE {LOVE.HE} TALKS ABOUT, AREN'T YOU? THE ONE ON THE ROAD. {LOVE.HE}'S AT THE WELL."
  opt "WE'LL SPEAK TO {LOVE}." -> love_talk

stage love_talk
  talk love
  say "{GIVER}. YOU CAME BACK. I WAITED, AND THEN I STOPPED WAITING, AND NOW YOU'RE HERE AND I DON'T KNOW WHAT I WANT. THERE'S A GOOD PERSON WHO WANTS TO MARRY ME. AND THERE'S YOU. SAY SOMETHING."
  opt "{GIVER}, IT'S YOUR CHOICE." -> giver_choice

stage giver_choice
  talk giver
  say "I CAN STAY. BUILD THE HOUSE, MARRY {LOVE}, AND NEVER SEE THE ROAD AGAIN. OR I CAN WISH THEM BOTH WELL AND GO ON WITH YOU. TELL ME WHAT YOU THINK. I'LL DECIDE, BUT I WANT TO HEAR IT."
  opt "STAY. BE HAPPY." -> stays
  opt "COME WITH ME. THE ROAD IS YOURS." -> leaves
  opt "ASK {LOVE} TO COME WITH US." -> together

stage stays
  end success
  say "THEN I'LL STAY. I'LL MISS YOU. I'LL MISS ALL OF IT, THE COLD AND THE FIGHTS AND THE BAD INNS. COME TO THE WEDDING, AND COME BY WHEN YOU PASS. THERE'LL ALWAYS BE A BED FOR YOU."
  journal "{GIVER} STAYED IN {TOWN} TO MARRY {LOVE}. THERE IS ALWAYS A BED FOR YOU THERE."
  do approve 15
  do bond 10
  do reward small
  do fate part
  do fact "{GIVER} OF {HOME} CAME BACK FROM THE ROAD TO {TOWN} AND MARRIED {LOVE} AT MIDSUMMER."

stage leaves
  end success
  say "{GIVER} KISSED {LOVE} ONCE ON THE FOREHEAD AND WISHED THEM BOTH JOY, AND WALKED OUT OF {TOWN} WITHOUT LOOKING BACK, AND DID NOT SPEAK FOR TWO DAYS. ON THE THIRD, {GIVER} LAUGHED AT SOMETHING YOU SAID, AND IT WAS ALMOST THE OLD LAUGH."
  journal "{GIVER} LEFT {LOVE} TO MARRY ANOTHER IN {TOWN}, AND CAME ON THE ROAD WITH YOU."
  do approve 2
  do bond 14
  do reward small
  do remember giver "THEY MARRIED AT MIDSUMMER. I SENT A GIFT. I DIDN'T GO."

stage together
  end success
  say "{LOVE} LAUGHED, AND THEN STOPPED LAUGHING, AND THEN SAID: GIVE ME A YEAR. ONE YEAR TO THINK. IF YOU'RE BOTH ALIVE AT MIDSUMMER NEXT, ASK ME AGAIN. {GIVER} HAS BEEN COUNTING THE DAYS EVER SINCE."
  journal "{LOVE} OF {TOWN} ASKED FOR A YEAR TO THINK. {GIVER} IS COUNTING THE DAYS."
  do deed befriended
  do approve 10
  do bond 10
  do reward small
  do remember love "A YEAR. I SAID A YEAR. I'M COUNTING TOO."
)SAGA";

void add(std::vector<Archetype>& v, const char* id, const char* name, uint32_t themes, uint16_t motives, uint32_t twists, const char* body) {
  Archetype a;
  a.id = id;
  a.name = name;
  a.source = Source::World;
  a.themes = themes;
  a.needs = 0;   // (a companion's own history: the caster finds the places, and the quest is chosen only where it casts)
  a.tier = 2;
  a.motives = motives;
  a.twists = twists;
  a.body = body;
  v.push_back(a);
}
uint16_t mb(Motive a, Motive b, Motive c = Motive::COUNT) {
  uint16_t m = (uint16_t)(motiveBit(a) | motiveBit(b));
  if (c != Motive::COUNT) m |= motiveBit(c);
  return m;
}

}  // namespace

void addCompanion(std::vector<Archetype>& v) {
  using M = Motive;
  add(v, "ashes_answered", "THE ASHES ANSWERED", TH_VENGEANCE | TH_MERCY | TH_JUDGEMENT | TH_HOMECOMING | TH_GRIEF, mb(M::Vengeance, M::Grief),
      TF_WORLD | TF_WONDER | TF_PRICE | TF_MERCY, kAshesAnswered);
  add(v, "ashes_rebuilt", "THE ROOF RAISED AGAIN", TH_HOMECOMING | TH_HOSPITALITY | TH_SACRIFICE | TH_HUNGER, mb(M::Hope, M::Duty, M::Grief),
      TF_WORLD | TF_PRICE | TF_WONDER, kAshesRebuilt);
  add(v, "kin_refuge", "THE FACE AMONG THE REFUGEES", TH_KINSHIP | TH_EXILE | TH_MERCY | TH_LOVE, mb(M::Love, M::Hope, M::Grief),
      TF_WORLD | TF_WONDER | TF_PRICE | TF_MERCY, kKinRefuge);
  add(v, "kin_taken", "THE ONES THEY CARRIED OFF", TH_KINSHIP | TH_FREEDOM | TH_COURAGE, mb(M::Love, M::Vengeance, M::Fear),
      TF_WORLD | TF_PRICE | TF_MERCY, kKinTaken);
  add(v, "kin_grave", "WHAT BECAME OF THEM", TH_KINSHIP | TH_GRIEF | TH_JUDGEMENT | TH_TRICKERY, mb(M::Grief, M::Love, M::Duty),
      TF_WONDER | TF_WORLD, kKinGrave);
  add(v, "old_company", "THE LAST OF THE COMPANY", TH_WAR | TH_LOYALTY | TH_BETRAYAL | TH_MERCY | TH_JUDGEMENT, mb(M::Grief, M::Vengeance, M::Duty),
      TF_WORLD | TF_PRICE | TF_WONDER | TF_MERCY, kOldCompany);
  add(v, "levy_roll", "THE NAME ON THE ROLL", TH_WAR | TH_JUDGEMENT | TH_REDEMPTION | TH_FREEDOM, mb(M::Shame, M::Fear, M::Hope), 0, kLevyRoll);
  add(v, "beast_reckoning", "THE BEAST ON THE HOME ROAD", TH_VENGEANCE | TH_COURAGE | TH_GRIEF | TH_KINSHIP, mb(M::Vengeance, M::Grief, M::Fear),
      TF_WORLD | TF_WONDER | TF_PRICE | TF_MERCY, kBeastReckoning);
  add(v, "chief_reckoning", "THE FACE I NEVER FORGOT", TH_VENGEANCE | TH_MERCY | TH_JUDGEMENT | TH_GRIEF, mb(M::Vengeance, M::Grief),
      TF_MERCY, kChiefReckoning);
  add(v, "heirloom_ruin", "WHAT THE RUIN KEPT", TH_KINSHIP | TH_GREED | TH_JUDGEMENT | TH_FRIENDSHIP, mb(M::Pride, M::Duty, M::Grief),
      TF_WORLD | TF_WONDER | TF_PRICE, kHeirloomRuin);
  add(v, "debt_called", "THE DEBT CALLED IN", TH_GREED | TH_JUDGEMENT | TH_KINSHIP | TH_TRICKERY, mb(M::Fear, M::Shame, M::Duty),
      TF_WORLD | TF_PRICE, kDebtCalled);
  add(v, "exile_return", "THE ROAD BACK", TH_EXILE | TH_HOMECOMING | TH_KINGSHIP | TH_JUDGEMENT, mb(M::Hope, M::Shame, M::Pride),
      TF_WORLD | TF_WONDER | TF_PRICE, kExileReturn);
  add(v, "pilgrim_vow", "THE VOW ON THE ROAD", TH_FAITH | TH_SACRIFICE | TH_HOSPITALITY | TH_GRIEF, mb(M::Devotion, M::Duty, M::Hope),
      TF_WONDER | TF_WORLD, kPilgrimVow);
  add(v, "song_renown", "A NAME IN THE SONGS", TH_PRIDE | TH_COURAGE | TH_FRIENDSHIP | TH_LOYALTY, mb(M::Pride, M::Hope), TF_WORLD | TF_PRICE | TF_WONDER,
      kSongRenown);
  add(v, "sweetheart", "THE ONE WHO WAITED", TH_LOVE | TH_HOMECOMING | TH_SACRIFICE | TH_FREEDOM, mb(M::Love, M::Hope, M::Fear), TF_WORLD | TF_WONDER,
      kSweetheart);
}

}  // namespace arch
}  // namespace saga
}  // namespace story
