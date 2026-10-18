// P1b radiant job kind 2: THE FENCE'S CELLAR (content: jobs_a.md section 2; judge notes resolved). JOBS lane.
//
// Goods were stolen and a fence means to sell them within days from the cellar under a bandit camp. The giver names
// their mark (a mark per trade, burned on the lid). The twist is rolled FIRST and the reason drawn from its pool, so the
// pitch never contradicts the den:
//   straight (A)  clear the camp's thieves; the beaten lookout tells why three crates carry tin bells (sold already, wired
//                 against the fence's own men); READ EVERY LID (quiet work: the marked crate, a better fee) or PRY THEM
//                 ALL (the bells wake the sleepers: a fight). Too late (the clock) and the fence has carted the goods off:
//                 the lookout holds the fence's coin for them (the sale money), and the giver takes half a loss.
//   hungry (B)    winter or spring, food goods only, a farmer, innkeeper, lord or priest giver (the priest's food is the
//                 ALMS BREAD): no camp, a cave and one thin thief with two hungry mouths behind them. Take it all, leave
//                 half (a carved spoon, a short purse at home), or bring the thief home to work (they walk with you).
//   inside (C)    a merchant, smith, innkeeper or farmer giver: the camp as in A (thieves, the crates), and standing apart,
//                 empty-handed, the giver's own sister's child, who shows the untouched crate. Tell the giver, or not.
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
{?tw_hungry
role den site cave near
role thief person [[male|female]] in den
}
{!tw_hungry
role den site camp near
role lookout person male in den
}
{?tw_inside
role kin person [[male|female]] young in den
}
var quiet 0

stage start
  talk giver
{!tw_hungry
  say "[[=reason]] [[=markline]] [[=ask]] [[THE PAY IS FAIR.|I'LL PAY WELL FOR IT.|NAME A FAIR FEE, THEN EARN IT.]]"
}
{?tw_hungry
  say "[[=reason]] FIND OUT WHO TOOK IT. THEN BRING WHAT'S LEFT. [[THE PAY IS FAIR.|I'LL PAY FOR YOUR TROUBLE.]]"
}
  opt "[[I'LL GET IT BACK.|I'LL FIND IT.|CONSIDER IT FOUND.]]" -> go
  opt "[[NOT MY BUSINESS.|NOT TODAY.|FIND SOMEONE ELSE.]]" -> decline

{!tw_hungry
stage go
  goal goto den
  say "[[COUNT IT WHEN YOU FIND IT. THEY'LL HAVE SKIMMED SOME.|LOOK AT EVERY LID BEFORE YOU PRY.]]"
  journal "{GIVER}'S [[=goods]] WAS CARRIED TO A FENCE'S CELLAR AT {DEN}, {DEN.DIR}. BEAT THE SALE. THE MARK ON THE LID: [[=mark]]."
  deadline [[=clock]] -> go_late
  then clear

stage clear
  goal kill [[=thieves]] bandit in den
  journal "THE THIEVES AT {DEN} ARE AWAKE AND ARMED. DEAL WITH THEM, THEN FIND THE CRATES."
  then [[=afterclear]]

stage go_late
  goal goto den
  journal "TOO MANY DAYS HAVE PASSED. THE FENCE MAY HAVE SOLD {GIVER}'S [[=goods]] BY NOW. GO TO {DEN} AND SAVE WHAT CAN BE SAVED."
  then clear_late

stage clear_late
  goal kill [[=thieves]] bandit in den
  journal "THE THIEVES AT {DEN} ARE STILL THERE. DEAL WITH THEM AND FIND OUT WHAT BECAME OF THE GOODS."
  then lookout_late

stage lookout_late
  talk lookout
  say "YOU'RE LATE. THE FENCE CAME LAST NIGHT AND CARTED IT ALL AWAY. HE LEFT HIS COIN FOR THE GOODS WITH ME, TO PAY THE BOYS. TAKE IT. LET ME CRAWL OFF."
  journal "THE FENCE GOT THERE FIRST. THE LOOKOUT AT {DEN} HOLDS HIS COIN FOR THE GOODS."
  opt "[[GIVE ME THE PURSE.|HAND IT OVER, AND CRAWL.]]" -> purse

stage purse
  talk giver
  say "THE FENCE WAS FASTER. BUT THIS IS HIS COIN FOR MY [[=goods]], AND HALF A LOSS BEATS A WHOLE ONE. TAKE HALF OF IT."
  journal "BRING THE FENCE'S PURSE TO {GIVER} IN {HOME}."
  do give "THE FENCE'S PURSE" gold
  opt "[[IT'S ALL I COULD SAVE.|BETTER THAN NOTHING.]]" -> paid_half
}

