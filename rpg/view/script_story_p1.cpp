// P1 "Fresh Roads" STORY lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Test only:
// none of them touches a save.
//   storyp1 played <script> [...]   the library tales named count as played in this world (finished tales are never
//                                   offered again: the board, the ruins and the tellers fall through to generated ones)
//   storyp1 boardwait               days pass (4 at a time) until the nearest notice board's own picker (offers.cpp
//                                   placeOffer) pins a generated campaign (within 120 days; else a generated story). Then
//                                   `story board` reads the board as the player would
//   storyp1 ruinwait [campaign]     inside a ruin: days pass (3 at a time, up to 240) until the ruin's words would begin a
//                                   generated story (or, with `campaign`, a campaign); the player is stood before the
//                                   next unread lore prop, facing it: `key E` then reads it through the game's own path
//   storyp1 hint <archetype> [vN] [sN] [mMOTIVE]   the nearest person greets the player with that story's hint (as a
//                                   teller does: tellerLine) and offers its pitch: the writing review of a teller's line
//   storyp1 teller <tale>           (P1b TALES lane) the nearest person of a library tale's hook trade greets the player
//                                   with its hint and offers its pitch (the opening a player meets)
//   storyp1 begin <tale>            ... and the tale begins with that person as its giver; its first dialogue opens
//                                   (`story start` casts the giver as the place: the tours want the teller's own name)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/view/script_api.h"

