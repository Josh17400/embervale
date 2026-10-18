// M4 STORY lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Test only: none of them
// touches a save.
//   story start <script>        (M6b: or a saga spec id) begin a story here (its hook: the settlement the player is in or the start village, the
//                               nearest capital for a herald's story, the ruin the player is in); its first dialogue opens
//   story show [script]         open the running story's current dialogue (as if its person had been talked to)
//   story complete [script]     its current objective is done (as if the player had done it)
//   story goto [script]         to the current stage's marker (a person spawns near the player there)
//   story talk [script]         talk to the current stage's person (walks them over first if they are about)
//   story speak <trade>         step up to the nearest person of that trade and talk (guard, innkeeper...)
//   story board                 stand before the nearest notice board and read it
//   story herald                stand before the capital's herald and talk to them
//   story read [n]              inside a ruin: read its n-th lore prop (default: the next unread one)
//   story fail [script]         (P1b) the current escort or bout is lost (the person died / the player went down): to its else
//   story late [script]         (P1b) the current stage's clock runs out (the day moves on to it): to its deadline stage
//   story bout win|lose [script]  (P1b) the current bout to its moment: the opponent kneels and yields / the player is
//                               left on 1 HP (staged face to face in front of the player; the goal moves on after it)
//   story escort [script]       (P1b) the walkers of the current stage (and a drove's beasts) set down beside the player
//   story near [script]         (P1b) stand two tiles short of the current stage's person (any map: inside a cave too)
//   story use [script]          (P1b) stand at the foot of the prop the current goal wants used (within 16 tiles) and use it
//   story days <n>              (P1b) n days pass (a stage's clock runs down)
//   expect news [words]         the player has heard some realm news (whose line contains the words)
//   expect story <script> <stage|done|running>   the script is at that stage / has ended / runs
//   expect lore [complete]      a Lost History entry exists (and is complete)
//   expect board                a notice board stands within 24 tiles
//   expect crier                a capital's herald is about
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/view/script_api.h"

