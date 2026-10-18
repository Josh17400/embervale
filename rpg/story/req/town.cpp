// P1b "New Roads Content" (VISION_PLAN 15.35): the ten new market-town requests (content spec: req_town.md, every final
// judge note of req_town.judge.md resolved). REQUESTS lane. Each script is `family request`, `settle town` (market towns
// only), `once site` and `cooldown 6`. The realm state each one answers (a festival, rising prices, a failed harvest, a
// broken trade deal, a famine, a trade deal, refugees) is checked by req_offers.cpp's raise policy, not by an event role:
// an event role would make the caster refuse the script wherever the news has not reached (rpg_test --story casts every
// script at the start village).
//
//   rq_candles    THE CHANDLER'S DEBT   a vigil without wax; an estranged sister, her father's mould and her failing eyes
//   rq_ledger     THE FORGED LEDGER     fifty names in a master ledger, forty in the collector's book, ten already paid
//   rq_measure    THE CROOKED DISH      a false-bottomed toll dish; the baker's journeyman who meant only her sacks
//   rq_players    THE STRANDED PLAYERS  a troupe's trunk from a bandit camp, and the player writes the play's last leaf
//   rq_wagon      THE CARTER'S GIFT     the town's iron given to a starving village; nine farms, three choices
//   rq_saltring   THE SALT RING         a foreign trader salted as a thief; a gate tally, new boots and a dice debt
//   rq_pitches    THE KING OF DUST      a refugees' warden of the fountain steps, his knotted cord and the captain's cut
//   rq_pig        SIR TRUFFLE           a prize pig, a woken grave and a dead man's coin the day before the fair
//   rq_bell       THE LAST BELL         clean bronze from a ruin, and the lead the founder hid in the town's bell
//   rq_courier    THE ORDER AT THE GATE a pale courier, an order that came too late, and what he is told it said
//
// Replacements of the spec (judge notes): THE SALT RING keeps its crowd and its salt but is no longer a sickness tale (the
// city's THE CHALKED ROW is): a theft pinned on a stranger, asked by the keeper of the gate tally, with a local origin for
// the custom. THE LAST BELL drops the who-pours succession (the tale HAMMER AND TONGS has it): the founder's lead is the
// decision. THE ORDER AT THE GATE fits any fall of its ruin (plague, flood, war...): the order said come away. The town
// market's own trades give every request (no herbalist lives in a town; the chandler is a merchant, the farmer whose sacks
// the miller refuses asks for the dish).
#include <string>
#include "rpg/story/dsl.h"

