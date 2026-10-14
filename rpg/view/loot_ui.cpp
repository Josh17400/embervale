// M8b Treasure (VIEW lane; VISION_PLAN 15.27): the rarity ladder on every surface. The owner: "Every item drawn on
// screen must use these colours: its name, frame and icon, the loot beam or sparkle, the paper doll, the shop, the chest
// and the forge" (Common white, Uncommon green, Rare blue, Epic purple, Legendary gold, Mythic a flaming red-and-orange,
// animated shimmer on the name, frame and icon border). "Every affix must show in the item tooltip and compare panel as
// plain words." "Show the item level in the tooltip, and compare against what is equipped (green or red arrows)."
//
// Here: the shared pieces every screen uses (rarityText, rarityFrame, rarityIcon, cmpArrow, itemCard with its compare),
// the loot on the ground (drawLootGround in the sorted world pass, lootLights in the light map: an escalating presence
// from an Uncommon's glint to a Mythic's flaming pillar), the LootChime for Epic and better finds, the item notices in
// their rarity and the embers of a Mythic wearer (hero and companions alike: gear_look marks their glow).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/gear.h"
#include "rpg/sim/gear_look.h"
#include "rpg/view/loot_paint.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
using lootp::colU;
using lootp::flameAt;
// a Mythic wearer: gear_look gives a Mythic piece's glow this exact colour (rpg/sim/gear_look.h kMythicGlow)
bool mythicWearer(const Actor& a) { return a.human && a.look.glow && a.look.glowColor == kMythicGlow; }
}  // namespace

// the game's painter for rpg/view/loot_paint.h: Pix rects and blends, the baked beam column and light blob
struct View::LootPainter {
  View& v;
  const Game* g;   // the icons' culture lookup (null where no icon is drawn: text, frames, arrows)
  Pix& P;
  void rect(float x, float y, float w, float h, Color c) { P.rect(x, y, w, h, c); }
  void add(float x, float y, float w, float h, Color c) { P.rectAdd(x, y, w, h, c); }
  void frame(float x, float y, float w, float h, Color c) { P.frame(x, y, w, h, c); }
  void text(float x, float y, const std::string& s, Color c, int align) { P.text(x, y, s, 1, c, align); }
  int textW(const std::string& s) const { return P.textW(s, 1); }
  void beam(float x, float y, float w, float h, Color c, int sy, int sh, bool add) { P.blitEx(v.beamTex(), 0, sy, 16, sh, x, y, w, h, false, c, add ? 1 : 0); }
  void pool(float x, float y, float w, float h, Color c) { P.blitEx(v.light_, 0, 0, 64, 64, x, y, w, h, false, c, 1); }
  void shadow(float x, float y, float w, float h, float a) { P.blitEx(v.shadow_, 0, 0, 14, 6, x, y, w, h, false, Color(1, 1, 1, a)); }
  void noted(Rarity) { v.lootCount_.frames++; }
  void icon(const Item& it, float x, float y, float size) { if (g) P.blitEx(v.itemTex(*g, it), 0, 0, 16, 16, x, y, size, size); }
};

const Tex& View::beamTex() {
  if (!beamTex_.t) {   // the beam column (loot_paint.h beamProfile), white: tinted additively per rarity
    Canvas c(16, 64);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < 16; x++) c.set(x, y, rgba(255, 255, 255, (int)std::lround(255 * lootp::beamProfile((float)x, (float)y))));
    beamTex_ = pix_->bake(c);
  }
  return beamTex_;
}

