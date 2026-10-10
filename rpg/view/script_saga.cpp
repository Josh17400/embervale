// M6b "Sagas" script commands (rpg/view/script_api.h; `embervale --script-help` lists them). Lead phase A; COMPOSER lane
// after. Test only: none of them touches a save.
//   saga start <archetype> [twist ...] [v<0-7>] [m<motive>] [s<seed>]
//                           compose that story (twists fill slots t1, t2, t3 in order; motive: fear greed grief pride
//                           devotion love duty vengeance shame hope) and begin it here, like `story start <spec id>` (its
//                           first dialogue opens). The spec id is printed; the other `story` commands drive it (story show
//                           / complete / talk / goto act on the running story)
//   saga campaign <id> [arc ...] [v..] [m..] [s..]   the same for a campaign (CAMPAIGNS lane)
//   saga speak [trade]      (COMPOSER) step up to the nearest person (of that trade: innkeeper, priest...; default any) who
//                           would pitch a GENERATED story now (saga.h offerFor; the library's unplayed tale would come
//                           first, so such tellers are passed over) and talk to them: the pitch is the dialogue's first
//                           option. Waits up to 4 s for people to come into play
//   saga accept             (COMPOSER) choose the open dialogue's generated-story pitch (it becomes the newest saga)
//   saga step [n]           (COMPOSER) play the newest saga on n steps (default 1): a goal completes as if done, a
//                           dialogue opens and its FIRST option is chosen; the dialogue shown is what the player reads
//   expect saga <stage|done|running>                 the newest generated story is at that stage / has ended / runs
//   expect saga offered     (COMPOSER) the open dialogue offers a generated story (an SA_SAGA option)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/saga.h"
#include "rpg/story/story.h"
#include "rpg/story/story_internal.h"
#include "rpg/view/script_api.h"

namespace {

std::string g_lastSaga;   // the spec id the last `saga start` / `saga accept` began

std::string lowerW(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32); return s; }

bool isSagaOpt(const DlgOpt& o) { return o.action >= DLG_STORY + story::SA_SAGA && o.action < DLG_STORY + story::SA_SAGA + 100; }

// open the instance's current dialogue as if its person had been talked to
void showInst(Game& g, story::Instance& in) {
  const story::dsl::Script* s = story::scriptOf(in);
  g.dlg = Dialogue();
  const story::Binding* b = s ? story::bindingOf(in, s->stages[(size_t)in.stage].talk) : nullptr;
  g.dlg.speaker = b ? b->name : std::string();
  for (const Actor& a : g.actors) if (g.story.talksTo(g, in, a)) { g.dlg.actor = a.id; g.dlg.speaker = a.name; }
  g.story.showDialogue(g, in);
  g.dlg.opts.push_back({"FAREWELL.", 0, 0});
  g.mode = Mode::Dialogue;
}

story::Instance* lastInst(Game& g) {
  for (story::Instance& i : g.story.running_) if (i.script == g_lastSaga && !i.done) return &i;
  return nullptr;
}