{?tw_straight
stage lookout
  talk lookout
  say "GO ON, THEN. IT WAS HEAVY ANYWAY. THE CRATES WITH THE TIN BELLS ARE ALREADY SOLD. THE BELLS ARE FOR OUR OWN HANDS. THE FENCE COMES AT DAWN. YOU'RE EARLY."
  journal "FOUR CRATES STAND IN THE CELLAR AT {DEN}, THREE OF THEM WIRED WITH TIN BELLS. {GIVER}'S MARK: [[=mark]]."
  opt "[[I'LL READ EVERY LID FIRST.|LIDS FIRST. THEN THE CROWBAR.]]" -> lids
  opt "[[I'LL PRY THEM ALL. QUICKLY.|NO TIME FOR LIDS.]]" -> bells

stage lids
  talk lookout
  say "[[=decoy1]] BURNED ON A LID. NOT IT. [[=decoy2]]. NOT IT. [[=decoy3]]. NOT IT. AND AT THE BACK: [[=mark]]. {GIVER}'S MARK. HEAVY. THEY BARELY OPENED IT."
  journal "THE MARKED CRATE AT {DEN} IS OPEN AND NO BELL HAS RUNG. CARRY {GIVER}'S [[=goods]] HOME."
  do set quiet 1
  do give "{GIVER}'S [[=goods]]" [[=icon]] heavy
  opt "[[PRY IT QUIETLY. TAKE IT HOME.|QUIET NOW. AND OUT.]]" -> report

stage bells
  goal kill 2 bandit in den
  say "TIN BELLS JANGLE IN THE LID! SOMEONE'S AWAKE BACK THERE!"
  journal "THE BELLS WOKE THE SLEEPERS AT {DEN}. FIGHT THEM OFF."
  do ambush bandit 2
  then found

stage found
  talk lookout
  say "YOU WOKE THE WHOLE CELLAR FOR NOTHING. THE CRATE WITH THE [[=mark]] WAS AT THE BACK, NO BELLS ON IT. TAKE IT AND GO."
  journal "{GIVER}'S [[=goods]] IS YOURS AGAIN. CARRY IT HOME TO {HOME}."
  do give "{GIVER}'S [[=goods]]" [[=icon]] heavy
  opt "[[ON MY WAY.|NEXT TIME, SLEEP LIGHTER.]]" -> report

stage report
  talk giver
  say "[[EVERY PIECE. YOU'RE AN HONEST ONE.|I HAD GIVEN IT UP. LOOK AT IT. EVERY PIECE.|THAT'S MY MARK ON THE LID. SIT DOWN.]]"
  journal "BRING {GIVER}'S [[=goods]] HOME TO {HOME}."
  opt "[[HERE IT IS.|COUNT IT.]]" -> paid
  opt "AND NOT ONE BELL RANG." if is quiet 1 -> paid_quiet
}

