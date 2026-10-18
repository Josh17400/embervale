// M4 "Banners": the story DSL's parser and validator (rpg/story/dsl.h documents the language). STORY lane.
#include "rpg/story/dsl.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art.h"
#include "rpg/sim/backgrounds.h"
#include "rpg/sim/realm.h"
#include "rpg/sim/world.h"
#include "rpg/world/economy.h"

namespace story {
namespace dsl {

namespace {

struct Tok { std::string s; bool quoted = false; };

std::string lower(std::string s) {
  for (char& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

// a line into words; "quoted text" is one word (no escapes: the library never needs a quote inside a line)
bool tokenize(const std::string& line, std::vector<Tok>& out, std::string& err) {
  out.clear();
  size_t i = 0;
  while (i < line.size()) {
    const char c = line[i];
    if (c == ' ' || c == '\t' || c == '\r') { i++; continue; }
    if (c == '#') break;
    if (c == '"') {
      const size_t e = line.find('"', i + 1);
      if (e == std::string::npos) { err = "unterminated quote"; return false; }
      out.push_back({line.substr(i + 1, e - i - 1), true});
      i = e + 1;
      continue;
    }
    size_t e = i;
    while (e < line.size() && line[e] != ' ' && line[e] != '\t' && line[e] != '\r') e++;
    out.push_back({line.substr(i, e - i), false});
    i = e;
  }
  return true;
}

int num(const std::string& s, bool& ok) {
  if (s.empty()) { ok = false; return 0; }
  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (!end || *end) { ok = false; return 0; }
  return (int)v;
}

std::string joinFrom(const std::vector<Tok>& t, size_t from) {
  std::string r;
  for (size_t k = from; k < t.size(); k++) { if (k > from) r += ' '; r += t[k].s; }
  return r;
}

}  // namespace

// ---------------------------------------------------------------- word lists
int monsterWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"wolf", "boar", "bear", "slime", "spider", "bat", "skeleton", "draugr", "goblin", "troll", "wraith",
                                "mudcrab", "icewolf", "frostspider", "sandworm", "dragon", "scorpion", "hyena", "lurker", "yeti",
                                "wisp", "emberhound", "blightspawn", "harpy", "golem", "seaserpent", "kraken", "merrow"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)art::Monster::COUNT, "a word for every monster");
  for (int i = 0; i < (int)art::Monster::COUNT; i++) if (w == names[i]) return i;
  if (w == "bandit" || w == "bandits") return MON_BANDIT;
  if (w == "undead") return MON_UNDEAD;
  if (w == "beast" || w == "beasts") return MON_BEAST;
  if (w == "any") return MON_ANY;
  return -99;
}
int propWord(const std::string& w0) {
  const std::string w = lower(w0);
  if (w == "noticeboard" || w == "board") return (int)art::Prop::NoticeBoard;
  if (w == "inscription") return (int)art::Prop::Inscription;
  if (w == "statue") return (int)art::Prop::ToppledStatue;
  if (w == "mural") return (int)art::Prop::Mural;
  if (w == "grave") return (int)art::Prop::NamedGrave;
  if (w == "journal") return (int)art::Prop::LostJournal;
  if (w == "shrine") return (int)art::Prop::Shrine;
  if (w == "altar") return (int)art::Prop::Altar;
  if (w == "stones") return (int)art::Prop::StandingStone;
  if (w == "cairn") return (int)art::Prop::GraveCairn;
  if (w == "beacon" || w == "watchtower") return (int)art::Prop::WatchtowerRuin;   // (P1b) a hilltop tower's fire basket
  // (P1b) everyday props a story may send the player to search or use (the ENGINE lane wires the interaction)
  if (w == "crate") return (int)art::Prop::Crate;
  if (w == "barrel") return (int)art::Prop::Barrel;
  if (w == "chest") return (int)art::Prop::Chest;
  if (w == "well") return (int)art::Prop::Well;
  if (w == "brazier") return (int)art::Prop::Brazier;
  if (w == "sacks") return (int)art::Prop::Sacks;
  return -1;
}
int tradeWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const std::pair<const char*, Role> t[] = {
      {"villager", Role::Villager}, {"guard", Role::Guard}, {"merchant", Role::Merchant}, {"smith", Role::Smith},
      {"innkeeper", Role::Innkeeper}, {"priest", Role::Priest}, {"jarl", Role::Jarl}, {"farmer", Role::Farmer},
      {"mage", Role::Mage}, {"king", Role::King}, {"hunter", Role::Hunter}, {"fisher", Role::Fisher},
      {"herbalist", Role::Herbalist}, {"traveller", Role::Traveller}, {"herald", Role::Herald},
      // (P1b) the harbour's people (M10 dock folk) as hook trades
      {"harbourmaster", Role::Harbourmaster}, {"shipwright", Role::Shipwright}, {"sailor", Role::Sailor}};
  for (const auto& p : t) if (w == p.first) return (int)p.second;
  if (w == "any") return -2;
  return -1;
}
const char* tradeName(int role) {
  switch ((Role)role) {
    case Role::Villager: return "VILLAGER";
    case Role::Guard: return "GUARD";
    case Role::Merchant: return "MERCHANT";
    case Role::Smith: return "SMITH";
    case Role::Innkeeper: return "INNKEEPER";
    case Role::Priest: return "PRIEST";
    case Role::Jarl: return "LORD";
    case Role::Farmer: return "FARMER";
    case Role::Mage: return "MAGE";
    case Role::King: return "KING";
    case Role::Hunter: return "HUNTER";
    case Role::Fisher: return "FISHER";
    case Role::Herbalist: return "HERBALIST";
    case Role::Traveller: return "TRAVELLER";
    case Role::Herald: return "HERALD";
    case Role::Harbourmaster: return "HARBOURMASTER";
    case Role::Shipwright: return "SHIPWRIGHT";
    case Role::Sailor: return "SAILOR";
    case Role::Steward: return "STEWARD";   // (M12)
    case Role::Warlord: return "WARLORD";
    case Role::Envoy: return "ENVOY";
    default: return "VILLAGER";
  }
}
int evWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"firstcontact", "tradedeal", "tradebroken", "harvestfailed", "famine", "borderincident", "skirmish",
                                "wardeclared", "siegebegun", "siegebroken", "towntaken", "townburned", "refugees", "peace",
                                "ruleddied", "succession", "civilwar", "kingdomfell", "kingdomrose", "resettled", "ruined",
                                "festival", "troopsmarching", "pricesrising", "beastraid", "beastslain",
                                "warlordrose", "warbandraid", "successorrose", "reunified", "alliancemade", "allyjoined",
                                "vassalised", "tributepaid", "tributerefused", "independence", "fiefgranted", "enlisted",
                                "promoted", "termscarried", "bosswoke", "rebuilt"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)realm::EvType::COUNT, "a word for every event");
  for (int i = 0; i < (int)realm::EvType::COUNT; i++) if (w == names[i]) return i;
  if (w == "war") return (int)realm::EvType::WarDeclared;
  if (w == "siege") return (int)realm::EvType::SiegeBegun;
  if (w == "burned") return (int)realm::EvType::TownBurned;
  if (w == "taken") return (int)realm::EvType::TownTaken;
  return -1;
}
int iconWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const std::pair<const char*, art::Icon> t[] = {
      {"letter", art::Icon::Letter}, {"scroll", art::Icon::Scroll}, {"book", art::Icon::Book}, {"ring", art::Icon::Ring},
      {"amulet", art::Icon::Amulet}, {"key", art::Icon::Key}, {"gem", art::Icon::Gem}, {"crown", art::Icon::Crown},
      {"sigil", art::Icon::Sigil}, {"map", art::Icon::Map}, {"herb", art::Icon::Herb}, {"bone", art::Icon::Bone},
      {"gold", art::Icon::Gold}, {"dagger", art::Icon::Dagger}, {"sword", art::Icon::Sword},
      // (P1b) more of the existing item icons a quest item may wear
      {"cheese", art::Icon::Cheese}, {"bread", art::Icon::Bread}, {"meat", art::Icon::Meat}, {"apple", art::Icon::Apple},
      {"pelt", art::Icon::Pelt}, {"ore", art::Icon::Ore}, {"gloves", art::Icon::Gloves}, {"cloak", art::Icon::Cloak},
      {"potion", art::Icon::PotionRed}, {"tonic", art::Icon::PotionGreen}, {"seeds", art::Icon::Seeds},
      {"sheaf", art::Icon::Sheaf}, {"veg", art::Icon::Veg}, {"ingot", art::Icon::Ingot}, {"boots", art::Icon::Boots}};
  for (const auto& p : t) if (w == p.first) return (int)p.second;
  return -1;
}
// (P1b) the words of the offer gates
int seasonWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"spring", "summer", "autumn", "winter"};
  for (int i = 0; i < 4; i++) if (w == names[i]) return i;
  return -1;
}
int goodWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"grain", "flour", "bread", "produce", "fish", "ore", "ingot", "tools", "logs", "planks", "wool", "cloth",
                                "livestock", "meat", "hides", "leather", "pottery"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)ew::Good::COUNT, "a word for every good");
  for (int i = 0; i < (int)ew::Good::COUNT; i++) if (w == names[i]) return i;
  if (w == "ingots") return (int)ew::Good::Ingot;
  return -1;
}
int moodFlagWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"content", "festival", "hungry", "famine", "wartorn", "grief", "brawls", "emigrating", "shuttered", "raided"};
  for (int i = 0; i < 10; i++) if (w == names[i]) return 1 << i;   // life.h MF_* bits in this order
  return 0;
}
int buildingWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"house", "stonehouse", "inn", "smithy", "shop", "temple", "keep", "tower", "farmhouse", "hut", "palace",
                                "barracks", "windmill", "watermill", "granary", "bakery", "butcher", "tanner", "fishmonger", "smelter",
                                "sawmill", "weaver", "guildhall", "exchange", "meadhall", "bathhouse", "teahouse", "lodge", "councilhall"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)art::Building::COUNT, "a word for every building");
  for (int i = 0; i < (int)art::Building::COUNT; i++) if (w == names[i]) return i;
  return -1;
}
bool settleWordOk(const std::string& w0) {
  const std::string w = lower(w0);
  return w == "village" || w == "town" || w == "city" || w == "capital" || w == "port" || w == "coast";
}

int backgroundWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"none", "blacksmith", "hunter", "novice", "urchin", "farmhand", "noble", "sailor", "marked"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)Background::COUNT, "a word for every background");
  for (int i = 0; i < (int)Background::COUNT; i++) if (w == names[i]) return i;
  return -1;
}

int relationWord(const std::string& w0) {
  const std::string w = lower(w0);
  // (fixer M6b r1) child / spouse / parent / sibling: a precise relation to the `of` person (kin is any of them)
  static const char* names[] = {"any", "kin", "friend", "mourner", "needy", "young", "old", "child", "spouse", "parent", "sibling"};
  for (int i = 0; i < 11; i++) if (w == names[i]) return i;
  return -1;
}
int companionHookWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"burned", "lostkin", "war", "revenge", "heirloom", "debt", "exile", "faith", "ambition", "love"};
  for (int i = 0; i < 10; i++) if (w == names[i]) return i + 1;
  return -1;
}
int companionFactWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"any", "kinalive", "kindead", "veteran", "deserter"};
  for (int i = 0; i < 5; i++) if (w == names[i]) return i;
  return -1;
}
int deedWord(const std::string& w0) {
  const std::string w = lower(w0);
  // (party::Deed order)
  static const char* names[] = {"questdone", "failed", "rescued", "refugees", "charity", "befriended", "mercy", "cruelty", "innocent", "guard",
                                "stole", "bandits", "beast", "monster", "war", "defended", "ruin", "tavern", "prayed", "bribed", "lied",
                                "oathbreak", "grave", "abandoned"};
  for (int i = 0; i < 24; i++) if (w == names[i]) return i;
  return -1;
}
int rewardWord(const std::string& w0) {
  const std::string w = lower(w0);
  static const char* names[] = {"small", "fair", "rich", "great"};
  for (int i = 0; i < 4; i++) if (w == names[i]) return i;
  return -1;
}

std::vector<std::pair<std::string, std::string>> placeholders(const std::string& text) {
  std::vector<std::pair<std::string, std::string>> out;
  size_t i = 0;
  while ((i = text.find('{', i)) != std::string::npos) {
    const size_t e = text.find('}', i);
    if (e == std::string::npos) { out.push_back({"{", ""}); break; }
    const std::string in = lower(text.substr(i + 1, e - i - 1));
    const size_t dot = in.find('.');
    out.push_back(dot == std::string::npos ? std::make_pair(in, std::string()) : std::make_pair(in.substr(0, dot), in.substr(dot + 1)));
    i = e + 1;
  }
  return out;
}

int wordCount(const std::string& text) {
  int n = 0;
  bool in = false;
  for (char c : text) {
    const bool w = c != ' ';
    if (w && !in) n++;
    in = w;
  }
  return n;
}

