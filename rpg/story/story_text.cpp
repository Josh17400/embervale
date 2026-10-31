// SOLID 1.3 STORY: what the people of a story say: talksTo, the dialogue (showDialogue, choose), and the line and
// word helpers (introductions, hand-offs, the next step, replies, hook lines). Moved from story.cpp (the engine);
// the effects are story_effects.cpp. Shared helpers: story_detail.h.
#include "rpg/story/story.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "engine/sound_ids.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/craft.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/life.h"
#include "rpg/sim/party.h"
#include "rpg/sim/party_voice.h"
#include "rpg/sim/talk.h"
#include "rpg/story/saga.h"
#include "rpg/sim/common.h"
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/story/radiant.h"
#include "rpg/story/story_internal.h"
#include "rpg/story/story_detail.h"
#include "rpg/sim/home.h"
#include "rpg/world/economy.h"
#include "rpg/world/source.h"

namespace story {

// ---------------------------------------------------------------- dialogue
bool Engine::talksTo(const Game& g, const Instance& in, const Actor& a) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done || !a.npc) return false;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  if (G.kind != StageKind::Talk) return false;
  const int ri = s->role(G.talk);
  const Binding* b = bindingOf(in, G.talk);
  if (!b || ri < 0) return false;
  switch ((RoleKind)b->kind) {
    case RoleKind::Giver:
      // (M8) a companion's own story: the companion, wherever they are
      if (a.comp) return (int)a.comp - 1 < (int)g.party.members.size() && party::companionKey(g.party.members[(size_t)(a.comp - 1)].id) == b->id;
      return !isStoryPerson(a) && g.npcKey(a) == b->id;
    case RoleKind::Npc: {
      if ((int)a.role != (int)b->trade || isStoryPerson(a)) return false;
      const int h = homeSiteOf(g, a);
      return h >= 0 && g.world.sites[(size_t)h].id == b->site;
    }
    case RoleKind::Person: return isStoryPerson(a) && a.slot == personSlot(in.id, ri) && a.name == b->name;
    case RoleKind::Resident: {
      // (M6b) the census resident's own body in the town, or the stand-in syncPersons sent while they are out of play
      if (isStoryPerson(a)) return a.slot == personSlot(in.id, ri) && a.name == b->name;
      if (b->trade != RESIDENT_CENSUS) return false;
      ew::Gid rs = 0;
      int ix = -1;
      return residentOfActor(g, a, rs, ix) && rs == b->site && life::npcId(rs, ix) == b->id;
    }
    case RoleKind::Ruler: {
      if (a.role != Role::King) return false;
      const int h = homeSiteOf(g, a);
      if (h < 0) return false;
      const Site& S = g.world.sites[(size_t)h];
      return S.kingdom >= 0 && g.world.kingdoms[(size_t)S.kingdom].id == b->id;
    }
    default: return false;
  }
}

// ---------------------------------------------------------------- (M7b) how the people of a story speak
// The phone panel holds one line at a time (rpg/sim/talk.h): who is speaking, who they talk of, what happened, what is
// wanted, and where to go next must all be said in the speakers' own words. These helpers write the words the engine
// adds around a stage's own line: who a speaker is when the player has not met them, who the next person is and where,
// and the way to the next place. Every addition is made only when the line still fits the panel.
namespace {

bool hasWord(const std::string& text, const std::string& w) {
  for (size_t p = text.find(w); p != std::string::npos; p = text.find(w, p + 1)) {
    const bool l = p == 0 || !(std::isalnum((unsigned char)text[p - 1]) || text[p - 1] == '\'');
    const size_t e = p + w.size();
    const bool r = e >= text.size() || !(std::isalnum((unsigned char)text[e]) || text[e] == '\'');
    if (l && r) return true;
  }
  return false;
}

// how far a global tile offset is, as a traveller says it
std::string farWords(int64_t dx, int64_t dy) {
  const double d = std::sqrt((double)(dx * dx + dy * dy));
  if (d < 70) return "JUST";
  if (d < 260) return "A SHORT WALK";
  if (d < 700) return "A FAIR WALK";
  if (d < 1500) return "A LONG WALK";
  return "MANY DAYS ON THE ROAD";
}

// the name of the place a binding is in or is ("" none) and its tile
bool placeOfBinding(const Game& g, const Binding& b, std::string& name, int32_t& gx, int32_t& gy, ew::Gid& id) {
  const bool isPlace = b.kind == (uint8_t)RoleKind::Site || b.kind == (uint8_t)RoleKind::Capital || b.kind == (uint8_t)RoleKind::Ruin;
  id = isPlace ? (b.site ? b.site : b.id) : b.site;
  if (!id || !b.hasPos) return false;
  const int h = g.world.siteHandle(id);
  if (isPlace) name = b.name;
  else if (h >= 0 && h < (int)g.world.sites.size()) name = g.world.sites[(size_t)h].name;
  gx = b.gx;
  gy = b.gy;
  if (h >= 0 && h < (int)g.world.sites.size() && !isPlace) { gx = g.world.ox + g.world.sites[(size_t)h].ex; gy = g.world.oy + g.world.sites[(size_t)h].ey; }
  return !name.empty();
}

// is the player at (or in) that place now?
bool playerAt(const Game& g, ew::Gid id, int32_t gx, int32_t gy) {
  if (g.curSite >= 0 && g.curSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.curSite].id == id) return true;
  if (g.inside && g.subSite >= 0 && g.subSite < (int)g.world.sites.size() && g.world.sites[(size_t)g.subSite].id == id) return true;
  int32_t px, py;
  playerGlobal(g, px, py);
  const int64_t dx = px - gx, dy = py - gy;
  return dx * dx + dy * dy < 40 * 40;
}

