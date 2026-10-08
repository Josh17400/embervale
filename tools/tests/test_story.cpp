// rpg_test --story [--seeds A..B] [--verbose]: the M4 story engine (rpg/story). STORY lane.
//   - the library parses and validates (no unknown or unreachable stage, an ending everywhere, no unbound role or
//     placeholder without a binding, phone-sized lines), and no line is used by two scripts
//   - every script is cast on the real world of each seed (its hook: an innkeeper, a smith, a notice board, a herald, a
//     ruin, a realm event the test forces), and EVERY path of it is walked headlessly to an ending: every option of every
//     dialogue (both outcomes of a check, options behind conditions too), every objective completed through the debug
//     helpers, no stage left dead and no "{" left in any text the player would read
//   - a save made mid-campaign carries the story block byte-identically (alone and through Game::serialize)
//   - rumours spread 30 tiles a day from the event and fade after 30 days (60 for a fall), and hearing one marks it heard
//   - notice boards stand on a free paved square tile in towns and cities, the same tile after leaving and coming back;
//     capitals have a herald
//   - ruins hold 3-6 lore props on the same tiles every visit; reading them all completes a Lost History entry
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/world/source.h"
#include "tools/tests/tests.h"

namespace {

bool g_verbose = false;

struct Snap {
  story::Engine st;
  realm::Realm rm;
  int gold = 0, plXp = 0, plLevel = 1, nextQuestId = 1, trackedQuest = -1, day = 1;
  std::vector<Item> inv;
  std::map<uint64_t, int32_t> marks;
  std::vector<Quest> quests;
};
Snap snap(const Game& g) {
  Snap s;
  s.st = g.story; s.rm = g.realm; s.gold = g.gold; s.plXp = g.plXp; s.plLevel = g.plLevel; s.nextQuestId = g.nextQuestId;
  s.trackedQuest = g.trackedQuest; s.inv = g.inv; s.marks = g.marks; s.quests = g.quests; s.day = g.day;
  return s;
}
void restore(Game& g, const Snap& s) {
  g.story = s.st; g.realm = s.rm; g.gold = s.gold; g.plXp = s.plXp; g.plLevel = s.plLevel; g.nextQuestId = s.nextQuestId;
  g.trackedQuest = s.trackedQuest; g.inv = s.inv; g.marks = s.marks; g.quests = s.quests; g.day = s.day;
  g.events.clear();
  g.mode = Mode::Play;
}

void steps(Game& g, int n) {
  for (int i = 0; i < n; i++) { g.update(SIM_DT, Input()); g.events.clear(); }
}

int32_t pgx(const Game& g) { return g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE); }
int32_t pgy(const Game& g) { return g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE); }

// the hook place of a script on this world (a settlement for npcs, boards and events, a capital for heralds, a ruin)
int hookSite(Game& g, const story::dsl::Script& s) {
  const int32_t gx = pgx(g), gy = pgy(g);
  switch (s.hook) {
    case story::dsl::HookKind::Herald: {
      // a capital of the kingdom the player stands in (the realm knows it)
      const ew::Gid land = story::landAt(g, gx, gy);
      if (const realm::KingdomState* K = g.realm.kingdom(land)) { const int h = story::siteByIdLoad(g, K->capital); if (h >= 0) return h; }
      return g.world.findSiteNear(gx, gy, SiteType::City, 4, true);
    }
    case story::dsl::HookKind::Ruin: return g.world.findSiteNear(gx, gy, SiteType::Ruin, 3);
    case story::dsl::HookKind::Board: {
      const int t = g.world.findSiteNear(gx, gy, SiteType::Town, 3);
      return t >= 0 ? t : g.world.findSiteNear(gx, gy, SiteType::City, 4);
    }
    default: return g.world.startSite;
  }
}