namespace story {
namespace dsl {

namespace {

const char* const kTown[] = {
R"DSL(
script rq_candles
title THE CHANDLER'S DEBT
archetype town request: a family quarrel
tier 2
family request
settle town
once site
cooldown 6
gossip "THE CANDLE-SELLER HAS POURED NOTHING."
gossip "NO WAX FOR THE VIGIL THIS YEAR."
hook npc merchant
pitch "YOUR MOULDS ARE ALL EMPTY."
hint "NO WAX, NO CANDLES, AND THE VIGIL AT DUSK. IF YOU'RE BUYING CANDLES, YOU'RE A WEEK TOO EARLY OR A LIFETIME TOO LATE."
role giver giver
role home site home
role hive site village near
role keeper person female at hive
var mould 0
var door 0
var stolen 0

stage start
  talk giver
  say "EVERY DOOR IN {HOME} LIGHTS A CANDLE AT THE VIGIL, AND I POUR THEM ALL. THE WAX RAN OUT. {KEEPER} KEEPS THE BEES AT {HIVE}, {HIVE.DIR} OF HERE. SHE'S MY SISTER. WE HAVEN'T SPOKEN IN ELEVEN YEARS."
  opt "I'LL FETCH YOUR WAX." -> go
  opt "WHY DON'T YOU SPEAK?" -> why
  opt "TALLOW WILL DO." -> refuse

stage why
  talk giver
  say "AT OUR FATHER'S WAKE SHE BROUGHT THE WAX FOR HIS VIGIL. HE'D PROMISED HER HIS BRASS MOULD, AND I KEPT IT. I SAID I'D PAY FOR THE WAX AFTER. I NEVER DID. TAKE THE MOULD TO HER. IT WAS ALWAYS HERS."
  opt "I'LL CARRY THE MOULD." -> go_mould
  opt "I'LL JUST FETCH THE WAX." -> go

stage go_mould
  goal goto hive
  then keeper
  do give "A BRASS CANDLE MOULD" ingot
  do set mould 1
  journal "TAKE THE BRASS CANDLE MOULD OF {GIVER} TO {KEEPER} AT {HIVE}. IT WAS PROMISED TO HER LONG AGO. {HOME} NEEDS BEESWAX."

stage go
  goal goto hive
  then keeper
  journal "{GIVER} OF {HOME} NEEDS BEESWAX FOR THE VIGIL. {KEEPER}, THE ESTRANGED SISTER, KEEPS BEES AT {HIVE}."

stage keeper
  talk keeper
  say "MY {GIVER.BROTHER} SENT A STRANGER. OF COURSE. ELEVEN YEARS, AND NOT A SCRAP OF WAX SOLD TO THAT SHOP. AND SOMETHING HAS TORN MY HIVES OPEN, TWO SETS OF CLAWS. THEY COME BACK EVERY DUSK FOR THE COMB."
  opt "YOUR FATHER'S MOULD. SHE SENT IT." if is mould 1 -> mould_read
  opt "I'LL DEAL WITH WHAT TORE YOUR HIVES." -> beasts
  opt "YOUR {GIVER.BROTHER} IS ASHAMED." check level 4 -> persuaded else stubborn

stage mould_read
  talk keeper
  say "FATHER'S MOULD. I WANTED IT FOR ELEVEN YEARS, AND NOW I CAN BARELY SEE IT. MY EYES ARE GOING. THAT'S HALF WHY THE HIVES WENT TO RUIN, NOT ONLY THE CLAWS. I WAS TOO PROUD TO ASK FOR HELP. TAKE A CRATE OF WAX. AND TELL {GIVER} TO COME."
  do take "A BRASS CANDLE MOULD"
  do give "A CRATE OF BEESWAX" sheaf
  do set door 1
  do remember keeper "FATHER'S MOULD SITS ON MY SILL. I CAN'T SEE IT WELL, BUT I KNOW IT'S THERE."
  opt "I'LL SEND {GIVER} TO YOU." -> wax

stage beasts
  talk keeper
  say "I CAN'T HARVEST WITH THEM ABOUT; I CAN HARDLY SEE THEM COMING. WAIT AT THE HIVES AT DUSK AND KILL THEM, AND MY CELLAR IS OPEN TO YOU."
  opt "THEY'RE AS GOOD AS GONE." -> dusk

stage dusk
  goal time 19 23
  then raid
  journal "SOMETHING WITH CLAWS TEARS THE HIVES OF {KEEPER} AT {HIVE} EVERY DUSK. WAIT FOR IT AT THE HIVES, AND SHE WILL OPEN HER WAX CELLAR."

stage raid
  goal kill 2 beast
  then beasts_done
  do ambush beast 2
  say "THERE, AT THE FAR HIVES. I CAN HEAR THEM."
  journal "THE HIVE-RAIDERS HAVE COME FOR THE COMB AT {HIVE}. KILL THEM BOTH."

stage beasts_done
  talk keeper
  say "QUIET. THE FIRST QUIET DUSK IN A MONTH. THE CELLAR'S YOURS: TAKE A CRATE. AND TELL {GIVER} THE DEBT STILL STANDS. ELEVEN BLOCKS. {GIVER.HE} CAN COUNT."
  do give "A CRATE OF BEESWAX" sheaf
  do remember keeper "YOU CLEARED MY HIVES. YOU DIDN'T CLEAR HER DEBT."
  opt "I'LL PASS IT ON." -> wax

stage persuaded
  talk keeper
  say "ASHAMED. YES, THAT I BELIEVE. I WAS TOO PROUD TO ASK FOR THE MOULD, AND TOO PROUD TO ASK FOR THE COIN. TAKE A CRATE. TELL {GIVER} THE DOOR IS OPEN, IF ANYONE WANTS TO WALK THROUGH IT."
  do give "A CRATE OF BEESWAX" sheaf
  do set door 1
  do remember keeper "A STRANGER SAID SORRY FOR MY {GIVER.BROTHER}. IT SHOULD HAVE COME FROM THE ONE WHO OWED IT."
  opt "I'LL SAY THE DOOR IS OPEN." -> wax

stage stubborn
  talk keeper
  say "WORDS COST NOTHING. WAX COSTS ME. THE CLAWS TOOK MY WHOLE SUMMER, AND I'LL NOT GIVE WHAT'S LEFT TO A SHOP THAT NEVER PAID."
  opt "THEN I'LL DEAL WITH THE CLAWS." -> beasts
  opt "[COME BACK FOR IT AFTER DARK]" check level 5 -> theft else caught
  opt "THEN THE VIGIL BURNS TALLOW." -> tallow

stage theft
  goal goto home
  then back
  do give "A CRATE OF BEESWAX" sheaf
  do set stolen 1
  do deed stole
  do remember keeper "SOMEONE EMPTIED MY CELLAR IN THE NIGHT. I KNOW WHO. I JUST CAN'T SEE THEM."
  say "THE LATCH IS OLD. THE DOG IS OLDER. NOBODY STIRS."
  journal "YOU TOOK A CRATE OF WAX FROM THE CELLAR OF {KEEPER} BY NIGHT. TAKE IT TO {GIVER} IN {HOME}."

stage caught
  end fail
  say "A DOG WAKES. {KEEPER} STANDS IN THE DOORWAY WITH A LAMP SHE CAN BARELY SEE BY, AND STILL SHE SEES ENOUGH. YOU LEAVE WITHOUT A CRUMB OF WAX."
  journal "{KEEPER} CAUGHT YOU IN HER WAX CELLAR AT {HIVE}. THE VIGIL OF {HOME} HAS NO BEESWAX."
  do deed stole
  do remember keeper "I CAUGHT A THIEF IN MY CELLAR. MY {GIVER.BROTHER}'S ERRAND-RUNNER, NO LESS."
  do fact "{KEEPER} CAUGHT A THIEF IN HER WAX CELLAR AT {HIVE}. THE VIGIL IN {HOME} HAS NO BEESWAX."

stage wax
  goal goto home
  then back
  journal "TAKE THE BEESWAX BACK TO {GIVER} IN {HOME}. THE MOULDS ARE EMPTY AND THE VIGIL IS AT DUSK."

stage back
  talk giver
  say "WAX. YELLOW, HEAVY, HONEST WAX. NOW I CAN POUR. ...DID SHE ASK AFTER ME? SHE DID, OR SHE DIDN'T. EITHER WAY, TELL ME."
  do take "A CRATE OF BEESWAX"
  do take "A BRASS CANDLE MOULD"
  opt "SHE WANTS TO SEE YOU HERSELF." if is door 1 -> goes
  opt "SHE SAYS THE DEBT STILL STANDS." if is door 0 if is stolen 0 -> owes
  opt "I TOOK IT FROM HER CELLAR BY NIGHT." if is stolen 1 -> confess
  opt "IT WAS BOUGHT. LIGHT YOUR CANDLES." if is stolen 1 -> end_hush

stage goes
  talk giver
  say "AFTER ELEVEN YEARS, SHE WANTS ME? HER EYES... I DIDN'T KNOW. NOBODY TOLD ME. HOW COULD THEY? I NEVER ASKED. THEN I'LL WALK UP THERE TOMORROW, WITH A PURSE AND NO EXCUSES, AND I'LL READ HER THE BEE-BOOKS MYSELF."
  opt "SHE'LL LIKE THAT." -> end_mended

stage owes
  talk giver
  say "THE DEBT STANDS. OF COURSE IT DOES. THEN I'LL PAY EVERY BLOCK, AT THE NEXT VIGIL, IN PERSON. NO MORE STRANGERS ON MY ERRANDS."
  opt "SHE'D RESPECT THAT." -> end_owes

stage confess
  talk giver
  say "BY NIGHT? YOU ROBBED MY SISTER? FOR ME. DAMN YOU, AND DAMN ME TOO. I'LL CARRY HER A PURSE TOMORROW AND TELL HER MYSELF. AND YOU STAY OUT OF HER CELLAR."
  opt "AGREED." -> end_confess

stage end_mended
  end success
  say "THE VIGIL IS LIT WITH BEESWAX. {GIVER} WALKED TO {HIVE} THE NEXT MORNING WITH A PURSE."
  journal "THE VIGIL OF {HOME} WAS LIT WITH BEESWAX. {GIVER} AND {KEEPER} SPEAK AGAIN."
  do reward fair
  do deed charity
  do mark candles_mended
  do remember keeper "{GIVER} CAME, ELEVEN YEARS LATE, AND READS TO ME IN THE EVENINGS. BADLY. I DON'T COMPLAIN."
  do remember giver "YOU SENT ME DOWN THAT ROAD. I'D BE STILL POURING TALLOW AND SULKING WITHOUT YOU."
  do fact "{GIVER} OF {HOME} WALKED TO {HIVE} AND MADE PEACE WITH {KEEPER}, AFTER ELEVEN YEARS OF SILENCE OVER A BRASS MOULD."

stage end_owes
  end success
  say "THE VIGIL BURNS BEESWAX. THE DEBT STANDS, BUT NOW SOMEBODY MEANS TO PAY IT."
  journal "THE VIGIL OF {HOME} BURNED BEESWAX. {GIVER} MEANS TO PAY {KEEPER} AT THE NEXT VIGIL."
  do reward small
  do remember giver "ELEVEN BLOCKS OF WAX. I'LL CARRY THEM UP MYSELF. THANK YOU."
  do fact "{GIVER} OF {HOME} MEANS TO PAY {KEEPER} ELEVEN BLOCKS OF WAX AT THE NEXT VIGIL, IN PERSON."

stage end_confess
  end success
  say "{GIVER} TAKES A PURSE TO {HIVE} AT DAWN. THE CELLAR LATCH GETS A NEW BOLT."
  journal "{GIVER} PAID {KEEPER} FOR THE WAX YOU STOLE. NEITHER OF THEM WILL SAY HOW IT CAME TO BE TAKEN."
  do reward small
  do remember giver "YOU WERE WRONG. I WAS WORSE. SHE LET ME IN, THOUGH."
  do fact "{GIVER} OF {HOME} PAID {KEEPER} FOR WAX SHE NEVER SOLD. NEITHER WILL SAY HOW IT CAME TO BE TAKEN."

stage end_hush
  end success
  say "THE VIGIL IS LIT WITH STOLEN WAX. {GIVER} DOESN'T KNOW. {KEEPER} DOES."
  journal "THE VIGIL OF {HOME} WAS LIT WITH STOLEN WAX. THE TWO OF THEM ARE FURTHER APART THAN EVER."
  do reward small
  do deed lied
  do remember giver "HONEST WAX, BOUGHT AND PAID FOR. IT BURNED BEAUTIFULLY."
  do fact "{KEEPER} BELIEVES {GIVER} HAD HER CELLAR ROBBED. {GIVER} BELIEVES THE WAX WAS BOUGHT."

stage tallow
  end fail
  say "YOU TURN FROM THE HIVES. {KEEPER} DOESN'T CALL AFTER YOU. IN {HOME} THE VIGIL BURNS TALLOW, AND THE CHANDLER SAYS NOTHING."
  journal "YOU CAME BACK FROM {HIVE} WITHOUT WAX. THE VIGIL OF {HOME} BURNED TALLOW."
  do remember keeper "THE STRANGER WALKED OFF. EVERYONE DOES, IN THE END."
  do remember giver "YOU CAME BACK WITH NOTHING BUT THE SMELL OF BEES. TALLOW IT IS."
  do fact "THE VIGIL OF {HOME} BURNED TALLOW. {KEEPER} OF {HIVE} STILL KEEPS HER CELLAR SHUT."

stage refuse
  end fail
  journal "YOU TOLD THE CHANDLER OF {HOME} THAT TALLOW WOULD DO."
  do remember giver "TALLOW. YOU SAID TALLOW WOULD DO. THE WHOLE TOWN SMELLS OF MUTTON."
)DSL",

R"DSL(
script rq_ledger
title THE FORGED LEDGER
archetype town request: a forged debt
tier 2
family request
settle town
once site
cooldown 6
gossip "SOME FOLK OWE A LOAN THEY NEVER TOOK."
gossip "HE READS THE DEBTS OUT EVERY MORNING."
hook npc merchant
pitch "YOU LOOK LIKE YOU OWE SOMEONE."
hint "HAVE YOU HEARD HIM YET? EVERY MORNING, IN THE MARKET, MY NAME AND A SUM. I'VE NEVER BORROWED A COIN IN MY LIFE."
role giver giver
role home site home
role collector person male at home
role camp site camp near
role boss foe male at camp

stage start
  talk giver
  say "I NEVER BORROWED A COIN FROM HIM, AND HE READS MY DEBT OUT EVERY MORNING. ONE HUNDRED AND TWENTY, FOR A NEW ROOF. MY MARK IS IN HIS BOOK, BUT I NEVER MADE IT. I CAN'T READ, AND HE KNOWS IT."
  opt "I'LL SPEAK TO HIM." -> collector
  opt "NOT MY BUSINESS." -> refuse

stage collector
  talk collector
  say "THE BOOK IS THE BOOK, FRIEND. A MARK, A SUM, A PROMISE. FORTY NAMES IN HERE AND FORTY MARKS. PAY, OR I SIT IN THIS MARKET TILL WINTER AND READ THEM ALL OUT, LOUDER EACH DAY."
  opt "SHOW ME THE PAGE." -> page
  opt "I'LL PAY THE DEBT. [120 GOLD]" if gold 120 -> paid
  opt "LEAVE THESE PEOPLE BE." check level 5 -> scare else jeer

stage page
  talk collector
  say "LOOK ALL YOU LIKE. IT'S A MARK. FOLK SIGN FUNNY; THE SCRIBE TIDIES THEM. THE FAIR COPY'S WITH MY MASTER, AND HE KEEPS IT UNDER HIS BED AT {CAMP}, WITH FIFTEEN SWORDS FOR COMPANY. WALK AWAY. YOU'RE NEW. I LIKE YOU."
  opt "I'LL FETCH THAT FAIR COPY." -> go_camp
  opt "I'LL PAY THE DEBT. [120 GOLD]" if gold 120 -> paid

stage jeer
  talk collector
  say "A STARE AND A SWORD? CUTE. THE REAL BOOK'S AT {CAMP}, AND FIFTEEN SWORDS SIT ROUND IT. GO ON, BE THE NEXT FOOL WHO TRIES. I'LL READ YOUR NAME OUT TOO."
  opt "I'LL SHOW YOU A FOOL." -> go_camp

stage go_camp
  goal goto camp
  then fight
  journal "THE COLLECTOR'S MASTER KEEPS THE FAIR COPY OF THE LEDGER AT {CAMP}. THE MARKS IN IT ARE FORGED. PROVE IT FOR {GIVER}."

stage fight
  goal slay boss
  then book
  journal "{BOSS}, THE MASTER AT {CAMP}, HOLDS THE FALSE LEDGER. END HIM."

stage book
  goal fetch "THE MASTER LEDGER" in camp
  then read
  journal "FIND THE MASTER LEDGER AT {CAMP} AND TAKE IT TO {GIVER} IN {HOME}. SOMEONE WILL HAVE TO READ IT ALOUD."

stage read
  talk giver
  say "READ IT TO ME. EVERY NAME. ...FIFTY NAMES? HIS BOOK HAS FORTY. TEN WITH A LINE THROUGH THEM, PAID. TEN FOLK PAID DEBTS THEY NEVER OWED. AND YOU SAY ONE HAND MADE EVERY MARK, ALL FIFTY, IN THE SAME INK?"
  do take "THE MASTER LEDGER"
  opt "BURN IT. NOBODY OWES ANYTHING." -> burn
  opt "THE REEVE MUST SEE IT." -> reeve
  opt "THE COLLECTOR WILL PAY WELL FOR IT." -> sold

stage burn
  end success
  say "THE PAGES CURL. FIFTY DEBTS GO UP IN SMOKE, AND THE TEN WHO PAID WILL NEVER SEE A COIN OF IT BACK."
  journal "THE FORGED LEDGER BURNED IN {HOME}. FORTY FALSE DEBTS ARE GONE; TEN WERE PAID AND ARE LOST."
  do reward fair
  do hide collector
  do mark ledger_burned
  do remember giver "THE ONE WHO BURNED THE BOOK. THE MARKET IS QUIET IN THE MORNINGS NOW."
  do fact "FIFTY FORGED DEBTS IN {HOME} WENT UP IN SMOKE. THE TEN WHO PAID ARE OWED NOTHING NOW, AND THE COLLECTOR WAS NEVER SEEN AGAIN."

stage reeve
  end success
  say "THE LEDGER HANGS IN THE GUARDHOUSE. THE REEVE VOIDS ALL FIFTY AND REPAYS THE TEN FROM THE DEAD MASTER'S CHEST."
  journal "THE FORGED LEDGER IS WITH THE REEVE OF {HOME}. THE DEBTS ARE VOID BY LAW AND THE TEN WHO PAID ARE REPAID."
  do reward fair
  do fame 1
  do hide collector
  do mark ledger_kept
  do remember giver "THE LAW READ ME MY OWN NAME, THEN STRUCK IT OUT. I CRIED IN FRONT OF THE REEVE."
  do fact "THE FORGED LEDGER OF {HOME} IS IN THE GUARDHOUSE. FIFTY DEBTS ARE VOID BY LAW AND THE TEN WHO PAID WILL BE REPAID."

stage sold
  end fail
  say "THE COLLECTOR COUNTS OUT ONE HUNDRED AND SMILES. THE MARKS STAY IN A BOOK, SOMEWHERE ELSE."
  journal "YOU SOLD THE MASTER LEDGER BACK TO THE COLLECTOR. THE FORGED DEBTS OF {HOME} STAND."
  do gold 100
  do deed bribed
  do remember giver "YOU SOLD MY NAME BACK TO HIM. I HEAR IT EVERY MORNING."
  do fact "A STRANGER SOLD THE MASTER LEDGER BACK TO THE COLLECTOR IN {HOME}. THE FORGED DEBTS STAND."

stage paid
  end success
  say "HE TAKES YOUR GOLD, STRIKES ONE NAME AND LEAVES. THE FORGERY STILL HOLDS THIRTY-NINE OTHERS."
  journal "YOU PAID A DEBT {GIVER} NEVER OWED. THIRTY-NINE OTHER FORGED DEBTS STAND IN {HOME}."
  do gold -120
  do xp 60
  do deed charity
  do hide collector
  do remember giver "A DEBT THAT NEVER WAS, AND YOU PAID IT. I DON'T KNOW WHETHER TO KISS YOU OR SHAKE YOU."
  do fact "A STRANGER PAID A FALSE DEBT IN {HOME}. THE FORGED BOOK STILL HOLDS THIRTY-NINE OTHERS."

stage scare
  end success
  say "HE SNAPS THE BOOK SHUT AND WALKS OUT OF THE MARKET. THE STALL IS SAFE. THE BOOK ISN'T."
  journal "THE COLLECTOR LEFT {HOME} WITH HIS BOOK. THE FORTY NAMES IN IT GO WITH HIM."
  do reward small
  do hide collector
  do remember giver "HE'S GONE. HE'LL BE BACK FOR SOMEONE ELSE. THEY ALWAYS ARE."
  do fact "A COLLECTOR LEFT {HOME} WHEN HIS BOOK WAS CHALLENGED. FORTY NAMES ARE STILL IN IT."

stage refuse
  end fail
  journal "YOU KEPT OUT OF THE DEBT COLLECTOR'S BUSINESS IN {HOME}."
  do remember giver "NOBODY WANTS TO BE SEEN WITH ME. I DON'T BLAME THEM."
)DSL",

R"DSL(
script rq_measure
title THE CROOKED DISH
archetype town request: a false measure
tier 2
family request
settle town
once site
cooldown 6
gossip "THE MILLER WON'T MILL FOR THE MARKET."
gossip "STONES IN THE SACKS, THE MILLER SAYS."
hook npc farmer
pitch "WHY ARE YOUR SACKS STILL FULL?"
hint "THE MILLER TURNED MY SACKS AWAY AGAIN. ONE BAD HARVEST, AND EVERY GROWER IN THE MARKET IS A CHEAT."
role giver giver
role home site home
role twin site town near
role baker person female at home
role miller person male at home
role hand person male at home
role weigher person male at twin

stage start
  talk giver
  say "THE MILLER TURNED OUR SACKS AWAY. HE SAYS WE PACK THEM WITH STONES. ONE BAD HARVEST AND WE'RE ALL CHEATS. NO FLOUR, NO BREAD, AND {BAKER} AT THE OVEN HAS BEEN CRYING OVER HER SUMS. FIND THE TRUTH."
  opt "I'LL SPEAK TO THE MILLER." -> miller
  opt "SOUNDS LIKE A MILLER'S PROBLEM." -> refuse

stage miller
  talk miller
  say "I WEIGH EVERY SACK, THEN TAKE A TENTH BY THE TOWN'S SEALED DISH. AND THE TOLL BIN FILLS A THUMB SHORT. SO SOMEONE PACKS STONES. SIMPLE. PROVE MY DISH TRUE AND I'LL EAT MY WORDS. FETCH ME A TRUE ONE."
  opt "I'LL BRING ONE FROM {TWIN}." -> go_twin
  opt "YOU'RE SHORT OF GRAIN, THAT'S ALL." -> insist

stage insist
  talk miller
  say "SHORT OF GRAIN, YES. NOT OF EYES. FETCH A TRUE DISH FROM {TWIN} AND SHUT ME UP WITH IT, IF YOU CAN."
  opt "I WILL." -> go_twin

stage go_twin
  goal goto twin
  then twin_talk
  journal "THE MILLER OF {HOME} WANTS A TRUE TOLL DISH FROM {TWIN} TO TEST HIS OWN. ASK {WEIGHER}, THE WEIGHER THERE."

stage twin_talk
  talk weigher
  say "A TRUE DISH? WE CAST SIX; TAKE ONE. FILL BOTH, TIP BOTH, COMPARE THE HEAPS. AND LOOK UNDER THE RIM: BRASS DUST THERE MEANS SOMEBODY'S BEEN AT IT WITH A FILE."
  opt "THANK YOU." -> back
  opt "HOW WOULD SOMEONE CHEAT A DISH?" -> twin_explain

stage twin_explain
  talk weigher
  say "A FALSE BOTTOM. WOOD, GLUE AND SAWDUST, SANDED SMOOTH. YOU'D NEED THE DISH IN YOUR HANDS A WHOLE NIGHT, SO ONLY SOMEONE WITH THE KEY TO WHERE IT'S KEPT. NOW GO. THE TRUE ONE'S YOURS."
  opt "I'LL BRING BACK THE RESULT." -> back

stage back
  goal goto home
  then compare
  do give "A TRUE TOLL DISH" sigil
  journal "TAKE THE TRUE TOLL DISH FROM {TWIN} BACK TO THE MILLER IN {HOME}."

stage compare
  talk miller
  say "SAME GRAIN, TWO DISHES. WATCH. ...MINE HOLDS A THUMB LESS. GLUE AND SAWDUST IN THE BOTTOM, SANDED SMOOTH. TWO KEYS TO MY SHED: MINE, AND {HAND}'S. {HAND} SWEEPS UP MY SCRAPS FOR THE BAKER."
  do take "A TRUE TOLL DISH"
  opt "THEN I'LL ASK {HAND}." -> hand
  opt "YOU MADE IT YOURSELF." -> rude

stage rude
  talk miller
  say "AND ROB MYSELF? I'D BE A FOOL, AND A POOR ONE. EVERY SACK IN THIS MARKET HAS BEEN SHORT, MINE AS WELL. THE OTHER KEY IS {HAND}'S. GO ON."
  opt "I'LL ASK HIM." -> hand

stage hand
  talk hand
  say "I SWEEP. THAT'S ALL. ...ALL RIGHT. MY MISTRESS CRIED OVER THE TOLL SUMS ALL WINTER. I GLUED THE DISH SO HER SACKS WOULD COST LESS. I ONLY MEANT HER SACKS. NOT EVERYONE'S. I DIDN'T THINK IT THROUGH."
  opt "WE TELL THE MILLER TOGETHER." -> expose
  opt "SAY A THIEF COPIED YOUR KEY." check level 4 -> cover else caught
  opt "RUN. LEAVE TOWN TONIGHT." -> run

stage expose
  talk miller
  say "{HAND} FIXED MY DISH FOR HER? FOR A CRYING WOMAN? ...I'D HAVE BEEN TEMPTED TOO. HE SWEEPS UNPAID TILL IT'S SQUARE, AND EVERY SACK GETS WEIGHED BY A NEW DISH IN FRONT OF ITS OWNER."
  opt "THAT'S FAIR. NOW THE BAKER." -> baker_after

stage baker_after
  talk baker
  say "{HAND} SHAVED A DISH FOR ME? I CRIED OVER THOSE SUMS AND HE SAW IT. I ASKED FOR THE TRUTH; THERE IT IS, AND IT'S MINE. I'LL PAY WHAT HE OWES THE MILLER. THE LAD STAYS WITH ME."
  opt "HE MEANT WELL." -> end_expose

stage end_expose
  end success
  say "THE MILLER MILLS FOR THE MARKET AGAIN. {HAND} SWEEPS, AND {BAKER} PAYS THE LOSS."
  journal "THE MILLER OF {HOME} FOUND WHO FIXED HIS TOLL DISH. THE MARKET MILLS AGAIN; THE BAKER PAID THE LOST GRAIN."
  do reward fair
  do mark dish_exposed
  do remember baker "I ASKED FOR THE TRUTH, AND IT WAS MINE. I'M GLAD YOU FOUND IT ANYWAY."
  do remember hand "YOU SAID IT TO HIS FACE WITH ME. I COULDN'T HAVE DONE IT ALONE."
  do remember giver "THE MILLER TOOK MY SACKS THIS MORNING AND APOLOGISED. I NEARLY DROPPED THEM."
  do fact "{HAND} FIXED THE MILLER'S TOLL DISH IN {HOME} FOR HIS MISTRESS. THE BAKER PAID THE LOST GRAIN AND THE MARKET MILLS AGAIN."

stage cover
  end success
  say "THE MILLER CURSES LOCKSMITHS AND CHANGES HIS LOCK. A NEW DISH IS CAST. NOBODY PAYS FOR THE OLD."
  journal "THE MILLER OF {HOME} BELIEVES A THIEF COPIED A KEY AND FIXED HIS DISH. {HAND} KNOWS BETTER, AND SO DO YOU."
  do reward small
  do deed lied
  do mark dish_hush
  do remember hand "YOU LIED FOR ME. I'LL NEVER TOUCH A DISH AGAIN. I SWEAR IT."
  do remember baker "THEY SAY A THIEF FIXED THE MILLER'S DISH. A THIEF WHO HELPED ME. HOW ODD."
  do fact "THE MILLER OF {HOME} BELIEVES A THIEF WITH A COPIED KEY FIXED HIS TOLL DISH. HE HAS A NEW LOCK AND A NEW DISH."

stage caught
  end fail
  say "THE MILLER STUDIES THE LOCK: NO SCRATCH, NO WAX, NOTHING FORCED. HE LOOKS AT {HAND}, THEN AT {BAKER}'S OVEN, AND HE WALKS OFF TO FETCH THE REEVE."
  journal "THE LIE FAILED. THE MILLER OF {HOME} BLAMES THE BAKER'S HOUSEHOLD, AND THE MARKET HAS SPLIT IN TWO."
  do remember baker "YOU HANDED HIM MY HOUSE ON A PLATE. WITH A LIE FOR GARNISH."
  do remember hand "THE LIE WAS WORSE THAN THE TRUTH. I KNEW IT AS YOU SAID IT."
  do fact "THE MILLER OF {HOME} FOUND HIS TOLL DISH FIXED AND BLAMED THE BAKER'S HOUSEHOLD. THE MARKET SPLIT IN TWO."

stage run
  end success
  say "{HAND} VANISHES BEFORE DAWN. THE MILLER CASTS A NEW DISH, CALLS HIM A THIEF TO ANYONE WHO LISTENS, AND NEVER LEARNS WHY HE DID IT."
  journal "{HAND} FLED {HOME}. THE MILLER KNOWS WHO FIXED HIS DISH, BUT NOT WHY. THE MARKET MILLS AGAIN."
  do reward small
  do hide hand
  do remember baker "MY LAD RAN OFF IN THE NIGHT. THE MILLER CALLS HIM A THIEF. I CAN'T THINK WHY HE'D DO IT."
  do fact "{HAND} FLED {HOME} THE NIGHT THE MILLER'S TOLL DISH WAS TESTED. THE MILLER CALLS HIM A THIEF AND NEVER LEARNED WHY."

stage refuse
  end fail
  journal "YOU LEFT THE GROWERS AND THE MILLER OF {HOME} TO THEIR QUARREL."
  do remember giver "THEN WE'LL EAT THE SEED CORN. THANK YOU FOR NOTHING."
)DSL",

R"DSL(
script rq_players
title THE STRANDED PLAYERS
archetype town request: a play without an ending
tier 2
family request
settle town
once site
cooldown 6
gossip "BANDITS TOOK THE PLAYERS' TRUNK."
gossip "THE BUTCHER AND THE BAKER STILL FEUD."
hook npc innkeeper
pitch "WHO IS SINGING UPSTAIRS?"
hint "THEY'RE STILL UPSTAIRS, EATING MY BREAD AND STEALING MY HEART. NINE NIGHTS. NOT A COPPER BETWEEN THEM."
role giver giver
role home site home
role camp site camp near
role lead person female at home
role fool person male at home

stage start
  talk giver
  say "NINE NIGHTS, NOT A COPPER, AND THEY SING. BANDITS TOOK THE TROUPE'S COSTUME TRUNK. NO TRUNK, NO PLAY, NO COIN. AND SINCE THE FLOOD TOOK THE BAKER'S OVEN AND THE BUTCHER WOULDN'T LEND HIS, THOSE TWO WON'T SIT IN ONE ROOM. A GOOD PLAY MIGHT FIX BOTH."
  opt "I'LL TALK TO THE PLAYERS." -> lead
  opt "THROW THEM OUT." -> out

stage lead
  talk lead
  say "THE KING WHO LAUGHED: OUR BEST PLAY. THEY TOOK THE TRUNK TO {CAMP}, AND IN IT THE CROWN, THE ROBES AND MY MOTHER'S PROMPT-BOOK. WITHOUT THEM WE'RE JUST PEOPLE SHOUTING IN AN INN."
  opt "I'LL FETCH THE TRUNK." -> seek
  opt "WHAT'S SO PRECIOUS ABOUT A BOOK?" -> book

stage book
  talk lead
  say "MY MOTHER COPIED IT BY HAND. EVERY CUE, EVERY LINE SHE LOVED. THE LAST LEAF HAS THE KING'S FATE ON IT. I'D RATHER LOSE THE CROWN. AND KEEP MY FOOL AWAY FROM WINE; HE FORGETS HIS LINES WHEN THE ROOM IS FULL."
  opt "I'LL FETCH THE TRUNK." -> seek

stage seek
  goal goto camp
  then fight
  journal "THE TROUPE'S COSTUME TRUNK WAS TAKEN TO {CAMP}. GET IT BACK FOR {LEAD}, WHO WAITS AT THE INN OF {HOME}."

stage fight
  goal kill 3 bandit in camp
  then loot
  journal "CLEAR THE BANDITS AT {CAMP}, THEN FIND THE PLAYERS' TRUNK."

stage loot
  goal fetch "THE COSTUME TRUNK" in camp
  then return
  journal "FIND THE PLAYERS' COSTUME TRUNK AT {CAMP}."

stage return
  goal goto home
  then burnt
  journal "TAKE THE COSTUME TRUNK BACK TO {LEAD} AT THE INN OF {HOME}."

stage burnt
  talk lead
  say "THE CROWN! THE ROBES! ...THE BOOK? THE LAST LEAF IS ASH. THEY LIT A FIRE WITH IT. THE KING'S FATE WAS ON THAT LEAF. NO ENDING, AND WE'RE BACK TO SHOUTING."
  do take "THE COSTUME TRUNK"
  opt "ASK THE FOOL. HE HAS OPINIONS." -> fool
  opt "I'LL PAY THE NINE NIGHTS. [60 GOLD]" if gold 60 -> paid

stage fool
  talk fool
  say "SHE KEEPS THINGS; SHE CAN'T END THEM. THE KING NEEDS ONE LAST SPEECH. I COULD MAKE IT UP ON THE NIGHT, I SUPPOSE. LAST TIME I DID THAT I SANG A SONG ABOUT CHEESE FOR AN HOUR. OR YOU PICK. WHAT HAPPENS TO HIM?"
  opt "HE DIES, AND THE TOWN WEEPS." -> night_dies
  opt "HE LAUGHS, AND THE TOWN LAUGHS." -> night_laughs
  opt "HE FORGIVES, AND THE TOWN LISTENS." -> night_forgives
  opt "LET THE FOOL MAKE IT UP." check level 4 -> night_lucky else night_flop

stage night_dies
  goal wait 1
  then after_dies
  say "THE TAPROOM WILL FILL TO THE DOOR. COME BACK IN THE MORNING."
  journal "THE PLAYERS PERFORM THE KING WHO LAUGHED TONIGHT, WITH THE ENDING YOU CHOSE: HE DIES. WAIT FOR THE MORNING."

stage night_laughs
  goal wait 1
  then after_laughs
  say "THE TAPROOM WILL FILL TO THE DOOR. COME BACK IN THE MORNING."
  journal "THE PLAYERS PERFORM THE KING WHO LAUGHED TONIGHT, WITH THE ENDING YOU CHOSE: HE LAUGHS. WAIT FOR THE MORNING."

stage night_forgives
  goal wait 1
  then after_forgives
  say "THE TAPROOM WILL FILL TO THE DOOR. COME BACK IN THE MORNING."
  journal "THE PLAYERS PERFORM THE KING WHO LAUGHED TONIGHT, WITH THE ENDING YOU CHOSE: HE FORGIVES. WAIT FOR THE MORNING."

stage night_lucky
  goal wait 1
  then after_lucky
  say "THE TAPROOM WILL FILL TO THE DOOR. HE SWEARS HE'S SOBER. COME BACK IN THE MORNING."
  journal "THE FOOL WILL MAKE UP THE KING'S LAST SPEECH ON THE NIGHT. WAIT FOR THE MORNING AND HOPE."

stage night_flop
  goal wait 1
  then after_flop
  say "THE TAPROOM WILL FILL TO THE DOOR. SOMEONE HAS GIVEN HIM WINE ALREADY. COME BACK IN THE MORNING."
  journal "THE FOOL WILL MAKE UP THE KING'S LAST SPEECH ON THE NIGHT, AND SOMEONE HAS GIVEN HIM WINE. WAIT FOR THE MORNING."

stage after_dies
  talk lead
  say "THE KING DIED, AND NOBODY BREATHED. THEN THE BUTCHER STOOD UP, AND THEN THE WHOLE ROOM. I'VE NEVER BEEN APPLAUDED LIKE THAT. MOTHER WOULD HAVE HATED THE ENDING. AND CRIED AT IT."
  opt "SOME ENDINGS SHOULD HURT." -> end_dies

stage after_laughs
  talk fool
  say "YOU WROTE A LAUGH AND I PLAYED IT. THE BAKER SPAT BEER ALL OVER THE BUTCHER, AND THEN THE TWO OF THEM LAUGHED AT THEMSELVES TILL THEY WEPT. A FEUD CAN'T SURVIVE A WET COAT."
  opt "SOME ENDINGS SHOULD SING." -> end_laughs

stage after_forgives
  talk giver
  say "THE KING FORGAVE, AND THE ROOM WENT STILL. THEN THE BUTCHER AND THE BAKER SHOOK HANDS OVER THE OVEN AND THE FLOOD AND ALL OF IT. I'VE WANTED THAT FOR TWO YEARS. NOBODY EVEN ASKED FOR A DISCOUNT."
  opt "SOME ENDINGS SHOULD HEAL." -> end_forgives

stage after_lucky
  talk giver
  say "HE MADE IT UP, AND IT WORKED. THE KING CROWNED HIS OWN FOOL AND WALKED OFF INTO THE KITCHEN. THE BAKER SAID IT WAS THE TRUEST THING SHE'D SEEN. NOBODY KNOWS WHAT IT MEANT. EVERYONE PAID."
  opt "LUCK IS A KIND OF ART." -> end_lucky

stage after_flop
  talk giver
  say "HE FORGOT THE KING HALFWAY THROUGH AND SANG ABOUT CHEESE. SOMEONE THREW A BOOT. HALF THE ROOM LEFT, HALF DIDN'T PAY, AND THE TROUPE SLIPPED OUT AT DAWN WITH THEIR TRUNK."
  opt "I SHOULD HAVE PICKED." -> end_flop

stage end_dies
  end success
  say "THE FULL TAPROOM PAID THE NINE NIGHTS IN COPPER AND TEARS. THE TROUPE LEAVES AT DAWN WITH A NEW LAST LEAF."
  journal "THE KING WHO LAUGHED WAS PLAYED IN {HOME}. THE KING DIED, THE TAPROOM WEPT AND PAID THE NINE NIGHTS."
  do reward fair
  do fame 1
  do remember giver "THE NIGHT THE KING DIED. PEOPLE STILL ASK ME ABOUT IT. I CRY EVERY TIME."
  do remember lead "YOU WROTE US A GRAND ENDING. I'VE COPIED IT IN, IN MY MOTHER'S HAND AS WELL AS I CAN."
  do fact "A TROUPE PLAYED THE KING WHO LAUGHED IN {HOME}. THE KING DIED, THE TAPROOM WEPT AND PAID THE NINE NIGHTS."

stage end_laughs
  end success
  say "THE FULL TAPROOM PAID IN COPPER AND HICCUPS. THE KING WHO LAUGHED IS A HIT AT LAST."
  journal "THE KING WHO LAUGHED WAS PLAYED IN {HOME}. THE KING LAUGHED, THE TOWN DID TOO, AND THE NINE NIGHTS WERE PAID."
  do reward fair
  do remember giver "THE NIGHT THE KING LAUGHED. EAT FREE, ANY NIGHT YOU LIKE."
  do remember fool "YOU WROTE A LAUGH. I SING IT IN EVERY TOWN NOW. YOUR NAME'S IN THE THIRD VERSE."
  do fact "A TROUPE PLAYED THE KING WHO LAUGHED IN {HOME}. THE KING LAUGHED, THE TOWN DID TOO AND THE TAPROOM PAID THE NINE NIGHTS."

stage end_forgives
  end success
  say "THE FULL TAPROOM PAID, AND THE OLD FEUDERS SHARED A TABLE. THE TROUPE WAVES FROM THE ROAD."
  journal "THE KING WHO LAUGHED WAS PLAYED IN {HOME}. THE KING FORGAVE, AND THE BUTCHER AND THE BAKER MADE PEACE."
  do reward fair
  do deed mercy
  do remember giver "THE NIGHT THE KING FORGAVE. THE BUTCHER LENDS HIS OVEN ON SUNDAYS NOW."
  do remember lead "YOU WROTE US THE MERCIFUL ONE. MOTHER WOULD HAVE LIKED THAT ONE BEST."
  do fact "A TROUPE PLAYED THE KING WHO LAUGHED IN {HOME}. THE KING FORGAVE, TWO OLD ENEMIES SHOOK HANDS AND THE NINE NIGHTS WERE PAID."

stage end_lucky
  end success
  say "THE FULL TAPROOM PAID THE NINE NIGHTS, AND NOBODY CAN AGREE WHAT THE ENDING MEANT. THE FOOL REFUSES TO EXPLAIN."
  journal "THE FOOL MADE UP THE KING'S LAST SPEECH IN {HOME}, AND IT WORKED. THE NINE NIGHTS ARE PAID."
  do reward fair
  do remember fool "I MADE IT UP AND IT WORKED. DON'T TELL HER I WAS TERRIFIED."
  do fact "A TROUPE PLAYED THE KING WHO LAUGHED IN {HOME} WITH AN ENDING THE FOOL MADE UP ON THE NIGHT. NOBODY AGREES WHAT IT MEANT."

stage end_flop
  end fail
  say "THE FOOL FORGOT THE KING AND THE TAPROOM EMPTIED. THE NINE NIGHTS STAY UNPAID."
  journal "THE PLAY FLOPPED IN {HOME}. THE TROUPE LEFT AT DAWN; THE INN IS NINE NIGHTS OUT OF POCKET."
  do remember giver "YOU LEFT THE ENDING TO A FOOL WITH A CUP IN HIS HAND."
  do fact "A TROUPE FLOPPED IN {HOME} AND LEFT AT DAWN. THE INNKEEPER IS NINE NIGHTS OUT OF POCKET."

stage paid
  end success
  say "THE TROUPE LEAVES AT DAWN WITH THEIR TRUNK AND NO ENDING. THEY BOW TO YOU FROM THE ROAD."
  journal "YOU PAID THE PLAYERS' NINE NIGHTS AT THE INN OF {HOME}. THE PLAY WAS NEVER PERFORMED."
  do gold -60
  do xp 50
  do deed charity
  do remember giver "YOU PAID THE PLAYERS' BILL. I'D HAVE LIKED THE PLAY, THOUGH."
  do remember lead "WE'LL FIND AN ENDING SOMEWHERE ON THE ROAD. WE ALWAYS DO."
  do fact "A STRANGER PAID A TROUPE'S NINE NIGHTS AT THE INN OF {HOME}. THE PLAY WAS NEVER PERFORMED."

stage out
  end fail
  say "THE PLAYERS LEAVE IN THE RAIN, SINGING."
  journal "THE PLAYERS WERE THROWN OUT OF THE INN OF {HOME}."
  do remember giver "THEY WERE GOOD COMPANY. I MISS THE SINGING, IF NOT THE BILL."
  do fact "A TROUPE WAS THROWN OUT OF THE INN IN {HOME} AND LEFT IN THE RAIN."
)DSL",

