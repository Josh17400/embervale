// M12 "Rise and Fall": THE WAR OF THE TWO CROWNS (campaign `succession`): a war of succession in a realm near the
// player. REALM lane (15.20, 15.33). It begins where the living sim crowned a successor (hook event successorrose: the
// remnant of a fallen house crowned its heir in an old town of the realm, the news fresh in a village): the player is
// the kingmaker between that heir and the ruler who holds the land, with the old chancellor, the marcher lords and a
// rival kingdom each pulling a different way. Themes from legend (the rightful king, the kingmaker) and the world sims
// (succession, civil war, slow realistic diplomacy: 15.6.3).
//   head      the heir crowned in a barn; what they need, why the player should care
//   arc 1     THE CHANCELLOR'S PRICE   the old chancellor in the capital | THE MARCHER LORDS   a show of strength at the ford
//   arc 2     THE CROWN IN THE TOMB    the old crown under the hill | THE BURNED HAMLET   the ruler's soldiers burned a hamlet
//   arc 3     THE FIGHT AT THE FORD    the ruler's outriders at the ford | THE NIGHT PARLEY   two crowns in one tent
//   arc 4     THE MARSHAL'S OATH       the marshal of the march decides | THE ENVOY OF THE RIVAL   help for a price in land
//   finale    the hall of the capital: the heir crowned (the realm made whole) | the old ruler keeps the crown and names
//             the heir successor (peace) | the realm split in two (civil war) | the heir hanged (a failure)
// Recurring: the heir, the chancellor, the marshal; the ruler, capital, land and rival; the ford, the tomb.
// (The plan is listed in rpg/story/chain.cpp campaignPlans(): `camp::successionPlan()`; its arcs in campaigns/arcs.cpp.)
#include <vector>
#include "rpg/story/saga.h"

