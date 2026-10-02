#include "game/render.h"
#include <cstdio>
#include <string>

namespace {
const Color kCyan(0.35f, 0.95f, 1.0f);
const Color kMagenta(1.0f, 0.25f, 0.75f);
const Color kLime(0.55f, 1.0f, 0.4f);
const Color kGold(1.0f, 0.85f, 0.35f);

Color enemyColor(EType t) {
  switch (t) {
    case EType::Drifter: return {1.0f, 0.35f, 0.3f};
    case EType::Charger: return {1.0f, 0.8f, 0.2f};
    case EType::Splitter: return {0.4f, 1.0f, 0.5f};
    case EType::Shooter: return {0.7f, 0.4f, 1.0f};
    case EType::Boss: return {1.0f, 0.25f, 0.7f};
  }
  return {1, 1, 1};
}

std::string fmtTime(float t) {
  int s = (int)t;
  char b[16];
  std::snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
  return b;
}

// Greedy word wrap to maxChars per line.
std::vector<std::string> wrap(const std::string& s, int maxChars) {
  std::vector<std::string> lines;
  std::string cur, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    if (!cur.empty() && (int)(cur.size() + 1 + word.size()) > maxChars) { lines.push_back(cur); cur.clear(); }
    if (!cur.empty()) cur += ' ';
    cur += word;
    word.clear();
  };
  for (char c : s) { if (c == ' ') flushWord(); else word += c; }
  flushWord();
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}
}  // namespace

// ------------------------------------------------------------------ events -> feel
void View::burst(Vec2 p, int n, float speed, float life, Color c, float size) {
  if (parts_.size() > 6000) return;
  for (int i = 0; i < n; i++) {
    Particle q;
    q.p = p;
    q.v = fromAngle(rng_.range(0, TAU)) * (speed * rng_.range(0.25f, 1.0f));
    q.life = q.max = life * rng_.range(0.6f, 1.0f);
    q.size = size * rng_.range(0.6f, 1.2f);
    q.c = c;
    parts_.push_back(q);
  }
}

void View::handleEvents(Game& g, Audio& a) {
  int pops = 0, gems = 0;
  for (const Event& e : g.events) {
    switch (e.t) {
      case Ev::Pop: {
        bool loop = e.n == 1;
        burst(e.p, loop ? 7 : 5, loop ? 380.0f : 240.0f, 0.55f, loop ? Color(1.0f, 0.75f, 0.35f) : Color(1.0f, 0.4f, 0.3f), 22);
        if (pops < 7) a.play(Sfx::Pop, 0.85f + 0.07f * pops, loop ? 0.8f : 0.6f);
        pops++;
        break;
      }
      case Ev::Gem:
        burst(e.p, 2, 120, 0.3f, kLime, 12);
        if (gems < 3) { a.play(Sfx::Gem, gemPitch_); gemPitch_ = std::min(2.4f, gemPitch_ + 0.04f); }
        gemPitchT_ = 0.5f;
        gems++;
        break;
      case Ev::Loop:
        shake_ = std::max(shake_, std::min(34.0f, 5.0f + e.n * 0.9f));
        hitstop = std::max(hitstop, std::min(0.14f, e.n > 0 ? 0.03f + e.n * 0.003f : 0.0f));
        burst(e.p, 40, e.a * 2.2f, 0.7f, kCyan, 26);
        a.play(Sfx::Loop, 1.0f + std::min(0.8f, e.n / 40.0f), e.n > 0 ? 1.0f : 0.5f);
        break;
      case Ev::Level: a.play(Sfx::Level); break;
      case Ev::Select: a.play(Sfx::Select); break;
      case Ev::Hurt:
        shake_ = std::max(shake_, 16.0f);
        hurtFlash_ = 0.35f;
        burst(e.p, 24, 380, 0.5f, Color(1.0f, 0.2f, 0.2f), 24);
        a.play(Sfx::Hurt);
        break;
      case Ev::Boost:
        burst(e.p, 18, 300, 0.4f, kCyan, 20);
        a.play(Sfx::Boost);
        break;
      case Ev::Zap: burst(e.p, 3, 160, 0.25f, Color(1.0f, 1.0f, 0.6f), 14); a.play(Sfx::Zap, 0.9f + rng_.f() * 0.4f, 0.8f); break;
      case Ev::BossSpawn: shake_ = std::max(shake_, 22.0f); bossBanner_ = 3.0f; a.play(Sfx::BossSpawn); break;
      case Ev::BossDie:
        shake_ = 44; hitstop = 0.25f;
        burst(e.p, 160, 900, 1.2f, kMagenta, 34);
        a.play(Sfx::BossDie);
        break;
      case Ev::Dead:
        shake_ = 30;
        burst(e.p, 120, 700, 1.0f, kCyan, 30);
        a.play(Sfx::Dead);
        break;
      case Ev::Burn: break;
    }
  }
  g.events.clear();
}

