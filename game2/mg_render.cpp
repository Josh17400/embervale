#include "game2/mg_render.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace {
uint32_t hash32(int x, int y, uint32_t seed = 0) {
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u + 0x9E3779B9u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
Color C(int r, int g, int b, float a = 1.0f) { return Color(r / 255.0f, g / 255.0f, b / 255.0f, a); }

const char* roleBlurb(UState s) { return s == UState::Party ? "CLICK: LODGE AT INN" : "CLICK: JOIN PARTY"; }

uint32_t tileColor(Tile t) {
  switch (t) {
    case Tile::Water: return rgba(52, 116, 196);
    case Tile::Sand: return rgba(226, 206, 140);
    case Tile::Grass: return rgba(88, 168, 72);
    case Tile::Dirt: return rgba(156, 118, 74);
    case Tile::Rock: return rgba(120, 124, 134);
  }
  return 0;
}
}  // namespace

// ============================================================ baking
void MView::bakeTerrain(Pix& pix, const MGame& g) {
  Canvas c(MAP_W * TILE, MAP_H * TILE);
  auto isWater = [&](int tx, int ty) { return g.tileAt(tx, ty) == Tile::Water; };
  auto isRock = [&](int tx, int ty) { return g.tileAt(tx, ty) == Tile::Rock; };
  for (int ty = 0; ty < MAP_H; ty++)
    for (int tx = 0; tx < MAP_W; tx++) {
      Tile t = g.tileAt(tx, ty);
      bool wN = isWater(tx, ty - 1), wS = isWater(tx, ty + 1), wW = isWater(tx - 1, ty), wE = isWater(tx + 1, ty);
      for (int py = 0; py < TILE; py++)
        for (int px = 0; px < TILE; px++) {
          int X = tx * TILE + px, Y = ty * TILE + py;
          uint32_t h = hash32(X, Y, 3);
          uint32_t col = 0;
          // distance (px) to each tile side, used for shore blending
          int dN = py, dS = TILE - 1 - py, dW = px, dE = TILE - 1 - px;
          int shoreD = 99;
          if (wN) shoreD = std::min(shoreD, dN);
          if (wS) shoreD = std::min(shoreD, dS);
          if (wW) shoreD = std::min(shoreD, dW);
          if (wE) shoreD = std::min(shoreD, dE);
          switch (t) {
            case Tile::Grass: {
              uint32_t tone = hash32(X >> 2, Y >> 2, 11) % 10;
              col = tone < 5 ? rgba(88, 168, 72) : (tone < 8 ? rgba(78, 154, 66) : rgba(100, 180, 80));
              if (h % 29 == 0) col = rgba(66, 138, 58);
              else if (h % 67 == 0) col = rgba(124, 200, 96);
              if (hash32(X, Y, 9) % 700 == 0) { uint32_t f = h % 3; col = f == 0 ? rgba(250, 230, 90) : (f == 1 ? rgba(250, 250, 250) : rgba(240, 130, 190)); }
              if (shoreD < 2 || (shoreD < 4 && (h & 1))) col = rgba(224, 206, 142);
              break;
            }
            case Tile::Sand: {
              col = (h % 7 == 0) ? rgba(208, 186, 120) : ((h % 11 == 0) ? rgba(238, 222, 160) : rgba(226, 206, 140));
              if (shoreD < 2) col = rgba(196, 176, 112);
              break;
            }
            case Tile::Dirt: {
              col = (h % 11 == 0) ? rgba(134, 98, 60) : ((h % 17 == 0) ? rgba(178, 140, 94) : rgba(156, 118, 74));
              auto grassN = [&](int nx, int ny) { return g.tileAt(nx, ny) == Tile::Grass; };
              int e = 99;
              if (grassN(tx, ty - 1)) e = std::min(e, dN);
              if (grassN(tx, ty + 1)) e = std::min(e, dS);
              if (grassN(tx - 1, ty)) e = std::min(e, dW);
              if (grassN(tx + 1, ty)) e = std::min(e, dE);
              if (e < 2 && (h & 1)) col = rgba(96, 164, 70);
              if (shoreD < 2) col = rgba(196, 176, 112);
              break;
            }
            case Tile::Water: {
              col = rgba(46, 108, 188);
              uint32_t w = hash32(X >> 1, Y >> 2, 21) % 19;
              if (w == 0) col = rgba(84, 148, 220);
              else if (w == 1) col = rgba(40, 98, 176);
              bool landN = !wN, landS = !wS, landW = !wW, landE = !wE;
              int ld = 99;
              if (landN) ld = std::min(ld, dN);
              if (landS) ld = std::min(ld, dS);
              if (landW) ld = std::min(ld, dW);
              if (landE) ld = std::min(ld, dE);
              if (ld < 5) col = rgba(72, 148, 214);
              if (ld < 1 || (ld < 3 && (h % 3 == 0))) col = rgba(232, 244, 250);
              break;
            }
            case Tile::Rock: {
              col = (h % 5 == 0) ? rgba(104, 108, 120) : ((h % 9 == 0) ? rgba(136, 140, 152) : rgba(120, 124, 134));
              bool above = isRock(tx, ty - 1), below = isRock(tx, ty + 1);
              if (!above && dN < 3) col = rgba(158, 162, 174);
              if (!below && dS < 5) col = (px % 4 == 0) ? rgba(70, 72, 86) : rgba(88, 90, 104);
              if (!isRock(tx - 1, ty) && dW < 1) col = rgba(96, 98, 110);
              if (!isRock(tx + 1, ty) && dE < 1) col = rgba(96, 98, 110);
              break;
            }
          }
          c.set(X, Y, col);
        }
    }
  terrain_ = pix.bake(c);
}