namespace story {
namespace saga {
namespace camp {

namespace {

const char* const kHead = R"SAGA(
title THE WAR OF THE [[crowns=TWO CROWNS|BROKEN CROWN|DIVIDED CROWN]]
hook event successorrose
pitch "WHOSE BANNER IS THAT ON THE HALL?"
hint "A NEW BANNER FLIES OVER THE HALL, AND THE OLD ONE STILL FLIES BESIDE IT. THIS VILLAGE HAS TWO MASTERS NOW, AND SOON IT WILL HAVE NONE."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role lord ruler land
role seat capital land
role heir person [[female|male]] at home
role chancellor person male at seat
role ford site village near
role marshal person [[male|female]] at ford
role tomb ruin near
var claim 0
var lords 0
var blood 0

stage call
  talk heir
  say "THEY CROWNED ME IN A BARN, WITH A CIRCLET THE SMITH BEAT OUT OF A HORSESHOE. I'M OF THE OLD HOUSE THAT RULED HERE BEFORE {LORD}, AND HALF OF {LAND} REMEMBERS IT. I DIDN'T ASK FOR THIS. I WON'T LET IT BE A MASSACRE."
  journal "IN {HOME} THE LAST OF THE OLD HOUSE, {HEIR}, HAS BEEN CROWNED AGAINST {LORD}. {LAND} IS ON THE EDGE OF A WAR."
  opt "WHAT DO YOU NEED FROM ME?" -> call2
  opt "WHY SHOULD I BACK YOU?" -> call3
  opt "I WANT NO PART OF A CIVIL WAR." -> declined

stage call2
  talk heir
  say "FRIENDS. THE OLD CHANCELLOR, {CHANCELLOR}, SERVED MY GRANDFATHER AND SERVES {LORD} NOW, AND HAS NOT DECIDED WHICH OF US HE SERVES. AND THE MARCHER LORDS AT {FORD}, WHO DECIDE NOTHING UNTIL THEY SEE WHO IS WINNING."
  opt "I'LL SEE WHAT CAN BE DONE." -> @next
  opt "[[THEN I'LL FIND YOU FRIENDS.|THEN WE START TODAY.]]" -> @next

stage call3
  talk heir
  say "BECAUSE {LORD} TAXES THE OLD LANDS TWICE FOR EVERY TIME THE NEW LANDS PAY ONCE, AND HANGS WHOEVER SAYS SO. BECAUSE IF I LOSE, THEY BURN {HOME} FOR HAVING ME. AND BECAUSE I'D RATHER NOT BE CROWNED OVER GRAVES."
  opt "THEN I'LL HELP YOU." -> @next
  opt "I WANT NO PART OF IT." -> declined

stage declined
  end fail
  say "{HEIR} NODS AS IF EXPECTING IT, AND TURNS BACK TO THE MAP NAILED TO THE BARN DOOR."
  journal "YOU WALKED AWAY FROM THE WAR OF THE [[=crowns]] IN {LAND}."
  do remember heir "THE ROAD WAS OPEN. YOU TOOK IT. MOST PEOPLE DO, WHEN THERE ARE TWO CROWNS IN ONE LAND."
)SAGA";

// (P1 Fresh Roads) another way in: the crown's own notice. A proclamation nailed to a market town's board (never the
// capital's: AH_NOT_CAPITAL) puts a price on the pretender's head, and someone has written over it in charcoal. The heir
// hides in that town. The same recurring roles and vars as kHead.
const char* const kHeadBoard = R"SAGA(
title THE WAR OF THE [[crowns=TWO CROWNS|BROKEN CROWN|DIVIDED CROWN]]
hook board
pitch "THE DEFACED PROCLAMATION"
hint "BY ORDER OF THE CROWN: A PRICE ON THE HEAD OF THE PRETENDER OF THE OLD HOUSE. ACROSS IT, IN CHARCOAL: LONG LIVE THE TRUE CROWN."
role giver giver
role home site home
role land kingdom home
role rival kingdom rival
role lord ruler land
role seat capital land
role heir person [[female|male]] at home
role chancellor person male at seat
role ford site village near
role marshal person [[male|female]] at ford
role tomb ruin near
var claim 0
var lords 0
var blood 0

stage hidden
  talk heir
  say "YOU READ THE NOTICE, AND YOU CAME LOOKING. EITHER YOU WANT THE PRICE ON MY HEAD, OR YOU WANT TO KNOW IF THE CHARCOAL IS TRUE. IT IS. THEY CROWNED ME IN A BARN, WITH A CIRCLET THE SMITH BEAT OUT OF A HORSESHOE. MY HOUSE RULED {LAND} BEFORE {LORD}."
  journal "THE CROWN HAS PUT A PRICE ON {HEIR}, LAST OF THE OLD HOUSE, CROWNED IN {HOME} AGAINST {LORD}. WAR IS NEAR."
  opt "WHAT DO YOU NEED FROM ME?" -> hidden2
  opt "WHY SHOULD I BACK YOU?" -> hidden3
  opt "I WANT NO PART OF A CIVIL WAR." -> declined

stage hidden2
  talk heir
  say "FRIENDS. THE OLD CHANCELLOR, {CHANCELLOR}, SERVED MY GRANDFATHER AND SERVES {LORD} NOW, AND HAS NOT DECIDED WHICH OF US HE SERVES. AND THE MARCHER LORDS AT {FORD}, WHO DECIDE NOTHING UNTIL THEY SEE WHO IS WINNING."
  opt "I'LL SEE WHAT CAN BE DONE." -> @next
  opt "[[THEN I'LL FIND YOU FRIENDS.|THEN WE START TODAY.]]" -> @next

stage hidden3
  talk heir
  say "BECAUSE {LORD} TAXES THE OLD LANDS TWICE FOR EVERY TIME THE NEW LANDS PAY ONCE, AND HANGS WHOEVER SAYS SO. BECAUSE IF I LOSE, THEY BURN {HOME} FOR HIDING ME. AND BECAUSE I WON'T BE CROWNED OVER GRAVES."
  opt "THEN I'LL HELP YOU." -> @next
  opt "I WANT NO PART OF IT." -> declined

stage declined
  end fail
  say "{HEIR} NODS AS IF EXPECTING IT. BY EVENING {HEIR.HE} HAS MOVED TO ANOTHER LOFT, AND THE CHARCOAL ON THE NOTICE HAS BEEN SCRUBBED AWAY, AND WRITTEN AGAIN."
  journal "YOU WALKED AWAY FROM THE WAR OF THE [[=crowns]] IN {LAND}."
  do remember heir "THE ROAD WAS OPEN. YOU TOOK IT. MOST PEOPLE DO, WHEN THERE ARE TWO CROWNS IN ONE LAND."
)SAGA";

const char* const kFinale = R"SAGA(
stage hall
  talk lord
  ?arc_su_parley say "YOU AGAIN, WHO SAT BETWEEN TWO CROWNS IN A TENT AND MADE US BOTH LISTEN. THE HALL IS FULL OF THE HEIR'S FRIENDS AND MINE. SAY WHAT YOU CAME TO SAY, AND WE'LL SEE WHO IS STILL STANDING BY SUPPER."
  !arc_su_parley say "SO YOU ARE THE ONE WHO CARRIED THE HEIR'S BANNER ACROSS MY LAND. THE HALL IS FULL OF THEIR FRIENDS AND MINE, AND THE MARCH IS WATCHING. SAY WHAT YOU CAME TO SAY."
  journal "THE WAR OF THE [[=crowns]] COMES TO THE HALL OF {SEAT}. {LORD} AND THE HEIR, {HEIR}, BOTH WAIT FOR YOUR WORD."
  opt "THE OLD HOUSE TAKES BACK THE CROWN." -> crowned
  opt "KEEP IT. NAME {HEIR} YOUR HEIR." -> named
  opt "SPLIT THE REALM. TWO CROWNS." -> split
  opt "...I CAN'T CHOOSE FOR {LAND}." -> lost

stage crowned
  talk heir
  ?arc_su_tomb say "WITH THE OLD CROWN FROM THE TOMB, AND THE MARCH BEHIND ME, AND NOBODY LEFT TO SAY NO. I SWORE NOT TO BE CROWNED OVER GRAVES. {LORD} GOES TO A QUIET HOUSE BY THE SEA, ALIVE. THAT IS MY FIRST DECREE."
  !arc_su_tomb say "WITH A HORSESHOE ON MY HEAD AND THE MARCH BEHIND ME. I SWORE NOT TO BE CROWNED OVER GRAVES. {LORD} GOES TO A QUIET HOUSE BY THE SEA, ALIVE. THAT IS MY FIRST DECREE."
  opt "LONG LIVE {HEIR}." -> crowned_end

stage crowned_end
  end success
  say "{HEIR} WAS CROWNED IN THE HALL OF {SEAT}, AND THE TWO BANNERS CAME DOWN TOGETHER AND ONE WENT UP. THE OLD LANDS AND THE NEW PAY THE SAME TAX NOW. {LAND} HAS ONE CROWN AGAIN, AND IT REMEMBERS WHO CARRIED THE BANNER."
  journal "{HEIR} OF THE OLD HOUSE RULES {LAND}. THE WAR OF THE [[=crowns]] IS OVER."
  do realm succession land heir
  do realm peace land rival
  do reward great
  do fame 6
  do mark succession_heir_crowned
  do remember chancellor "I SERVED THE GRANDFATHER, AND THE ONE WHO TOOK HIS CROWN, AND NOW THE GRANDCHILD. I AM VERY OLD AND VERY FLEXIBLE. <<proverb>>"
  do fact "IN THE WAR OF THE [[=crowns]], {HEIR} OF THE OLD HOUSE TOOK BACK THE CROWN OF {LAND} FROM {LORD}, AND SPARED THEM."

stage named
  talk lord
  say "NAME THE HEIR MY HEIR. THE OLD HOUSE AND MINE IN ONE LINE, AND THE TAX MADE EVEN, AND THE CROWN ON MY HEAD UNTIL I DIE, WHICH THE PHYSICIANS SAY WILL NOT BE LONG. IT IS EITHER WISDOM OR COWARDICE. I AM TOO TIRED TO TELL WHICH."
  opt "SWEAR IT BEFORE THE HALL." -> named_end

stage named_end
  end success
  say "{LORD} SWORE IT BEFORE THE HALL WITH {HEIR} KNEELING AT THE FOOT OF THE THRONE, AND THEN {LORD} KNELT TOO, WHICH NOBODY EXPECTED. THE MARCH WENT HOME. THE TAX WAS MADE EVEN. NOBODY SANG, BUT NOBODY DIED."
  journal "{LORD} KEEPS THE CROWN OF {LAND} AND HAS NAMED {HEIR} OF THE OLD HOUSE HEIR. THE WAR IS OVER."
  do realm peace land rival
  do reward rich
  do rep land 6
  do mark succession_heir_named
  do remember heir "I WAIT FOR A CROWN NOW INSTEAD OF FIGHTING FOR ONE. IT IS HARDER THAN IT SOUNDS. <<doubt>>"
  do fact "THE WAR OF THE [[=crowns]] ENDED WHEN {LORD} NAMED {HEIR} OF THE OLD HOUSE HEIR TO THE CROWN OF {LAND}."

stage split
  talk chancellor
  say "TWO CROWNS. THE OLD LANDS TO THE HEIR, THE NEW TO {LORD}, AND A RIVER BETWEEN THEM, AND A TOLL ON THE BRIDGE THAT BOTH OF THEM WILL WANT. I HAVE SEEN THIS DONE ONCE BEFORE. IT LASTED ONE GENERATION. ONE IS SOMETHING."
  opt "THEN DRAW THE LINE ON THE MAP." -> split_end

stage split_end
  end success
  say "THE CHANCELLOR DREW THE LINE ALONG THE RIVER WITH A SHAKING HAND, AND BOTH CROWNS SIGNED, AND {LAND} WOKE UP AS TWO REALMS WITH ONE NAME. THE BRIDGE AT {FORD} HAS TWO TOLLHOUSES NOW, ONE ON EACH BANK, AND THEY DO NOT SPEAK."
  journal "{LAND} IS SPLIT ALONG THE RIVER: THE OLD LANDS TO {HEIR}, THE NEW TO {LORD}."
  do realm event civilwar land
  do reward rich
  do mark succession_realm_split
  do remember marshal "MY BROTHER'S FARM IS ON THE OTHER BANK. HE'S A FOREIGNER NOW. WE STILL DRINK TOGETHER. ON THE BRIDGE. IN THE MIDDLE. <<proverb>>"
  do fact "THE WAR OF THE [[=crowns]] SPLIT {LAND} IN TWO ALONG THE RIVER, THE OLD LANDS UNDER {HEIR} AND THE NEW UNDER {LORD}."

stage lost
  end fail
  say "YOU SAID NOTHING, AND THE HALL DECIDED FOR YOU. BY MORNING {HEIR} WAS IN CHAINS, BY NOON THE OLD BANNER WAS BURNING IN THE SQUARE OF {SEAT}, AND BY NIGHT THE SOLDIERS WERE ON THE ROAD TO {HOME} WITH TORCHES."
  journal "THE WAR OF THE [[=crowns]] IS LOST. {HEIR} IS IN CHAINS, AND {LORD} HAS SENT SOLDIERS TO {HOME}."
  do realm event townburned land home
  do mark succession_heir_lost
)SAGA";

// ---- arc 1: friends
const char* const kChancellor = R"SAGA(
stage %road
  goal goto seat
  then %meet
  journal "THE OLD CHANCELLOR OF {LAND}, {CHANCELLOR}, SERVED THE OLD HOUSE ONCE. FIND HIM IN {SEAT}, {SEAT.DIR}."
stage %meet
  talk chancellor
  say "I'M {CHANCELLOR}. I SERVED THE GRANDFATHER OF THE HEIR, AND I SERVE {LORD}, AND I HAVE KEPT THIS REALM'S BOOKS THROUGH BOTH. YOU WANT TO KNOW WHICH CROWN I'LL BOW TO. THE ONE THAT KEEPS THE PEACE. CAN THE HEIR?"
  opt "THE HEIR WANTS NO MASSACRE." -> %price
  opt "WHAT WOULD IT TAKE?" -> %price
stage %price
  talk chancellor
  say "A PROMISE, WRITTEN: NO HANGINGS AFTER, ON EITHER SIDE. AND THE OLD TAX BOOKS BURNED, SO NOBODY OWES TEN YEARS OF ARREARS THE DAY THE CROWN CHANGES. BRING ME THAT, SIGNED BY THE HEIR, AND I'LL OPEN THE BOOKS OF {LORD}'S ARMY TO YOU."
  opt "I'LL BRING IT." -> %sign
  opt "THAT'S A FAIR PRICE." -> %sign
stage %sign
  goal goto home
  then %after
  journal "CARRY THE CHANCELLOR'S TERMS TO THE HEIR, {HEIR}, IN {HOME}: NO HANGINGS AFTER, AND THE OLD TAX BOOKS BURNED."
stage %after
  talk heir
  say "NO HANGINGS AND NO ARREARS. I WOULD HAVE OFFERED BOTH IF HE'D ASKED. I'VE SIGNED IT TWICE, IN CASE HE LOSES ONE. AND HE SENT THIS BACK WITH YOUR RIDER: WHERE {LORD}'S RIDERS WILL CAMP, AND WHEN."
  do add claim 1
  do add lords 1
  opt "THEN WE KNOW WHERE THEY'LL BE." -> @next
  opt "[[THE OLD MAN IS WITH US.|ONE FRIEND AT COURT.]]" -> @next
)SAGA";

