// World rendering: sprite bank, y-sorted scene, effects, lighting, weather, event handling.
#include <algorithm>
#include <cmath>
#include "rpg/view/view.h"

using art::Prop;
using art::Monster;

namespace {
Color col(uint32_t c, float a = 1) { return Color((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, a); }
uint64_t lookKey(const art::HumanLook& L) {
  uint64_t k = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { k ^= v; k *= 1099511628211ull; };
  mix(L.skin); mix(L.hairColor); mix(L.topColor); mix(L.bottomColor); mix(L.tabardColor); mix(L.weaponColor);
  mix((uint64_t)L.hair | (uint64_t)L.outfit << 8 | (uint64_t)L.beard << 16 | (uint64_t)L.helmet << 17 | (uint64_t)L.hood << 18 |
      (uint64_t)L.cape << 19 | (uint64_t)L.shield << 20 | (uint64_t)L.weapon << 24);
  return k;
}
bool flatProp(Prop p) {
  return p == Prop::Rug || p == Prop::LilyPad || p == Prop::Flowers1 || p == Prop::Flowers2 || p == Prop::Flowers3 || p == Prop::Bones ||
         p == Prop::SkullPile || p == Prop::Mushrooms || p == Prop::Ladder;
}
bool natureProp(Prop p) { return (int)p <= (int)Prop::Fern; }
bool treeProp(Prop p) {
  return p == Prop::OakTree || p == Prop::OakTree2 || p == Prop::PineTree || p == Prop::PineTree2 || p == Prop::SnowPine || p == Prop::BirchTree ||
         p == Prop::DeadTree || p == Prop::WillowTree || p == Prop::PalmTree || p == Prop::AutumnTree;
}
// light sources: radius (px) and colour
bool propLight(Prop p, float& r, Color& c) {
  switch (p) {
    case Prop::Torch: r = 56; c = Color(1.0f, 0.62f, 0.3f); return true;
    case Prop::Campfire: r = 90; c = Color(1.0f, 0.55f, 0.25f); return true;
    case Prop::Brazier: r = 70; c = Color(1.0f, 0.6f, 0.3f); return true;
    case Prop::Lamppost: r = 64; c = Color(1.0f, 0.8f, 0.5f); return true;
    case Prop::Fireplace: r = 80; c = Color(1.0f, 0.6f, 0.3f); return true;
    case Prop::Crystal: r = 44; c = Color(0.4f, 0.8f, 1.0f); return true;
    case Prop::Shrine: r = 40; c = Color(0.8f, 0.8f, 1.0f); return true;
    case Prop::Altar: r = 40; c = Color(0.9f, 0.6f, 1.0f); return true;
    case Prop::Cauldron: r = 36; c = Color(0.5f, 1.0f, 0.6f); return true;
    default: return false;
  }
}
}  // namespace

bool View::init(Pix& pix, Audio& audio) {
  pix_ = &pix;
  audio_ = &audio;
  for (int i = 0; i < (int)Prop::COUNT; i++) props_.push_back(pix.bake(art::propSprite((Prop)i)));
  for (int i = 0; i < (int)Monster::COUNT; i++) monsters_.push_back(pix.bake(art::monsterSheet((Monster)i)));
  for (int i = 0; i < (int)art::Fx::COUNT; i++) fx_.push_back(pix.bake(art::fxSprite((art::Fx)i)));
  for (int m = 0; m < 16; m++) walls_.push_back(pix.bake(art::wallPiece(m)));
  gate_ = pix.bake(art::gatePiece());
  {
    Canvas c(14, 6);
    for (int y = 0; y < 6; y++)
      for (int x = 0; x < 14; x++) {
        float dx = (x - 6.5f) / 7.0f, dy = (y - 2.5f) / 3.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) c.set(x, y, rgba(20, 16, 30, (int)(110 * (1 - d * 0.5f))));
      }
    shadow_ = pix.bake(c);
    Canvas b(40, 12);
    for (int y = 0; y < 12; y++)
      for (int x = 0; x < 40; x++) {
        float dx = (x - 19.5f) / 20.0f, dy = (y - 5.5f) / 6.0f;
        float d = dx * dx + dy * dy;
        if (d < 1) b.set(x, y, rgba(20, 16, 30, (int)(100 * (1 - d * 0.6f))));
      }
    shadowBig_ = pix.bake(b);
  }
  {
    Canvas l(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float d = std::hypot(x - 31.5f, y - 31.5f) / 32.0f;
        float a = d < 1 ? (1 - d) * (1 - d) : 0;
        l.set(x, y, rgba(255, 255, 255, (int)(255 * a)));
      }
    light_ = pix.bake(l);
    Canvas w(64, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 64; x++) {
        float n = vnoise(x / 6.0f, y / 3.0f, 777);
        // tileable-ish: fade at edges handled by the scroll; sparse highlight streaks
        if (n > 0.70f && (y % 3) == 0) w.set(x, y, rgba(170, 215, 245, (int)((n - 0.7f) * 600)));
      }
    water_ = pix.bake(w);
    Canvas v(120, 68);
    for (int y = 0; y < 68; y++)
      for (int x = 0; x < 120; x++) {
        float dx = (x - 59.5f) / 60.0f, dy = (y - 33.5f) / 34.0f;
        float d = std::sqrt(dx * dx + dy * dy);
        float a = smooth01(clampf((d - 0.65f) / 0.5f, 0, 1));
        v.set(x, y, rgba(8, 6, 16, (int)(150 * a)));
      }
    vignette_ = pix.bake(v);
    Canvas wt(1, 1); wt.set(0, 0, rgba(255, 255, 255));
    white_ = pix.bake(wt);
  }
  lightMap_ = pix.makeTarget(Pix::W / 2, Pix::H / 2);
  miniPx_.assign(64 * 64, 0);
  pix.miniInit(64, 64);
  return true;
}

const Tex& View::humanTex(const art::HumanLook& L) {
  uint64_t k = lookKey(L);
  auto it = humans_.find(k);
  if (it != humans_.end()) return it->second;
  return humans_[k] = pix_->bake(art::humanSheet(L));
}
const Tex& View::iconTex(art::Icon i, uint32_t tint) {
  uint64_t k = (uint64_t)i << 32 | tint;
  auto it = icons_.find(k);
  if (it != icons_.end()) return it->second;
  return icons_[k] = pix_->bake(art::itemIcon(i, tint));
}
const Tex& View::bldgTex(const Bldg& b, int index) {
  uint64_t k = (uint64_t)index;
  auto it = bldgTex_.find(k);
  if (it != bldgTex_.end()) return it->second;
  return bldgTex_[k] = pix_->bake(art::buildingSprite(b.type, b.r.w, b.r.h, b.roof, b.seed));
}

