// M5 "Hearth and Hall" VIEW / AMBIENCE lane script commands (rpg/view/script_api.h; `embervale --script-help` lists
// them). Review tools: they put the townsfolk and the town in the states the view draws, so the shots show them before
// (and independently of) the sim deciding them. None of them touches a save.
//   posture <name> [n] [here | row R]   the n (default 1) nearest townsfolk take that posture (sit, siteat, sitdrink, eat, drink,
//                         cheer, hammer, hoe, sweep, chop, stir, carry, fish, sleep, wave, play, dance, lute, drum, flute,
//                         pray, beg, lamp, read, none). `here`: they stand in a row in front of the player first (so
//                         the shot shows them all together); `row R`: the n nearest not yet posed, in row R (32 px
//                         apart, below the player). The sim may change it back at its next decision.
//   sitat <x> <y> [face]  the nearest townsperson sits on the furniture at tile (x, y) of the current map (Actor::useX /
//                         useY; face down|up|left|right, default down): the seat's draw order (art PostureInfo::seated)
//   sleepat <x> <y>       the nearest townsperson lies in the bed at tile (x, y) (PostureInfo::lying)
//   seatall [posture]     inside: every chair / bench / stool of the room gets the nearest free townsperson seated on it
//                         (Sit by default; SitEat, SitDrink...), facing the nearest table; beds get sleepers with `sleep`
//   critter <kind> [n]    test only: n (default 3) animals of a kind (dog, cat, chicken, rooster, goat, pig, duck) appear
//                         round the player (Game::actors; critter = kind + 1), each with its own coat
//   bubble <kind> [n] [secs]   the n nearest townsfolk show that speech bubble (talk, exclaim, note, mug, heart, bread,
//                         zzz, coin, anger, tear, question) for secs (default 4)
//   say <words...>        the nearest townsperson says the words (Ev::Text over them: the spoken-line style)
//   moodflags <flag...>   the nearest settlement's census takes those mood flags (content, festival, hungry, famine,
//                         wartorn, grief, brawls, emigrating, shuttered, raided; `none`); the view also keeps them
//                         (scriptLifeLook) so the next hourly aggregate does not take them back during the shot
//   festival [on|off|auto]   today is (not) the nearest settlement's festival (Census::festivalDay and the view's force)
//   tolamp [n]            stand two tiles south of the n-th nearest street lamp (prints the hours it is lit and put out)
//   m5stats               print the view's last-frame M5 counts (View::m5Stats)
//   expect m5 <what> <min> [max]   the last frame drew at least min (at most max) of: posed, fallback, critters,
//                         bubbles, lampslit, lampsdark, shut, bunting, lanterns, winlit, windark, speech; or
//                         `expect m5 music tavern|town|night|wild` (the piece playing), `expect m5 tavern <min>` (its level
//                         x100), `expect m5 crowd <min>` (the crowd murmur x100)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

namespace {

std::string lower(std::string s) { for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32); return s; }

const char* kPostureNames[(int)art::Posture::COUNT] = {"none", "sit", "siteat", "sitdrink", "eat", "drink", "cheer", "hammer", "hoe",
                                                        "sweep", "chop", "stir", "carry", "fish", "sleep", "wave", "play", "dance",
                                                        "lute", "drum", "flute", "pray", "beg", "lamp", "read", "sitfloor",
                                                        "sitflooreat", "sitfloordrink"};
const char* kCritterNames[(int)art::Critter::COUNT] = {"dog", "cat", "chicken", "rooster", "goat", "pig", "duck"};
const char* kBubbleNames[(int)art::Bubble::COUNT] = {"none", "talk", "exclaim", "note", "mug", "heart", "bread", "zzz", "coin", "anger", "tear", "question"};

int lookup(const char* const* names, int n, const std::string& w) {
  for (int i = 0; i < n; i++) if (w == names[i]) return i;
  return -1;
}

// the townsfolk (people, not the player, not hostile, alive) nearest the player, nearest first (fresh: only those not
// already in a posture)
std::vector<int> nearestFolk(Game& g, int n, bool fresh = false) {
  std::vector<std::pair<float, int>> v;
  const Vec2 pp = g.pl().p;
  for (int i = 0; i < (int)g.actors.size(); i++) {
    const Actor& a = g.actors[(size_t)i];
    if (a.player || !a.npc || !a.human || a.hostile || a.st == AState::Dead) continue;
    if (fresh && a.posture != art::Posture::None) continue;
    if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Captain) continue;   // the watch keeps its post
    v.push_back({len2(a.p - pp), i});
  }
  std::sort(v.begin(), v.end());
  std::vector<int> out;
  for (int k = 0; k < (int)v.size() && k < n; k++) out.push_back(v[(size_t)k].second);
  return out;
}