{?tw_inside
stage youngster
  talk kin
  say "DON'T TELL {GIVER}. PLEASE. MY OWN KIN PAYS IN PRAISE, NOT COIN. THE FENCE ASKED, AND I SAID YES. THE CRATE WITH THE [[=mark]] IS THE ONE AT THE BACK. NOBODY TOUCHED IT. I MADE SURE."
  journal "AMONG THE THIEVES AT {DEN} STOOD {KIN}, {GIVER}'S OWN SISTER'S CHILD. THE MARKED CRATE IS WHOLE."
  do give "{GIVER}'S [[=goods]]" [[=icon]] heavy
  opt "[[GO HOME. I'LL CARRY IT.|GET OUT OF HERE. NOW.]]" -> report_c

stage report_c
  talk giver
  say "EVERY PIECE, AND MY MARK ON THE LID. WHO WAS IT, IN THE END? TELL ME STRAIGHT."
  journal "BRING {GIVER}'S [[=goods]] HOME TO {HOME}. DECIDE WHAT TO SAY ABOUT {KIN}."
  opt "IT WAS {KIN}. YOUR OWN KIN." -> told
  opt "[[JUST THIEVES. THEY WON'T BE BACK.|THIEVES. NOBODY YOU KNOW.]]" -> silent
}

{?tw_hungry
stage go
  goal goto den
  journal "SOMEONE EMPTIED {GIVER}'S LARDER OF [[=goods]] AND RAN FOR {DEN}, {DEN.DIR}. FIND OUT WHO, AND BRING BACK WHAT'S LEFT."
  then thief

stage thief
  talk thief
  say "I'M NOT PROUD. THERE ARE THREE OF US IN HERE, AND THE OTHER TWO ARE SMALL. WE ATE WHAT WE COULD. TAKE THE REST. I'D HAVE WORKED IT OFF. NOBODY ASKED ME."
  journal "THE THIEF AT {DEN} IS ONE THIN SOUL WITH TWO HUNGRY MOUTHS BEHIND. THE [[=goods]] IS HALF EATEN."
  opt "[[I'LL TAKE WHAT'S LEFT.|THE REST GOES BACK.]]" -> take_all
  opt "[[KEEP HALF. FOR THE SMALL ONES.|LEAVE HALF FOR YOUR THREE.]]" -> share
  opt "COME BACK WITH ME. ASK FOR WORK." -> bring

stage take_all
  talk giver
  say "WINTER HUNGER MAKES THIEVES OF HONEST MEN. I'M NOT ANGRY. I'M ONLY POOR. GIVE IT HERE."
  journal "CARRY THE REST OF THE [[=goods]] HOME TO {GIVER}."
  do give "WHAT'S LEFT OF THE [[=goods]]" [[=icon]]
  do remember thief "YOU TOOK IT ALL BACK. FAIR. WE WENT HUNGRY."
  opt "[[HERE'S WHAT WAS LEFT.|THEY HAD EATEN SOME.]]" -> paid_b

stage share
  talk giver
  say "SHORT BY HALF. I CAN COUNT, YOU KNOW. ...THREE OF THEM? SMALL ONES? HM. WELL. I'LL PAY FOR WHAT CAME BACK."
  journal "CARRY HALF THE [[=goods]] HOME TO {GIVER}. THE OTHER HALF STAYED WITH THE THREE AT {DEN}."
  do give "HALF THE [[=goods]]" [[=icon]]
  do give "A CARVED SPOON" bone
  do remember thief "ONE OF THE THREE. I CARVED YOU THAT SPOON. KEEP IT."
  opt "[[THEY NEEDED IT MORE.|I LEFT THEM HALF.]]" -> paid_share

stage bring
  goal escort thief to home walk
  journal "WALK {THIEF} HOME TO {HOME} TO ASK {GIVER} FOR WORK INSTEAD OF A CELL."
  do follow thief
  then bring_back
  else thief_lost

stage bring_back
  talk giver
  say "SO THIS IS THE THIEF. THREE MOUTHS, YOU SAY. HM. THE ROOF NEEDS DOING, AND THE WOODPILE. WORK, NOT A CELL. WE'LL SEE."
  journal "{THIEF} STANDS BEFORE {GIVER}, ASKING FOR WORK."
  do moves thief home
  do remember thief "WORK, NOT A CELL. I'LL REMEMBER IT."
  opt "[[GIVE THEM A CHANCE.|THEY'LL WORK HARD.]]" -> paid_bring
}