R"DSL(
script rq_wagon
title THE CARTER'S GIFT
archetype town request: a kind theft
tier 2
family request
settle town
once site
cooldown 6
gossip "THE IRON WAGON CAME BACK EMPTY."
gossip "NO TRACKS AT THE FORD, THEY SAY."
hook npc smith
pitch "YOUR FORGE IS COLD."
hint "NO IRON, NO WORK. THE CARTER SAYS BANDITS TOOK IT AT THE FORD. THE FORD SAYS NOTHING; IT HAS NO TRACKS."
role giver giver
role home site home
role vill site village near
role carter person male at home
role vsmith person male at vill
var route 0

stage start
  talk giver
  say "THREE WEEKS OF IRON AND NOT A BAR REACHED MY FORGE. THE CARTER SAYS BANDITS TOOK IT AT THE FORD. THE FORD HAS NO CART TRACKS, NONE. AND HE'S HONEST. THAT'S WHAT SCARES ME."
  opt "THEN I'LL HAVE A WORD WITH THE CARTER." -> carter
  opt "BUY IRON IN THE NEXT TOWN." -> refuse

stage carter
  talk carter
  say "BANDITS TOOK THE IRON AT THE FORD. FIVE OF THEM. I RAN, AND I'M NOT PROUD OF IT. DON'T GO LOOKING. THEY'LL KILL YOU."
  opt "YOU SOLD IT. THERE WERE NO BANDITS." -> accuse
  opt "THERE ARE NO TRACKS AT THE FORD." check level 3 -> confess else hold
  opt "I KNOW WHAT A FAILED FIELD DOES." if bg farmhand -> confess
  opt "I'LL LOOK ANYWAY." -> trail