// "VOND, A SHORT WALK TO THE NORTH-EAST" (a place), or "HERE IN VOND" when the player stands in it
std::string placeWords(const Game& g, const std::string& name, ew::Gid id, int32_t gx, int32_t gy, bool& here) {
  here = playerAt(g, id, gx, gy);
  if (here) return name;
  int32_t px, py;
  playerGlobal(g, px, py);
  const std::string far = farWords(gx - px, gy - py);
  return name + ", " + far + (far == "JUST" ? " " : " TO THE ") + dirWord(gx - px, gy - py, false);
}

// "VOND IS A SHORT WALK TO THE NORTH-EAST." ("" when the player stands in it)
std::string placeSentence(const Game& g, const std::string& name, ew::Gid id, int32_t gx, int32_t gy) {
  if (playerAt(g, id, gx, gy)) return "";
  int32_t px, py;
  playerGlobal(g, px, py);
  const std::string far = farWords(gx - px, gy - py);
  return name + " IS " + far + (far == "JUST" ? " " : " TO THE ") + dirWord(gx - px, gy - py, false) + ".";
}

// a name that is no one's own ("GUARD", "THE SMITH"): nobody introduces themselves by it
bool properName(const Binding& b) {
  if (b.name.empty() || b.name.compare(0, 4, "THE ") == 0) return false;
  for (int r = 0; r < (int)Role::COUNT; r++) if (b.name == dsl::tradeName(r)) return false;
  return true;
}

const char* pron(const Binding& b, int which) {   // 0 HE, 1 HIM, 2 HIS
  static const char* const m[3] = {"HE", "HIM", "HIS"};
  static const char* const f[3] = {"SHE", "HER", "HER"};
  return b.female ? f[which] : m[which];
}

// the trade a speaker introduces themselves by ("" none worth saying: a villager says where they live instead)
const char* tradeSelf(int role) {
  switch ((Role)role) {
    case Role::Innkeeper: return "I KEEP THE INN";
    case Role::Smith: return "I WORK THE FORGE";
    case Role::Merchant: return "I KEEP A STALL";
    case Role::Priest: return "I SERVE THE TEMPLE";
    case Role::Guard: return "I STAND WATCH";
    case Role::Farmer: return "I FARM";
    case Role::Hunter: return "I HUNT";
    case Role::Fisher: return "I FISH";
    case Role::Herbalist: return "I GATHER HERBS";
    case Role::Mage: return "I STUDY THE ARTS";
    case Role::Traveller: return "";
    default: return "";
  }
}

// the resident's word parts ("JOB|KIN|LOST")
std::string residentPart(const Binding& b, int k) {
  std::string part[3];
  size_t p0 = 0;
  for (int k2 = 0; k2 < 3; k2++) {
    const size_t bar = b.extra.find('|', p0);
    part[k2] = b.extra.substr(p0, bar == std::string::npos ? std::string::npos : bar - p0);
    if (bar == std::string::npos) break;
    p0 = bar + 1;
  }
  return part[k];
}