void MView::bakeSprites(Pix& pix) {
  // ---- trees (24x32): oak, bushy oak, pine
  for (int v = 0; v < 3; v++) {
    Canvas c(24, 32);
    for (int dx = -8; dx <= 8; dx++) for (int dy = 0; dy <= 2; dy++) if (dx * dx / 70.0f + dy * dy / 3.0f < 1.0f) c.set(12 + dx, 29 + dy, rgba(20, 40, 20, 70));
    if (v < 2) {
      c.rect(10, 19, 4, 11, rgba(92, 60, 34));
      c.rect(10, 19, 1, 11, rgba(66, 42, 24));
      c.rect(13, 19, 1, 11, rgba(112, 76, 44));
      if (v == 1) { c.disc(7, 15, 6, rgba(24, 80, 42)); c.disc(17, 15, 6, rgba(24, 80, 42)); }
      c.disc(12, 12, v == 0 ? 10 : 9, rgba(24, 80, 42));
      c.disc(12, 12, v == 0 ? 9 : 8, rgba(36, 116, 54));
      if (v == 1) { c.disc(7, 15, 5, rgba(36, 116, 54)); c.disc(17, 15, 5, rgba(36, 116, 54)); }
      c.disc(10, 10, 6, rgba(54, 152, 68));
      c.disc(9, 8, 3, rgba(100, 194, 86));
      c.set(14, 15, rgba(24, 80, 42)); c.set(15, 13, rgba(24, 80, 42)); c.set(11, 16, rgba(24, 80, 42));
    } else {
      c.rect(11, 24, 3, 6, rgba(86, 56, 32));
      for (int tier = 0; tier < 4; tier++) {
        int y0 = 2 + tier * 6, w0 = 3 + tier * 3;
        for (int r = 0; r < 8; r++) {
          int half = std::min(w0 + 1, 1 + r * (w0 + 1) / 7);
          for (int x = -half; x <= half; x++) {
            uint32_t col = x < -half / 2 ? rgba(52, 132, 84) : (x > half / 2 ? rgba(24, 82, 60) : rgba(36, 106, 70));
            if (r == 0 || x == -half || x == half) col = rgba(20, 70, 52);
            c.set(12 + x, y0 + r, col);
          }
        }
      }
    }
    tree_[v] = pix.bake(c);
  }
  { Canvas c(12, 10);
    c.disc(6, 6, 4, rgba(30, 90, 46)); c.disc(6, 5, 3, rgba(50, 140, 64)); c.disc(5, 4, 1, rgba(110, 200, 90));
    c.set(8, 6, rgba(240, 110, 150)); c.set(4, 7, rgba(250, 230, 90));
    bush_ = pix.bake(c); }
  { Canvas c(14, 12);
    for (int dx = -5; dx <= 5; dx++) for (int dy = 0; dy <= 1; dy++) c.set(7 + dx, 10 + dy, rgba(20, 40, 20, 60));
    c.disc(7, 6, 5, rgba(76, 78, 90)); c.disc(7, 6, 4, rgba(122, 126, 138)); c.disc(5, 4, 2, rgba(162, 166, 178));
    c.rect(4, 9, 7, 1, rgba(70, 72, 84));
    boulder_ = pix.bake(c); }
  // ---- houses (32x32) in three roof colors, inn 48x44
  auto drawHouse = [&](Canvas& c, int w, int h, uint32_t roofA, uint32_t roofB, uint32_t wall, bool inn) {
    int roofH = inn ? 18 : 14;
    int wallY = roofH - 2;
    c.rect(1, wallY, w - 2, h - wallY, wall);
    c.rect(w - 8, wallY, 7, h - wallY, rgba((wall & 255) * 85 / 100, ((wall >> 8) & 255) * 85 / 100, ((wall >> 16) & 255) * 85 / 100));
    c.rect(1, h - 3, w - 2, 3, rgba(112, 106, 100));
    for (int y = 0; y < roofH; y++) {
      int half = 3 + y * (w / 2 - 2) / (roofH - 1);
      for (int x = -half; x <= half; x++) {
        uint32_t col = ((y / 3) % 2) ? roofA : roofB;
        if (y == roofH - 1 || x == -half || x == half) col = rgba(40, 28, 28);
        c.set(w / 2 + x, y, col);
      }
    }
    int dw = inn ? 8 : 6, dh = inn ? 14 : 11;
    c.rect(w / 2 - dw / 2, h - 3 - dh, dw, dh, rgba(98, 62, 34));
    c.rect(w / 2 - dw / 2 + 1, h - 3 - dh + 1, dw - 2, dh - 1, rgba(120, 78, 44));
    c.set(w / 2 + dw / 2 - 2, h - 3 - dh / 2, rgba(250, 220, 90));
    auto window = [&](int x, int y) {
      c.rect(x, y, 5, 5, rgba(64, 44, 34));
      c.rect(x + 1, y + 1, 3, 3, inn ? rgba(255, 224, 120) : rgba(150, 200, 235));
      c.set(x + 2, y + 1, rgba(255, 255, 255, 160));
    };
    window(inn ? 6 : 4, wallY + 4);
    window(inn ? w - 11 : w - 9, wallY + 4);
    if (inn) {
      c.rect(w - 6, wallY + 12, 1, 8, rgba(70, 48, 30));
      c.rect(w - 5, wallY + 12, 5, 4, rgba(132, 88, 48));
      c.rect(w - 4, wallY + 13, 3, 2, rgba(250, 210, 90));
      c.rect(w / 2 + 6, h - 14, 2, 2, rgba(255, 210, 90));
    }
    c.rect(w - (inn ? 14 : 9), 0, 4, inn ? 8 : 6, rgba(110, 100, 98));
  };
  const uint32_t roofs[3][2] = {{rgba(178, 74, 60), rgba(150, 56, 48)}, {rgba(86, 110, 168), rgba(68, 90, 144)}, {rgba(200, 150, 70), rgba(170, 120, 54)}};
  for (int i = 0; i < 3; i++) { Canvas c(32, 32); drawHouse(c, 32, 32, roofs[i][0], roofs[i][1], rgba(224, 198, 152), false); house_[i] = pix.bake(c); }
  { Canvas c(48, 44); drawHouse(c, 48, 44, rgba(214, 100, 50), rgba(184, 80, 40), rgba(232, 206, 160), true); inn_ = pix.bake(c); }
  // ---- goblin tent, campfire, chest
  { Canvas c(26, 20);
    for (int y = 0; y < 18; y++) {
      int half = 1 + y * 12 / 17;
      for (int x = -half; x <= half; x++) {
        uint32_t col = (x + y) % 6 < 3 ? rgba(140, 98, 64) : rgba(112, 76, 48);
        if (x == -half || x == half) col = rgba(60, 40, 28);
        c.set(13 + x, y + 1, col);
      }
    }
    c.rect(11, 10, 4, 9, rgba(36, 24, 20));
    c.rect(12, 0, 1, 3, rgba(90, 60, 40)); c.rect(13, 0, 4, 2, rgba(200, 50, 50));
    tent_ = pix.bake(c); }
  for (int f = 0; f < 2; f++) {
    Canvas c(10, 12);
    c.rect(1, 9, 8, 2, rgba(90, 58, 34)); c.rect(2, 8, 6, 1, rgba(120, 78, 44));
    if (f == 0) { c.rect(3, 4, 4, 5, rgba(240, 130, 40)); c.rect(4, 2, 2, 3, rgba(250, 200, 70)); c.rect(4, 6, 2, 3, rgba(255, 240, 150)); }
    else { c.rect(3, 5, 4, 4, rgba(240, 120, 36)); c.rect(5, 1, 2, 4, rgba(250, 190, 60)); c.rect(4, 6, 2, 3, rgba(255, 240, 150)); }
    fire_[f] = pix.bake(c);
  }
  for (int o = 0; o < 2; o++) {
    Canvas c(14, 12);
    c.rect(1, 5, 12, 6, rgba(122, 78, 40)); c.rect(1, 5, 12, 1, rgba(160, 110, 60));
    c.rect(1, 8, 12, 1, rgba(78, 48, 28)); c.rect(6, 5, 2, 6, rgba(246, 206, 80));
    if (!o) { c.rect(1, 2, 12, 3, rgba(140, 90, 46)); c.rect(6, 2, 2, 3, rgba(246, 206, 80)); }
    else { c.rect(2, 0, 10, 2, rgba(140, 90, 46)); c.rect(3, 5, 8, 2, rgba(255, 240, 140)); c.set(5, 3, rgba(255, 255, 255)); c.set(9, 4, rgba(255, 255, 255)); }
    (o ? chestOpen_ : chestShut_) = pix.bake(c);
  }
}

