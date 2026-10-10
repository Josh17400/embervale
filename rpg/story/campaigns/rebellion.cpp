// M6b "Sagas": THE IRON TITHE (campaign `rebellion`): the rebellion against a tyrant. CAMPAIGNS lane. Themes from
// scripture and legend (the exodus out of bondage, the rightful rule, the judge raised from the threshing floor) and the
// world sims (levies for a war with the neighbour, hunger, slow realistic diplomacy: 15.6.3).
//
// Hook: a herald in the capital proclaims a new tithe of the kingdom's REAL ruler: grain and a son of every household
// for the war against the REAL rival kingdom. Under the proclamation the herald whispers. In a village nearby a miller's
// widow, whose husband hanged for hiding grain, is gathering the angry; the ruler's own captain is losing sleep over
// the orders; the tithe-taker is a sellsword who enjoys the work; and someone in the village is selling names.
//   head      the herald's whisper; the widow in her village
//   arc 1     THE TITHE                tithe day at the village | the hanged miller's wake
//   arc 2     THE RISING               the armoury of the capital | the grain of the crown
//   arc 3     THE TRAITOR              the informer (a real villager) | the captain's choice
//   arc 4     THE ROAD TO THE THRONE   the gate at night | the parley under a white flag
//   finale    the throne room: the widow crowned (and peace with the rival) | the captain crowned | the ruler yields
//             the tithe and keeps the throne | the rising crushed, the village burned (a failure) | civil war
// Recurring: the herald, the widow, the captain, the tithe-taker; the ruler, capital, land and rival; the village.
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title THE [[tithe=IRON|BLACK|THIRD]] TITHE
hook herald
pitch "WHAT ELSE ISN'T IN THE DECREE?"
hint "HEAR YE! BY DECREE OF THE CROWN: A NEW TITHE, OF GRAIN AND OF SONS, FOR THE WAR! ...KEEP WALKING, FRIEND. NOT HERE. NOT LOUD."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role lord ruler land
role hamlet site village near
role widow person female at hamlet
role captain person [[male|female]] at home
role den site camp near
role taker foe male at den
var rising 0
var captain 0
var blood 0

stage whisper
  talk giver
  say "I READ WHAT I'M GIVEN. TODAY I WAS GIVEN THE [[=tithe]] TITHE: A SACK IN THREE AND A SON IN EVERY HOUSE, FOR {LORD}'S WAR ON {RIVAL}. THE LAST TITHE HANGED A MILLER IN {HAMLET} FOR HIDING SEED. HIS WIDOW IS STILL THERE. SHE ISN'T QUIET."
  journal "{LORD} HAS DECREED THE [[=tithe]] TITHE: GRAIN, AND A SON OF EVERY HOUSE. THE HERALD OF {HOME} WHISPERED OF {HAMLET}."
  opt "WHY TELL ME THIS?" -> whisper2
  opt "WHO IS THE WIDOW?" -> whisper3
  opt "A CROWN TAKES WHAT IT NEEDS." -> loyal

stage whisper2
  talk giver
  say "BECAUSE I HAVE A SON. BECAUSE I'VE SHOUTED FOUR DECREES THIS YEAR AND EVERY ONE WAS WORSE. BECAUSE YOU LOOK LIKE NOBODY OWNS YOU, AND IN {LAND} THAT IS GETTING RARE. <<plea>>"
  opt "WHO IS THE WIDOW?" -> whisper3
  opt "A CROWN TAKES WHAT IT NEEDS." -> loyal

stage whisper3
  talk giver
  say "{WIDOW}. HER HUSBAND KEPT BACK TWO SACKS OF SEED-CORN SO {HAMLET} COULD PLANT IN SPRING. {LORD}'S TITHE-TAKER, {TAKER}, HANGED HIM FROM HIS OWN MILL-WHEEL AND TOOK THE SEED ANYWAY. GO TO {HAMLET}, {HAMLET.DIR}. SAY THE HERALD SENT YOU."
  opt "I'LL GO TO {HAMLET}." -> @next
  opt "[[I'LL HEAR HER OUT.|I'M ONLY GOING TO LOOK.]]" -> @next

stage loyal
  end fail
  say "THE HERALD LOOKS AT YOU A MOMENT TOO LONG, THEN STEPS BACK UP ON THE BOX AND SHOUTS THE DECREE AGAIN, LOUDER, AS IF YOU WERE NEVER THERE."
  journal "YOU WALKED AWAY FROM THE HERALD OF {HOME} AND THE [[=tithe]] TITHE."
  do remember giver "HEAR YE, HEAR YE. THE TITHE IS PAID. THE SONS HAVE MARCHED. ...DON'T STOP, FRIEND. YOU DIDN'T STOP BEFORE."
)SAGA";