// does the line already say who the speaker is?
bool saysOwnName(const std::string& text, const std::string& name) {
  return hasWord(text, "I'M " + name) || hasWord(text, "I AM " + name) || text.find("MY NAME") != std::string::npos ||
         hasWord(text, "CALL ME " + name) || hasWord(text, "NAME'S " + name);
}

// (fixer M7b r3) does a teller's hint say they are not from here ("I'M NOT FROM HERE", "FROM FURTHER OFF")? Their
// introduction then leaves out the town ("I'M GRULLA, OF NARDA. I'M FROM FURTHER OFF..." read as a contradiction)
bool hintDisownsHome(const std::string& hint) {
  static const char* const k[] = {"NOT FROM HERE", "FROM FURTHER OFF", "FROM SOMEWHERE ELSE", "ISN'T MY HOME", "NOT MY HOME", "I'M A STRANGER HERE",
                                  "I'M AN EXILE", "AN EXILE HERE", "NOT FROM AROUND"};
  for (const char* x : k) if (hint.find(x) != std::string::npos) return true;
  return false;
}

// a name a person can introduce themselves by: not a bare trade ("GUARD", "ROYAL GUARD", "THE HERDER")
bool selfNamed(const std::string& name) {
  if (name.empty() || name.compare(0, 4, "THE ") == 0 || name.find("GUARD") != std::string::npos) return false;
  for (int r = 0; r < (int)Role::COUNT; r++) if (name == dsl::tradeName(r)) return false;
  return true;
}

// who the speaker of role `role` is, in their own words, for a player who has not met them ("" nothing to add)
std::string introOf(const Game& g, const Instance& in, const dsl::Script& s, const std::string& role) {
  const Binding* b = bindingOf(in, role);
  const int ri = s.role(role);
  if (!b || ri < 0 || b->name.empty()) return "";
  const dsl::RoleDef& R = s.roles[(size_t)ri];
  if (!properName(*b) || !selfNamed(b->name)) return "";
  const Binding* home = bindingOf(in, "home");
  const std::string town = home && home->kind == (uint8_t)RoleKind::Site && !hintDisownsHome(s.hint) ? home->name : std::string();
  switch (R.kind) {
    case RoleKind::Giver: {
      if (s.hook != dsl::HookKind::Npc && s.hook != dsl::HookKind::Herald) return "";
      // (fixer M7b r3) the watch, a soldier or a bandit has no name of their own to give ("I'M ROYAL GUARD.")
      if (b->trade == (uint8_t)Role::Guard || b->trade == (uint8_t)Role::Soldier || b->trade == (uint8_t)Role::Bandit) return "";
      if (b->trade == (uint8_t)Role::Herald) return "I AM " + b->name + ", HERALD OF THE CROWN.";
      const std::string t = tradeSelf(b->trade);
      if (!t.empty()) return "I'M " + b->name + ". " + t + (town.empty() ? std::string(".") : " HERE IN " + town + ".");
      return "I'M " + b->name + (town.empty() ? std::string(".") : ", OF " + town + ".");
    }
    case RoleKind::Resident: {
      const std::string kin = residentPart(*b, 1);
      const Binding* of = R.b.empty() ? bindingOf(in, "giver") : bindingOf(in, R.b);
      if (of && !kin.empty() && kin != "NEIGHBOUR" && of->name != b->name && of->kind != (uint8_t)RoleKind::Site)
        return "I'M " + b->name + ", " + of->name + "'S " + kin + ".";
      const std::string job = residentPart(*b, 0);
      if (!job.empty() && job != "VILLAGER") return "I'M " + b->name + ", THE " + job + ".";
      return "I'M " + b->name + ".";
    }
    // a person or foe the player seeks out was named to them already (the journal, the teller); a stranger who
    // seeks the player out says what they want first, and the panel's title names them. An npc is named by their
    // trade, a ruler by their title.
    default: return "";
  }
}

std::string withIntro(const std::string& intro, const std::string& text, const std::string& name) {
  if (intro.empty() || saysOwnName(text, name)) return text;
  const std::string t = text.empty() ? intro : intro + " " + text;
  return speech::fitsPanel(t) ? t : text;
}