namespace {

std::string lowerP1(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

bool cmdStoryP1(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lowerP1(c.arg(1));
  if (what == "played") {
    for (size_t i = 2; i < c.a.size(); i++) g.story.played_[lowerP1(c.a[i])]++;
    return true;
  }
  if (what == "boardwait") {
    // (the board `story board` reads: the first board site of the near list standing within 80 tiles)
    int si = -1;
    int32_t gx, gy;
    story::playerGlobal(g, gx, gy);
    for (int s : g.world.nearSites) {
      int tx = 0, ty = 0;
      if (!story::isBoardSite(g, s) || !story::boardTile(g, s, tx, ty) || g.world.over.propAt(tx, ty) != (int)art::Prop::NoticeBoard + 1) continue;
      if (std::abs(g.world.ox + tx - gx) > 80 || std::abs(g.world.oy + ty - gy) > 80) continue;
      si = s;
      break;
    }
    if (si < 0) { if (c.waited < 4.0f) return false; c.fail("storyp1 boardwait: no notice board near"); return true; }
    // (a campaign when one comes up in 120 days, else the first generated story)
    const int day0 = g.day;
    int skipped = 0;
    for (int k = 0; k < 60; k++) {
      const story::Offer o = story::placeOffer(g, (int)story::dsl::HookKind::Board, si);
      // (P1b) a board's own library tale (THE NAME AMONG THE DEAD, THE UNSIGNED NOTICES) counts as read: the wait is
      // for what the board pins once its tales are told
      if (o.lib >= 0 && !o.campaign() && skipped < 32 && !g.story.played_[o.id()]) { g.story.played_[o.id()]++; skipped++; k--; continue; }
      if (!o.saga.empty() && (o.campaign() || k >= 30)) {
        std::printf("script: storyp1 boardwait: day %d, the board of %s pins %s\n", g.day, g.world.sites[(size_t)si].name.c_str(), o.saga.c_str());
        return true;
      }
      g.day += 4;
      if (k == 29) g.day = day0;
    }
    c.fail("storyp1 boardwait: the board pinned no generated story in 240 days");
    return true;
  }
  if (what == "ruinwait") {
    if (!g.inside || g.subSite < 0 || g.story.ruinProps_.empty()) { c.fail("storyp1 ruinwait: not in a ruin with lore"); return true; }
    const bool wantCampaign = lowerP1(c.arg(2)) == "campaign";
    bool found = false;
    for (int k = 0; k < 80 && !found; k++) {
      const story::Offer o = story::placeOffer(g, (int)story::dsl::HookKind::Ruin, g.subSite);
      if (!o.saga.empty() && o.campaign() == wantCampaign) {
        std::printf("script: storyp1 ruinwait: day %d, the ruin's words would begin %s\n", g.day, o.saga.c_str());
        found = true;
        break;
      }
      g.day += 3;
    }
    if (!found) { c.fail("storyp1 ruinwait: no generated story here in 240 days"); return true; }
    // before the next unread lore prop, facing it (as `story read` stands the player; the read is left to `key E`)
    const story::LoreEntry* E = nullptr;
    for (const story::LoreEntry& e : g.story.lore()) if (e.key == g.story.ruinPropsSite_) E = &e;
    size_t n = 0;
    for (size_t i = 0; i < g.story.ruinProps_.size(); i++)
      if (!E || !(E->foundMask & (1u << std::min(g.story.ruinProps_[i].clue, 30)))) { n = i; break; }
    const story::Engine::PropRef p = g.story.ruinProps_[n];
    int sx = p.tx, sy = p.ty + 1, face = 1;
    if (g.sub.blocked(sx, sy)) {
      if (!g.sub.blocked(p.tx - 1, p.ty)) { sx = p.tx - 1; sy = p.ty; face = 2; }
      else if (!g.sub.blocked(p.tx + 1, p.ty)) { sx = p.tx + 1; sy = p.ty; face = 3; }
    }
    g.pl().p = Vec2(sx * TILE + 8.0f, sy * TILE + 10.0f);
    g.pl().face = face;
    g.pl().aim = face == 1 ? Vec2(0, -1) : face == 2 ? Vec2(1, 0) : Vec2(-1, 0);
    return true;
  }
  if (what == "hint") {
    story::saga::Spec s;
    s.arch = lowerP1(c.arg(2));
    s.seed = 1;
    for (size_t k = 3; k < c.a.size(); k++) {
      const std::string w = lowerP1(c.a[k]);
      if (w.size() >= 2 && w[0] == 'v') s.voice = std::atoi(w.c_str() + 1);
      else if (w.size() >= 2 && w[0] == 's') s.seed = (uint32_t)std::strtoul(w.c_str() + 1, nullptr, 10);
      else if (w.size() >= 2 && w[0] == 'm') { const int m = story::saga::motiveWord(w.substr(1)); if (m >= 0) s.motive = (story::saga::Motive)m; }
    }
    const story::dsl::Script* sc = story::saga::script(story::saga::specId(s));
    if (!sc) { c.fail("storyp1 hint: '" + s.arch + "' does not compose"); return true; }
    // (a teller of the story's own trade when one is about: a priest's story from a priest)
    const Actor* best = nullptr;
    float bd = 1e30f;
    for (int pass = 0; pass < 2 && !best; pass++)
      for (const Actor& a : g.actors) {
        if (!a.npc || !a.human || a.st == AState::Dead || story::isStoryPerson(a) || a.role == Role::Child) continue;
        if (pass == 0 && (sc->hookTrade < 0 || (int)a.role != sc->hookTrade)) continue;
        const float d = len2(a.p - g.pl().p);
        if (d < bd) { bd = d; best = &a; }
      }
    if (!best) { if (c.waited < 4.0f) return false; c.fail("storyp1 hint: nobody about"); return true; }
    g.pl().p = best->p + Vec2(0, 16);
    g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    g.dlg = Dialogue();
    g.dlg.actor = best->id;
    g.dlg.speaker = best->name;
    g.dlg.text = sc->hint;
    g.dlg.opts.push_back({sc->pitch, 0, 0});
    g.dlg.opts.push_back({"FAREWELL.", 0, 0});
    g.mode = Mode::Dialogue;
    std::printf("script: storyp1 hint %s: %s\n", story::saga::specId(s).c_str(), sc->hint.c_str());
    return true;
  }
  if (what == "teller" || what == "begin") {
    // (P1b TALES lane) a library tale told by a real person of its hook trade, as play has it (the tours): `teller`
    // stands the player before the nearest such person, who greets with the tale's hint and offers its pitch; `begin`
    // starts the tale with that person as the giver (story start casts the giver as the place) and opens its first
    // dialogue. A tale whose hook is no person (a board, a ruin) begins with the place, as `story start` does.
    const std::string id = lowerP1(c.arg(2));
    const story::dsl::Library& L = story::dsl::library();
    const int li = L.find(id);
    if (li < 0) { c.fail("storyp1 " + what + ": no library script '" + id + "'"); return true; }
    const story::dsl::Script& sc = L.scripts[(size_t)li];
    const Actor* best = nullptr;
    if (sc.hook == story::dsl::HookKind::Npc) {
      float bd = 1e30f;
      for (int pass = 0; pass < 2 && !best; pass++)
        for (const Actor& a : g.actors) {
          if (!a.npc || !a.human || a.st == AState::Dead || story::isStoryPerson(a) || a.role == Role::Child || a.role == Role::Herald) continue;
          if (pass == 0 && (sc.hookTrade == -2 ? (a.role != Role::Guard && a.role != Role::Jarl) : (int)a.role != sc.hookTrade)) continue;
          const float d = len2(a.p - g.pl().p);
          if (d < bd) { bd = d; best = &a; }
        }
      if (!best) { if (c.waited < 4.0f) return false; c.fail("storyp1 " + what + ": nobody about"); return true; }
      g.pl().p = best->p + Vec2(0, 16);
      g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    }
    if (what == "teller") {
      g.dlg = Dialogue();
      g.dlg.actor = best ? best->id : -1;
      g.dlg.speaker = best ? best->name : sc.title;
      g.dlg.text = sc.hint;
      g.dlg.opts.push_back({sc.pitch, 0, 0});
      g.dlg.opts.push_back({"FAREWELL.", 0, 0});
      g.mode = Mode::Dialogue;
      std::printf("script: storyp1 teller %s: %s\n", id.c_str(), best ? best->name.c_str() : "(the place)");
      return true;
    }
    int site = g.curSite >= 0 && g.world.sites[(size_t)g.curSite].settlement() ? g.curSite : g.world.startSite;
    if (best) site = story::homeSiteOf(g, *best) >= 0 ? story::homeSiteOf(g, *best) : site;
    std::string why;
    const uint32_t inst = g.story.start(g, id, best ? best->id : -1, site, &why);
    if (!inst) { c.fail("storyp1 begin " + id + ": " + why); return true; }
    story::Instance* in = g.story.find(inst);
    std::printf("script: storyp1 begin %s (giver %s)\n", id.c_str(), best ? best->name.c_str() : "the place");
    if (in && !g.story.isGoal(*in)) {
      g.dlg = Dialogue();
      const story::dsl::StageDef& G = sc.stages[(size_t)in->stage];
      const story::Binding* b = story::bindingOf(*in, G.talk);
      g.dlg.actor = best && G.talk == "giver" ? best->id : -1;
      g.dlg.speaker = b ? b->name : std::string();
      g.story.showDialogue(g, *in);
      g.dlg.opts.push_back({"FAREWELL.", 0, 0});
      g.mode = Mode::Dialogue;
    }
    return true;
  }
  c.fail("storyp1: played | boardwait | ruinwait [campaign] | hint <archetype> [vN] [sN] [mMOTIVE] | teller <tale> | begin <tale>");
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("storyp1", "storyp1 played <ids> | boardwait | ruinwait [campaign] | hint <archetype> [vN sN mMOTIVE]: P1 story lane reviews", cmdStoryP1);