const char* const kFinale = R"SAGA(
stage throne
  talk lord
  ?arc_re_parley say "YOU AGAIN. THE ONE WHO WALKED IN UNDER A WHITE FLAG AND WALKED OUT WITH MY CAPTAIN'S HEART. WELL? THE PALACE IS YOURS, OR NEARLY. MY GUARDS ARE THINKING ABOUT IT. WHAT DO YOU WANT? SAY IT PLAINLY."
  ?arc_re_night_gate say "SO THE GATE WAS OPENED FROM INSIDE. I ALWAYS WONDERED WHAT IT WOULD SOUND LIKE. QUIETER THAN I THOUGHT. WELL? THE PALACE IS YOURS, OR NEARLY. MY GUARDS ARE THINKING ABOUT IT. WHAT DO YOU WANT?"
  opt "{WIDOW} WILL WEAR THE CROWN." -> crown_widow
  opt "{CAPTAIN} WILL WEAR IT." -> crown_captain
  opt "END THE TITHE AND THE WAR. KEEP IT." -> yields
  opt "...I'M SORRY. I CAN'T DO THIS." -> crushed

stage crown_widow
  talk widow
  say "ME? I GRIND CORN. I CAN'T READ HALF THE WORDS ON A DECREE. ...BUT I CAN READ A SACK, AND A FIELD, AND A HUNGRY CHILD'S FACE. MAYBE THAT'S ENOUGH. WE'LL FIND OUT. FIRST THING: THE SONS COME HOME FROM THE BORDER."
  opt "LONG LIVE {WIDOW}." -> widow_end
  opt "AND {LORD}?" -> widow_mercy

stage widow_mercy
  talk widow
  say "{LORD} GOES TO {HAMLET} AND TURNS A MILL-WHEEL FOR A YEAR, BY HAND, THE WAY MY HUSBAND DID. THEN WE'LL SEE IF THERE'S A PERSON LEFT UNDER THE CROWN-MARKS. I WON'T HANG ANYONE. I'VE SEEN WHAT HANGING DOES TO THE ONES WHO WATCH."
  opt "LONG LIVE {WIDOW}." -> widow_end

stage widow_end
  end success
  say "THEY CROWNED {WIDOW} IN A FLOUR-DUSTED APRON, BECAUSE SHE WOULD NOT TAKE IT OFF. HER FIRST DECREE WAS READ BY THE SAME HERALD WHO READ THE TITHE, WEEPING ALL THE WAY THROUGH IT. THE SONS OF {LAND} CAME HOME FROM THE {RIVAL} BORDER."
  journal "{WIDOW}, A MILLER'S WIDOW OF {HAMLET}, RULES {LAND}. THE TITHE IS ENDED, AND {LAND} HAS MADE PEACE WITH {RIVAL}."
  do realm succession land widow
  do realm peace land rival
  do reward great
  do fame 5
  do mark rebellion_widow
  do remember giver "HEAR YE! BY DECREE OF THE CROWN: THE TITHE IS ENDED! ...I'VE SHOUTED THAT ONE NINE TIMES TODAY. MY VOICE IS GONE. I DON'T CARE."
  do fact "{WIDOW} OF {HAMLET}, WHOSE HUSBAND HANGED FOR HIDING SEED, TOOK THE THRONE OF {LAND} AND ENDED THE WAR WITH {RIVAL}."

stage crown_captain
  talk captain
  say "ME. ...YES. SOMEONE WHO KNOWS WHERE THE SOLDIERS ARE, AND WHERE THE MONEY WENT. {WIDOW} WOULD BE LOVED AND OVERTHROWN BY SPRING. I WON'T BE LOVED. I WON'T BE OVERTHROWN EITHER. THE TITHE ENDS. THE WAR, I'M NOT SURE ABOUT. YET."
  opt "THAT'S NOT WHAT WE FOUGHT FOR." -> captain_end
  opt "LONG LIVE {CAPTAIN}." -> captain_end

stage captain_end
  end success
  say "{CAPTAIN} WAS CROWNED IN MAIL, WITH THE PALACE GUARD AROUND THE THRONE. THE TITHE ENDED. THE WAR WITH {RIVAL} DID NOT. {WIDOW} WENT HOME TO {HAMLET} AND THE MILL, AND DID NOT COME TO THE CROWNING."
  journal "{CAPTAIN} RULES {LAND}. THE TITHE IS ENDED. THE WAR GOES ON, AND {HAMLET} WATCHES."
  do realm succession land captain
  do realm war land rival
  do reward rich
  do mark rebellion_captain
  do remember widow "{CAPTAIN} IS A FAIR MASTER, THEY SAY. A MASTER, THOUGH. WE DIDN'T BLEED FOR A FAIR MASTER."
  do fact "{CAPTAIN}, ONCE CAPTAIN OF THE GUARD, TOOK THE THRONE OF {LAND} WHEN THE TITHE REBELLION REACHED {HOME}."

