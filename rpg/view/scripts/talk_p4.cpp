// rpg/view/scripts/talk_p4.cpp - the P4 TALK lane's script checks (test only; docs/FIX_BATCH_P4.md 5, 9.2).
//   expect optindex <words> <n>   the open dialogue's page on show lists an option containing <words> as its n-th row
//                                 (1-based: the number the panel and the keyboard give it); one order of options, talk.h 6
//   p4food give|drop              (staging) two loaves of bread into the pack, or every food out of it (a speaker's
//                                 SHARE YOUR BREAD comes and goes with it)
//   expect cursor <words>         the keyboard cursor's first row (speech::startCursor: the first passing quest option,
//                                 COLLECT, the main quest, a story scene; else the first row) contains <words>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/items.h"
#include "rpg/sim/talk.h"
#include "rpg/view/script_api.h"

namespace {

std::vector<const DlgOpt*> page(const Game& g) {
  std::vector<const DlgOpt*> v;
  for (const DlgOpt& o : g.dlg.opts) if (g.dlg.shows(o)) v.push_back(&o);
  return v;
}
std::string listOf(const std::vector<const DlgOpt*>& p) {
  std::string s;
  for (size_t i = 0; i < p.size(); i++) s += (i ? " / " : "") + std::to_string(i + 1) + ") " + p[i]->label;
  return s;
}

bool expOptIndex(ScriptCtx& c) {
  if (c.a.size() < 4) { c.fail("expect optindex <words> <n>"); return true; }
  const std::string nWord = c.a.back();
  const int want = std::atoi(nWord.c_str());
  std::string words;
  for (size_t k = 2; k + 1 < c.a.size(); k++) words += (k > 2 ? " " : "") + c.a[k];
  if (want < 1) { c.fail("expect optindex <words> <n>: n counts from 1"); return true; }
  if (c.game.mode != Mode::Dialogue) { c.fail("expect optindex " + words + ": no dialogue is open"); return true; }
  const std::vector<const DlgOpt*> p = page(c.game);
  int at = -1;
  for (size_t i = 0; i < p.size() && at < 0; i++) if (p[i]->label.find(words) != std::string::npos) at = (int)i + 1;
  if (at != want)
    c.fail("expect optindex " + words + " " + nWord + ": " + (at < 0 ? std::string("not on the page") : "it is row " + std::to_string(at)) + " (" +
           listOf(p) + ")");
  return true;
}

bool expCursor(ScriptCtx& c) {
  const std::string words = c.rest(2);
  if (c.game.mode != Mode::Dialogue) { c.fail("expect cursor " + words + ": no dialogue is open"); return true; }
  const std::vector<const DlgOpt*> p = page(c.game);
  const int k = speech::startCursor(c.game.dlg);
  const std::string got = k >= 0 && k < (int)p.size() ? p[(size_t)k]->label : std::string();
  if (got.find(words) == std::string::npos) c.fail("expect cursor " + words + ": the cursor starts on '" + got + "' (" + listOf(p) + ")");
  return true;
}

bool p4Food(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  if (w == "give") {
    Item bread = makeFood(0);
    bread.count = 2;
    g.inv.push_back(bread);
  } else if (w == "drop") {
    for (int i = (int)g.inv.size() - 1; i >= 0; i--) if (g.inv[(size_t)i].kind == ItemKind::Food) g.dropItem(i);
  } else c.fail("p4food give|drop");
  return true;
}

}  // namespace

EMB_SCRIPT_CMD("p4food", "p4food give|drop: two loaves into the pack, or every food out of it (P4 TALK staging, test only)", p4Food);
EMB_SCRIPT_CMD("expect:optindex", "expect optindex <words> <n>: the page on show lists an option with <words> as row n (1-based; P4 TALK, test only)",
               expOptIndex);
EMB_SCRIPT_CMD("expect:cursor", "expect cursor <words>: the dialogue's keyboard cursor starts on an option with <words> (P4 TALK, test only)", expCursor);