void MView::init(Pix& pix, const MGame& g) {
  bakeTerrain(pix, g);
  bakeSprites(pix);
  pix.miniInit(MAP_W, MAP_H);
  miniPx_.assign((size_t)MAP_W * MAP_H, 0);
}

// ============================================================ feel
void MView::burst(Vec2 p, int n, float speed, float life, Color c, float grav, int size) {
  if (parts_.size() > 2500) return;
  for (int i = 0; i < n; i++) {
    MParticle q;
    q.p = p;
    q.v = fromAngle(rng_.range(0, TAU)) * (speed * rng_.range(0.3f, 1.0f));
    q.life = q.max = life * rng_.range(0.6f, 1.0f);
    q.grav = grav; q.c = c; q.size = size;
    parts_.push_back(q);
  }
}

void MView::handleEvents(MGame& g, Audio& a) {
  int hits = 0, shots = 0;
  for (const Event& e : g.events) {
    switch (e.t) {
      case Ev::Recruit: a.play(Sfx::Select, 1.2f); burst(e.p + Vec2(0, -6), 14, 50, 0.7f, C(120, 255, 210), -30, 1); break;
      case Ev::Merge: {
        float pitch = 0.9f + 0.13f * (e.n - 1);
        a.play(Sfx::Merge, pitch);
        shake_ = std::max(shake_, 1.0f + e.n * 0.5f);
        burst(e.p + Vec2(0, -6), 18 + e.n * 5, 70 + e.n * 12, 0.7f, C(255, 214, 80), 60, 1);
        burst(e.p + Vec2(0, -6), 8 + e.n * 2, 40, 0.5f, C(255, 255, 255), 0, 2);
        break;
      }
      case Ev::Lucky:
        a.play(Sfx::Lucky);
        burst(e.p + Vec2(0, -6), 50, 110, 1.0f, C(90, 240, 255), 20, 2);
        break;
      case Ev::Hit: if (hits++ < 3) a.play(Sfx::Hit, 0.9f + rng_.f() * 0.4f, 0.8f); burst(e.p + Vec2(0, -6), 3, 40, 0.25f, C(255, 240, 220), 50, 1); break;
      case Ev::Kill: a.play(Sfx::Coin, 0.9f + rng_.f() * 0.3f); burst(e.p + Vec2(0, -4), 8, 50, 0.5f, C(130, 190, 80), 80, 1); burst(e.p + Vec2(0, -8), 3, 40, 0.6f, C(255, 214, 80), 40, 2); break;
      case Ev::Shot: if (shots++ < 2) a.play(Sfx::Shot, e.a == 2 ? 1.2f : 0.7f, 0.6f); break;
      case Ev::CampCleared: a.play(Sfx::WaveClear); shake_ = std::max(shake_, 2.0f); burst(e.p, 40, 90, 1.0f, C(255, 220, 120), 30, 2); break;
      case Ev::RaidWarn: a.play(Sfx::WaveStart); shake_ = std::max(shake_, 2.5f); break;
      case Ev::RaidEnd: a.play(Sfx::WaveClear); break;
      case Ev::VillageHit: a.play(Sfx::WallHit, 1.0f, 0.5f); shake_ = std::max(shake_, 1.5f); break;
      case Ev::HeroHurt: a.play(Sfx::Hurt, 1.0f, 0.5f); hurtFlash_ = 0.25f; shake_ = std::max(shake_, 1.5f); burst(e.p + Vec2(0, -6), 6, 60, 0.4f, C(255, 70, 70), 60, 1); break;
      case Ev::HeroDown: a.play(Sfx::Dead); break;
      case Ev::Chest: a.play(Sfx::Level); burst(e.p + Vec2(0, 14), 36, 80, 1.0f, C(255, 214, 80), 50, 2); break;
      case Ev::Discover: a.play(Sfx::Level); burst(e.p, 40, 70, 1.2f, C(255, 255, 255), -20, 1); break;
      case Ev::Lodge: case Ev::Join: a.play(Sfx::Select, 0.8f); break;
      case Ev::Hire: a.play(Sfx::Summon); a.play(Sfx::Coin); break;
      case Ev::Plundered: a.play(Sfx::Dead); shake_ = 5; break;
      case Ev::Denied: a.play(Sfx::Select, 0.5f, 0.5f); break;
    }
  }
  g.events.clear();
}

void MView::snapCamera(const MGame& g) {
  cam_ = g.hero.p - Vec2(Pix::W * 0.5f, Pix::H * 0.5f);
  camInit_ = false;
}

void MView::update(MGame& g, Audio& a, float dt) {
  clock_ += dt;
  if (shake_ > 0) shake_ = std::max(0.0f, shake_ - 10.0f * dt);
  if (hurtFlash_ > 0) hurtFlash_ -= dt;
  for (size_t i = 0; i < parts_.size();) {
    MParticle& p = parts_[i];
    p.life -= dt;
    if (p.life <= 0) { parts_[i] = parts_.back(); parts_.pop_back(); continue; }
    p.v.y += p.grav * dt;
    p.v *= std::max(0.0f, 1.0f - 1.5f * dt);
    p.p += p.v * dt;
    i++;
  }
  Vec2 target = g.hero.p - Vec2(Pix::W * 0.5f, Pix::H * 0.5f) + Vec2(0, 8);
  if (!camInit_) { cam_ = target; camInit_ = true; }
  cam_ += (target - cam_) * std::min(1.0f, 6.0f * dt);
  float maxX = MAP_W * TILE - Pix::W, maxY = MAP_H * TILE - Pix::H;
  Vec2 c(clampf(cam_.x, 0, maxX), clampf(cam_.y, 0, maxY));
  float sx = shake_ > 0 ? rng_.range(-shake_, shake_) : 0, sy = shake_ > 0 ? rng_.range(-shake_, shake_) : 0;
  camX_ = (int)std::floor(clampf(c.x + sx, 0, maxX));
  camY_ = (int)std::floor(clampf(c.y + sy, 0, maxY));

  int near = 0;
  for (const Enemy& e : g.enemies) if (len2(e.p - g.hero.p) < 160.0f * 160.0f) near++;
  float intensity = clampf(0.18f + near * 0.07f + (g.raidActive ? 0.25f : 0.0f), 0, 1);
  a.setIntensity(g.mode == MMode::Title ? 0.12f : intensity);
  shownHint_ += dt;
}