stage yields
  talk lord
  say "...THE TITHE ENDS. THE WAR ENDS. I SEND ENVOYS TO {RIVAL} WITH MY HAT IN MY HAND. AND I KEEP MY THRONE, AND MY HEAD, AND THE WIDOW KEEPS HER MILL. THAT IS YOUR BARGAIN? YOU ARE EITHER VERY WISE OR VERY TIRED. I'LL TAKE IT."
  opt "SWEAR IT BEFORE THE HERALD." -> yields_end

stage yields_end
  end success
  say "{LORD} SWORE IT IN THE SQUARE, BEFORE THE HERALD AND HALF OF {HOME}, AND KEPT TO IT, MOSTLY, AND THE ENVOYS CAME BACK FROM {RIVAL} WITH TERMS. NOBODY SANG ABOUT IT. IT IS HARD TO SING ABOUT A COMPROMISE."
  journal "{LORD} STILL RULES {LAND}, BUT THE TITHE IS ENDED AND THERE IS PEACE WITH {RIVAL}. {WIDOW} KEEPS {HAMLET}'S MILL."
  do realm peace land rival
  do reward rich
  do rep land 5
  do mark rebellion_yielded
  do remember widow "WE DIDN'T GET A NEW CROWN. WE GOT SEED-CORN AND OUR SONS. I'LL TAKE SEED-CORN OVER A CROWN, MOST YEARS."
  do fact "THE TITHE REBELLION OF {HAMLET} ENDED WITH {LORD} SWEARING IN THE SQUARE OF {HOME} TO END THE TITHE AND THE WAR WITH {RIVAL}."

stage crushed
  end fail
  say "YOU WALKED OUT. THE GUARDS STOPPED THINKING ABOUT IT. BY MORNING {CAPTAIN} WAS IN CHAINS, AND BY THE NEXT NIGHT {TAKER}'S MEN WERE IN {HAMLET} WITH TORCHES, AND THE MILL BURNED LONGEST OF ALL."
  journal "THE TITHE REBELLION IS CRUSHED. {HAMLET} WAS BURNED, AND {WIDOW} IS IN THE HILLS, IF SHE LIVES."
  do realm event townburned land hamlet
  do mark rebellion_crushed
  do remember giver "HEAR YE. THE REBELS OF {HAMLET} ARE PUNISHED. ...I READ WHAT I'M GIVEN. I DON'T HAVE TO LOOK AT YOU WHILE I DO IT."
  do fact "{HAMLET} WAS BURNED BY ORDER OF {LORD} FOR THE TITHE REBELLION. THEY SAY THE MILL BURNED FOR TWO DAYS."
)SAGA";

// ---- arc 1: the tithe
const char* const kTitheDay = R"SAGA(
stage %road
  goal goto hamlet
  then %meet
  journal "{HAMLET}, {HAMLET.DIR}: A MILLER'S WIDOW NAMED {WIDOW}, AND A TITHE-TAKER DUE ANY DAY."
stage %meet
  talk widow
  say "THE HERALD SENT YOU? THEN YOU'RE IN TIME FOR THE SHOW. {TAKER} COMES TODAY FOR THE [[=tithe]] TITHE. THERE ISN'T A THIRD SACK IN {HAMLET} THAT ISN'T SEED, AND THE ONLY SONS LEFT ARE TWELVE. HE'LL TAKE THEM ANYWAY. WATCH."
  opt "I WON'T JUST WATCH." -> %stand
  opt "WHAT CAN I DO ALONE?" -> %alone
stage %alone
  talk widow
  say "ALONE? NOTHING. THAT'S WHAT HE COUNTS ON. EVERY ONE OF US ALONE, BEHIND OUR OWN DOOR, HOPING HE KNOCKS NEXT DOOR INSTEAD. ...STAND IN THE ROAD WITH ME, STRANGER. JUST STAND. LET'S SEE WHO ELSE COMES OUT."
  opt "I'LL STAND." -> %stand
stage %stand
  goal kill 3 bandit
  then %after
  say "{TAKER}'S SELLSWORDS COME UP THE ROAD WITH EMPTY CARTS AND FULL WINESKINS. THEY STOP LAUGHING WHEN THEY SEE THE ROAD IS FULL."
  journal "{TAKER}'S SELLSWORDS HAVE COME TO {HAMLET} FOR THE TITHE. THE VILLAGE IS IN THE ROAD. SO ARE YOU."