// who a person is, at their first mention by someone else ("ARNA THE HERBALIST", "A WOMAN CALLED ARNA")
std::string whoWords(const Instance& in, const dsl::Script& s, const Binding& b) {
  const int ri = s.role(b.role);
  const RoleKind k = ri >= 0 ? s.roles[(size_t)ri].kind : (RoleKind)b.kind;
  if (k == RoleKind::Npc || k == RoleKind::Ruler) return b.name;
  if (k == RoleKind::Resident) {
    const std::string job = residentPart(b, 0);
    if (!job.empty() && job != "VILLAGER") return b.name + " THE " + job;
  }
  if (k == RoleKind::Giver) {
    const char* tn = dsl::tradeName(b.trade);
    if (tn && *tn && b.trade != (uint8_t)Role::Villager) return b.name + " THE " + tn;
  }
  (void)in;
  return std::string("A ") + (b.female ? "WOMAN" : "MAN") + " CALLED " + b.name;
}

uint32_t lineSalt(const Instance& in, int stage) { return hash32(in.id * 2654435761u ^ (uint32_t)stage * 40503u ^ (uint32_t)in.script.size()); }

}  // namespace

bool Engine::optionShown(const Game& g, const Instance& in, int opt) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return false;
  const auto& O = s->stages[(size_t)in.stage].opts;
  if (opt < 0 || opt >= (int)O.size()) return false;
  for (const dsl::CondDef& c : O[(size_t)opt].ifs) if (!evalCond(g, in, c)) return false;
  return true;
}

bool Engine::showDialogue(Game& g, Instance& in) {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.done) return false;
  const dsl::StageDef& G = s->stages[(size_t)in.stage];
  if (G.kind != StageKind::Talk) return false;
  // (M7b) the speaker says who they are first when the player has not met them (speech::Memory)
  const int ai = host().findActor ? host().findActor(g, g.dlg.actor) : -1;
  const uint64_t key = ai >= 0 && ai < (int)g.actors.size() && g.actors[(size_t)ai].human ? g.npcKey(g.actors[(size_t)ai]) : 0;
  const bool introduce = key && !g.talkMem.introduced(key);
  g.dlg.text = talkLine(g, in, introduce);
  if (introduce && (hasWord(g.dlg.text, "I'M") || hasWord(g.dlg.text, "I AM") || g.dlg.text.find("MY NAME") != std::string::npos || hasWord(g.dlg.text, "CALL ME")))
    g.talkMem.flag(key, speech::TM_INTRODUCED, g.day);
  // (fixer M7b r1, review: "a saga beat spoken by an unnamed THE VILLAGER") someone the story sent the player to by name
  // (the journal and the tracker say "SPEAK WITH TANBATUR") is known to the player: the panel names them
  if (introduce && G.talk != "giver" && ai >= 0) {
    const Binding* tb = bindingOf(in, G.talk);
    if (tb && !tb->name.empty() && hasWord(g.actors[(size_t)ai].name, tb->name)) g.talkMem.flag(key, speech::TM_INTRODUCED, g.day);
  }
  // the story's line holds the panel with its options (a later hook's lower-priority line never replaces it)
  g.dlg.pri = std::max(g.dlg.pri, (int)speech::P_STORY);
  g.dlg.topic = (uint8_t)speech::Topic::Story;
  std::vector<DlgOpt> mine;
  for (size_t i = 0; i < G.opts.size(); i++) {
    const dsl::OptDef& o = G.opts[i];
    bool ok = true;
    for (const dsl::CondDef& c : o.ifs) if (!evalCond(g, in, c)) ok = false;
    if (!ok) continue;
    mine.push_back({fillText(g, in, o.label), DLG_STORY + SA_OPT + (int)i, (int)in.id});
  }
  // the story's choices come first; the person's usual business stays below them
  std::vector<DlgOpt> rest;
  for (const DlgOpt& o : g.dlg.opts) if (!(o.action >= DLG_STORY && o.action < DLG_WAR)) rest.push_back(o);
  g.dlg.opts = mine;
  for (const DlgOpt& o : rest) g.dlg.opts.push_back(o);
  return true;
}