{?tw_straight
stage paid
  end success
  say "[[A FAIR FEE, AND MY THANKS.|TAKE YOUR FEE. AND IF YOU HEAR OF THAT FENCE AGAIN, TELL ME.]]"
  journal "{GIVER}'S [[=goods]] IS HOME AGAIN, RECOVERED FROM THE FENCE'S CELLAR AT {DEN}."
  do take "{GIVER}'S [[=goods]]"
  do reward fair
  do remember giver "THE [[=goods]] YOU BROUGHT BACK. I LOCK THE SHED NOW."
  do census home stock [[=good]] 4

stage paid_quiet
  end success
  say "NOT ONE BELL? THEN THE FENCE WILL FIND HIS SOLD CRATES WHERE HE LEFT THEM, AND MINE GONE. QUIET WORK PAYS BETTER."
  journal "{GIVER}'S [[=goods]] IS HOME AGAIN, LIFTED FROM {DEN} WITHOUT A BELL RINGING."
  do take "{GIVER}'S [[=goods]]"
  do reward fair 125
  do remember giver "QUIET WORK, AT {DEN}. NOT ONE BELL. I TELL THAT STORY."
  do census home stock [[=good]] 4
}

{!tw_hungry
stage paid_half
  end success
  journal "THE FENCE SOLD {GIVER}'S [[=goods]] BEFORE YOU CAME, BUT HIS COIN FOR IT CAME HOME WITH YOU."
  do take "THE FENCE'S PURSE"
  do reward fair 50
  do remember giver "THE FENCE WAS FASTER THAN YOU. HALF A LOSS. I'LL LIVE."
}

{?tw_inside
stage told
  end success
  say "MY SISTER'S CHILD. ...I'LL DEAL WITH IT. QUIETLY. THANK YOU FOR THE TRUTH. IT COSTS MORE THAN THE GOODS."
  journal "YOU TOLD {GIVER} THAT {KIN} WAS ONE OF THE THIEVES. {KIN} MUCKS OUT THE STABLE NOW, GLUM BUT PRESENT."
  do take "{GIVER}'S [[=goods]]"
  do reward fair 110
  do moves kin home
  do remember giver "MY SISTER'S CHILD IS MUCKING OUT THE STABLE. GLUM. PRESENT. THANK YOU."
  do remember kin "YOU TOLD. ...FINE. I'D HAVE TOLD, IN YOUR BOOTS."

stage silent
  end success
  say "THIEVES. WELL, THEY'RE GONE. TAKE YOUR FEE."
  journal "YOU SAID NOTHING OF {KIN}. {GIVER} HAS THE GOODS AND NO IDEA. THE THEFTS MAY GO ON, AND YOU KNOW WHY."
  do take "{GIVER}'S [[=goods]]"
  do reward fair
  do gold 25
  do remember giver "SOMETHING'S MISSING FROM THE SHED AGAIN. I CAN'T THINK WHAT."
  do remember kin "FOR YOUR SILENCE. I WON'T FORGET IT."
}

{?tw_hungry
stage paid_b
  end success
  journal "YOU BROUGHT BACK WHAT WAS LEFT OF {GIVER}'S [[=goods]]. THE THIEF AT {DEN} WENT HUNGRY."
  do take "WHAT'S LEFT OF THE [[=goods]]"
  do reward fair 90
  do census home stock [[=good]] 3

stage paid_share
  end success
  journal "YOU LEFT HALF THE [[=goods]] WITH THE HUNGRY THREE AT {DEN}. {GIVER} PAID FOR HALF, AND GRUMBLED."
  do take "HALF THE [[=goods]]"
  do reward fair 70
  do deed charity
  do census home stock [[=good]] 2

stage paid_bring
  end success
  say "THEY CAN START ON THE ROOF TOMORROW. AND YOU, TAKE THIS FOR BRINGING ME A WORKER INSTEAD OF A CORPSE."
  journal "{THIEF} WORKS FOR {GIVER} NOW, MENDING THE ROOF. THREE MOUTHS ARE FED HONESTLY."
  do reward fair 90
  do deed mercy
  do census home mood 3
  do census home stock [[=good]] 2

stage thief_lost
  end fail
  say "{THIEF} DID NOT LIVE TO REACH {HOME}. THE ROAD WAS NO KINDER THAN THE WINTER."
  journal "{THIEF} DIED ON THE ROAD TO {HOME}."
  do remember giver "A THIEF WHO DIED ON THE ROAD TO MY DOOR. I'D HAVE GIVEN THEM WORK."
}