stage %after
  talk widow
  say "THEY RAN. {TAKER} RAN. TWELVE OF US IN THE ROAD WITH FLAILS AND ONE STRANGER WITH A SWORD, AND THEY RAN. ...THEY'LL BE BACK WITH MORE. THAT MAKES US REBELS NOW, YOU KNOW. THERE'S NO WAY BACK FROM A ROAD."
  do add rising 1
  do add blood 1
  do remember widow "THE DAY WE STOOD IN THE ROAD. I THINK ABOUT IT EVERY TIME I'M AFRAID. IT HELPS."
  opt "THEN WE GO FORWARD." -> @next
  opt "[[THEN WE ARM {HAMLET}.|THEN WE NEED MORE THAN FLAILS.]]" -> @next
)SAGA";

const char* const kMillerWake = R"SAGA(
stage %road
  goal goto hamlet
  then %wake
  journal "{HAMLET}, {HAMLET.DIR}: A MILLER'S WIDOW NAMED {WIDOW}. THE HERALD SAYS SHE ISN'T QUIET."
stage %wake
  talk widow
  say "YOU'VE COME TO THE WAKE, THEN. LATE. HE'S BEEN DEAD A MONTH, BUT {TAKER} WOULDN'T LET US CUT HIM DOWN TILL TODAY. A LESSON, HE SAID. WE'VE LEARNED IT. WE'RE JUST NOT SURE IT'S THE LESSON HE MEANT."
  opt "WHAT LESSON HAVE YOU LEARNED?" -> %lesson
  opt "I'M SORRY FOR YOUR LOSS." -> %sorry
stage %sorry
  talk widow
  say "SORRY. EVERYONE'S SORRY. THE PRIEST WAS SORRY, THE REEVE WAS SORRY, {LORD} IS PROBABLY SORRY IF ANYONE TOLD {LORD.HIM}. SORRY DOESN'T TURN A MILL. <<grief>>"
  opt "THEN WHAT DOES?" -> %lesson
stage %lesson
  talk widow
  say "THAT A CROWN IS JUST A MAN IN A HAT UNTIL ENOUGH OF US AGREE IT ISN'T. HALF THE VALLEY IS HERE TONIGHT FOR THE WAKE. THE OTHER HALF IS SCARED. I NEED THE SCARED HALF. HELP ME CARRY HIM TO THE HILL, IN THE DAYLIGHT, WHERE THEY CAN SEE."
  opt "I'LL CARRY HIM." -> %carry
  opt "THAT'S A BIG RISK." -> %carry
stage %carry
  goal wait 1
  then %after
  say "YOU CARRY THE MILLER UP THE HILL ON A DOOR, IN FULL DAYLIGHT, WITH {WIDOW} WALKING IN FRONT. BY THE TOP, THE WHOLE VALLEY IS WALKING BEHIND."
  journal "CARRY THE HANGED MILLER OF {HAMLET} TO THE HILL IN DAYLIGHT, WHERE THE VALLEY CAN SEE."
stage %after
  talk widow
  say "DID YOU SEE THEM? ALL OF THEM, EVEN THE REEVE. NOT ONE SWORD, AND THEY'RE ALREADY TALKING LIKE AN ARMY. NOW WE NEED WHAT ARMIES NEED. AND WE NEED IT BEFORE {TAKER} COMES BACK."
  do add rising 1
  do remember widow "YOU CARRIED MY HUSBAND UP THE HILL. YOU DIDN'T KNOW HIS NAME. IT WAS {HAMLET}'S, THAT DAY."
  opt "THEN LET'S GET IT." -> @next
)SAGA";

// ---- arc 2: the rising
const char* const kArmoury = R"SAGA(
stage %plan
  talk widow
  say "SPEARS. THE CROWN'S ARMOURY IN {HOME} IS FULL OF THEM, WAITING FOR OUR SONS TO CARRY THEM TO THE BORDER. THERE'S A CAPTAIN THERE, {CAPTAIN}, WHO LOOKS AWAY WHEN THE PRISONERS ARE WHIPPED. SOMEONE WHO LOOKS AWAY CAN BE MADE TO LOOK AWAY LONGER."
  journal "THE ARMOURY OF {HOME} HOLDS THE SPEARS MEANT FOR {HAMLET}'S SONS. {CAPTAIN} OF THE GUARD MAY LOOK AWAY."
  opt "I'LL TALK TO {CAPTAIN}." -> %go
stage %go
  goal goto home
  then %captain
  journal "GO TO {HOME} AND FIND {CAPTAIN}, CAPTAIN OF THE GUARD. FIND OUT WHAT {CAPTAIN.HE} WILL LOOK AWAY FROM."