stage accuse
  end fail
  say "YOU TELL THE SMITH THE CARTER SOLD THE IRON. HE IS DISMISSED BY NIGHTFALL AND GOES WITHOUT A WORD. THE IRON IS NEVER TRACED, AND THE FORGE BUYS AT DOUBLE COST."
  journal "YOU STOPPED BEFORE THE TRUTH. THE CARTER OF {HOME} WAS DISMISSED AND THE IRON WAS NEVER TRACED."
  do remember giver "HE'S GONE AND THE IRON'S GONE. YOU WERE SURE. I WISH I WERE."
  do fact "THE CARTER OF {HOME} WAS DISMISSED OVER THE TOWN'S MISSING IRON. NOBODY EVER LEARNED WHERE IT WENT."

stage confess
  talk carter
  say "...NO. THERE WERE NO BANDITS. I TOOK IT TO {VILL}, EVERY BAR. NINE FARMS THERE HADN'T PLANTED, AND THEIR SMITH HAD NO IRON FOR PLOUGHSHARES. I GAVE IT AWAY. I'LL SWEAR TO THAT BEFORE ANYONE."
  opt "THEN I'LL GO AND SEE." -> trail

stage hold
  talk carter
  say "THEN THE BANDITS WENT ANOTHER ROAD. STOP ASKING. PLEASE."
  opt "I'LL CHECK THE OTHER ROADS." -> trail

