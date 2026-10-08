// M5 "Hearth and Hall" script commands (rpg/view/script_api.h; `embervale --script-help` lists them). CITIZENS lane.
// PHASE A (lead):
//   life                     print the nearest settlement's census to stdout: residents, mood, flags, what they do
//                            this hour (counts per activity), the player's buffs
//   life stats               print Life::stats (censuses, residents, tick ms: last / worst / average)
//   buff <wellfed|rested|hungry|weary|none>   put the player in that state (tests: the HUD icon, the effects)
//   expect life residents <N>        the nearest settlement's census has at least N residents
//   expect life act <activity> <N>   at least N of its residents are doing it this hour (sleep, work, tavern, eat...)
//   expect life mood <lo> [hi]       its mood is within [lo, hi]
//   expect buff <wellfed|rested|hungry|weary> [0|1]   the player has (1, default) / has not (0) the buff
// PHASE B (CITIZENS):
//   life famine [days]       a famine felt in the nearest settlement's streets (its food gone, no imports, a poor yield)
//   life festival            its festival today
//   life kill <name|index>   one of its residents dies (kin and friends grieve)
//   life stock <good> <n>    set its stores of a good (prices follow)
//   life hours <n> | life days <n>   run the clock n in-game hours / days (an hour a step: the aggregate runs each one)
//   life mark                remember its mood now (expect life moodfall)
//   raid now                 a night raid on it now (live when the player is within 120 tiles and outdoors)
//   waitraid                 wait (up to 150 s) until the raid going on is decided
//   expect life flag <content|festival|hungry|famine|wartorn|grief|brawls|emigrating|shuttered|raided> [0|1]
//   expect life price <good> <lo> [hi]   expect life stock <good> <lo> [hi]   expect life grieving <N>
//   expect life moodfall <N>  its mood fell by N or more since `life mark`
//   expect raid <none|going|repelled|failed|over>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace {

std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

// the loaded settlement nearest the player (-1 none)
int nearestSettlement(Game& g) {
  int best = -1;
  float bd = 1e30f;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (int si : g.world.nearSites) {
    if (si < 0 || si >= (int)g.world.sites.size() || !g.world.sites[(size_t)si].settlement()) continue;
    const Site& s = g.world.sites[(size_t)si];
    const float dx = s.ex - px, dy = s.ey - py, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = si; }
  }
  if (g.inside && g.subBldg >= 0) best = g.world.over.bldgs[(size_t)g.subBldg].site;
  return best;
}
life::Census* nearestCensus(Game& g) {
  const int si = nearestSettlement(g);
  return si >= 0 ? g.life.census(g.world, si) : nullptr;
}
uint8_t buffBit(const std::string& w) {
  if (w == "wellfed") return life::BUFF_WELLFED;
  if (w == "rested") return life::BUFF_RESTED;
  if (w == "hungry") return life::BUFF_HUNGRY;
  if (w == "weary") return life::BUFF_WEARY;
  return 0;
}

std::string upcase(std::string s) { for (char& ch : s) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32); return s; }
int goodOf(const std::string& w) {
  const std::string u = upcase(w);
  for (int k = 0; k < (int)ew::Good::COUNT; k++) if (u == ew::goodName((ew::Good)k)) return k;
  if (u == "INGOT") return (int)ew::Good::Ingot;
  return -1;
}
uint16_t flagOf(const std::string& w) {
  const std::string u = upcase(w);
  for (int b = 0; b < 16; b++) if (u == life::moodFlagName((uint16_t)(1u << b))) return (uint16_t)(1u << b);
  return 0;
}
std::string flagNames(uint16_t f) {
  std::string s;
  for (int b = 0; b < 16; b++) if (f & (1u << b)) { if (!s.empty()) s += ' '; s += life::moodFlagName((uint16_t)(1u << b)); }
  return s.empty() ? std::string("-") : s;
}
int g_markedMood = -1;

