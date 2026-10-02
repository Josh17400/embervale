// Presentation layer: camera, particles, screen shake, HUD and menus. Reads Game, never mutates sim state.
#pragma once
#include <vector>
#include "engine/audio.h"
#include "engine/gfx.h"
#include "game/game.h"

struct Particle {
  Vec2 p, v;
  float life = 0, max = 1, size = 10, drag = 2.0f;
  Color c;
};

struct SaveData {
  float bestTime = 0;
  int bestKills = 0;
  int bestLoop = 0;
  int runs = 0;
};

inline void cardRect(int i, int n, float& x, float& y, float& w, float& h) {
  w = 500; h = 520; y = 330;
  float gap = 60;
  float total = n * w + (n - 1) * gap;
  x = (VIEW_W - total) * 0.5f + i * (w + gap);
}

class View {
 public:
  void handleEvents(Game& g, Audio& a);          // drain sim events -> particles / shake / sound
  void update(Game& g, Audio& a, float dt);      // camera, particles, decay
  void draw(Game& g, Gfx& gfx, float beat, Vec2 mouse);

  SaveData save;
  bool newBest = false;
  float hitstop = 0;                              // seconds the app should freeze the sim

 private:
  std::vector<Particle> parts_;
  float shake_ = 0, hurtFlash_ = 0, bossBanner_ = 0, clock_ = 0;
  Vec2 cam_;
  float zoom_ = 1.0f;
  bool camInit_ = false;
  Rng rng_{12345};
  float gemPitch_ = 1.0f, gemPitchT_ = 0;

  void burst(Vec2 p, int n, float speed, float life, Color c, float size);
  void drawWorld(Game& g, Gfx& gfx, float beat);
  void drawHud(Game& g, Gfx& gfx, Vec2 mouse);
  void drawMenus(Game& g, Gfx& gfx, Vec2 mouse);
  friend class ViewAccess;
};