void View::update(Game& g, Audio& a, float dt) {
  clock_ += dt;
  if (shake_ > 0) shake_ = std::max(0.0f, shake_ - 60.0f * dt);
  if (hurtFlash_ > 0) hurtFlash_ -= dt;
  if (bossBanner_ > 0) bossBanner_ -= dt;
  if (gemPitchT_ > 0) { gemPitchT_ -= dt; if (gemPitchT_ <= 0) gemPitch_ = 1.0f; }

  for (size_t i = 0; i < parts_.size();) {
    Particle& p = parts_[i];
    p.life -= dt;
    if (p.life <= 0) { parts_[i] = parts_.back(); parts_.pop_back(); continue; }
    p.p += p.v * dt;
    p.v *= std::max(0.0f, 1.0f - p.drag * dt);
    i++;
  }

  // camera follows the head with a little look-ahead, zooms out as the tail grows
  Vec2 target = g.head + fromAngle(g.heading) * (g.speed * 0.18f);
  float tz = 1.0f - std::min(0.2f, g.tail.size() * 0.0016f);
  if (!camInit_) { cam_ = target; zoom_ = tz; camInit_ = true; }
  cam_ += (target - cam_) * std::min(1.0f, 7.0f * dt);
  zoom_ += (tz - zoom_) * std::min(1.0f, 1.5f * dt);

  float intensity = g.mode == Mode::Title ? 0.15f
                    : std::min(1.0f, 0.1f + g.stats.time / 240.0f * 0.6f + g.enemies.size() / 500.0f * 0.4f);
  a.setIntensity(g.mode == Mode::Dead ? 0.0f : intensity);
}