void View::snap(Game& g) {
  const Actor& p = g.pl();
  cam_ = Vec2(p.p.x - Pix::W / 2.0f, p.p.y - 10 - Pix::H / 2.0f);
  const Map& m = g.map();
  float mw = m.w * 16.0f, mh = m.h * 16.0f;
  if (mw > Pix::W) cam_.x = clampf(cam_.x, 0, mw - Pix::W); else cam_.x = (mw - Pix::W) / 2;
  if (mh > Pix::H) cam_.y = clampf(cam_.y, 0, mh - Pix::H); else cam_.y = (mh - Pix::H) / 2;
}

// ------------------------------------------------------------------ events -> particles / audio / UI
void View::spawnParticles(const Event& e, Game& g) {
  Rng r((uint32_t)(t_ * 1000) ^ (uint32_t)(e.p.x * 7 + e.p.y * 13));
  auto add = [&](Vec2 p, Vec2 v, float life, Color c, float size, float grav) {
    Particle q; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.size = size; q.grav = grav; parts_.push_back(q);
  };
  auto addFx = [&](art::Fx f, Vec2 p, float life) {
    Particle q; q.p = p; q.life = q.max = life; q.kind = 1; q.fx = (int)f; parts_.push_back(q);
  };
  switch (e.type) {
    case Ev::Sfx: audio_->play((Sfx)e.a, e.f, e.vol); break;
    case Ev::Hit:
      for (int i = 0; i < 6; i++) add(e.p, Vec2(r.range(-70, 70), r.range(-80, 20)), 0.25f, Color(1, 1, 0.85f), 1, 200);
      break;
    case Ev::Blood:
      for (int i = 0; i < 7; i++) add(e.p, Vec2(r.range(-50, 50), r.range(-70, 0)), 0.45f, Color(0.7f, 0.08f, 0.08f), r.f() < 0.5f ? 2.f : 1.f, 260);
      break;
    case Ev::Explode:
      addFx(art::Fx::Explosion, e.p, 0.45f);
      for (int i = 0; i < 14; i++) add(e.p, Vec2(r.range(-90, 90), r.range(-100, 40)), 0.5f, Color(1, r.range(0.4f, 0.8f), 0.2f), 1.5f, 120);
      shake_ = std::max(shake_, 2.0f * e.f);
      break;
    case Ev::Sparkle: for (int i = 0; i < 8; i++) add(e.p, Vec2(r.range(-30, 30), r.range(-50, -10)), 0.7f, Color(1, 0.95f, 0.6f), 1, 0); break;
    case Ev::Dust: addFx(art::Fx::Dust, e.p + Vec2(r.range(-3, 3), -2), 0.3f); break;
    case Ev::Heal: addFx(art::Fx::Heal, e.p + Vec2(0, -10), 0.6f); for (int i = 0; i < 10; i++) add(e.p + Vec2(r.range(-8, 8), r.range(-16, 0)), Vec2(0, r.range(-40, -15)), 0.8f, Color(0.5f, 1, 0.6f), 1, 0); break;
    case Ev::Frost: addFx(art::Fx::Frost, e.p + Vec2(0, -6), 0.35f); break;
    case Ev::Text: {
      FloatText ft; ft.p = e.p + Vec2(r.range(-4, 4), 0); ft.s = e.s; ft.c = col((uint32_t)e.a); texts_.push_back(ft);
      break;
    }
    case Ev::Discover:
      banner_ = "DISCOVERED"; bannerSub_ = e.s; bannerT_ = 4.0f;
      break;
    case Ev::LevelUp:
      banner_ = "LEVEL UP"; bannerSub_ = "LEVEL " + std::to_string(e.a) + "  -  CHOOSE A STAT IN THE MENU"; bannerT_ = 4.5f;
      // a bigger moment: a layered fanfare, two rings of light and a column of rising sparks
      audio_->play(Sfx::LevelUp, 0.5f, 1.2f);
      audio_->play(Sfx::QuestDone, 1.5f, 0.8f);
      audio_->play(Sfx::Discover, 0.75f, 0.7f);
      for (int i = 0; i < 32; i++) { float a = i / 32.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 75, std::sin(a) * 48), 1.0f, Color(1, 0.85f, 0.3f), 2.0f, 0); }
      for (int i = 0; i < 20; i++) { float a = i / 20.0f * TAU; add(e.p + Vec2(0, -10), Vec2(std::cos(a) * 38, std::sin(a) * 24), 0.8f, Color(1, 1, 0.8f), 1.0f, 0); }
      for (int i = 0; i < 26; i++) add(e.p + Vec2(r.range(-9, 9), r.range(-4, 2)), Vec2(r.range(-6, 6), r.range(-90, -40)), r.range(0.8f, 1.4f), Color(1, r.range(0.75f, 0.95f), 0.35f), 1, -20);
      shake_ = std::max(shake_, 3.0f);
      break;
    case Ev::QuestUpdate: {
      Toast t; t.s = e.s; t.c = e.f == 1 ? Color(1, 0.85f, 0.3f) : Color(0.9f, 0.85f, 0.7f); toasts_.push_back(t);
      if (e.f == 0 || e.f == 1) { banner_ = e.f == 1 ? "QUEST COMPLETE" : "NEW QUEST"; bannerSub_ = e.s.substr(e.s.find(':') == std::string::npos ? 0 : e.s.find(':') + 2); bannerT_ = 3.5f; }
      break;
    }
    case Ev::Notice: { Toast t; t.s = e.s; t.c = col((uint32_t)e.a); toasts_.push_back(t); break; }
    case Ev::Shake: shake_ = std::max(shake_, e.f); break;
    case Ev::MapChange: fade_ = 1.0f; snap(g); break;
    default: break;
  }
}