// the loaded settlement nearest the player (inside a building: its own)
int nearestSettlement(Game& g) {
  if (g.inside && g.subBldg >= 0 && g.subBldg < (int)g.world.over.bldgs.size()) return g.world.over.bldgs[(size_t)g.subBldg].site;
  int best = -1;
  float bd = 1e30f;
  const float px = g.pl().p.x / TILE, py = g.pl().p.y / TILE;
  for (int si = 0; si < (int)g.world.sites.size(); si++) {
    const Site& s = g.world.sites[(size_t)si];
    if (!s.settlement()) continue;
    const float dx = s.ex - px, dy = s.ey - py, d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = si; }
  }
  return best;
}

bool cmdPosture(ScriptCtx& c) {
  const int p = lookup(kPostureNames, (int)art::Posture::COUNT, lower(c.arg(1)));
  if (p < 0) { c.fail("posture: unknown posture " + c.arg(1)); return true; }
  int n = 1, row = -1;
  bool here = false;
  for (size_t k = 2; k < c.a.size(); k++) {
    if (lower(c.a[k]) == "here") here = true;
    else if (lower(c.a[k]) == "row" && k + 1 < c.a.size()) { row = std::atoi(c.a[k + 1].c_str()); here = true; k++; }
    else n = std::max(1, std::atoi(c.a[k].c_str()));
  }
  const std::vector<int> folk = nearestFolk(c.game, n, row >= 0);
  const Vec2 pp = c.game.pl().p;
  int k = 0;
  for (int i : folk) {
    Actor& a = c.game.actors[(size_t)i];
    a.posture = (art::Posture)p;
    a.postureT = 0.01f + 0.37f * (float)k;
    a.useX = a.useY = -1;
    a.st = AState::Idle;
    a.vel = Vec2();
    if (here) {   // a row of them in front of the player, a little apart, facing the camera
      a.p = pp + Vec2(-24.0f * (float)((int)folk.size() - 1) / 2.0f + 24.0f * (float)k, 34.0f + 32.0f * (float)std::max(0, row));
      a.goal = a.home = a.p;
      a.face = 0;
    }
    k++;
  }
  std::printf("script: posture %s on %d townsfolk\n", kPostureNames[p], (int)folk.size());
  return true;
}
EMB_SCRIPT_CMD("posture", "posture <name> [n] [here]: the n nearest townsfolk take that posture (M5 VIEW)", cmdPosture);

int faceOf(const std::string& w) { return w == "up" ? 1 : w == "right" ? 2 : w == "left" ? 3 : 0; }

bool cmdSitAt(ScriptCtx& c) {
  const int x = std::atoi(c.arg(1).c_str()), y = std::atoi(c.arg(2).c_str());
  const std::vector<int> folk = nearestFolk(c.game, 1);
  if (folk.empty()) { c.fail("sitat: nobody near"); return true; }
  Actor& a = c.game.actors[(size_t)folk[0]];
  a.posture = art::Posture::Sit;
  a.useX = x; a.useY = y;
  a.face = faceOf(lower(c.arg(3)));
  a.p = Vec2(x * 16.0f + 8.0f, y * 16.0f + 12.0f);
  a.goal = a.home = a.p;
  a.vel = Vec2();
  a.st = AState::Idle;
  return true;
}
EMB_SCRIPT_CMD("sitat", "sitat <x> <y> [down|up|left|right]: the nearest townsperson sits on that tile's seat (M5 VIEW)", cmdSitAt);

bool cmdSleepAt(ScriptCtx& c) {
  const int x = std::atoi(c.arg(1).c_str()), y = std::atoi(c.arg(2).c_str());
  const std::vector<int> folk = nearestFolk(c.game, 1);
  if (folk.empty()) { c.fail("sleepat: nobody near"); return true; }
  Actor& a = c.game.actors[(size_t)folk[0]];
  a.posture = art::Posture::Sleep;
  a.useX = x; a.useY = y;
  a.face = 0;
  a.p = Vec2(x * 16.0f + 8.0f, y * 16.0f + 12.0f);
  a.goal = a.home = a.p;
  a.vel = Vec2();
  a.st = AState::Idle;
  return true;
}
EMB_SCRIPT_CMD("sleepat", "sleepat <x> <y>: the nearest townsperson lies in that tile's bed (M5 VIEW)", cmdSleepAt);