// ------------------------------------------------------------------ world
void View::drawWorld(Game& g, Gfx& gfx, float beat) {
  float pulse = beat * beat * beat;
  gfx.cam = cam_;
  gfx.zoom = zoom_;
  gfx.shake = shake_ > 0 ? Vec2(rng_.range(-shake_, shake_), rng_.range(-shake_, shake_)) : Vec2();

  // arena floor + grid
  gfx.rectW({0, 0}, {ARENA_W, ARENA_H}, Color(0.025f, 0.035f, 0.08f, 1.0f));
  Vec2 tl = gfx.toWorld({0, 0}), br = gfx.toWorld({(float)VIEW_W, (float)VIEW_H});
  const float G = 200;
  float ga = 0.10f + 0.10f * pulse;
  Color gc(0.15f, 0.45f, 0.8f, ga);
  for (float x = std::ceil(std::max(0.0f, tl.x) / G) * G; x <= std::min(ARENA_W, br.x); x += G)
    gfx.line({x, std::max(0.0f, tl.y)}, {x, std::min(ARENA_H, br.y)}, 2.0f, gc);
  for (float y = std::ceil(std::max(0.0f, tl.y) / G) * G; y <= std::min(ARENA_H, br.y); y += G)
    gfx.line({std::max(0.0f, tl.x), y}, {std::min(ARENA_W, br.x), y}, 2.0f, gc);
  Color bc(1.0f, 0.25f, 0.75f, 0.55f + 0.3f * pulse);
  gfx.line({0, 0}, {ARENA_W, 0}, 8, bc); gfx.line({ARENA_W, 0}, {ARENA_W, ARENA_H}, 8, bc);
  gfx.line({ARENA_W, ARENA_H}, {0, ARENA_H}, 8, bc); gfx.line({0, ARENA_H}, {0, 0}, 8, bc);

  // gems
  for (const Gem& m : g.gems) {
    if (!gfx.onScreen(m.p, 40)) continue;
    float s = 7.0f + std::min(6.0f, m.value * 0.8f);
    gfx.glow(m.p, s * 3.0f, kLime.withA(0.55f));
    gfx.disc(m.p, s, Color(0.8f, 1.0f, 0.7f));
  }

  // loop flashes (under enemies so the pop reads on top)
  for (const LoopFx& f : g.loopFx) {
    float t = f.t;
    float k = (1.0f - t);
    gfx.polyFill(f.poly, Color(0.35f, 0.9f, 1.0f, 0.30f * k * k));
    for (size_t i = 0; i < f.poly.size(); i++)
      gfx.line(f.poly[i], f.poly[(i + 1) % f.poly.size()], 8.0f * k + 2, Color(0.8f, 1.0f, 1.0f, 0.9f * k));
    gfx.ring(f.center, f.radius * (0.3f + 1.1f * t), 26.0f * k + 2, Color(0.6f, 0.95f, 1.0f, 0.7f * k));
  }

  // enemies
  for (const Enemy& e : g.enemies) {
    if (e.maxhp <= 0 || !gfx.onScreen(e.p, e.r + 60)) continue;
    Color c = enemyColor(e.t);
    float sc = clampf(e.spawnT, 0.0f, 1.0f);
    float r = e.r * (0.4f + 0.6f * sc);
    if (e.flash > 0) c = Color(1, 1, 1);
    if (e.burn > 0) gfx.glow(e.p, r * 3.0f, Color(1.0f, 0.5f, 0.1f, 0.6f));
    gfx.glow(e.p, r * 3.4f, c.withA(0.75f + 0.25f * pulse));
    gfx.disc(e.p, r, c.scaled(0.55f));
    gfx.disc(e.p, r * 0.62f, c);
    if (e.t == EType::Charger) {
      if (e.state == 1) {
        float k = std::fmod(clock_ * 14.0f, 1.0f);
        gfx.line(e.p, e.p + e.dir * 520.0f, 6.0f, Color(1.0f, 0.7f, 0.1f, 0.25f + 0.35f * k));
        gfx.glow(e.p, r * 4.0f, Color(1.0f, 0.8f, 0.2f, 0.7f));
      } else if (e.state == 2) {
        gfx.line(e.p, e.p - e.dir * 90.0f, r * 1.2f, Color(1.0f, 0.8f, 0.2f, 0.5f));
      }
    } else if (e.t == EType::Splitter) {
      gfx.disc(e.p + Vec2(-7, -4), r * 0.28f, c.scaled(0.4f));
      gfx.disc(e.p + Vec2(8, 5), r * 0.22f, c.scaled(0.4f));
    } else if (e.t == EType::Shooter) {
      gfx.ring(e.p, r * 1.5f, 3.0f, c.withA(0.8f), 20);
    } else if (e.t == EType::Boss) {
      for (int k = 0; k < 3; k++) {
        float a = clock_ * (0.6f + 0.4f * k) * (k % 2 ? -1 : 1);
        gfx.ring(e.p, r * (1.25f + 0.25f * k), 5.0f, c.withA(0.6f), 48);
        gfx.disc(e.p + fromAngle(a) * r * (1.25f + 0.25f * k), 9.0f, Color(1, 0.8f, 0.95f), true);
      }
    }
  }

  // bullets
  for (const Bullet& b : g.bullets) {
    gfx.glow(b.p, 26, Color(1.0f, 0.4f, 0.8f, 0.8f));
    gfx.disc(b.p, 7, Color(1.0f, 0.85f, 0.95f));
  }

  // lightning
  for (const ZapFx& z : g.zaps) {
    Vec2 prev = z.a;
    int segs = 7;
    Vec2 d = z.b - z.a;
    Vec2 n = norm(Vec2(-d.y, d.x));
    for (int i = 1; i <= segs; i++) {
      Vec2 p = z.a + d * ((float)i / segs);
      if (i < segs) p += n * rng_.range(-22, 22);
      gfx.line(prev, p, 6.0f, Color(1.0f, 1.0f, 0.6f, 0.4f));
      gfx.line(prev, p, 2.5f, Color(1.0f, 1.0f, 1.0f, 0.95f));
      prev = p;
    }
  }

  // tail ribbon (3 layers: halo, body, core)
  {
    std::vector<Vec2> pts;
    pts.reserve(g.tail.size() + 1);
    pts.push_back(g.head);
    for (const Vec2& p : g.tail) pts.push_back(p);
    size_t n = pts.size();
    std::vector<float> w(n);
    std::vector<Color> cHalo(n), cBody(n), cCore(n);
    float tw = g.tailWidth();
    for (size_t i = 0; i < n; i++) {
      float u = n > 1 ? (float)i / (float)(n - 1) : 0;     // 0 head -> 1 tip
      w[i] = tw * (1.0f - 0.85f * u) + 2.0f;
      float fade = 1.0f - u * u;
      Color base(lerpf(0.35f, 1.0f, u), lerpf(0.95f, 0.25f, u), lerpf(1.0f, 0.8f, u));  // cyan -> magenta
      cHalo[i] = base.withA(0.20f * fade * (1.0f + 0.5f * pulse));
      cBody[i] = base.withA(0.55f * fade);
      cCore[i] = Color(1, 1, 1, 0.9f * fade);
    }
    std::vector<float> wHalo(n), wBody(n), wCore(n);
    for (size_t i = 0; i < n; i++) { wHalo[i] = w[i] * 3.2f; wBody[i] = w[i] * 1.4f; wCore[i] = w[i] * 0.5f; }
    // halo: soft glow sprites along the path (a flat ribbon halo reads as a dark band)
    for (size_t i = 0; i < n; i += 2) {
      if (!gfx.onScreen(pts[i], 120)) continue;
      float u = n > 1 ? (float)i / (float)(n - 1) : 0;
      gfx.glow(pts[i], w[i] * 3.4f + 10.0f, Color(cHalo[i].r, cHalo[i].g, cHalo[i].b, 0.42f * (1.0f - u * u) * (1.0f + 0.5f * pulse)));
    }
    (void)wHalo;
    gfx.ribbon(pts, wBody, cBody);
    gfx.ribbon(pts, wCore, cCore);
    if (g.upLevel[(int)Up::BurnTail] > 0)
      for (size_t i = 2; i < n; i += 5) gfx.glow(pts[i], 24, Color(1.0f, 0.5f, 0.1f, 0.35f));
  }

  // comet head
  {
    bool blink = g.invuln > 0 && std::fmod(clock_ * 20.0f, 1.0f) < 0.5f;
    float boost = g.boostT > 0 ? 1.0f : 0.0f;
    gfx.glow(g.head, 70 + 40 * boost + 14 * pulse, kCyan.withA(0.7f));
    gfx.glow(g.head, 34, Color(1, 1, 1, 0.9f));
    if (!blink) gfx.disc(g.head, 14, Color(1, 1, 1));
    if (boost > 0) gfx.ring(g.head, 38, 4, Color(0.6f, 1.0f, 1.0f, 0.8f));
    // magnet range hint while collecting
  }

  // particles
  for (const Particle& p : parts_) {
    float k = p.life / p.max;
    gfx.glow(p.p, p.size * (0.4f + 0.8f * k), p.c.withA(clampf(k * 1.4f, 0, 1)));
  }
}