// the world events an event story needs, forced at its hook place (a famine there, a war of its kingdom)
void forceHookEvent(Game& g, const story::dsl::Script& s, int site) {
  if (s.hook != story::dsl::HookKind::Event || site < 0) return;
  const Site S = g.world.sites[(size_t)site];
  const int32_t hx = g.world.ox + S.ex, hy = g.world.oy + S.ey;
  const ew::Gid land = story::landAt(g, hx, hy);
  const ew::Gid home = S.homeKingdom >= 0 ? g.world.kingdoms[(size_t)S.homeKingdom].id : 0;
  g.realm.noteSite(S.id, home, (uint8_t)S.type, hx, hy);
  if (s.hookEv == (int)realm::EvType::Famine) g.realm.forceFamine(S.id, g.day);
  else if (s.hookEv == (int)realm::EvType::WarDeclared) {
    ew::Gid other = 0;
    for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != land && !K.fallen) { other = K.id; break; }
    if (other && land) {
      g.realm.forceWar(other, land, g.day);
      g.realm.forceEvent(realm::EvType::WarDeclared, other, land, S.id, hx, hy, g.day);
    }
  } else g.realm.forceEvent((realm::EvType)s.hookEv, land, 0, S.id, hx, hy, g.day);
}

// an actor of the hook's trade in the hook place (talking to a real person when one is out), else -1 (the place)
int hookActor(Game& g, const story::dsl::Script& s, int site) {
  if (s.hook != story::dsl::HookKind::Npc && s.hook != story::dsl::HookKind::Herald) return -1;
  for (const Actor& a : g.actors) {
    if (!a.npc || !a.human || story::isStoryPerson(a)) continue;
    const int hs = story::homeSiteOf(g, a);
    if (hs != site) continue;
    if (s.hook == story::dsl::HookKind::Herald ? a.role == Role::Herald : (s.hookTrade < 0 || (int)a.role == s.hookTrade)) return a.id;
  }
  return -1;
}

struct WalkStats { int paths = 0, endings = 0, stagesSeen = 0, choices = 0; std::set<std::string> endNames; };

// walk every path of instance `id` from its current stage (the engine state is restored between branches)
int walk(Game& g, uint32_t id, const std::string& script, WalkStats& ws, std::map<std::string, int>& visits, int depth, std::string& trail) {
  int bad = 0;
  story::Instance* in = g.story.find(id);
  if (!in) { out("FAIL: story %s: the instance vanished on the way (%s)\n", script.c_str(), trail.c_str()); return 1; }
  // every text the player would read here has its cast filled in
  for (const std::string& t : g.story.stageTexts(g, *in))
    if (t.find('{') != std::string::npos || t.find('}') != std::string::npos) {
      out("FAIL: story %s stage %s: an unresolved placeholder: %s\n", script.c_str(), g.story.stageName(*in).c_str(), t.c_str());
      bad++;
    }
  const std::string stName = g.story.stageName(*in);
  if (in->done || g.story.isEnd(*in)) {
    ws.paths++;
    ws.endNames.insert(stName);
    bool closed = false;
    for (const Quest& q : g.quests) if (q.id == in->questId && q.type == QType::Story && q.state == QState::Done) closed = true;
    if (!closed) { out("FAIL: story %s ended at %s but its journal quest is not done\n", script.c_str(), stName.c_str()); bad++; }
    if (g_verbose) out("  path %s -> %s\n", trail.c_str(), stName.c_str());
    return bad;
  }
  if (++visits[stName] > 2 || depth > 80) {
    out("FAIL: story %s: a loop through stage %s (%s)\n", script.c_str(), stName.c_str(), trail.c_str());
    visits[stName]--;
    return bad + 1;
  }
  ws.stagesSeen++;
  const Snap s0 = snap(g);
  if (g.story.isGoal(*in)) {
    if (!g.story.debugComplete(g, id)) { out("FAIL: story %s stage %s: the objective cannot be completed\n", script.c_str(), stName.c_str()); bad++; }
    else {
      story::Instance* after = g.story.find(id);
      const std::string t0 = trail;
      if (after) trail += ">" + g.story.stageName(*after);
      bad += walk(g, id, script, ws, visits, depth + 1, trail);
      trail = t0;
    }
    restore(g, s0);
    visits[stName]--;
    return bad;
  }
  const int n = g.story.optionCount(*in);
  if (n <= 0) { out("FAIL: story %s stage %s: a dialogue with no options\n", script.c_str(), stName.c_str()); visits[stName]--; return bad + 1; }
  for (int i = 0; i < n; i++) {
    const bool check = g.story.optionIsCheck(*in, i);
    for (int c = check ? 1 : 0; c <= (check ? 2 : 0); c++) {
      restore(g, s0);
      ws.choices++;
      if (!g.story.choose(g, id, i, true, c)) { out("FAIL: story %s stage %s: option %d cannot be chosen\n", script.c_str(), stName.c_str(), i); bad++; continue; }
      story::Instance* after = g.story.find(id);
      if (!after) { out("FAIL: story %s: lost after option %d of %s\n", script.c_str(), i, stName.c_str()); bad++; continue; }
      const std::string t0 = trail;
      trail += ">" + std::to_string(i) + (check ? (c == 1 ? "+" : "-") : "") + ":" + g.story.stageName(*after);
      bad += walk(g, id, script, ws, visits, depth + 1, trail);
      trail = t0;
    }
  }
  restore(g, s0);
  visits[stName]--;
  return bad;
}

