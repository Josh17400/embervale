// HomeView (SOLID 4.2, View part: the M7 Home lane), a value member of View (View::home_). The property's draws in
// the world (yard objects, crops, ground edits, the building site, the build ghost: y-sorted with the scene as
// Drawable kind 8), their flat and shadow passes and lights, the player on horseback, the Mode::Build screens (shell,
// yard, decorate, storage, cooking, buy) with their camera, drags and hints, and the HUD's home pills. Bodies:
// rpg/view/home_view.cpp, buildmode.cpp.
// From view.h's M7 Home block (VIEW lane: rpg/view/home_view.cpp and buildmode.cpp; the VIEW lane is the only editor and
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"
#include "rpg/view/fx_state.h"   // LightPool
class View;

struct HomeView {
  //      may ADD members here). The world: drawWorld asks homeCollect for the property's draws (yard objects, crops,
  //      ground edits, the building site, the build ghost) each frame and y-sorts them with the scene (Drawable kind 8:
  //      drawHomeDraw blits one); the player on horseback is drawn by drawMounted (true: drawn, the standing figure is
  //      skipped). The screens: Mode::Build (Game::home.ui: the shell, yard, decorate, storage, cooking and buy screens)
  //      by drawBuild / buildKey / buildTap (screen coordinates; the world under the panel is the ghost's canvas).
  struct HomeDraw {
    float sortY = 0;                 // the y it sorts by (world px: the footprint's bottom edge)
    const Tex* tex = nullptr;
    int sx = 0, sy = 0, sw = 0, sh = 0;   // source rect in tex
    float x = 0, y = 0;              // top-left on screen... in WORLD px (drawHomeDraw subtracts the camera)
    bool flip = false;
    Color tint = Color(1, 1, 1, 1);
    uint8_t shadow = 0;              // 0 none, 1 a small round ground shadow under it, 2 a footprint-wide one
    bool flat = false;               // a ground edit (drawn flat: sorts under everything standing on its row)
    // (M7 VIEW lane) added: the ground shadow's ellipse in world px (shadow != 0; drawn in the shadow pass, under
    // everything standing), a crop's sway (px of lean at the top in the wind, 0 none; phase from x), and UI overlays
    // drawn as tinted rects instead of a texture (ui 1: a footprint cell of the build ghost, 2: the plot's border)
    float shx = 0, shy = 0, shw = 0, shh = 0, shA = 0.7f;
    float sway = 0;
    uint8_t ui = 0;
  };
  std::vector<HomeDraw> homeDraws_;
  void homeCollect(View& v, Game& g, const Map& m, Vec2 cam);
  void drawHomeDraw(View& v, const HomeDraw& d, Vec2 cam);
  bool drawMounted(View& v, Game& g, const Actor& a, Vec2 cam);
  int buildSel_ = 0, buildScroll_ = 0, buildTab_ = 0;   // the Mode::Build screens' UI state (free for the lane)
  void drawBuild(View& v, Game& g);
  void buildKey(View& v, Game& g, int key);
  void buildTap(View& v, Game& g, Vec2 p);
  std::vector<LightPool> homeLights_;
  void drawHomeFlats(View& v, Vec2 cam);
  void drawHomeShadow(View& v, const HomeDraw& d, Vec2 cam);
  void drawMountShadow(View& v, Game& g, const Actor& a, Vec2 cam);
  bool buildCamWant(View& v, Game& g, Vec2& want);       // Mode::Build: where the camera wants to be (the ghost in view)
  void buildPointer(View& v, Game& g, int phase, Vec2 p); // 1 move, 2 up (screen coords; the down is buildTap)
  // the HUD's home pills (YARD / DECORATE where the player owns the ground or the house, DISMOUNT on horseback)
  int homeHere(const Game& g, bool& indoors) const;   // the owned plot the player stands in or whose house he is in (-1)
  void drawHomeHud(View& v, Game& g);
  bool homeHudTap(View& v, Game& g, Vec2 p);              // true: a pill took the tap
  bool homeHudKey(View& v, Game& g, int key);             // B: yard / decorate, F: dismount
  void openBuild(Game& g, home::UiMode mode, int plot);
  struct BuildDrag { bool on = false, moved = false; Vec2 start; int gx = 0, gy = 0; int list = -1; float scroll0 = 0; int zone = 0; };
  BuildDrag buildDrag_;
  int buildCat_ = 0, storeSel_ = 0, storeSide_ = 0, packScroll_ = 0, storeScroll_ = 0, cookScroll_ = 0;
  std::string cookNote_;   // (M7 fix r3) the Cook screen's own "COOKED: X (HAVE n)" line (for the meal row cookNoteSel_)
  int cookNoteSel_ = -1;
  float cookNoteT_ = -100.0f;   // (t_ when it was set: it shows for a few seconds)
  bool buildHintDone_ = false;                       // (M7) the yard / decorate move hint was seen and used
  int buildHintGx_ = INT32_MIN, buildHintGy_ = INT32_MIN;   // the ghost tile the hint was first shown at
};
