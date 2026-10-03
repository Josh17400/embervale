// HOLDLINE presentation: bakes pixel-art terrain/sprites from code, draws the world, fog, HUD, menus.
#pragma once
#include <vector>
#include "engine/audio.h"
#include "engine/pix.h"
#include "game2/mg_game.h"

struct MParticle {
  Vec2 p, v;
  float life = 0, max = 1, grav = 0;
  Color c;
  int size = 1;
};

class MView {
 public:
  void init(Pix& pix, const MGame& g);
  void handleEvents(MGame& g, Audio& a);
  void update(MGame& g, Audio& a, float dt);
  void draw(MGame& g, Pix& pix, Vec2 mouse, int dragId, bool hasSave);

  Vec2 screenToWorld(Vec2 s) const { return s + Vec2((float)camX_, (float)camY_); }
  void snapCamera(const MGame& g);

 private:
  Tex terrain_;
  Tex tree_[3], bush_, boulder_, house_[3], inn_, tent_, fire_[2], chestShut_, chestOpen_;
  std::vector<uint32_t> miniPx_;
  float miniT_ = 0, clock_ = 0, shake_ = 0, hurtFlash_ = 0;
  Vec2 cam_;
  int camX_ = 0, camY_ = 0;
  bool camInit_ = false;
  std::vector<MParticle> parts_;
  Rng rng_{777};
  float shownHint_ = 0;

  void bakeTerrain(Pix& pix, const MGame& g);
  void bakeSprites(Pix& pix);
  void burst(Vec2 p, int n, float speed, float life, Color c, float grav = 0, int size = 1);
  bool lit(const MGame& g, Vec2 p, float scale = 1.0f) const;
  void drawUnit(Pix& pix, const MGame& g, const Unit& u, bool mergeable, bool lifted);
  void drawEnemy(Pix& pix, const Enemy& e);
  void drawHero(Pix& pix, const Hero& h);
  void drawFog(Pix& pix, const MGame& g);
  void drawHud(MGame& g, Pix& pix, Vec2 mouse, int dragId);
  void drawMenus(MGame& g, Pix& pix, bool hasSave);
  void updateMinimap(Pix& pix, const MGame& g);
};