void View::update(Game& g, float dt) {
  t_ += dt;
  modeT_ += dt;
  if (g.mode != lastMode_) {
    modeT_ = 0;
    if (g.mode == Mode::Dialogue) { dlgChars_ = 0; dlgSel_ = 0; }
    if (g.mode == Mode::Shop) { shopSide_ = 0; shopSel_ = 0; }
    lastMode_ = g.mode;
  }
  for (const Event& e : g.events) spawnParticles(e, g);
  g.events.clear();
  if (toasts_.size() > 5) toasts_.erase(toasts_.begin(), toasts_.begin() + (toasts_.size() - 5));
  // camera follows with a little lead in the aim direction
  if (g.mode != Mode::Title) {
    const Actor& p = g.pl();
    Vec2 want(p.p.x - Pix::W / 2.0f + p.aim.x * 10, p.p.y - 10 - Pix::H / 2.0f + p.aim.y * 6);
    cam_ += (want - cam_) * std::min(1.0f, dt * 6.0f);
    const Map& m = g.map();
    float mw = m.w * 16.0f, mh = m.h * 16.0f;
    if (mw > Pix::W) cam_.x = clampf(cam_.x, 0, mw - Pix::W); else cam_.x = (mw - Pix::W) / 2;
    if (mh > Pix::H) cam_.y = clampf(cam_.y, 0, mh - Pix::H); else cam_.y = (mh - Pix::H) / 2;
  } else {
    // title: slow drift across the start region
    const Site& home = g.world.sites[g.world.startSite];
    Vec2 c0(home.r.cx() * 16.0f - Pix::W / 2.0f - 300, home.r.cy() * 16.0f - Pix::H / 2.0f - 120);
    if (titleT_ == 0 || titleT_ > 90) { cam_ = c0; titleT_ = 0.001f; }
    titleT_ += dt;
    cam_ = c0 + Vec2(titleT_ * 8.0f, titleT_ * 2.5f);
  }
  shake_ = std::max(0.0f, shake_ - dt * 12);
  shakeOff_ = shake_ > 0.1f ? Vec2(std::sin(t_ * 91) * shake_, std::cos(t_ * 77) * shake_ * 0.7f) : Vec2();
  for (size_t i = 0; i < parts_.size();) {
    Particle& q = parts_[i];
    q.life -= dt;
    if (q.life <= 0) { parts_.erase(parts_.begin() + i); continue; }
    q.v.y += q.grav * dt;
    q.p += q.v * dt;
    i++;
  }
  for (size_t i = 0; i < texts_.size();) {
    texts_[i].t += dt;
    texts_[i].p.y -= dt * 22;
    if (texts_[i].t > 0.9f) texts_.erase(texts_.begin() + i); else i++;
  }
  for (size_t i = 0; i < toasts_.size();) {
    toasts_[i].t += dt;
    if (toasts_[i].t > 4.0f) toasts_.erase(toasts_.begin() + i); else i++;
  }
  if (bannerT_ > 0) bannerT_ -= dt;
  if (fade_ > 0) fade_ = std::max(0.0f, fade_ - dt * 2.2f);
  {
    float before = dlgChars_;
    dlgChars_ += dt * 70;
    if (g.mode == Mode::Dialogue && (int)(before / 3) != (int)(dlgChars_ / 3) && dlgChars_ < (float)g.dlg.text.size()) {
      size_t ci = std::min(g.dlg.text.size() - 1, (size_t)dlgChars_);
      if (g.dlg.text[ci] != ' ') audio_->play(Sfx::Talk, 0.7f + (hash32((uint32_t)std::hash<std::string>()(g.dlg.speaker)) % 80) / 100.0f, 0.8f);
    }
  }

  // music director
  Music want = Music::Wild;
  if (g.mode == Mode::Title) want = Music::Title;
  else if (g.mode == Mode::Dead) want = Music::Silence;
  else {
    bool fight = false, boss = false;
    for (const Actor& a : g.actors)
      if (a.hostile && a.aggro && a.st != AState::Dead && len2(a.p - g.pl().p) < 170 * 170) { fight = true; if (a.boss) boss = true; }
    if (fight) combatT_ = boss ? 6.0f : 4.0f;
    else combatT_ = std::max(0.0f, combatT_ - dt);
    if (boss && fight) want = Music::Boss;
    else if (combatT_ > 0) want = Music::Combat;
    else if (g.inside && g.subBldg >= 0) want = Music::Town;
    else if (g.inside) want = Music::Cave;
    else if (g.curSite >= 0 && (g.world.sites[g.curSite].type == SiteType::City || g.world.sites[g.curSite].type == SiteType::Town || g.world.sites[g.curSite].type == SiteType::Village)) want = Music::Town;
    else if (g.isNight()) want = Music::Night;
  }
  if (want != music_) { music_ = want; audio_->setMusic(want); }

  // ambient particles
  if (g.mode != Mode::Title && !g.inside) {
    const Actor& p = g.pl();
    Biome b = g.world.over.biomeAt((int)(p.p.x / 16), (int)(p.p.y / 16));
    Rng r((uint32_t)(t_ * 997));
    if (g.isNight() && (b == Biome::Plains || b == Biome::Forest || b == Biome::Swamp || b == Biome::Autumn) && r.f() < dt * 6) {
      Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), r.range(0, Pix::H)); q.v = Vec2(r.range(-6, 6), r.range(-6, 6));
      q.life = q.max = r.range(2, 4); q.c = Color(0.8f, 1.0f, 0.4f); q.size = 1; q.layer = 1; parts_.push_back(q);
    }
    if (b == Biome::Autumn && r.f() < dt * 5) {
      Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), -4); q.v = Vec2(r.range(8, 20), r.range(14, 24));
      q.life = q.max = 10; q.c = r.f() < 0.5f ? Color(0.85f, 0.35f, 0.15f) : Color(0.95f, 0.7f, 0.25f); q.size = 2; q.layer = 2; parts_.push_back(q);
    }
  }
  if (g.inside && g.subSite >= 0 && hashf((int)(t_ * 10), 0, 9) < dt * 4) {
    Rng r((uint32_t)(t_ * 331));
    Particle q; q.p = cam_ + Vec2(r.range(0, Pix::W), r.range(0, Pix::H)); q.v = Vec2(r.range(-3, 3), r.range(-5, -1));
    q.life = q.max = 3; q.c = Color(0.7f, 0.65f, 0.6f); q.size = 1; q.layer = 1; parts_.push_back(q);
  }
  if (parts_.size() > 600) parts_.erase(parts_.begin(), parts_.begin() + (parts_.size() - 600));
}