namespace {

std::string lowerS(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
std::string upperS(std::string s) { for (char& c : s) c = (char)std::toupper((unsigned char)c); return s; }

story::Instance* pick(Game& g, const std::string& name) {
  if (!name.empty()) return g.story.byScript(lowerS(name));
  for (story::Instance& in : g.story.running_) if (!in.done) return &in;
  return nullptr;
}

bool cmdStory(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lowerS(c.arg(1));
  // (P1b ENGINE lane) the new branches and moments, for tests and tours
  if ((what == "escort" || what == "near" || what == "use" || what == "bout") && g.mode == Mode::Dialogue) g.mode = Mode::Play;   // (a tour: the talk is done)
  if (what == "fail" || what == "late" || what == "escort") {
    story::Instance* in = pick(g, c.arg(2));
    if (!in) { c.fail("story " + what + ": no running story"); return true; }
    const uint32_t id = in->id;
    const bool ok = what == "fail" ? g.story.debugFail(g, id) : what == "late" ? g.story.debugDeadline(g, id) : story::debugEscort(g, id);
    if (!ok) { c.fail("story " + what + ": the current stage has no " + (what == "fail" ? "else" : what == "late" ? "clock" : "walker")); return true; }
    std::printf("script: story %s -> %s\n", what.c_str(), g.story.find(id) ? g.story.stageName(*g.story.find(id)).c_str() : "(ended)");
    return true;
  }
  if (what == "open") {
    // (P1b tours) step out onto open country (the nearest stretch of 20 x 12 walkable tiles with no building, wall or
    // settlement on it; `story open road`: one with a road or a track running through its middle), facing east
    const bool road = lowerS(c.arg(2)) == "road";
    const Map& m = g.world.over;
    if (g.inside) { c.fail("story open: outdoors only"); return true; }
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    int best = -1, bx = 0, by = 0;
    for (int cy = std::max(8, py - 120); cy < std::min(m.h - 8, py + 120); cy += 2)
      for (int cx = std::max(12, px - 120); cx < std::min(m.w - 12, px + 120); cx += 2) {
        const int d = std::abs(cx - px) + std::abs(cy - py);
        if (best >= 0 && d >= best) continue;
        if (road && m.at(cx, cy) != Ground::Road) continue;
        if (g.settlementAt(Vec2(cx * TILE + 8.0f, cy * TILE + 8.0f)) >= 0 || g.world.siteAt(cx, cy, 16) >= 0) continue;   // (no camp or den at hand)
        int bad = 0;
        for (int y = cy - 5; y <= cy + 5 && bad <= 22; y++)
          for (int x = cx - 9; x <= cx + 9 && bad <= 22; x++) {
            if (m.bldgAt[(size_t)y * m.w + x] >= 0 || m.at(x, y) == Ground::Water || m.at(x, y) == Ground::DeepWater) bad += 100;
            else if (m.blocked(x, y)) bad++;
          }
        if (bad > 22 || m.blocked(cx, cy)) continue;
        best = d; bx = cx; by = cy;
      }
    if (best >= 0) {
      g.teleportGlobal(g.world.ox + bx, g.world.oy + by);
      g.pl().face = 2; g.pl().aim = Vec2(1, 0);
      std::printf("script: story open -> %d %d\n", g.world.ox + bx, g.world.oy + by);
      return true;
    }
    c.fail("story open: no open ground within 90 tiles");
    return true;
  }
  if (what == "days") {   // (P1b) let days pass: a stage's clock runs down (the tracker's days left)
    g.day += std::max(0, std::atoi(c.arg(2).c_str()));
    return true;
  }
  if (what == "bout") {
    const std::string how = lowerS(c.arg(2));
    story::Instance* in = pick(g, c.arg(3));
    if (!in || (how != "win" && how != "lose")) { c.fail("story bout win|lose [script]: no running story or no outcome"); return true; }
    if (!story::debugBout(g, in->id, how == "win")) { c.fail("story bout: the current stage is no bout with its opponent at hand"); return true; }
    std::printf("script: story bout %s\n", how.c_str());
    return true;
  }
  if (what == "start") {
    const std::string id = lowerS(c.arg(2));
    const story::dsl::Script* sp = story::scriptById(id);   // (M6b: a saga spec id composes its story)
    if (!sp) { c.fail("story start: no script '" + id + "'"); return true; }
    const story::dsl::Script& s = *sp;
    int32_t gx, gy;
    story::playerGlobal(g, gx, gy);
    int site = g.curSite >= 0 && g.world.sites[(size_t)g.curSite].settlement() ? g.curSite : g.world.startSite;
    if (s.hook == story::dsl::HookKind::Herald) site = g.world.findSiteNear(gx, gy, SiteType::City, 4, true);
    if (s.hook == story::dsl::HookKind::Ruin) site = g.inside && g.subSite >= 0 ? g.subSite : g.world.findSiteNear(gx, gy, SiteType::Ruin, 3);
    int actor = -1;
    if (s.hook == story::dsl::HookKind::Herald)
      for (const Actor& a : g.actors) if (a.role == Role::Herald && a.slot == story::HERALD_SLOT) actor = a.id;
    std::string why;
    const uint32_t id2 = g.story.start(g, id, actor, site, &why);
    if (!id2) { c.fail("story start " + id + ": " + why); return true; }
    std::printf("script: story %s started (instance %u, stage %s)\n", id.c_str(), id2, g.story.stageName(*g.story.find(id2)).c_str());
    story::Instance* in = g.story.find(id2);
    if (in && !g.story.isGoal(*in)) {
      g.dlg = Dialogue();
      const story::dsl::StageDef& G = s.stages[(size_t)in->stage];
      const story::Binding* b = story::bindingOf(*in, G.talk);
      g.dlg.speaker = b ? b->name : std::string();
      g.story.showDialogue(g, *in);
      g.dlg.opts.push_back({"FAREWELL.", 0, 0});
      g.mode = Mode::Dialogue;
    }
    return true;
  }
  if (what == "board") {
    int32_t gx, gy;
    story::playerGlobal(g, gx, gy);
    for (int si : g.world.nearSites) {
      int tx = 0, ty = 0;
      if (!story::isBoardSite(g, si) || !story::boardTile(g, si, tx, ty) || g.world.over.propAt(tx, ty) != (int)art::Prop::NoticeBoard + 1) continue;
      if (std::abs(g.world.ox + tx - gx) > 80 || std::abs(g.world.oy + ty - gy) > 80) continue;
      g.pl().p = Vec2(tx * TILE + 8.0f, (ty + 1) * TILE + 10.0f);
      g.pl().face = 1; g.pl().aim = Vec2(0, -1);
      if (!story::useBoard(g, tx, ty)) c.fail("story board: the board would not open");
      return true;
    }
    if (c.waited < 4.0f) return false;   // (boards are laid a moment after a town comes into view)
    c.fail("story board: no notice board near");
    return true;
  }
  if (what == "herald") {
    for (const Actor& a : g.actors)
      if (a.role == Role::Herald && a.slot == story::HERALD_SLOT) {
        g.pl().p = a.p + Vec2(0, 18);
        g.pl().face = 1; g.pl().aim = Vec2(0, -1);
        if (story::host().talkTo) story::host().talkTo(g, a.id);
        return true;
      }
    if (c.waited < 4.0f) return false;
    c.fail("story herald: no herald about");
    return true;
  }
  if (what == "speak") {   // story speak guard|innkeeper|villager...: step up to the nearest such person and talk
    const int trade = story::dsl::tradeWord(c.arg(2));
    const Actor* best = nullptr;
    float bd = 1e30f;
    for (const Actor& a : g.actors)
      if (a.npc && a.human && (int)a.role == trade && a.st != AState::Dead && !story::isStoryPerson(a)) {
        const float d = len2(a.p - g.pl().p);
        if (d < bd) { bd = d; best = &a; }
      }
    if (!best) { if (c.waited < 4.0f) return false; c.fail("story speak: nobody of that trade about"); return true; }
    g.pl().p = best->p + Vec2(0, 16);
    g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    if (story::host().talkTo) story::host().talkTo(g, best->id);
    return true;
  }
  if (what == "read") {
    if (!g.inside || g.story.ruinProps_.empty()) { c.fail("story read: not in a ruin with lore"); return true; }
    int n = c.arg(2).empty() ? -1 : std::atoi(c.arg(2).c_str());
    if (n < 0) {
      const story::LoreEntry* E = nullptr;
      for (const story::LoreEntry& e : g.story.lore()) if (e.key == g.story.ruinPropsSite_) E = &e;
      n = 0;
      for (size_t i = 0; i < g.story.ruinProps_.size(); i++)
        if (!E || !(E->foundMask & (1u << std::min(g.story.ruinProps_[i].clue, 30)))) { n = (int)i; break; }
    }
    if (n >= (int)g.story.ruinProps_.size()) { c.fail("story read: no such lore prop"); return true; }
    const story::Engine::PropRef p = g.story.ruinProps_[(size_t)n];
    // stand below it, facing it (a free tile beside it when below is blocked)
    int sx = p.tx, sy = p.ty + 1;
    if (g.sub.blocked(sx, sy)) { if (!g.sub.blocked(p.tx - 1, p.ty)) { sx = p.tx - 1; sy = p.ty; } else if (!g.sub.blocked(p.tx + 1, p.ty)) { sx = p.tx + 1; sy = p.ty; } }
    g.pl().p = Vec2(sx * TILE + 8.0f, sy * TILE + 10.0f);
    if (!story::readLore(g, p.tx, p.ty)) c.fail("story read: it would not read");
    else g.story.onUseProp(g, p.prop, p.tx, p.ty);
    return true;
  }
  story::Instance* in = pick(g, c.arg(2));
  if (!in) { c.fail("story " + what + ": no story running"); return true; }
  const story::dsl::Script* s = story::scriptOf(*in);
  if (what == "complete") {
    if (!g.story.debugComplete(g, in->id)) c.fail("story complete: the current stage is not an objective");
    return true;
  }
  if (what == "show") {
    if (g.story.isGoal(*in) || g.story.isEnd(*in)) { c.fail("story show: the current stage is not a dialogue"); return true; }
    g.dlg = Dialogue();
    const story::Binding* b = s ? story::bindingOf(*in, s->stages[(size_t)in->stage].talk) : nullptr;
    g.dlg.speaker = b ? b->name : std::string();
    for (const Actor& a : g.actors) if (g.story.talksTo(g, *in, a)) { g.dlg.actor = a.id; g.dlg.speaker = a.name; }
    g.story.showDialogue(g, *in);
    g.dlg.opts.push_back({"FAREWELL.", 0, 0});
    g.mode = Mode::Dialogue;
    return true;
  }
  if (what == "goto") {
    for (const Quest& q : g.quests)
      if (q.id == in->questId && q.hasPos) { g.teleportGlobal(q.tgx, q.tgy + 2); return true; }
    c.fail("story goto: the stage has no place");
    return true;
  }
  if (what == "talk") {
    for (Actor& a : g.actors)
      if (g.story.talksTo(g, *in, a)) {
        g.pl().p = a.p + Vec2(0, 16);
        g.pl().face = 1; g.pl().aim = Vec2(0, -1);
        if (story::host().talkTo) story::host().talkTo(g, a.id);
        return true;
      }
    if (c.waited < 6.0f) return false;   // (a person spawns a moment after the player comes near)
    c.fail("story talk: nobody here is the stage's person");
    return true;
  }
  // (P1b ENGINE lane) tours: stand two tiles short of the stage's person (any map), facing them; stand at the foot of
  // the prop the stage wants used and use it; let days pass (the stage's clock runs down)
  if (what == "near") {
    // the stage's person: the one it talks with, else its goal's (a bout's opponent, an escort's walker)
    int goalSlot = -1;
    if (s && g.story.isGoal(*in)) {
      const int ri = s->role(s->stages[(size_t)in->stage].goal.role);
      if (ri >= 0) goalSlot = story::personSlot(in->id, ri);
    }
    for (Actor& a : g.actors)
      if (g.story.talksTo(g, *in, a) || (goalSlot >= 0 && story::isStoryPerson(a) && a.slot == goalSlot && a.st != AState::Dead)) {
        const Vec2 at = ClimbOps::freeSpot(g, (int)std::floor(a.p.x / TILE), (int)std::floor(a.p.y / TILE) + 2);
        g.pl().p = at;
        g.pl().aim = norm(a.p - at); g.pl().face = 1;
        return true;
      }
    if (c.waited < 6.0f) return false;
    c.fail("story near: nobody here is the stage's person");
    return true;
  }
  if (what == "use") {
    const Map& m = g.map();
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    const story::dsl::Script* s = story::scriptOf(*in);
    const int want = s && g.story.isGoal(*in) ? s->stages[(size_t)in->stage].goal.prop : -1;
    for (int r = 0; r <= 16 && want >= 0; r++)
      for (int y = py - r; y <= py + r; y++)
        for (int x = px - r; x <= px + r; x++) {
          if (std::max(std::abs(x - px), std::abs(y - py)) != r || !m.in(x, y) || m.propAt(x, y) != want + 1) continue;
          const Vec2 at = ClimbOps::freeSpot(g, x, y + 1);
          g.pl().p = at;
          g.pl().aim = norm(Vec2(x * TILE + 8.0f, y * TILE + 8.0f) - at);
          g.pl().face = 1;
          int wx = 0, wy = 0;
          const int wp = story::wantedProp(g, wx, wy);
          if (!wp || !story::useWantedProp(g, wp - 1, wx, wy)) { c.fail("story use: the prop does not answer the use button"); return true; }
          std::printf("script: story use -> %s\n", g.story.find(in->id) ? g.story.stageName(*g.story.find(in->id)).c_str() : "(ended)");
          return true;
        }
    if (c.waited < 2.0f) return false;
    c.fail("story use: no prop the stage wants within 16 tiles");
    return true;
  }

  c.fail("story start <script> | show | complete | goto | talk | near | use | fail | late | escort [script] | bout win|lose | days <n> | board | herald | read [n]");
  return true;
}
EMB_SCRIPT_CMD("story", "story start <script> | show | complete | goto | talk | near | use | fail | late | escort [script] | bout win|lose [script] | days <n> | board | herald | read [n]: drive the story engine (M4, P1b)", cmdStory);

bool expNews(ScriptCtx& c) {
  Game& g = c.game;
  const std::string words = upperS(c.rest(2));
  for (const realm::WorldEvent& e : g.realm.events())
    if (e.heard && (words.empty() || story::newsLine(g, e, 0).find(words) != std::string::npos)) {
      std::printf("script: news heard: %s\n", story::newsLine(g, e, 0).c_str());
      return true;
    }
  c.fail("expect news" + (words.empty() ? std::string() : " " + words) + ": no such news heard");
  return true;
}
EMB_SCRIPT_CMD("expect:news", "expect news [words]: the player has heard realm news (containing the words) (M4)", expNews);

bool expStory(ScriptCtx& c) {
  Game& g = c.game;
  const std::string id = lowerS(c.arg(2)), want = lowerS(c.arg(3));
  story::Instance* in = g.story.byScript(id);
  if (want == "done") {
    if (in) { c.fail("expect story " + id + " done: still at " + g.story.stageName(*in)); return true; }
    if (g.story.timesPlayed(id) < 1) c.fail("expect story " + id + " done: it never ended");
    return true;
  }
  if (!in) { c.fail("expect story " + id + ": not running"); return true; }
  if (want.empty() || want == "running") return true;
  if (g.story.stageName(*in) != want) c.fail("expect story " + id + " " + want + ": at " + g.story.stageName(*in));
  return true;
}
EMB_SCRIPT_CMD("expect:story", "expect story <script> <stage|done|running> (M4)", expStory);

bool expLore(ScriptCtx& c) {
  Game& g = c.game;
  const bool complete = lowerS(c.arg(2)) == "complete";
  for (const story::LoreEntry& e : g.story.lore())
    if (!complete || e.complete) { std::printf("script: lost history: %s (%d/%d)\n", e.title.c_str(), e.found, e.total); return true; }
  c.fail(complete ? "expect lore complete: no complete Lost History entry" : "expect lore: no Lost History entry");
  return true;
}
EMB_SCRIPT_CMD("expect:lore", "expect lore [complete]: a (complete) Lost History entry exists (M4)", expLore);

bool expBoard(ScriptCtx& c) {
  Game& g = c.game;
  const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
  for (int y = py - 24; y <= py + 24; y++)
    for (int x = px - 24; x <= px + 24; x++)
      if (!g.inside && g.world.over.propAt(x, y) == (int)art::Prop::NoticeBoard + 1) return true;
  if (c.waited < 4.0f) return false;
  c.fail("expect board: no notice board within 24 tiles");
  return true;
}
EMB_SCRIPT_CMD("expect:board", "expect board: a notice board stands within 24 tiles (M4)", expBoard);

bool expHerald(ScriptCtx& c) {
  for (const Actor& a : c.game.actors) if (a.role == Role::Herald && a.slot == story::HERALD_SLOT) return true;
  if (c.waited < 4.0f) return false;
  c.fail("expect herald: no herald about");
  return true;
}
EMB_SCRIPT_CMD("expect:crier", "expect crier: a capital's herald (the story's town crier) is about (M4)", expHerald);

}  // namespace
