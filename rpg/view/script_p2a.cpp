// P2a Clear Sight test commands (VISION_PLAN 15.36 item 10; the phase A harness, frozen: lanes report a harness problem
// instead of editing it). Every command is also a Witness call (rpg/main.cpp lists the registered commands).
//   zoom far|normal|near|close|-1..2|+|-   the ZOOM setting (an offset from D0; + / - step through the levels allowed)
//   rig 1|2                                 the human rig (art::setRigVersion; the rig-dependent caches are rebuilt)
//   body man|woman                          the hero's body base (Appearance::female, the v2 rig's body)
//   pose <clip> [family] [variant] [frame] | pose off   freeze the hero's frame (clip names: art::clipName)
//   hideplayer [0|1]                        the hero's body not drawn (the shadow and ring stay): the shot pair's 2nd shot
//   shotpair <with.png> <without.png> [hero|ring]   two device shots of the same moment, with and without the hero's
//                                           body (or the PLAYER RING): the pairs p2_measure.mjs mask / contrast / ring take
//   ui 0|1                                  the UI layer drawn or not (ui 0: the world alone, for p2_measure.mjs)
//   ring [0|1]                              PLAYER RING (toggles without an argument)
//   expect screen d0 <D> | levels <D,D,...> | want <D> (the zoom's D) | world <D> (the layer as drawn) | integer 0|1 | ui <w> <h>
//   expect zoom <offset|name>   expect rig <1|2>   expect anim <clip> [family] [variant]
//   expect corpse <n> [minAge] [maxAge]     at least n dead humans within 20 tiles, dead for minAge..maxAge s
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/sim/anim.h"
#include "rpg/sim/game.h"
#include "rpg/view/screen.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"