bool cmdLife(ScriptCtx& c) {
  Game& g = c.game;
  const std::string sub = lower(c.arg(1));
  if (sub == "stats") {
    const life::Life::Stats& s = g.life.stats;
    printf("life stats: censuses %d residents %d hourTicks %d tick ms last %.3f worst %.3f avg %.4f census worst %.2f slice worst %.3f "
           "raids live %d abstract %d failed %d festivals %d emigrated %d trades %d\n",
           s.censuses, s.residents, s.hourTicks, s.lastTickMs, s.worstTickMs, s.ticks ? s.tickMsSum / s.ticks : 0.0, s.worstCensusMs,
           s.worstSliceMs, s.raidsLive, s.raidsAbstract, s.raidsFailed, s.festivals, s.emigrated, s.trades);
    return true;
  }
  if (sub == "hours" || sub == "days") {
    const int n = std::max(0, std::atoi(c.arg(2).c_str())) * (sub == "days" ? 24 : 1);
    g.life.advanceHours(g, std::min(n, 24 * 30));
    return true;
  }
  life::Census* cs = nearestCensus(g);
  if (!cs) { if (sub.empty()) printf("life: no settlement census here\n"); else c.fail("life " + sub + ": no settlement census here"); return true; }
  if (sub == "famine") { g.life.forceFamine(cs->site, g.day, c.arg(2).empty() ? 3 : std::atoi(c.arg(2).c_str())); return true; }
  if (sub == "festival") { g.life.forceFestival(cs->site, g.day); return true; }
  if (sub == "mark") { g_markedMood = cs->mood; return true; }
  if (sub == "stock") {
    const int gd = goodOf(c.arg(2));
    if (gd < 0) { c.fail("life stock <good> <n>"); return true; }
    cs->stock[(size_t)gd] = (uint16_t)std::clamp(std::atoi(c.arg(3).c_str()), 0, 60000);
    g.life.supply(cs->site, (ew::Good)gd, 0);
    return true;
  }
  if (sub == "kill") {
    const std::string who = upcase(c.rest(2));
    int idx = -1;
    if (!who.empty() && who[0] >= '0' && who[0] <= '9') idx = std::atoi(who.c_str());
    else for (const life::Resident& r : cs->res) if (r.name == who && !(r.flags & life::RF_DEAD)) { idx = r.idx; break; }
    if (idx < 0 || idx >= (int)cs->res.size()) { c.fail("life kill: nobody called " + who); return true; }
    g.life.residentDied(cs->site, idx, g.day);
    printf("life: %s died\n", cs->res[(size_t)idx].name.c_str());
    return true;
  }
  const Site& st = g.world.sites[(size_t)cs->handle];
  int acts[(int)life::Act::COUNT] = {};
  for (const life::Resident& r : cs->res) if (!(r.flags & (life::RF_DEAD | life::RF_AWAY))) acts[(int)r.act]++;
  printf("life: %s residents %zu ties %zu mood %d flags 0x%x (%s) hour %.1f day %d buffs 0x%x festival %d (%s)\n", st.name.c_str(), cs->res.size(),
         cs->ties.size(), cs->mood, cs->moodFlags, flagNames(cs->moodFlags).c_str(), g.hour, g.day, g.life.player.buffs(), cs->festivalDay,
         g.life.festivalTitle(g.world, *cs).c_str());
  std::string stock = "  stores:";
  for (int k = 0; k < (int)ew::Good::COUNT; k++)
    if (cs->stock[(size_t)k])
      stock += std::string(" ") + ew::goodName((ew::Good)k) + " " + std::to_string(cs->stock[(size_t)k]) + "@" + std::to_string(cs->price[(size_t)k]) + "%";
  printf("%s\n", stock.c_str());
  std::string nd = "  needs:";
  for (int k = 0; k < life::NEEDS; k++) nd += std::string(" ") + life::needName((life::Need)k) + " " + std::to_string(life::Life::needAverage(*cs, (life::Need)k));
  printf("%s\n", nd.c_str());
  std::string line = "  doing:";
  for (int a = 0; a < (int)life::Act::COUNT; a++)
    if (acts[a]) line += std::string(" ") + life::actName((life::Act)a) + "=" + std::to_string(acts[a]);
  printf("%s\n", line.c_str());
  return true;
}
EMB_SCRIPT_CMD("life", "life [stats | famine [days] | festival | kill <who> | stock <good> <n> | hours <n> | days <n> | mark]: the nearest settlement's census", cmdLife);