stage decline
  end fail
  journal "YOU TURNED DOWN {GIVER}'S LOST GOODS."
)DSL";

enum { V_FARMER, V_SMITH, V_PRIEST, V_MERCHANT, V_INNKEEPER, V_JARL };
enum { T_STRAIGHT, T_HUNGRY, T_INSIDE };

struct Goods { const char* name; const char* good; const char* icon; bool food; };
const std::vector<Goods>& goodsOf(int v) {
  static const std::vector<Goods> farmer = {{"SEED GRAIN", "grain", "seeds", true}, {"WOOL", "wool", "cloak", false}, {"SMOKED MEAT", "meat", "meat", true},
                                            {"CHEESE", "produce", "cheese", true}, {"HONEY", "produce", "potion", true}};
  static const std::vector<Goods> smith = {{"FORGE IRON", "ingot", "ingot", false}, {"BAR STEEL", "ingot", "ingot", false},
                                           {"BRASSWORK", "tools", "gem", false}, {"COPPER", "ore", "ore", false}};
  static const std::vector<Goods> priest = {{"TEMPLE PLATE", "pottery", "gem", false}, {"ALTAR SILVER", "ingot", "gem", false},
                                            {"ALTAR CLOTH", "cloth", "cloak", false}, {"INCENSE", "produce", "herb", false}, {"ALMS BREAD", "bread", "bread", true}};
  static const std::vector<Goods> merchant = {{"DYED CLOTH", "cloth", "cloak", false}, {"GLASSWARE", "pottery", "gem", false}, {"PEPPER", "produce", "herb", false},
                                              {"SILK", "cloth", "cloak", false}, {"LAMP OIL", "produce", "potion", false}};
  static const std::vector<Goods> inn = {{"CELLAR WINE", "produce", "potion", false}, {"SPICED MEAD", "produce", "potion", true},
                                         {"FINE LINEN", "cloth", "cloak", false}, {"OLD BRANDY", "produce", "potion", false}};
  static const std::vector<Goods> jarl = {{"SPEAR STEEL", "ingot", "ingot", false}, {"TITHE GRAIN", "grain", "sheaf", true},
                                          {"BANNER CLOTH", "cloth", "cloak", false}, {"STATE SILVER", "ingot", "gem", false}};
  switch (v) {
    case V_FARMER: return farmer;
    case V_SMITH: return smith;
    case V_PRIEST: return priest;
    case V_MERCHANT: return merchant;
    case V_INNKEEPER: return inn;
    default: return jarl;
  }
}