const char* const kMarcherLords = R"SAGA(
stage %road
  goal goto ford
  then %meet
  journal "THE MARCHER LORDS MEET AT {FORD}, {FORD.DIR}. THEIR MARSHAL, {MARSHAL}, DECIDES WHERE THEIR SPEARS GO."
stage %meet
  talk marshal
  say "I'M {MARSHAL}, MARSHAL OF THE MARCH. RAIDERS HAVE BEEN BURNING OUR STEADINGS ALL SUMMER, AND NEITHER CROWN HAS LIFTED A FINGER. THE HEIR WANTS OUR SPEARS? LET THE HEIR'S CHAMPION CLEAR OUR ROADS FIRST."
  opt "I'LL CLEAR YOUR ROADS." -> %clear
  opt "AND THEN THE MARCH WILL SIDE WITH US?" -> %terms
stage %terms
  talk marshal
  say "THEN THE MARCH WILL LISTEN. LISTENING IS MORE THAN EITHER CROWN HAS HAD FROM US IN TEN YEARS. GO AND SHOW US WHAT THE HEIR'S FRIENDS ARE WORTH."
  opt "I'LL CLEAR YOUR ROADS." -> %clear
stage %clear
  goal kill 4 bandit
  then %after
  journal "THE MARSHAL OF THE MARCH, {MARSHAL}, WANTS THE RAIDERS ON THE MARCH ROADS DEAD BEFORE THE MARCH LISTENS TO THE HEIR."
