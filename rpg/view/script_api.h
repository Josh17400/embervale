// M2: script commands registered from any rpg/view/*.cpp, so milestone lanes add test commands without editing
// rpg/main.cpp (the script driver, which owns the built-in commands: see its header for the script language).
//
//   #include "rpg/view/script_api.h"
//   static bool cmdTravel(ScriptCtx& c) { ...; return true; }            // true: done
//   EMB_SCRIPT_CMD("travel", "travel <site words>: fast travel behind the fade", cmdTravel);
//   static bool expQuestType(ScriptCtx& c) { if (...) c.fail("..."); return true; }
//   EMB_SCRIPT_CMD("expect:questtype", "expect questtype deliver: ...", expQuestType);
//
// A command name "expect:<what>" extends `expect <what> ...` (the built-in checks win). The function runs in the
// frame its line is due; returning false asks to be called again next frame (script time stands still meanwhile, like
// walkto), up to `timeout` seconds (then the line fails). c.a[0] is the command (lower case); the other words keep
// their case. Commands must never touch the player's save (scripts run without one).
#pragma once
#include <functional>
#include <string>
#include <vector>

class Game;
class View;

struct ScriptCtx {
  Game& game;
  View& view;
  const std::vector<std::string>& a;   // the words after the time: a[0] the command (for expect: a[0] == "expect")
  int line = 0;
  float scriptT = 0;                   // script time now
  float waited = 0;                    // seconds this line has been retried (0 on the first call)
  std::function<void(const std::string&)> fail;
  std::function<void(float, float)> tap;   // a touch tap at logical screen coordinates (as the `tap` command)
  std::string arg(size_t i) const { return i < a.size() ? a[i] : std::string(); }
  std::string rest(size_t from) const {   // the words from `from` on, joined by single spaces
    std::string r;
    for (size_t k = from; k < a.size(); k++) { if (k > from) r += ' '; r += a[k]; }
    return r;
  }
};

using ScriptFn = bool (*)(ScriptCtx&);
struct ScriptExt {
  const char* name;      // "travel", or "expect:<what>"
  const char* help;
  ScriptFn fn;
  float timeout = 60;    // seconds a command that keeps returning false may wait
};
std::vector<ScriptExt>& scriptExts();
inline int registerScriptExt(const ScriptExt& e) { scriptExts().push_back(e); return (int)scriptExts().size(); }
#define EMB_SCRIPT_CAT2(a, b) a##b
#define EMB_SCRIPT_CAT(a, b) EMB_SCRIPT_CAT2(a, b)
#define EMB_SCRIPT_CMD(name, help, fn) \
  static const int EMB_SCRIPT_CAT(embScriptReg_, __LINE__) = registerScriptExt(ScriptExt{name, help, fn})
