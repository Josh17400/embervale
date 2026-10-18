// M4 "Banners": the story DSL (owner 15.9): its data, parser and validator. STORY lane.
//
// THE LANGUAGE. One command per line; '#' starts a comment; "quoted text" is one word. Text is upper case (the HUD's
// font) and may name the cast with placeholders: {role} (its name) or {role.field}.
//
//   script <id>                      begins a script (ids are unique across the library)
//   title <words>                    the journal title ("THE PRODIGAL'S RETURN")
//   archetype <words>                the story archetype it is built on (reports, tests)
//   tier 2|3                         2: a story quest, 3: a campaign
//   hook npc <trade>|board|herald|ruin|event <evtype>|companion <stake> [<fact>]
//                                    where it starts: an NPC of that trade (innkeeper, priest, smith, merchant, guard,
//                                    farmer, hunter, villager, any), a notice board, a herald, a ruin, a realm event,
//                                    (M8) a companion's own story from their stake (burned lostkin war revenge
//                                    heirloom debt exile faith ambition love), the fact their history must hold
//                                    (kinalive kindead veteran deserter; default any). The giver is the companion.
//   role ... stake                   (M8, companion quests) the role is bound from the companion's stake
//   pitch "<option label>"           the option the hook offers ("YOU LOOK TROUBLED.") or the board's notice line
//   hint "<line>"                    what the hook speaker says instead of their greeting while they have the story
//   role <name> <kind> [args]        a role the caster binds to a real entity:
//       giver                          the hook's speaker (npc hooks, heralds) or the hook's place (boards, ruins)
//       person male|female [at <site role>]   a named person in the hook's culture (spawned where they are)
//       foe male|female at <site role>        a named enemy (spawned in that site, fought to the death)
//       npc <trade> [at <site role>]          someone of that trade in a settlement (default: the hook's)
//       site home|<type> near|far      home: the hook's settlement; type cave|ruin|camp|village|town|city (near: within a
//                                      day or two, far: further)
//       capital <kingdom role>         the kingdom's capital (its seat of power)
//       kingdom home|rival             home: whose land the hook is in; rival: its nearest neighbour
//       ruler <kingdom role>           the kingdom's ruler (the realm's record; a succession the player made wins)
//       ruin near                      a ruin and its true record (realm::RuinRecord: 15.3)
//       war <kingdom role>             an ongoing war of that kingdom (the script cannot start without one)
//       event <evtype>                 the freshest such realm event that has reached the hook
//     (M6b, saga.h) the living world as cast:
//       resident any|kin|friend|mourner|needy|young|old [of <role>] [at <site role>]
//                                      a real resident of the M5 census of that settlement (default the hook's): any;
//                                      kin / friend of the `of` role (default the giver: a household member / a tie);
//                                      mourner (a grieving household); needy (hungry or penniless); young (< 18); old (60+).
//                                      Talked to like a person (their own body in the town)
//       beast near|far                 an M6 named unique (foes::namedInRegion): slain with `goal slay`
//       boss                           the M6 world boss of the hook's kingdom cell (foes::worldBossOf): `goal slay`
//   var <name> <value>               a variable (conditions and effects read and change it)
//   stage <name>                     begins a stage (the first one is where the script starts). Exactly one of:
//     talk <role>                      a dialogue with that role (giver, person, npc, ruler): `say` and `opt`s
//     goal <objective>                 an objective, then `then <stage>`
//     end success|fail                 an ending (its effects run; the quest closes)
//   and any of:
//     say "<text>"                     what is said (talk), what the speaker adds as you go (goal), the epilogue (end)
//     journal "<text>"                 the quest log's description while this stage runs
//     do <effect>                      run on entering the stage
//     opt "<label>" [if <cond>]... -> <stage>          a choice (hidden unless every condition holds)
//     opt "<label>" check <cond> -> <stage> else <stage>   a check: one choice, two outcomes
//   objectives: goto <site> | enter <site> | kill <n> <monster|bandit|undead|beast> [in <site>] | slay <foe> |
//               fetch "<item>" in <site> | use <prop> [in <site>] | wait <days> | war <kingdom> <kingdom> |
//               famine <site> | rep <kingdom> <n> | have "<item>"
//   conditions: gold <n> | level <n> | bg <background> | rep <kingdom> <n> | fame <n> | var <name> <n> (>= n) |
//               novar <name> <n> (< n) | have "<item>" | mark <name> | nomark <name> | female <role> | war <k> <k>
//   effects:    gold <n> | xp <n> | give "<item>" <icon> | take "<item>" | loot | rep <kingdom> <n> | fame <n> |
//               set <var> <n> | add <var> <n> | mark <name> | remember <role> "<line>" | fact "<line>" |
//               moves <person> <site> | shop <npc> "<line>" | hide <person> | show <person> | notice "<text>" |
//               sound fanfare|bell|quest|roar | rumour | realm war <k> <k> | realm famine <site> |
//               realm peace <k> <k> | realm succession <k> <person> | realm event <evtype> <k> [<k>] [<site>] |
//               (M6b) reward small|fair|rich|great (gold, XP and for rich / great a piece of gear, all inside the M6
//               band of the hook's danger: slow rags to riches) | secret <site> (progress toward a smithing secret of
//               that place's culture: craft HOW_QUEST) | befriend <resident> (the resident becomes the player's friend) |
//               (M8 Fellowship) deed <word> [grave] (a deed of the player's on the companions' deeds bus, Game::deed:
//               questdone failed rescued refugees charity befriended mercy cruelty innocent guard stole bandits beast
//               monster war defended ruin tavern prayed bribed lied oathbreak grave abandoned) | recruit <role> (a
//               rescued or sworn person or resident joins the party) | approve <n> / bond <n> (companion quests: the
//               companion's approval / bond change) | fate home|part (companion quests: they go home to stay a while /
//               part from the player for good, their life resumed)
//   placeholders: {role} its name; person/foe/ruler .he .him .his .man .son .lad .brother (M6b: .father .husband .sir
//               .boy: MOTHER, WIFE, LADY, GIRL for a woman); site .dir (from the player) .realm;
//               kingdom .lord; ruler .name .title; npc .town; ruin .old .lord .builder .years .cause .clue;
//               war .foe; and {player}; (M6b) resident: the person fields + .job .kin (what they are to the `of`
//               role: WIFE, SON, FRIEND...) .lost (a mourner's dead, by name); beast / boss .kind ("WOLVES" / "FROST GIANT") .dir .realm; giver, person,
//               npc, resident, site, capital, ruin, kingdom: .god (the first god of its culture's faith) .people (its
//               culture's adjective: "VETHMARKI")
//
// (P1b "New Roads Content", lead phase A; VISION_PLAN 15.35) the shared mechanics of the 65 new items, as language:
//   family job|request|tale|test     what the item is (reports, rpg_test --content-p1b / --quest-mix; `test` scripts are
//                                    never offered: tests start them). Scripts without one are the older library.
//   settle <word>...                 where it may be OFFERED (the pickers' precheck; a forced start casts anywhere):
//                                    tier words OR'd: village town city capital (city includes capitals); port (the
//                                    settlement has a harbour: archetype Port) and coast (the sea within reach) are AND'd
//   needs <cond> [args]              more offer gates, all AND'd (one per line): nowar | war | coast | season <s>... |
//                                    stock <good> below|above <n> | mood below|above <n> | flag <moodflag> |
//                                    building <type> [<n>] (the hook settlement has n such buildings) | level <n>
//   once world|site                  world (default): never offered again once played; site: once per settlement (the
//                                    same request may come up in another town)
//   cooldown <days>                  a settlement offers no `cooldown` script for this many days after one ended there
//   gossip "<line>"                  (0-3) an overheard line (no placeholders, <= 38) the townsfolk may say while the item
//                                    is on offer in their town: a journal LEAD pointing at the teller (life gossip)
//   role ... person male|female [young] in <site>   (instead of `at`) the person waits INSIDE that cave / ruin / camp
//                                    map (SITE-TALK); young: a child or youth (the look). Until the ENGINE lane places
//                                    them inside, they stand at the site's mouth like `at`
//   role ... site <type> near|far [with <filter>]...   filters: beast <monster> (a cave or ruin whose creatures are that
//                                    kind) | min <tiles> | max <tiles> | road (within a few tiles of a road) | flag
//                                    <moodflag> (a settlement whose census shows it: wartorn, raided, famine...) | makes
//                                    <good> (a settlement that produces it); types also watchtower (a hilltop watchtower
//                                    vignette) and dwelling (a lone hut, camp or garden vignette). road / flag / makes and
//                                    the two vignette types are cast by the ENGINE lane: until then such a role refuses
//   stage attributes:
//     deadline <days> -> <stage>     if the stage is still current `days` days after it began, go there (a hard clock;
//                                    the tracker shows the days left). On talk and goal stages.
//     else <stage>                   (escort, bout) where the goal goes when it is lost
//   objectives: escort <person> to <site> [wagon|drove|walk]   the person follows the player across the world (a wagon or
//                                    a small drove beside them); done when both reach the place; lost (else) if they die
//               bout <person|foe>    a fight not to the death: they yield at a quarter health (done); the player at
//                                    nothing is left on their feet, loses (else), no gold lost
//               time <from> <to>     done when the time of day is in [from, to) (dusk, the small hours: 19 23, 0 4)
//   conditions: is <var> <n> (== n; `var x 0` is refused: it is always true) | days <n> (n days or more since the story
//               began) | nodays <n> (fewer) | season spring|summer|autumn|winter | hour <from> <to> (the time of day in
//               [from, to), wrapping midnight)
//   effects:    days <var> (var = days since the story began) | census <site> mood <+-n> | census <site> stock <good>
//               <+-n> | census <site> price <good> <percent> | census <site> famine <days> | census <site> festival |
//               census <site> refugees <n> | follow <person> / stay <person> (walks behind the player / stops) |
//               hostile <person> (turns on the player: a fight to the death; slay completes on them) | calm <site>
//               (the site's creatures stand down: no more fights there for this story) | ambush <monster|bandit|beast>
//               <n> (n hostile foes come at the player from 8-12 tiles off, now: a road ambush, watchers at dusk; outdoors
//               only, nothing indoors) | give "<item>" <icon> heavy (a
//               burden: the player walks slower while carrying it) | reward <band> <percent> (the band's pay times
//               percent / 100, 10..300)
//   placeholders: {var.<name>} a variable's value as a word ("THREE"; digits past twelve)
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace story {
namespace dsl {

enum class RoleKind : uint8_t { Giver, Person, Foe, Npc, Site, Capital, Kingdom, Ruler, Ruin, War, Event,
                                Resident, Beast, Boss,   // M6b (saga.h)
                                COUNT };
enum class HookKind : uint8_t { Npc, Board, Herald, Ruin, Event,
                                Companion,   // (M8) a companion's own personal quest: `hook companion <stake> [<fact>]`
                                COUNT };
enum class CondType : uint8_t { Gold, Level, Bg, Rep, Fame, Var, NoVar, Have, Mark, NoMark, Female, War,
                                Is, Days, NoDays, Season, Hour,   // P1b
                                COUNT };
enum class ObjType : uint8_t { None, Goto, Enter, Kill, Slay, Fetch, Use, Wait, War, Famine, Rep, Have,
                               Escort, Bout, Time,   // P1b
                               COUNT };
enum class EffType : uint8_t { Gold, Xp, Give, Take, Loot, Rep, Fame, Set, Add, Mark, Remember, Fact, Moves, Shop, Hide, Show,
                               Notice, Sound, Rumour, RealmWar, RealmFamine, RealmPeace, RealmSuccession, RealmEvent,
                               Reward, Secret, Befriend,   // M6b (saga.h)
                               Deed, Recruit, Approve, Bond, Fate,   // M8 Fellowship (companions)
                               Days, Census, Follow, Stay, Hostile, Calm, Ambush,   // P1b
                               COUNT };
// (P1b) census <site> <what>: EffDef::n the field, args {site, good word}, ev the amount
enum CensusField : uint8_t { CEN_MOOD, CEN_STOCK, CEN_PRICE, CEN_FAMINE, CEN_FESTIVAL, CEN_REFUGEES, CEN_COUNT };
// (P1b) escort skins (ObjDef::n)
constexpr int ESCORT_WALK = 0, ESCORT_WAGON = 1, ESCORT_DROVE = 2;
enum class StageKind : uint8_t { None, Talk, Goal, End };

// monster words for kill objectives: an art::Monster index, or these
constexpr int MON_BANDIT = -2, MON_UNDEAD = -3, MON_BEAST = -4, MON_ANY = -5;

struct CondDef {
  CondType type = CondType::Gold;
  std::string a, b;     // role / variable / item / background / mark names
  int n = 0;
};
struct ObjDef {
  ObjType type = ObjType::None;
  std::string role, role2, item;
  int n = 0;
  int monster = MON_ANY;
  int prop = -1;        // art::Prop for use
                        // (P1b) escort: role the person, role2 the place, n the skin (ESCORT_*); bout: role the opponent;
                        // time: n the first hour, prop the last (the time of day in [n, prop), wrapping midnight)
};
struct EffDef {
  EffType type = EffType::Gold;
  std::vector<std::string> args;   // role / var / mark names
  std::string text;                 // quoted text (items, lines)
  int n = 0;
  int icon = 0;                     // art::Icon for give
  int ev = 0;                       // realm::EvType for realm event
                                    // (M6b) reward: n = 0 small, 1 fair, 2 rich, 3 great
                                    // (M8) deed: n = party::Deed, ev = its weight (1, or 2 for `grave`); approve /
                                    // bond: n the change; fate: args[0] home|part; recruit: args[0] the role
                                    // (P1b) give: ev 1 = heavy (a burden); reward: ev the percent (0 = 100);
                                    // census: n the CensusField, args {site[, good]}, ev the amount; days: args {var};
                                    // ambush: n the monster word (monsterWord: an art::Monster or MON_BANDIT / MON_BEAST /
                                    // MON_UNDEAD), ev how many
};
struct OptDef {
  std::string label;
  std::vector<CondDef> ifs;
  bool check = false;
  CondDef chk;
  std::string to, orElse;
  int toIx = -1, elseIx = -1;
  int line = 0;
};
struct StageDef {
  std::string name;
  StageKind kind = StageKind::None;
  std::string talk;                 // talk: the role
  std::string say, journal;
  ObjDef goal;
  std::string then;
  int thenIx = -1;
  bool success = true;              // end
  std::vector<EffDef> effs;
  std::vector<OptDef> opts;
  int line = 0;
  // (P1b) deadline <days> -> <stage>: the clock (0 none); else <stage>: where a lost escort or bout goes
  int deadline = 0;
  std::string deadlineTo, orElse;
  int deadlineIx = -1, elseIx = -1;
};
struct RoleDef {
  std::string name;
  RoleKind kind = RoleKind::Giver;
  std::string a, b;                 // kind arguments (site type / near|far / kingdom role / trade / evtype; resident: a
                                    // the relation word, b the `of` role)
  std::string at;                   // person, foe, npc: the site role where they are
  bool female = false;
  int trade = 0;                    // npc: Role
  int ev = 0;                       // event: realm::EvType
  int line = 0;
  bool stake = false;               // (M8) companion quests: bound from the companion's own stake (party::Stake) when it
                                    // has one (person: the stake's kin, named as the census knows them; beast: the
                                    // stake's foe; kingdom: the stake's kingdom; ruin: the stake's ruin; site: the
                                    // stake's place), else as the kind says
  bool inside = false;              // (P1b) person ... in <site>: waits inside that site's map (r.at holds the site)
  bool young = false;               // (P1b) person male|female young: a child or a youth (their look; .boy fits them)
  std::string title;                // (P1b fixer) person ... as <WORD>: what the dialogue panel calls them ("SHIP'S-MATE" -> "SHIP'S MATE")
  std::vector<std::string> with;    // (P1b) site filters: "beast wolf", "road", "min 60", "max 250" (one entry each)
};
// (P1b) an offer gate: `needs <word> [args]`
struct NeedDef { std::string word; std::vector<std::string> args; int line = 0; };
struct Script {
  std::string id, title, archetype;
  int tier = 2;
  HookKind hook = HookKind::Npc;
  int hookTrade = -1;               // npc hooks: Role (-1 any trade that talks)
  int hookEv = 0;                   // event hooks: realm::EvType; (M8) companion hooks: the stake fact (COMPANION_*)
  std::string pitch, hint;
  std::vector<RoleDef> roles;
  std::map<std::string, int> vars;
  std::vector<StageDef> stages;
  std::string source;               // the library file it came from (errors)
  int line = 0;
  // (P1b) what it is and where it may be offered (dsl.h header words; the pickers check them, a forced start does not)
  std::string family;               // "job" | "request" | "tale" | "test" | "" (the older library)
  std::vector<std::string> settle;  // tier words (village town city capital) OR'd; port / coast AND'd
  std::vector<NeedDef> needs;
  bool onceSite = false;            // once site: once per settlement (else once per world)
  int cooldown = 0;                 // days
  std::vector<std::string> gossip;  // overheard lines
  int role(const std::string& n) const { for (size_t i = 0; i < roles.size(); i++) if (roles[i].name == n) return (int)i; return -1; }
  int stage(const std::string& n) const { for (size_t i = 0; i < stages.size(); i++) if (stages[i].name == n) return (int)i; return -1; }
};

// (M8) companion hooks: Script::hookTrade is the stake (party::Hook 1..10, companionHookWord), hookEv the fact the
// companion's history must hold for the quest to fit
constexpr int COMPANION_ANY = 0, COMPANION_KIN_ALIVE = 1, COMPANION_KIN_DEAD = 2, COMPANION_VETERAN = 3, COMPANION_DESERTER = 4;
int companionHookWord(const std::string& w);           // "burned" "lostkin" "war" "revenge" "heirloom" "debt" "exile" "faith"
                                                       // "ambition" "love" -> 1..10, -1 unknown
int companionFactWord(const std::string& w);           // "any" "kinalive" "kindead" "veteran" "deserter" -> COMPANION_*, -1
// (M8) the deed words of `do deed <word> [grave]`: party::Deed or -1
int deedWord(const std::string& w);

struct Library {
  std::vector<Script> scripts;
  std::vector<std::string> errors;  // parse and validation problems ("tales:120 prodigal: unknown stage 'hom'")
  int find(const std::string& id) const { for (size_t i = 0; i < scripts.size(); i++) if (scripts[i].id == id) return (int)i; return -1; }
};
// the library: every source parsed, linked and validated at first use
const Library& library();
// parse one source into scripts (errors appended); validate one script (problems returned)
void parse(const char* text, const char* srcName, std::vector<Script>& out, std::vector<std::string>& errors);
std::vector<std::string> validate(const Script& s);
// (M6b) resolve a parsed script's stage links (thenIx, toIx, elseIx): the library does it for its own; the composer for
// what it composes
void link(Script& s);
// (M6b) the resident relation words ("any", "kin", "friend", "mourner", "needy", "young", "old"): index or -1
int relationWord(const std::string& w);
// (M6b) the reward words ("small", "fair", "rich", "great"): 0..3 or -1
int rewardWord(const std::string& w);

// the sources (lib_tales.cpp, lib_campaign.cpp)
const char* talesSource();
const char* campaignSource();
// (P1b) the new items' sources: the REQUESTS lane's (rpg/story/req/village.cpp, town.cpp, city.cpp), the TALES lane's
// (rpg/story/tales2/lib_tales2.cpp) and the lead's mechanics test scripts (rpg/story/lib_p1b_test.cpp; family test)
const char* requestsVillageSource();
const char* requestsTownSource();
const char* requestsCitySource();
const char* tales2Source();
const char* p1bTestSource();
// (P1b) the words of the new headers and conditions (dsl.cpp): season 0..3 / -1, a ew::Good / -1, a mood flag bit / 0,
// an art::Building / -1
int seasonWord(const std::string& w);
int goodWord(const std::string& w);
int moodFlagWord(const std::string& w);
int buildingWord(const std::string& w);
// (P1b) is this an offer gate word the pickers know (settle / needs / with filters)?
bool settleWordOk(const std::string& w);

// word lists (the parser's and the engine's)
int monsterWord(const std::string& w);        // MON_* or an art::Monster index; -99 unknown
int propWord(const std::string& w);           // art::Prop or -1
int tradeWord(const std::string& w);          // Role or -1 ("any": -2)
int evWord(const std::string& w);             // realm::EvType or -1
int iconWord(const std::string& w);           // art::Icon or -1
int backgroundWord(const std::string& w);     // Background or -1
const char* tradeName(int role);              // "INNKEEPER"...

// placeholders in a text: every {role} / {role.field} it names
std::vector<std::pair<std::string, std::string>> placeholders(const std::string& text);
// words of plain text (reports)
int wordCount(const std::string& text);

}  // namespace dsl
}  // namespace story
