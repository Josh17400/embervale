// M9 script commands (UI lane): drive and check the SKILLS and SPELLS pages and the quick-cast controls.
//   skillsui open skills|spells        open the menu on that page
//   skillsui skill <skill>             put that skill in focus (skill: its name without spaces, any case, or its enum
//                                      word: sword, heavyarmour, destruction ...)
//   skillsui perk <i>                  put perk i of the skill in focus on its tree
//   skillsui slot <1..4>               the spellbook's slot in focus
//   skillsui spell <spell>             the spellbook's spell in focus (its name without spaces: frostlance ...)
//   skillsui key <name>                a page key (enter, left, right, up, down, x, 1..4) straight to the open page
//   skillsui tap star <i> | take | skill <skill> | group <n> | slot <1..4> | spell <spell> | prepare | clear | quick <1..5>
//                                      a touch tap where that thing is drawn (the page's own layout; quick: the HUD's
//                                      quick-cast button, 5 the sneak toggle)
//   skillsui rested                    as after a night's rest (MagicOps::rested: the loadout may be changed)
//   skillsui give book <skill> | tome <spell>   a skill book / a spell tome at the top of the pack
//   expect skillsui page skills|spells
//   expect skillsui skill <skill> | perk <i> | slotsel <1..4>
//   expect skillsui slot <1..4> <spell>|empty      what the book has prepared there
//   expect skillsui why <words>        the last refusal shown on a page contains the words
//   expect skillsui toast <words>      the newest skill toast contains the words
//   expect skillsui ready <n>          at least n ready (pulsing) stars were drawn on the open tree
//   expect skillsui quick <n>          at least n filled quick-cast buttons were drawn (touch)
//   expect skillsui input press <1..4> [n] | hold <secs> | dual [n] | sneak [n] | release [n]
//                                      what the quick-cast input sent the game (presses of a slot, the longest hold,
//                                      dual-cast releases, sneak toggles, releases)
//   expect skillsui sneaking 0|1       Skills::live.sneaking
//   expect skillsui casts <n>          at least n spells cast (magic::Stats::casts)
#include <SDL3/SDL.h>
#include <cstdlib>
#include <string>
#include "rpg/sim/game.h"
#include "rpg/sim/magic.h"
#include "rpg/sim/magic_ops.h"
#include "rpg/sim/skills.h"
#include "rpg/view/script_api.h"
#include "rpg/view/skills_ui.h"

// the pages' layouts (skills_ui.cpp, spellbook_ui.cpp): menu box coordinates
bool skillsUiStarAt(bool touch, skills::Skill s, int idx, float& x, float& y);
void skillsUiTakeAt(bool touch, float& x, float& y);
bool skillsUiRowAt(bool touch, skills::Skill s, float& x, float& y);
void skillsUiGroupAt(bool touch, int grp, float& x, float& y);
void spellsUiSlotAt(bool touch, int slot, float& x, float& y);
bool spellsUiRowAt(const Game& g, bool touch, int scroll, uint32_t id, float& x, float& y);
void spellsUiButtonAt(bool touch, bool clear, float& x, float& y);

