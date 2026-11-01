// LifeView (SOLID 4.2, View part: the M5 "Hearth and Hall" lane), a value member of View (View::life_). The
// settlement under a tile and its mood flags, speech bubbles and their pop-in, the festival's bunting, poles and
// lanterns, the player's buff icons, and the life of the place in sound (the bard's tavern piece, festival music, the
// crowd, animal voices, the alarm bell), with the M5 counters and the scripts' forced town look. Bodies:
// rpg/view/render.cpp, render_markers.cpp, hud.cpp.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/sim/game.h"

class View;

struct LifeView {
  // (scripts) the last frame's counts of what M5 drew
  struct M5Stats {
    int posed = 0, poseFallback = 0, critters = 0, bubbles = 0, lampsLit = 0, lampsDark = 0, stallsShut = 0, bunting = 0,
        festLanterns = 0, winLit = 0, winDark = 0, poseBakes = 0, speech = 0;
    int music = 0;            // the Music playing (its enum value)
    float tavernLevel = 0, crowd = 0;
  };
  M5Stats m5Stats_, m5Count_;
  int forceFest_ = -1, forceShut_ = -1;
  ew::Gid siteIdAt(View& v, const Game& g, int tx, int ty);   // the settlement a tile lies in (cached per 8 x 8-tile block; 0 none)
  uint16_t lifeFlagsAt(View& v, const Game& g, int tx, int ty);   // its mood flags (life::MF_*; forced by scriptLifeLook)
  bool festivalHere(const Game& g, ew::Gid site);
  // speech bubbles: when each actor's current bubble appeared (the pop-in) by Actor::id
  struct BubbleSeen { uint8_t kind = 0; float t0 = 0, seen = 0, maxT = 0; };
  std::unordered_map<int, BubbleSeen> bubbleSeen_;
  void drawBubbles(View& v, Game& g, Vec2 cam);
  bool buffTap(View& v, Game& g, Vec2 p);   // (fixer M5 r3) a tap on the HUD's buff icons explains them (true: taken)
  // festival: bunting strung between eaves across the streets of a festive settlement (drawn over the scene: it hangs
  // overhead), paper lanterns at doors and stalls lit at night (festLights_: their glow for the light pass)
  std::vector<Vec2> festLights_;
  void drawFestival(View& v, Game& g, const Map& m, Vec2 cam);
  // the festival's strings across the plaza: a run of open paving per string, a pole at each end (y-sorted with the
  // scene: Drawable kind 7), planned once per festive settlement and window
  struct FestSpan { int x0 = 0, x1 = 0, y = 0; uint32_t ca = 0, cb = 0, seed = 0; };
  std::vector<FestSpan> festSpans_;
  uint64_t festPlanKey_ = 0;
  void festivalPlan(Game& g, const Map& m, Vec2 cam);
  void drawFestPole(View& v, const FestSpan& s, bool right, Vec2 cam);
  // sound: the bard's tavern piece inside the gathering place and faintly outside its door at night, festival music on
  // the plaza, the crowd's murmur, animal voices, the alarm bell tolling through a night raid (update())
  Music lifeMusic(View& v, Game& g, Music want, float dt, bool& festive);
  float tollT_ = 0, critterVoiceT_ = 0, cheerT_ = 0;
  std::unordered_map<int, uint8_t> lastPosture_;   // Actor::id -> the posture it had (a toast's cheer, a bard's last chord)
  int bardSite_ = -1, bardBldg_ = -1;               // the gathering place a bard performs in tonight (-1 none)
  float bardScanT_ = 0;
  bool musicFestive_ = false;                       // the tavern piece playing is the festival's dance
  float levelHoldT_ = 0, heldGain_ = 1, heldMuffle_ = 0;   // the street bleed's level held through a crossfade away
  int alarmWas_ = -1;
  // HUD: the player's buff icons (Well Fed, Rested, Hungry, Weary)
  void drawBuffs(View& v, Game& g, float x, float y);
};
