// The registry behind rpg/view/script_api.h (a function-local static: registrations from other files' static
// initialisers may run before anything else in this file).
#include "rpg/view/script_api.h"

std::vector<ScriptExt>& scriptExts() {
  static std::vector<ScriptExt> v;
  return v;
}