bool cmdBuff(ScriptCtx& c) {
  life::PlayerNeeds& n = c.game.life.player;
  const std::string w = lower(c.arg(1));
  if (w == "none") { n = life::PlayerNeeds(); return true; }
  switch (buffBit(w)) {
    case life::BUFF_WELLFED: n.fedH = 4; n.sinceMealH = 0; n.mealQuality = 2; break;
    case life::BUFF_RESTED: n.restedH = 10; n.sinceSleepH = 0; break;
    case life::BUFF_HUNGRY: n.fedH = 0; n.sinceMealH = 40; break;
    case life::BUFF_WEARY: n.restedH = 0; n.sinceSleepH = 60; break;
    default: c.fail("buff <wellfed|rested|hungry|weary|none>"); break;
  }
  return true;
}
EMB_SCRIPT_CMD("buff", "buff <wellfed|rested|hungry|weary|none>: put the player in that state (15.2)", cmdBuff);

bool expLife(ScriptCtx& c) {
  Game& g = c.game;
  life::Census* cs = nearestCensus(g);
  if (!cs) { c.fail("expect life: no settlement census here"); return true; }
  const std::string what = lower(c.arg(2));
  if (what == "residents") {
    const int n = std::atoi(c.arg(3).c_str());
    if ((int)cs->res.size() < n) c.fail("expect life residents: " + std::to_string(cs->res.size()) + " < " + std::to_string(n));
  } else if (what == "act") {
    const std::string a = upper(c.arg(3));
    const int n = std::atoi(c.arg(4).c_str());
    int have = 0, act = -1;
    for (int k = 0; k < (int)life::Act::COUNT; k++) if (a == life::actName((life::Act)k)) act = k;
    if (act < 0) { c.fail("expect life act <sleep|eat|work|wander|socialise|tavern|pray|patrol|play|perform|shop|home|beg|...> <N>"); return true; }
    for (const life::Resident& r : cs->res) if ((int)r.act == act && !(r.flags & (life::RF_DEAD | life::RF_AWAY))) have++;
    if (have < n) c.fail("expect life act " + c.arg(3) + ": " + std::to_string(have) + " < " + std::to_string(n));
  } else if (what == "mood") {
    const int lo = std::atoi(c.arg(3).c_str()), hi = c.arg(4).empty() ? 100 : std::atoi(c.arg(4).c_str());
    if (cs->mood < lo || cs->mood > hi) c.fail("expect life mood: " + std::to_string(cs->mood) + " not in [" + c.arg(3) + ", " + std::to_string(hi) + "]");
  } else if (what == "flag") {
    const uint16_t f = flagOf(c.arg(3));
    if (!f) { c.fail("expect life flag <content|festival|hungry|famine|wartorn|grief|brawls|emigrating|shuttered|raided> [0|1]"); return true; }
    const bool want = c.arg(4).empty() || c.arg(4) != "0";
    if (((cs->moodFlags & f) != 0) != want) c.fail("expect life flag " + c.arg(3) + ": flags are " + flagNames(cs->moodFlags));
  } else if (what == "price" || what == "stock") {
    const int gd = goodOf(c.arg(3));
    if (gd < 0) { c.fail("expect life " + what + " <good> <lo> [hi]"); return true; }
    const int v = what == "price" ? cs->price[(size_t)gd] : cs->stock[(size_t)gd];
    const int lo = std::atoi(c.arg(4).c_str()), hi = c.arg(5).empty() ? 1 << 30 : std::atoi(c.arg(5).c_str());
    if (v < lo || v > hi) c.fail("expect life " + what + " " + c.arg(3) + ": " + std::to_string(v));
  } else if (what == "grieving") {
    int n = 0;
    for (const life::Resident& r : cs->res) n += (r.flags & life::RF_GRIEVING) && !(r.flags & life::RF_DEAD);
    if (n < std::atoi(c.arg(3).c_str())) c.fail("expect life grieving: " + std::to_string(n));
  } else if (what == "moodfall") {
    if (g_markedMood < 0) { c.fail("expect life moodfall: no `life mark` first"); return true; }
    if (g_markedMood - cs->mood < std::atoi(c.arg(3).c_str()))
      c.fail("expect life moodfall: " + std::to_string(g_markedMood) + " -> " + std::to_string(cs->mood));
  } else c.fail("expect life residents <N> | act <activity> <N> | mood <lo> [hi] | flag <f> [0|1] | price|stock <good> <lo> [hi] | grieving <N> | moodfall <N>");
  return true;
}
EMB_SCRIPT_CMD("expect:life", "expect life residents <N> | act <activity> <N> | mood <lo> [hi]: the nearest settlement's census", expLife);