// (M8b fixer r3, review: "a Mythic-geared hero looks plain in the world: the glint is a few stray orange pixels") the
// Mythic wearer's flame aura: per standing-sheet cell, a fire that hugs the whole silhouette (a hot yellow-orange ring
// against the sprite's dark outline, an orange band, a red fringe) and licks upward in tongues off the head and
// shoulders; AURA_V flicker variants per cell. Drawn behind the body in the world pass (normal blend: it reads on a sunlit
// field) and again additively after the lighting, so it burns at night. Cells are AURA_CW x AURA_CH (the 16 x 24 cell
// with AURA_PAD round it), laid in one row: ((row * HUMAN_FRAMES + frame) * AURA_V + variant).
const Tex* View::mythicAuraTex(const art::HumanLook& L) {
  const uint64_t k = L.key() ^ 0xA0A5F1A3E5ull;
  auto it = mythicAura_.find(k);
  if (it != mythicAura_.end()) return &it->second;
  const Canvas S = art::humanSheet(L);
  constexpr int W = art::HUMAN_W, H = art::HUMAN_H, PD = AURA_PAD, CW = AURA_CW, CH = AURA_CH;
  Canvas out(CW * art::HUMAN_FRAMES * 3 * AURA_V, CH);
  std::vector<uint8_t> ring((size_t)CW * CH);
  for (int row = 0; row < 3; row++)
    for (int f = 0; f < art::HUMAN_FRAMES; f++) {
      // the silhouette and its rings (1: 4-neighbours of the body, 2: 8-neighbours of ring 1, 3: of ring 2)
      std::fill(ring.begin(), ring.end(), (uint8_t)0);
      auto at = [&](int x, int y) -> uint8_t& { return ring[(size_t)y * CW + x]; };
      int feet = -1, top = CH;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          if ((S.get(f * W + x, row * H + y) >> 24) > 40) { at(x + PD, y + PD) = 9; feet = std::max(feet, y + PD); top = std::min(top, y + PD); }
      if (feet < 0) continue;
      for (int r = 1; r <= 3; r++) {
        const uint8_t want = r == 1 ? 9 : (uint8_t)(r - 1);
        for (int y = 0; y < CH; y++)
          for (int x = 0; x < CW; x++) {
            if (at(x, y)) continue;
            bool hit = false;
            for (int dy = -1; dy <= 1 && !hit; dy++)
              for (int dx = -1; dx <= 1 && !hit; dx++) {
                if (dx == 0 && dy == 0) continue;
                if (r == 1 && dx != 0 && dy != 0) continue;   // ring 1 hugs the outline (4-neighbours)
                const int xx = x + dx, yy = y + dy;
                if (xx >= 0 && yy >= 0 && xx < CW && yy < CH && at(xx, yy) == want) hit = true;
              }
            if (hit) at(x, y) = (uint8_t)(10 + r);   // marked this pass, settled below so the pass grows from the last only
          }
        for (uint8_t& v : ring) if (v > 10) v = (uint8_t)(v - 10);
      }
      for (int v = 0; v < AURA_V; v++) {
        const int ox = ((row * art::HUMAN_FRAMES + f) * AURA_V + v) * CW;
        auto noise = [&](int x, int y, uint32_t salt) { return hash32((uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(v * 83492791) ^ (uint32_t)(f * 2654435761u) ^ salt) % 100u; };
        for (int y = 0; y < CH; y++)
          for (int x = 0; x < CW; x++) {
            const uint8_t r = at(x, y);
            if (!r || r == 9 || y > feet) continue;
            if (y >= feet - 1 && r > 1) continue;   // no fire on the ground under the feet
            const float up = 1.0f - (float)(y - top) / (float)std::max(1, feet - top);   // 1 at the head, 0 at the feet
            if (r == 1) { out.set(ox + x, y, (int)noise(x, y, 0x11u) < 80 ? rgba(255, 190, 80, 220) : rgba(255, 150, 56, 210)); continue; }
            if (r == 2) { if ((int)noise(x, y, 0x21u) < 80 + (int)(20 * up)) out.set(ox + x, y, rgba(246, 108, 38, 205)); continue; }
            // the red fringe only where it rises off the band (a flame's edge, never loose specks beside the legs)
            const bool rises = y + 1 < CH && at(x, y + 1) == 2;
            if (rises && (int)noise(x, y, 0x31u) < 20 + (int)(60 * up)) out.set(ox + x, y, rgba(184, 40, 24, 160));
          }
        // tongues: off the upper body's top edge, a few columns lick 1-4 px higher (hot at the root, red at the tip)
        for (int x = 1; x < CW - 1; x++) {
          int ty = -1;
          for (int y = 0; y < CH; y++) if ((out.get(ox + x, y) >> 24) > 0) { ty = y; break; }
          if (ty < 1 || ty > top + 10) continue;
          const uint32_t n = noise(x, ty, 0x41u);
          if (n >= 45) continue;
          const int len = 1 + (int)(n % 4u);
          for (int j = 1; j <= len && ty - j >= 0; j++)
            out.set(ox + x, ty - j, j == len ? rgba(150, 30, 20, 120) : j == 1 ? rgba(246, 108, 38, 200) : rgba(200, 56, 26, 165));
        }
      }
    }
  return &(mythicAura_[k] = pix_->bake(out));
}

void View::mythicAuraAt(const art::HumanLook& L, int frame, int row, bool flip, float x, float y, float sc) {
  if (!L.glow || L.glowColor != kMythicGlow) return;
  const Tex* au = mythicAuraTex(L);
  if (!au) return;
  const int v = (int)(t_ * 9.0f) % AURA_V;
  const int sx = ((row * art::HUMAN_FRAMES + frame) * AURA_V + v) * AURA_CW;
  pix_->blitEx(*au, sx, 0, AURA_CW, AURA_CH, x - AURA_PAD * sc, y - AURA_PAD * sc, AURA_CW * sc, AURA_CH * sc, flip, Color(1, 1, 1, 0.95f));
}

// ------------------------------------------------------------------------------------------------ the View's wrappers
void View::rarityText(float x, float y, const std::string& s, Rarity r, float alpha, int align, bool shadow) {
  LootPainter lp{*this, nullptr, *pix_};
  if (r == Rarity::Mythic) lootCount_.mythicText++;
  lootp::rarityText(lp, t_, x, y, s, r, alpha, align, shadow);
}
void View::rarityFrame(float x, float y, float w, float h, Rarity r, bool hot, bool small) {
  LootPainter lp{*this, nullptr, *pix_};
  lootp::rarityFrame(lp, t_, x, y, w, h, r, hot, small);
}
void View::rarityIcon(const Game& g, const Item& it, float x, float y, float size) {
  LootPainter lp{*this, &g, *pix_};
  lootp::rarityIcon(lp, t_, it, x, y, size);
}
void View::cmpArrow(float x, float y, int dir) {
  LootPainter lp{*this, nullptr, *pix_};
  lootp::arrow(lp, x, y, dir);
}
const Item* View::wornFor(const Game& g, const Item& it, int selfIndex, bool& isWorn) const {
  isWorn = false;
  if (!itemEquippable(it.kind)) return nullptr;
  int* e = const_cast<Game&>(g).equipSlot(it.kind);
  if (!e || *e < 0 || *e >= (int)g.inv.size()) return nullptr;
  if (*e == selfIndex) { isWorn = true; return nullptr; }
  return &g.inv[(size_t)*e];
}
int View::cmpVerdict(const Game& g, const Item& it, const Item* vs) const { return lootp::verdict(it, vs, g.plLevel); }
// (M8b fixer r2, review: "tapping the greyed ATTUNED FULL button does nothing") the pack's use: a great piece that
// cannot be worn now is not handed to Game::equip (its notice would hide behind the open menu): the card's reason line
// blinks and the refusal sounds instead
void View::packUse(Game& g, int i) {
  if (i < 0 || i >= (int)g.inv.size()) return;
  bool isWorn = false;
  const Item* vs = wornFor(g, g.inv[(size_t)i], i, isWorn);
  if (!isWorn && attuneWhy(g, g.inv[(size_t)i], vs)) { lootBlockFlash_ = 1.2f; if (audio_) audio_->play(Sfx::MenuBack); return; }
  g.useItem(i);
}
// (M8b fixer r2) why a great piece cannot be worn now (0 it can; 1 below level 10; 2 every attunement taken, name: a
// worn great piece other than the one it would replace), as Game::equip judges it (game_rpg.cpp)
int View::attuneWhy(const Game& g, const Item& it, const Item* vs, std::string* name) const {
  if (!itemEquippable(it.kind) || !gear::attunes(it)) return 0;
  const int slots = gear::attunementSlots(g.plLevel);
  if (slots == 0) return 1;
  int worn = g.craft.live.stats.legendaries;
  if (vs && gear::attunes(*vs)) worn--;
  if (worn < slots) return 0;
  if (name) {
    name->clear();
    for (size_t i = 0; i < g.inv.size() && name->empty(); i++) {
      const Item& o = g.inv[i];
      if (&o == vs || &o == &it || !itemEquippable(o.kind) || !gear::attunes(o)) continue;
      const int* e = const_cast<Game&>(g).equipSlot(o.kind);
      if (e && *e == (int)i) *name = o.name;
    }
  }
  return 2;
}

float View::itemCard(const Game& g, const Item& it, float x, float y, float w, float maxY, bool cmp, const Item* vs, bool worn, bool bigIcon) {
  LootPainter lp{*this, &g, *pix_};
  lootp::CardCtx X;
  X.level = g.plLevel;
  X.attuned = g.craft.live.stats.legendaries;
  X.maker = it.culture && g.world.src ? &g.world.src->culture(it.culture) : nullptr;
  if (cmp && !worn) X.block = attuneWhy(g, it, vs, &X.blockName);
  X.flash = X.block && lootBlockFlash_ > 0 && std::fmod(lootBlockFlash_, 0.3f) > 0.12f;
  X.skills = &g.skills;          // (M9) the family / armour lines at the hero's levels, what a book teaches
  X.book = &g.arcana.book;
  lootp::CardInfo info;
  const float yb = lootp::itemCard(lp, t_, X, it, x, y, w, maxY, cmp, vs, worn, bigIcon, &info);
  lootCount_.cards++;
  if (info.compare) lootCount_.compares++;
  if (it.rarity == Rarity::Mythic) lootCount_.mythicText++;
  lastCard_ = info;
  return yb;
}

// ------------------------------------------------------------------------------------------------ the loot on the ground
// The sorted world pass draws each pickup here (the beam stands behind its icon; actors and props south of it are drawn
// after and stand in front, so a beam never hides them); loot_paint.h lootGround draws it.
void View::drawLootGround(Game& g, const Pickup& k, int idx, Vec2 cam, float bob) {
  LootPainter lp{*this, &g, *pix_};
  const int r = k.gold > 0 || !itemEquippable(k.item.kind) ? 0 : (int)k.item.rarity;
  if (r > 0) lootCount_.beams[std::min(r, 5)]++;
  const float d = std::sqrt(len2(k.p - g.pl().p));
  const float near = clampf((d - 12.0f) / 36.0f, 0.35f, 1.0f);   // the beam fades as the hero walks right up to it
  if (k.gold > 0) {
    lp.shadow(std::floor(k.p.x - cam.x) - 5, std::floor(k.p.y - cam.y) - 2, 10, 4, 0.8f);
    pix_->blit(iconTex(art::Icon::Gold, 0), std::floor(k.p.x - cam.x) - 8, std::floor(k.p.y - cam.y) - 16 + bob);
    return;
  }
  lootp::lootGround(lp, t_, &k.item, r, k.p.x - cam.x, k.p.y - cam.y, bob, near, idx);
  // (M8b fixer r2, review: "Legendary and Mythic pieces are washed out by a beam drawn over them") a beam rises over
  // the ground north of its pickup, which the sorted pass has already drawn: a piece lying in that column is drawn
  // again on top, so every beam stands behind every other piece (its own colours, never bleached by a neighbour's)
  if (r >= 2) {
    const float reach = r == 2 ? 36.0f : r == 3 ? 48.0f : r == 4 ? 62.0f : 84.0f;
    for (size_t j = 0; j < g.pickups.size(); j++) {
      if ((int)j == idx) continue;
      const Pickup& o = g.pickups[j];
      const float dy = k.p.y - o.p.y;
      if (dy <= 0 || dy > reach + 16 || std::fabs(o.p.x - k.p.x) > 20) continue;
      const float ob = std::sin(t_ * 4 + (float)j) * 1.5f;   // its bob (render.cpp's sorted pass)
      const float ox = std::floor(o.p.x - cam.x), oy = std::floor(o.p.y - cam.y);
      if (o.gold > 0) pix_->blit(iconTex(art::Icon::Gold, 0), ox - 8, oy - 16 + ob);
      else lp.icon(o.item, ox - 8, oy - 16 + ob, 16);
    }
  }
}

// the lights loot carries into the dark (drawLighting): the pool at the foot and the column above it
void View::lootLights(Game& g, const std::function<void(Vec2, float, Color, float)>& light) {
  // (M8b fix) only what can light the view: the biggest blob reaches 58 px, the column 40 px above the foot
  const float vx0 = cam_.x - 64, vy0 = cam_.y - 64, vx1 = cam_.x + (float)Pix::W + 64, vy1 = cam_.y + (float)Pix::H + 104;
  for (size_t i = 0; i < g.pickups.size(); i++) {
    const Pickup& k = g.pickups[i];
    if (k.gold || !itemEquippable(k.item.kind) || k.item.rarity == Rarity::Common) continue;
    if (k.p.x < vx0 || k.p.y < vy0 || k.p.x > vx1 || k.p.y > vy1) continue;
    const Color rc = colU(rarityColor(k.item.rarity));
    const float tt = t_ + (float)i * 0.71f;
    switch (k.item.rarity) {
      case Rarity::Uncommon: light(k.p + Vec2(0, -4), 16, rc, 0.30f); break;
      case Rarity::Rare: light(k.p + Vec2(0, -4), 26, rc, 0.45f); light(k.p + Vec2(0, -22), 22, rc, 0.36f); break;
      case Rarity::Epic: light(k.p + Vec2(0, -4), 34, rc, 0.52f); light(k.p + Vec2(0, -28), 26, rc, 0.42f); break;
      case Rarity::Legendary: {
        const float pulse = 0.88f + 0.12f * std::sin(tt * 2.4f);
        light(k.p + Vec2(0, -6), 50, rc, 0.62f * pulse);
        light(k.p + Vec2(0, -36), 32, rc, 0.46f * pulse);
        break;
      }
      case Rarity::Mythic: {
        const float fl = 0.80f + 0.12f * std::sin(tt * 13.0f) * std::sin(tt * 7.3f) + 0.08f * std::sin(tt * 3.1f);
        light(k.p + Vec2(0, -8), 58, Color(1.0f, 0.50f, 0.20f), 0.70f * fl);
        light(k.p + Vec2(0, -40), 36, Color(1.0f, 0.36f, 0.12f), 0.52f * fl);
        break;
      }
      default: break;
    }
  }
  // a Mythic wearer smoulders: a small warm flicker round them
  for (const Actor& a : g.actors)
    if (mythicWearer(a) && a.st != AState::Dead && a.p.x >= vx0 && a.p.y >= vy0 && a.p.x <= vx1 && a.p.y <= vy1) light(a.p + Vec2(0, -10), 34, Color(1.0f, 0.45f, 0.18f), 0.30f + 0.08f * std::sin(t_ * 11 + a.id));
}

// once a frame (update): the chime for an Epic or better piece coming into view, the embers off a Mythic wearer
void View::lootUpdate(Game& g, float dt) {
  lootBlockFlash_ = std::max(0.0f, lootBlockFlash_ - dt);
  if (g.mode == Mode::Play) {
    const float vx0 = cam_.x - 8, vy0 = cam_.y - 8, vx1 = cam_.x + (float)Pix::W + 8, vy1 = cam_.y + (float)Pix::H + 8;
    int best = -1;
    // (M8b fixer r3) a piece chimes once: the flag rides on the pickup itself, so the pull toward the hero and an endless
    // window shift (both move k.p) no longer make it a 'new' find
    for (Pickup& k : g.pickups) {
      if (k.sighted || k.gold || !itemEquippable(k.item.kind) || k.item.rarity < Rarity::Epic) continue;
      if (k.p.x < vx0 || k.p.y < vy0 || k.p.x > vx1 || k.p.y > vy1) continue;
      k.sighted = true;
      best = std::max(best, (int)k.item.rarity);
    }
    if (best >= (int)Rarity::Epic && t_ - lootChimeT_ > 0.6f) {
      audio_->play(Sfx::LootChime, best == (int)Rarity::Mythic ? 1.26f : best == (int)Rarity::Legendary ? 1.12f : 1.0f, best >= (int)Rarity::Legendary ? 0.9f : 0.75f);
      lootChimeT_ = t_;
      lootCount_.chimes++;
    }
  }
  // (M8b fixer r3) the Mythic aura sheets: a handful of looks at most (the hero, companions); past that start again,
  // here between frames (the draw's glow list holds pointers into the cache until the frame is presented)
  if (mythicAura_.size() > 16) {
    for (auto& kv : mythicAura_) pix_->destroy(kv.second);
    mythicAura_.clear();
  }
  // embers rising off a Mythic wearer (the hero, a companion: gear_look's flame glow on their sheet)
  emberT_ += dt;
  // (M8b fix, review: "at 1x a Mythic wearer looks ordinary") twice as many, some two pixels big at the root, rising
  // from the silhouette's edges (shoulders, flanks, hands) so they lick the outline rather than float off the head
  if (emberT_ >= 0.045f) {
    emberT_ = 0;
    for (const Actor& a : g.actors) {
      if (!mythicWearer(a) || a.st == AState::Dead) continue;
      if (a.p.x < cam_.x - 24 || a.p.y < cam_.y - 24 || a.p.x > cam_.x + (float)Pix::W + 24 || a.p.y > cam_.y + (float)Pix::H + 24) continue;
      const uint32_t h = hash32((uint32_t)a.id * 2654435761u ^ (uint32_t)(t_ * 197.0f));
      Particle q;
      const int side = (h & 1u) ? 1 : -1;
      q.p = a.p + Vec2((float)(side * (4 + (int)((h >> 1) % 3u))), -2.0f - (float)((h >> 8) % 18u));
      q.v = Vec2((float)((int)((h >> 16) % 9u) - 4) * 1.5f, -16.0f - (float)((h >> 20) % 10u));
      q.life = q.max = 0.55f + 0.04f * (float)((h >> 24) % 6u);
      const Color fc = flameAt(t_, (float)(h % 7u), 0.2f + 0.1f * (float)((h >> 4) % 4u));
      q.c = fc;
      q.size = ((h >> 12) % 3u) == 0 ? 2 : 1;
      q.grav = -6;
      q.layer = 0;   // a solid pixel (an additive one vanishes on a sunlit field): the hot core of an ember
      parts_.push_back(q);
      lootCount_.embers++;
    }
  }
  lootShown_ = lootCount_;
  const int chimes = lootCount_.chimes, embers = lootCount_.embers;
  lootCount_ = LootStats();
  lootCount_.chimes = chimes;
  lootCount_.embers = embers;
}

// an item's pickup notice (Ev::Notice in the rarity's colour, Game::addItem): the toast keeps the rarity (its name is
// drawn in the ladder colour, a Mythic's in the flame) and an Epic or better find chimes, unless it just did on sight
void View::lootNotice(Game& g, const Event& e, Toast& t) {
  (void)g;
  for (int r = 1; r < RARITIES; r++)
    if ((uint32_t)e.a == rarityColor((Rarity)r)) t.rarity = r;
  if (t.rarity >= (int)Rarity::Epic && t_ - lootChimeT_ > 3.0f) {
    audio_->play(Sfx::LootChime, t.rarity == (int)Rarity::Mythic ? 1.26f : t.rarity == (int)Rarity::Legendary ? 1.12f : 1.0f, 0.8f);
    lootChimeT_ = t_;
    lootCount_.chimes++;
  }
}

// (scripts) the cursor on a row, as the keys put it (script_loot.cpp `lootsel`)
bool View::lootSelect(Game& g, const std::vector<std::string>& a, std::string& err) {
  auto idx = [&](const std::string& w, int n) { return w == "last" ? n - 1 : std::atoi(w.c_str()); };
  const std::string what = a.size() > 1 ? a[1] : std::string();
  if (what == "menu") {
    const int n = (int)g.inv.size();
    if (!n || a.size() < 3) { err = "lootsel menu <i|last>: an empty pack"; return false; }
    if (g.mode != Mode::Menu) openMenu(g, 0);
    menuTab_ = 0;
    menuSel_ = std::clamp(idx(a[2], n), 0, n - 1);
    return true;
  }
  if (what == "shop") {
    if (g.mode != Mode::Shop || a.size() < 4) { err = "lootsel shop <buy|sell> <i|last>: no shop open"; return false; }
    shopSide_ = a[2] == "sell" ? 1 : 0;
    const int n = shopSide_ == 0 ? (int)g.shop.stock.size() : (int)g.inv.size();
    if (!n) { err = "lootsel shop: that list is empty"; return false; }
    shopSel_ = std::clamp(idx(a[3], n), 0, n - 1);
    shopArm_ = -1;
    return true;
  }
  if (what == "doll") {
    if (a.size() < 4) { err = "lootsel doll <slot 0-10> <cand>"; return false; }
    if (g.mode != Mode::Menu) openMenu(g, 5);
    menuTab_ = 5;
    paperdollPeek(g, std::clamp(std::atoi(a[2].c_str()), 0, 10), idx(a[3], 64));
    return true;
  }
  err = "lootsel menu|shop|doll ...";
  return false;
}