bool Engine::choose(Game& g, uint32_t instId, int opt, bool forceIf, int forceCheck) {
  Instance* in = find(instId);
  if (!in || in->done) return false;
  const dsl::Script* s = scriptOf(*in);
  if (!s) return false;
  const dsl::StageDef& G = s->stages[(size_t)in->stage];
  if (G.kind != StageKind::Talk || opt < 0 || opt >= (int)G.opts.size()) return false;
  const dsl::OptDef& o = G.opts[(size_t)opt];
  if (!forceIf) for (const dsl::CondDef& c : o.ifs) if (!evalCond(g, *in, c)) return false;
  int to = o.toIx;
  if (o.check) {
    const bool pass = forceCheck == 1 || (forceCheck == 0 && evalCond(g, *in, o.chk));
    to = pass ? o.toIx : o.elseIx;
  }
  const std::string who = G.talk;
  // what the player just heard, then (after a line break) what they answered: a name the speaker said needs no
  // introduction; one the player named may be sent to
  const std::string sayNow = fillText(g, *in, G.say);
  if (said_.size() > 4000) said_.clear();
  said_ += sayNow + " ";
  const std::string heard = said_ + "\n" + fillText(g, *in, o.label);
  {
    // (fixer M6b r1) the journal context of a hand-off: what this speaker told the player (their first sentence or
    // two) and what the player answered
    handCtx_.clear();
    const Binding* sb = bindingOf(*in, who);
    std::string said = fillText(g, *in, G.say);
    size_t end = std::string::npos;
    for (size_t k = 0; k < said.size(); k++)
      if ((said[k] == '.' || said[k] == '?' || said[k] == '!') && k >= 40 && (k + 1 >= said.size() || said[k + 1] == ' ')) { end = k + 1; break; }
    if (end != std::string::npos) said = said.substr(0, end);
    if (said.size() > 170) {
      size_t sp = said.rfind(' ', 166);
      said = said.substr(0, sp == std::string::npos ? 166 : sp) + "...";
    }
    if (sb && !said.empty()) handCtx_ = sb->name + " SAID: \"" + said + "\" YOU ANSWERED: \"" + fillText(g, *in, o.label) + "\"";
  }
  choosing_ = true;
  enterStage(g, *in, to);
  choosing_ = false;
  handCtx_.clear();
  in = find(instId);
  if (!in) return true;
  if (g.mode != Mode::Dialogue) return true;
  const dsl::StageDef& T = s->stages[(size_t)in->stage];
  const DlgOpt bye{"FAREWELL.", 0, 0};   // (A_BYE == 0: game_internal.h DlgAct)
  // (fixer M6b r1) the next talk is still the one in front of the player (the same role, or another role the same
  // person plays): it goes on here
  bool samePerson = T.kind == StageKind::Talk && T.talk == who;
  if (T.kind == StageKind::Talk && !samePerson && !in->done) {
    const int ai = host().findActor ? host().findActor(g, g.dlg.actor) : -1;
    samePerson = ai >= 0 && ai < (int)g.actors.size() && talksTo(g, *in, g.actors[(size_t)ai]);
  }
  std::string text;
  const Reply r = replyAfter(g, *in, who, samePerson, heard, G.name, text);
  if (r == Reply::Continue) {
    if (!samePerson && s->hook == dsl::HookKind::Companion) {   // (M8) the companion takes up the talk
      const int mi = companionOf(g, *in);
      if (mi >= 0 && g.party.members[(size_t)mi].actor >= 0) {
        g.dlg.actor = g.party.members[(size_t)mi].actor;
        g.dlg.speaker = g.party.members[(size_t)mi].name;
      }
    }
    g.dlg.opts.clear();
    showDialogue(g, *in);
    g.dlg.opts.push_back(bye);
    return true;
  }
  if (r == Reply::Close) {
    g.dlg.text = text;
    g.dlg.opts = {bye};
    // (M7b) an ending told as a story is not put in the speaker's mouth: the panel names the tale instead
    const Binding* sb = bindingOf(*in, who);
    if (T.kind == StageKind::End && narration(text, sb ? sb->name : std::string())) g.dlg.speaker = s->title;
    return true;
  }
  g.mode = Mode::Play;
  return true;
}