bool expBuff(ScriptCtx& c) {
  const uint8_t b = buffBit(lower(c.arg(2)));
  if (!b) { c.fail("expect buff <wellfed|rested|hungry|weary> [0|1]"); return true; }
  const bool want = c.arg(3).empty() || c.arg(3) != "0";
  const bool has = (c.game.life.player.buffs() & b) != 0;
  if (has != want) c.fail(std::string("expect buff ") + c.arg(2) + ": " + (has ? "has it" : "has not"));
  return true;
}
EMB_SCRIPT_CMD("expect:buff", "expect buff <wellfed|rested|hungry|weary> [0|1]: the player's 15.2 state", expBuff);

bool cmdRaid(ScriptCtx& c) {
  Game& g = c.game;
  if (lower(c.arg(1)) != "now") { c.fail("raid now"); return true; }
  life::Census* cs = nearestCensus(g);
  if (!cs) { c.fail("raid now: no settlement census here"); return true; }
  g.life.forceRaidSite = cs->site;
  g.life.lastRaidOutcome = 0;
  return true;
}
EMB_SCRIPT_CMD("raid", "raid now: a night raid on the nearest settlement now (VISION_PLAN 10.4 M5)", cmdRaid);

bool cmdWaitRaid(ScriptCtx& c) {
  Game& g = c.game;
  if (g.life.forceRaidSite) return false;   // not begun yet
  return g.life.raid.site == 0;             // decided (or none)
}
const int kWaitRaidReg = registerScriptExt(ScriptExt{"waitraid", "waitraid: wait until the raid going on is decided (150 s at most)", cmdWaitRaid, 150});

bool expRaid(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = lower(c.arg(2));
  const int o = g.life.lastRaidOutcome;
  const bool going = g.life.raid.site != 0;
  bool ok = false;
  if (w == "none") ok = !going && o == 0;
  else if (w == "going") ok = going;
  else if (w == "repelled") ok = !going && o == 1;
  else if (w == "failed") ok = !going && o == 2;
  else if (w == "over") ok = !going && o != 0;
  else { c.fail("expect raid <none|going|repelled|failed|over>"); return true; }
  if (!ok) c.fail("expect raid " + w + ": " + (going ? std::string("going on") : "outcome " + std::to_string(o)));
  return true;
}
EMB_SCRIPT_CMD("expect:raid", "expect raid <none|going|repelled|failed|over>: the night raid near the player", expRaid);

}  // namespace