stage trail
  goal goto vill
  then smith
  journal "THE CARTER'S RUTS LEAVE THE ROAD FOR {VILL}, NOT THE FORD. LOOK THERE FOR THE IRON OF {HOME}."

stage smith
  talk vsmith
  say "IRON? AYE, A CARTER BROUGHT IT. I'VE STRUCK SIX SHARES; SIX FARMS PLOUGH. THE LAST BARS WILL MAKE THREE MORE FOR THE OTHER THREE. TAKE THE BARS AND THREE FARMS STARVE. TAKE THE SHARES AND SIX DO."
  opt "THE TOWN NEEDS THE BARS BACK." -> take_bars
  opt "I'LL TAKE THE SHARES FROM THE FIELDS." -> take_shares
  opt "FORGE FOR THE TOWN TOO, IN TRADE." check level 3 -> deal else hard
  opt "LET ME WORK YOUR FORGE." if bg blacksmith -> deal
  opt "I'LL SAY BANDITS TOOK IT, THEN." -> cover

stage hard
  talk vsmith
  say "I FORGE FOR HUNGRY FARMERS FIRST. THE TOWN CAN SPARE A WEEK. IT'S GOT A MARKET; WE'VE GOT MUD."
  opt "THEN I'LL TAKE THE BARS." -> take_bars
  opt "THEN I'LL TAKE THE SHARES." -> take_shares
  opt "THEN I'LL SAY BANDITS TOOK IT." -> cover

stage take_bars
  goal goto home
  then report
  do set route 1
  do give "THE TOWN'S UNWORKED BARS" ingot heavy
  journal "TAKE THE UNWORKED BARS BACK TO {GIVER} IN {HOME}. THREE FARMS NEAR {VILL} WILL NOT PLANT."

stage take_shares
  goal goto home
  then report
  do set route 2
  do give "SIX PLOUGHSHARES" ingot heavy
  journal "YOU TOOK SIX PLOUGHSHARES OUT OF THE FIELDS OF {VILL}. TAKE THEM BACK TO {GIVER} IN {HOME}."

stage deal
  goal goto home
  then report
  do set route 3
  do give "A VILLAGE SMITH'S PLEDGE" scroll
  do secret vill
  journal "{VILL} KEEPS ITS SHARES, MAKES THREE MORE AND PLEDGES TO FORGE FOR {HOME}. TELL {GIVER}."

stage cover
  goal goto home
  then report
  do set route 4
  journal "{VILL} KEEPS THE IRON AND PLOUGHS. TELL {GIVER} IN {HOME} IT WAS BANDITS AFTER ALL."

stage report
  talk giver
  say "WELL? WHERE IS MY IRON?"
  opt "HERE ARE YOUR BARS." if is route 1 -> end_bars
  opt "HERE ARE SIX PLOUGHSHARES." if is route 2 -> end_shares
  opt "{VILL}'S SMITH SENDS A PLEDGE." if is route 3 -> end_deal
  opt "BANDITS, I'M AFRAID. IT'S GONE." if is route 4 -> end_cover

stage end_bars
  end success
  say "THE SMITH HAS THE BARS. THREE FARMS NEAR {VILL} DO NOT PLANT. THE CARTER LOSES HIS CART."
  journal "THE IRON OF {HOME} IS BACK. THREE FARMS OF {VILL} STAND UNPLANTED, AND THE CARTER IS DISMISSED."
  do take "THE TOWN'S UNWORKED BARS"
  do reward fair
  do mark carter_dismissed
  do remember carter "YOU TOOK THE BARS. THREE FARMS WILL HUNGER. I HOPE THE NAILS ARE WORTH IT."
  do fact "THE TOWN'S UNWORKED IRON WAS TAKEN BACK FROM {VILL}. THREE FARMS THERE STAND UNPLANTED."

stage end_shares
  end success
  say "SIX SHARES COME BACK FROM THE FIELDS AND SIX FARMS STOP. THE SMITH MELTS THEM WITHOUT A WORD."
  journal "SIX PLOUGHSHARES WERE TAKEN FROM THE FIELDS OF {VILL}. THE VILLAGE WILL NOT PLANT THIS YEAR."
  do take "SIX PLOUGHSHARES"
  do reward small
  do deed cruelty
  do mark carter_dismissed
  do remember carter "THOSE SHARES FED SIX FARMS. YOU KNOW THAT. YOU WATCHED THEM MELT."
  do fact "SIX PLOUGHSHARES WERE TAKEN FROM THE FIELDS OF {VILL} FOR THE SMITH OF {HOME}. THE VILLAGE WILL NOT PLANT THIS YEAR."

stage end_deal
  end success
  say "{VILL} PLOUGHS, AND FORGES FOR {HOME} EVERY SEASON. THE SMITH GETS A PLEDGE AND A NEW TRICK OF THE TRADE."
  journal "{VILL} PLOUGHS ALL NINE FARMS AND FORGES FOR {HOME}. NOBODY WENT HUNGRY."
  do take "A VILLAGE SMITH'S PLEDGE"
  do reward fair
  do deed mercy
  do mark carter_forgiven
  do remember carter "YOU FOUND A WAY. NOBODY WENT HUNGRY. I DIDN'T THINK THERE WAS ONE."
  do remember giver "A PLEDGE FROM A VILLAGE SMITH, AND A TRICK I DIDN'T KNOW. THE IRON CAME BACK AS HORSESHOES."
  do fact "THE SMITH OF {VILL} STRIKES SHARES FOR HIS OWN FARMS AND TOOLS FOR {HOME}. NOBODY STARVES."

stage end_cover
  end success
  say "THE SMITH CURSES THE FORD AND BUYS DEAR. THE CARTER KEEPS HIS CART AND HIS NAME."
  journal "{GIVER} BELIEVES BANDITS TOOK THE IRON. {VILL} PLOUGHS ON THE TOWN'S BARS."
  do reward small
  do deed lied
  do mark carter_forgiven
  do remember carter "YOU LIED FOR ME. I'LL REMEMBER IT, AND I'LL CARRY YOUR LOADS FREE TILL I DIE."
  do fact "THE SMITH OF {HOME} BELIEVES BANDITS TOOK THE IRON AND BOUGHT MORE AT DOUBLE COST. {VILL} PLOUGHS."

stage refuse
  end fail
  journal "YOU LEFT THE SMITH OF {HOME} TO BUY IRON AT THE NEXT TOWN'S PRICE."
  do remember giver "THE NEXT TOWN'S PRICE IS DOUBLE. THANKS FOR THE ADVICE."
)DSL",

R"DSL(
script rq_saltring
title THE SALT RING
archetype town request: a stranger blamed
tier 2
family request
settle town
once site
cooldown 6
gossip "THEY'VE SALTED THE FOREIGNER'S CART."
gossip "THE GUILD CLERK HAS NEW BOOTS."
hook npc villager
pitch "WHY IS THERE SALT ON THE COBBLES?"
hint "I KEEP THE GATE TALLY. I KNOW WHO CAME IN AND WHEN. NOBODY ASKS A GATE-KEEPER ANYTHING. HAVE YOU SEEN THE SQUARE?"
role giver giver
role home site home
role trader person male at home
role clerk person male at home
role loud person male at home
var told 0

stage start
  talk giver
  say "THEY'VE POURED A RING OF SALT ROUND A FOREIGN TRADER'S CART. OUR OLD WAY HERE, EVER SINCE A PEDLAR ROBBED THE MARKET IN MY GRANDFATHER'S TIME: SALT MARKS A THIEF. NOBODY CROSSES IT, AND AT DUSK THE WATCH BURNS WHAT STANDS INSIDE."
  opt "WHAT DO THEY SAY HE STOLE?" -> stole
  opt "NOT MY FIGHT." -> refuse

stage stole
  talk giver
  say "THE GUILD'S ALMS BOX, GONE FROM THE HALL. BUT I KEEP THE GATE TALLY: THE SEXTON FOUND IT GONE AT THE DAWN BELL, AND THE TRADER CAME THROUGH MY GATE AT NOON. HE CAMPED OUTSIDE THE WALL ALL NIGHT. I SAW HIS FIRE. TAKE THE TALLY. THEY'LL HEAR YOU."
  do give "THE GATE TALLY" scroll
  opt "I'LL SPEAK TO THE TRADER." -> trader

stage trader
  talk trader
  say "TIN AND DYE, UNDER THE NEW TREATY. I CAME TO SELL, NOT TO STEAL. MY BOY HASN'T EATEN SINCE THEY POURED THE SALT; NOBODY WILL SELL ACROSS IT. ASK WHO HAS NEW BOOTS. THE GUILD CLERK HAD HOLES IN HIS WHEN HE TAXED US AT THE GATE."
  opt "WHERE DO I FIND THIS CLERK?" -> clerk
  opt "GO NOW, BEFORE DUSK." -> gone

stage clerk
  talk clerk
  say "THE BOX? GONE AT DAWN, EVERYONE KNOWS. THE FOREIGNER TOOK IT. THESE BOOTS? MY AUNT SENT THEM. FROM DOWN THE RIVER. WHY ARE YOU LOOKING AT MY FEET?"
  opt "NEW BOOTS, AND A DICE DEBT." check level 3 -> confess else deny
  opt "NO REASON. GOOD DAY." -> square

stage confess
  talk clerk
  say "...I OWED AT DICE. THE BOX WAS THERE AND THE KEY WAS ON MY BELT. I MEANT TO PUT IT BACK BEFORE THE AUDIT. THEN THEY SALTED THE FOREIGNER, AND I LET THEM. THE BOX IS UNDER THE GUILDHALL STAIR. MOST OF IT."
  do set told 1
  opt "THEN COME AND SAY SO." -> square

stage deny
  talk clerk
  say "MY AUNT'S BOOTS ARE NONE OF YOUR BUSINESS. GO AND STARE AT SOMEONE ELSE'S FEET. I'VE WORK TO DO."
  opt "AS YOU LIKE." -> square

stage square
  talk loud
  say "OUT OF THE WAY, STRANGER. HE CAME, THE BOX WENT. SALT DOESN'T LIE. AT DUSK WE BURN THE CART, AND HE CAN WALK HOME TO HIS TREATY."
  opt "THE CLERK HAS SOMETHING TO SAY." if is told 1 -> stand_confess
  opt "THE GATE TALLY SAYS HE CAME AT NOON." if have "THE GATE TALLY" -> stand_tally
  opt "HE'S INNOCENT. YOU KNOW MY NAME." if fame 2 -> stand_oath
  opt "THEN I'LL WALK THEM OUT MYSELF." -> escort
  opt "STAND ASIDE." -> mob

