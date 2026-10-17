// M12 "Rise and Fall": enlisting, ranks, campaign orders and diplomacy missions (VISION_PLAN 4.7 "Enlisting (M12):
// ranks of Recruit, Sergeant, Captain and Marshal, with a wage. Campaign quests are generated from active sieges.";
// "A diplomacy quest ('carry the terms') forces a peace roll"; 15.6.4 renown; 15.33). SERVICE lane: it owns this header
// and rpg/sim/service*.cpp. It may ADD to this header; what phase A declared is the contract (the LORD lane reads the
// rank to grant a lordship; the VIEW lane reads rank, sworn kingdom, orders and missions for the STANDING page and the
// tabard), never renamed, removed or changed in meaning.
//
// Saved: the service block (SAVE_VER 22, after the sea block; its own version byte first). Realm::renown().rank /
// sworn mirror `rank` / `sworn` (ServiceOps keeps them in step on every change and on load).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "rpg/world/ids.h"

namespace service {
using ew::Gid;

enum class Rank : uint8_t { None, Recruit, Sergeant, Captain, Marshal, COUNT };
const char* rankName(Rank r);       // "RECRUIT", "SERGEANT", "CAPTAIN", "MARSHAL" ("" for None)
int wageOf(Rank r);                 // gold a week (7 days): 12, 30, 70, 150
int meritToRise(Rank r);            // merit needed to rise from r to the next rank (0: the top)
int squadOf(Rank r);                // soldiers who march with the player on an order (0, 0, 2, 4, 6)

// a campaign order (a Quest of QType::Campaign mirrors it: Quest::stage = Order::id). Generated from the sworn
// kingdom's active sieges and wars (the SERVICE lane's tick; phase A: forced by scripts).
enum class OrderKind : uint8_t {
  None,
  BreakSiege,   // our town is besieged: kill the besiegers and their captain (adds def)
  Assault,      // our siege of their town: open the gate / kill the defenders' captain (adds atk)
  Supply,       // carry supplies to a besieged town of ours (+5 % def for 3 days: VISION_PLAN 4.7)
  Scout,        // walk to an enemy camp or town and come back alive (the war's score)
  Assassinate,  // kill the enemy's captain at their camp (-15 % atk)
  Escort,       // walk refugees or a train of carts from a burned place to a friendly town
  Hold,         // stand with the garrison of a frontier town through a night raid
  COUNT
};
const char* orderName(OrderKind k);  // "BREAK THE SIEGE OF", "TAKE", ...
constexpr uint8_t OS_OPEN = 0, OS_DONE = 1, OS_FAILED = 2;
struct Order {
  uint32_t id = 0;
  OrderKind kind = OrderKind::None;
  Gid site = 0;              // the town / camp it is about
  uint32_t war = 0, siege = 0;   // the realm's war / siege ids (0 none)
  int32_t gx = 0, gy = 0;    // where to go (global tile)
  uint16_t givenDay = 0, dueDay = 0;
  int quest = 0;             // the Quest id mirroring it (0 none)
  uint8_t state = OS_OPEN;
  uint8_t merit = 10;        // merit when done
  // ---- SERVICE lane (phase B; service block v2): the order played out in the world (rpg/sim/service_orders.cpp)
  uint8_t flags = 0;         // OF_* progress
  uint8_t squad = 0;         // soldiers of the squad still marching with the player (the fallen never come back)
  Gid foe = 0;               // the enemy kingdom it is against
  Gid from = 0;              // Supply: the friendly place the supplies are drawn at; Scout: where to report back;
                             // Escort: the burned place the people walk from (site ids; 0 none)
  uint16_t aux = 0;          // the kind's counter: Hold raiders slain, Escort walkers alive, Supply's boost (hundredths)
  uint16_t untilDay = 0;     // Supply: the day the delivered boost runs out (VISION_PLAN 4.7: +5 % def for 3 days)
};
// Order::flags
constexpr uint8_t OF_REACHED = 1;   // Scout: the enemy seen; Supply: the supplies loaded; Escort: the people found; Hold: the raid began
constexpr uint8_t OF_CAPDEAD = 2;   // BreakSiege / Assassinate: the enemy captain fell
constexpr uint8_t OF_LEAD = 4;      // given to a Captain or Marshal: the player leads (a bigger squad, more merit)
constexpr uint8_t OF_BOOST = 8;     // Supply: the boost is on the siege (taken off again on untilDay)
constexpr uint8_t OF_AMBUSH = 16;   // Escort: the road's ambush came; Hold: the second wave came
constexpr uint8_t OF_HOME = 32;     // the squad was sent home (the order is over)

// a diplomacy mission (a Quest of QType::Envoy mirrors it: Quest::stage = Mission::id): carry a court's terms to
// another court. On arrival the realm rolls (realm::Realm::peaceRoll / allianceRoll / tributeRoll) with the player's
// renown as the bonus.
enum class MissionKind : uint8_t { None, CarryTerms, SeekAlliance, DemandTribute, FirstEnvoy, COUNT };
// FirstEnvoy: after first contact (M10), a mainland court's greeting carried to a hidden kingdom
const char* missionName(MissionKind k);
struct Mission {
  uint32_t id = 0;
  MissionKind kind = MissionKind::None;
  Gid from = 0, to = 0;      // the kingdoms (from: whose terms; to: whose court receives them)
  Gid toSite = 0;            // the receiving court's seat (its capital when given)
  int32_t gx = 0, gy = 0;
  uint16_t givenDay = 0, dueDay = 0;
  int quest = 0;
  uint8_t state = OS_OPEN;
  uint8_t outcome = 0;       // TermsCarried's mag (1 peace, 2 alliance, 3 tribute, 0 refused)
};

// armies the player served and how it ended
constexpr uint8_t PAST_DISCHARGED = 1, PAST_DESERTED = 2, PAST_DISBANDED = 3;   // disbanded: the realm fell
struct Past { Gid kingdom = 0; Rank rank = Rank::None; uint8_t how = 0; uint16_t day = 0; };

struct Service {
  Gid sworn = 0;             // the kingdom served now (0: none)
  Rank rank = Rank::None;
  int merit = 0;             // toward the next rank (kills of the enemy, orders done, sieges won)
  uint16_t enlistDay = 0, paidDay = 0, lastOrderDay = 0, rankDay = 0;
  uint16_t battles = 0, ordersDone = 0, ordersFailed = 0, missionsDone = 0;
  uint32_t nextId = 1;
  std::vector<Order> orders;       // open and the last few finished (bounded)
  std::vector<Mission> missions;
  std::vector<Past> past;          // bounded (16)