int storySeed(uint64_t seed, int& scriptsCast, int& pathsWalked) {
  int bad = 0;
  auto fail = [&](const std::string& m) { out("FAIL: story seed %llu: %s\n", (unsigned long long)seed, m.c_str()); bad++; };
  Game g(seed);
  g.newEndlessGame(seed);
  g.mode = Mode::Play;
  g.godMode = true;
  g.noWildSpawns = true;
  steps(g, 90);
  const story::dsl::Library& L = story::dsl::library();

  // ---- every script cast on this world, every path walked
  for (const story::dsl::Script& s : L.scripts) {
    const Snap s0 = snap(g);
    const int site = hookSite(g, s);
    if (site < 0) { fail("no hook place for " + s.id); continue; }
    forceHookEvent(g, s, site);
    const int actor = hookActor(g, s, site);
    std::string why;
    const uint32_t id = g.story.start(g, s.id, actor, site, &why);
    if (!id) { fail("cannot cast " + s.id + ": " + why); restore(g, s0); continue; }
    scriptsCast++;
    WalkStats ws;
    std::map<std::string, int> visits;
    std::string trail = g.story.stageName(*g.story.find(id));
    bad += walk(g, id, s.id, ws, visits, 0, trail);
    pathsWalked += ws.paths;
    // every ending of the script was reached by some path
    std::set<std::string> ends;
    for (const story::dsl::StageDef& G : s.stages) if (G.kind == story::dsl::StageKind::End) ends.insert(G.name);
    for (const std::string& e : ends)
      if (!ws.endNames.count(e)) fail(s.id + ": no path reaches the ending '" + e + "'");
    if (g_verbose || seed == 1)
      out("story seed %llu: %-12s cast %s (giver %s), %d paths, %d choices, %zu endings\n", (unsigned long long)seed, s.id.c_str(),
          actor >= 0 ? "on a person" : "on a place", g.story.find(id) ? g.story.find(id)->cast[0].name.c_str() : "?", ws.paths, ws.choices, ws.endNames.size());
    restore(g, s0);
  }

  // ---- a save made mid-campaign keeps the story block byte for byte
  {
    const Snap s0 = snap(g);
    const story::dsl::Script* camp = nullptr;
    for (const story::dsl::Script& s : L.scripts) if (s.tier == 3) camp = &s;
    if (!camp) fail("no campaign in the library");
    else {
      const int site = hookSite(g, *camp);
      const uint32_t id = g.story.start(g, camp->id, hookActor(g, *camp, site), site);
      if (!id) fail("the campaign cannot be cast for the save test");
      else {
        for (int k = 0; k < 4; k++) {
          story::Instance* in = g.story.find(id);
          if (!in || in->done) break;
          if (g.story.isGoal(*in)) g.story.debugComplete(g, id);
          else g.story.choose(g, id, 0, true, 1);
        }
        std::vector<uint8_t> a, b, c;
        g.story.serialize(a);
        story::Engine e2;
        if (!e2.deserialize(a)) fail("the story block does not load");
        e2.serialize(b);
        if (a != b) fail("the story block does not round-trip byte-identically");
        std::vector<uint8_t> save;
        g.serialize(save);
        Game g2(seed);
        if (!g2.deserialize(save)) fail("a save made mid-campaign does not load");
        else {
          g2.story.serialize(c);
          if (a != c) fail("the story block changed through Game::serialize / deserialize");
          const story::Instance* in2 = g2.story.find(id);
          if (!in2 || in2->stage != g.story.find(id)->stage) fail("the campaign's stage did not survive the save");
        }
        // the empty phase A block (the fixture's) still loads as nothing running
        const std::vector<uint8_t> v1 = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        story::Engine e3;
        if (!e3.deserialize(v1) || !e3.running().empty()) fail("the version 1 empty block does not load as nothing running");
      }
    }
    restore(g, s0);
  }

  // ---- rumours: reach grows 30 tiles a day, they fade after 30 days (60 for a fall)
  {
    const Snap s0 = snap(g);
    const int32_t px = pgx(g), py = pgy(g);
    const ew::Gid land = story::landAt(g, px, py);
    const int d0 = g.day;
    g.realm.forceEvent(realm::EvType::Famine, land, 0, 0, px + 200, py, d0);
    const realm::WorldEvent e = g.realm.events().back();
    if (story::rumourReaches(e, d0, px, py)) fail("a rumour 200 tiles away was heard the day it happened");
    if (story::rumourReaches(e, d0 + 5, px, py)) fail("a rumour travelled faster than 30 tiles a day");
    if (!story::rumourReaches(e, d0 + 7, px, py)) fail("a rumour 200 tiles away had not arrived after 7 days");
    if (story::rumourReaches(e, d0 + 31, px, py)) fail("a famine rumour lived past 30 days");
    g.realm.forceEvent(realm::EvType::TownTaken, land, 0, 0, px + 10, py, d0);
    const realm::WorldEvent f = g.realm.events().back();
    if (!story::rumourReaches(f, d0 + 45, px, py)) fail("the news of a fall faded before 60 days");
    if (story::rumourReaches(f, d0 + 61, px, py)) fail("the news of a fall lived past 60 days");
    g.day = d0 + 8;
    const realm::WorldEvent* r = story::pickRumour(g, px, py, false, 7);
    if (!r) fail("no rumour picked where two have arrived");
    else {
      const uint32_t rid = r->id;
      const std::string line = story::hearEvent(g, *r, g.world.sites[(size_t)g.world.startSite].culture);
      bool heard = false;
      for (const realm::WorldEvent& x : g.realm.events()) if (x.id == rid) heard = x.heard;
      if (!heard) fail("hearing a rumour did not mark it heard");
      bool news = false;
      for (const Event& ev : g.events) if (ev.type == Ev::News && ev.a == (int)rid) news = true;
      if (!news) fail("hearing a rumour emitted no Ev::News");
      if (line.empty() || line.size() > 300 || line.find('{') != std::string::npos) fail("a bad rumour line: " + line);
      if (seed == 1) out("story seed %llu: rumour: %s\n", (unsigned long long)seed, line.c_str());
    }
    if (story::pickRumour(g, px, py, true, 7) && !story::warNews(story::pickRumour(g, px, py, true, 7)->type)) fail("a guard told peace-time news");
    if (story::speakerChance((int)Role::Innkeeper) != 70 || story::speakerChance((int)Role::Traveller) != 50 ||
        story::speakerChance((int)Role::Guard) != 60 || story::speakerChance((int)Role::Villager) != 25)
      fail("the speaker chances are not VISION_PLAN 4.6's");
    // the voices differ between cultures
    std::set<std::string> voices;
    for (int v = 0; v < 8; v++) {
      for (const Site& s : g.world.sites)
        if (s.culture && story::voiceOf(g, s.culture) == v) { voices.insert(story::newsLine(g, f, s.culture)); break; }
    }
    if (seed == 1) for (const std::string& v : voices) out("story seed %llu: voice: %s\n", (unsigned long long)seed, v.c_str());
    restore(g, s0);
  }

  // ---- notice boards in a town and a city (the same tile after leaving and coming back), a herald in a capital
  {
    const SiteType kinds[2] = {SiteType::Town, SiteType::City};
    for (SiteType t : kinds) {
      const int si = g.world.findSiteNear(pgx(g), pgy(g), t, 4);
      if (si < 0) { fail(std::string("no ") + siteTypeName(t) + " to put a board in"); continue; }
      const ew::Gid sid = g.world.sites[(size_t)si].id;
      const int32_t hx = g.world.ox + g.world.sites[(size_t)si].ex, hy = g.world.oy + g.world.sites[(size_t)si].ey;
      g.teleportGlobal(hx, hy + 6);
      steps(g, 120);
      const int h = g.world.siteHandle(sid);
      int bx = 0, by = 0;
      if (h < 0 || !story::boardTile(g, h, bx, by) || g.world.over.propAt(bx, by) != (int)art::Prop::NoticeBoard + 1) {
        fail(std::string("no notice board in the ") + siteTypeName(t) + " " + (h >= 0 ? g.world.sites[(size_t)h].name : std::string("?")));
        continue;
      }
      const int32_t gbx = g.world.ox + bx, gby = g.world.oy + by;
      if (g.world.over.at(bx, by) != Ground::Plaza) fail("a notice board off the paving");
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
          if ((dx || dy) && (g.world.over.blocked(bx + dx, by + dy) || g.world.over.propAt(bx + dx, by + dy))) fail("a notice board crowds a path");
      // read it
      if (!story::useBoard(g, bx, by) || g.mode != Mode::Dialogue || g.dlg.opts.empty()) fail("the notice board cannot be read");
      else if (seed == 1) out("story seed %llu: board of %s: %s (%zu options)\n", (unsigned long long)seed, g.world.sites[(size_t)h].name.c_str(), g.dlg.text.c_str(), g.dlg.opts.size());
      g.mode = Mode::Play;
      // away and back: the same tile
      g.teleportGlobal(hx + 1500, hy + 1500);
      steps(g, 30);
      g.teleportGlobal(hx, hy + 6);
      steps(g, 120);
      const int h2 = g.world.siteHandle(sid);
      int bx2 = 0, by2 = 0;
      if (h2 < 0 || !story::boardTile(g, h2, bx2, by2) || g.world.ox + bx2 != gbx || g.world.oy + by2 != gby)
        fail(std::string("the board of a ") + siteTypeName(t) + " moved after a visit away");
    }
    // a herald by the seat of a capital
    const int cap = g.world.findSiteNear(pgx(g), pgy(g), SiteType::City, 5, true);
    if (cap < 0) fail("no capital for a herald");
    else {
      const Site C = g.world.sites[(size_t)cap];
      g.teleportGlobal(g.world.ox + C.ex, g.world.oy + C.ey + 6);
      steps(g, 120);
      int herald = -1;
      for (const Actor& a : g.actors) if (a.role == Role::Herald && a.slot == story::HERALD_SLOT) herald = a.id;
      if (herald < 0) fail("no herald in the capital " + C.name);
      else {
        // a declaration of war is proclaimed with the herald in earshot
        const ew::Gid land = story::landAt(g, pgx(g), pgy(g));
        ew::Gid other = 0;
        for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != land) { other = K.id; break; }
        if (land && other) {
          const Actor* H = nullptr;
          for (const Actor& a : g.actors) if (a.id == herald) H = &a;
          if (H) g.pl().p = H->p + Vec2(0, 24);
          g.realm.forceWar(other, land, g.day);
          bool proclaimed = false;
          for (int i = 0; i < 90 && !proclaimed; i++) {
            g.update(SIM_DT, Input());
            for (const Event& ev : g.events) if (ev.type == Ev::Text && ev.s.find("HEAR YE") != std::string::npos) proclaimed = true;
            g.events.clear();
          }
          if (!proclaimed) fail("the herald did not proclaim a declaration of war");
        }
      }
    }
  }

  // ---- ruins: 3-6 lore props, on the same tiles every visit; reading them all completes the Lost History entry
  {
    const int ri = g.world.findSiteNear(pgx(g), pgy(g), SiteType::Ruin, 3);
    if (ri < 0) fail("no ruin near");
    else {
      const ew::Gid rid = g.world.sites[(size_t)ri].id;
      auto loreTiles = [&]() {
        std::vector<int> v;
        for (int i = 0; i < g.sub.w * g.sub.h; i++) {
          const int p = g.sub.prop[(size_t)i];
          if (p && art::isLoreProp((art::Prop)(p - 1))) v.push_back(i * 64 + p);
        }
        return v;
      };
      if (!g.debugEnterSite(ri)) fail("cannot enter the ruin");
      else {
        const std::vector<int> a = loreTiles();
        if (a.size() < 3 || a.size() > 6) fail("a ruin holds " + std::to_string(a.size()) + " lore props (3-6)");
        if (g.story.ruinProps_.size() != a.size()) fail("the ruin's lore props and the engine's list differ");
        g.debugLeave();
        steps(g, 5);
        g.debugEnterSite(g.world.siteHandle(rid));
        if (loreTiles() != a) fail("the ruin's lore props moved between visits");
        int read = 0;
        for (const story::Engine::PropRef p : g.story.ruinProps_) { if (story::readLore(g, p.tx, p.ty)) read++; g.mode = Mode::Play; }
        const story::LoreEntry* E = nullptr;
        for (const story::LoreEntry& e : g.story.lore()) if (e.key == rid) E = &e;
        if (!E) fail("reading the ruin made no Lost History entry");
        else {
          if (read != (int)a.size() || E->found != E->total || !E->complete) fail("reading every clue did not complete the entry");
          if (seed == 1) {
            out("story seed %llu: lost history: %s (%d/%d)\n", (unsigned long long)seed, E->title.c_str(), E->found, E->total);
            for (const std::string& l : E->lines) out("    %s\n", l.c_str());
          }
        }
        g.debugLeave();
      }
    }
  }
  out("story seed %llu: %d failures\n", (unsigned long long)seed, bad);
  return bad;
}