// ---------------------------------------------------------------- the parser
namespace {

bool parseCond(const std::vector<Tok>& t, size_t& i, CondDef& c, std::string& err) {
  if (i >= t.size()) { err = "a condition is missing"; return false; }
  const std::string w = lower(t[i].s);
  bool ok = true;
  auto need = [&](size_t k) { if (i + k >= t.size()) { err = "condition '" + w + "' needs more words"; return false; } return true; };
  if (w == "gold" || w == "level" || w == "fame") {
    if (!need(1)) return false;
    c.type = w == "gold" ? CondType::Gold : w == "level" ? CondType::Level : CondType::Fame;
    c.n = num(t[i + 1].s, ok);
    i += 2;
  } else if (w == "bg") {
    if (!need(1)) return false;
    c.type = CondType::Bg; c.a = lower(t[i + 1].s);
    if (backgroundWord(c.a) < 0) { err = "unknown background '" + c.a + "'"; return false; }
    c.n = backgroundWord(c.a);
    i += 2;
  } else if (w == "rep") {
    if (!need(2)) return false;
    c.type = CondType::Rep; c.a = lower(t[i + 1].s); c.n = num(t[i + 2].s, ok);
    i += 3;
  } else if (w == "var" || w == "novar") {
    if (!need(2)) return false;
    c.type = w == "var" ? CondType::Var : CondType::NoVar; c.a = lower(t[i + 1].s); c.n = num(t[i + 2].s, ok);
    i += 3;
  } else if (w == "have") {
    if (!need(1)) return false;
    c.type = CondType::Have; c.a = t[i + 1].s;
    i += 2;
  } else if (w == "mark" || w == "nomark") {
    if (!need(1)) return false;
    c.type = w == "mark" ? CondType::Mark : CondType::NoMark; c.a = lower(t[i + 1].s);
    i += 2;
  } else if (w == "female") {
    if (!need(1)) return false;
    c.type = CondType::Female; c.a = lower(t[i + 1].s);
    i += 2;
  } else if (w == "war") {
    if (!need(2)) return false;
    c.type = CondType::War; c.a = lower(t[i + 1].s); c.b = lower(t[i + 2].s);
    i += 3;
  } else if (w == "is") {   // (P1b) equality
    if (!need(2)) return false;
    c.type = CondType::Is; c.a = lower(t[i + 1].s); c.n = num(t[i + 2].s, ok);
    i += 3;
  } else if (w == "days" || w == "nodays") {
    if (!need(1)) return false;
    c.type = w == "days" ? CondType::Days : CondType::NoDays; c.n = num(t[i + 1].s, ok);
    i += 2;
  } else if (w == "season") {
    if (!need(1)) return false;
    c.type = CondType::Season; c.a = lower(t[i + 1].s); c.n = seasonWord(c.a);
    if (c.n < 0) { err = "season spring|summer|autumn|winter"; return false; }
    i += 2;
  } else if (w == "hour") {
    if (!need(2)) return false;
    c.type = CondType::Hour; c.n = num(t[i + 1].s, ok);
    bool ok2 = true;
    const int to = num(t[i + 2].s, ok2);
    if (!ok2) ok = false;
    c.a = std::to_string(to);
    if (ok && (c.n < 0 || c.n > 24 || to < 0 || to > 24)) { err = "hour <from 0..24> <to 0..24>"; return false; }
    i += 3;
  } else {
    err = "unknown condition '" + w + "'";
    return false;
  }
  if (!ok) { err = "condition '" + w + "' needs a number"; return false; }
  return true;
}

bool parseObj(const std::vector<Tok>& t, ObjDef& o, std::string& err) {
  // t[0] == "goal"
  if (t.size() < 2) { err = "goal needs an objective"; return false; }
  const std::string w = lower(t[1].s);
  bool ok = true;
  auto argc = [&](size_t n) { if (t.size() < n + 2) { err = "objective '" + w + "' needs more words"; return false; } return true; };
  if (w == "goto" || w == "enter") {
    if (!argc(1)) return false;
    o.type = w == "goto" ? ObjType::Goto : ObjType::Enter; o.role = lower(t[2].s);
  } else if (w == "kill") {
    if (!argc(2)) return false;
    o.type = ObjType::Kill; o.n = num(t[2].s, ok); o.monster = monsterWord(t[3].s);
    if (o.monster == -99) { err = "unknown monster '" + t[3].s + "'"; return false; }
    if (t.size() >= 6 && lower(t[4].s) == "in") o.role = lower(t[5].s);
  } else if (w == "slay") {
    if (!argc(1)) return false;
    o.type = ObjType::Slay; o.role = lower(t[2].s);
  } else if (w == "fetch") {
    if (!argc(3) || !t[2].quoted || lower(t[3].s) != "in") { err = "fetch \"<item>\" in <site>"; return false; }
    o.type = ObjType::Fetch; o.item = t[2].s; o.role = lower(t[4].s);
  } else if (w == "use") {
    if (!argc(1)) return false;
    o.type = ObjType::Use; o.prop = propWord(t[2].s);
    if (o.prop < 0) { err = "unknown prop '" + t[2].s + "'"; return false; }
    if (t.size() >= 5 && lower(t[3].s) == "in") o.role = lower(t[4].s);
  } else if (w == "wait") {
    if (!argc(1)) return false;
    o.type = ObjType::Wait; o.n = num(t[2].s, ok);
  } else if (w == "war") {
    if (!argc(2)) return false;
    o.type = ObjType::War; o.role = lower(t[2].s); o.role2 = lower(t[3].s);
  } else if (w == "famine") {
    if (!argc(1)) return false;
    o.type = ObjType::Famine; o.role = lower(t[2].s);
  } else if (w == "rep") {
    if (!argc(2)) return false;
    o.type = ObjType::Rep; o.role = lower(t[2].s); o.n = num(t[3].s, ok);
  } else if (w == "have") {
    if (!argc(1) || !t[2].quoted) { err = "have \"<item>\""; return false; }
    o.type = ObjType::Have; o.item = t[2].s;
  } else if (w == "escort") {   // (P1b) escort <person> to <site> [wagon|drove|walk]
    if (!argc(3) || lower(t[3].s) != "to") { err = "escort <person> to <site> [wagon|drove|walk]"; return false; }
    o.type = ObjType::Escort; o.role = lower(t[2].s); o.role2 = lower(t[4].s); o.n = ESCORT_WALK;
    if (t.size() >= 6) {
      const std::string sk = lower(t[5].s);
      if (sk == "wagon") o.n = ESCORT_WAGON;
      else if (sk == "drove") o.n = ESCORT_DROVE;
      else if (sk != "walk") { err = "escort ... wagon|drove|walk"; return false; }
    }
  } else if (w == "bout") {     // (P1b) a fight not to the death
    if (!argc(1)) return false;
    o.type = ObjType::Bout; o.role = lower(t[2].s);
  } else if (w == "time") {     // (P1b) time <from> <to>: the time of day
    if (!argc(2)) return false;
    o.type = ObjType::Time; o.n = num(t[2].s, ok);
    bool ok2 = true;
    o.prop = num(t[3].s, ok2);
    if (!ok2) ok = false;
    if (ok && (o.n < 0 || o.n > 24 || o.prop < 0 || o.prop > 24 || o.n == o.prop)) { err = "time <from 0..24> <to 0..24>"; return false; }
  } else {
    err = "unknown objective '" + w + "'";
    return false;
  }
  if (!ok) { err = "objective '" + w + "' needs a number"; return false; }
  return true;
}

bool parseEff(const std::vector<Tok>& t, EffDef& e, std::string& err) {
  // t[0] == "do"
  if (t.size() < 2) { err = "do needs an effect"; return false; }
  const std::string w = lower(t[1].s);
  bool ok = true;
  auto argc = [&](size_t n) { if (t.size() < n + 2) { err = "effect '" + w + "' needs more words"; return false; } return true; };
  auto quoted = [&](size_t k) { if (t.size() <= k || !t[k].quoted) { err = "effect '" + w + "' needs \"text\""; return false; } e.text = t[k].s; return true; };
  if (w == "gold" || w == "xp" || w == "fame") {
    if (!argc(1)) return false;
    e.type = w == "gold" ? EffType::Gold : w == "xp" ? EffType::Xp : EffType::Fame;
    e.n = num(t[2].s, ok);
  } else if (w == "give") {
    if (!quoted(2) || t.size() < 4) { err = "give \"<item>\" <icon>"; return false; }
    e.type = EffType::Give; e.icon = iconWord(t[3].s);
    if (e.icon < 0) { err = "unknown icon '" + t[3].s + "'"; return false; }
    if (t.size() >= 5) { if (lower(t[4].s) != "heavy") { err = "give \"<item>\" <icon> [heavy]"; return false; } e.ev = 1; }
  } else if (w == "take") {
    if (!quoted(2)) return false;
    e.type = EffType::Take;
  } else if (w == "loot") {
    e.type = EffType::Loot;
  } else if (w == "rep") {
    if (!argc(2)) return false;
    e.type = EffType::Rep; e.args = {lower(t[2].s)}; e.n = num(t[3].s, ok);
  } else if (w == "set" || w == "add") {
    if (!argc(2)) return false;
    e.type = w == "set" ? EffType::Set : EffType::Add; e.args = {lower(t[2].s)}; e.n = num(t[3].s, ok);
  } else if (w == "mark") {
    if (!argc(1)) return false;
    e.type = EffType::Mark; e.args = {lower(t[2].s)};
  } else if (w == "remember") {
    if (!argc(2) || !quoted(3)) return false;
    e.type = EffType::Remember; e.args = {lower(t[2].s)};
  } else if (w == "fact") {
    if (!quoted(2)) return false;
    e.type = EffType::Fact;
  } else if (w == "moves") {
    if (!argc(2)) return false;
    e.type = EffType::Moves; e.args = {lower(t[2].s), lower(t[3].s)};
  } else if (w == "shop") {
    if (!argc(2) || !quoted(3)) return false;
    e.type = EffType::Shop; e.args = {lower(t[2].s)};
  } else if (w == "hide" || w == "show") {
    if (!argc(1)) return false;
    e.type = w == "hide" ? EffType::Hide : EffType::Show; e.args = {lower(t[2].s)};
  } else if (w == "notice") {
    if (!quoted(2)) return false;
    e.type = EffType::Notice;
  } else if (w == "sound") {
    if (!argc(1)) return false;
    e.type = EffType::Sound; e.args = {lower(t[2].s)};
    if (e.args[0] != "fanfare" && e.args[0] != "bell" && e.args[0] != "quest" && e.args[0] != "roar") { err = "unknown sound '" + e.args[0] + "'"; return false; }
  } else if (w == "rumour") {
    e.type = EffType::Rumour;
  } else if (w == "reward") {
    if (!argc(1)) return false;
    e.type = EffType::Reward; e.n = rewardWord(t[2].s);
    if (e.n < 0) { err = "reward small|fair|rich|great [percent]"; return false; }
    if (t.size() >= 4) {   // (P1b) the band's pay times percent / 100
      e.ev = num(t[3].s, ok);
      if (ok && (e.ev < 10 || e.ev > 300)) { err = "reward <band> <percent 10..300>"; return false; }
    }
  } else if (w == "secret") {
    if (!argc(1)) return false;
    e.type = EffType::Secret; e.args = {lower(t[2].s)};
  } else if (w == "befriend") {
    if (!argc(1)) return false;
    e.type = EffType::Befriend; e.args = {lower(t[2].s)};
  } else if (w == "deed") {
    if (!argc(1)) return false;
    e.type = EffType::Deed; e.n = deedWord(t[2].s); e.ev = 1;
    if (e.n < 0) { err = "unknown deed '" + t[2].s + "'"; return false; }
    if (t.size() >= 4) { if (lower(t[3].s) != "grave") { err = "deed <word> [grave]"; return false; } e.ev = 2; }
  } else if (w == "recruit") {
    if (!argc(1)) return false;
    e.type = EffType::Recruit; e.args = {lower(t[2].s)};
    if (t.size() >= 4) {
      const std::string how = lower(t[3].s);
      if (how != "rescued" && how != "sworn") { err = "recruit <role> [rescued|sworn]"; return false; }
      e.n = how == "sworn" ? 1 : 0;
    }
  } else if (w == "approve" || w == "bond") {
    if (!argc(1)) return false;
    e.type = w == "approve" ? EffType::Approve : EffType::Bond; e.n = num(t[2].s, ok);
  } else if (w == "fate") {
    if (!argc(1)) return false;
    e.type = EffType::Fate; e.args = {lower(t[2].s)};
    if (e.args[0] != "home" && e.args[0] != "part") { err = "fate home|part"; return false; }
  } else if (w == "days") {   // (P1b)
    if (!argc(1)) return false;
    e.type = EffType::Days; e.args = {lower(t[2].s)};
  } else if (w == "follow" || w == "stay" || w == "hostile" || w == "calm") {
    if (!argc(1)) return false;
    e.type = w == "follow" ? EffType::Follow : w == "stay" ? EffType::Stay : w == "hostile" ? EffType::Hostile : EffType::Calm;
    e.args = {lower(t[2].s)};
  } else if (w == "ambush") {   // ambush <monster|bandit|beast|undead> <n>
    if (!argc(2)) return false;
    e.type = EffType::Ambush; e.n = monsterWord(t[2].s); e.ev = num(t[3].s, ok);
    if (e.n == -99 || e.n == MON_ANY) { err = "ambush <monster|bandit|beast|undead> <n>"; return false; }
    if (ok && (e.ev < 1 || e.ev > 8)) { err = "ambush ... 1..8"; return false; }
  } else if (w == "census") {   // census <site> <field> [args]
    if (!argc(2)) return false;
    e.type = EffType::Census; e.args = {lower(t[2].s)};
    const std::string f = lower(t[3].s);
    if (f == "mood" || f == "famine" || f == "refugees") {
      if (!argc(3)) return false;
      e.n = f == "mood" ? CEN_MOOD : f == "famine" ? CEN_FAMINE : CEN_REFUGEES;
      e.ev = num(t[4].s, ok);
      if (ok && f == "mood" && (e.ev < -50 || e.ev > 50)) { err = "census <site> mood -50..50"; return false; }
      if (ok && f == "famine" && (e.ev < 1 || e.ev > 60)) { err = "census <site> famine 1..60 days"; return false; }
      if (ok && f == "refugees" && (e.ev < 1 || e.ev > 12)) { err = "census <site> refugees 1..12"; return false; }
    } else if (f == "stock" || f == "price") {
      if (!argc(4)) return false;
      e.n = f == "stock" ? CEN_STOCK : CEN_PRICE;
      if (goodWord(t[4].s) < 0) { err = "unknown good '" + t[4].s + "'"; return false; }
      e.args.push_back(lower(t[4].s));
      e.ev = num(t[5].s, ok);
      if (ok && f == "stock" && (e.ev < -200 || e.ev > 200)) { err = "census <site> stock <good> -200..200"; return false; }
      if (ok && f == "price" && (e.ev < 25 || e.ev > 400)) { err = "census <site> price <good> 25..400 percent"; return false; }
    } else if (f == "festival") {
      e.n = CEN_FESTIVAL;
    } else { err = "census <site> mood|stock|price|famine|festival|refugees"; return false; }
  } else if (w == "realm") {
    if (!argc(2)) return false;
    const std::string k = lower(t[2].s);
    if (k == "war" || k == "peace") {
      if (!argc(3)) return false;
      e.type = k == "war" ? EffType::RealmWar : EffType::RealmPeace; e.args = {lower(t[3].s), lower(t[4].s)};
    } else if (k == "famine") {
      e.type = EffType::RealmFamine; e.args = {lower(t[3].s)};
    } else if (k == "succession") {
      if (!argc(3)) return false;
      e.type = EffType::RealmSuccession; e.args = {lower(t[3].s), lower(t[4].s)};
    } else if (k == "event") {
      if (!argc(3)) return false;
      e.type = EffType::RealmEvent; e.ev = evWord(t[3].s);
      if (e.ev < 0) { err = "unknown event '" + t[3].s + "'"; return false; }
      for (size_t k2 = 4; k2 < t.size(); k2++) e.args.push_back(lower(t[k2].s));
    } else {
      err = "unknown realm effect '" + k + "'";
      return false;
    }
  } else {
    err = "unknown effect '" + w + "'";
    return false;
  }
  if (!ok) { err = "effect '" + w + "' needs a number"; return false; }
  return true;
}

}  // namespace

