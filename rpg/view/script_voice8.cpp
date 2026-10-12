// M8 Fellowship, VOICE lane: script commands that stage what companions and the people round them say, for the phone
// screenshots (tools/scripts/m8_voice_*.txt) and checks. They never touch the player's save. Member index m defaults
// to 0. (The FELLOWS lane's `party ...` commands recruit, order, down and kill: rpg/view/script_party.cpp.)
//   voice regular                  inside an inn: the first person who would travel with the player as a tavern regular
//                                  (else a befriended local) is talked to (their conversation opens)
//   voice quest [m]                their personal quest is ready (approval 60, bond 40, a stake if none): `party talk`
//                                  then shows its offer
//   voice banter <trigger> [m]     the member says their line for a trigger now (border: the land of the first other
//                                  people met; war, wonder, hound...: party_voice.h Trig names, lower case, '-' for ' ')
//   voice kin [m|fallen]           someone of the member's (or the last fallen's) household is talked to (their own body
//                                  brought beside the player)
//   voice grave [i]                the grave of the i-th fallen (default the last) is spoken to
//   voice deeds [m]                a few deeds remembered (mercy, bandits put down, a theft) so their opinions have matter
//   voice recruit-kin              a census adult here with kin at home joins (the kin and grief scenes)
//   voice choose <n>               choose the n-th option on show (1 the first: a pitch whose words a script cannot know)
//   expect voice banter <words>    the last banter line contains the words
//   expect voice text <words>      the open dialogue's line contains the words
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/game_internal.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/story/story.h"
#include "rpg/view/script_api.h"
#include "rpg/world/source.h"