stage stand_confess
  end success
  say "THE CLERK SPEAKS. THE CROWD GOES QUIET, THEN LOUD AGAIN, THEN QUIET. THE BOX COMES OUT FROM UNDER THE STAIR, AND SOMEONE SWEEPS THE SALT AWAY."
  journal "THE GUILD CLERK OF {HOME} CONFESSED TO TAKING THE ALMS BOX. THE TRADER {TRADER} WAS CLEARED."
  do take "THE GATE TALLY"
  do reward fair
  do deed innocent
  do remember trader "THE SALT IS SWEPT AND MY BOY HAS EATEN. I'LL TRADE HERE AGAIN. I'LL TRADE WITH YOU FIRST."
  do remember clerk "I SAID IT IN FRONT OF EVERYONE. I'M SCRUBBING THE GUILDHALL STEPS TILL MIDWINTER. FAIR."
  do remember giver "MY TALLY AND YOUR MOUTH. THE SQUARE HASN'T BEEN THAT QUIET SINCE THE PEDLAR."
  do fact "THE GUILD CLERK OF {HOME} CONFESSED TO TAKING THE ALMS BOX FOR A DICE DEBT. A FOREIGN TRADER SALTED AS A THIEF WAS CLEARED."

stage stand_tally
  end success
  say "THE TALLY IS READ ALOUD: NOON, ONE CART, ONE MAN AND A BOY. THE CROWD GRUMBLES AND SCUFFS OUT THE SALT. THE BOX IS STILL MISSING, AND SO IS THE THIEF."
  journal "THE GATE TALLY CLEARED {TRADER} OF THE THEFT IN {HOME}. NOBODY KNOWS WHO TOOK THE ALMS BOX."
  do take "THE GATE TALLY"
  do reward small
  do remember trader "A GATE-KEEPER'S SCRATCHES SAVED MY CART. I'LL NEVER LAUGH AT A CLERK AGAIN."
  do remember giver "THEY HEARD MY TALLY IN YOUR VOICE. ODD FEELING. GOOD ONE."
  do fact "A FOREIGN TRADER SALTED AS A THIEF IN {HOME} WAS CLEARED BY THE GATE TALLY. THE GUILD'S ALMS BOX IS STILL MISSING."

stage stand_oath
  end success
  say "THE CROWD GRUMBLES, BUT A NAME LIKE YOURS CARRIES. THE TORCHES GO DOWN. THE TRADER STAYS. THE CLERK WATCHES FROM THE GUILDHALL DOOR."
  journal "YOUR GOOD NAME CLEARED {TRADER} IN {HOME}. THE ALMS BOX HAS NOT BEEN FOUND."
  do take "THE GATE TALLY"
  do reward small
  do fame 1
  do remember trader "YOUR WORD SAVED US. I DON'T KNOW WHO YOU ARE, BUT THEY DID."
  do fact "A STRANGER OF SOME FAME VOUCHED FOR A FOREIGN TRADER IN {HOME}, AND THE SALT WAS SWEPT AWAY."

stage escort
  end success
  say "YOU WALK THE TRADER AND HIS BOY OUT THROUGH THE GATE BEFORE DUSK. BEHIND YOU THE CART BURNS INSIDE ITS RING OF SALT."
  journal "YOU WALKED {TRADER} AND HIS BOY OUT OF {HOME} ALIVE. THE CART BURNED, AND THE REAL THIEF WAS NEVER NAMED."
  do take "THE GATE TALLY"
  do reward small
  do hide trader
  do remember giver "YOU GOT THEM OUT. THE CART BURNED. THE BOX IS STILL GONE. I KEEP COUNTING."
  do fact "A STRANGER WALKED A FOREIGN TRADER OUT OF {HOME} BEFORE THE WATCH BURNED HIS CART. THE ALMS BOX WAS NEVER FOUND."

stage mob
  end fail
  say "THE TORCHES FALL. THE CART BURNS IN ITS RING OF SALT, AND THE TRADER DRIVES HIS BOY OUT INTO THE NIGHT ON FOOT."
  journal "THE CART OF {TRADER} BURNED IN {HOME} FOR A THEFT HE DID NOT DO."
  do take "THE GATE TALLY"
  do hide trader
  do remember giver "THEY BURNED IT. I HELD MY TALLY AND SAID NOTHING. NEITHER DID YOU."
  do fact "A FOREIGN TRADER'S CART WAS BURNED IN {HOME} FOR A THEFT HE DID NOT DO."

stage gone
  end fail
  say "HE DRIVES OUT BEFORE DUSK WITH HIS BOY. THE SALT RING STAYS ON THE COBBLES ROUND AN EMPTY SPACE."
  journal "{TRADER} FLED {HOME} BEFORE DUSK. EVERYONE NOW BELIEVES HE TOOK THE ALMS BOX."
  do take "THE GATE TALLY"
  do hide trader
  do remember giver "YOU TOLD HIM TO RUN, SO NOW HE'S GUILTY. THAT'S HOW A TOWN COUNTS."
  do fact "A FOREIGN TRADER FLED {HOME} AFTER SALT WAS POURED ROUND HIS CART. THE TOWN BELIEVES HE WAS THE THIEF."

stage refuse
  end fail
  journal "YOU LEFT THE SALTED TRADER OF {HOME} TO THE DUSK."
  do remember giver "THEN WATCH THE SMOKE AT DUSK, LIKE EVERYONE ELSE."
)DSL",

R"DSL(
script rq_pitches
title THE KING OF DUST
archetype town request: a beggars' tax
tier 2
family request
settle town
once site
cooldown 6
gossip "THE BEST PITCHES ARE SOLD BY THE STEP."
gossip "SIXTY MOUTHS FED FROM ONE POT."
hook npc guard
pitch "WHO IS THAT ON THE FOUNTAIN STEPS?"
hint "THERE'S A THING ON THE FOUNTAIN STEPS I CAN'T ARREST AND CAN'T IGNORE. HAVE YOU MET THE KING OF DUST?"
role giver giver
role home site home
role king person male at home
role captain person male at home
role reeve person male at home
var proof 0

stage start
  talk giver
  say "REFUGEES CAME IN THIS SEASON, AND THE POOREST PAY A TENTH OF WHAT THEY BEG TO {KING}. THEY CALL HIM KING. HE SELLS PITCHES ON THE FOUNTAIN STEPS, ONE STEP AT A TIME. BREAK IT UP AND THE WATCH'S PURSE IS YOURS."
  opt "I'LL TALK TO HIM." -> king
  opt "LEAVE THE POOR ALONE." -> refuse

stage king
  talk king
  say "A TENTH. YES. IT FILLS ONE BIG POT, AND SIXTY MOUTHS EAT FROM IT EVERY DUSK. THE STEPS ARE THE TOWN'S, I KNOW THAT. AND SOMEONE ELSE TAKES A SHARE OF MY TENTH. ASK ME WHO."
  opt "WHO ELSE TAKES A SHARE?" -> cord
  opt "I'VE HEARD ENOUGH." -> reeve

stage cord
  talk king
  say "THE CAPTAIN OF THE WATCH. TWO COINS IN EVERY TEN, AND HIS BOOTS STAY OFF OUR BOWLS FOR IT. I TIE A KNOT IN THIS CORD FOR EVERY COIN HE TAKES. HE'LL SAY I LIE. KNOTS DON'T."
  do give "THE KING'S KNOTTED CORD" scroll
  do set proof 1
  opt "I'LL TAKE THIS TO THE REEVE." -> reeve

stage reeve
  talk reeve
  say "THE STEPS BELONG TO THE TOWN. A MAN WHO TAXES THE POOR ON THEM? A SCANDAL, AND A CLEVER ONE. WHAT WOULD YOU HAVE ME DO WITH HIM?"
  opt "CLEAR THE STEPS." -> scatter
  opt "LICENSE HIM. LET THE POT STAND." if fame 1 -> lawful
  opt "THE CAPTAIN TAKES A CUT. HERE." if have "THE KING'S KNOTTED CORD" -> exposed
  opt "YOUR CAPTAIN TAKES BRIBES." if is proof 0 -> slander

stage exposed
  talk reeve
  say "TWO IN TEN, KNOT BY KNOT. I SHOULD HAVE LOOKED. HE HOLDS THE GUARDHOUSE KEYS, AND HE'LL ANSWER BEFORE THE BENCH FOR EVERY KNOT ON THIS CORD."
  do take "THE KING'S KNOTTED CORD"
  opt "AND THE POT?" -> end_exposed

stage end_exposed
  end success
  say "THE CAPTAIN IS BROKEN BEFORE THE BENCH. THE POT STAYS ON THE STEPS, AND THE BOOTS STAY OFF THE BOWLS."
  journal "THE CAPTAIN OF THE WATCH OF {HOME} WAS BROKEN FOR TAKING FROM THE POOR. {KING} KEEPS HIS POT."
  do reward fair
  do fame 1
  do deed innocent
  do hide captain
  do remember giver "A CLEAN WATCH. I'D NEVER HAVE DARED ASK WHERE THE CAPTAIN'S NEW CLOAK CAME FROM."
  do remember king "YOU TOOK MY KNOTS TO THE BENCH. THE POT'S FULLER. SO ARE WE."
  do fact "THE CAPTAIN OF THE WATCH OF {HOME} WAS BROKEN FOR TAKING TWO COINS IN TEN FROM THE POOR. THE KING OF DUST KEEPS HIS POT."

stage scatter
  end success
  say "THE GUARDS CLEAR THE STEPS BY NOON. THE POOR SCATTER TO THE GATES, AND THE POT GOES COLD."
  journal "THE FOUNTAIN STEPS OF {HOME} ARE CLEARED. THE POOR WENT TO THE GATES, AND NOBODY FEEDS THEM NOW."
  do take "THE KING'S KNOTTED CORD"
  do reward fair
  do deed cruelty
  do remember giver "ONE CLEAR FOUNTAIN. HERE'S YOUR PAY. I DON'T FEEL LIKE COUNTING IT."
  do remember king "YOU BROKE THE ONE THING THAT FED US. THE GATES ARE COLD AT NIGHT."
  do fact "THE KING OF DUST WAS BROKEN IN {HOME}. THE REFUGEES WENT TO THE GATES AND THE CAPTAIN OF THE WATCH LOST HIS TWO COINS IN TEN."

stage lawful
  end success
  say "THE REEVE LICENSES {KING} AS WARDEN OF THE STEPS. THE TENTH GOES TO THE REEVE'S OFFICE, AND THE POT BECOMES THE TOWN'S."
  journal "{KING} IS WARDEN OF THE FOUNTAIN STEPS OF {HOME} NOW, BY LICENCE. THE POT IS THE TOWN'S."
  do take "THE KING'S KNOTTED CORD"
  do reward small
  do fame 1
  do deed charity
  do mark pitches_licensed
  do remember giver "A LICENSED BEGGAR. WHAT A DAY. HE SALUTES ME NOW."
  do remember king "YOU MADE IT LAWFUL. NOW I OWE YOU, AND I PAY MY DEBTS. ASK ANYONE ON THE STEPS."
  do fact "THE REEVE OF {HOME} LICENSED {KING} AS WARDEN OF THE STEPS. THE TENTH IS THE TOWN'S AND THE CAPTAIN'S TWO COINS ARE NOBODY'S."

stage slander
  end fail
  say "THE REEVE LAUGHS. THE CAPTAIN HEARS. YOU SPEND THE NIGHT IN THE GUARDHOUSE, AND NOBODY BRINGS YOU SUPPER."
  journal "YOU ACCUSED THE CAPTAIN OF THE WATCH OF {HOME} WITHOUT PROOF AND SPENT A NIGHT IN THE GUARDHOUSE."
  do remember giver "YOU ACCUSED HIM WITHOUT PROOF. NOW HE WATCHES ME. THANKS."
  do fact "A STRANGER ACCUSED THE CAPTAIN OF THE WATCH OF {HOME} WITHOUT PROOF AND SLEPT IN THE GUARDHOUSE."

