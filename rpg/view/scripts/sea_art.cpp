// M10 ART lane: script commands that stage the sea's looks for screenshots (test only; never touch a save).
// (storms, casting, the sea's creatures: the SEA lane's `sea storm`, `sea fish`, `sea spawn`)
//   seaart line bite|reel [t]     the line on the hook: the bite cue, or a fish being reeled at tension t (0..1)
//   seaart catch <species>        a fish landed: the catch card (the species index into sea::species())
//   seaart contact <words>        the first-contact title over the sea
//   seaart stats                  print last frame's sea view counts and the sea draw's time (ms)
//   expect seaview <boats|wakes|waves|foam> <min>   last frame's sea view counts (View::seaViewStats)
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/sim/sea.h"
#include "rpg/sim/sea_ops.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

void seaArtEvent(View& v, Game& g, const Event& e);   // render_sea.cpp: hand the view a sea event

namespace {
bool cmdSeaArt(ScriptCtx& c) {
  Game& g = c.game;
  const std::string op = c.arg(1);
  if (op == "line") {
    sea::Sea::Line& ln = g.sea.live.line;
    if (ln.phase == sea::Sea::Cast::None) { c.fail("seaart line: no line out"); return true; }
    if (c.arg(2) == "bite") { ln.phase = sea::Sea::Cast::Bite; ln.t = 0; ln.bite = 30.0f; }
    else {
      ln.phase = sea::Sea::Cast::Reeling;
      ln.t = 0;
      ln.tension = c.arg(3).empty() ? 0.6f : (float)std::atof(c.arg(3).c_str());
      ln.dist = 0.55f;
    }
    return true;
  }
  if (op == "catch") {
    Event e;
    e.type = Ev::Sea;
    e.a = (int)sea::SeaEv::Landed;
    e.id = (uint32_t)std::atoi(c.arg(2).c_str());
    e.f = e.id < sea::species().size() ? (float)sea::species()[e.id].rarity : 0.0f;
    e.p = g.pl().p;
    seaArtEvent(c.view, g, e);
    return true;
  }
  if (op == "contact") {
    Event e;
    e.type = Ev::Sea;
    e.a = (int)sea::SeaEv::FirstContact;
    e.s = c.rest(2);
    seaArtEvent(c.view, g, e);
    return true;
  }
  if (op == "stats") {
    const View::SeaViewStats st = c.view.seaViewStats();
    std::printf("seaview: boats %d waves %d foam %d wakes %d, sea draw %.3f ms\n", st.boats, st.waveTiles, st.foamTiles, st.wakes, st.ms);
    return true;
  }
  c.fail("seaart: line|catch|contact|stats");
  return true;
}
EMB_SCRIPT_CMD("seaart", "seaart line bite|reel [t] | catch <n> | contact <words> (M10 ART)", cmdSeaArt);

bool expSeaView(ScriptCtx& c) {
  const View::SeaViewStats s = c.view.seaViewStats();
  const std::string what = c.arg(2);
  const int min = std::atoi(c.arg(3).c_str());
  const int v = what == "boats" ? s.boats : what == "wakes" ? s.wakes : what == "waves" ? s.waveTiles : what == "foam" ? s.foamTiles : -1;
  if (v < 0) { c.fail("expect seaview: boats|wakes|waves|foam"); return true; }
  if (v < min) c.fail("expect seaview " + what + ": " + std::to_string(v) + " < " + std::to_string(min));
  return true;
}
EMB_SCRIPT_CMD("expect:seaview", "expect seaview <boats|wakes|waves|foam> <min>: last frame's sea view counts (M10 ART)", expSeaView);
}  // namespace