stage %after
  talk marshal
  say "THE ROADS ARE QUIET. THE STEADINGS NOTICED. THE MARCH WILL CARRY THE HEIR'S BANNER, FOR NOW, AND WE'LL SEE IF THE HEIR REMEMBERS WHO CARRIED IT WHEN THE CROWN IS WON."
  do add lords 1
  do add blood 1
  opt "THE HEIR WILL REMEMBER." -> @next
  opt "[[THEN WE RIDE TOGETHER.|THE MARCH WON'T REGRET IT.]]" -> @next
)SAGA";

// ---- arc 2: the claim
const char* const kTomb = R"SAGA(
stage %plan
  talk heir
  say "THE OLD CROWN WAS BURIED WITH MY GREAT-GRANDFATHER IN {TOMB}, {TOMB.DIR}, SO {LORD}'S HOUSE COULD NEVER WEAR IT. A HORSESHOE MAKES A POOR CROWN. THE DEAD UP THERE DON'T LIKE VISITORS. WILL YOU GO?"
  opt "I'LL BRING THE CROWN DOWN." -> %tomb
  opt "IS A CROWN WORTH GRAVE-ROBBING?" -> %why
stage %why
  talk heir
  say "TO THE MARCH, YES. THEY'LL FOLLOW A CROWN THEY CAN SEE MORE READILY THAN A CLAIM THEY HAVE TO BELIEVE. I HATE THAT IT'S TRUE. IT'S STILL TRUE."
  opt "THEN I'LL GO." -> %tomb
