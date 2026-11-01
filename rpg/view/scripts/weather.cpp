// Test-only script command `weather` (force a Sky kind for screenshots), moved from rpg/view/render.cpp (SOLID 1.5).
// kSkyClear is render.cpp's file-local value for "no weather at all"; the two copies must stay equal.
#include <cstdio>
#include <string>
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/view/view_probe.h"

// (M11 TERRAIN) View::scriptSky's value for "weather clear": no rain, snow, fog or sand at all (review shots)
constexpr int kSkyClear = 99;

// (M3c LIFE) test scripts: force the weather (screenshots of every Sky kind)
namespace {
bool cmdWeather(ScriptCtx& c) {
  static const char* names[] = {"temperate", "showery", "misty", "dry", "arid", "snowy", "blizzard", "monsoon", "ashfall", "eerie"};
  static_assert(sizeof(names) / sizeof(names[0]) == (size_t)Sky::COUNT, "a name for every Sky");
  const std::string w = c.arg(1);
  if (w == "auto" || w.empty()) { ViewProbe::scriptSky(c.view, -1); return true; }
  if (w == "clear") { ViewProbe::scriptSky(c.view, kSkyClear); std::printf("weather: clear\n"); return true; }   // (M11 TERRAIN) no weather at all: review shots
  for (int i = 0; i < (int)Sky::COUNT; i++)
    if (w == names[i]) { ViewProbe::scriptSky(c.view, i); std::printf("weather: %s\n", names[i]); return true; }
  c.fail("weather: unknown kind " + w);
  return true;
}
}  // namespace
EMB_SCRIPT_CMD("weather", "weather temperate|showery|misty|dry|arid|snowy|blizzard|monsoon|ashfall|eerie|clear|auto: force the weather (full strength; clear: none)", cmdWeather);
