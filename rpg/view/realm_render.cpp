// M4 "Banners" VIEW lane: the realm in the world (VISION_PLAN 4.5).
//   - war props (tents, the command tent, catapults, palisades, refugee tents, barricades) in the colours of the
//     kingdom whose camp they belong to: Game::warPropKingdom (WARDS), else the realm's siege whose camp lies within 40
//     tiles (its attacker); art::warPropSprite cached by (prop, field, trim)
//   - burned buildings (Bldg::charred 1 scorched, 2 burned out) smoulder: dark smoke columns from the broken roof, a
//     drift of sparks and soot, a red ember glow at night
//   - siege camps at night: lantern glow in the tents and the command pavilion (the campfires light themselves:
//     prop_traits propLight)
//   - nearSiege: the player stands in or near a besieged settlement (the Siege music)
#include <algorithm>
#include <cmath>
#include "rpg/art/art_building.h"
#include "rpg/art/art_props.h"
#include "rpg/view/realm_ui.h"
#include "rpg/view/view.h"

namespace {
// the kingdom whose camp a war prop at window tile (tx, ty) belongs to
ew::Gid campOwner(Game& g, art::Prop p, int tx, int ty) {
  ew::Gid k = g.warPropKingdom(tx, ty);
  if (k) return k;
  if (p == art::Prop::RefugeeTent || p == art::Prop::Rubble || p == art::Prop::Ash || p == art::Prop::Scaffold) return 0;
  const int32_t gx = g.world.ox + tx, gy = g.world.oy + ty;
  // the nearest siege still fought whose camp, or whose besieged settlement, lies within 80 tiles
  int64_t bd = 80 * 80;
  for (const realm::Siege& s : g.realm.sieges()) {
    if (s.over) continue;
    int64_t dx = s.campX - gx, dy = s.campY - gy, d = dx * dx + dy * dy;
    if (const realm::SettlementState* st = g.realm.settlement(s.site)) {
      const int64_t ex = st->gx - gx, ey = st->gy - gy;
      d = std::min(d, ex * ex + ey * ey);
    }
    if (d <= bd) { bd = d; k = s.attacker; }
  }
  return k;
}
}  // namespace

const Tex* View::warPropTex(Game& g, art::Prop p, int tx, int ty) {
  m4Count_.warProps++;
  const ew::Gid k = campOwner(g, p, tx, ty);
  uint32_t field = 0, trim = 0;
  if (k) {
    const rui::Look L = rui::look(g, k);
    if (L.ok) {
      cult::Heraldry h;
      rui::arms(L, h);
      field = h.field;
      trim = h.charge ? h.charge : L.color2;
      m4Count_.warPropsOwned++;
    }
  }
  if (!field && p != art::Prop::Rubble && p != art::Prop::Ash && p != art::Prop::Scaffold) return nullptr;
  const uint64_t key = ew::mix64(((uint64_t)field << 32 | trim) ^ ((uint64_t)p << 20) ^ 0x3A9Full);
  auto it = warPropTex_.find(key);
  if (it != warPropTex_.end()) return &it->second;
  return &(warPropTex_[key] = pix_->bake(art::warPropSprite(p, field, trim)));
}