std::string Engine::handOffLine(const Game& g, const Instance& in, const std::string& role, const std::string& heard, bool spoken) const {
  const dsl::Script* s = scriptOf(in);
  const Binding* b = bindingOf(in, role);
  if (!s || !b || b->name.empty()) return "";
  const uint32_t h = lineSalt(in, in.stage);
  // named already in what the player just heard (or the one who began it all): the name alone; else who they are
  const bool named = hasWord(heard.substr(0, heard.find('\n')), b->name) || role == "giver";
  // the player's answer was a question
  const size_t nl = heard.find('\n');
  const std::string answer = nl == std::string::npos ? std::string() : heard.substr(nl + 1);
  const bool asked = !answer.empty() && answer.back() == '?';
  const std::string who = named ? b->name : whoWords(in, *s, *b);
  std::string place;
  int32_t px = 0, py = 0;
  ew::Gid pid = 0;
  bool here = false;
  std::string where;
  if (placeOfBinding(g, *b, place, px, py, pid)) {
    const std::string pw = placeWords(g, place, pid, px, py, here);
    where = here ? "HERE IN " + place : "IN " + pw;
    // (fixer M7b r2, review: "IN DAR QUR, A SHORT WALK..." twice in one turn) the way there just told is not told again
    if (!here && heard.find(pw) != std::string::npos) where.clear();
    // (M8 VOICE: "HERE IN BAB MINZI" said twice in one talk, the speaker's own intro and the hand-off) once is enough
    if (here && heard.find("HERE IN " + place) != std::string::npos) where = "HERE";
  }
  std::string t;
  if (!spoken) t = where.empty() ? "THE NOTICE ASKS YOU TO FIND {W}." : "THE NOTICE ASKS YOU TO FIND {W}, {WHERE}.";
  else if (role == "giver") t = where.empty() || here ? "THEN GO BACK TO {W}. {HE} WILL WANT TO HEAR IT FROM YOU." : "THEN GO BACK TO {W}, {WHERE}.";
  else if (asked) {
    // (the player asked something this speaker cannot answer themselves: they say who can)
    t = where.empty() ? "{W} CAN TELL YOU MORE THAN I CAN." : "{W} CAN TELL YOU MORE THAN I CAN. YOU'LL FIND {HIM} {WHERE}.";
  } else if (!answer.empty()) {
    // (fixer M7b r1, review: "the giver's reply ignores the option the player picked": "LET HER GO." was answered only
    // "SPEAK WITH BASAALAI NEXT") the choice is taken up first, then the next person is given with the reason to go
    static const char* const ack[3] = {"SO BE IT.", "VERY WELL. IT IS YOUR CHOICE TO MAKE.", "IF THAT IS YOUR WORD, I WILL STAND BY IT."};
    static const char* const k[3] = {"{W} MUST HEAR IT FROM YOU.", "THEN GO AND TELL {W} YOURSELF.", "{W} IS THE ONE TO SEE ABOUT IT NOW."};
    t = std::string(ack[h % 3]) + " " + k[(h / 3) % 3] + (where.empty() ? "" : " YOU'LL FIND {HIM} {WHERE}.");
  } else if (where.empty()) {
    static const char* const k[3] = {"YOU SHOULD SPEAK WITH {W} NEXT.", "{W} IS THE ONE TO SEE NOW.", "GO AND FIND {W}. TELL {HIM} I SENT YOU."};
    t = k[h % 3];
  } else {
    static const char* const k[3] = {"GO AND SEE {W}, AND TELL {HIM} I SENT YOU. YOU'LL FIND {HIM} {WHERE}.", "THEN YOU NEED {W}. YOU'LL FIND {HIM} {WHERE}.",
                                     "SPEAK WITH {W} NEXT. YOU'LL FIND {HIM} {WHERE}."};
    t = k[h % 3];
  }
  auto sub = [&](const std::string& a, const std::string& v) { for (size_t p = t.find(a); p != std::string::npos; p = t.find(a, p + v.size())) t.replace(p, a.size(), v); };
  sub("{WHERE}", where);
  sub("{W}", who);
  sub("{HIM}", pron(*b, 1));
  sub("{HE}", pron(*b, 0));
  return upper(t);
}