stage refuse
  end fail
  journal "YOU LEFT THE FOUNTAIN STEPS OF {HOME} TO THE KING OF DUST."
  do remember giver "THEN LEAVE THEM THE STEPS. NO HELP. NO SURPRISE."
)DSL",

R"DSL(
script rq_pig
title SIR TRUFFLE
archetype town request: a prize pig
tier 2
family request
settle town
once site
cooldown 6
gossip "A PRIZE PIG RAN OFF BEFORE THE FAIR."
gossip "THREE RIBBONS AND AN EMPTY PEN."
hook npc farmer
pitch "WHY DO YOU KEEP LOOKING AT THE ROAD?"
hint "HAS ANYONE SEEN A PIG? A LARGE, DIGNIFIED PIG? HE ANSWERS TO SIR. HE DOESN'T ANSWER TO ANYTHING ELSE."
role giver giver
role home site home
role ruin ruin

stage start
  talk giver
  say "SIR TRUFFLE HAS WON THREE FAIRS. HE ISN'T A PIG, HE'S A GENTLEMAN. HE SLIPPED HIS PEN AT DAWN AND THE TRACKS RUN TO THE OLD RUIN OF {RUIN.OLD}. HE'LL FOLLOW A SOUR APPLE ANYWHERE. THE FAIR IS TOMORROW."
  opt "I'LL FIND HIM." -> go
  opt "A PIG? REALLY?" -> why

stage why
  talk giver
  say "I LIVE ALONE OUT THERE, AND THE DOGS DIED. HE MEETS ME AT THE GATE EVERY DAWN. NOBODY ELSE ON THAT FARM SAYS HELLO. THE BLUE RIBBON IS A BONUS. HE IS NOT."
  opt "THEN I'LL FETCH SIR TRUFFLE." -> go
  opt "I HAVE BETTER THINGS TO DO." -> refuse

stage go
  goal goto ruin
  then dead
  do give "A BUCKET OF SOUR APPLES" apple
  journal "SIR TRUFFLE, THE PRIZE PIG OF {GIVER}, FOLLOWED A SMELL TO {RUIN.OLD}. TAKE THE SOUR APPLES; HE WILL FOLLOW THOSE."

stage dead
  goal kill 3 undead in ruin
  then lure
  journal "SIR TRUFFLE ROOTED OPEN A SEALED CELLAR IN {RUIN.OLD} AND WOKE ITS DEAD. CLEAR THEM."

stage lure
  goal fetch "SIR TRUFFLE, SULKING" in ruin
  then back
  journal "SIR TRUFFLE IS SITTING ON A GRAVE IN {RUIN.OLD} AND WON'T BUDGE. COAX HIM OFF WITH THE APPLES AND CARRY HIM HOME TO {GIVER} IN {HOME}."

stage back
  talk giver
  say "THERE HE IS! WHY DOES HE SMELL OF GRAVE? THE DEAD WERE AWAKE? HE ROOTED THEM UP? ...WHAT'S HE CHEWING? OLD GOLD. A FARE FOR THE DEAD. AND THE JUDGES SNIFF EVERY ENTRY, TWICE."
  do take "A BUCKET OF SOUR APPLES"
  do take "SIR TRUFFLE, SULKING"
  do give "A DEAD MAN'S BURIAL COIN" gold
  opt "SLIP THE COIN TO THE JUDGES." -> end_sell
  opt "THE COIN GOES BACK. ENTER HIM AS HE IS." -> rest
  opt "SCRUB HIM TONIGHT. COIN AFTER THE FAIR." -> scrub

stage rest
  goal enter ruin
  then end_rest
  journal "THE COIN BELONGS TO THE DEAD OF {RUIN.OLD}. CARRY IT BACK DOWN TO THE OPEN GRAVE NOW, AND LET SIR TRUFFLE GO TO THE FAIR AS HE IS."

stage scrub
  goal wait 1
  then fair
  say "THE RIVER, THEN. HE'LL SULK ALL NIGHT. BRING A LANTERN AND A STRONG BRUSH."
  journal "SCRUB SIR TRUFFLE IN THE RIVER OVERNIGHT SO HE PASSES THE JUDGES. THE COIN MUST GO BACK TO {RUIN.OLD} AFTER THE FAIR."

stage fair
  talk giver
  say "LAVENDER! HE SMELLS OF LAVENDER, AND HE WON. FOURTH BLUE RIBBON. BUT LISTEN: THE MILLER SAYS LIGHTS MOVED IN {RUIN.OLD} LAST NIGHT. THE DEAD WANT THEIR FARE BACK, AND THEY'RE NOT WAITING POLITELY."
  opt "I'LL TAKE THE COIN BACK NOW." -> return

stage return
  goal kill 3 undead in ruin
  then thanks
  journal "THE DEAD OF {RUIN.OLD} ARE UP AGAIN, LOOKING FOR THEIR FARE. FIGHT YOUR WAY BACK DOWN TO THE OPEN GRAVE."

stage thanks
  goal goto home
  then told
  say "THE LAST OF THEM FALLS. YOU PRESS THE COIN INTO THE OPEN GRAVE, AND THE RUIN GOES QUIET."
  journal "THE COIN IS BACK IN ITS GRAVE. TELL {GIVER} IN {HOME} THE DEAD OF {RUIN.OLD} ARE RESTING."

stage told
  talk giver
  say "THE COIN'S BACK AND THE LIGHTS ARE OUT IN THE RUIN? THEN WE CAN ALL SLEEP. HIM ESPECIALLY: HE'S SNORING ON HIS RIBBON."
  opt "GIVE HIM MY REGARDS." -> end_best

stage end_best
  end success
  say "SIR TRUFFLE WEARS HIS FOURTH RIBBON, SMELLING OF LAVENDER. THE COIN LIES ON ITS GRAVE, AND THE DEAD STAY DOWN."
  journal "SIR TRUFFLE WON HIS FOURTH RIBBON IN {HOME}, AND THE DEAD OF {RUIN.OLD} HAVE THEIR FARE BACK."
  do take "A DEAD MAN'S BURIAL COIN"
  do reward fair
  do deed prayed
  do remember giver "A CLEAN PIG AND A QUIET RUIN. HE SNIFFS THE AIR TOWARDS IT SOMETIMES. I DON'T LET HIM."
  do fact "SIR TRUFFLE WON HIS FOURTH RIBBON AT THE FAIR IN {HOME}. THE COIN HE STOLE LIES ON ITS GRAVE AT {RUIN.OLD} AGAIN."

stage end_rest
  end success
  say "THE COIN IS BACK AND THE DEAD LIE DOWN. AT THE FAIR THE JUDGES SNIFF SIR TRUFFLE ONCE AND STEP BACK. NO RIBBON. HE DOESN'T SEEM TO MIND."
  journal "THE DEAD OF {RUIN.OLD} HAVE THEIR FARE BACK. SIR TRUFFLE WAS DISQUALIFIED FOR SMELLING OF GRAVE."
  do take "A DEAD MAN'S BURIAL COIN"
  do reward small
  do deed prayed
  do remember giver "NO RIBBON. BUT HE SLEPT WELL, AND SO DID I, AND SO DID THE DEAD."
  do fact "SIR TRUFFLE WAS DISQUALIFIED AT THE FAIR IN {HOME} FOR SMELLING OF GRAVE. THE COIN IS BACK AND {RUIN.OLD} IS QUIET."

stage end_sell
  end success
  say "THE COIN GOES INTO A JUDGE'S SLEEVE, AND THE JUDGES DON'T SNIFF VERY HARD. SIR TRUFFLE WINS, STINKING. THE DEAD OF {RUIN.OLD} KEEP WALKING."
  journal "SIR TRUFFLE WON IN {HOME} ON A DEAD MAN'S COIN. THE DEAD OF {RUIN.OLD} WALK."
  do take "A DEAD MAN'S BURIAL COIN"
  do reward fair
  do deed bribed
  do remember giver "A RIBBON IS A RIBBON. I DON'T GO NEAR THE RUIN ROAD AFTER DARK NOW, THOUGH."
  do fact "SIR TRUFFLE WON HIS FOURTH RIBBON IN {HOME} BY A DEAD MAN'S COIN IN A JUDGE'S SLEEVE. THE DEAD OF {RUIN.OLD} WALK."

stage refuse
  end fail
  journal "YOU LEFT THE FARMER OF {HOME} TO FIND SIR TRUFFLE ALONE."
  do remember giver "NOBODY CARES ABOUT A PRIZE PIG. EXCEPT ME. AND HIM."
)DSL",

R"DSL(
script rq_bell
title THE LAST BELL
archetype town request: a founder's secret
tier 2
family request
settle town
once site
cooldown 6
gossip "THE BELL HASN'T RUNG TRUE SINCE FROST."
gossip "THE TEMPLE BELL CLANKS LIKE A POT."
hook npc priest
pitch "WHAT HAPPENED TO YOUR BELL?"
hint "IF YOU'VE WONDERED WHY THE HOURS DRIFT IN THIS TOWN, LOOK UP AT THE TOWER. THEN COVER YOUR EARS."
role giver giver
role home site home
role ruin ruin
role founder person male at home
var choice 0

stage start
  talk giver
  say "THE BELL CRACKED IN THE FROST. FOR THIRTY YEARS IT CALLED US TO {GIVER.GOD}, AND NOW IT CLANKS LIKE A POT. {FOUNDER} CAST HER. HE SAYS HE CAN RECAST HER, BUT HE WANTS SOMETHING FIRST, AND HE WON'T SAY IT TO ME."
  opt "I'LL ASK {FOUNDER}." -> founder
  opt "BUY A BELL ELSEWHERE." -> refuse

stage founder
  talk founder
  say "RECAST HER? NOT FROM HER OWN METAL. I NEED CLEAN OLD BRONZE: THE ALARM BELL OF {RUIN.OLD}. THE DEAD THERE STILL TOLL IT AT DUSK, THEY SAY. NEW TIN SINGS LIKE A BOWL. OLD BRONZE HAS BEEN RUNG. IT KNOWS."
  opt "I'LL BRING IT." -> seek
  opt "WHY NOT HER OWN METAL?" -> evade

stage evade
  talk founder
  say "BECAUSE. THE FROST. THE YEARS. METAL TIRES. ...STOP LOOKING AT ME LIKE THE PRIEST DOES. BRING ME THE BRONZE AND YOU'LL HAVE YOUR ANSWER WHEN SHE'S BROKEN UP. NOT BEFORE."
  opt "I'LL BRING IT." -> seek

stage seek
  goal goto ruin
  then clear
  journal "{FOUNDER} WILL RECAST THE BELL OF {HOME} FROM THE OLD ALARM BELL OF {RUIN.OLD}. THE DEAD THERE TOLL IT AT DUSK."

stage clear
  goal kill 2 undead in ruin
  then fetch
  journal "THE DEAD OF {RUIN.OLD} GUARD THEIR ALARM BELL. CLEAR THEM."

stage fetch
  goal fetch "THE OLD ALARM BELL" in ruin
  then home
  journal "FIND THE OLD ALARM BELL IN {RUIN.OLD}."

stage home
  goal goto home
  then broken
  journal "TAKE THE OLD ALARM BELL TO {FOUNDER} IN {HOME}."

stage broken
  talk founder
  say "THAT'S HER. HEAVY AS A SIN. NOW WATCH: I BREAK THE OLD TOWN BELL. ...THERE. SEE THE GREY IN THE BREAK, LIKE FAT IN MEAT? LEAD. I PADDED HER IN A LEAN YEAR AND THE TEMPLE PAID FOR BRONZE. NOBODY KNEW. THE FROST DID."
  do take "THE OLD ALARM BELL"
  opt "TELL THE PRIEST YOURSELF." check level 3 -> owns else balks
  opt "I'LL TELL THE PRIEST." -> told
  opt "IT STAYS BETWEEN US." -> kept