// seats and beds of the room the player is in
bool isSeat(art::Prop p) { return p == art::Prop::Chair || p == art::Prop::Bench || p == art::Prop::Stool || p == art::Prop::Cushion; }
bool isTable(art::Prop p) { return p == art::Prop::Table || p == art::Prop::LowTable || p == art::Prop::Desk; }
bool isBed(art::Prop p) { return p == art::Prop::Bed || p == art::Prop::BunkBed || p == art::Prop::SleepingMat; }

bool cmdSeatAll(ScriptCtx& c) {
  Game& g = c.game;
  if (!g.inside) { c.fail("seatall: not inside"); return true; }
  const Map& m = g.map();
  int p = (int)art::Posture::Sit;
  if (!c.arg(1).empty()) {
    p = lookup(kPostureNames, (int)art::Posture::COUNT, lower(c.arg(1)));
    if (p < 0) { c.fail("seatall: unknown posture " + c.arg(1)); return true; }
  }
  const bool beds = (art::Posture)p == art::Posture::Sleep;
  std::vector<std::pair<int, int>> spots;
  for (int y = 0; y < m.h; y++)
    for (int x = 0; x < m.w; x++) {
      const int pr = m.propAt(x, y);
      if (!pr) continue;
      const art::Prop q = (art::Prop)(pr - 1);
      if (beds ? isBed(q) : isSeat(q)) spots.push_back({x, y});
    }
  std::vector<int> folk = nearestFolk(g, (int)spots.size());
  int seated = 0;
  for (size_t k = 0; k < spots.size() && k < folk.size(); k++) {
    Actor& a = g.actors[(size_t)folk[k]];
    const int x = spots[k].first, y = spots[k].second;
    a.posture = (art::Posture)p;
    a.postureT = 0.01f + 0.53f * (float)k;
    a.useX = x; a.useY = y;
    // facing the table beside it (below: down, above: up, else sideways toward it)
    int face = 0;
    auto tableAt = [&](int tx, int ty) { const int q = m.propAt(tx, ty); return q && isTable((art::Prop)(q - 1)); };
    if (!beds) {
      if (tableAt(x, y + 1)) face = 0;
      else if (tableAt(x, y - 1)) face = 1;
      else if (tableAt(x + 1, y)) face = 2;
      else if (tableAt(x - 1, y)) face = 3;
    }
    a.face = face;
    a.p = Vec2(x * 16.0f + 8.0f, y * 16.0f + 12.0f);
    a.goal = a.home = a.p;
    a.vel = Vec2();
    a.st = AState::Idle;
    seated++;
  }
  std::printf("script: seatall %s: %d of %d %s taken\n", kPostureNames[p], seated, (int)spots.size(), beds ? "beds" : "seats");
  return true;
}
EMB_SCRIPT_CMD("seatall", "seatall [posture]: every seat (or with sleep: every bed) of the room gets the nearest townsperson (M5 VIEW)", cmdSeatAll);

bool cmdCritter(ScriptCtx& c) {
  const int k = lookup(kCritterNames, (int)art::Critter::COUNT, lower(c.arg(1)));
  if (k < 0) { c.fail("critter: unknown kind " + c.arg(1)); return true; }
  const int n = c.arg(2).empty() ? 3 : std::max(1, std::atoi(c.arg(2).c_str()));
  Game& g = c.game;
  static int nextId = 0x40000000;   // far above the game's own ids
  const Vec2 pp = g.pl().p;
  for (int i = 0; i < n; i++) {
    Actor a;
    a.id = nextId++;
    a.npc = true;
    a.human = false;
    a.hostile = false;
    a.faction = Faction::Town;
    a.critter = (uint8_t)(k + 1);
    a.critterVar = (uint32_t)(a.id * 2654435761u) >> 8;
    a.name = kCritterNames[k];
    const float an = (float)i * 2.39996f + 0.7f, r = 26.0f + 9.0f * (float)i;
    a.p = pp + Vec2(std::cos(an) * r, std::sin(an) * r * 0.7f + 10.0f);
    a.home = a.goal = a.p;
    a.face = i % 4;
    a.radius = 4;
    a.speed = 18;
    a.hp = a.maxHp = 5;
    a.level = 1;
    a.xp = 0;
    a.aggroR = 0;
    a.range = 0;
    a.dmg = 0;
    g.actors.push_back(a);
  }
  std::printf("script: %d %s(s) round the player\n", n, kCritterNames[k]);
  return true;
}
EMB_SCRIPT_CMD("critter", "critter <dog|cat|chicken|rooster|goat|pig|duck> [n]: test only: animals round the player (M5 VIEW)", cmdCritter);