void View::burnedFx(Game& g, const Map& m, const Bldg& b, int index, float dt) {
  (void)m; (void)index; (void)dt;
  if (b.charred != 1 && b.charred != 2) return;
  m4Count_.burned++;
  if (g.mode == Mode::Title) return;
  const bool out = b.charred == 2;
  const float x0 = b.r.x * 16.0f, w = b.r.w * 16.0f, bottom = (b.r.y + b.r.h) * 16.0f;
  const float roofY = bottom - b.r.h * 16.0f - 6.0f;   // about the ridge
  // two or three smoke columns from the broken roof, by the building's own hash (stable spots)
  const uint32_t hb = hash32((uint32_t)(b.r.x * 73856093) ^ (uint32_t)(b.r.y * 19349663));
  const int cols = out ? 2 + (int)(hb % 2) : 1;
  for (int c = 0; c < cols; c++) {
    const float fx = x0 + w * (0.25f + 0.5f * (float)((hb >> (4 + c * 5)) % 11) / 10.0f);
    Rng r((uint32_t)(t_ * 1000.0f) * 131u + (uint32_t)c * 977u + hb);
    // a smoke column: thick dark puffs (two side by side) that billow, drift east and thin out as they rise
    for (int puff = 0; puff < (out ? 2 : 1); puff++) {
      if (r.f() > (out ? 0.07f : 0.03f)) continue;
      Particle q;
      q.p = Vec2(fx + r.range(-3, 3) + puff * 3.0f, roofY + r.range(0, 8));
      q.v = Vec2(r.range(2, 8), r.range(-15, -8));
      q.life = q.max = r.range(3.0f, 4.6f);
      const float gr = out ? r.range(0.16f, 0.30f) : r.range(0.40f, 0.54f);
      q.c = Color(gr, gr * 0.95f, gr * 0.96f);
      q.size = out ? r.range(3.0f, 5.0f) : r.range(2.0f, 3.0f);
      q.grav = -1.5f;
      q.kind = 2;
      parts_.push_back(q);
    }
    if (out && r.f() < 0.05f) {   // sparks and embers lifting from the ruin
      Particle q;
      q.p = Vec2(fx + r.range(-6, 6), bottom - r.range(6, 20));
      q.v = Vec2(r.range(-6, 6), r.range(-34, -18));
      q.life = q.max = r.range(0.6f, 1.2f);
      q.c = r.f() < 0.5f ? Color(1.0f, 0.62f, 0.18f) : Color(1.0f, 0.86f, 0.40f);
      q.size = 1;
      q.grav = 8.0f;
      parts_.push_back(q);
    }
  }
}

void View::m4Lights(Game& g, const Map& m, Vec2 cam, float dark, std::vector<LightPool>& out) {
  out.clear();
  if (g.inside || g.mode == Mode::Title || m.kind != MapKind::Overworld || dark < 0.12f) return;
  const int tx0 = std::max(0, (int)std::floor(cam.x / 16) - 4), ty0 = std::max(0, (int)std::floor(cam.y / 16) - 4);
  const int tx1 = std::min(m.w, tx0 + Pix::W / 16 + 9), ty1 = std::min(m.h, ty0 + Pix::H / 16 + 9);
  for (int ty = ty0; ty < ty1; ty++)
    for (int tx = tx0; tx < tx1; tx++) {
      const int pr = m.prop[(size_t)ty * m.w + tx];
      if (!pr) continue;
      const art::Prop p = (art::Prop)(pr - 1);
      const float f = 0.88f + 0.12f * std::sin(t_ * 6.5f + tx * 1.9f + ty);
      if (p == art::Prop::CommandTent || p == art::Prop::WarTent || p == art::Prop::RefugeeTent) {
        // lantern light glowing through the cloth and a pool at the door
        const float big = p == art::Prop::CommandTent ? 1.3f : 1.0f;
        out.push_back({Vec2(tx * 16 + 8.0f, ty * 16 - 6.0f), 40 * big, Color(1.0f, 0.72f, 0.40f), 0.55f * dark * f});
        out.push_back({Vec2(tx * 16 + 8.0f, ty * 16 + 12.0f), 30 * big, Color(1.0f, 0.62f, 0.30f), 0.40f * dark * f});
        m4Count_.campFires++;
      }
    }
  // burned-out buildings smoulder red in the dark
  for (const Bldg& b : m.bldgs) {
    if (b.charred != 2) continue;
    // (M4 integration) the glow sits in the gutted shell (mid footprint), not at the door, and is strong enough to
    // read on a phone; a smaller, hotter ember bed flickers off-centre by the building's hash
    const Vec2 c(b.r.x * 16 + b.r.w * 8.0f, (b.r.y + b.r.h * 0.55f) * 16);
    if (c.x < cam.x - 80 || c.x > cam.x + Pix::W + 80 || c.y < cam.y - 80 || c.y > cam.y + Pix::H + 80) continue;
    const float f = 0.75f + 0.25f * std::sin(t_ * 3.1f + b.r.x) * std::sin(t_ * 4.7f + b.r.y);
    out.push_back({c, 30 + b.r.w * 6.0f, Color(1.0f, 0.36f, 0.12f), 0.85f * dark * f});
    const uint32_t hb = hash32((uint32_t)(b.r.x * 73856093) ^ (uint32_t)(b.r.y * 19349663));
    const float ex = b.r.x * 16 + b.r.w * 16.0f * (0.3f + 0.4f * (float)(hb % 9) / 8.0f);
    const float f2 = 0.7f + 0.3f * std::sin(t_ * 7.3f + (float)(hb & 15));
    out.push_back({Vec2(ex, c.y + 6.0f), 18 + b.r.w * 2.0f, Color(1.0f, 0.55f, 0.18f), 0.7f * dark * f2});
  }
}