  // ---- SERVICE lane (phase B; service block v2)
  Gid huntedBy = 0;                // a deserted army whose soldiers stay hostile until huntedUntil
  uint16_t huntedUntil = 0;
  uint16_t siegeDay = 0;           // sieges resolved up to this day were looked at for the merit of a siege won
  // runtime (never saved: rebuilt from the orders and the window; service_orders.cpp)
  struct Runtime {
    int lastDay = -1;              // the day the daily checks last ran
    float t = 0, capT = 0;         // the order and squad checks (4 a second), the town captains (twice a second)
    std::vector<int> squad;        // actor ids of the squad in play
    std::vector<int> foes;         // actor ids of the order's own enemies (an enemy captain's guard, raiders, an ambush)
    std::vector<int> walkers;      // actor ids of the people an Escort leads
    std::vector<std::pair<Gid, int>> captains;   // a town's captain of the watch in play (site id, actor id)
    std::vector<std::pair<int, float>> going;    // squad men sent home (actor id, time left)
    uint32_t squadOrder = 0;       // the order the squad in play marches on
    float raidT = 0;               // Hold: the time to the second wave
    std::vector<std::pair<Gid, std::string>> names;   // settlement names looked up (siteName)
    int nextSlot = 0;
  };
  Runtime rt;

  bool enlisted() const { return sworn != 0 && rank != Rank::None; }
  bool deserterOf(Gid k) const;    // the player deserted that army (it will not take them back)
  Order* order(uint32_t id);
  Mission* mission(uint32_t id);
  void serialize(std::vector<uint8_t>& out) const;
  bool deserialize(const std::vector<uint8_t>& in);
};

}  // namespace service