// ------------------------------------------------------------------ HUD
void View::drawHud(Game& g, Gfx& gfx, Vec2) {
  const float W = VIEW_W;
  // XP bar
  gfx.rectS(0, 0, W, 16, Color(0.05f, 0.1f, 0.15f, 0.9f));
  float xf = clampf(g.xp / g.xpNeed, 0, 1);
  gfx.rectS(0, 0, W * xf, 16, Color(0.4f, 0.95f, 0.6f, 1));
  gfx.rectS(0, 0, W * xf, 16, Color(0.4f, 0.95f, 0.6f, 0.5f), true);
  gfx.text(24, 30, "LV " + std::to_string(g.level), 4, Color(1, 1, 1, 0.95f));
  gfx.text(W * 0.5f, 28, fmtTime(g.stats.time), 6, Color(1, 1, 1, 0.95f), 1);
  gfx.text(W - 24, 30, "KILLS " + std::to_string(g.stats.kills), 4, Color(1, 0.9f, 0.7f, 0.95f), 2);

  // upgrade list
  float uy = 74;
  for (int i = 0; i < (int)Up::COUNT; i++) {
    if (g.upLevel[i] <= 0) continue;
    gfx.text(24, uy, std::string(Game::upName((Up)i)) + " " + std::to_string(g.upLevel[i]), 2, Color(0.7f, 0.9f, 1.0f, 0.8f));
    uy += 22;
  }

  // HP + boost bars (bottom left)
  float bx = 24, by = VIEW_H - 84, bw = 420;
  gfx.rectS(bx, by, bw, 26, Color(0.1f, 0.03f, 0.05f, 0.85f));
  float hf = clampf(g.hp / g.maxHp, 0, 1);
  Color hc = hf > 0.35f ? Color(1.0f, 0.3f, 0.45f) : Color(1.0f, 0.15f, 0.15f);
  gfx.rectS(bx, by, bw * hf, 26, hc);
  gfx.frameS(bx, by, bw, 26, 2, Color(1, 1, 1, 0.5f));
  gfx.text(bx + 8, by + 4, std::to_string((int)std::ceil(g.hp)) + "/" + std::to_string((int)g.maxHp), 3, Color(1, 1, 1));

  float by2 = VIEW_H - 48;
  float cd = g.boostCd > 0 ? 1.0f - g.boostCd / g.boostCooldown() : 1.0f;
  gfx.rectS(bx, by2, bw, 16, Color(0.03f, 0.08f, 0.12f, 0.85f));
  Color boostC = cd >= 1.0f ? Color(0.4f, 1.0f, 1.0f, 0.7f + 0.3f * std::sin(clock_ * 8)) : Color(0.2f, 0.5f, 0.6f);
  gfx.rectS(bx, by2, bw * clampf(cd, 0, 1), 16, boostC);
  gfx.frameS(bx, by2, bw, 16, 2, Color(1, 1, 1, 0.4f));
  gfx.text(bx + bw + 12, by2 + 1, cd >= 1.0f ? "BOOST READY" : "BOOST", 2, Color(0.7f, 1, 1, 0.8f));

  // chain readout
  if (g.chainShow > 0 && g.chainCount > 0) {
    float k = clampf(g.chainShow / 1.8f, 0, 1);
    float pop = 1.0f + 0.35f * std::max(0.0f, (k - 0.8f) * 5.0f);
    float sc = (8.0f + std::min(10.0f, g.chainCount * 0.15f)) * pop;
    std::string s = "CHAIN x" + std::to_string(g.chainCount);
    float y = 170;
    gfx.glowS(W * 0.5f, y + sc * 3.5f, gfx.textWidth(s, sc) * 0.7f, Color(1.0f, 0.6f, 0.2f, 0.25f * k));
    gfx.text(W * 0.5f + 3, y + 3, s, sc, Color(0.6f, 0.2f, 0.0f, k), 1);
    gfx.text(W * 0.5f, y, s, sc, Color(1.0f, 0.9f, 0.5f, k), 1);
  }

  // boss bar
  if (const Enemy* b = g.boss()) {
    float bw2 = 900, x = (W - bw2) * 0.5f, y = VIEW_H - 70;
    gfx.rectS(x, y, bw2, 24, Color(0.1f, 0.02f, 0.08f, 0.9f));
    gfx.rectS(x, y, bw2 * clampf(b->hp / b->maxhp, 0, 1), 24, Color(1.0f, 0.25f, 0.7f));
    gfx.frameS(x, y, bw2, 24, 2, Color(1, 1, 1, 0.6f));
    gfx.text(W * 0.5f, y - 30, "HIVE MOTHER", 3, Color(1.0f, 0.6f, 0.85f), 1);
  }
  if (bossBanner_ > 0) {
    float a = clampf(bossBanner_, 0, 1);
    gfx.text(W * 0.5f, 380, "WARNING", 12, Color(1.0f, 0.2f, 0.5f, a), 1);
    gfx.text(W * 0.5f, 490, "A BOSS APPROACHES - LOOP IT AGAIN AND AGAIN", 3, Color(1, 0.8f, 0.9f, a), 1);
  }

  // minimap
  {
    float mw = 288, mh = mw * ARENA_H / ARENA_W, mx = W - mw - 24, my = VIEW_H - mh - 24;
    gfx.rectS(mx, my, mw, mh, Color(0.02f, 0.04f, 0.1f, 0.75f));
    gfx.frameS(mx, my, mw, mh, 2, Color(1.0f, 0.25f, 0.75f, 0.6f));
    float sx = mw / ARENA_W, sy = mh / ARENA_H;
    for (const Enemy& e : g.enemies) {
      if (e.maxhp <= 0) continue;
      float s = e.t == EType::Boss ? 8.0f : 2.0f;
      gfx.rectS(mx + e.p.x * sx - s * 0.5f, my + e.p.y * sy - s * 0.5f, s, s, e.t == EType::Boss ? Color(1, 0.3f, 0.8f) : Color(1.0f, 0.4f, 0.3f, 0.8f));
    }
    gfx.rectS(mx + g.head.x * sx - 3, my + g.head.y * sy - 3, 6, 6, Color(0.5f, 1, 1));
  }

  if (hurtFlash_ > 0) gfx.rectS(0, 0, W, VIEW_H, Color(1.0f, 0.0f, 0.1f, 0.25f * (hurtFlash_ / 0.35f)), true);
}