bool MView::lit(const MGame& g, Vec2 p, float scale) const {
  if (len(p - g.hero.p) < 9.5f * TILE * scale) return true;
  for (const Village& v : g.villages) if (v.discovered && len(p - v.p) < 13.0f * TILE * scale) return true;
  return false;
}

void MView::updateMinimap(Pix& pix, const MGame& g) {
  for (int y = 0; y < MAP_H; y++)
    for (int x = 0; x < MAP_W; x++) {
      uint32_t col = rgba(8, 10, 16, 230);
      if (g.explored[y * MAP_W + x]) {
        Tile t = g.tileAt(x, y);
        col = tileColor(t);
        Obj o = g.objAt(x, y);
        if (o == Obj::Tree) col = rgba(40, 110, 56);
      }
      miniPx_[(size_t)y * MAP_W + x] = col;
    }
  for (const Village& v : g.villages) {
    int tx = (int)(v.p.x / TILE), ty = (int)(v.p.y / TILE);
    if (!v.discovered) continue;
    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) if (MGame::inMap(tx + dx, ty + dy)) miniPx_[(size_t)(ty + dy) * MAP_W + tx + dx] = rgba(255, 255, 255);
  }
  for (const Camp& c : g.camps) {
    int tx = (int)(c.p.x / TILE), ty = (int)(c.p.y / TILE);
    if (!g.explored[ty * MAP_W + tx]) continue;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) miniPx_[(size_t)(ty + dy) * MAP_W + tx + dx] = c.cleared ? rgba(120, 200, 120) : rgba(255, 60, 50);
  }
  pix.miniUpdate(miniPx_.data());
}

// ============================================================ characters
static void shadow(Pix& pix, float x, float y, int w = 8) { pix.rect(x - w / 2.0f, y - 1, (float)w, 2, C(10, 20, 10, 0.35f)); }

void MView::drawHero(Pix& pix, const Hero& h) {
  float x = h.p.x - camX_, y = h.p.y - camY_;
  if (h.dead) { pix.rect(x - 5, y - 4, 10, 4, C(60, 120, 220, 0.6f)); return; }
  shadow(pix, x, y);
  int step = h.moving ? (int)(h.anim) & 1 : 0;
  float bob = h.moving ? (float)step : 0;
  float f = h.face;
  bool flash = h.hurt > 0;
  Color skin = flash ? C(255, 255, 255) : C(240, 200, 160), tunic = flash ? C(255, 200, 200) : C(60, 120, 220), hair = C(96, 62, 40), boots = C(70, 50, 40);
  pix.rect(x - 3, y - 3 - step * 0, 2, 3, boots);                 // legs
  pix.rect(x + 1, y - 3 + step * 0, 2, 3, boots);
  if (step) { pix.rect(x - 3, y - 3, 2, 2, boots); }
  pix.rect(x - 4, y - 9 - bob, 8, 6, tunic);                       // torso
  pix.rect(x - 4, y - 5 - bob, 8, 1, C(250, 214, 90));            // belt
  pix.rect(x - 3 - f * 3, y - 9 - bob, 2, 7, C(200, 50, 60));      // cape
  pix.rect(x - 3, y - 14 - bob, 6, 5, skin);                       // head
  pix.rect(x - 3, y - 15 - bob, 6, 2, hair);
  pix.rect(x + (f > 0 ? 1 : -2), y - 12 - bob, 1, 1, C(30, 30, 40));
  // sword
  float sx = x + f * 5, swing = h.cd > 0.2f ? -3.0f : 0.0f;
  pix.rect(sx, y - 11 - bob + swing, 1, 5, C(220, 230, 240));
  pix.rect(sx - 1, y - 7 - bob + swing, 3, 1, C(250, 214, 90));
}

void MView::drawEnemy(Pix& pix, const Enemy& e) {
  float x = e.p.x - camX_, y = e.p.y - camY_;
  bool big = e.kind == 1;
  shadow(pix, x, y, big ? 12 : 8);
  int step = e.moving ? (int)(e.anim) & 1 : 0;
  bool fl = e.flash > 0;
  Color skin = fl ? C(255, 255, 255) : (big ? C(96, 140, 90) : C(112, 172, 72));
  Color cloth = fl ? C(255, 220, 220) : (big ? C(110, 60, 50) : C(130, 90, 56));
  float s = big ? 1.5f : 1.0f;
  pix.rect(x - 3 * s, y - 3, 2 * s, 3, cloth);
  pix.rect(x + 1 * s, y - 3 + (step ? -1 : 0), 2 * s, 3, cloth);
  pix.rect(x - 4 * s, y - 8 * s - step, 8 * s, 6 * s, skin);
  pix.rect(x - 4 * s, y - 5 * s - step, 8 * s, 2 * s, cloth);
  pix.rect(x - 4 * s, y - 13 * s - step, 8 * s, 5 * s, skin);
  pix.rect(x - 6 * s, y - 12 * s - step, 2 * s, 2 * s, skin);       // ears
  pix.rect(x + 4 * s, y - 12 * s - step, 2 * s, 2 * s, skin);
  pix.rect(x + e.face * 1, y - 11 * s - step, 2, 2, C(255, 40, 40));
  pix.rect(x + e.face * 5 * s, y - 9 * s - step, 2, 6 * s, C(110, 70, 40));    // club
  if (big) pix.rect(x + e.face * 5 * s - 1, y - 12 * s - step, 4, 3, C(90, 60, 36));
  if (e.hp < e.maxhp) { pix.rect(x - 6, y - 17 * s - 2, 12, 2, C(40, 10, 10)); pix.rect(x - 6, y - 17 * s - 2, 12 * clampf(e.hp / e.maxhp, 0, 1), 2, C(230, 60, 50)); }
}

