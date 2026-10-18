// The one translation unit that compiles the Witness adapter (engine/witness.h, STB-style). Built only with the
// EMBERVALE_WITNESS option (desktop; the web build has no sockets): see CMakeLists.txt and rpg/main.cpp.
#define WITNESS_IMPLEMENTATION
#include "engine/witness.h"
#include <cstdlib>
#include <functional>
#include <string>

// (rpg/view/witness_state.h) a JSON value as a string: the replay's final state (WITNESS_RECORD) and the `statematch`
// call's comparison text. The writer's struct is only complete in this file.
std::string witnessJsonString(const std::function<void(witness::Json&)>& fill) {
  witness_json w{};
  witness::Json j(&w);
  fill(j);
  std::string s = w.buf && !w.failed ? std::string(w.buf, w.len) : std::string();
  std::free(w.buf);
  return s;
}