// (fixer M4 r3, review: "the burned-out shell at night is a dark grey silhouette with a faint brown wash") drawn over
// the night's light map: a burned-out shell's open roof cavity burns from inside (a hot red-orange core and a few
// white-hot ember beds), a soft halo round the walls, and the smoke above lit from below. Additive, so it reads on
// a phone at 1x whatever the darkness.
void View::m4Emissive(Game& g, const Map& m, Vec2 cam, float dark) {
  if (g.inside || g.mode == Mode::Title || m.kind != MapKind::Overworld || dark < 0.15f) return;
  Pix& P = *pix_;
  const float k = std::min(1.0f, (dark - 0.15f) * 2.4f);
  auto blob = [&](float x, float y, float rw, float rh, Color c) {
    P.blitEx(light_, 0, 0, 64, 64, x - rw - cam.x, y - rh - cam.y, rw * 2, rh * 2, false, c, 1);
  };
  for (size_t i = 0; i < m.bldgs.size(); i++) {
    const Bldg& b = m.bldgs[i];
    if (b.charred != 2) continue;
    const float x0 = b.r.x * 16.0f, w = b.r.w * 16.0f, bottom = (b.r.y + b.r.h) * 16.0f;
    if (x0 + w < cam.x - 60 || x0 > cam.x + Pix::W + 60 || bottom < cam.y - 40 || bottom - 140 > cam.y + Pix::H) continue;
    float top = bottom - b.r.h * 16.0f - 24.0f;
    auto t = bldgTex_.find(bldgKey(m, b, (int)i));
    if (t != bldgTex_.end()) top = bottom + art::BLDG_PAD_B - t->second.h;
    const float cx = x0 + w * 0.5f, cy = top + (bottom - top) * 0.46f;   // the gutted roof's cavity
    const uint32_t hb = hash32((uint32_t)(b.r.x * 73856093) ^ (uint32_t)(b.r.y * 19349663));
    const float f = 0.78f + 0.22f * std::sin(t_ * 3.1f + b.r.x) * std::sin(t_ * 4.7f + b.r.y);
    // the halo round the shell, the deep red bed in the cavity, its hot orange heart
    const float hh = bottom - top;
    blob(cx, cy + 8, w * 1.0f + 24, hh * 0.75f + 14, Color(0.85f, 0.22f, 0.06f, 0.40f * k * f));
    blob(cx, cy, w * 0.70f, hh * 0.36f, Color(1.0f, 0.28f, 0.07f, 0.95f * k * f));
    blob(cx, cy, w * 0.70f, hh * 0.36f, Color(0.9f, 0.20f, 0.05f, 0.60f * k * f));
    blob(cx, cy + 2, w * 0.42f, hh * 0.20f, Color(1.0f, 0.60f, 0.20f, 0.95f * k * f));
    // ember beds flickering on their own
    for (int e = 0; e < 3; e++) {
      const float ex = x0 + w * (0.25f + 0.5f * (float)((hb >> (e * 6)) % 13) / 12.0f);
      const float ey = cy + ((float)((hb >> (e * 6 + 3)) % 9) - 4.0f) * 1.5f;
      const float fe = 0.6f + 0.4f * std::sin(t_ * (6.0f + e * 1.7f) + (float)e * 2.1f + (float)(hb & 7));
      blob(ex, ey, 7, 5, Color(1.0f, 0.86f, 0.42f, 0.95f * k * fe));
    }
    // the smoke column over it lit from below: a tall faint glow rising from the cavity
    blob(cx + 4, cy - 30, 16, 40, Color(0.90f, 0.36f, 0.12f, 0.30f * k * f));
  }
}

bool View::nearSiege(Game& g) const {
  if (!g.world.endless || g.inside) return false;
  const int32_t gx = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), gy = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  for (const realm::Siege& s : g.realm.sieges()) {
    if (s.over) continue;
    const realm::SettlementState* st = g.realm.settlement(s.site);
    const int32_t sx = st ? st->gx : s.campX, sy = st ? st->gy : s.campY;
    const int64_t dx = sx - gx, dy = sy - gy;
    if (dx * dx + dy * dy < 70 * 70) return true;
    const int64_t cx = s.campX - gx, cy = s.campY - gy;
    if (cx * cx + cy * cy < 40 * 40) return true;
  }
  return false;
}