void MView::drawUnit(Pix& pix, const MGame& g, const Unit& u, bool mergeable, bool lifted) {
  float x = u.p.x - camX_, y = u.p.y - camY_ - (lifted ? 7 : 0);
  Color main, trim;
  switch (u.sp) {
    case Species::Knight: main = C(150, 172, 206); trim = C(250, 214, 90); break;
    case Species::Archer: main = C(70, 156, 84); trim = C(196, 150, 80); break;
    case Species::Mage: main = C(148, 84, 204); trim = C(250, 220, 120); break;
    default: break;
  }
  if (u.fallen > 0) {
    pix.rect(x - 5, y - 3, 10, 3, main.withA(0.55f));
    pix.text(x, y - 12 + std::sin(clock_ * 3) * 1.5f, "Z", 1, C(200, 220, 255, 0.8f), 1);
    return;
  }
  if (!lifted) shadow(pix, x, y);
  else pix.rect(x - 4, u.p.y - camY_ - 1, 8, 2, C(10, 20, 10, 0.3f));
  float f = u.face;
  int step = u.moving ? (int)(u.anim) & 1 : 0;
  float bob = u.moving ? (float)step : 0;
  bool fl = u.flash > 0;
  if (fl) { main = C(255, 255, 255); }
  Color skin = C(240, 200, 160), boots = C(70, 50, 40);
  // tier aura: lv4+ pulses gold, lv6 adds a halo ring of sparkles
  if (u.lv >= 4) {
    float p = 0.5f + 0.5f * std::sin(clock_ * 5);
    pix.rectAdd(x - 7, y - 16 - bob, 14, 17, C(255, 200, 60, 0.10f + 0.10f * p));
  }
  pix.rect(x - 3, y - 3, 2, 3, boots);
  pix.rect(x + 1, y - 3, 2, 3, boots);
  float s = 1.0f + (u.lv >= 5 ? 0.0f : 0.0f);
  switch (u.sp) {
    case Species::Knight:
      pix.rect(x - 4, y - 9 - bob, 8, 6, main);
      pix.rect(x - 4, y - 9 - bob, 8, 1, trim);
      pix.rect(x - 4, y - 14 - bob, 8, 5, C(170, 180, 200));              // helmet
      pix.rect(x - 4, y - 14 - bob, 8, 1, trim);
      pix.rect(x - 2 + (f > 0 ? 1 : 0), y - 12 - bob, 3, 1, C(30, 30, 40));
      pix.rect(x - f * 6 - 2, y - 9 - bob, 4, 6, C(200, 60, 60));          // shield
      pix.rect(x - f * 6 - 2, y - 9 - bob, 4, 1, trim);
      pix.rect(x + f * 5, y - 12 - bob, 1, 7, C(224, 232, 240));            // sword
      if (u.lv >= 3) pix.rect(x - 1, y - 17 - bob, 2, 3, C(230, 60, 60));   // plume
      break;
    case Species::Archer:
      pix.rect(x - 4, y - 9 - bob, 8, 6, main);
      pix.rect(x - 4, y - 5 - bob, 8, 1, trim);
      pix.rect(x - 4, y - 14 - bob, 8, 5, main.scaled(0.8f));              // hood
      pix.rect(x - 3, y - 13 - bob, 6, 4, skin);
      pix.rect(x + (f > 0 ? 1 : -2), y - 12 - bob, 1, 1, C(30, 30, 40));
      pix.rect(x + f * 5, y - 13 - bob, 1, 9, C(120, 80, 44));              // bow
      pix.rect(x + f * 4, y - 13 - bob, 1, 1, C(120, 80, 44)); pix.rect(x + f * 4, y - 5 - bob, 1, 1, C(120, 80, 44));
      pix.rect(x - f * 4, y - 9 - bob, 2, 5, C(150, 100, 56));              // quiver
      if (u.lv >= 3) pix.rect(x - 1, y - 16 - bob, 2, 2, C(230, 230, 90));  // feather
      break;
    case Species::Mage:
      pix.rect(x - 4, y - 9 - bob, 8, 7, main);
      pix.rect(x - 4, y - 3, 8, 1, trim);
      pix.rect(x - 3, y - 13 - bob, 6, 4, skin);
      pix.rect(x - 5, y - 14 - bob, 10, 2, main.scaled(0.8f));              // hat brim
      pix.rect(x - 3, y - 18 - bob, 6, 4, main.scaled(0.8f));
      pix.rect(x - 1, y - 21 - bob, 3, 3, main.scaled(0.8f));
      pix.rect(x + (f > 0 ? 1 : -2), y - 12 - bob, 1, 1, C(30, 30, 40));
      pix.rect(x + f * 5, y - 14 - bob, 1, 12, C(120, 80, 44));             // staff
      pix.rect(x + f * 5 - 1, y - 16 - bob, 3, 3, C(120, 230, 255));
      pix.rectAdd(x + f * 5 - 3, y - 18 - bob, 7, 7, C(90, 200, 255, 0.18f + 0.08f * std::sin(clock_ * 6)));
      break;
    default: break;
  }
  (void)s; (void)g;
  // level badge
  if (u.lv >= 2) {
    Color bc = u.lv >= 5 ? C(255, 214, 80) : (u.lv >= 3 ? C(120, 230, 255) : C(235, 235, 245));
    std::string n = std::to_string(u.lv);
    float by = y - (u.sp == Species::Mage ? 30 : 24) - bob;
    pix.rect(x - 3, by - 1, 7, 9, C(10, 12, 20, 0.75f));
    pix.text(x - 2, by, n, 1, bc);
  }
  if (u.lv >= 6) {   // crown
    pix.rect(x - 3, y - (u.sp == Species::Mage ? 34 : 28) - bob, 7, 2, C(255, 214, 80));
    pix.rect(x - 3, y - (u.sp == Species::Mage ? 36 : 30) - bob, 1, 2, C(255, 214, 80));
    pix.rect(x, y - (u.sp == Species::Mage ? 36 : 30) - bob, 1, 2, C(255, 214, 80));
    pix.rect(x + 3, y - (u.sp == Species::Mage ? 36 : 30) - bob, 1, 2, C(255, 214, 80));
  }
  if (mergeable) {
    float b = std::sin(clock_ * 6 + u.id) * 1.5f;
    float py = y - (u.sp == Species::Mage ? 38 : 32) - bob + b - (u.lv >= 6 ? 3 : 0);
    pix.rect(x - 1, py, 3, 1, C(255, 240, 90)); pix.rect(x, py - 1, 1, 3, C(255, 240, 90));
  }
  if (u.hp < u.maxhp) { pix.rect(x - 6, y + 2, 12, 2, C(10, 30, 10)); pix.rect(x - 6, y + 2, 12 * clampf(u.hp / u.maxhp, 0, 1), 2, C(90, 220, 90)); }
}