bool cmdSaga(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lowerW(c.arg(1));
  if (what == "speak") {
    const std::string tw = lowerW(c.arg(2));
    const int trade = tw.empty() || tw == "any" ? -2 : story::dsl::tradeWord(tw);
    const Actor* best = nullptr;
    float bd = 1e30f;
    for (const Actor& a : g.actors) {
      if (!a.npc || !a.human || a.st == AState::Dead || story::isStoryPerson(a)) continue;
      if (trade != -2 && (int)a.role != trade) continue;
      const float d = len2(a.p - g.pl().p);
      if (d >= bd) continue;
      const int lib = g.story.offerFor(g, a);
      if (lib >= 0 && g.story.timesPlayed(story::dsl::library().scripts[(size_t)lib].id) == 0) continue;   // (the tale first)
      if (story::saga::offerFor(g, a).empty()) continue;
      bd = d;
      best = &a;
    }
    if (!best) { if (c.waited < 4.0f) return false; c.fail("saga speak: nobody about would tell a generated story"); return true; }
    g.pl().p = best->p + Vec2(0, 16);
    g.pl().face = 1; g.pl().aim = Vec2(0, -1);
    std::printf("script: saga speak: %s (%s)\n", best->name.c_str(), story::dsl::tradeName((int)best->role));
    if (story::host().talkTo) story::host().talkTo(g, best->id);
    return true;
  }
  if (what == "accept") {
    if (g.mode != Mode::Dialogue) { c.fail("saga accept: no dialogue open"); return true; }
    for (size_t o = 0; o < g.dlg.opts.size(); o++) {
      if (!isSagaOpt(g.dlg.opts[o])) continue;
      const int i = g.dlg.opts[o].action - DLG_STORY - story::SA_SAGA;
      const std::string id = i >= 0 && i < (int)g.story.sagaOffers_.size() ? g.story.sagaOffers_[(size_t)i] : std::string();
      g.dialogueChoose((int)o);
      g_lastSaga = id;
      std::printf("script: saga accept: %s\n", id.c_str());
      return true;
    }
    c.fail("saga accept: the dialogue offers no generated story");
    return true;
  }
  if (what == "step") {
    const int n = c.arg(2).empty() ? 1 : std::max(1, std::atoi(c.arg(2).c_str()));
    for (int k = 0; k < n; k++) {
      story::Instance* in = lastInst(g);
      if (!in) { if (k == 0) c.fail("saga step: no saga running"); return true; }
      if (g.story.isGoal(*in)) { g.story.debugComplete(g, in->id); continue; }
      if (g.story.isEnd(*in)) return true;
      showInst(g, *in);
      int pick = -1;
      for (size_t o = 0; o < g.dlg.opts.size() && pick < 0; o++) if (g.dlg.opts[o].action >= DLG_STORY && g.dlg.opts[o].action < DLG_STORY + story::SA_START) pick = (int)o;
      if (pick < 0) { c.fail("saga step: the dialogue has no story option"); return true; }
      g.dialogueChoose(pick);
    }
    return true;
  }
  if (what != "start" && what != "campaign") { c.fail("saga start <archetype> [twists] [vN] [mMOTIVE] [sSEED] | campaign <id> [arcs] | speak [trade] | accept | step [n]"); return true; }
  story::saga::Spec s;
  s.campaign = what == "campaign";
  s.arch = lowerW(c.arg(2));
  s.seed = 1;
  int slot = 0;
  for (size_t i = 3; i < c.a.size(); i++) {
    const std::string w = lowerW(c.a[i]);
    if (w.size() >= 2 && w[0] == 'v' && w[1] >= '0' && w[1] <= '9') { s.voice = std::atoi(w.c_str() + 1); continue; }
    if (w.size() >= 2 && w[0] == 'm' && story::saga::motiveWord(w.substr(1)) >= 0) { s.motive = (story::saga::Motive)story::saga::motiveWord(w.substr(1)); continue; }
    if (w.size() >= 2 && w[0] == 's' && w[1] >= '0' && w[1] <= '9') { s.seed = (uint32_t)std::strtoul(w.c_str() + 1, nullptr, 10); continue; }
    if (s.campaign) s.arcs.push_back(w);
    else if (slot < 3) s.twist[slot++] = w;
  }
  const std::string id = story::saga::specId(s);
  const story::dsl::Script* sc = story::scriptById(id);
  if (!sc) {
    story::saga::Composed cm;
    if (s.campaign) story::saga::composeCampaign(s, cm); else story::saga::compose(s, cm);
    c.fail("saga: " + id + " does not compose: " + (cm.errors.empty() ? std::string("?") : cm.errors[0]));
    return true;
  }
  int32_t gx, gy;
  story::playerGlobal(g, gx, gy);
  int site = g.curSite >= 0 && g.world.sites[(size_t)g.curSite].settlement() ? g.curSite : g.world.startSite;
  if (sc->hook == story::dsl::HookKind::Herald) site = g.world.findSiteNear(gx, gy, SiteType::City, 4, true);
  if (sc->hook == story::dsl::HookKind::Ruin) site = g.inside && g.subSite >= 0 ? g.subSite : g.world.findSiteNear(gx, gy, SiteType::Ruin, 3);
  const story::dsl::HookKind hookKind = sc->hook;
  const int hookTrade = sc->hookTrade;
  // the teller: a person of the hook's trade here (a census resident first: kin and friends need a household), else
  // the first who casts
  std::vector<int> tellers;
  if (hookKind == story::dsl::HookKind::Npc || hookKind == story::dsl::HookKind::Herald)
    for (int pass = 0; pass < 2; pass++)
      for (const Actor& a : g.actors) {
        if (!a.npc || !a.human || story::isStoryPerson(a) || a.role == Role::Child || story::homeSiteOf(g, a) != site) continue;
        if ((a.resident >= 0) != (pass == 0)) continue;
        if (hookKind == story::dsl::HookKind::Herald ? a.role == Role::Herald : (hookTrade < 0 || (int)a.role == hookTrade)) tellers.push_back(a.id);
      }
  if (tellers.empty()) tellers.push_back(-1);
  std::string why;
  uint32_t inst = 0;
  for (size_t k = 0; k < tellers.size() && !inst && k < 12; k++) inst = g.story.start(g, id, tellers[k], site, &why);
  if (!inst) { c.fail("saga start " + id + ": " + why); return true; }
  g_lastSaga = id;
  std::printf("script: saga %s started (instance %u, stage %s)\n", id.c_str(), inst, g.story.stageName(*g.story.find(inst)).c_str());
  story::Instance* in = g.story.find(inst);
  if (in && !g.story.isGoal(*in) && !g.story.isEnd(*in)) showInst(g, *in);
  return true;
}

bool expSaga(ScriptCtx& c) {
  Game& g = c.game;
  const std::string want = lowerW(c.arg(2));
  if (want == "offered") {
    bool any = false;
    for (const DlgOpt& o : g.dlg.opts) if (isSagaOpt(o)) any = true;
    if (g.mode != Mode::Dialogue || !any) c.fail("expect saga offered: the dialogue offers no generated story");
    return true;
  }
  if (g_lastSaga.empty()) { c.fail("expect saga: no saga was started"); return true; }
  const story::Instance* in = nullptr;
  for (const story::Instance& i : g.story.running()) if (i.script == g_lastSaga) in = &i;
  if (want == "done") { if (in && !in->done) c.fail("expect saga done: it still runs at " + g.story.stageName(*in)); return true; }
  if (!in || in->done) { c.fail("expect saga " + want + ": it is not running"); return true; }
  if (want != "running" && g.story.stageName(*in) != want) c.fail("expect saga " + want + ": it is at " + g.story.stageName(*in));
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("saga", "saga start <archetype> [twists] [vN] [mMOTIVE] [sSEED] | campaign <id> [arcs] | speak [trade] | accept | step [n]: generated stories (M6b)", cmdSaga);
EMB_SCRIPT_CMD("expect:saga", "expect saga <stage|done|running|offered>: the newest generated story's state, or a pitch in the open dialogue (M6b)", expSaga);