stage balks
  talk founder
  say "MYSELF? AND LOSE THIRTY YEARS OF NAME IN ONE MORNING? NO. IF IT'S TO BE TOLD, YOU TELL IT. OR NOBODY DOES."
  opt "THEN I'LL TELL THE PRIEST." -> told
  opt "THEN NOBODY DOES." -> kept

stage owns
  talk founder
  say "...YOU'RE RIGHT. A BELL SHOULD RING TRUE AND SO SHOULD THE MAN WHO MAKES IT. I'LL WALK TO THE TEMPLE NOW, BEFORE I LOSE MY NERVE, AND THEN I'LL POUR. TWO DAYS FOR THE MELT."
  do set choice 1
  opt "I'LL WAIT WITH YOU." -> melt

stage told
  talk founder
  say "THEN TELL HIM. I WON'T STOP YOU, AND I WON'T STAND THERE WHILE YOU DO. I'LL POUR HER ANYWAY. TWO DAYS FOR THE MELT, AND DON'T EXPECT ME TO TALK TO YOU WHILE IT RUNS."
  do set choice 2
  opt "I'LL WAIT." -> melt

stage kept
  talk founder
  say "BETWEEN US. THANK YOU. I'LL POUR HER CLEAN THIS TIME, AND NOBODY WILL EVER KNOW SHE WAS ANYTHING ELSE. TWO DAYS FOR THE MELT."
  do set choice 3
  opt "I'LL WAIT." -> melt

stage melt
  goal wait 2
  then pour
  say "THE FURNACE ROARS. NOBODY GOES NEAR."
  journal "THE OLD BRONZE MELTS IN THE FOUNDRY OF {FOUNDER}. WAIT TWO DAYS FOR THE POUR."

stage pour
  talk giver
  say "THE NEW BELL IS UP. LISTEN TO HER. ...I HAVEN'T HEARD THAT NOTE SINCE I WAS A NOVICE. AND I'VE HEARD SOMETHING ELSE THIS WEEK, ABOUT LEAD."
  opt "HE CAME TO YOU HIMSELF." if is choice 1 -> end_owned
  opt "NOW YOU KNOW WHAT CRACKED HER." if is choice 2 -> end_told
  opt "THE FROST CRACKED HER. THAT'S ALL." if is choice 3 -> end_kept

stage end_owned
  end success
  say "HE KNELT IN THE TEMPLE AND TOLD ME. I FORGAVE HIM BEFORE HE FINISHED. HE CAST THIS ONE AT HIS OWN COST AND WOULDN'T TAKE A COIN. SHE'LL RING LONGER THAN EITHER OF US."
  journal "{FOUNDER} CONFESSED THE LEAD IN THE OLD BELL OF {HOME} AND RECAST HER FROM CLEAN BRONZE AT HIS OWN COST."
  do reward fair
  do deed innocent
  do secret home
  do mark bell_true
  do remember founder "I SLEEP NOW. FIRST TIME IN THIRTY YEARS. THE BELL WAKES ME AT DAWN AND I DON'T MIND."
  do remember giver "HE KNELT AND TOLD ME. A BELL THAT RINGS TRUE, AND A MAN WHO DOES. A GOOD WEEK."
  do fact "THE BELL OF {HOME} CRACKED BECAUSE {FOUNDER} PADDED IT WITH LEAD. HE CONFESSED IT TO THE TEMPLE AND RECAST IT FROM THE ALARM BELL OF {RUIN.OLD}."

stage end_told
  end success
  say "LEAD. THIRTY YEARS OF LEAD, AND THE TEMPLE PAID FOR BRONZE. HE'LL PAY IT BACK, EVERY COIN, AND NO TEMPLE WILL BUY FROM HIM AGAIN. BUT THE BELL IS TRUE. I SUPPOSE THAT'S THE ORDER THINGS COME IN."
  journal "THE PRIEST OF {HOME} KNOWS THE OLD BELL WAS PADDED WITH LEAD. {FOUNDER} REPAYS THE TEMPLE, HIS NAME RUINED."
  do reward fair
  do deed guard
  do secret home
  do mark bell_told
  do remember founder "YOU TOLD HIM. THE NEW BELL'S TRUE, AND NOBODY WILL BUY A SPOON FROM ME. BOTH THINGS."
  do fact "{FOUNDER} OF {HOME} WAS SHOWN TO HAVE PADDED THE TEMPLE BELL WITH LEAD. HE RECAST IT FROM CLEAN BRONZE AND REPAYS THE TEMPLE."

stage end_kept
  end success
  say "THE FROST, THEN. THE FROST AND THE YEARS. WELL, SHE SINGS NOW. I WON'T ASK WHY THE FOUNDER WON'T LOOK ME IN THE EYE."
  journal "THE BELL OF {HOME} RINGS TRUE AGAIN. ONLY YOU AND {FOUNDER} KNOW WHAT CRACKED THE OLD ONE."
  do reward small
  do deed lied
  do secret home
  do mark bell_kept
  do remember founder "WHAT WE KNOW STAYS WITH US. THE BELL RINGS TRUE. THAT'S MY APOLOGY, AND IT'LL HAVE TO DO."
  do fact "THE BELL OF {HOME} WAS RECAST FROM THE OLD ALARM BELL OF {RUIN.OLD}. THE TEMPLE BELIEVES THE FROST CRACKED THE OLD ONE."

stage refuse
  end fail
  journal "YOU LEFT THE PRIEST OF {HOME} WITH A CRACKED BELL."
  do remember giver "THEN WE GO ON WITHOUT HOURS. THE BAKER IS ALWAYS LATE NOW."
)DSL",

R"DSL(
script rq_courier
title THE ORDER AT THE GATE
archetype town request: a restless courier
tier 2
family request
settle town
once site
cooldown 6
gossip "A PALE MAN WAITS AT THE GATE AT DUSK."
gossip "HE ASKS THE WAY TO THE OLD RUIN."
hook npc guard
pitch "WHO IS THE MAN AT YOUR GATE?"
hint "THERE'S A MAN AT MY GATE EVERY DUSK. SAME QUESTION, SAME COAT, SAME MUD ON HIS BOOTS. NEVER ANY NEW MUD."
role giver giver
role home site home
role ruin ruin
role courier person male at home

stage start
  talk giver
  say "A COURIER STANDS AT THE GATE EVERY DUSK AND ASKS THE WAY TO {RUIN}. {RUIN.CAUSE} EMPTIED IT, AND NOBODY LIVES THERE NOW. HE STILL THINKS HE'S LATE. NOBODY HERE WILL WALK HIM THERE."
  opt "I'LL SPEAK TO HIM." -> courier
  opt "LET THE DEAD KEEP THEIR GATE." -> refuse

stage courier
  talk courier
  say "I AM LATE. THE ORDER IS SEALED, AND THEY'RE WAITING FOR IT AT {RUIN}. THE ROAD WON'T LET ME WALK IT ALONE. BEHIND THE BEARER, IT DOES. TAKE IT. PLEASE."
  opt "I'LL TAKE IT." -> take
  opt "THEY ARE DEAD, COURIER." -> truth
  opt "GO HOME." -> out

stage truth
  talk courier
  say "DEAD? ALL OF THEM? THEN I'M LATER THAN I THOUGHT. ...DON'T ASK ME WHY. NOT YET. TAKE IT ANYWAY. LAY IT WHERE THEY ARE, AND THEN TELL ME WHAT IT SAID."
  opt "I'LL TAKE IT." -> take

stage take
  goal goto ruin
  then fight
  do give "A SEALED ORDER" letter
  do moves courier ruin
  say "I'LL WALK BEHIND YOU. DON'T LOOK BACK. IT MAKES THE ROAD UNEASY."
  journal "THE COURIER'S SEALED ORDER BELONGS AT {RUIN}. HE WALKS BEHIND YOU. TAKE IT THERE."

stage fight
  goal kill 3 undead in ruin
  then ask
  journal "THE DEAD OF {RUIN.OLD} RISE AGAINST ANYONE WHO COMES. FIGHT THROUGH TO THEIR GRAVES AND LAY THE SEALED ORDER THERE."

stage ask
  talk courier
  say "THEY'VE STOPPED. LISTENING. THE SEAL FELL WHEN YOU LAID IT DOWN; YOU READ IT, YES? I WAS DUE IN THREE DAYS. I STOPPED FOR MY DAUGHTER'S NAMING FEAST, TEN MILES SHORT. I TOOK SIX. WHAT DID IT SAY? TELL ME TRUE."
  opt "COME AWAY. THEY COULD HAVE LIVED." -> truth_end
  opt "STAY. THEY DID EXACTLY RIGHT." -> lie_end
  opt "DON'T ASK. LET IT BURN." -> burn_end

stage truth_end
  end success
  say "YOU READ THE ORDER ALOUD: COME AWAY, ALL OF YOU, BEFORE IT REACHES YOU. THE COURIER KNEELS AMONG THE GRAVES. HE TAKES HIS SIN TO THE DEAD, AND FADES."
  journal "THE ORDER SAID COME AWAY. THE COURIER HEARD IT AT LAST, AMONG THE DEAD OF {RUIN.OLD}, AND WAS GONE."
  do take "A SEALED ORDER"
  do reward fair
  do deed prayed
  do hide courier
  do remember giver "THE GATE IS EMPTY AT DUSK. I STILL LOOK. THANK YOU."
  do fact "THE ORDER A COURIER CARRIED TOO LATE TO {RUIN.OLD} SAID COME AWAY. HE HEARD IT READ AMONG THE DEAD AND WAS NEVER SEEN AGAIN."

stage lie_end
  end success
  say "YOU TELL HIM: STAY. THEY DID EXACTLY RIGHT. HE STRAIGHTENS, SMILES, AND WALKS INTO THE DARK LIGHT AND WRONG."
  journal "YOU TOLD THE COURIER HIS ORDER SAID STAY. HE LEFT SMILING. IT SAID COME AWAY."
  do take "A SEALED ORDER"
  do reward small
  do deed lied
  do hide courier
  do remember giver "THE GATE IS EMPTY. HE LEFT SMILING, THEY SAY. I DIDN'T SEE IT."
  do fact "A COURIER WHO CARRIED AN ORDER TOO LATE TO {RUIN.OLD} WAS TOLD IT SAID STAY. HE LEFT THE GATE SMILING. IT SAID COME AWAY."

stage burn_end
  end success
  say "THE ORDER BURNS UNREAD IN YOUR HAND. THE DEAD STAY STILL. THE COURIER WALKS INTO THE DARK, STILL ASKING."
  journal "THE ORDER BURNED UNREAD AT {RUIN.OLD}. THE COURIER LEFT THE GATE WITHOUT AN ANSWER."
  do take "A SEALED ORDER"
  do reward small
  do hide courier
  do remember giver "THE GATE IS EMPTY. HE SAID NOTHING WHEN HE PASSED. NOT EVEN THE WAY."
  do fact "THE COURIER OF THE GATE OF {HOME} LEFT AFTER HIS ORDER WAS BURNED UNREAD AT {RUIN.OLD}."

stage out
  end fail
  say "THE COURIER BOWS. HE IS STILL THERE AT DUSK, AND EVERY DUSK AFTER."
  journal "YOU TOLD THE COURIER TO GO HOME. HE IS STILL AT THE GATE OF {HOME} AT DUSK."
  do remember giver "HE'S STILL THERE EVERY DUSK. GO HOME, YOU TOLD HIM. HE HASN'T GOT ONE."
  do fact "A PALE COURIER STILL STANDS AT THE GATE OF {HOME} EVERY DUSK, ASKING THE WAY TO {RUIN}."

stage refuse
  end fail
  journal "YOU LEFT THE COURIER AT THE GATE OF {HOME}."
  do remember giver "NOBODY WILL CARRY IT. SO I ANSWER HIM EVERY DUSK. SAME ANSWER."
)DSL",
};

}  // namespace

const char* requestsTownSource() {
  static const std::string joined = [] {
    std::string s;
    for (const char* p : kTown) s += p;
    return s;
  }();
  return joined.c_str();
}

}  // namespace dsl
}  // namespace story