void parse(const char* text, const char* srcName, std::vector<Script>& out, std::vector<std::string>& errors) {
  std::string src = text ? text : "";
  Script* S = nullptr;
  StageDef* G = nullptr;
  int lineNo = 0;
  size_t pos = 0;
  auto error = [&](const std::string& m) {
    errors.push_back(std::string(srcName) + ":" + std::to_string(lineNo) + (S ? " " + S->id : std::string()) + ": " + m);
  };
  while (pos <= src.size()) {
    size_t e = src.find('\n', pos);
    if (e == std::string::npos) e = src.size();
    const std::string line = src.substr(pos, e - pos);
    pos = e + 1;
    lineNo++;
    std::vector<Tok> t;
    std::string err;
    if (!tokenize(line, t, err)) { error(err); continue; }
    if (t.empty()) continue;
    const std::string k = lower(t[0].s);
    if (k == "script") {
      if (t.size() < 2) { error("script needs an id"); continue; }
      out.push_back(Script());
      S = &out.back();
      S->id = lower(t[1].s);
      S->source = srcName;
      S->line = lineNo;
      G = nullptr;
      continue;
    }
    if (!S) { error("'" + k + "' before any script"); continue; }
    if (k == "title") { S->title = joinFrom(t, 1); continue; }
    if (k == "archetype") { S->archetype = joinFrom(t, 1); continue; }
    if (k == "tier") { bool ok = true; S->tier = t.size() > 1 ? num(t[1].s, ok) : 0; if (!ok || (S->tier != 2 && S->tier != 3)) error("tier 2 or 3"); continue; }
    if (k == "pitch") { if (t.size() < 2 || !t[1].quoted) error("pitch \"<label>\""); else S->pitch = t[1].s; continue; }
    if (k == "hint") { if (t.size() < 2 || !t[1].quoted) error("hint \"<line>\""); else S->hint = t[1].s; continue; }
    if (k == "hook") {
      const std::string h = t.size() > 1 ? lower(t[1].s) : std::string();
      if (h == "npc") {
        S->hook = HookKind::Npc;
        S->hookTrade = t.size() > 2 ? tradeWord(t[2].s) : -2;
        if (S->hookTrade == -1) error("unknown trade '" + (t.size() > 2 ? t[2].s : std::string()) + "'");
      } else if (h == "companion") {
        S->hook = HookKind::Companion;
        S->hookTrade = t.size() > 2 ? companionHookWord(t[2].s) : -1;
        S->hookEv = t.size() > 3 ? companionFactWord(t[3].s) : COMPANION_ANY;
        if (S->hookTrade < 0) error("hook companion needs a stake (burned lostkin war revenge heirloom debt exile faith ambition love)");
        if (S->hookEv < 0) error("hook companion: unknown fact '" + (t.size() > 3 ? t[3].s : std::string()) + "'");
      } else if (h == "board") S->hook = HookKind::Board;
      else if (h == "herald") S->hook = HookKind::Herald;
      else if (h == "ruin") S->hook = HookKind::Ruin;
      else if (h == "event") {
        S->hook = HookKind::Event;
        S->hookEv = t.size() > 2 ? evWord(t[2].s) : -1;
        if (S->hookEv < 0) error("hook event needs an event type");
      } else error("unknown hook '" + h + "'");
      continue;
    }
    // (P1b) what the item is and where it may be offered
    if (k == "family") {
      const std::string f = t.size() > 1 ? lower(t[1].s) : std::string();
      if (f != "job" && f != "request" && f != "tale" && f != "test") error("family job|request|tale|test");
      else S->family = f;
      continue;
    }
    if (k == "settle") {
      if (t.size() < 2) { error("settle <village|town|city|capital|port|coast>..."); continue; }
      for (size_t q = 1; q < t.size(); q++) {
        const std::string w = lower(t[q].s);
        if (!settleWordOk(w)) error("settle: unknown word '" + w + "' (village town city capital port coast)");
        else S->settle.push_back(w);
      }
      continue;
    }
    if (k == "needs") {
      if (t.size() < 2) { error("needs <cond> [args]"); continue; }
      NeedDef n;
      n.word = lower(t[1].s);
      n.line = lineNo;
      for (size_t q = 2; q < t.size(); q++) n.args.push_back(lower(t[q].s));
      bool ok = true;
      auto numArg = [&](size_t i) { return i < n.args.size() ? num(n.args[i], ok) : (ok = false, 0); };
      if (n.word == "nowar" || n.word == "war" || n.word == "coast") { if (!n.args.empty()) error("needs " + n.word + " takes no words"); }
      else if (n.word == "season") {
        if (n.args.empty()) error("needs season <spring|summer|autumn|winter>...");
        for (const std::string& a : n.args) if (seasonWord(a) < 0) error("needs season: unknown season '" + a + "'");
      } else if (n.word == "stock") {
        if (n.args.size() != 3 || goodWord(n.args[0]) < 0 || (n.args[1] != "below" && n.args[1] != "above")) error("needs stock <good> below|above <n>");
        else numArg(2);
      } else if (n.word == "mood") {
        if (n.args.size() != 2 || (n.args[0] != "below" && n.args[0] != "above")) error("needs mood below|above <n>");
        else numArg(1);
      } else if (n.word == "flag") {
        if (n.args.size() != 1 || !moodFlagWord(n.args[0])) error("needs flag <content|festival|hungry|famine|wartorn|grief|brawls|emigrating|shuttered|raided>");
      } else if (n.word == "building") {
        if (n.args.empty() || n.args.size() > 2 || buildingWord(n.args[0]) < 0) error("needs building <type> [<n>]");
        else if (n.args.size() == 2) numArg(1);
      } else if (n.word == "level") {
        if (n.args.size() != 1) error("needs level <n>");
        else numArg(0);
      } else error("unknown need '" + n.word + "' (nowar war coast season stock mood flag building level)");
      if (!ok) error("needs " + n.word + ": a number is missing");
      S->needs.push_back(n);
      continue;
    }
    if (k == "once") {
      const std::string w = t.size() > 1 ? lower(t[1].s) : std::string();
      if (w != "world" && w != "site") error("once world|site");
      else S->onceSite = w == "site";
      continue;
    }
    if (k == "cooldown") {
      bool ok = true;
      S->cooldown = t.size() > 1 ? num(t[1].s, ok) : 0;
      if (!ok || S->cooldown < 1 || S->cooldown > 120) error("cooldown <days 1..120>");
      continue;
    }
    if (k == "gossip") {
      if (t.size() < 2 || !t[1].quoted) error("gossip \"<line>\"");
      else S->gossip.push_back(t[1].s);
      continue;
    }
    if (k == "var") {
      bool ok = true;
      if (t.size() < 3) { error("var <name> <value>"); continue; }
      S->vars[lower(t[1].s)] = num(t[2].s, ok);
      if (!ok) error("var needs a number");
      continue;
    }
    if (k == "role") {
      if (t.size() < 3) { error("role <name> <kind> ..."); continue; }
      RoleDef r;
      // (M8) `stake` anywhere after the kind: bound from a companion's stake
      for (size_t q = 3; q < t.size(); q++)
        if (!t[q].quoted && lower(t[q].s) == "stake") { r.stake = true; t.erase(t.begin() + (std::ptrdiff_t)q); break; }
      r.name = lower(t[1].s);
      r.line = lineNo;
      const std::string kind = lower(t[2].s);
      auto atArg = [&](size_t from) {
        for (size_t q = from; q + 1 < t.size(); q++) if (lower(t[q].s) == "at") r.at = lower(t[q + 1].s);
      };
      if (kind == "giver") r.kind = RoleKind::Giver;
      else if (kind == "person" || kind == "foe") {
        r.kind = kind == "person" ? RoleKind::Person : RoleKind::Foe;
        const std::string sx = t.size() > 3 ? lower(t[3].s) : std::string();
        if (sx != "male" && sx != "female") { error("person/foe needs male or female"); continue; }
        r.female = sx == "female";
        atArg(4);
        for (size_t q = 4; q < t.size(); q++) if (lower(t[q].s) == "young") r.young = true;   // (P1b)
        for (size_t q = 4; q + 1 < t.size(); q++)   // (P1b fixer) person ... as <word>: the panel's title for them
          if (lower(t[q].s) == "as") {
            std::string w = t[q + 1].s;
            for (char& ch : w) { if (ch == '-') ch = ' '; else if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A'); }
            r.title = w;
          }
        if (!r.title.empty() && (r.kind != RoleKind::Person || r.title.size() > 14)) { error("'as <title>' is for a person, 14 letters at most"); continue; }
        if (r.young && r.kind != RoleKind::Person) { error("only a person is young"); continue; }
        // (P1b) person ... in <site>: inside that site's map
        for (size_t q = 4; q + 1 < t.size(); q++)
          if (lower(t[q].s) == "in") {
            if (r.kind != RoleKind::Person) { error("only a person waits 'in' a site (a foe is always inside)"); break; }
            if (!r.at.empty()) { error("a person is 'at' or 'in' a site, not both"); break; }
            r.at = lower(t[q + 1].s);
            r.inside = true;
          }
        if (r.kind == RoleKind::Foe && r.at.empty()) { error("a foe needs 'at <site>'"); continue; }
      } else if (kind == "npc") {
        r.kind = RoleKind::Npc;
        r.trade = t.size() > 3 ? tradeWord(t[3].s) : -1;
        if (r.trade < 0) { error("npc needs a trade"); continue; }
        atArg(4);
      } else if (kind == "site") {
        r.kind = RoleKind::Site;
        r.a = t.size() > 3 ? lower(t[3].s) : std::string();
        r.b = t.size() > 4 ? lower(t[4].s) : std::string("near");
        static const std::set<std::string> types = {"home", "cave", "ruin", "camp", "village", "town", "city",
                                                    "watchtower", "dwelling"};   // (P1b) vignettes
        if (!types.count(r.a)) { error("site home|cave|ruin|camp|village|town|city|watchtower|dwelling"); continue; }
        if (r.b != "near" && r.b != "far") { error("site ... near|far"); continue; }
        // (P1b) with <filter>: beast <monster> | road | min <tiles> | max <tiles> (any number of `with`)
        bool badWith = false;
        for (size_t q = 5; q < t.size() && !badWith; q++) {
          if (lower(t[q].s) != "with") { error("site ...: unexpected '" + t[q].s + "' (with <filter>)"); badWith = true; break; }
          const std::string f = q + 1 < t.size() ? lower(t[q + 1].s) : std::string();
          if (f == "road") { r.with.push_back("road"); q += 1; continue; }
          if ((f == "beast" || f == "min" || f == "max" || f == "flag" || f == "makes") && q + 2 < t.size()) {
            const std::string v = lower(t[q + 2].s);
            bool ok = true;
            if (f == "beast" && monsterWord(v) < 0) { error("with beast: unknown monster '" + v + "'"); badWith = true; break; }
            if (f == "flag" && !moodFlagWord(v)) { error("with flag: unknown mood flag '" + v + "'"); badWith = true; break; }
            if (f == "makes" && goodWord(v) < 0) { error("with makes: unknown good '" + v + "'"); badWith = true; break; }
            if ((f == "min" || f == "max") && (num(v, ok) < 1 || !ok)) { error("with " + f + " <tiles>"); badWith = true; break; }
            r.with.push_back(f + " " + v);
            q += 2;
            continue;
          }
          error("with beast <monster> | road | min <tiles> | max <tiles> | flag <moodflag> | makes <good>");
          badWith = true;
        }
        if (badWith) continue;
      } else if (kind == "capital" || kind == "ruler" || kind == "war") {
        r.kind = kind == "capital" ? RoleKind::Capital : kind == "ruler" ? RoleKind::Ruler : RoleKind::War;
        r.a = t.size() > 3 ? lower(t[3].s) : std::string();
        if (r.a.empty()) { error(kind + " needs a kingdom role"); continue; }
      } else if (kind == "kingdom") {
        r.kind = RoleKind::Kingdom;
        r.a = t.size() > 3 ? lower(t[3].s) : std::string("home");
        if (r.a != "home" && r.a != "rival") { error("kingdom home|rival"); continue; }
      } else if (kind == "ruin") {
        r.kind = RoleKind::Ruin;
      } else if (kind == "event") {
        r.kind = RoleKind::Event;
        r.ev = t.size() > 3 ? evWord(t[3].s) : -1;
        if (r.ev < 0) { error("event needs an event type"); continue; }
      } else if (kind == "resident") {
        r.kind = RoleKind::Resident;
        r.a = t.size() > 3 ? lower(t[3].s) : std::string("any");
        if (relationWord(r.a) < 0) { error("resident any|kin|friend|mourner|needy|young|old|child|spouse|parent|sibling"); continue; }
        for (size_t q = 4; q + 1 < t.size(); q++) if (lower(t[q].s) == "of") r.b = lower(t[q + 1].s);
        atArg(4);
      } else if (kind == "beast") {
        r.kind = RoleKind::Beast;
        r.a = t.size() > 3 ? lower(t[3].s) : std::string("near");
        if (r.a != "near" && r.a != "far") { error("beast near|far"); continue; }
      } else if (kind == "boss") {
        r.kind = RoleKind::Boss;
      } else { error("unknown role kind '" + kind + "'"); continue; }
      if (S->role(r.name) >= 0) { error("role '" + r.name + "' declared twice"); continue; }
      S->roles.push_back(r);
      continue;
    }
    if (k == "stage") {
      if (t.size() < 2) { error("stage needs a name"); continue; }
      if (S->stage(lower(t[1].s)) >= 0) { error("stage '" + lower(t[1].s) + "' declared twice"); continue; }
      S->stages.push_back(StageDef());
      G = &S->stages.back();
      G->name = lower(t[1].s);
      G->line = lineNo;
      continue;
    }
    if (!G) { error("'" + k + "' outside a stage"); continue; }
    auto setKind = [&](StageKind sk) {
      if (G->kind != StageKind::None) { error("stage '" + G->name + "' has two of talk / goal / end"); return false; }
      G->kind = sk;
      return true;
    };
    if (k == "talk") { if (t.size() < 2) error("talk <role>"); else if (setKind(StageKind::Talk)) G->talk = lower(t[1].s); continue; }
    if (k == "say") { if (t.size() < 2 || !t[1].quoted) error("say \"<text>\""); else G->say = t[1].s; continue; }
    if (k == "journal") { if (t.size() < 2 || !t[1].quoted) error("journal \"<text>\""); else G->journal = t[1].s; continue; }
    if (k == "goal") { ObjDef o; if (!parseObj(t, o, err)) error(err); else if (setKind(StageKind::Goal)) G->goal = o; continue; }
    if (k == "then") { if (t.size() < 2) error("then <stage>"); else G->then = lower(t[1].s); continue; }
    if (k == "else") { if (t.size() < 2) error("else <stage>"); else G->orElse = lower(t[1].s); continue; }   // (P1b)
    if (k == "deadline") {   // (P1b) deadline <days> -> <stage>
      bool ok = true;
      if (t.size() < 4 || lower(t[2].s) != "->") { error("deadline <days> -> <stage>"); continue; }
      G->deadline = num(t[1].s, ok);
      if (!ok || G->deadline < 1 || G->deadline > 60) { error("deadline 1..60 days"); continue; }
      G->deadlineTo = lower(t[3].s);
      continue;
    }
    if (k == "end") {
      const std::string r = t.size() > 1 ? lower(t[1].s) : std::string();
      if (r != "success" && r != "fail") { error("end success|fail"); continue; }
      if (setKind(StageKind::End)) G->success = r == "success";
      continue;
    }
    if (k == "do") { EffDef ef; if (!parseEff(t, ef, err)) error(err); else G->effs.push_back(ef); continue; }
    if (k == "opt") {
      if (t.size() < 4 || !t[1].quoted) { error("opt \"<label>\" ... -> <stage>"); continue; }
      OptDef o;
      o.label = t[1].s;
      o.line = lineNo;
      size_t i = 2;
      bool bad = false;
      while (i < t.size() && !bad) {
        const std::string w = lower(t[i].s);
        if (w == "if") { i++; CondDef c; if (!parseCond(t, i, c, err)) { error(err); bad = true; } else o.ifs.push_back(c); continue; }
        if (w == "check") { i++; o.check = true; if (!parseCond(t, i, o.chk, err)) { error(err); bad = true; } continue; }
        if (w == "->") { if (i + 1 >= t.size()) { error("-> needs a stage"); bad = true; } else o.to = lower(t[i + 1].s); i += 2; continue; }
        if (w == "else") { if (i + 1 >= t.size()) { error("else needs a stage"); bad = true; } else o.orElse = lower(t[i + 1].s); i += 2; continue; }
        error("unexpected '" + t[i].s + "' in an option");
        bad = true;
      }
      if (bad) continue;
      if (o.to.empty()) { error("an option without -> <stage>"); continue; }
      if (o.check && o.orElse.empty()) { error("a check without else <stage>"); continue; }
      G->opts.push_back(o);
      continue;
    }
    error("unknown command '" + k + "'");
  }
}

// ---------------------------------------------------------------- the validator
std::vector<std::string> validate(const Script& s) {
  std::vector<std::string> P;
  auto at = [&](int line) { return s.source + ":" + std::to_string(line) + " " + s.id + ": "; };
  auto bad = [&](int line, const std::string& m) { P.push_back(at(line) + m); };
  if (s.title.empty()) bad(s.line, "no title");
  if (s.archetype.empty()) bad(s.line, "no archetype");
  if (s.stages.empty()) { bad(s.line, "no stages"); return P; }
  if (s.pitch.empty()) bad(s.line, "no pitch (the hook's option)");
  if (s.pitch.size() > 40) bad(s.line, "the pitch is longer than 40 characters");
  const bool npcHook = s.hook == HookKind::Npc || s.hook == HookKind::Herald || s.hook == HookKind::Companion;
  const bool companion = s.hook == HookKind::Companion;
  auto kindOf = [&](const std::string& r) -> int { const int i = s.role(r); return i < 0 ? -1 : (int)s.roles[(size_t)i].kind; };
  auto siteLike = [&](const std::string& r) {
    const int k = kindOf(r);
    return k == (int)RoleKind::Site || k == (int)RoleKind::Capital || k == (int)RoleKind::Ruin || (k == (int)RoleKind::Giver && !npcHook);
  };
  auto needRole = [&](int line, const std::string& r, const char* what) {
    if (s.role(r) < 0) { bad(line, std::string("unbound role '") + r + "' (" + what + ")"); return false; }
    return true;
  };
  auto needKind = [&](int line, const std::string& r, RoleKind k, const char* what) {
    if (!needRole(line, r, what)) return;
    if (kindOf(r) != (int)k) bad(line, std::string("role '") + r + "' is the wrong kind for " + what);
  };
  // roles: arguments refer to roles declared before
  for (size_t i = 0; i < s.roles.size(); i++) {
    const RoleDef& r = s.roles[i];
    auto before = [&](const std::string& n) { const int j = s.role(n); return j >= 0 && j < (int)i; };
    if (!r.at.empty()) {
      if (!before(r.at)) bad(r.line, "role '" + r.name + "': '" + r.at + "' is not a role declared before it");
      else if (!siteLike(r.at)) bad(r.line, "role '" + r.name + "': '" + r.at + "' is not a place");
    }
    if (r.kind == RoleKind::Capital || r.kind == RoleKind::Ruler || r.kind == RoleKind::War) {
      if (!before(r.a)) bad(r.line, "role '" + r.name + "': '" + r.a + "' is not a role declared before it");
      else if (kindOf(r.a) != (int)RoleKind::Kingdom) bad(r.line, "role '" + r.name + "': '" + r.a + "' is not a kingdom");
    }
    if (r.kind == RoleKind::Giver && i != 0) bad(r.line, "the giver must be the first role");
    if (r.stake && !companion) bad(r.line, "role '" + r.name + "': 'stake' belongs to a companion's own story (hook companion)");
    if (r.stake && r.kind != RoleKind::Person && r.kind != RoleKind::Beast && r.kind != RoleKind::Kingdom && r.kind != RoleKind::Ruin &&
        r.kind != RoleKind::Site)
      bad(r.line, "role '" + r.name + "': a stake binds a person, beast, kingdom, ruin or site");
    if (r.kind == RoleKind::Resident && !r.b.empty()) {
      if (!before(r.b)) bad(r.line, "role '" + r.name + "': '" + r.b + "' is not a role declared before it");
      else {
        const int k = kindOf(r.b);
        if (k != (int)RoleKind::Giver && k != (int)RoleKind::Resident && k != (int)RoleKind::Person && k != (int)RoleKind::Npc)
          bad(r.line, "role '" + r.name + "': a resident is kin or friend of a person, not of '" + r.b + "'");
      }
    }
  }
  if (s.roles.empty() || s.roles[0].kind != RoleKind::Giver) bad(s.line, "the first role must be the giver");
  if (s.role("var") >= 0) bad(s.line, "'var' is not a role name ({var.<name>} spells a variable)");
  // (P1b) the new headers
  for (const RoleDef& r : s.roles)
    if (r.inside) {
      const int k = s.role(r.at) >= 0 ? (int)s.roles[(size_t)s.role(r.at)].kind : -1;
      const std::string& ty = k == (int)RoleKind::Site ? s.roles[(size_t)s.role(r.at)].a : std::string();
      if (k != (int)RoleKind::Ruin && !(k == (int)RoleKind::Site && (ty == "cave" || ty == "ruin" || ty == "camp")))
        bad(r.line, "role '" + r.name + "': 'in' needs a cave, ruin or camp (or a ruin role)");
    }
  if (!s.family.empty() && s.family != "test") {
    if (s.family == "request" && s.settle.empty()) bad(s.line, "a request says where it is raised (settle village|town|city|capital|port)");
    for (const std::string& gl : s.gossip) {
      if (!placeholders(gl).empty()) bad(s.line, "a gossip line has a placeholder (it is overheard before casting): " + gl);
      if (gl.size() > 38) bad(s.line, "a gossip line is longer than 38 characters: " + gl);
      for (char c : gl) if (c >= 'a' && c <= 'z') { bad(s.line, "lower case in a gossip line"); break; }
    }
    if (s.gossip.size() > 3) bad(s.line, "more than 3 gossip lines");
  }
  if (!s.gossip.empty() && s.family.empty()) bad(s.line, "gossip belongs to a P1b item (family job|request|tale)");
  // (M6b) the engine keys spawned people on (instance, role index): 32 roles at most (story_internal.h PERSON_ROLES)
  if (s.roles.size() > 32) bad(s.line, "more than 32 roles (" + std::to_string(s.roles.size()) + ")");
  // placeholders
  static const std::set<std::string> personF = {"he", "him", "his", "man", "son", "lad", "brother",
                                                "father", "husband", "sir", "boy"};   // (M6b: father .. boy)
  auto checkText = [&](int line, const std::string& txt, size_t maxLen, const char* what) {
    size_t len = 0;
    for (const auto& ph : placeholders(txt)) {
      if (ph.first == "{") { bad(line, std::string("an unclosed placeholder in the ") + what); continue; }
      if (ph.first == "player") continue;
      if (ph.first == "var") {   // (P1b) {var.<name>}: a declared variable spelled as a word
        if (ph.second.empty() || !s.vars.count(ph.second)) bad(line, "placeholder {var." + ph.second + "}: no such variable (" + what + ")");
        continue;
      }
      const int ri = s.role(ph.first);
      if (ri < 0) { bad(line, "placeholder {" + ph.first + "} has no binding (" + what + ")"); continue; }
      if (ph.second.empty()) continue;
      const RoleKind k = s.roles[(size_t)ri].kind;
      const std::string& f = ph.second;
      bool ok = false;
      // (M6b) the culture words of anyone or anywhere with a culture
      if ((f == "god" || f == "people") && k != RoleKind::War && k != RoleKind::Event && k != RoleKind::Ruler && k != RoleKind::Foe &&
          k != RoleKind::Beast && k != RoleKind::Boss)
        continue;
      switch (k) {
        case RoleKind::Person: case RoleKind::Foe: ok = personF.count(f) > 0 || (f == "kin" && s.roles[(size_t)ri].stake); break;
        case RoleKind::Resident: ok = personF.count(f) > 0 || f == "job" || f == "kin" || f == "lost"; break;
        case RoleKind::Beast: case RoleKind::Boss: ok = f == "kind" || f == "dir" || f == "realm"; break;
        case RoleKind::Ruler: ok = personF.count(f) > 0 || f == "name" || f == "title"; break;
        case RoleKind::Site: case RoleKind::Capital: ok = f == "dir" || f == "realm"; break;
        case RoleKind::Giver: ok = f == "dir" || f == "realm" || personF.count(f) > 0; break;
        case RoleKind::Kingdom: ok = f == "lord"; break;
        case RoleKind::Npc: ok = f == "town"; break;
        case RoleKind::Ruin: ok = f == "old" || f == "lord" || f == "builder" || f == "years" || f == "cause" || f == "clue" || f == "dir"; break;
        case RoleKind::War: ok = f == "foe"; break;
        default: break;
      }
      if (!ok) bad(line, "placeholder {" + ph.first + "." + f + "}: no such field (" + what + ")");
    }
    // length: placeholders count as about a name each (12)
    bool in = false;
    for (char c : txt) { if (c == '{') { in = true; len += 12; } else if (c == '}') in = false; else if (!in) len++; }
    if (len > maxLen) bad(line, std::string("the ") + what + " is too long for the phone's dialogue panel (" + std::to_string(len) + " > " + std::to_string(maxLen) + ")");
    for (char c : txt) if (c >= 'a' && c <= 'z') { bad(line, std::string("lower case in the ") + what + " (the HUD font is upper case)"); break; }
  };
  checkText(s.line, s.pitch, 40, "pitch");
  if (!s.hint.empty()) checkText(s.line, s.hint, 300, "hint");
  // the hook speaks before the cast exists: no placeholders in a pitch or a hint
  if (!placeholders(s.pitch).empty() || !placeholders(s.hint).empty()) bad(s.line, "a placeholder in the pitch or hint (they are spoken before casting)");
  auto checkCond = [&](int line, const CondDef& c) {
    switch (c.type) {
      case CondType::Rep: needKind(line, c.a, RoleKind::Kingdom, "rep"); break;
      case CondType::Var: case CondType::NoVar: case CondType::Is:
        if (!s.vars.count(c.a)) bad(line, "undeclared variable '" + c.a + "'");
        // (P1b) `var x 0` is always true and `novar x 0` never: the judge found three gates that meant equality
        if (c.type == CondType::Var && c.n <= 0) bad(line, "'var " + c.a + " " + std::to_string(c.n) + "' is always true (use is / novar)");
        if (c.type == CondType::NoVar && c.n <= 0) bad(line, "'novar " + c.a + " " + std::to_string(c.n) + "' is never true");
        break;
      case CondType::Female: needRole(line, c.a, "female"); break;
      case CondType::War: needKind(line, c.a, RoleKind::Kingdom, "war"); needKind(line, c.b, RoleKind::Kingdom, "war"); break;
      default: break;
    }
  };
  std::vector<std::vector<int>> next(s.stages.size());
  bool anyEnd = false;
  for (size_t i = 0; i < s.stages.size(); i++) {
    const StageDef& G = s.stages[i];
    auto link = [&](const std::string& n, int line) {
      const int j = s.stage(n);
      if (j < 0) bad(line, "unknown stage '" + n + "'");
      else next[i].push_back(j);
    };
    switch (G.kind) {
      case StageKind::None: bad(G.line, "stage '" + G.name + "' has no talk, goal or end"); break;
      case StageKind::Talk: {
        if (!needRole(G.line, G.talk, "talk")) break;
        const int k = kindOf(G.talk);
        if (k != (int)RoleKind::Person && k != (int)RoleKind::Npc && k != (int)RoleKind::Ruler && k != (int)RoleKind::Giver &&
            k != (int)RoleKind::Resident)
          bad(G.line, "stage '" + G.name + "': '" + G.talk + "' cannot be talked to");
        if (k == (int)RoleKind::Giver && !npcHook) bad(G.line, "stage '" + G.name + "': the giver of a " + std::string(s.hook == HookKind::Board ? "board" : s.hook == HookKind::Ruin ? "ruin" : "event") + " story is a place, not a person");
        if (G.say.empty()) bad(G.line, "stage '" + G.name + "': a dialogue with nothing said");
        if (G.opts.empty()) bad(G.line, "stage '" + G.name + "': a dialogue without options");
        if (!G.then.empty()) bad(G.line, "stage '" + G.name + "': 'then' in a dialogue (options choose the next stage)");
        for (const OptDef& o : G.opts) {
          link(o.to, o.line);
          if (o.check) link(o.orElse, o.line);
          for (const CondDef& c : o.ifs) checkCond(o.line, c);
          if (o.check) checkCond(o.line, o.chk);
          checkText(o.line, o.label, 40, "option");
        }
        break;
      }
      case StageKind::Goal: {
        if (G.then.empty()) bad(G.line, "stage '" + G.name + "': a goal without 'then'");
        else link(G.then, G.line);
        // (P1b) escort and bout can be lost: they say where to (else); nothing else has an else
        const bool losable = G.goal.type == ObjType::Escort || G.goal.type == ObjType::Bout;
        if (G.goal.type == ObjType::Time && !G.journal.empty() && G.journal.size() < 4) bad(G.line, "a time goal says what the player waits for");
        if (losable && G.orElse.empty()) bad(G.line, "stage '" + G.name + "': an escort or bout needs 'else <stage>' (when it is lost)");
        if (!losable && !G.orElse.empty()) bad(G.line, "stage '" + G.name + "': 'else' belongs to an escort or a bout");
        if (!G.orElse.empty()) link(G.orElse, G.line);
        if (!G.opts.empty()) bad(G.line, "stage '" + G.name + "': options in a goal stage");
        const ObjDef& o = G.goal;
        switch (o.type) {
          case ObjType::Goto: case ObjType::Enter: case ObjType::Famine:
            if (needRole(G.line, o.role, "goal") && !siteLike(o.role)) bad(G.line, "goal: '" + o.role + "' is not a place");
            break;
          case ObjType::Kill: case ObjType::Use:
            if (!o.role.empty() && needRole(G.line, o.role, "goal") && !siteLike(o.role)) bad(G.line, "goal: '" + o.role + "' is not a place");
            if (o.type == ObjType::Kill && (o.n < 1 || o.n > 12)) bad(G.line, "kill 1..12");
            break;
          case ObjType::Slay:
            if (needRole(G.line, o.role, "slay")) {
              const int k = kindOf(o.role);
              // (P1b) a person the story turned hostile (do hostile) may be slain too
              if (k != (int)RoleKind::Foe && k != (int)RoleKind::Beast && k != (int)RoleKind::Boss && k != (int)RoleKind::Person)
                bad(G.line, "role '" + o.role + "' is the wrong kind for slay");
            }
            break;
          case ObjType::Fetch: if (needRole(G.line, o.role, "fetch") && !siteLike(o.role)) bad(G.line, "fetch: '" + o.role + "' is not a place"); break;
          case ObjType::Wait: if (o.n < 1 || o.n > 30) bad(G.line, "wait 1..30 days"); break;
          case ObjType::Escort: {   // (P1b)
            if (needRole(G.line, o.role, "escort")) {
              const int k = kindOf(o.role);
              if (k != (int)RoleKind::Person && k != (int)RoleKind::Resident) bad(G.line, "escort: '" + o.role + "' is not a person");
            }
            if (needRole(G.line, o.role2, "escort") && !siteLike(o.role2)) bad(G.line, "escort: '" + o.role2 + "' is not a place");
            break;
          }
          case ObjType::Bout:
            if (needRole(G.line, o.role, "bout")) {
              const int k = kindOf(o.role);
              if (k != (int)RoleKind::Person && k != (int)RoleKind::Foe && k != (int)RoleKind::Resident) bad(G.line, "bout: '" + o.role + "' is not a person or foe");
            }
            break;
          case ObjType::War: needKind(G.line, o.role, RoleKind::Kingdom, "war"); needKind(G.line, o.role2, RoleKind::Kingdom, "war"); break;
          case ObjType::Rep: needKind(G.line, o.role, RoleKind::Kingdom, "rep"); break;
          default: break;
        }
        break;
      }
      case StageKind::End:
        anyEnd = true;
        if (!G.then.empty() || !G.opts.empty()) bad(G.line, "stage '" + G.name + "': an ending goes nowhere");
        break;
    }
    // (P1b) the clock
    if (G.deadline > 0) {
      if (G.kind != StageKind::Talk && G.kind != StageKind::Goal) bad(G.line, "stage '" + G.name + "': a deadline on an ending");
      link(G.deadlineTo, G.line);
    }
    if (G.kind == StageKind::Talk && !G.orElse.empty()) bad(G.line, "stage '" + G.name + "': 'else' belongs to an escort or a bout");
    if (!G.say.empty()) checkText(G.line, G.say, 300, "line");
    if (!G.journal.empty()) checkText(G.line, G.journal, 150, "journal");
    for (const EffDef& e : G.effs) {
      switch (e.type) {
        case EffType::Rep: needKind(G.line, e.args[0], RoleKind::Kingdom, "rep"); break;
        case EffType::Set: case EffType::Add: if (!s.vars.count(e.args[0])) bad(G.line, "undeclared variable '" + e.args[0] + "'"); break;
        case EffType::Remember: {
          if (!needRole(G.line, e.args[0], "remember")) break;
          const int k = kindOf(e.args[0]);
          if (k != (int)RoleKind::Giver && k != (int)RoleKind::Person && k != (int)RoleKind::Npc && k != (int)RoleKind::Resident) bad(G.line, "remember: '" + e.args[0] + "' cannot remember");
          checkText(G.line, e.text, 300, "memory");
          break;
        }
        case EffType::Fact: checkText(G.line, e.text, 160, "fact"); break;
        case EffType::Notice: checkText(G.line, e.text, 60, "notice"); break;
        case EffType::Give: case EffType::Take: checkText(G.line, e.text, 32, "item"); break;
        case EffType::Moves:
          if (needRole(G.line, e.args[0], "moves") && kindOf(e.args[0]) != (int)RoleKind::Person && kindOf(e.args[0]) != (int)RoleKind::Resident)
            bad(G.line, "role '" + e.args[0] + "' is the wrong kind for moves");
          if (needRole(G.line, e.args[1], "moves") && !siteLike(e.args[1])) bad(G.line, "moves: not a place"); break;
        case EffType::Shop: needKind(G.line, e.args[0], RoleKind::Npc, "shop"); checkText(G.line, e.text, 300, "memory"); break;
        case EffType::Hide: case EffType::Show: if (needRole(G.line, e.args[0], "hide/show")) { const int k = kindOf(e.args[0]); if (k != (int)RoleKind::Person && k != (int)RoleKind::Foe) bad(G.line, "hide/show: not a person"); } break;
        case EffType::RealmWar: case EffType::RealmPeace: needKind(G.line, e.args[0], RoleKind::Kingdom, "realm"); needKind(G.line, e.args[1], RoleKind::Kingdom, "realm"); break;
        case EffType::RealmFamine: if (needRole(G.line, e.args[0], "realm famine") && !siteLike(e.args[0])) bad(G.line, "realm famine: not a place"); break;
        case EffType::RealmSuccession: needKind(G.line, e.args[0], RoleKind::Kingdom, "succession"); needKind(G.line, e.args[1], RoleKind::Person, "succession"); break;
        case EffType::RealmEvent: for (const std::string& a : e.args) needRole(G.line, a, "realm event"); break;
        case EffType::Secret: if (needRole(G.line, e.args[0], "secret") && !siteLike(e.args[0])) bad(G.line, "secret: '" + e.args[0] + "' is not a place"); break;
        case EffType::Befriend: needKind(G.line, e.args[0], RoleKind::Resident, "befriend"); break;
        case EffType::Recruit:
          if (needRole(G.line, e.args[0], "recruit") && kindOf(e.args[0]) != (int)RoleKind::Person && kindOf(e.args[0]) != (int)RoleKind::Resident)
            bad(G.line, "recruit: '" + e.args[0] + "' is not a person or a resident");
          break;
        case EffType::Approve: case EffType::Bond:
          if (!companion) bad(G.line, "approve / bond belong to a companion's own story (hook companion)");
          if (e.n < -30 || e.n > 30) bad(G.line, "approve / bond -30..30");
          break;
        case EffType::Fate: if (!companion) bad(G.line, "fate belongs to a companion's own story (hook companion)"); break;
        case EffType::Days: if (!s.vars.count(e.args[0])) bad(G.line, "undeclared variable '" + e.args[0] + "'"); break;   // (P1b)
        case EffType::Census: if (needRole(G.line, e.args[0], "census") && !siteLike(e.args[0])) bad(G.line, "census: '" + e.args[0] + "' is not a place"); break;
        case EffType::Follow: case EffType::Stay:
          if (needRole(G.line, e.args[0], "follow/stay") && kindOf(e.args[0]) != (int)RoleKind::Person && kindOf(e.args[0]) != (int)RoleKind::Resident)
            bad(G.line, "follow/stay: '" + e.args[0] + "' is not a person");
          break;
        case EffType::Hostile:
          if (needRole(G.line, e.args[0], "hostile") && kindOf(e.args[0]) != (int)RoleKind::Person && kindOf(e.args[0]) != (int)RoleKind::Foe)
            bad(G.line, "hostile: '" + e.args[0] + "' is not a person or foe");
          break;
        case EffType::Calm: if (needRole(G.line, e.args[0], "calm") && !siteLike(e.args[0])) bad(G.line, "calm: '" + e.args[0] + "' is not a place"); break;
        default: break;
      }
    }
  }
  if (!anyEnd) bad(s.line, "an ending is missing");
  // the first stage: npc hooks open with a dialogue with the giver
  if (npcHook && (s.stages[0].kind != StageKind::Talk || s.stages[0].talk != s.roles.front().name))
    bad(s.stages[0].line, "an npc story opens with a dialogue with its giver");
  // reachability from the first stage, and every reachable stage can reach an ending
  std::vector<uint8_t> reach(s.stages.size(), 0);
  std::vector<int> stack = {0};
  reach[0] = 1;
  while (!stack.empty()) {
    const int c = stack.back(); stack.pop_back();
    for (int n : next[(size_t)c]) if (!reach[(size_t)n]) { reach[(size_t)n] = 1; stack.push_back(n); }
  }
  for (size_t i = 0; i < s.stages.size(); i++) if (!reach[i]) bad(s.stages[i].line, "unreachable stage '" + s.stages[i].name + "'");
  std::vector<uint8_t> toEnd(s.stages.size(), 0);
  for (size_t i = 0; i < s.stages.size(); i++) if (s.stages[i].kind == StageKind::End) toEnd[i] = 1;
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t i = 0; i < s.stages.size(); i++)
      if (!toEnd[i]) for (int n : next[i]) if (toEnd[(size_t)n]) { toEnd[i] = 1; grew = true; break; }
  }
  for (size_t i = 0; i < s.stages.size(); i++)
    if (reach[i] && !toEnd[i]) bad(s.stages[i].line, "dead stage '" + s.stages[i].name + "': no ending can be reached from it");
  // tier 2 stories must offer at least one choice and end in a persistent consequence
  int choices = 0;
  bool consequence = false;
  for (const StageDef& G : s.stages) {
    if (G.kind == StageKind::Talk && G.opts.size() >= 2) choices++;
    for (const EffDef& e : G.effs)
      if (e.type == EffType::Remember || e.type == EffType::Fact || e.type == EffType::Moves || e.type == EffType::Shop ||
          e.type == EffType::Mark || e.type == EffType::Rep || (e.type >= EffType::RealmWar && e.type <= EffType::RealmEvent) ||
          e.type == EffType::Secret || e.type == EffType::Befriend || e.type == EffType::Recruit || e.type == EffType::Approve ||
          e.type == EffType::Bond || e.type == EffType::Fate)
        consequence = true;
  }
  if (choices == 0) bad(s.line, "no choice anywhere (a story needs at least one)");
  if (!consequence) bad(s.line, "no persistent consequence (remember, fact, moves, shop, mark, rep, secret, befriend or a realm effect)");
  return P;
}