stage %tomb
  goal kill 3 undead in tomb
  then %crown
  journal "THE OLD CROWN OF {LAND} LIES IN {TOMB}, {TOMB.DIR}, WITH THE DEAD OF THE OLD HOUSE. BRING IT TO THE HEIR, {HEIR}."
stage %crown
  goal goto home
  then %after
  do give "THE OLD CROWN" crown
  journal "YOU HAVE THE OLD CROWN OF {LAND} FROM {TOMB}. CARRY IT TO THE HEIR IN {HOME}."
stage %after
  talk heir
  say "IT'S HEAVIER THAN IT LOOKS, AND IT SMELLS OF THE GRAVE. IT FITS. OF COURSE IT FITS. WORD OF IT IS ALREADY ON THE ROAD, FASTER THAN ANY RIDER."
  do take "THE OLD CROWN"
  do add claim 1
  opt "NOW THEY'LL FOLLOW." -> @next
  opt "[[WEAR IT LIGHTLY.|NOW THE MARCH WILL SEE IT.]]" -> @next
)SAGA";

const char* const kBurnedHamlet = R"SAGA(
role %hamlet site village near
stage %news
  talk heir
  say "{LORD}'S SOLDIERS BURNED {%hamlet}, {%hamlet.DIR}, FOR FLYING MY BANNER. THE PEOPLE ARE IN THE WOODS. IF I DON'T HELP THEM, NOBODY WILL FLY MY BANNER AGAIN. IF I DO, IT'S WAR IN EARNEST."
  opt "I'LL GO TO {%hamlet}." -> %go
  opt "WAR IN EARNEST IS COMING ANYWAY." -> %go