int storyCmd(int argc, char** argv) {
  uint64_t A = 1, B = 5;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--seeds") && i + 1 < argc) parseSeedRange(argv[++i], A, B);
    if (!strcmp(argv[i], "--verbose")) g_verbose = true;
  }
  int bad = 0;
  auto t0 = std::chrono::steady_clock::now();
  // ---- the library
  const story::dsl::Library& L = story::dsl::library();
  for (const std::string& e : L.errors) { printf("FAIL: library: %s\n", e.c_str()); bad++; }
  const story::LibraryStats st = story::Engine::libraryStats();
  int tier2 = 0, tier3 = 0, campaignStages = 0;
  for (const story::dsl::Script& s : L.scripts) {
    if (s.tier == 2) tier2++;
    else { tier3++; campaignStages = std::max(campaignStages, (int)s.stages.size()); }
  }
  if (tier2 < 10) { printf("FAIL: library: %d story quests (want >= 10)\n", tier2); bad++; }
  if (tier3 < 1 || campaignStages < 10) { printf("FAIL: library: no campaign of 10+ stages\n"); bad++; }
  // no line used by two scripts (lines and journal entries; options of 20+ characters)
  {
    std::map<std::string, std::string> owner;
    for (const story::dsl::Script& s : L.scripts) {
      std::set<std::string> mine;
      auto see = [&](const std::string& t) {
        if (t.size() < 20 || !mine.insert(t).second) return;
        auto it = owner.find(t);
        if (it != owner.end()) { printf("FAIL: library: '%s' is used by %s and %s\n", t.c_str(), it->second.c_str(), s.id.c_str()); bad++; }
        else owner[t] = s.id;
      };
      see(s.hint);
      for (const story::dsl::StageDef& G : s.stages) {
        see(G.say); see(G.journal);
        for (const story::dsl::OptDef& o : G.opts) see(o.label);
        for (const story::dsl::EffDef& e : G.effs) see(e.text);
      }
    }
  }
  printf("story library: %d scripts (%d story quests, %d campaign of %d stages), %d stages, %d options, %d words of dialogue\n",
         st.scripts, tier2, tier3, campaignStages, st.stages, st.options, st.words);
  for (const story::dsl::Script& s : L.scripts) printf("  %-12s %-28s %s\n", s.id.c_str(), s.title.c_str(), s.archetype.c_str());
  int cast = 0, paths = 0;
  for (uint64_t s = A; s <= B; s++) { g_curSeed = s; bad += storySeed(s, cast, paths); }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  printf("story: %d scripts cast, %d paths walked to an ending, %.0f ms; %d failures\n", cast, paths, ms, bad);
  return bad ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--story", "M4 story engine: the library, every script cast and every path walked, saves, rumours, boards, heralds, ruins [--seeds A..B] [--verbose]", storyCmd);