namespace {
std::string squashU(const std::string& s) {
  std::string r;
  for (char ch : s) {
    if (ch == ' ' || ch == '_' || ch == '-' || ch == '\'') continue;
    r += (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
  }
  return r;
}
bool skillOf(const std::string& w, skills::Skill& out) {
  static const char* enumWords[] = {"sword", "axe", "mace", "spear", "dagger", "bow", "unarmed", "block", "lightarmour", "mediumarmour",
                                    "heavyarmour", "heritage", "athletics", "smithing", "cooking", "farming", "riding", "speech", "stealth",
                                    "lockpicking", "destruction", "restoration", "conjuration", "illusion", "alteration"};
  const std::string q = squashU(w);
  for (int i = 0; i < skills::SKILLS; i++)
    if (q == squashU(skills::skillInfo((skills::Skill)i).name) || (i < (int)(sizeof(enumWords) / sizeof(enumWords[0])) && q == enumWords[i])) { out = (skills::Skill)i; return true; }
  return false;
}
uint32_t spellOf(const std::string& w) {
  const std::string q = squashU(w);
  for (const magic::SpellDef& s : magic::schoolSpells()) if (squashU(s.name) == q) return s.id;
  const unsigned long id = std::strtoul(w.c_str(), nullptr, 10);
  return magic::spellById((uint32_t)id) ? (uint32_t)id : 0;
}
int keyOf(const std::string& w) {
  if (w == "enter" || w == "return") return SDLK_RETURN;
  if (w == "left") return SDLK_LEFT;
  if (w == "right") return SDLK_RIGHT;
  if (w == "up") return SDLK_UP;
  if (w == "down") return SDLK_DOWN;
  if (w == "x") return SDLK_X;
  if (w.size() == 1 && w[0] >= '1' && w[0] <= '4') return SDLK_1 + (w[0] - '1');
  return 0;
}
// a point of the open page (menu box coordinates, laid out at the box's size) to the screen
struct BoxScope {
  Pix& P;
  int x = 0, y = 0;
  BoxScope(View& v, const Game& g) : P(SkillsUiOps::pix(v)) {
    int w = 0, h = 0;
    SkillsUiOps::boxOf(v, g, x, y, w, h);
    P.pushBox(x, y, w, h);
  }
  ~BoxScope() { P.popBox(); }
};

bool cmdSkillsUi(ScriptCtx& c) {
  Game& g = c.game;
  View& v = c.view;
  View::SkillsUiState& S = SkillsUiOps::st(v);
  const std::string op = c.arg(1);
  const bool touch = SkillsUiOps::touch(v);
  if (op == "open") {
    const std::string pg = c.arg(2);
    if (pg != "skills" && pg != "spells") { c.fail("skillsui open skills|spells"); return true; }
    v.openMenu(g, pg == "skills" ? View::kSkillsTab : View::kSpellsTab);
    return true;
  }
  if (op == "skill") {
    skills::Skill s;
    if (!skillOf(c.arg(2), s)) { c.fail("skillsui skill: no skill called " + c.arg(2)); return true; }
    S.sel = (int)s; S.perkFor = -1;
    return true;
  }
  if (op == "perk") { S.perkSel = std::atoi(c.arg(2).c_str()); S.perkFor = S.sel; return true; }
  if (op == "slot") { S.slotSel = std::max(0, std::min(magic::QUICK_SLOTS - 1, std::atoi(c.arg(2).c_str()) - 1)); return true; }
  if (op == "spell") {
    const uint32_t id = spellOf(c.arg(2));
    if (!id) { c.fail("skillsui spell: no spell called " + c.arg(2)); return true; }
    S.spellSel = id;
    return true;
  }
  if (op == "key") {
    const int k = keyOf(c.arg(2));
    if (!k) { c.fail("skillsui key: enter|left|right|up|down|x|1..4"); return true; }
    SkillsUiOps::pageKey(v, g, k);
    return true;
  }
  if (op == "tap") {
    const std::string what = c.arg(2);
    float x = 0, y = 0;
    if (what == "quick") {   // the HUD (screen coordinates)
      const int b = std::atoi(c.arg(3).c_str()) - 1;
      if (b < 0 || b > 4) { c.fail("skillsui tap quick <1..5>"); return true; }
      const QuickBtn q = quickBtnPos(b);
      c.tap(q.x, q.y);
      return true;
    }
    bool ok = true;
    int bx = 0, by = 0;
    {
      BoxScope box(v, g);
      bx = box.x; by = box.y;
      if (what == "star") {
        ok = skillsUiStarAt(touch, (skills::Skill)S.sel, std::atoi(c.arg(3).c_str()), x, y);
      } else if (what == "take") {
        skillsUiTakeAt(touch, x, y);
      } else if (what == "skill") {
        skills::Skill s;
        ok = skillOf(c.arg(3), s) && skillsUiRowAt(touch, s, x, y);
      } else if (what == "group") {
        skillsUiGroupAt(touch, std::atoi(c.arg(3).c_str()), x, y);
      } else if (what == "slot") {
        spellsUiSlotAt(touch, std::max(0, std::atoi(c.arg(3).c_str()) - 1), x, y);
      } else if (what == "spell") {
        const uint32_t id = spellOf(c.arg(3));
        ok = id && spellsUiRowAt(g, touch, S.spellScroll, id, x, y);
      } else if (what == "prepare" || what == "clear") {
        spellsUiButtonAt(touch, what == "clear", x, y);
      } else {
        c.fail("skillsui tap star|take|skill|group|slot|spell|prepare|clear|quick ...");
        return true;
      }
    }
    if (!ok) { c.fail("skillsui tap " + what + ": not on the page"); return true; }
    c.tap((float)bx + x, (float)by + y);
    return true;
  }
  if (op == "rested") { MagicOps::rested(g, 8); return true; }   // as after a night's rest: the loadout may change
  if (op == "give") {   // a skill book / a spell tome into the pack, at its top (its card shows first)
    Item it;
    if (c.arg(2) == "book") {
      skills::Skill s;
      if (!skillOf(c.arg(3), s)) { c.fail("skillsui give book <skill>"); return true; }
      it = skills::makeSkillBook(s, 0xB00Cu + (uint32_t)s, 10);
    } else if (c.arg(2) == "tome") {
      const uint32_t id = spellOf(c.arg(3));
      if (!id) { c.fail("skillsui give tome <spell>"); return true; }
      it = magic::makeTome(id);
    } else { c.fail("skillsui give book <skill> | tome <spell>"); return true; }
    g.inv.insert(g.inv.begin(), it);
    return true;
  }
  c.fail("skillsui: open | skill | perk | slot | spell | key | tap | give");
  return true;
}
EMB_SCRIPT_CMD("skillsui", "skillsui open|skill|perk|slot|spell|key|tap ...: drive the SKILLS / SPELLS pages and quick-cast (M9)", cmdSkillsUi);

bool expSkillsUi(ScriptCtx& c) {
  Game& g = c.game;
  View& v = c.view;
  View::SkillsUiState& S = SkillsUiOps::st(v);
  const std::string what = c.arg(2);
  auto atLeast = [&](size_t i, int def) { return c.arg(i).empty() ? def : std::atoi(c.arg(i).c_str()); };
  if (what == "page") {
    const int want = c.arg(3) == "skills" ? View::kSkillsTab : View::kSpellsTab;
    if (g.mode != Mode::Menu || SkillsUiOps::menuTab(v) != want) c.fail("expect skillsui page: not on " + c.arg(3));
  } else if (what == "skill") {
    skills::Skill s;
    if (!skillOf(c.arg(3), s) || S.sel != (int)s) c.fail("expect skillsui skill: " + std::string(skills::skillInfo((skills::Skill)S.sel).name) + " in focus");
  } else if (what == "perk") {
    if (S.perkSel != std::atoi(c.arg(3).c_str())) c.fail("expect skillsui perk: perk " + std::to_string(S.perkSel) + " in focus");
  } else if (what == "slotsel") {
    if (S.slotSel != std::atoi(c.arg(3).c_str()) - 1) c.fail("expect skillsui slotsel: slot " + std::to_string(S.slotSel + 1) + " in focus");
  } else if (what == "slot") {
    const int i = std::atoi(c.arg(3).c_str()) - 1;
    if (i < 0 || i >= magic::QUICK_SLOTS) { c.fail("expect skillsui slot <1..4> <spell|empty>"); return true; }
    const uint32_t have = g.arcana.book.slot[i];
    const uint32_t want = c.arg(4) == "empty" ? 0 : spellOf(c.arg(4));
    if (have != want) {
      const magic::SpellDef* d = magic::spellById(have);
      c.fail("expect skillsui slot " + c.arg(3) + ": holds " + (d ? d->name : std::string("nothing")));
    }
  } else if (what == "why") {
    const std::string w = c.rest(3);
    if (S.why.find(w) == std::string::npos) c.fail("expect skillsui why: '" + S.why + "'");
  } else if (what == "toast") {
    const std::string w = c.rest(3);
    if (S.lastToast.find(w) == std::string::npos) c.fail("expect skillsui toast: '" + S.lastToast + "'");
  } else if (what == "ready") {
    if (S.starsReady < atLeast(3, 1)) c.fail("expect skillsui ready: " + std::to_string(S.starsReady) + " ready stars drawn");
  } else if (what == "quick") {
    if (S.quickDrawn < atLeast(3, 1)) c.fail("expect skillsui quick: " + std::to_string(S.quickDrawn) + " quick buttons drawn");
  } else if (what == "input") {
    const std::string k = c.arg(3);
    if (k == "press") {
      const int i = std::atoi(c.arg(4).c_str()) - 1;
      if (i < 0 || i > 3 || S.sentPress[i] < atLeast(5, 1)) c.fail("expect skillsui input press: slot " + c.arg(4) + " pressed " + std::to_string(i >= 0 && i < 4 ? S.sentPress[i] : 0) + " times");
    } else if (k == "hold") {
      if (S.sentHoldMax + 1e-3f < (float)std::atof(c.arg(4).c_str())) c.fail("expect skillsui input hold: the longest hold was " + std::to_string(S.sentHoldMax) + " s");
    } else if (k == "dual") {
      if (S.sentDual < atLeast(4, 1)) c.fail("expect skillsui input dual: " + std::to_string(S.sentDual) + " dual releases");
    } else if (k == "sneak") {
      if (S.sentSneak < atLeast(4, 1)) c.fail("expect skillsui input sneak: " + std::to_string(S.sentSneak) + " toggles");
    } else if (k == "release") {
      if (S.sentRelease < atLeast(4, 1)) c.fail("expect skillsui input release: " + std::to_string(S.sentRelease) + " releases");
    } else c.fail("expect skillsui input press|hold|dual|sneak|release ...");
  } else if (what == "sneaking") {
    if ((int)g.skills.live.sneaking != std::atoi(c.arg(3).c_str())) c.fail(std::string("expect skillsui sneaking: ") + (g.skills.live.sneaking ? "1" : "0"));
  } else if (what == "casts") {
    if (g.arcana.live.stats.casts < atLeast(3, 1)) c.fail("expect skillsui casts: " + std::to_string(g.arcana.live.stats.casts));
  } else {
    c.fail("expect skillsui page|skill|perk|slotsel|slot|why|toast|ready|quick|input|sneaking|casts ...");
  }
  return true;
}
EMB_SCRIPT_CMD("expect:skillsui", "expect skillsui page|skill|perk|slot|why|toast|ready|quick|input|sneaking|casts ... (M9)", expSkillsUi);
}  // namespace