stage %captain
  talk captain
  say "THE WIDOW SENT YOU. I KNOW, BECAUSE I'VE HAD HER NAME ON MY DESK FOR A WEEK WITH ORDERS NEXT TO IT. ...THE ARMOURY'S NIGHT WATCH CHANGES AT THE THIRD BELL. FOR ONE HOUR IT'S TWO BOYS AND A DOG. I'VE SAID NOTHING."
  opt "WHY HELP US?" -> %why
  opt "THANK YOU, CAPTAIN." -> %raid
stage %why
  talk captain
  say "BECAUSE I SWORE TO PROTECT {LAND}, AND I'VE SPENT A YEAR PROTECTING {LORD} FROM IT. BECAUSE I HAD A BROTHER IN {HAMLET}. DON'T ASK ME AGAIN. AND DON'T COME TO ME FOR ANYTHING ELSE. I WON'T BE ABLE TO SAY NO TWICE."
  do set captain 1
  opt "THE THIRD BELL, THEN." -> %raid
stage %raid
  goal wait 1
  then %spears
  say "THE THIRD BELL. TWO BOYS AND A DOG, AS {CAPTAIN} SAID. THE DOG WAGS. THE BOYS ARE ASLEEP."
  journal "AT THE THIRD BELL, THE ARMOURY OF {HOME} IS WATCHED BY TWO BOYS AND A DOG. EMPTY IT INTO {HAMLET}'S CARTS."
stage %spears
  talk widow
  do moves widow home
  say "TWO HUNDRED SPEARS. TWO HUNDRED! THE CARTS ARE ALREADY ON THE ROAD. {HAMLET} HAS AN ARMOURY AND {LORD} HAS A HOLE IN {LORD.HIS} WALL. ...WE CAN'T GO BACK NOW. I DON'T THINK I WANT TO."
  do add rising 1
  do rep land -5
  opt "NO GOING BACK." -> @next
)SAGA";

const char* const kCrownGrain = R"SAGA(
stage %plan
  talk widow
  say "SPEARS WON'T FEED {HAMLET} THROUGH WINTER. BUT THE CROWN'S TITHE-BARN AT {DEN} IS FULL TO THE RAFTERS WITH OUR OWN GRAIN, GUARDED BY {TAKER}'S SELLSWORDS. TAKE IT BACK, AND EVERY VILLAGE IN THE VALLEY EATS. AND EVERY VILLAGE LISTENS."
  journal "{TAKER}'S SELLSWORDS GUARD THE CROWN'S TITHE-BARN AT {DEN}, {DEN.DIR}. IT IS FULL OF {HAMLET}'S GRAIN."
  opt "THEN WE TAKE THE BARN." -> %barn
  opt "GRAIN FIRST, OR {TAKER} FIRST?" -> %which
stage %which
  talk widow
  say "GRAIN. ALWAYS GRAIN. KILL {TAKER} AND {LORD} SENDS ANOTHER, WORSE, BY MONDAY. FEED THE VALLEY AND THE VALLEY REMEMBERS WHO FED IT. ...BUT IF {TAKER} STANDS BETWEEN YOU AND A SACK, I WON'T WEEP."
  opt "THE BARN, THEN." -> %barn
stage %barn
  goal kill 4 bandit in den
  then %carts
  say "THE TITHE-BARN STINKS OF SPILLED GRAIN AND SOUR WINE. THE SELLSWORDS ARE PLAYING DICE ON THE SACKS."
  journal "TAKE BACK THE TITHE-BARN AT {DEN}, {DEN.DIR}. {TAKER}'S SELLSWORDS HOLD IT."
stage %carts
  talk widow
  do moves widow den
  say "LOOK AT IT. A YEAR OF BREAD. THERE'S SEED-CORN IN THE BACK, TOO, THE SAME SACKS THEY TOOK FROM MY HUSBAND'S MILL. I KNOW THE STITCHING. I STITCHED IT. ...EVERY VILLAGE GETS ITS SHARE. EVERY ONE. EVEN THE ONES WHO WOULDN'T STAND."
  do add rising 1
  do add blood 1
  do rep land -3
  do remember widow "THE DAY WE OPENED THE TITHE-BARN, THE CHILDREN OF {HAMLET} ATE TILL THEY WERE SICK. BEST SOUND I EVER HEARD."
  opt "EVEN THEM." -> @next
  opt "[[THEN THE VALLEY IS OURS.|NOW {LORD} WILL COME.]]" -> @next
)SAGA";