// ------------------------------------------------------------------ world
namespace {
struct Drawable {
  float y;
  int kind;     // 0 prop, 1 building, 2 wall, 3 actor, 4 pickup, 5 projectile, 6 gate
  int idx;
  int tx, ty;
};
}  // namespace

void View::drawWorld(Game& g) {
  Pix& P = *pix_;
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  int key = g.mode == Mode::Title ? 0 : g.mapKey();
  if (key != lastMapKey_) { lastMapKey_ = key; }
  uint64_t mapId = hash32((uint32_t)g.world.seed ^ (uint32_t)(g.world.seed >> 32)) * 2654435761ull + (uint64_t)(key + 7);
  Vec2 cam(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y));
  // water layer underneath the terrain (shows through translucent water pixels)
  if (m.kind == MapKind::Overworld) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.16f, 0.36f, 0.6f));
    float ox = std::fmod(t_ * 5.0f + cam.x * 0.0f, 64.0f), oy = std::fmod(std::sin(t_ * 0.6f) * 6 + 64, 64.0f);
    for (float y = -64; y < Pix::H + 64; y += 64)
      for (float x = -64; x < Pix::W + 64; x += 64) {
        float wx = x - std::fmod(cam.x, 64.0f) + ox, wy = y - std::fmod(cam.y, 64.0f) + oy;
        P.blit(water_, wx, wy, false, Color(1, 1, 1, 0.55f + 0.25f * std::sin(t_ * 1.7f + x * 0.01f)));
        P.blit(water_, wx + 32 - ox * 1.6f, wy + 17, true, Color(1, 1, 1, 0.35f));
      }
  } else {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.04f, 0.035f, 0.05f));
    if (m.kind == MapKind::Cave || m.kind == MapKind::Ruin) {
      // dark water in caves animates gently too
      P.rect(0, 0, Pix::W, Pix::H, Color(0.08f, 0.14f, 0.2f));
    }
  }
  // terrain chunks
  int c0x = (int)std::floor(cam.x / 512), c0y = (int)std::floor(cam.y / 512);
  int c1x = (int)std::floor((cam.x + Pix::W) / 512), c1y = (int)std::floor((cam.y + Pix::H) / 512);
  for (int cy = c0y; cy <= c1y; cy++)
    for (int cx = c0x; cx <= c1x; cx++) {
      if (cx < 0 || cy < 0 || cx * 32 >= m.w || cy * 32 >= m.h) continue;
      Tex t = chunkTex(m, mapId, cx, cy);
      P.blit(t, cx * 512 - cam.x, cy * 512 - cam.y);
    }
  prefetch(m, mapId, cam);
  pumpBake(5.0);
  // collect drawables
  std::vector<Drawable> list;
  int tx0 = (int)std::floor(cam.x / 16) - 3, ty0 = (int)std::floor(cam.y / 16) - 2;
  int tx1 = tx0 + Pix::W / 16 + 6, ty1 = ty0 + Pix::H / 16 + 7;
  const Actor& pl = g.pl();
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty1); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx1); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      if (pr) {
        Prop p = (Prop)(pr - 1);
        if (flatProp(p)) {
          const Tex& t = props_[(int)p];
          int fw = art::propW(p);
          int frames = std::max(1, art::propFrames(p));
          int fr = frames > 1 ? (int)(t_ * 8 + tx * 3) % frames : 0;
          P.blitRegion(t, fr * fw, 0, fw, t.h, tx * 16 + 8 - fw / 2 - cam.x, ty * 16 + 16 - t.h - cam.y);
        } else list.push_back({ty * 16.0f + 15.0f, 0, pr - 1, tx, ty});
      }
      if (m.wall[(size_t)ty * m.w + tx]) list.push_back({ty * 16.0f + 15.0f, 2, 0, tx, ty});
    }
  for (int bi = 0; bi < (int)m.bldgs.size(); bi++) {
    const Bldg& b = m.bldgs[bi];
    if (b.r.x * 16 > cam.x + Pix::W + 16 || (b.r.x + b.r.w) * 16 < cam.x - 16) continue;
    if ((b.r.y - 3) * 16 > cam.y + Pix::H || (b.r.y + b.r.h) * 16 < cam.y) continue;
    list.push_back({(b.r.y + b.r.h) * 16.0f - 1.0f, 1, bi, 0, 0});
  }
  for (int i = 0; i < (int)g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (a.p.x < cam.x - 80 || a.p.x > cam.x + Pix::W + 80 || a.p.y < cam.y - 40 || a.p.y > cam.y + Pix::H + 80) continue;
    list.push_back({a.p.y + (a.fly ? 60.0f : 0.0f) - (a.st == AState::Dead ? 8.0f : 0.0f), 3, i, 0, 0});
  }
  for (int i = 0; i < (int)g.pickups.size(); i++) list.push_back({g.pickups[i].p.y - 4, 4, i, 0, 0});
  // projectiles fly at chest height: sort them by the ground point beneath (arrows, spells, spit, dragon fire)
  for (int i = 0; i < (int)g.projs.size(); i++) {
    const Projectile& pr = g.projs[i];
    if (pr.p.x < cam.x - 24 || pr.p.x > cam.x + Pix::W + 24 || pr.p.y < cam.y - 24 || pr.p.y > cam.y + Pix::H + 40) continue;
    list.push_back({pr.p.y + 8.0f, 5, i, 0, 0});
  }
  if (m.kind == MapKind::Overworld)
    for (auto& gt : g.world.gates) {
      if ((gt.first + 4) * 16 < cam.x || (gt.first - 2) * 16 > cam.x + Pix::W || gt.second * 16 < cam.y - 40 || gt.second * 16 > cam.y + Pix::H + 40) continue;
      list.push_back({gt.second * 16.0f + 15.0f, 6, 0, gt.first, gt.second});
    }
  std::sort(list.begin(), list.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

  // shadows first (under everything standing)
  for (const Drawable& d : list) {
    if (d.kind == 3) {
      const Actor& a = g.actors[d.idx];
      if (a.st == AState::Dead) continue;
      bool big = a.radius > 7;
      const Tex& s = big ? shadowBig_ : shadow_;
      float sc = big ? std::min(1.6f, a.radius / 10.0f) : 1.0f;
      if (a.mon == Monster::Dragon) sc = a.fly ? 1.8f : 2.0f;
      P.blitEx(s, 0, 0, s.w, s.h, a.p.x - s.w * sc / 2 - cam.x, a.p.y - s.h * sc / 2 - cam.y, s.w * sc, s.h * sc, false, Color(1, 1, 1, a.fly ? 0.6f : 1));
    } else if (d.kind == 0) {
      Prop p = (Prop)d.idx;
      if (treeProp(p)) P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 12 - cam.x, d.ty * 16 + 11 - cam.y, 24, 8, false, Color(1, 1, 1, 0.8f));
      else if (p == Prop::Boulder || p == Prop::Tent || p == Prop::Well || p == Prop::Fountain || p == Prop::Statue || p == Prop::Cart)
        P.blitEx(shadowBig_, 0, 0, 40, 12, d.tx * 16 + 8 - 11 - cam.x, d.ty * 16 + 12 - cam.y, 22, 7, false, Color(1, 1, 1, 0.7f));
    }
  }
  for (const Drawable& d : list) {
    switch (d.kind) {
      case 0: {
        Prop p = (Prop)d.idx;
        const Tex& t = props_[d.idx];
        int fw = art::propW(p), fh = art::propH(p);
        int frames = std::max(1, art::propFrames(p));
        int fr = frames > 1 ? (int)(t_ * 8 + d.tx * 3 + d.ty) % frames : 0;
        float jx = 0, jy = 0;
        if (natureProp(p) && m.kind == MapKind::Overworld) { uint32_t h = hash2(d.tx, d.ty, 55); jx = (float)((int)(h % 7) - 3); jy = (float)((int)((h >> 4) % 3) - 1); }
        float x = d.tx * 16 + 8 - fw / 2 + jx - cam.x, y = d.ty * 16 + 16 - fh + jy - cam.y;
        float alpha = 1;
        if (treeProp(p) && pl.p.y < d.ty * 16 + 10 && pl.p.y > d.ty * 16 + 16 - fh + 4 && std::fabs(pl.p.x - (d.tx * 16 + 8 + jx)) < fw * 0.4f) alpha = 0.5f;
        P.blitEx(t, fr * fw, 0, fw, fh, x, y, (float)fw, (float)fh, false, Color(1, 1, 1, alpha));
        if (p == Prop::Campfire || p == Prop::Brazier) {
          Rng r((uint32_t)(t_ * 30) + d.tx * 7);
          if (r.f() < 0.3f) { Particle q; q.p = Vec2(d.tx * 16 + 8 + r.range(-3, 3), d.ty * 16 + 6.0f); q.v = Vec2(r.range(-5, 5), r.range(-30, -15)); q.life = q.max = 0.8f; q.c = Color(1, 0.6f, 0.2f); q.size = 1; parts_.push_back(q); }
        }
        break;
      }
      case 1: {
        const Bldg& b = m.bldgs[d.idx];
        const Tex& t = bldgTex(b, d.idx);
        float x = b.r.x * 16.0f - cam.x, y = (b.r.y + b.r.h) * 16.0f - t.h - cam.y;
        float alpha = 1;
        if (pl.p.y < (b.r.y + b.r.h) * 16 - 4 && pl.p.y > (b.r.y + b.r.h) * 16 - t.h + 8 && pl.p.x > b.r.x * 16 && pl.p.x < (b.r.x + b.r.w) * 16) alpha = 0.55f;
        P.blitEx(t, 0, 0, t.w, t.h, x, y, (float)t.w, (float)t.h, false, Color(1, 1, 1, alpha));
        break;
      }
      case 2: {
        int mask = 0;
        auto W = [&](int x, int y) { return m.in(x, y) && m.wall[(size_t)y * m.w + x]; };
        if (W(d.tx, d.ty - 1)) mask |= 1;
        if (W(d.tx + 1, d.ty)) mask |= 2;
        if (W(d.tx, d.ty + 1)) mask |= 4;
        if (W(d.tx - 1, d.ty)) mask |= 8;
        const Tex& t = walls_[mask];
        P.blit(t, d.tx * 16.0f - cam.x, d.ty * 16.0f + 16 - t.h - cam.y);
        break;
      }
      case 6:
        P.blit(gate_, d.tx * 16.0f + 24 - gate_.w / 2.0f - cam.x, d.ty * 16.0f + 16 - gate_.h - cam.y);
        break;
      case 3: {
        const Actor& a = g.actors[d.idx];
        float flash = a.flash > 0 ? a.flash / 0.12f : 0;
        float alpha = a.st == AState::Dead ? clampf(1.0f - (a.stT - 4.0f) / 2.0f, 0, 1) : 1.0f;
        if (a.player && a.iframes > 0 && a.st != AState::Roll && ((int)(t_ * 20) & 1)) alpha *= 0.5f;
        if (a.human) {
          const Tex& t = humanTex(a.look);
          int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
          bool flip = a.face == 3;
          int fr = 0;
          switch (a.st) {
            case AState::Walk: fr = 1 + (int)(a.animT * 9) % 4; break;
            case AState::Roll: fr = 1 + (int)(a.stT * 20) % 4; break;
            case AState::Windup: case AState::Cast: fr = 5; break;
            case AState::Strike: fr = 6; break;
            case AState::Recover: fr = 6; break;
            case AState::Hurt: case AState::Dead: fr = 7; break;
            default: fr = 0; break;
          }
          float bob = a.st == AState::Roll ? 3.0f : 0;
          float x = a.p.x - art::HUMAN_W / 2.0f - cam.x, y = a.p.y - art::HUMAN_H + 2 + bob - cam.y;
          if (a.st == AState::Dead) y += 3;
          P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, alpha));
          if (flash > 0) P.blitEx(t, fr * art::HUMAN_W, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, x, y, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, Color(1, 1, 1, flash), 1);
          if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + 5, y + 6, 6, 10, Color(0.6f, 0.25f, 0.05f, 0.6f));
        } else {
          const Tex& t = monsters_[(int)a.mon];
          int cw = art::monsterCellW(a.mon), chh = art::monsterCellH(a.mon);
          int fr = 0;
          switch (a.st) {
            case AState::Walk: fr = (int)(a.animT * (a.mon == Monster::Bat ? 14 : 8)) % 4; break;
            case AState::Windup: fr = 4; break;
            case AState::Strike: fr = 5; break;
            case AState::Hurt: fr = 6; break;
            case AState::Dead: case AState::Down: fr = 7; break;
            default: fr = (int)(a.animT * 3) % 2; break;
          }
          if (a.fly && a.mon == Monster::Dragon) fr = (int)(a.animT * 6) % 4;
          bool flip = a.aim.x < 0;
          float lift = a.fly ? 34 + std::sin(a.animT * 3) * 3 : (a.flying && a.st != AState::Dead ? 6 + std::sin(a.animT * 6) * 2 : 0);
          float x = a.p.x - cw / 2.0f - cam.x, y = a.p.y - chh + 2 - lift - cam.y;
          // collapsed bones rattle harder as they are about to stand back up
          if (a.st == AState::Down && a.stT > 1.2f) x += std::sin(t_ * 60) * std::min(2.0f, (a.stT - 1.2f) * 1.5f);
          // a heavy windup rears back; a wolf crouches before the lunge
          if (a.st == AState::Windup && a.heavy) y -= std::min(3.0f, a.stT * 6);
          if (a.st == AState::Windup && a.lunge) y += 1;
          P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, a.slowT > 0 ? 0.85f : 1, a.slowT > 0 ? 1 : 1, alpha));
          if (a.slowT > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(0.2f, 0.4f, 0.7f, 0.5f), 1);
          if (flash > 0) P.blitEx(t, fr * cw, 0, cw, chh, x, y, (float)cw, (float)chh, flip, Color(1, 1, 1, flash), 1);
          if (a.burnT > 0 && ((int)(t_ * 10) & 1)) P.rectAdd(x + cw * 0.3f, y + chh * 0.3f, cw * 0.4f, chh * 0.5f, Color(0.6f, 0.25f, 0.05f, 0.5f));
        }
        // melee slash arc
        if ((a.st == AState::Strike || (a.st == AState::Recover && a.stT < 0.05f)) && (a.player || a.human)) {
          art::Fx f = a.face == 0 ? art::Fx::SlashDown : a.face == 1 ? art::Fx::SlashUp : art::Fx::Slash;
          const Tex& ft = fx_[(int)f];
          int fw = art::fxW(f), fh = art::fxH(f), frames = std::max(1, art::fxFrames(f));
          float prog = a.st == AState::Strike ? a.stT / 0.11f : 1.0f;
          int fr = std::min(frames - 1, (int)(prog * frames));
          Vec2 c = a.p + Vec2(0, -8) + a.aim * 11;
          Color tint = a.player && a.combo == 2 ? Color(1, 0.9f, 0.6f) : Color(1, 1, 1, 0.9f);
          float sc = a.player && a.combo == 2 ? 1.3f : 1.0f;
          P.blitEx(ft, fr * fw, 0, fw, fh, c.x - fw * sc / 2 - cam.x, c.y - fh * sc / 2 - cam.y, fw * sc, fh * sc, a.face == 3, tint);
        }
        // attack telegraph
        if (a.hostile && a.st == AState::Windup && a.mon != Monster::Dragon) {
          float lift = a.human ? 30.0f : art::monsterCellH(a.mon) + 6.0f;
          float pulse = 0.6f + 0.4f * std::sin(t_ * 30);
          if (a.heavy) {
            // heavy slam: a ground ring that fills in as the blow comes, and a double "!!" - roll out (or through)
            float wu = a.mon == Monster::Troll ? 0.9f : 0.8f;
            float k = clampf(a.stT / wu, 0, 1);
            Vec2 c = a.p + a.aim * 10.0f;
            float r = 26.0f + a.radius;
            int seg = 44;
            for (int i = 0; i < seg; i++) {   // the danger zone: a solid outline...
              float an = i * TAU / seg;
              float px = c.x + std::cos(an) * r - cam.x, py = c.y + std::sin(an) * r * 0.55f - cam.y;
              P.rect(px - 1, py - 1, 2, 2, Color(1, 0.22f, 0.12f, 0.55f + 0.45f * k));
            }
            for (float ir = 4; ir < r * k; ir += 4) {   // ...that fills from the middle as the blow comes
              int n = std::max(8, (int)(ir * 1.2f));
              for (int i = 0; i < n; i++) {
                float an = i * TAU / n;
                P.rect(c.x + std::cos(an) * ir - cam.x, c.y + std::sin(an) * ir * 0.55f - cam.y, 1, 1, Color(1, 0.35f, 0.2f, 0.25f + 0.3f * k));
              }
            }
            Color rc(1, 0.2f + 0.3f * k, 0.15f, pulse);
            P.rect(a.p.x - 4 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
            P.rect(a.p.x - 4 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
            P.rect(a.p.x + 2 - cam.x, a.p.y - lift - 2 - cam.y, 2, 6, rc);
            P.rect(a.p.x + 2 - cam.x, a.p.y - lift + 5 - cam.y, 2, 2, rc);
          } else {
            Color tc = a.lunge ? Color(1, 0.65f, 0.2f, pulse) : Color(1, 0.3f, 0.2f, pulse);
            P.rect(a.p.x - 1 - cam.x, a.p.y - lift - cam.y, 2, 5, tc);
            P.rect(a.p.x - 1 - cam.x, a.p.y - lift + 6 - cam.y, 2, 2, tc);
            if (a.lunge) {   // a short streak showing where the lunge will go
              for (int i = 1; i <= 4; i++) {
                Vec2 q = a.p + Vec2(0, -3) + a.aim * (6.0f + i * 5.0f);
                P.rect(q.x - cam.x, q.y - cam.y, 1, 1, Color(1, 0.65f, 0.2f, 0.5f - i * 0.08f));
              }
            }
          }
        }
        // health bar for damaged enemies
        if (a.hostile && a.st != AState::Dead && a.st != AState::Down && a.hp < a.maxHp && !a.boss) {
          float lift = a.fly ? 34 : 0;
          float bw = 16, x = a.p.x - bw / 2 - cam.x, y = a.p.y - (a.human ? 26 : art::monsterCellH(a.mon) + 2) - lift - cam.y;
          P.rect(x - 1, y - 1, bw + 2, 4, Color(0.05f, 0.03f, 0.05f, 0.8f));
          P.rect(x, y, bw * clampf(a.hp / a.maxHp, 0, 1), 2, Color(0.85f, 0.2f, 0.18f));
        }
        break;
      }
      case 4: {
        const Pickup& k = g.pickups[d.idx];
        float bob = std::sin(t_ * 4 + d.idx) * 1.5f;
        const Tex& t = k.gold > 0 ? iconTex(art::Icon::Gold, 0) : iconTex(k.item.icon, k.item.tint);
        P.blitEx(shadow_, 0, 0, 14, 6, k.p.x - 5 - cam.x, k.p.y - 2 - cam.y, 10, 4, false, Color(1, 1, 1, 0.8f));
        P.blit(t, k.p.x - 8 - cam.x, k.p.y - 16 + bob - cam.y);
        if (k.gold == 0 && k.item.rarity >= Rarity::Rare && ((int)(t_ * 3 + d.idx) % 3 == 0))
          P.rectAdd(k.p.x - 1 - cam.x, k.p.y - 18 + bob - cam.y, 2, 2, col(rarityColor(k.item.rarity), 0.8f));
        break;
      }
      case 5: {
        const Projectile& pr = g.projs[d.idx];
        Vec2 dir = norm(pr.v);
        float x = pr.p.x - cam.x, y = pr.p.y - cam.y;
        switch (pr.kind) {
          case ProjKind::Arrow:
            for (int i = 0; i < 8; i++) P.rect(x - dir.x * i, y - dir.y * i, 1, 1, i < 2 ? Color(0.85f, 0.85f, 0.9f) : (i > 5 ? Color(0.95f, 0.95f, 0.95f) : Color(0.55f, 0.38f, 0.2f)));
            break;
          case ProjKind::Fireball: case ProjKind::DragonFire: {
            const Tex& t = fx_[(int)art::Fx::Fireball];
            int fw = art::fxW(art::Fx::Fireball), fh = art::fxH(art::Fx::Fireball), frames = std::max(1, art::fxFrames(art::Fx::Fireball));
            int fr = (int)(t_ * 14 + d.idx) % frames;
            float sc = pr.kind == ProjKind::DragonFire ? 1.5f : 1.0f;
            P.blitEx(t, fr * fw, 0, fw, fh, x - fw * sc / 2, y - fh * sc / 2, fw * sc, fh * sc, dir.x < 0);
            Rng r((uint32_t)(t_ * 60) + d.idx);
            Particle q; q.p = pr.p; q.v = Vec2(r.range(-10, 10), r.range(-10, 10)); q.life = q.max = 0.3f; q.c = Color(1, r.range(0.3f, 0.7f), 0.1f); q.size = 1; parts_.push_back(q);
            break;
          }
          case ProjKind::IceSpike: {
            const Tex& t = fx_[(int)art::Fx::Frost];
            int fw = art::fxW(art::Fx::Frost), fh = art::fxH(art::Fx::Frost);
            P.blitEx(t, 0, 0, fw, fh, x - fw / 2.0f, y - fh / 2.0f, (float)fw, (float)fh, dir.x < 0);
            break;
          }
          case ProjKind::Spit:
            P.rect(x - 2, y - 2, 4, 4, Color(0.55f, 0.85f, 0.3f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.8f, 1, 0.6f));
            break;
          case ProjKind::Magic:
            P.rectAdd(x - 3, y - 3, 6, 6, Color(0.4f, 0.3f, 0.9f, 0.7f));
            P.rect(x - 1, y - 1, 2, 2, Color(0.85f, 0.8f, 1));
            break;
        }
        break;
      }
      default: break;
    }
  }
  // particles (world layer)
  for (const Particle& q : parts_) {
    float a = clampf(q.life / q.max, 0, 1);
    if (q.kind == 1) {
      art::Fx f = (art::Fx)q.fx;
      const Tex& t = fx_[q.fx];
      int fw = art::fxW(f), fh = art::fxH(f), frames = std::max(1, art::fxFrames(f));
      int fr = std::min(frames - 1, (int)((1 - a) * frames));
      P.blitRegion(t, fr * fw, 0, fw, fh, q.p.x - fw / 2.0f - cam.x, q.p.y - fh / 2.0f - cam.y);
      continue;
    }
    if (q.layer == 1) { P.rectAdd(q.p.x - cam.x, q.p.y - cam.y, 1, 1, Color(q.c.r, q.c.g, q.c.b, a * (0.5f + 0.5f * std::sin(t_ * 5 + q.p.x)))); continue; }
    P.rect(q.p.x - cam.x, q.p.y - cam.y, q.size, q.size, Color(q.c.r, q.c.g, q.c.b, std::min(1.0f, a * 1.5f)));
  }
}