bool cmdBubble(ScriptCtx& c) {
  const int b = lookup(kBubbleNames, (int)art::Bubble::COUNT, lower(c.arg(1)));
  if (b < 0) { c.fail("bubble: unknown kind " + c.arg(1)); return true; }
  const int n = c.arg(2).empty() ? 1 : std::max(1, std::atoi(c.arg(2).c_str()));
  const float secs = c.arg(3).empty() ? 4.0f : (float)std::atof(c.arg(3).c_str());
  for (int i : nearestFolk(c.game, n)) {
    Actor& a = c.game.actors[(size_t)i];
    a.bubble = (art::Bubble)b;
    a.bubbleT = secs;
  }
  return true;
}
EMB_SCRIPT_CMD("bubble", "bubble <talk|exclaim|note|mug|heart|bread|zzz|coin|anger|tear|question> [n] [secs]: speech bubbles (M5 VIEW)", cmdBubble);

bool cmdSay(ScriptCtx& c) {
  const std::vector<int> folk = nearestFolk(c.game, 1);
  if (folk.empty()) { c.fail("say: nobody near"); return true; }
  const Actor& a = c.game.actors[(size_t)folk[0]];
  std::string words;
  for (size_t k = 1; k < c.a.size(); k++) {
    std::string w = c.a[k];
    for (char& ch : w) if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    words += (k > 1 ? " " : "") + w;
  }
  Event e;
  e.type = Ev::Text; e.p = a.p + Vec2(0, -26); e.a = (int)rgba(240, 228, 196); e.f = 0; e.s = words;
  c.game.events.push_back(e);
  return true;
}
EMB_SCRIPT_CMD("say", "say <words...>: the nearest townsperson says it (the spoken-line style over them) (M5 VIEW)", cmdSay);

bool cmdMoodFlags(ScriptCtx& c) {
  Game& g = c.game;
  const int si = nearestSettlement(g);
  life::Census* cs = si >= 0 ? g.life.census(g.world, si) : nullptr;
  if (!cs) { c.fail("moodflags: no settlement census here"); return true; }
  static const char* names[10] = {"content", "festival", "hungry", "famine", "wartorn", "grief", "brawls", "emigrating", "shuttered", "raided"};
  uint16_t f = 0;
  for (size_t k = 1; k < c.a.size(); k++) {
    const std::string w = lower(c.a[k]);
    if (w == "none") continue;
    const int i = lookup(names, 10, w);
    if (i < 0) { c.fail("moodflags: unknown flag " + w); return true; }
    f |= (uint16_t)(1u << i);
  }
  cs->moodFlags = f;
  if (f & life::MF_FESTIVAL) cs->festivalDay = g.day;
  else if (cs->festivalDay == g.day) cs->festivalDay = -1;
  c.view.scriptLifeLook((f & life::MF_FESTIVAL) ? 1 : 0, (f & (life::MF_SHUTTERED | life::MF_FAMINE)) ? 1 : 0);
  std::printf("script: %s mood flags 0x%x\n", g.world.sites[(size_t)si].name.c_str(), f);
  return true;
}
EMB_SCRIPT_CMD("moodflags", "moodflags <content|festival|hungry|famine|wartorn|grief|brawls|emigrating|shuttered|raided|none...>: the nearest census's mood (M5 VIEW)", cmdMoodFlags);

bool cmdFestival(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = lower(c.arg(1));
  const int si = nearestSettlement(g);
  life::Census* cs = si >= 0 ? g.life.census(g.world, si) : nullptr;
  if (w == "auto") { c.view.scriptLifeLook(-1, -1); return true; }
  const bool on = w != "off";
  if (cs) {
    cs->festivalDay = on ? g.day : -1;
    cs->moodFlags = (uint16_t)(on ? (cs->moodFlags | life::MF_FESTIVAL) : (cs->moodFlags & ~life::MF_FESTIVAL));
  }
  c.view.scriptLifeLook(on ? 1 : 0, -1);
  return true;
}
EMB_SCRIPT_CMD("festival", "festival [on|off|auto]: today is (not) the nearest settlement's festival (M5 VIEW)", cmdFestival);