// ---- arc 3: the traitor
const char* const kInformer = R"SAGA(
role %informer resident any at hamlet
stage %ambush
  goal kill 4 bandit
  then %question
  do moves widow hamlet
  say "SOLDIERS ON THE MOOR ROAD. THEY KNEW THE PATH. THEY KNEW THE HOUR. SOMEBODY TOLD THEM."
  journal "AN AMBUSH ON THE MOOR ROAD TO {HAMLET}. THE CROWN'S SOLDIERS KNEW THE PATH AND THE HOUR. BREAK OUT."
stage %question
  talk widow
  say "SOMEONE IN {HAMLET} IS SELLING US. ONLY SIX KNEW THE MOOR ROAD. I'VE WATCHED THEM ALL DAY. ONE OF THEM HAS NEW BOOTS. {%informer}, THE {%informer.JOB}. NEW BOOTS, IN A HUNGRY YEAR."
  opt "LET ME TALK TO {%informer}." -> %talk
  opt "BOOTS PROVE NOTHING." -> %talk
stage %talk
  talk %informer
  say "...THEY HAVE MY ELDEST. DID SHE TELL YOU THAT? THE TITHE TOOK MY ELDEST IN SPRING. {TAKER} SAYS MY CHILD COMES HOME FROM THE BORDER IF I TELL HIM THINGS. THE BOOTS WERE HIS JOKE. HE LIKES JOKES."
  opt "WE'LL BRING YOUR CHILD HOME." -> %mercy
  opt "YOU NEARLY GOT US KILLED." -> %judge
stage %mercy
  talk %informer
  say "...YOU WOULD? AFTER... THEN LET ME BE USEFUL FOR ONCE. I'LL KEEP TELLING {TAKER} THINGS. THE THINGS YOU WANT HIM TO HEAR. HE'LL NEVER KNOW THE DIFFERENCE. HE THINKS WE'RE ALL TOO STUPID TO LIE."
  do add rising 1
  do befriend %informer
  do remember %informer "I SOLD YOU, AND YOU FORGAVE ME, AND I'VE NEVER WORKED SO HARD FOR ANYONE IN MY LIFE."
  opt "THEN LIE WELL." -> @next
stage %judge
  talk widow
  say "WHAT DO WE DO WITH {%informer.HIM}? THE OLD WAY IS A ROPE. I KNOW SOMETHING ABOUT ROPES. ...SAY IT. YOU FOUND {%informer.HIM}. YOU SAY IT."
  opt "DRIVE {%informer.HIM} OUT OF {HAMLET}." -> %exile
  opt "NO ROPES. NOT EVER." -> %exile
stage %exile
  talk widow
  say "OUT, THEN, WITH THE CLOTHES ON {%informer.HIS} BACK AND NOT THE BOOTS. ...THAT'S HOW IT STARTS, YOU KNOW. FIRST YOU JUDGE ONE OF YOUR OWN. THEN IT GETS EASIER. WATCH ME, STRANGER. IF IT EVER GETS EASY, TELL ME."
  do add blood 1
  do remember %informer "YOU TOLD THEM. I KNOW IT WAS YOU. I HOPE YOUR CROWN IS HEAVY."
  opt "I'LL WATCH." -> @next
)SAGA";

const char* const kCaptainChoice = R"SAGA(
stage %letter
  talk widow
  do moves widow hamlet
  say "A LETTER, UNDER THE MILL DOOR, IN A SOLDIER'S HAND. {CAPTAIN}. {LORD} HAS ORDERED THE GUARD TO BURN {HAMLET} AT THE NEW MOON. {CAPTAIN} ASKS TO MEET YOU. ALONE. AT THE PALACE STABLES. IT COULD BE A TRAP. IT COULD BE A GIFT."
  journal "{CAPTAIN} OF THE GUARD HAS WRITTEN: {HAMLET} IS TO BURN AT THE NEW MOON. MEET {CAPTAIN.HIM} ALONE IN {HOME}."
  opt "I'LL GO." -> %go
stage %go
  goal goto home
  then %stables
  journal "{CAPTAIN} WAITS IN THE PALACE STABLES OF {HOME}. ALONE, THE LETTER SAYS."
stage %stables
  talk captain
  say "YOU CAME. GOOD. I HAVE THE ORDER HERE, SEALED. {HAMLET} BURNS AT THE NEW MOON, AND I LEAD IT. OR I DON'T, AND I HANG, AND THE NEXT CAPTAIN LEADS IT. OR... THE GUARD FOLLOWS ME, NOT THE CROWN. ALL OF IT, IF I ASK."
  opt "THEN ASK THEM." -> %turn
  opt "WHY SHOULD WE TRUST YOU?" -> %trust
  opt "THIS IS A TRAP." -> %trap
