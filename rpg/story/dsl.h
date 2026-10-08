// M4 "Banners": the story DSL (owner 15.9): its data, parser and validator. STORY lane.
//
// THE LANGUAGE. One command per line; '#' starts a comment; "quoted text" is one word. Text is upper case (the HUD's
// font) and may name the cast with placeholders: {role} (its name) or {role.field}.
//
//   script <id>                      begins a script (ids are unique across the library)
//   title <words>                    the journal title ("THE PRODIGAL'S RETURN")
//   archetype <words>                the story archetype it is built on (reports, tests)
//   tier 2|3                         2: a story quest, 3: a campaign
//   hook npc <trade>|board|herald|ruin|event <evtype>
//                                    where it starts: an NPC of that trade (innkeeper, priest, smith, merchant, guard,
//                                    farmer, hunter, villager, any), a notice board, a herald, a ruin, a realm event
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
//               realm peace <k> <k> | realm succession <k> <person> | realm event <evtype> <k> [<k>] [<site>]
//   placeholders: {role} its name; person/foe/ruler .he .him .his .man .son .lad; site .dir (from the player) .realm;
//               kingdom .lord; ruler .name .title; npc .town; ruin .old .lord .builder .years .cause .clue;
//               war .foe; and {player}
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace story {
namespace dsl {

enum class RoleKind : uint8_t { Giver, Person, Foe, Npc, Site, Capital, Kingdom, Ruler, Ruin, War, Event, COUNT };
enum class HookKind : uint8_t { Npc, Board, Herald, Ruin, Event, COUNT };
enum class CondType : uint8_t { Gold, Level, Bg, Rep, Fame, Var, NoVar, Have, Mark, NoMark, Female, War, COUNT };
enum class ObjType : uint8_t { None, Goto, Enter, Kill, Slay, Fetch, Use, Wait, War, Famine, Rep, Have, COUNT };
enum class EffType : uint8_t { Gold, Xp, Give, Take, Loot, Rep, Fame, Set, Add, Mark, Remember, Fact, Moves, Shop, Hide, Show,
                               Notice, Sound, Rumour, RealmWar, RealmFamine, RealmPeace, RealmSuccession, RealmEvent, COUNT };
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
};
struct EffDef {
  EffType type = EffType::Gold;
  std::vector<std::string> args;   // role / var / mark names
  std::string text;                 // quoted text (items, lines)
  int n = 0;
  int icon = 0;                     // art::Icon for give
  int ev = 0;                       // realm::EvType for realm event
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
};
struct RoleDef {
  std::string name;
  RoleKind kind = RoleKind::Giver;
  std::string a, b;                 // kind arguments (site type / near|far / kingdom role / trade / evtype)
  std::string at;                   // person, foe, npc: the site role where they are
  bool female = false;
  int trade = 0;                    // npc: Role
  int ev = 0;                       // event: realm::EvType
  int line = 0;
};
struct Script {
  std::string id, title, archetype;
  int tier = 2;
  HookKind hook = HookKind::Npc;
  int hookTrade = -1;               // npc hooks: Role (-1 any trade that talks)
  int hookEv = 0;                   // event hooks: realm::EvType
  std::string pitch, hint;
  std::vector<RoleDef> roles;
  std::map<std::string, int> vars;
  std::vector<StageDef> stages;
  std::string source;               // the library file it came from (errors)
  int line = 0;
  int role(const std::string& n) const { for (size_t i = 0; i < roles.size(); i++) if (roles[i].name == n) return (int)i; return -1; }
  int stage(const std::string& n) const { for (size_t i = 0; i < stages.size(); i++) if (stages[i].name == n) return (int)i; return -1; }
};

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

// the sources (lib_tales.cpp, lib_campaign.cpp)
const char* talesSource();
const char* campaignSource();

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