void prep(const JobCtx& c, uint64_t seed, std::map<std::string, std::string>& P) {
  Dice d(seed);
  const bool hungry = c.twist == T_HUNGRY, inside = c.twist == T_INSIDE;
  // the goods: food only for the hungry thief; never the priest's alms bread otherwise (temple goods are plate and cloth)
  std::vector<Goods> pool;
  for (const Goods& g : goodsOf(c.variant)) {
    if (hungry && !g.food) continue;
    if (!hungry && std::string(g.name) == "ALMS BREAD") continue;
    pool.push_back(g);
  }
  if (pool.empty()) pool.push_back(goodsOf(c.variant).front());
  const Goods& G = pool[(size_t)d.below((int)pool.size())];
  const std::string goods = G.name;
  P["goods"] = goods; P["good"] = G.good; P["icon"] = G.icon;
  // the giver's mark (one per trade) and three decoys, never the giver's
  static const char* marks[] = {"A WHEAT SHEAF", "AN ANVIL", "A SUN", "A SCALE", "A BARREL HOOP", "A TOWER"};
  const std::string mark = marks[c.variant];
  P["mark"] = mark;
  static const char* lead[] = {"MY MARK: A WHEAT SHEAF ON THE LID.", "MY MARK: AN ANVIL ON THE LID.", "OUR MARK: A SUN, BURNED ON THE LID.",
                               "MY MARK: A SCALE ON THE LID.", "MY MARK: A BARREL HOOP ON THE LID.", "THE LORD'S MARK: A TOWER ON THE LID."};
  P["markline"] = lead[c.variant];
  std::vector<std::string> decoys = {"A WHEEL", "A FISH-HOOK", "A BELL", "A CROSSED KEY", "A STAR", "A HORSESHOE"};
  for (int k = 0; k < 3; k++) {
    const size_t at = (size_t)d.below((int)decoys.size());
    P["decoy" + std::to_string(k + 1)] = decoys[at];
    decoys.erase(decoys.begin() + (std::ptrdiff_t)at);
  }
  P["thieves"] = std::to_string(3 + (c.tier >= 1 ? 1 : 0) + (c.danger ? 1 : 0));
  P["clock"] = std::to_string(4 + (c.tier == 0 ? 1 : 0));
  P["afterclear"] = inside ? "youngster" : "lookout";
  // the reason, from the twist's own pool (A or C: the den is a fence's camp; B: a larder emptied, one hungry thief)
  std::vector<std::string> R;
  if (hungry) {
    R.push_back("WINTER HUNGER MAKES THIEVES OF HONEST FOLK. THEY TOOK THE " + goods + " AND RAN FOR {DEN}. I AM NOT ANGRY, ONLY POOR.");
    R.push_back("THE LARDER WAS EMPTIED AND NOTHING ELSE TOUCHED. THE " + goods + " IS GONE. THE TRACKS END AT {DEN}, {DEN.DIR} OF HERE.");
  } else {
    switch (c.variant) {
      case V_FARMER: R.push_back("THEY EMPTIED MY SHED OF " + goods + " WHILE I WAS AT MARKET. THE TRACKS LED TO {DEN}, {DEN.DIR} OF HERE.");
                     if (c.season == 0) R.push_back("THE " + goods + " WAS THE WHOLE OF MY SPRING. THE THIEVES WENT TO GROUND AT {DEN}.");
                     break;
      case V_SMITH: R.push_back("THE FORGE IS COLD WITHOUT THE " + goods + ". I SAW THE CART TRACKS, AND THE TRAIL LEADS TO {DEN}.");
                    R.push_back("THEY EMPTIED MY SHED OF " + goods + " WHILE I WAS AT MARKET. THE TRACKS LED TO {DEN}, {DEN.DIR} OF HERE.");
                    break;
      case V_PRIEST: R.push_back("THE TEMPLE WAS ROBBED AT NEW MOON. THE " + goods + " IS GONE. THE FOOTPRINTS END AT {DEN}, {DEN.DIR} OF HERE."); break;
      case V_MERCHANT: R.push_back("MY WAGON WAS STRIPPED OF " + goods + " WHILE THE DRIVER SLEPT. A FENCE IS SELLING IT OFF FROM {DEN}. IT WON'T LAST.");
                       R.push_back("A FENCE WILL SELL MY " + goods + " BY WEEK'S END, AND THE WATCH WOULD ONLY SCARE HIM OFF. I WANT IT BACK QUIETLY.");
                       break;
      case V_INNKEEPER: R.push_back("A DRUNK SWORE HE SAW THE " + goods + " CARRIED INTO {DEN}. HE'S A SOT, BUT HE'S NEVER BEEN WRONG ABOUT A CELLAR."); break;
      default: R.push_back("MY STORE WAS EMPTIED WHILE THE WATCH SLEPT. THE " + goods + " IS AT {DEN} NOW. FETCH IT AND KEEP THE SHAME QUIET."); break;
    }
    if (c.variant != V_PRIEST && c.variant != V_JARL)
      R.push_back("SOMEONE HAS BEEN SELLING MY " + goods + " A LITTLE AT A TIME ON THE ROAD. THE TRAIL ENDS AT {DEN}, {DEN.DIR} OF HERE.");
  }
  P["reason"] = R[(size_t)d.below((int)R.size())];
  P["ask"] = d.pick({"FIND THE CRATE WITH MY MARK AND CARRY IT HOME.", "BRING IT BACK BEFORE THE FENCE SELLS IT.", "NOT A COIN OF IT STAYS WITH THEM.",
                     "I DON'T CARE ABOUT THE THIEVES. I CARE ABOUT THE " + goods + "."});
  std::vector<std::string> T;
  if (hungry) T = {"THE EMPTY LARDER", "WHO TOOK " + goods, "HUNGER'S THIEF", "STOLEN: " + goods};
  else T = {"THE FENCE'S CELLAR", "BEFORE THEY SELL IT", "LIFTED: " + goods, "READ THE LIDS", "WHO TOOK " + goods, "TRACE THE LOOT", "STOLEN: " + goods};
  P["title"] = T[(size_t)d.below((int)T.size())];
}