stage %trust
  talk captain
  say "YOU SHOULDN'T. I'VE SERVED {LORD} FOR TEN YEARS AND HANGED PEOPLE WHO DESERVED IT LESS THAN {LORD.HIM}. BUT I CAN'T BURN A VILLAGE. I FOUND THE BOTTOM OF MYSELF, AND THAT'S WHERE IT IS. IT'S NOT MUCH OF A BOTTOM. IT'S WHAT I HAVE."
  opt "IT'S ENOUGH. ASK THEM." -> %turn
  opt "THIS IS STILL A TRAP." -> %trap
stage %trap
  goal kill 3 bandit
  then %turn
  say "SHAPES IN THE STABLE LOFT. NOT GUARDSMEN: {TAKER}'S SELLSWORDS, SENT TO WATCH THE CAPTAIN. THEY WERE NEVER {CAPTAIN}'S."
  journal "{TAKER}'S SELLSWORDS WERE WATCHING {CAPTAIN}. THEY ARE IN THE STABLE LOFT. NOW THEY ARE COMING DOWN."
stage %turn
  talk captain
  say "...THEY'LL FOLLOW ME. MOST OF THEM. TELL {WIDOW} THE GUARD WON'T BURN {HAMLET}. TELL HER THE GUARD MIGHT EVEN OPEN A GATE ONE NIGHT, IF SOMEONE IS STANDING ON THE OTHER SIDE OF IT. <<vow>>"
  do set captain 1
  do add rising 1
  do remember captain "I WAS A CROWN'S DOG FOR TEN YEARS. YOU MADE ME CHOOSE. I'M STILL NOT SURE IF I SHOULD THANK YOU."
  opt "SHE'LL BE THERE." -> @next
)SAGA";

// ---- arc 4: the road to the throne
const char* const kNightGate = R"SAGA(
stage %taker
  goal slay taker
  then %gather
  say "{TAKER} HAS FORTIFIED {DEN} AS HIS TITHE-CAMP. WHILE HE HOLDS IT, NO REBEL CAN REACH {HOME} UNSEEN."
  journal "{TAKER}, THE TITHE-TAKER WHO HANGED THE MILLER, HOLDS {DEN}, {DEN.DIR}. END HIM, AND THE ROAD TO {HOME} IS OPEN."
stage %gather
  talk widow
  do moves widow home
  say "{TAKER} IS DEAD. I THOUGHT I'D FEEL SOMETHING. I FEEL LIKE IT'S RAINING AND I'VE LEFT THE WASHING OUT. ...THE VALLEY'S HERE. FOUR HUNDRED, OUTSIDE THE WALLS OF {HOME}. IF THE GATE OPENS, WE GO IN. IF NOT..."
  do add blood 1
  opt "IT WILL OPEN." -> %gate
stage %gate
  goal wait 1
  then @next
  do moves captain home
  say "MIDNIGHT. THE GREAT GATE OF {HOME}. FOR A LONG TIME, NOTHING. THEN THE BAR SCRAPES, AND A CRACK OF LAMPLIGHT, AND SOMEONE ON THE OTHER SIDE WHISPERS YOUR NAME."
  journal "THE VALLEY WAITS OUTSIDE {HOME} IN THE DARK. THE GATE OPENS AT MIDNIGHT, OR NOT AT ALL."
)SAGA";

const char* const kParley = R"SAGA(
stage %flag
  talk widow
  do moves widow home
  say "{LORD} HAS SENT A WHITE FLAG. A PARLEY, IN THE THRONE ROOM ITSELF, AND SAFE CONDUCT. IT'S EITHER A TRICK OR {LORD.HE}'S FRIGHTENED. EITHER WAY, I WON'T GO. I'D SPIT IN {LORD.HIS} FACE AND GET US ALL KILLED. YOU GO."
  journal "{LORD} HAS OFFERED PARLEY UNDER A WHITE FLAG, IN THE THRONE ROOM OF {HOME}. {WIDOW} SENDS YOU."
  opt "I'LL GO." -> %throne
stage %throne
  talk lord
  say "SO YOU'RE THE ONE. SIT. NO? STAND, THEN. I'LL BE BRIEF: {RIVAL} IS AT MY BORDER, MY GUARD WHISPERS, AND A MILLER'S WIDOW HAS AN ARMY. I'LL OFFER YOU A TITLE AND A HALL TO GO AWAY. WHAT DO YOU OFFER ME?"
  opt "NOTHING. I CAME TO LOOK AT YOU." -> %look
  opt "YOUR LIFE, IF YOU STEP DOWN." -> %life
  opt "A TITLE, YOU SAID?" -> %bribe