void View::drawLighting(Game& g) {
  Pix& P = *pix_;
  const Map& m = g.mode == Mode::Title ? g.world.over : g.map();
  float day = g.daylight();
  Color amb;
  bool interior = g.mode != Mode::Title && g.inside && g.subBldg >= 0;
  bool dungeon = g.mode != Mode::Title && g.inside && g.subSite >= 0;
  if (dungeon) amb = Color(0.30f, 0.27f, 0.34f);
  else if (interior) amb = Color(0.78f, 0.68f, 0.58f);
  else {
    Color night(0.12f, 0.15f, 0.30f), dusk(1.0f, 0.72f, 0.55f), noon(1, 1, 1);
    float h = g.hour;
    bool evening = h > 12;
    if (day >= 1) amb = noon;
    else if (day <= 0) amb = night;
    else {
      Color mid = evening ? dusk : Color(1.0f, 0.82f, 0.7f);
      amb = day > 0.5f ? Color(lerpf(mid.r, 1, (day - 0.5f) * 2), lerpf(mid.g, 1, (day - 0.5f) * 2), lerpf(mid.b, 1, (day - 0.5f) * 2))
                       : Color(lerpf(night.r, mid.r, day * 2), lerpf(night.g, mid.g, day * 2), lerpf(night.b, mid.b, day * 2));
    }
  }
  if (amb.r > 0.99f && amb.g > 0.99f && amb.b > 0.99f) return;
  Vec2 cam(std::floor(cam_.x + shakeOff_.x), std::floor(cam_.y + shakeOff_.y));
  P.setTarget(&lightMap_);
  SDL_Renderer* ren = P.renderer();
  (void)ren;
  P.rect(0, 0, (float)lightMap_.w, (float)lightMap_.h, amb);
  float dark = 1 - (amb.r + amb.g + amb.b) / 3;
  auto light = [&](Vec2 wp, float r, Color c, float k) {
    float x = (wp.x - cam.x) * 0.5f, y = (wp.y - cam.y) * 0.5f, rr = r * 0.5f;
    if (x < -rr || y < -rr || x > lightMap_.w + rr || y > lightMap_.h + rr) return;
    P.blitEx(light_, 0, 0, 64, 64, x - rr, y - rr, rr * 2, rr * 2, false, Color(c.r, c.g, c.b, k), 1);
  };
  float flick = 0.9f + 0.1f * std::sin(t_ * 13) * std::sin(t_ * 7.3f);
  // the player carries a torch at night / underground
  if (g.mode != Mode::Title) light(g.pl().p + Vec2(0, -8), dungeon ? 150.0f : 100.0f, Color(1, 0.85f, 0.65f), (dungeon ? 0.95f : 0.8f * dark) * flick);
  int tx0 = (int)std::floor(cam.x / 16) - 6, ty0 = (int)std::floor(cam.y / 16) - 6;
  for (int ty = std::max(0, ty0); ty < std::min(m.h, ty0 + Pix::H / 16 + 12); ty++)
    for (int tx = std::max(0, tx0); tx < std::min(m.w, tx0 + Pix::W / 16 + 12); tx++) {
      int pr = m.prop[(size_t)ty * m.w + tx];
      if (!pr) continue;
      float r; Color c;
      if (!propLight((Prop)(pr - 1), r, c)) continue;
      float f = 0.85f + 0.15f * std::sin(t_ * 9 + tx * 1.7f + ty);
      light(Vec2(tx * 16 + 8.0f, ty * 16 + 4.0f), r, c, 0.9f * f);
    }
  // warm windows at night
  if (!g.inside && dark > 0.2f)
    for (const Bldg& b : m.bldgs) {
      Vec2 c(b.r.x * 16 + b.r.w * 8.0f, (b.r.y + b.r.h) * 16 - 10.0f);
      if (c.x < cam.x - 80 || c.x > cam.x + Pix::W + 80 || c.y < cam.y - 80 || c.y > cam.y + Pix::H + 80) continue;
      if (hash32((uint32_t)(b.seed + g.day)) % 3 == 0) continue;   // not every house is awake
      light(c + Vec2(0, 4), 18 + b.r.w * 4.0f, Color(1, 0.7f, 0.35f), 0.45f * dark);
    }
  for (const Projectile& pr : g.projs) {
    if (pr.kind == ProjKind::Fireball || pr.kind == ProjKind::DragonFire) light(pr.p, 60, Color(1, 0.6f, 0.25f), 0.9f);
    else if (pr.kind == ProjKind::IceSpike || pr.kind == ProjKind::Magic) light(pr.p, 40, Color(0.5f, 0.6f, 1), 0.8f);
  }
  for (const Particle& q : parts_) if (q.kind == 1 && q.fx == (int)art::Fx::Explosion) light(q.p, 90, Color(1, 0.6f, 0.3f), q.life / q.max);
  for (const Actor& a : g.actors) if (a.mon == Monster::Wraith && a.hostile && a.st != AState::Dead) light(a.p + Vec2(0, -10), 40, Color(0.5f, 0.5f, 1), 0.6f);
  for (const Pickup& k : g.pickups) if (k.gold == 0 && k.item.rarity >= Rarity::Rare) light(k.p, 24, col(rarityColor(k.item.rarity)), 0.5f);
  P.setTarget(nullptr);
  P.blitEx(lightMap_, 0, 0, lightMap_.w, lightMap_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, 1), 2);
}