// stand two tiles south of the n-th nearest street lamp (the lamplighter's round: shots of a lamp lit / dark)
bool cmdToLamp(ScriptCtx& c) {
  Game& g = c.game;
  if (g.inside) { c.fail("tolamp: inside"); return true; }
  const Map& m = g.map();
  const int n = c.arg(1).empty() ? 0 : std::atoi(c.arg(1).c_str());
  const Vec2 pp = g.pl().p;
  std::vector<std::pair<float, int>> lamps;
  const int r = 60, px = (int)(pp.x / 16), py = (int)(pp.y / 16);
  for (int y = std::max(0, py - r); y < std::min(m.h, py + r); y++)
    for (int x = std::max(0, px - r); x < std::min(m.w, px + r); x++)
      if (m.propAt(x, y) == (int)art::Prop::Lamppost + 1) lamps.push_back({(float)((x - px) * (x - px) + (y - py) * (y - py)), y * m.w + x});
  std::sort(lamps.begin(), lamps.end());
  if (lamps.empty() || n >= (int)lamps.size()) { c.fail("tolamp: no lamppost near"); return true; }
  const int lx = lamps[(size_t)n].second % m.w, ly = lamps[(size_t)n].second / m.w;
  g.teleportGlobal(lx + g.world.ox, ly + 2 + g.world.oy);
  std::printf("script: at the lamp on %d,%d (global %d,%d): lit at %.2f, out at %.2f\n", lx, ly, lx + g.world.ox, ly + g.world.oy,
              life::lampLightHour(lx + g.world.ox, ly + g.world.oy), life::lampOutHour(lx + g.world.ox, ly + g.world.oy));
  return true;
}
EMB_SCRIPT_CMD("tolamp", "tolamp [n]: stand two tiles south of the n-th nearest street lamp (M5 VIEW)", cmdToLamp);

void printStats(const View::M5Stats& s) {
  std::printf("m5 view: posed %d (fallback %d, bakes %d) critters %d bubbles %d lamps lit %d dark %d stalls shut %d bunting %d lanterns %d "
              "windows lit %d dark %d speech %d | music %d tavern %.2f crowd %.2f\n",
              s.posed, s.poseFallback, s.poseBakes, s.critters, s.bubbles, s.lampsLit, s.lampsDark, s.stallsShut, s.bunting, s.festLanterns,
              s.winLit, s.winDark, s.speech, s.music, s.tavernLevel, s.crowd);
}
bool cmdM5Stats(ScriptCtx& c) { printStats(c.view.m5Stats()); return true; }
EMB_SCRIPT_CMD("m5stats", "m5stats: print the view's last-frame M5 counts (M5 VIEW)", cmdM5Stats);

bool expM5(ScriptCtx& c) {
  const View::M5Stats s = c.view.m5Stats();
  const std::string what = lower(c.arg(2));
  if (what == "music") {
    static const char* mn[(int)Music::COUNT] = {"silence", "title", "wild", "night", "town", "cave", "combat", "boss", "tavern"};
    const std::string want = lower(c.arg(3));
    const std::string have = s.music >= 0 && s.music < (int)Music::COUNT ? mn[s.music] : "?";
    if (have != want) { printStats(s); c.fail("expect m5 music " + want + ": playing " + have); }
    return true;
  }
  int v = -1;
  if (what == "posed") v = s.posed;
  else if (what == "fallback") v = s.poseFallback;
  else if (what == "critters") v = s.critters;
  else if (what == "bubbles") v = s.bubbles;
  else if (what == "lampslit") v = s.lampsLit;
  else if (what == "lampsdark") v = s.lampsDark;
  else if (what == "shut") v = s.stallsShut;
  else if (what == "bunting") v = s.bunting;
  else if (what == "lanterns") v = s.festLanterns;
  else if (what == "winlit") v = s.winLit;
  else if (what == "windark") v = s.winDark;
  else if (what == "speech") v = s.speech;
  else if (what == "tavern") v = (int)std::lround(s.tavernLevel * 100);
  else if (what == "crowd") v = (int)std::lround(s.crowd * 100);
  if (v < 0) { c.fail("expect m5 posed|fallback|critters|bubbles|lampslit|lampsdark|shut|bunting|lanterns|winlit|windark|speech|tavern|crowd <min> [max] | music <name>"); return true; }
  const int lo = std::atoi(c.arg(3).c_str()), hi = c.arg(4).empty() ? 1 << 30 : std::atoi(c.arg(4).c_str());
  if (v < lo || v > hi) { printStats(s); c.fail("expect m5 " + what + ": " + std::to_string(v) + " not in [" + std::to_string(lo) + ", " + (c.arg(4).empty() ? std::string("...") : c.arg(4)) + "]"); }
  return true;
}
EMB_SCRIPT_CMD("expect:m5", "expect m5 <what> <min> [max] | music <name>: the view's last-frame M5 counts (M5 VIEW)", expM5);

}  // namespace