namespace {

std::string lw(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}
int memberArg(ScriptCtx& c, size_t i) {
  const std::string w = c.arg(i);
  const int m = w.empty() ? 0 : std::atoi(w.c_str());
  if (m < 0 || m >= (int)c.game.party.members.size()) { c.fail("voice: no member " + std::to_string(m)); return -1; }
  return m;
}
// open a conversation with an actor standing beside the player
void talkWith(Game& g, int ai) {
  Actor& a = g.actors[(size_t)ai];
  a.p = g.pl().p + Vec2(14, 0);
  g.pl().aim = Vec2(1, 0);
  g.pl().face = 2;
  HomeOps::talkTo(g, a);
}

bool cmdVoice(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(1));
  if (what == "regular") {
    int best = -1;
    for (int pass = 0; pass < 2 && best < 0; pass++)
      for (size_t i = 1; i < g.actors.size() && best < 0; i++) {
        const Actor& a = g.actors[i];
        if (!a.npc || !a.human || a.comp || a.resident < 0 || a.hostile || a.role == Role::Innkeeper) continue;
        const Game::PartyOffer o = g.partyOffer(a);
        if (o.ok && (pass == 1 || o.origin == party::Origin::Tavern)) best = (int)i;
      }
    if (best < 0)   // (nobody here would come of their own accord: a local befriended)
      for (size_t i = 1; i < g.actors.size() && best < 0; i++) {
        const Actor& a = g.actors[i];
        if (!a.npc || !a.human || a.comp || a.resident < 0 || a.site < 0 || a.role == Role::Innkeeper || a.role == Role::Guard) continue;
        g.life.befriend(g.world.sites[(size_t)a.site].id, a.resident);
        if (g.partyOffer(a).ok) best = (int)i;
      }
    if (best < 0) { c.fail("voice regular: nobody here would travel with you"); return true; }
    talkWith(g, best);
    if (g.mode != Mode::Dialogue) c.fail("voice regular: no conversation opened");
    return true;
  }
  if (what == "quest") {
    const int m = memberArg(c, 2);
    if (m < 0) return true;
    party::Member& M = g.party.members[(size_t)m];
    M.approval = std::max<int8_t>(M.approval, 60);
    M.bond = std::max<uint8_t>(M.bond, 40);
    if (M.stake.hook == party::Hook::None) M.stake.hook = party::Hook::Ambition;
    M.flags = (uint16_t)(M.flags & ~party::MF_QUEST_DONE);
    if (party::voice::questFor(g, M).empty()) c.fail("voice quest: no personal quest can be told for " + M.name);
    return true;
  }
  if (what == "deeds") {
    const ew::Gid sid = g.curSite >= 0 && g.curSite < (int)g.world.sites.size() ? g.world.sites[(size_t)g.curSite].id : 0;
    g.deed(party::Deed::Mercy, sid);
    g.deed(party::Deed::ClearedBandits, sid);
    g.deed(party::Deed::Stole, sid);
    return true;
  }
  if (what == "banter") {
    std::string t = lw(c.arg(2));
    for (char& ch : t) if (ch == '-') ch = ' ';
    const int m = memberArg(c, 3);
    if (m < 0) return true;
    int trig = -1;
    for (int k = 0; k < (int)party::voice::Trig::COUNT; k++) if (lw(party::voice::trigName((party::voice::Trig)k)) == t) trig = k;
    if (trig < 0) { c.fail("voice banter: unknown trigger " + c.arg(2)); return true; }
    const party::Member& M = g.party.members[(size_t)m];
    uint64_t arg = 0;
    std::string name;
    if ((party::voice::Trig)trig == party::voice::Trig::Border)
      for (const Site& S : g.world.sites) if (S.culture && cult::familyOf(S.culture) != cult::familyOf(M.culture)) { arg = S.culture; break; }
    if ((party::voice::Trig)trig == party::voice::Trig::War) {
      const ew::Gid hl = party::voice::bio(g, M).homelandId;
      for (const realm::KingdomState& K : g.realm.kingdoms()) if (K.id != hl && !K.fallen) { arg = K.id; break; }
    }
    if ((party::voice::Trig)trig == party::voice::Trig::Wonder) name = "THE SINGING FALLS";
    if ((party::voice::Trig)trig == party::voice::Trig::Hound) name = "BRAN";
    std::string l;
    for (uint32_t salt = 0; salt < 8 && l.empty(); salt++) l = party::voice::banterLine(g, m, (party::voice::Trig)trig, arg, name, salt * 977u);
    if (l.empty()) { c.fail("voice banter: nothing to say for " + c.arg(2)); return true; }
    g.partySay(m, l, 6.0f);
    printf("voice banter: %s: %s\n", M.name.c_str(), l.c_str());
    return true;
  }
  if (what == "kin") {
    // (`voice kin fallen`: the household of the last companion to die)
    party::Member M;
    if (lw(c.arg(2)) == "fallen") {
      if (g.party.fallen.empty()) { c.fail("voice kin fallen: nobody has fallen"); return true; }
      const party::Fallen& f = g.party.fallen.back();
      M.name = f.name; M.npc = f.npc; M.site = f.site; M.culture = f.culture;
      if (const life::Census* fc = g.life.find(f.site))
        for (const life::Resident& r : fc->res) if (life::npcId(f.site, r.idx) == f.npc) { M.res = (int16_t)r.idx; M.female = r.female; }
    } else {
      const int m = memberArg(c, 2);
      if (m < 0) return true;
      M = g.party.members[(size_t)m];
    }
    const party::voice::Bio b = party::voice::bio(g, M);
    const int si = M.site ? g.world.siteHandle(M.site) : -1;
    life::Census* C = si >= 0 ? g.life.census(g.world, si) : nullptr;
    if (!C) { c.fail("voice kin: no census for " + M.name); return true; }
    for (const party::voice::Kin& k : b.household) {
      if (k.dead || k.away || k.idx < 0) continue;
      int ai = -1;
      for (size_t i = 1; i < g.actors.size(); i++) if (g.actors[i].resident == k.idx && g.actors[i].site == si && !g.actors[i].comp) ai = (int)i;
      if (ai < 0 && story::host().spawnHuman && story::host().findActor) {   // (their body brought beside the player)
        Spawn sp;
        sp.npc = true;
        sp.role = life::jobRole(C->res[(size_t)k.idx].job);
        sp.site = si;
        sp.slot = 3000 + k.idx;
        const int aid = story::host().spawnHuman(g, sp, g.pl().p.x + 14.0f, g.pl().p.y);
        ai = story::host().findActor(g, aid);
        if (ai > 0) {
          Actor& a = g.actors[(size_t)ai];
          a.site = si; a.bldg = -1; a.resident = k.idx; a.fromMap = false;
          g.dressAsResident(a, M.site, k.idx);
          C->res[(size_t)k.idx].actor = aid;
        }
      }
      if (ai < 0) { c.fail("voice kin: their kin cannot be brought here"); return true; }
      talkWith(g, ai);
      if (g.mode != Mode::Dialogue) c.fail("voice kin: no conversation opened");
      return true;
    }
    c.fail("voice kin: " + M.name + " has no kin at home");
    return true;
  }
  if (what == "grave") {
    if (g.party.fallen.empty()) { c.fail("voice grave: nobody has fallen"); return true; }
    const int i = c.arg(2).empty() ? (int)g.party.fallen.size() - 1 : std::atoi(c.arg(2).c_str());
    if (i < 0 || i >= (int)g.party.fallen.size()) { c.fail("voice grave: no grave " + c.arg(2)); return true; }
    PartyTalkOps::graveTalk(g, i);
    if (g.mode != Mode::Dialogue) c.fail("voice grave: no conversation opened");
    return true;
  }
  if (what == "recruit-kin") {   // a census adult of the settlement here (else the start village) with kin at home joins
    int si = g.curSite >= 0 ? g.curSite : g.world.startSite;
    life::Census* C = si >= 0 ? g.life.census(g.world, si) : nullptr;
    if (!C) { c.fail("voice recruit-kin: no census here"); return true; }
    for (size_t ri = 0; ri < C->res.size(); ri++) {
      const life::Resident r = C->res[ri];
      if ((r.flags & (life::RF_DEAD | life::RF_KEY | life::RF_AWAY | life::RF_COMPANION)) || r.age < 18 || r.age > 60 || r.job == life::Job::Child) continue;
      bool kin = false;
      for (const life::Resident& o : C->res)
        if (o.idx != r.idx && o.household == r.household && !(o.flags & (life::RF_DEAD | life::RF_AWAY))) kin = true;
      if (!kin) continue;
      Actor a;
      a.id = -96; a.npc = true; a.human = true; a.site = si; a.resident = r.idx; a.p = g.pl().p + Vec2(16, 0); a.name = r.name;
      if (g.partyRecruit(a, party::Origin::Tavern) >= 0) return true;
      C = g.life.census(g.world, si);
      if (!C) break;
    }
    c.fail("voice recruit-kin: nobody with kin here would come");
    return true;
  }
  if (what == "choose") {   // the n-th option on show (1 = the first): a pitch whose words a script cannot know
    const int n = std::atoi(c.arg(2).c_str());
    int k = 0;
    for (size_t i = 0; i < g.dlg.opts.size(); i++)
      if (g.dlg.shows(g.dlg.opts[i]) && ++k == n) { g.dialogueChoose((int)i); return true; }
    c.fail("voice choose: no option " + c.arg(2));
    return true;
  }
  c.fail("voice: unknown command " + what);
  return true;
}
EMB_SCRIPT_CMD("voice", "voice regular | quest [m] | deeds | banter <trigger> [m] | kin [m] | grave [i] | choose <n>: M8 companions' words staged", cmdVoice);

bool expVoice(ScriptCtx& c) {
  Game& g = c.game;
  const std::string what = lw(c.arg(2));
  const std::string words = c.rest(3);
  if (what == "banter") { if (g.party.lastBanter.find(words) == std::string::npos) c.fail("expect voice banter: \"" + g.party.lastBanter + "\""); return true; }
  if (what == "text") { if (g.dlg.text.find(words) == std::string::npos) c.fail("expect voice text: \"" + g.dlg.text + "\""); return true; }
  c.fail("expect voice: banter|text <words>");
  return true;
}
EMB_SCRIPT_CMD("expect:voice", "expect voice banter|text <words>: the last banter line / the open dialogue's line holds the words", expVoice);

}  // namespace