// ============================================================ fog
void MView::drawFog(Pix& pix, const MGame& g) {
  int tx0 = std::max(0, camX_ / TILE), ty0 = std::max(0, camY_ / TILE);
  int tx1 = std::min(MAP_W - 1, (camX_ + Pix::W) / TILE), ty1 = std::min(MAP_H - 1, (camY_ + Pix::H) / TILE);
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      float x = (float)(tx * TILE - camX_), y = (float)(ty * TILE - camY_);
      if (!g.explored[ty * MAP_W + tx]) {
        bool edge = false;
        for (int dy = -1; dy <= 1 && !edge; dy++) for (int dx = -1; dx <= 1; dx++) {
          int nx = tx + dx, ny = ty + dy;
          if (MGame::inMap(nx, ny) && g.explored[ny * MAP_W + nx]) { edge = true; break; }
        }
        pix.rect(x, y, TILE, TILE, C(8, 10, 18, edge ? 0.80f : 1.0f));
      } else {
        Vec2 c((tx + 0.5f) * TILE, (ty + 0.5f) * TILE);
        float a = 0.0f;
        if (!lit(g, c, 1.0f)) a = 0.42f;            // explored but out of sight: dimmed
        else if (!lit(g, c, 0.86f)) a = 0.18f;      // soft rim around the lit area
        if (a > 0) pix.rect(x, y, TILE, TILE, C(10, 14, 36, a));
      }
    }
}

// ============================================================ HUD + menus
void MView::drawHud(MGame& g, Pix& pix, Vec2 mouse, int dragId) {
  const float W = Pix::W;
  // top-left panel
  pix.rect(4, 4, 134, 34, C(10, 12, 24, 0.72f));
  pix.frame(4, 4, 134, 34, C(90, 110, 150, 0.8f));
  pix.rect(9, 9, 5, 5, C(230, 60, 70)); pix.rect(10, 8, 3, 1, C(230, 60, 70));
  pix.rect(18, 9, 114, 6, C(40, 12, 16));
  pix.rect(18, 9, 114 * clampf(g.hero.hp / g.hero.maxhp, 0, 1), 6, C(230, 64, 74));
  pix.rect(9, 19, 6, 6, C(250, 204, 60)); pix.rect(10, 20, 4, 4, C(255, 232, 120));
  pix.text(19, 19, std::to_string(g.gold), 1, C(255, 232, 140));
  pix.text(72, 19, "UNITS " + std::to_string(g.units.size()), 1, C(190, 210, 240));
  pix.text(9, 29, "PARTY " + std::to_string(g.partyCount()) + "/" + std::to_string(MAX_PARTY), 1, C(150, 230, 200));
  int cleared = 0; for (const Camp& c : g.camps) cleared += c.cleared;
  pix.text(72, 29, "CAMPS " + std::to_string(cleared) + "/" + std::to_string(g.camps.size()), 1, C(255, 170, 150));

  // minimap
  pix.rect(W - 120, 4, 116, 76, C(8, 10, 18, 0.85f));
  pix.miniDraw(W - 118, 6, 0.5f);
  pix.frame(W - 120, 4, 116, 76, C(90, 110, 150, 0.9f));
  {
    float mx = W - 118 + g.hero.p.x / TILE * 0.5f, my = 6 + g.hero.p.y / TILE * 0.5f;
    if (std::fmod(clock_, 0.6f) < 0.4f) pix.rect(mx - 1, my - 1, 3, 3, C(255, 255, 80));
    for (const Enemy& e : g.enemies) {
      if (e.camp >= 0 || !g.explored[(int)(e.p.y / TILE) * MAP_W + (int)(e.p.x / TILE)]) continue;
      pix.rect(W - 118 + e.p.x / TILE * 0.5f, 6 + e.p.y / TILE * 0.5f, 2, 2, C(255, 60, 60));
    }
  }

  // banner
  if (g.bannerT > 0) {
    float a = clampf(g.bannerT, 0, 1);
    int tw = pix.textW(g.banner, 1);
    pix.rect(W * 0.5f - tw * 0.5f - 6, 44, (float)tw + 12, 13, C(10, 12, 24, 0.8f * a));
    pix.frame(W * 0.5f - tw * 0.5f - 6, 44, (float)tw + 12, 13, C(255, 214, 90, a));
    pix.text(W * 0.5f, 48, g.banner, 1, C(255, 240, 200, a), 1);
  }
  // raid warning marker toward the origin
  if (g.raidBanner > 0 || g.raidActive) {
    Vec2 sp = g.raidFrom - Vec2((float)camX_, (float)camY_);
    bool on = sp.x > 6 && sp.x < Pix::W - 6 && sp.y > 6 && sp.y < Pix::H - 6;
    if (!on && std::fmod(clock_, 0.7f) < 0.45f) {
      Vec2 c(Pix::W * 0.5f, Pix::H * 0.5f), d = sp - c;
      float k = std::min((Pix::W * 0.5f - 14) / std::max(1.0f, std::fabs(d.x)), (Pix::H * 0.5f - 14) / std::max(1.0f, std::fabs(d.y)));
      Vec2 e = c + d * k;
      if (e.x < 142 && e.y < 44) e.y = 44;     // keep clear of the HUD panel
      pix.rect(e.x - 3, e.y - 3, 7, 7, C(255, 50, 50));
      pix.rect(e.x - 1, e.y - 1, 3, 3, C(255, 220, 220));
      pix.text(e.x, e.y + 6, "RAID", 1, C(255, 120, 110), 1);
    }
  }
  // village HP when under attack
  if (g.villages[0].hp < g.villages[0].maxhp) {
    float hp = g.villages[0].hp / g.villages[0].maxhp;
    pix.rect(W * 0.5f - 40, 62, 80, 5, C(40, 12, 16, 0.9f));
    pix.rect(W * 0.5f - 40, 62, 80 * clampf(hp, 0, 1), 5, C(255, 160, 60));
    pix.text(W * 0.5f, 69, "OAKHOLLOW", 1, C(255, 210, 160), 1);
  }
  // contextual prompt near an inn
  int v = g.nearVillage(g.hero.p);
  if (v >= 0 && !g.hero.dead) {
    std::string s = "E: LODGE PARTY";
    if (v == 0) s += "   B: HIRE (" + std::to_string(g.hireCost()) + "G)";
    pix.rect(W * 0.5f - pix.textW(s, 1) * 0.5f - 4, Pix::H - 46, (float)pix.textW(s, 1) + 8, 12, C(10, 12, 24, 0.75f));
    pix.text(W * 0.5f, Pix::H - 43, s, 1, C(255, 240, 180), 1);
  }
  // controls hint
  if (shownHint_ < 40.0f) {
    float a = clampf((40.0f - shownHint_) / 5.0f, 0, 1);
    pix.text(6, Pix::H - 22, "WASD MOVE   DRAG A UNIT ONTO A MATCH TO MERGE (+)", 1, C(220, 230, 255, 0.85f * a));
    pix.text(6, Pix::H - 12, "M MERGE ALL   CLICK UNIT: LODGE/JOIN   ESC PAUSE", 1, C(220, 230, 255, 0.85f * a));
  }
  // tooltip
  if (dragId < 0) {
    Vec2 mw = screenToWorld(mouse);
    int id = g.unitAt(mw);
    if (id >= 0) {
      const Unit* u = g.unitById(id);
      std::string a = "LV" + std::to_string(u->lv) + " " + MGame::speciesName(u->sp);
      std::string b = "HP " + std::to_string((int)u->hp) + "/" + std::to_string((int)u->maxhp) + "  DMG " + std::to_string((int)MGame::unitDmg(u->sp, u->lv));
      std::string c = roleBlurb(u->st);
      int w = std::max(pix.textW(a, 1), std::max(pix.textW(b, 1), pix.textW(c, 1))) + 8;
      float x = std::min(mouse.x + 8, (float)Pix::W - w - 2), y = std::min(mouse.y + 8, (float)Pix::H - 34);
      pix.rect(x, y, (float)w, 32, C(10, 12, 24, 0.9f));
      pix.frame(x, y, (float)w, 32, C(120, 140, 190));
      pix.text(x + 4, y + 4, a, 1, C(255, 232, 140));
      pix.text(x + 4, y + 14, b, 1, C(200, 220, 255));
      pix.text(x + 4, y + 23, c, 1, C(150, 230, 200));
    }
  }
  if (hurtFlash_ > 0) pix.rect(0, 0, Pix::W, Pix::H, C(255, 0, 0, 0.18f * (hurtFlash_ / 0.25f)));
  if (g.hero.dead) {
    pix.rect(0, 0, Pix::W, Pix::H, C(0, 0, 0, 0.4f));
    pix.text(W * 0.5f, 120, "KNOCKED OUT", 3, C(255, 120, 120), 1);
  }
}