// ------------------------------------------------------------------ menus
void View::drawMenus(Game& g, Gfx& gfx, Vec2 mouse) {
  const float W = VIEW_W;
  auto dim = [&](float a) { gfx.rectS(0, 0, W, VIEW_H, Color(0.01f, 0.01f, 0.04f, a)); };

  if (g.mode == Mode::Title) {
    dim(0.55f);
    float h = clock_ * 0.7f;
    Color c(0.6f + 0.4f * std::sin(h), 0.7f + 0.3f * std::sin(h + 2.0f), 1.0f);
    gfx.glowS(W * 0.5f, 330, 520, c.withA(0.25f));
    gfx.text(W * 0.5f + 5, 255, "TAILSPIN", 22, Color(0.1f, 0.0f, 0.2f, 0.9f), 1);
    gfx.text(W * 0.5f, 250, "TAILSPIN", 22, c, 1);
    gfx.text(W * 0.5f, 450, "LOOP YOUR TAIL AROUND ENEMIES TO POP THEM", 4, Color(0.8f, 0.95f, 1.0f), 1);
    gfx.text(W * 0.5f, 520, "CHAIN BLASTS TURN ONE LOOP INTO A WIPEOUT", 3, Color(0.7f, 0.8f, 0.9f), 1);
    float p = 0.6f + 0.4f * std::sin(clock_ * 4.0f);
    gfx.text(W * 0.5f, 680, "CLICK OR PRESS SPACE TO START", 5, Color(1.0f, 0.9f, 0.5f, p), 1);
    gfx.text(W * 0.5f, 800, "MOUSE STEERS    SPACE OR RIGHT CLICK BOOSTS    ESC PAUSES", 3, Color(0.7f, 0.85f, 1.0f, 0.85f), 1);
    if (save.runs > 0) {
      std::string s = "BEST  " + fmtTime(save.bestTime) + "   " + std::to_string(save.bestKills) + " KILLS   BEST LOOP x" + std::to_string(save.bestLoop);
      gfx.text(W * 0.5f, 900, s, 3, Color(1.0f, 0.85f, 0.5f, 0.9f), 1);
    }
  } else if (g.mode == Mode::LevelUp) {
    dim(0.62f);
    gfx.text(W * 0.5f, 130, "LEVEL UP!", 12, Color(0.6f, 1.0f, 0.7f), 1);
    gfx.text(W * 0.5f, 262, "PICK AN UPGRADE   (1 2 3 OR CLICK)", 3, Color(0.8f, 0.9f, 1.0f, 0.9f), 1);
    for (int i = 0; i < g.numChoices; i++) {
      float x, y, w, h;
      cardRect(i, g.numChoices, x, y, w, h);
      bool hot = mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
      float lift = hot ? -10.0f : 0.0f;
      gfx.rectS(x, y + lift, w, h, Color(0.05f, 0.09f, 0.16f, 0.96f));
      gfx.rectS(x, y + lift, w, h, Color(0.2f, 0.5f, 0.8f, hot ? 0.18f : 0.06f), true);
      gfx.frameS(x, y + lift, w, h, hot ? 5.0f : 3.0f, hot ? Color(0.6f, 1.0f, 0.8f) : Color(0.35f, 0.7f, 1.0f, 0.8f));
      gfx.text(x + w * 0.5f, y + lift + 40, std::to_string(i + 1), 6, Color(0.4f, 0.6f, 0.8f), 1);
      auto lines = wrap(Game::upName(g.choices[i]), 14);
      float ty = y + lift + 130;
      for (auto& l : lines) { gfx.text(x + w * 0.5f, ty, l, 5, Color(1, 1, 1), 1); ty += 50; }
      int lv = g.upLevel[(int)g.choices[i]];
      gfx.text(x + w * 0.5f, ty + 14, "LV " + std::to_string(lv) + " > " + std::to_string(lv + 1), 4, kGold, 1);
      auto dl = wrap(Game::upDesc(g.choices[i]), 24);
      float dy = y + lift + 340;
      for (auto& l : dl) { gfx.text(x + w * 0.5f, dy, l, 3, Color(0.75f, 0.88f, 1.0f), 1); dy += 32; }
    }
  } else if (g.mode == Mode::Dead) {
    dim(0.6f);
    gfx.text(W * 0.5f, 200, "RUN OVER", 16, Color(1.0f, 0.3f, 0.45f), 1);
    float y = 400;
    auto row = [&](const std::string& a, const std::string& b) {
      gfx.text(W * 0.5f - 20, y, a, 4, Color(0.7f, 0.85f, 1.0f), 2);
      gfx.text(W * 0.5f + 20, y, b, 4, Color(1, 1, 1), 0);
      y += 56;
    };
    row("SURVIVED", fmtTime(g.stats.time));
    row("KILLS", std::to_string(g.stats.kills));
    row("LOOPS", std::to_string(g.stats.loops));
    row("BEST LOOP", "x" + std::to_string(g.stats.bestLoop));
    row("LEVEL", std::to_string(g.stats.level));
    if (newBest) gfx.text(W * 0.5f, y + 10, "NEW BEST TIME!", 5, Color(1.0f, 0.9f, 0.4f, 0.6f + 0.4f * std::sin(clock_ * 8)), 1);
    else gfx.text(W * 0.5f, y + 10, "BEST  " + fmtTime(save.bestTime), 4, Color(1.0f, 0.85f, 0.5f, 0.9f), 1);
    // big retry button
    float bw = 560, bh = 110, bx = (W - bw) * 0.5f, by = 800;
    bool hot = mouse.x >= bx && mouse.x <= bx + bw && mouse.y >= by && mouse.y <= by + bh;
    gfx.rectS(bx, by, bw, bh, hot ? Color(0.2f, 0.8f, 0.6f, 0.95f) : Color(0.1f, 0.5f, 0.45f, 0.95f));
    gfx.frameS(bx, by, bw, bh, 4, Color(0.7f, 1.0f, 0.9f));
    gfx.text(W * 0.5f, by + 34, "RETRY (R)", 8, Color(1, 1, 1), 1);
  } else if (g.mode == Mode::Paused) {
    dim(0.55f);
    gfx.text(W * 0.5f, 400, "PAUSED", 16, Color(1, 1, 1), 1);
    gfx.text(W * 0.5f, 560, "ESC TO RESUME", 4, Color(0.8f, 0.9f, 1.0f), 1);
  }
}

void View::draw(Game& g, Gfx& gfx, float beat, Vec2 mouse) {
  gfx.begin(Color(0.01f, 0.01f, 0.03f));
  drawWorld(g, gfx, beat);
  if (g.mode != Mode::Title) drawHud(g, gfx, mouse);
  drawMenus(g, gfx, mouse);
}