stage %go
  goal goto %hamlet
  then %woods
  journal "{LORD}'S SOLDIERS BURNED {%hamlet} FOR FLYING THE HEIR'S BANNER. ITS PEOPLE HIDE IN THE WOODS."
stage %woods
  goal kill 3 bandit
  then %after
  journal "THE SOLDIERS WHO BURNED {%hamlet} ARE STILL IN THE WOODS, HUNTING ITS PEOPLE. DRIVE THEM OFF."
stage %after
  talk heir
  say "THE PEOPLE OF {%hamlet} ARE IN {HOME}, IN EVERY BARN AND LOFT, AND THEY'VE TOLD EVERYONE WHO HELPED THEM. THE WHOLE MARCH KNOWS THE HEIR'S FRIENDS CAME WHEN THE CROWN'S SOLDIERS BURNED."
  do add lords 1
  do add blood 1
  do deed rescued
  opt "THEN IT'S WAR IN EARNEST." -> @next
  opt "[[THEY'LL REBUILD.|THE MARCH HAS SEEN IT.]]" -> @next
)SAGA";

// ---- arc 3: the field
const char* const kFord = R"SAGA(
stage %warn
  talk marshal
  say "{LORD}'S OUTRIDERS ARE CROSSING AT {FORD} TONIGHT TO TAKE THE HEIR BEFORE THE MARCH CAN GATHER. I'M {MARSHAL}, AND I HAVE TWENTY SPEARS. I NEED ONE MORE WHO KNOWS WHAT THEY'RE DOING."
  opt "I'LL HOLD THE FORD WITH YOU." -> %fight
  opt "HOW MANY ARE COMING?" -> %count
stage %count
  talk marshal
  say "MORE THAN TWENTY. LESS THAN A HUNDRED, IF THE SCOUTS CAN COUNT. ENOUGH THAT THE FORD IS THE ONLY PLACE WE CAN STOP THEM. IN THE WATER, NUMBERS MATTER LESS."
  opt "THEN WE STOP THEM IN THE WATER." -> %fight
stage %fight
  goal kill 5 bandit
  then %after
  journal "{LORD}'S OUTRIDERS CROSS AT {FORD} TONIGHT TO TAKE THE HEIR. HOLD THE FORD WITH THE MARSHAL, {MARSHAL}."
stage %after
  talk marshal
  say "THE FORD HELD. THE OUTRIDERS WENT BACK WET AND FEWER, AND EVERY STEADING ON THE MARCH SAW THEIR TORCHES GO BACK THE WAY THEY CAME. NOW THE CAPITAL HAS TO TALK TO US."
  do add blood 1
  do add lords 1
  opt "THEN TO THE CAPITAL." -> @next
)SAGA";