void MView::drawMenus(MGame& g, Pix& pix, bool hasSave) {
  const float W = Pix::W;
  if (g.mode == MMode::Title) {
    pix.rect(0, 0, W, Pix::H, C(6, 8, 20, 0.78f));
    float bob = std::sin(clock_ * 2) * 2;
    pix.text(W * 0.5f + 2, 52 + bob + 2, "HOLDLINE", 7, C(40, 20, 10), 1);
    pix.text(W * 0.5f, 52 + bob, "HOLDLINE", 7, C(255, 214, 90), 1);
    pix.text(W * 0.5f, 112, "FIND ALLIES. MERGE THEM. HOLD THE LINE.", 1, C(220, 235, 255), 1);
    float p = 0.6f + 0.4f * std::sin(clock_ * 4);
    pix.text(W * 0.5f, 150, hasSave ? "SPACE: CONTINUE" : "SPACE: BEGIN", 2, C(255, 240, 160, p), 1);
    if (hasSave) pix.text(W * 0.5f, 172, "N: NEW WORLD", 1, C(200, 215, 240), 1);
    pix.text(W * 0.5f, 206, "WASD MOVE   MOUSE: DRAG UNITS TO MERGE   M MERGE ALL", 1, C(170, 190, 225), 1);
    pix.text(W * 0.5f, 218, "E LODGE AT INN   B HIRE   ESC PAUSE   F11 FULLSCREEN", 1, C(170, 190, 225), 1);
  } else if (g.mode == MMode::Paused) {
    pix.rect(0, 0, W, Pix::H, C(6, 8, 20, 0.6f));
    pix.text(W * 0.5f, 100, "PAUSED", 4, C(255, 255, 255), 1);
    pix.text(W * 0.5f, 140, "ESC: RESUME", 1, C(210, 225, 255), 1);
  }
}

// ============================================================ world draw
struct DrawItem { float y; int type; int a; int b; };