void View::drawWeather(Game& g, float dt) {
  (void)dt;
  Pix& P = *pix_;
  if (g.inside || g.mode == Mode::Title) {
    P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, g.inside && g.subSite >= 0 ? 1.0f : 0.6f));
    return;
  }
  const Actor& p = g.pl();
  Biome b = g.world.over.biomeAt((int)(p.p.x / 16), (int)(p.p.y / 16));
  uint32_t w = hash32((uint32_t)(g.day * 8 + (int)(g.hour / 3)) ^ (uint32_t)g.seed) % 10;
  bool cold = b == Biome::Snow || b == Biome::Mountain || b == Biome::Taiga;
  bool rain = w < 3 && !cold && b != Biome::Desert;
  bool snow = cold && w < 6;
  bool fog = b == Biome::Swamp;
  if (rain) {
    P.rect(0, 0, Pix::W, Pix::H, Color(0.1f, 0.12f, 0.2f, 0.18f));
    for (int i = 0; i < 90; i++) {
      float sx = std::fmod(hashf(i, 0, 5) * 600 + t_ * 120 - cam_.x * 1.0f, 520.0f) - 20;
      float sy = std::fmod(hashf(i, 1, 5) * 400 + t_ * 330 - cam_.y * 1.0f, 300.0f) - 15;
      if (sx < 0) sx += 520; if (sy < 0) sy += 300;
      for (int k = 0; k < 4; k++) P.rect(sx - k * 0.5f, sy - k * 1.5f, 1, 1, Color(0.7f, 0.8f, 0.95f, 0.45f - k * 0.08f));
    }
  }
  if (snow) {
    for (int i = 0; i < 110; i++) {
      float sp = 14 + hashf(i, 3, 7) * 18;
      float sx = std::fmod(hashf(i, 0, 7) * 600 + std::sin(t_ * 0.8f + i) * 10 - cam_.x * 1.0f + t_ * 6, 500.0f) - 10;
      float sy = std::fmod(hashf(i, 1, 7) * 400 + t_ * sp - cam_.y * 1.0f, 290.0f) - 10;
      if (sx < 0) sx += 500; if (sy < 0) sy += 290;
      float s = hashf(i, 2, 7) < 0.3f ? 2.f : 1.f;
      P.rect(sx, sy, s, s, Color(1, 1, 1, 0.8f));
    }
  }
  if (fog) {
    for (int i = 0; i < 6; i++) {
      float x = std::fmod(i * 140 + t_ * 8 - cam_.x * 0.3f, 700.0f) - 160;
      P.blitEx(light_, 0, 0, 64, 64, x, 40 + i * 30 + std::sin(t_ * 0.3f + i) * 10, 260, 90, false, Color(0.75f, 0.8f, 0.75f, 0.12f));
    }
  }
  // falling leaves / fireflies are world particles drawn earlier; light vignette always
  P.blitEx(vignette_, 0, 0, vignette_.w, vignette_.h, 0, 0, (float)Pix::W, (float)Pix::H, false, Color(1, 1, 1, 0.45f));
}