const char* const kParley = R"SAGA(
stage %offer
  talk chancellor
  say "I'M {CHANCELLOR}, AND I'VE PERSUADED {LORD} TO A PARLEY, TWO CROWNS IN ONE TENT AT {FORD}, AND NO SPEARS WITHIN A MILE. YOU'LL SIT BETWEEN THEM, BECAUSE NEITHER TRUSTS ANYONE ELSE TO."
  opt "I'LL SIT BETWEEN THEM." -> %tent
  opt "WHY WOULD THEY TRUST ME?" -> %trust
stage %trust
  talk chancellor
  say "BECAUSE YOU OWE NEITHER OF THEM ANYTHING, AND THAT IS RARER IN {LAND} THAN AN HONEST TAX-MAN. AND BECAUSE IF IT GOES WRONG, YOU'RE THE EASIEST ONE TO BLAME."
  opt "THEN I'LL SIT BETWEEN THEM." -> %tent
stage %tent
  goal goto ford
  then %after
  journal "THE CHANCELLOR, {CHANCELLOR}, HAS ARRANGED A PARLEY AT {FORD}: {LORD} AND THE HEIR IN ONE TENT, AND YOU BETWEEN THEM."
stage %after
  talk heir
  say "{LORD} LOOKED OLDER THAN I EXPECTED, AND SADDER, AND SAID MY GRANDFATHER'S NAME WITHOUT SPITTING. NOTHING WAS AGREED. BUT WE WILL MEET AGAIN, IN THE HALL OF {SEAT}, AND THAT IS NOT NOTHING."
  do add claim 1
  opt "THEN TO THE HALL." -> @next
)SAGA";

// ---- arc 4: the swords
const char* const kMarshalOath = R"SAGA(
stage %ask
  talk marshal
  say "I'M {MARSHAL}. THE MARCH WILL FOLLOW ME, AND I WILL FOLLOW WHOEVER YOU TELL ME TO, BECAUSE YOU'RE THE ONLY ONE IN THIS WAR WHO HAS CLEARED A ROAD WITHOUT ASKING FOR A TOLL. SO TELL ME. WHICH CROWN?"
  opt "THE HEIR'S. THE OLD HOUSE." -> %oath
  opt "NO CROWN YET. HOLD THE MARCH READY." -> %ready
stage %oath
  talk marshal
  say "THEN THE MARCH RIDES UNDER THE OLD BANNER. I'LL SWEAR IT IN THE SQUARE OF {HOME} TONIGHT, WHERE EVERYONE CAN SEE, SO NOBODY CAN SAY LATER THAT THE MARCH WAS BOUGHT."
  do add lords 1
  opt "SWEAR IT." -> %sworn
stage %ready
  talk marshal
  say "READY, THEN. THE MARCH WILL SIT ON ITS HORSES AT THE EDGE OF THE CAPITAL AND LET BOTH CROWNS SEE IT SITTING THERE. SOMETIMES THAT IS ENOUGH."
  opt "SOMETIMES IT IS." -> %sworn
stage %sworn
  goal wait 1
  then %after
  journal "THE MARCH GATHERS UNDER ITS MARSHAL, {MARSHAL}. TOMORROW, THE HALL OF {SEAT}."
stage %after
  talk heir
  say "THE MARCH IS AT THE GATES OF {SEAT}, AND THE GATES ARE OPEN, AND NOBODY KNOWS YET IF THAT IS A WELCOME OR A TRAP. {LORD} HAS ASKED FOR YOU BY NAME. NOT FOR ME. FOR YOU."
  opt "THEN I'LL GO IN." -> @next
)SAGA";

const char* const kRivalEnvoy = R"SAGA(
role %envoy person male at home
stage %offer
  talk %envoy
  say "I'M {%envoy}, ENVOY OF {RIVAL}. MY MASTERS WILL SEND A THOUSAND SPEARS TO THE HEIR'S BANNER. THEIR PRICE: THE BORDER VILLAGES, AND THE FORD AT {FORD}. A SMALL PRICE FOR A CROWN."
  opt "NO. {LAND} IS NOT FOR SALE." -> %refuse
  opt "WHAT DOES THE HEIR SAY?" -> %heir