void MView::draw(MGame& g, Pix& pix, Vec2 mouse, int dragId, bool hasSave) {
  pix.begin(C(8, 10, 18));
  if (miniT_ <= 0 || g.exploredDirty) { updateMinimap(pix, g); g.exploredDirty = false; miniT_ = 0.5f; }
  miniT_ -= 1.0f / 60.0f;

  pix.blitRegion(terrain_, camX_, camY_, Pix::W, Pix::H, 0, 0);

  std::vector<DrawItem> items;
  items.reserve(400);
  int tx0 = std::max(0, camX_ / TILE - 1), ty0 = std::max(0, camY_ / TILE - 1);
  int tx1 = std::min(MAP_W - 1, (camX_ + Pix::W) / TILE + 1), ty1 = std::min(MAP_H - 1, (camY_ + Pix::H) / TILE + 3);
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      Obj o = g.objAt(tx, ty);
      if (o != Obj::None && g.explored[ty * MAP_W + tx]) items.push_back({(ty + 1) * (float)TILE - 1, 0, tx, ty});
    }
  for (size_t i = 0; i < g.villages.size(); i++) {
    const Village& v = g.villages[i];
    if (len(v.p - g.hero.p) > 400) continue;
    items.push_back({v.p.y - 12, 1, (int)i, 0});       // inn
    items.push_back({v.p.y - 16, 1, (int)i, 1});       // house L
    items.push_back({v.p.y - 20, 1, (int)i, 2});       // house R
    items.push_back({v.p.y + 54, 1, (int)i, 3});       // house S
  }
  for (size_t i = 0; i < g.camps.size(); i++) {
    const Camp& c = g.camps[i];
    if (std::fabs(c.p.x - g.hero.p.x) > 360 || std::fabs(c.p.y - g.hero.p.y) > 260) continue;
    if (!g.explored[(int)(c.p.y / TILE) * MAP_W + (int)(c.p.x / TILE)]) continue;
    items.push_back({c.p.y + 6, 2, (int)i, 0});
    if (c.cleared && !c.chestTaken) items.push_back({c.p.y + 22, 3, (int)i, 0});
  }
  for (size_t i = 0; i < g.units.size(); i++) items.push_back({g.units[i].p.y, 4, (int)i, 0});
  for (size_t i = 0; i < g.enemies.size(); i++) {
    const Enemy& e = g.enemies[i];
    if (!g.explored[(int)(e.p.y / TILE) * MAP_W + (int)(e.p.x / TILE)]) continue;
    items.push_back({e.p.y, 5, (int)i, 0});
  }
  items.push_back({g.hero.p.y, 6, 0, 0});
  std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) { return a.y < b.y; });

  // which units have a merge partner
  std::vector<uint8_t> canMerge(g.units.size(), 0);
  for (size_t i = 0; i < g.units.size(); i++) {
    if (g.units[i].st == UState::Wild || g.units[i].lv >= MAX_LV) continue;
    for (size_t j = i + 1; j < g.units.size(); j++)
      if (g.units[j].st != UState::Wild && g.units[j].sp == g.units[i].sp && g.units[j].lv == g.units[i].lv) { canMerge[i] = canMerge[j] = 1; }
  }
  const Unit* dragged = dragId >= 0 ? g.unitById(dragId) : nullptr;

  for (const DrawItem& it : items) {
    switch (it.type) {
      case 0: {
        Obj o = g.objAt(it.a, it.b);
        float fx = it.a * TILE + 8.0f - camX_, fy = it.b * TILE + 15.0f - camY_;
        uint32_t h = hash32(it.a, it.b, 4);
        if (o == Obj::Tree) pix.blit(tree_[h % 3], fx - 12, fy - 30, (h >> 8) & 1);
        else if (o == Obj::Bush) pix.blit(bush_, fx - 6, fy - 9);
        else pix.blit(boulder_, fx - 7, fy - 11);
        break;
      }
      case 1: {
        const Village& v = g.villages[it.a];
        float px = v.p.x - camX_, py = v.p.y - camY_;
        if (it.b == 0) {
          pix.blit(inn_, px - 24, py - 12 + 2 - 44);
          pix.text(px + 1, py - 65, v.name, 1, C(30, 20, 10), 1);
          pix.text(px, py - 66, v.name, 1, C(255, 232, 160), 1);
        } else if (it.b == 1) pix.blit(house_[0], px - 88, py - 16 + 2 - 32);
        else if (it.b == 2) pix.blit(house_[1], px + 56, py - 20 + 2 - 32);
        else pix.blit(house_[2], px - 16, py + 54 + 2 - 32);
        break;
      }
      case 2: {
        const Camp& c = g.camps[it.a];
        float cx = c.p.x - camX_, cy = c.p.y - camY_;
        if (!c.cleared) {
          pix.blit(tent_, cx - 38, cy - 22);
          pix.blit(tent_, cx + 12, cy - 18, true);
          pix.blit(fire_[(int)(clock_ * 6) & 1], cx - 5, cy - 8);
          pix.rectAdd(cx - 14, cy - 18, 28, 24, C(255, 140, 40, 0.07f));
        } else {
          pix.rect(cx - 4, cy - 1, 8, 3, C(50, 44, 40));
        }
        break;
      }
      case 3: {
        const Camp& c = g.camps[it.a];
        pix.blit(chestShut_, c.p.x - camX_ - 7, c.p.y - camY_ + 12);
        float p = 0.5f + 0.5f * std::sin(clock_ * 5);
        pix.rectAdd(c.p.x - camX_ - 9, c.p.y - camY_ + 10, 18, 14, C(255, 220, 90, 0.10f + 0.10f * p));
        break;
      }
      case 4: {
        const Unit& u = g.units[it.a];
        if (u.st == UState::Wild) {
          float x = u.p.x - camX_, y = u.p.y - camY_;
          if (!g.explored[(int)(u.p.y / TILE) * MAP_W + (int)(u.p.x / TILE)]) break;
          // sleeping, waiting to be found: draw a dim silhouette + beacon
          Unit tmp = u; tmp.moving = false; tmp.fallen = 0;
          drawUnit(pix, g, tmp, false, false);
          float b = std::sin(clock_ * 3 + u.id) * 1.5f;
          pix.text(x, y - 36 + b, "!", 1, C(255, 240, 120), 1);
          pix.text(x + 5, y - 19, "Z", 1, C(200, 220, 255, 0.8f));
          pix.rectAdd(x - 8, y - 18, 16, 20, C(120, 255, 220, 0.06f + 0.05f * std::sin(clock_ * 3)));
          break;
        }
        if (dragged && dragged->id == u.id) break;
        drawUnit(pix, g, u, canMerge[it.a] != 0, false);
        break;
      }
      case 5: drawEnemy(pix, g.enemies[it.a]); break;
      case 6: drawHero(pix, g.hero); break;
    }
  }

  // projectiles
  for (const Proj& p : g.projs) {
    float x = p.p.x - camX_, y = p.p.y - camY_;
    if (p.arrow) { pix.rect(x - 2, y, 5, 1, C(150, 100, 56)); pix.rect(x + 2, y, 2, 1, C(220, 230, 240)); }
    else { pix.rect(x - 1, y - 1, 3, 3, C(200, 120, 255)); pix.rectAdd(x - 4, y - 4, 9, 9, C(180, 100, 255, 0.25f)); }
  }
  // particles
  for (const MParticle& p : parts_) {
    float k = p.life / p.max;
    pix.rect(p.p.x - camX_, p.p.y - camY_, (float)p.size, (float)p.size, p.c.withA(clampf(k * 1.6f, 0, 1)));
  }

  drawFog(pix, g);

  // dragged unit on top + merge target highlights
  if (dragged) {
    for (size_t i = 0; i < g.units.size(); i++) {
      const Unit& u = g.units[i];
      if (u.id == dragged->id || u.st == UState::Wild) continue;
      if (u.sp == dragged->sp && u.lv == dragged->lv && u.lv < MAX_LV) {
        float x = u.p.x - camX_, y = u.p.y - camY_;
        float p = 0.6f + 0.4f * std::sin(clock_ * 10);
        pix.frame(x - 9, y - 22, 18, 26, C(255, 240, 90, p));
        pix.frame(x - 10, y - 23, 20, 28, C(255, 200, 60, p * 0.6f));
      }
    }
    Unit d = *dragged;
    d.p = screenToWorld(mouse) + Vec2(0, 6);
    d.moving = false;
    drawUnit(pix, g, d, false, true);
  }

  // floating text
  for (const Pop& p : g.pops) {
    float k = p.t / 1.2f;
    float x = p.p.x - camX_, y = p.p.y - camY_ - p.t * 14;
    Color c(((p.color) & 255) / 255.0f, ((p.color >> 8) & 255) / 255.0f, ((p.color >> 16) & 255) / 255.0f, clampf(2.0f - k * 2.0f, 0, 1));
    pix.text(x + 1, y + 1, p.text, 1, C(0, 0, 0, c.a * 0.8f), 1);
    pix.text(x, y, p.text, 1, c, 1);
  }

  if (g.mode != MMode::Title) drawHud(g, pix, mouse, dragId);
  drawMenus(g, pix, hasSave);
  // note: caller presents (so screenshots can read back first)
}