stage %bribe
  talk lord
  say "THERE. EVERYONE HAS A PRICE, EVEN HEROES. A HALL IN THE HILLS, A HUNDRED ACRES, A SEAT AT MY TABLE. ...YOU'RE NOT SMILING. OH. YOU WERE TESTING ME. HOW TIRESOME. AND HOW VERY, VERY DANGEROUS FOR YOU."
  do add blood 1
  opt "I WAS. YOU FAILED." -> %look
stage %life
  talk lord
  say "MY LIFE. HOW GENEROUS. I HAVE A CAPTAIN WHO CAN'T LOOK ME IN THE EYE, AND A HERALD WHO READS MY DECREES LIKE A DIRGE. PERHAPS YOU'RE RIGHT. PERHAPS THE CROWN IS ALREADY OFF, AND I HAVEN'T NOTICED THE DRAUGHT."
  opt "THEN TAKE IT OFF YOURSELF." -> %look
stage %look
  talk captain
  say "THE PARLEY'S OVER. {LORD.HE} DOESN'T KNOW YET, BUT IT IS. I WALKED YOU IN UNDER A WHITE FLAG. I'LL WALK YOU OUT UNDER THE GUARD'S. TELL {WIDOW} TO COME UP TO THE PALACE IN THE MORNING. THE DOORS WILL BE OPEN."
  do set captain 1
  do add rising 1
  opt "THE DOORS WILL BE OPEN?" -> %doors
stage %doors
  goal wait 1
  then @next
  say "IN THE MORNING THE PALACE DOORS STAND OPEN, AND THE GUARD STANDS ASIDE, AND THE VALLEY WALKS IN, VERY QUIETLY, LIKE PEOPLE INTO A CHURCH."
  journal "THE GUARD OF {HOME} HAS STOOD ASIDE. THE VALLEY WALKS INTO THE PALACE. {LORD} IS WAITING ON THE THRONE."
)SAGA";

}  // namespace

void addRebellionArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::World;
  a.tier = 3;
  a.needs = N_CAPITAL | N_KINGDOM | N_VILLAGE | N_CAMP;

  a.id = "re_tithe_day"; a.name = "TITHE DAY";
  a.themes = TH_COURAGE | TH_FREEDOM | TH_HUNGER; a.body = kTitheDay; v.push_back(a);
  a.source = Source::Scripture;
  a.id = "re_miller_wake"; a.name = "THE HANGED MILLER'S WAKE";
  a.themes = TH_GRIEF | TH_FREEDOM | TH_JUDGEMENT; a.body = kMillerWake; v.push_back(a);
  a.source = Source::World;
  a.id = "re_armoury"; a.name = "THE ARMOURY AT THE THIRD BELL";
  a.themes = TH_TRICKERY | TH_FREEDOM | TH_LOYALTY; a.body = kArmoury; v.push_back(a);
  a.id = "re_crown_grain"; a.name = "THE GRAIN OF THE CROWN";
  a.themes = TH_HUNGER | TH_MERCY | TH_FREEDOM; a.body = kCrownGrain; v.push_back(a);
  a.id = "re_informer"; a.name = "THE ONE WITH NEW BOOTS";
  a.themes = TH_BETRAYAL | TH_MERCY | TH_KINSHIP; a.body = kInformer; v.push_back(a);
  a.source = Source::Legend;
  a.id = "re_captain_choice"; a.name = "THE CAPTAIN'S CHOICE";
  a.themes = TH_LOYALTY | TH_BETRAYAL | TH_REDEMPTION; a.body = kCaptainChoice; v.push_back(a);
  a.source = Source::World;
  a.id = "re_night_gate"; a.name = "THE GATE AT MIDNIGHT";
  a.themes = TH_VENGEANCE | TH_COURAGE | TH_WAR; a.body = kNightGate; v.push_back(a);
  a.source = Source::Legend;
  a.id = "re_parley"; a.name = "THE PARLEY UNDER A WHITE FLAG";
  a.themes = TH_KINGSHIP | TH_TEMPTATION | TH_COURAGE; a.body = kParley; v.push_back(a);
}

CampaignPlan rebellionPlan() {
  CampaignPlan p;
  p.id = "rebellion";
  p.name = "THE IRON TITHE";
  p.source = Source::World;
  p.themes = TH_FREEDOM | TH_KINGSHIP | TH_BETRAYAL | TH_HUNGER | TH_COURAGE;
  p.needs = N_CAPITAL | N_KINGDOM | N_RIVAL | N_VILLAGE | N_CAMP;
  p.head = kHead;
  p.arcs = {ArcSlot{{"re_tithe_day", "re_miller_wake"}}, ArcSlot{{"re_armoury", "re_crown_grain"}},
            ArcSlot{{"re_informer", "re_captain_choice"}}, ArcSlot{{"re_night_gate", "re_parley"}}};
  p.finale = kFinale;
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