struct P2aScriptOps {
  static int zoomArg(const std::string& s, int cur, bool& ok) {
    ok = true;
    std::string w = s;
    for (char& ch : w) ch = (char)std::tolower((unsigned char)ch);
    if (w == "far") return -1;
    if (w == "normal") return 0;
    if (w == "near") return 1;
    if (w == "close") return 2;
    if (w == "+" || w == "-") {   // the next / previous level the screen allows
      const std::vector<int> lv = screen::zoomLevels();
      const int d = screen::worldScale();
      size_t i = 0;
      while (i < lv.size() && lv[i] != d) i++;
      if (i >= lv.size()) return cur;
      if (w == "+" && i + 1 < lv.size()) return screen::zoomLevelOf(lv[i + 1]);
      if (w == "-" && i > 0) return screen::zoomLevelOf(lv[i - 1]);
      return cur;
    }
    char* e = nullptr;
    const long v = std::strtol(w.c_str(), &e, 10);
    if (e == w.c_str() || *e) { ok = false; return cur; }
    return (int)v;
  }
  static bool clipArg(const std::string& s, art::Clip& out) {
    for (int c = 0; c < art::CLIP_COUNT; c++)
      if (s == art::clipName((art::Clip)c)) { out = (art::Clip)c; return true; }
    return false;
  }
  static bool familyArg(const std::string& s, art::Family& out) {
    for (int f = 0; f < art::FAMILY_COUNT; f++)
      if (s == art::familyName((art::Family)f)) { out = (art::Family)f; return true; }
    return false;
  }
  static std::string lower(std::string s) { for (char& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; }
  static const Pix::Layer& worldLayer(const View& v) { return v.pix_->worldLayer(); }
  static Pix* pix(View& v) { return v.pix_; }
};

namespace {
using Ops = P2aScriptOps;

bool cmdZoom(ScriptCtx& c) {
  screen::Settings s = screen::get();
  bool ok = false;
  const int z = Ops::zoomArg(c.arg(1), s.zoom, ok);
  if (!ok) { c.fail("zoom: far|normal|near|close|-1..2|+|- (got '" + c.arg(1) + "')"); return true; }
  s.zoom = std::clamp(z, screen::kZoomMin, screen::kZoomMax);
  screen::set(s);
  return true;
}
EMB_SCRIPT_CMD("zoom", "zoom far|normal|near|close|-1..2|+|-: the ZOOM setting (P2a; an offset from D0, each level only where the screen allows it)", cmdZoom);

bool cmdRig(ScriptCtx& c) {
  const int v = std::atoi(c.arg(1).c_str());
  if (v != 1 && v != 2) { c.fail("rig: 1 or 2"); return true; }
  if (v != art::rigVersion()) {
    art::setRigVersion(v);
    c.view.rigChanged();
  }
  return true;
}
EMB_SCRIPT_CMD("rig", "rig 1|2: the human rig (P2a; 2 previews the v2 people where they are painted)", cmdRig);

bool cmdBody(ScriptCtx& c) {
  const std::string w = Ops::lower(c.arg(1));
  if (w != "man" && w != "woman" && w != "0" && w != "1") { c.fail("body: man|woman"); return true; }
  c.game.app.female = w == "woman" || w == "1";
  AnimOps::refreshHero(c.game);
  return true;
}
EMB_SCRIPT_CMD("body", "body man|woman: the hero's body base (P2a: Appearance::female, the v2 rig's body)", cmdBody);

bool cmdPose(ScriptCtx& c) {
  const std::string w = Ops::lower(c.arg(1));
  if (w == "off" || w.empty()) { c.view.p2a.poseOn = false; return true; }
  art::Clip clip;
  if (!Ops::clipArg(w, clip)) { c.fail("pose: a clip (idle walk hurt windup strike follow cast draw release block roll fall lying) or off"); return true; }
  art::Family fam = art::familyOf(c.game.pl().look);
  size_t i = 2;
  if (i < c.a.size() && Ops::familyArg(Ops::lower(c.arg(i)), fam)) i++;
  const int variant = i < c.a.size() ? std::atoi(c.arg(i++).c_str()) : 0;
  const int frame = i < c.a.size() ? std::atoi(c.arg(i++).c_str()) : 0;
  View::P2aHooks& h = c.view.p2a;
  h.poseOn = true; h.poseClip = clip; h.poseFamily = fam;
  h.poseVariant = std::clamp(variant, 0, art::RIG_VARIANTS - 1); h.poseFrame = std::max(0, frame);
  return true;
}
EMB_SCRIPT_CMD("pose", "pose <clip> [family] [variant] [frame] | pose off: freeze the hero's frame (P2a)", cmdPose);

bool cmdHidePlayer(ScriptCtx& c) {
  c.view.p2a.hidePlayer = c.arg(1).empty() || c.arg(1) != "0";
  return true;
}
EMB_SCRIPT_CMD("hideplayer", "hideplayer [0|1]: the hero's body not drawn, the shadow and ring stay (P2a: the shot pair's mask)", cmdHidePlayer);

// two device-resolution shots of the SAME moment, with and without the hero's body (no sim step or view time between
// them: the world, its weather and its particles are identical, so p2_measure.mjs masks exactly the figure)
// (what = ring: with and without the PLAYER RING, the pair p2_measure.mjs ring takes)
bool cmdShotPair(ScriptCtx& c) {
  if (c.arg(1).empty() || c.arg(2).empty()) { c.fail("shotpair: <with.png> <without.png> [hero|ring]"); return true; }
  const bool ring = Ops::lower(c.arg(3)) == "ring";
  const bool wasHidden = c.view.p2a.hidePlayer;
  const screen::Settings wasSet = screen::get();
  Pix* pix = Ops::pix(c.view);
  for (int k = 0; k < 2; k++) {
    if (ring) { screen::Settings s = wasSet; s.ring = k == 0; screen::set(s); }
    else c.view.p2a.hidePlayer = k == 1;
    c.view.draw(c.game, false);
    if (!pix || !pix->screenshot(c.arg(1 + k).c_str())) c.fail("shotpair: could not write " + c.arg(1 + k));
  }
  c.view.p2a.hidePlayer = wasHidden;
  if (ring) screen::set(wasSet);
  return true;
}
EMB_SCRIPT_CMD("shotpair", "shotpair <with.png> <without.png> [hero|ring]: two shots of the same moment, with and without the hero's body (or the ring) (P2a: p2_measure.mjs mask / contrast / ring)", cmdShotPair);

bool cmdUi(ScriptCtx& c) {
  c.view.p2a.ui = c.arg(1) != "0";
  return true;
}
EMB_SCRIPT_CMD("ui", "ui 0|1: the UI layer drawn or not (P2a; ui 0 = the world alone)", cmdUi);

bool cmdRing(ScriptCtx& c) {
  screen::Settings s = screen::get();
  s.ring = c.arg(1).empty() ? !s.ring : c.arg(1) != "0";
  screen::set(s);
  return true;
}
EMB_SCRIPT_CMD("ring", "ring [0|1]: PLAYER RING (P2a; toggles without an argument)", cmdRing);

// ---- checks
bool expScreen(ScriptCtx& c) {
  const std::string what = Ops::lower(c.arg(2));
  if (what == "d0") {
    const int want = std::atoi(c.arg(3).c_str());
    if (screen::defaultWorldScale() != want) c.fail("expect screen d0 " + c.arg(3) + ": D0 is " + std::to_string(screen::defaultWorldScale()));
  } else if (what == "levels") {
    std::string have;
    for (int d : screen::zoomLevels()) { if (!have.empty()) have += ','; have += std::to_string(d); }
    if (have != c.arg(3)) c.fail("expect screen levels " + c.arg(3) + ": the levels are " + have);
  } else if (what == "want") {   // the D the zoom asks for (screen::worldScale)
    const int want = std::atoi(c.arg(3).c_str());
    if (screen::worldScale() != want) c.fail("expect screen want " + c.arg(3) + ": the zoom asks for " + std::to_string(screen::worldScale()));
  } else if (what == "world") {   // the world layer as presented (LENS makes it the D the zoom asks for)
    const float ws = Ops::worldLayer(c.view).scale;
    if (std::fabs(ws - (float)std::atof(c.arg(3).c_str())) > 1e-3f) c.fail("expect screen world " + c.arg(3) + ": the world layer is drawn at " + std::to_string(ws));
  } else if (what == "integer") {
    const float ws = Ops::worldLayer(c.view).scale;
    const bool integer = std::fabs(ws - std::round(ws)) < 1e-4f;
    if (integer != (c.arg(3) != "0")) c.fail("expect screen integer " + c.arg(3) + ": the world layer is drawn at " + std::to_string(ws));
  } else if (what == "ui") {
    const int w = std::atoi(c.arg(3).c_str()), h = std::atoi(c.arg(4).c_str());
    if (Pix::W != w || Pix::H != h) c.fail("expect screen ui " + c.arg(3) + " " + c.arg(4) + ": the UI canvas is " + std::to_string(Pix::W) + " x " + std::to_string(Pix::H));
  } else {
    c.fail("expect screen d0|levels|want|world|integer|ui ...");
  }
  return true;
}
EMB_SCRIPT_CMD("expect:screen", "expect screen d0 <D> | levels <D,D..> | want <D> | world <D> | integer 0|1 | ui <w> <h>: the P2a screen fit (want: the zoom's D; world: the layer as drawn)", expScreen);

bool expZoom(ScriptCtx& c) {
  bool ok = false;
  const int want = Ops::zoomArg(c.arg(2), 99, ok);
  if (!ok) { c.fail("expect zoom <offset|far|normal|near|close>"); return true; }
  if (screen::get().zoom != want) c.fail("expect zoom " + c.arg(2) + ": ZOOM is " + std::to_string(screen::get().zoom));
  return true;
}
EMB_SCRIPT_CMD("expect:zoom", "expect zoom <offset|far|normal|near|close>: the ZOOM setting (P2a)", expZoom);

bool expRig(ScriptCtx& c) {
  if (art::rigVersion() != std::atoi(c.arg(2).c_str())) c.fail("expect rig " + c.arg(2) + ": the rig is " + std::to_string(art::rigVersion()));
  return true;
}
EMB_SCRIPT_CMD("expect:rig", "expect rig 1|2: the human rig (P2a)", expRig);

bool expAnim(ScriptCtx& c) {
  const AnimPick k = animPick(c.game, c.game.pl());
  art::Clip clip = k.clip;
  art::Family fam = k.family;
  int variant = k.variant;
  if (c.view.p2a.poseOn) { clip = c.view.p2a.poseClip; fam = c.view.p2a.poseFamily; variant = c.view.p2a.poseVariant; }
  art::Clip want;
  if (!Ops::clipArg(Ops::lower(c.arg(2)), want)) { c.fail("expect anim <clip> [family] [variant]"); return true; }
  std::string have = std::string(art::clipName(clip)) + " " + art::familyName(fam) + " " + std::to_string(variant);
  if (clip != want) { c.fail("expect anim " + c.rest(2) + ": the hero shows " + have); return true; }
  art::Family wf;
  if (c.a.size() > 3 && Ops::familyArg(Ops::lower(c.arg(3)), wf) && wf != fam) { c.fail("expect anim " + c.rest(2) + ": the hero shows " + have); return true; }
  if (c.a.size() > 4 && std::atoi(c.arg(4).c_str()) != variant) c.fail("expect anim " + c.rest(2) + ": the hero shows " + have);
  return true;
}
EMB_SCRIPT_CMD("expect:anim", "expect anim <clip> [family] [variant]: what the hero shows (P2a animPick, or the frozen pose)", expAnim);

bool expCorpse(ScriptCtx& c) {
  const int n = std::atoi(c.arg(2).c_str());
  const float lo = c.a.size() > 3 ? (float)std::atof(c.arg(3).c_str()) : 0.0f;
  const float hi = c.a.size() > 4 ? (float)std::atof(c.arg(4).c_str()) : 1e9f;
  int have = 0;
  float oldest = 0;
  const Vec2 p = c.game.pl().p;
  for (size_t i = 1; i < c.game.actors.size(); i++) {
    const Actor& a = c.game.actors[i];
    if (!a.human || a.st != AState::Dead || len(a.p - p) > 20.0f * TILE) continue;
    oldest = std::max(oldest, a.stT);
    if (a.stT >= lo && a.stT <= hi) have++;
  }
  if (have < n) c.fail("expect corpse " + c.rest(2) + ": " + std::to_string(have) + " dead within 20 tiles in that age (oldest " + std::to_string(oldest) + " s)");
  return true;
}
EMB_SCRIPT_CMD("expect:corpse", "expect corpse <n> [minAge] [maxAge]: at least n dead humans within 20 tiles, dead minAge..maxAge s (P2a)", expCorpse);

}  // namespace