stage %heir
  talk heir
  say "THE HEIR SAYS THAT A THOUSAND FOREIGN SPEARS WOULD WIN THE CROWN IN A MONTH AND LOSE THE KINGDOM IN A YEAR. THE HEIR SAYS NO. THE HEIR WANTED YOU TO HEAR IT SAID."
  do add claim 1
  opt "THEN NO." -> %refuse
stage %refuse
  talk %envoy
  say "THEN THE BORDER STAYS WHERE IT IS, AND SO DO OUR SPEARS. MY MASTERS WILL REMEMBER THAT THE OLD HOUSE WAS PROUD. PRIDE IS EXPENSIVE. GOOD DAY."
  do hide %envoy
  opt "GOOD DAY." -> %after
stage %after
  goal goto seat
  then @next
  journal "THE HEIR, {HEIR}, REFUSED THE SPEARS OF {RIVAL}. {LORD} HAS SENT FOR YOU TO THE HALL OF {SEAT}."
)SAGA";

}  // namespace

void addSuccessionArcs(std::vector<Archetype>& v) {
  Archetype a;
  a.source = Source::World;
  a.tier = 3;
  a.needs = N_CAPITAL | N_KINGDOM | N_RIVAL | N_VILLAGE | N_RUIN;

  a.id = "su_chancellor"; a.name = "THE CHANCELLOR'S PRICE";
  a.themes = TH_KINGSHIP | TH_LOYALTY | TH_MERCY; a.body = kChancellor; v.push_back(a);
  a.id = "su_march"; a.name = "THE MARCHER LORDS";
  a.themes = TH_COURAGE | TH_LOYALTY | TH_WAR; a.body = kMarcherLords; v.push_back(a);
  a.source = Source::Legend;
  a.id = "su_tomb"; a.name = "THE CROWN IN THE TOMB";
  a.themes = TH_KINGSHIP | TH_COURAGE | TH_DOOM; a.body = kTomb; v.push_back(a);
  a.source = Source::World;
  a.id = "su_burned"; a.name = "THE BURNED HAMLET";
  a.themes = TH_MERCY | TH_WAR | TH_COURAGE; a.body = kBurnedHamlet; v.push_back(a);
  a.id = "su_ford"; a.name = "THE FIGHT AT THE FORD";
  a.themes = TH_WAR | TH_COURAGE | TH_LOYALTY; a.body = kFord; v.push_back(a);
  a.source = Source::Legend;
  a.id = "su_parley"; a.name = "TWO CROWNS IN ONE TENT";
  a.themes = TH_KINGSHIP | TH_MERCY | TH_TEMPTATION; a.body = kParley; v.push_back(a);
  a.source = Source::World;
  a.id = "su_marshal"; a.name = "THE MARSHAL'S OATH";
  a.themes = TH_LOYALTY | TH_WAR | TH_KINGSHIP; a.body = kMarshalOath; v.push_back(a);
  a.id = "su_envoy"; a.name = "THE ENVOY OF THE RIVAL";
  a.themes = TH_TEMPTATION | TH_PRIDE | TH_KINGSHIP; a.body = kRivalEnvoy; v.push_back(a);
}

CampaignPlan successionPlan() {
  CampaignPlan p;
  p.id = "succession";
  p.name = "THE WAR OF THE TWO CROWNS";
  p.source = Source::World;
  p.themes = TH_KINGSHIP | TH_LOYALTY | TH_WAR | TH_MERCY | TH_TEMPTATION;
  p.needs = N_CAPITAL | N_KINGDOM | N_RIVAL | N_VILLAGE | N_RUIN;
  p.head = kHead;
  p.arcs = {ArcSlot{{"su_chancellor", "su_march"}}, ArcSlot{{"su_tomb", "su_burned"}}, ArcSlot{{"su_ford", "su_parley"}},
            ArcSlot{{"su_marshal", "su_envoy"}}};
  p.finale = kFinale;
  // (P1) the crown's defaced notice on a market town's board (head 1): the heir hides in that town
  AltHead board;
  board.head = kHeadBoard;
  board.needs = N_CAPITAL | N_KINGDOM | N_RIVAL | N_VILLAGE | N_RUIN;
  board.flags = AH_NOT_CAPITAL;
  p.alts.push_back(board);
  return p;
}

}  // namespace camp
}  // namespace saga
}  // namespace story
