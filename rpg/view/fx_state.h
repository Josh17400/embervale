// FxState (SOLID 4.2, View part 4): the view's short-lived effects, a value member of View (View::fx_). The particles
// (squares and fx sprites), the floating lines (combat numbers, spoken lines), the toasts and the toast column's extent,
// the arrival banner's lines and its clock, and the screen shake. Events spawn them (spawnParticles, update's event pass),
// updateEffects ages them, updateShake moves the shake, drawToasts draws the toast column. Included by
// rpg/view/view.h; the bodies stay in render.cpp and hud.cpp, where they were.
#pragma once
#include <string>
#include <vector>
#include "engine/color.h"
#include "engine/mathx.h"

class View;
class Game;
struct Event;
struct Pickup;

// (P4 6f, loot_ui.cpp) a fresh drop's toss out of the body: the offset it is drawn at for its first kTossTime seconds
// (0, 0 after), and the lift (px above its spot) and age of the last tossed piece drawn since the last read (`expect pickuplift`)
constexpr float kTossTime = 0.3f;
Vec2 pickupToss(const Pickup& k);
float pickupLiftDrawn(float& age);

struct Particle {
  Vec2 p, v;
  float life = 1, max = 1, size = 1, grav = 0;
  Color c;
  int kind = 0;      // 0 square, 1 fx sprite
  int fx = 0, layer = 0;
  bool world = true;
};
// a floating line over the world: a combat number rising and fading (0.9 s), or (M5) a spoken line (an overheard
// remark, a greeting by name: Ev::Text of words) held over the speaker on a dark plate, wrapped, for `life` seconds
// (P1 fixer r2) actor: a spoken line's speaker (Actor::id; -1 none), whom the plate follows at off from their feet
struct FloatText { Vec2 p; std::string s; Color c; float t = 0; bool speech = false; float life = 0.9f; int actor = -1; Vec2 off; };
// (M8b) rarity: an item notice (its name in the ladder colour)
// (P4 item 3) dlg: an item notice ("+ OLD BLADE") made while the dialogue is open: the dialogue panel shows it in its
// gift strip under the speaker's name (dialogue_ui.cpp), and it is dropped, never shown in the column, when the panel closes
struct Toast { std::string s; Color c; float t = 0; int rarity = -1; bool dlg = false; };

// a pool of light added to the light map at night (the lanes' lamps, fires, yard lights: drawLighting)
struct LightPool { Vec2 p; float r; Color c; float k; };

struct FxState {
  Vec2 shakeOff_;
  std::vector<Particle> parts_;
  std::vector<FloatText> texts_;
  std::vector<Toast> toasts_;
  std::string banner_, bannerSub_;
  float bannerT_ = 0;
  float shake_ = 0;
  float toastX1_ = 0, toastY0_ = 0, toastY1_ = 0;   // (M7 fix) the toast column's extent last frame (world prompts keep clear)
  void updateShake(View& v, Game& g, float dt, bool camHeld);
  void updateEffects(View& v, Game& g, float dt);
  void drawToasts(View& v);
  void spawnParticles(View& v, const Event& e, Game& g);
  // (P4, dialogue_ui.cpp) the HUD notes: the gift strip's toasts (Toast::dlg, oldest first), the danger banner on entering
  // a dungeon well above the hero (6a), the menu page's key hint (6b); `of` reads a View's effects (scripts, Witness)
  static std::vector<const Toast*> dlgGifts(const View& v);
  static void dangerBanner(View& v, const Game& g);
  static void drawMenuHint(View& v, int tab);
  static const FxState& of(const View& v);
};
