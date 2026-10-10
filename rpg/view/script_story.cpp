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
  c.fail("story start <script> | show | complete | goto | talk [script] | board | herald | read [n]");
  return true;
}
EMB_SCRIPT_CMD("story", "story start <script> | show | complete | goto | talk [script] | board | herald | read [n]: drive the story engine (M4)", cmdStory);

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