std::string Engine::nextStepLine(const Game& g, const Instance& in, const std::string& heard, bool alone) const {
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return "";
  const dsl::StageDef& T = s->stages[(size_t)in.stage];
  if (T.kind != StageKind::Goal) return "";
  const dsl::ObjDef& O = T.goal;
  const uint32_t h = lineSalt(in, in.stage);
  std::string target;
  switch (O.type) {
    case ObjType::Goto: case ObjType::Enter: case ObjType::Kill: case ObjType::Fetch: case ObjType::Use: case ObjType::Famine:
      target = O.role.empty() ? O.role2 : O.role;
      if (O.type == ObjType::Kill || O.type == ObjType::Use) target = O.role;
      break;
    case ObjType::Slay: target = O.role; break;
    case ObjType::Wait: {
      if (!alone) return "";
      static const char* const k[2] = {"GIVE IT {N}. THEN COME BACK TO ME.", "WAIT {N}, NO LESS. THEN COME AND FIND ME."};
      std::string t = k[h % 2];
      const std::string n = O.n == 1 ? "A DAY" : O.n <= 10 ? std::string(O.n == 2 ? "TWO" : O.n == 3 ? "THREE" : O.n == 4 ? "FOUR" : O.n == 5 ? "FIVE" : O.n == 6 ? "SIX" : O.n == 7 ? "SEVEN" : O.n == 8 ? "EIGHT" : O.n == 9 ? "NINE" : "TEN") + " DAYS" : std::to_string(O.n) + " DAYS";
      t.replace(t.find("{N}"), 3, n);
      return t;
    }
    default: return "";
  }
  const Binding* b = bindingOf(in, target);
  if (!b) return "";
  std::string place;
  int32_t px = 0, py = 0;
  ew::Gid pid = 0;
  if (!placeOfBinding(g, *b, place, px, py, pid)) return "";
  bool here = false;
  const std::string pw = placeWords(g, place, pid, px, py, here);
  if (here) return "";   // (right here: the errand needs no road)
  const bool isPlace = b->kind == (uint8_t)RoleKind::Site || b->kind == (uint8_t)RoleKind::Capital || b->kind == (uint8_t)RoleKind::Ruin;
  std::string t;
  if (isPlace || O.type != ObjType::Slay || hasWord(heard, place)) t = placeSentence(g, place, pid, px, py);
  else t = "YOU'LL FIND " + (hasWord(heard, b->name) ? b->name : whoWords(in, *s, *b)) + " AT " + pw + ".";
  if (t.empty()) return "";
  // a question is answered with the way itself; a promise is thanked first
  const size_t nl = heard.find('\n');
  const bool asked = nl != std::string::npos && nl + 1 < heard.size() && heard.back() == '?';
  if (alone && !asked) {
    static const char* const ack[3] = {"THANK YOU.", "GOOD.", "THEN GO WELL."};
    t = std::string(ack[h % 3]) + " " + t;
  }
  return upper(t);
}

bool Engine::narration(const std::string& text, const std::string& speaker) {
  // speech has a first person; an ending told as a story ("THE PEDDLER TOOK THE COMB IN BOTH HANDS...") has none, or
  // names its own teller as "him" would
  static const char* const first[] = {"I", "I'M", "I'LL", "I'D", "I'VE", "ME", "MY", "MINE", "MYSELF", "WE", "US", "OUR", "WE'LL", "WE'RE",
                                      "WE'VE", "LET'S", "OURS"};
  bool firstPerson = false;
  for (const char* w : first) if (hasWord(text, w)) { firstPerson = true; break; }
  if (!firstPerson) return true;
  return !speaker.empty() && hasWord(text, speaker) && !saysOwnName(text, speaker);
}

namespace {
// the part of a composed story a stage belongs to: "t1_" (a twist slot), "a3_" (a campaign arc), "" (the archetype's own)
std::string partOf(const std::string& stage) {
  if (stage.size() > 3 && (stage[0] == 't' || stage[0] == 'a') && stage[1] >= '0' && stage[1] <= '9' && stage[2] == '_') return stage.substr(0, 3);
  return std::string();
}
}  // namespace