bool choose(const JobCtx& w, uint64_t dice, JobCtx& out) {
  Dice d(dice);
  out = w;
  switch ((Role)w.trade) {
    case Role::Farmer: out.variant = V_FARMER; break;
    case Role::Smith: out.variant = V_SMITH; break;
    case Role::Priest: out.variant = V_PRIEST; break;
    case Role::Merchant: out.variant = V_MERCHANT; break;
    case Role::Innkeeper: out.variant = V_INNKEEPER; break;
    case Role::Jarl: out.variant = V_JARL; break;
    default: return false;
  }
  // the twist first (A 60, B 20, C 20); an illegal one gives its share to A
  const int roll = d.below(100);
  out.twist = T_STRAIGHT;
  const bool foodGiver = out.variant == V_FARMER || out.variant == V_INNKEEPER || out.variant == V_JARL || out.variant == V_PRIEST;
  const bool hungrySeason = w.season == 0 || w.season == 3;
  if (roll >= 60 && roll < 80 && foodGiver && hungrySeason) out.twist = T_HUNGRY;
  if (roll >= 80 && (out.variant == V_MERCHANT || out.variant == V_SMITH || out.variant == V_INNKEEPER || out.variant == V_FARMER)) out.twist = T_INSIDE;
  return true;
}

JobCtx walk(Role t, int variant, int twist, int season, int tier = 0) {
  JobCtx c;
  c.trade = (int)t; c.variant = variant; c.twist = twist; c.season = season; c.tier = tier;
  return c;
}

}  // namespace

const KindDef& kindFence() {
  static const KindDef K = [] {
    KindDef k;
    k.id = "fence";
    k.title = "THE FENCE'S CELLAR";
    k.lead = "WAS ROBBED, AND A FENCE IS SELLING";
    k.body = kBody;
    k.variants = {"farmer", "smith", "priest", "merchant", "innkeeper", "jarl"};
    k.twists = {"straight", "hungry", "inside"};
    k.trades = {(int)Role::Farmer, (int)Role::Smith, (int)Role::Priest, (int)Role::Merchant, (int)Role::Innkeeper, (int)Role::Jarl};
    k.choose = choose;
    k.prep = prep;
    k.walks = {walk(Role::Merchant, V_MERCHANT, T_STRAIGHT, 2, 1), walk(Role::Priest, V_PRIEST, T_HUNGRY, 3),
               walk(Role::Smith, V_SMITH, T_INSIDE, 1), walk(Role::Jarl, V_JARL, T_STRAIGHT, 0, 2), walk(Role::Farmer, V_FARMER, T_HUNGRY, 0)};
    return k;
  }();
  return K;
}

}  // namespace radiant
}  // namespace story