// ---------------------------------------------------------------- the library
void link(Script& s) {
  for (StageDef& G : s.stages) {
    G.thenIx = G.then.empty() ? -1 : s.stage(G.then);
    G.elseIx = G.orElse.empty() ? -1 : s.stage(G.orElse);          // (P1b)
    G.deadlineIx = G.deadlineTo.empty() ? -1 : s.stage(G.deadlineTo);
    for (OptDef& o : G.opts) { o.toIx = s.stage(o.to); o.elseIx = o.orElse.empty() ? -1 : s.stage(o.orElse); }
  }
}

const Library& library() {
  static Library L = [] {
    Library lib;
    parse(talesSource(), "tales", lib.scripts, lib.errors);
    parse(campaignSource(), "campaign", lib.scripts, lib.errors);
    // (P1b) the new items (REQUESTS and TALES lanes) and the lead's mechanics test scripts
    parse(requestsVillageSource(), "req_village", lib.scripts, lib.errors);
    parse(requestsTownSource(), "req_town", lib.scripts, lib.errors);
    parse(requestsCitySource(), "req_city", lib.scripts, lib.errors);
    parse(tales2Source(), "tales2", lib.scripts, lib.errors);
    parse(p1bTestSource(), "p1b_test", lib.scripts, lib.errors);
    std::set<std::string> ids;
    for (Script& s : lib.scripts) {
      if (!ids.insert(s.id).second) lib.errors.push_back(s.source + ":" + std::to_string(s.line) + " " + s.id + ": a second script with this id");
      link(s);
      for (const std::string& p : validate(s)) lib.errors.push_back(p);
    }
    return lib;
  }();
  return L;
}

}  // namespace dsl
}  // namespace story