Engine::Reply Engine::replyAfter(const Game& g, const Instance& in, const std::string& who, bool samePerson, const std::string& heard,
                                 const std::string& fromStage, std::string& text) const {
  (void)who;
  text.clear();
  const dsl::Script* s = scriptOf(in);
  if (!s || in.stage < 0 || in.stage >= (int)s->stages.size()) return Reply::Silent;
  const dsl::StageDef& T = s->stages[(size_t)in.stage];
  // (M8) a companion's own story: when the companion speaks next, they are at the player's side and speak at once
  // (Engine::choose hands the dialogue to them), whoever spoke before
  if (s->hook == dsl::HookKind::Companion && T.kind == StageKind::Talk && !in.done && !samePerson && !s->roles.empty() && T.talk == s->roles[0].name)
    return Reply::Continue;
  if (samePerson && !in.done) return Reply::Continue;
  // (fixer M6b r1) a talk stage that belongs to someone else is never read out here: its words are theirs (the
  // player hears them from that person); only a goal's or an ending's lines close this conversation
  const bool samePart = partOf(fromStage) == partOf(T.name);
  const Binding* sp = bindingOf(in, who);
  const bool told = T.kind == StageKind::End && narration(fillText(g, in, T.say), sp ? sp->name : std::string());
  if (!T.say.empty() && T.kind != StageKind::Talk && (samePart || told)) {
    text = fillText(g, in, T.say);
    // (M7b) where to go next, when the line does not say it and the panel has room
    if (T.kind == StageKind::Goal) {
      const std::string step = nextStepLine(g, in, heard + " " + text, false);
      std::string place;
      int32_t px = 0, py = 0;
      ew::Gid pid = 0;
      const Binding* tb = bindingOf(in, T.goal.role.empty() ? T.goal.role2 : T.goal.role);
      const bool named = tb && placeOfBinding(g, *tb, place, px, py, pid) && hasWord(text, place);
      if (!step.empty() && !named && speech::fitsPanel(text + " " + step)) text += " " + step;
    }
    return Reply::Close;
  }
  if (T.kind == StageKind::Talk) {
    // (M7b) the story moves on to someone else: the speaker sends the player to them, by name and place, when the
    // speaker knows of them (named in what was just said, or the one who began it all). Someone the speaker never
    // spoke of (a twist's stranger who seeks the player out) is not theirs to send the player to: the talk ends and
    // the journal says who wants a word.
    const Binding* nb = bindingOf(in, T.talk);
    if (!nb || (T.talk != "giver" && !hasWord(heard, nb->name) && !samePart)) return Reply::Silent;
    text = handOffLine(g, in, T.talk, heard);
    return text.empty() ? Reply::Silent : Reply::Close;
  }
  // (M7b) a goal with nothing to say: the way to it, spoken
  text = nextStepLine(g, in, heard, true);
  return text.empty() ? Reply::Silent : Reply::Close;
}

std::string Engine::talkLine(const Game& g, const Instance& in, bool introduce) const {
  const std::string say = sayText(g, in);
  const dsl::Script* s = scriptOf(in);
  if (!introduce || !s || in.stage < 0 || in.stage >= (int)s->stages.size()) return say;
  const std::string& role = s->stages[(size_t)in.stage].talk;
  const Binding* b = bindingOf(in, role);
  return b ? withIntro(introOf(g, in, *s, role), say, b->name) : say;
}

std::string Engine::hookLine(const Game& g, const Instance& in, const dsl::Script& s, bool introduce) const {
  const std::string hint = fillText(g, in, s.hint);
  if (!introduce || in.cast.empty()) return hint;
  return withIntro(introOf(g, in, s, "giver"), hint, in.cast[0].name);
}

std::string Engine::hookLineFor(const Game& g, const dsl::Script& s, const Actor& a, const std::string& home, bool introduce) const {
  std::string hint = s.hint;
  if (hint.find('{') != std::string::npos) return hint;   // (never: hints name no role; the validator keeps them plain)
  if (!introduce) return hint;
  // (fixer M7b r3) a guard, soldier or bandit has no name of their own to give ("I'M GUARD. I STAND WATCH..."): the
  // hint alone, the panel's title says who speaks
  if (a.role == Role::Guard || a.role == Role::Soldier || a.role == Role::Bandit || !selfNamed(a.name)) return hint;
  const std::string t = tradeSelf((int)a.role);
  const std::string at = hintDisownsHome(hint) ? std::string() : home;
  const std::string intro = a.role == Role::Herald ? "I AM " + a.name + ", HERALD OF THE CROWN."
                            : !t.empty() ? "I'M " + a.name + ". " + t + (at.empty() ? std::string(".") : " HERE IN " + at + ".")
                                         : "I'M " + a.name + (at.empty() ? std::string(".") : ", OF " + at + ".");
  return withIntro(upper(intro), hint, a.name);
}

}  // namespace story
