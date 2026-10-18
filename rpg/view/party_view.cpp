// M8 Fellowship, the companions in the world (VIEW lane).
//   - The bonded hound or wolf: art::houndSheet (species, age from MF_PUP and the days since it was taken in, a
//     leather collar), frames by what it does: idle (two, each animal out of step), running, biting, lying hurt while
//     downed, sitting while it waits; rows by the way it moves (flipped for the left); its own small shadow.
//   - A companion downed: their Posture::Downed sheet (the sim leaves the posture to the view while AState::Down), a
//     dark pool spreading under them as they bleed, a pulsing wound mark over them; while the player binds them, a
//     gold ring filling over REVIVE_SECS with a cross in it, and the player kneels beside them (Posture::Kneel).
//   - A mounted companion (MF_MOUNTED while the player rides; their own horse, Member::horse = home::Breed + 1): the
//     horse sheet with the rider's Posture::Ride seated at art::riderSeat, layered as the player's own rider is
//     (home_view.cpp drawMounted), and a shadow as long as the horse.
//   - The graves of the fallen (Party::fallen gx / gy, global tiles, outdoors): art::graveSprite in their people's rite
//     (the culture's burial custom and archetype), their weapon laid on it, fresh earth for the first seven days,
//     flowers once laid (GF_FLOWERS); y-sorted with the scene, with a shadow thrown down-right; the prompt to visit.
//   - The minimap and the world map: companions as small friendly markers (waiting ones where they wait), graves as
//     small stones.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/sim/home.h"
#include "rpg/sim/party.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {
uint64_t vtag(uint64_t tag, uint64_t v) { return (tag << 56) ^ (v & 0x00FFFFFFFFFFFFFFull); }

// where a grave stands in the window (map pixels: the bottom-centre of its tile)
Vec2 graveAt(const Game& g, const party::Fallen& f) {
  return Vec2((float)(f.gx - g.world.ox) * 16.0f + 8.0f, (float)(f.gy - g.world.oy) * 16.0f + 16.0f);
}
// the horse's frame (idle 0, walk 1-4, gallop 5-7) by the ground speed, as the player's (home_view.cpp)
int compHorseFrame(const Actor& a, float t, bool& gallop) {
  const float sp = len(a.vel);
  gallop = false;
  if (a.st != AState::Walk || sp < 6.0f) return 0;
  gallop = sp > home::WALK_SPEED_PX * 1.3f;
  if (gallop) return 5 + (int)(t * sp / 13.0f) % 3;
  return 1 + (int)(t * std::max(40.0f, sp) / 9.0f) % 4;
}
// the member travelling with the player who rides beside them (MF_MOUNTED, their own horse, the player riding)
bool riding(const Game& g, const party::Member& m) {
  return (m.flags & party::MF_MOUNTED) && m.horse > 0 && g.homeRiding() && m.voc != party::Vocation::Hound && m.state == party::State::With;
}
}  // namespace

// what the player may use here for the party (the order Game::partyInteract takes them in): 1 a grave in reach (idx: the
// fallen index), 2 a downed companion in reach (idx: the members index), 0 nothing. The HUD's prompt, the touch
// button's verb and the input's use-instead-of-attack read it (hud.cpp).
int partyUseHere(const Game& g, int& idx) {
  idx = -1;
  if (g.party.members.empty() && g.party.fallen.empty()) return 0;
  const Actor& p = g.pl();
  if (!g.inside)
    for (size_t fi = 0; fi < g.party.fallen.size(); fi++) {
      // (as Game::partyInteract: only a grave stamped into the world, reached from where the player faces)
      const party::Fallen& f = g.party.fallen[fi];
      if (!(f.graveFlags & party::GF_SITED)) continue;
      const Vec2 at((f.gx - g.world.ox) * (float)TILE + 8.0f, (f.gy - g.world.oy) * (float)TILE + 10.0f);
      static const Vec2 fv[4] = {Vec2(0, 1), Vec2(0, -1), Vec2(1, 0), Vec2(-1, 0)};   // (faceVec)
      const Vec2 reach = p.p + fv[p.face & 3] * 6.0f;
      if (len2(at - reach) < (1.4f * TILE) * (1.4f * TILE)) { idx = (int)fi; return 1; }
    }
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    if (!a.comp || a.comp > g.party.members.size() || a.st == AState::Dead) continue;
    if (g.party.members[(size_t)a.comp - 1].state != party::State::Downed) continue;
    if (len2(a.p - p.p) > (1.8f * TILE) * (1.8f * TILE)) continue;
    idx = a.comp - 1;
    return 2;
  }
  return 0;
}

art::Posture View::partyPosture(const Game& g, const Actor& a) const {
  if (a.comp && a.human && a.st == AState::Down) {
    const int mi = companionIndex(g, a);
    if (mi >= 0 && g.party.members[(size_t)mi].state == party::State::Downed) return art::Posture::Downed;
  }
  if (a.player && a.posture == art::Posture::None && a.st == AState::Idle && !g.homeRiding())
    for (size_t i = 1; i < g.actors.size(); i++) {   // binding a friend's wounds: on one knee beside them
      const Actor& o = g.actors[i];
      if (!o.comp || o.comp > g.party.members.size()) continue;
      const party::Member& m = g.party.members[(size_t)o.comp - 1];
      if (m.state == party::State::Downed && m.reviveT > 0 && len2(o.p - a.p) < (1.8f * TILE) * (1.8f * TILE)) return art::Posture::Kneel;
    }
  return a.posture;
}

bool View::drawCompanionBody(Game& g, const Actor& a, Vec2 cam, float alpha, float flash) {
  const int mi = companionIndex(g, a);
  if (mi < 0) return false;
  Pix& P = *pix_;
  const party::Member& m = g.party.members[(size_t)mi];
  if (!a.human) {
    // ---- the hound
    const Tex& t = houndTex(m, g);
    Vec2 dirv = len2(a.vel) > 4.0f ? a.vel : a.aim;
    int row = 0;
    bool flip = false;
    if (std::fabs(dirv.x) >= std::fabs(dirv.y) * 0.9f) { row = 2; flip = dirv.x < 0; }
    else row = dirv.y > 0 ? 0 : 1;
    const float own = (float)(hash32((uint32_t)m.id * 0x9E3779B1u) % 1000u) * 0.001f;
    int fr = art::HF_IDLE + ((int)(t_ * 2.2f + own * 5) & 1);
    switch (a.st) {
      case AState::Walk: fr = art::HF_RUN + (int)(a.animT * 11) % 4; break;
      case AState::Windup: fr = art::HF_BITE; break;
      case AState::Strike: case AState::Recover: fr = art::HF_BITE + 1; break;
      case AState::Down: fr = art::HF_DOWN; break;
      default:
        if (m.state == party::State::Waiting || (len2(a.vel) < 1.0f && len2(a.p - g.pl().p) < 30.0f * 30.0f && std::fmod(t_ * 0.07f + own, 1.0f) < 0.45f)) fr = art::HF_SIT;
        break;
    }
    const float x = std::floor(a.p.x - art::HOUND_W / 2.0f - cam.x + 0.5f), y = std::floor(a.p.y - art::HOUND_H + 2 - cam.y + 0.5f);
    P.blitEx(t, fr * art::HOUND_W, row * art::HOUND_H, art::HOUND_W, art::HOUND_H, x, y, (float)art::HOUND_W, (float)art::HOUND_H, flip, Color(1, 1, 1, alpha));
    if (flash > 0) P.blitEx(t, fr * art::HOUND_W, row * art::HOUND_H, art::HOUND_W, art::HOUND_H, x, y, (float)art::HOUND_W, (float)art::HOUND_H, flip, Color(1, 1, 1, flash), 1);
    drawnHead_[a.id] = DrawnHead{a.p.x, y + cam.y + 4, t_};
    return true;
  }
  if (!riding(g, m)) return false;
  // ---- a companion on their own horse (the player rides)
  const home::BreedInfo& bi = home::breedInfo((home::Breed)std::min<int>(m.horse - 1, (int)home::Breed::COUNT - 1));
  const uint32_t seedv = m.id * 2654435761u ^ m.lookSeed;
  // (P2a LENS fix r1, H3) the rig's horse (as the hero's, home_view.cpp drawMounted): HOLDERS' k horse at rig 2
  const bool rig2 = art::rigVersion() >= 2;
  const int HW = art::horseCellW(), HH = art::horseCellH();
  const uint64_t key = vtag(rig2 ? 0x52 : 0x51, (uint64_t)(bi.coat & 0xFFFFFFu) << 16 ^ (seedv & 0xFFFFu));
  auto it = partyTex_.find(key);
  if (it == partyTex_.end()) it = partyTex_.emplace(key, P.bake(art::horseSheetRig(bi.coat, seedv, true))).first;
  const Tex& ht = it->second;
  const int row = a.face == 0 ? 0 : a.face == 1 ? 1 : 2;
  const bool flip = a.face == 3;
  bool gallop = false;
  const int fr = compHorseFrame(a, t_, gallop);
  const float fx = std::floor(a.p.x - cam.x + 0.5f), fy = std::floor(a.p.y + 2 - cam.y + 0.5f);
  const float hx = fx - HW / 2.0f, hy = fy - HH + 2;
  const int cx = fr * HW, cy = row * HH;
  auto horsePart = [&](int x0, int y0, int w, int h, Color c, int blend) {
    if (w <= 0 || h <= 0) return;
    const int dx0 = flip ? HW - x0 - w : x0;
    P.blitEx(ht, cx + x0, cy + y0, w, h, hx + dx0, hy + y0, (float)w, (float)h, flip, c, blend);
  };
  int dx = 0, dy = 0;
  art::riderSeatRig(row, fr, dx, dy);
  int sdx = 0, sdy = 0;   // the saddle's seat point (from the hooves' row at the cell's centre column)
  art::saddleSeat(row, fr, sdx, sdy);
  if (flip) { dx = -dx; sdx = -sdx; }
  const Tex* rt = poseTex(a.look, art::Posture::Ride);
  const Tex& rider = rt ? *rt : humanTex(a.look);
  const float rx = std::floor(fx + dx - art::HUMAN_W / 2.0f), ry = std::floor(fy + dy - art::HUMAN_H + 2 + art::postureInfo(art::Posture::Ride).dy);
  const int seatRow = std::clamp(HH - 2 + dy, 0, HH);
  const int stripW = rig2 ? 18 : 12;
  float headY = ry;
  auto layer = [&](Color c, int blend) {
    horsePart(0, 0, HW, HH, c, blend);
    // (P2a LENS) the rider on v2 where the painters cover the look (as the hero's, home_view.cpp): the hip anchor on
    // the saddle's seat point; recorded for the night lift (the horse is not)
    RideGhost g2;
    if (rig2 && drawRiderV2(a.look, a.face, fx + sdx, fy + sdy, fx + sdx, fy + sdy + 3.0f, c, blend, &g2)) {
      headY = g2.y;
    } else {
      if (blend == 0) P.recordBlits(&liftRecs_);
      P.blitEx(rider, 0, row * art::HUMAN_H, art::HUMAN_W, art::HUMAN_H, rx, ry, (float)art::HUMAN_W, (float)art::HUMAN_H, flip, c, blend);
      P.recordBlits(nullptr);
    }
    if (row == 1) horsePart(0, seatRow + 1, HW, HH - seatRow - 1, c, blend);
    else if (row == 0) {
      horsePart(HW / 2 - stripW / 2, std::max(0, seatRow - 5), stripW, HH - std::max(0, seatRow - 5), c, blend);
      P.blitEx(ht, cx, 3 * HH, HW, HH, hx, hy, (float)HW, (float)HH, flip, c, blend);
    }
  };
  layer(Color(1, 1, 1, alpha), 0);
  if (flash > 0) layer(Color(1, 1, 1, flash), 1);
  drawnHead_[a.id] = DrawnHead{fx + cam.x, headY + cam.y, t_};
  return true;
}

void View::drawCompanionShadow(Game& g, const Actor& a, Vec2 cam, bool& done) {
  done = false;
  const int mi = companionIndex(g, a);
  if (mi < 0) return;
  Pix& P = *pix_;
  const party::Member& m = g.party.members[(size_t)mi];
  if (m.state == party::State::With && g.mode != Mode::Title) {
    // (fixer M8 r2, review: "companions cannot be told apart from the market crowd") a thin party-gold ring on the
    // ground round their feet, under the shadow (whole pixels, the top-left arc lit, the bottom-right in shade); the one
    // whose card is selected or whose orders are open wears it brighter
    const bool sel = (mi == partySel_ && partyTagT_ > 0) || (radial_.open && radial_.member == mi);
    const bool ride = riding(g, m);
    const float rx = ride ? 15.0f : a.human ? 8.5f : 11.0f, ry = ride ? 5.0f : a.human ? 3.5f : 4.0f;
    const float ox = std::floor(a.p.x - cam.x + 1), oy = std::floor(a.p.y - cam.y + (ride ? 3.0f : 1.0f));
    const float al = sel ? 1.0f : 0.78f;
    for (int j = -(int)ry - 1; j <= (int)ry + 1; j++)
      for (int q = -(int)rx - 1; q <= (int)rx + 1; q++) {
        const float e = ((float)q * q) / (rx * rx) + ((float)j * j) / (ry * ry);
        if (e < 0.56f || e > 1.14f) continue;
        const bool lit = q + j * 2 < 0;
        const Color c = lit ? Color(1.0f, 0.86f, 0.46f, al) : Color(0.78f, 0.58f, 0.24f, al * 0.85f);
        P.rect(ox + q, oy + j, 1, 1, c);
      }
  }
  if (m.state == party::State::Downed) {
    // the blood under them, spreading as the clock runs down (a dark pool lit faintly at its top-left lip)
    const float k = 1.0f - clampf(m.downT / party::DOWNED_SECS, 0, 1);
    // (fixer M8 r1, review: "no visible blood") wider than the lying body from the first second, so it shows round it
    const float w = 20.0f + 10.0f * k, h = std::max(6.0f, w * 0.38f);
    const float x = std::floor(a.p.x - w / 2 + 2 - cam.x), y = std::floor(a.p.y - h / 2 - 1 - cam.y);
    P.blitEx(shadowBig_, 0, 0, 40, 12, x - 2, y - 1, w + 4, h + 2, false, Color(1, 1, 1, 0.6f));
    for (int j = 0; j < (int)h; j++) {
      const float t = (j + 0.5f) / h * 2 - 1;
      const float hw = std::floor(w / 2 * std::sqrt(std::max(0.0f, 1 - t * t)));
      P.rect(x + w / 2 - hw, y + j, hw * 2, 1, Color(0.46f, 0.03f, 0.05f, 0.86f));
      if (hw > 3) P.rect(x + w / 2 - hw + 2, y + j, std::max(0.0f, hw - 3), 1, Color(0.58f, 0.06f, 0.07f, 0.55f));   // the wet core
    }
    P.rect(x + w / 2 - 5, y, 6, 1, Color(0.80f, 0.24f, 0.20f, 0.7f));   // the lit lip (top-left light)
    P.rect(x + w / 2 - 7, y + 1, 3, 1, Color(0.80f, 0.24f, 0.20f, 0.45f));
    if (a.human) return;   // the body throws its usual shadow too
  }
  if (!a.human) {   // the hound: a shadow as long as its body (longer side-on)
    const bool side = std::fabs((len2(a.vel) > 4.0f ? a.vel : a.aim).x) >= 0.6f;
    const float w = side ? 20.0f : 13.0f, h = side ? 6.0f : 7.0f;
    P.blitEx(shadow_, 0, 0, shadow_.w, shadow_.h, std::floor(a.p.x - w / 2 + 1 - cam.x), std::floor(a.p.y - h / 2 - cam.y), w, h, false, Color(1, 1, 1, 0.85f));
    done = true;
    return;
  }
  if (riding(g, m)) {
    const bool side = a.face >= 2;
    const float w = side ? 32.0f : 18.0f, h = side ? 8.0f : 10.0f;
    P.blitEx(shadowBig_, 0, 0, 40, 12, std::floor(a.p.x - w / 2 + 2 - cam.x + 0.5f), std::floor(a.p.y - h / 2 + 1 - cam.y + 0.5f), w, h, false, Color(1, 1, 1, 0.85f));
    done = true;
  }
}

void View::companionOverlays(Game& g, Vec2 cam) {
  if (g.party.members.empty() || g.mode == Mode::Title) return;
  Pix& P = *pix_;
  for (size_t i = 1; i < g.actors.size(); i++) {
    const Actor& a = g.actors[i];
    const int mi = companionIndex(g, a);
    if (mi < 0 || a.st == AState::Dead) continue;
    const party::Member& m = g.party.members[(size_t)mi];
    // (fixer M8 r2) the card just tapped: their name over them (P2a: the UI layer, partyWorldText)
    if (m.state == party::State::With && g.mode == Mode::Play && mi == partySel_ && partyTagT_ > 0 && !radial_.open) continue;
    if (m.state != party::State::Downed) continue;
    const Vec2 s = a.p - cam;
    if (s.x < -20 || s.y < -10 || s.x > Pix::W + 20 || s.y > Pix::H + 40) continue;
    const float cx = std::floor(s.x), cy = std::floor(s.y - (a.human ? 28.0f : 22.0f));
    if (m.reviveT > 0) {
      // the binding: a ring filling clockwise over REVIVE_SECS with a white cross in it
      const float k = clampf(m.reviveT / party::REVIVE_SECS, 0, 1);
      const int R = 8;   // (fixer M8 r2) a size larger: read at a glance on a phone
      for (int j = -R - 1; j <= R + 1; j++)
        for (int q = -R - 1; q <= R + 1; q++) {
          const float r = std::sqrt((float)(q * q + j * j));
          if (r > R + 1.0f) continue;
          if (r > R - 2.0f) {
            float an = std::atan2((float)q + 0.5f, -(float)j - 0.5f);
            if (an < 0) an += 6.2831853f;
            const bool on = an / 6.2831853f <= k;
            const Color c = r > R ? Color(0.06f, 0.04f, 0.03f, 0.9f) : on ? Color(1.0f, 0.84f, 0.38f) : Color(0.22f, 0.16f, 0.10f, 0.9f);
            P.rect(cx + q, cy + j, 1, 1, c);
          } else P.rect(cx + q, cy + j, 1, 1, Color(0.05f, 0.04f, 0.05f, 0.72f));
        }
      P.rect(cx - 1, cy - 3, 3, 7, Color(0.95f, 0.95f, 0.9f));
      P.rect(cx - 3, cy - 1, 7, 3, Color(0.95f, 0.95f, 0.9f));
      P.rect(cx, cy - 3, 1, 7, Color(0.85f, 0.25f, 0.2f));
      P.rect(cx - 3, cy, 7, 1, Color(0.85f, 0.25f, 0.2f));
    } else {
      // the wound: a pulsing blood drop over them, quicker as the clock runs out
      const float left = clampf(m.downT / party::DOWNED_SECS, 0, 1);
      const float pu = 0.5f + 0.5f * std::sin(t_ * (left < 0.33f ? 12.0f : 6.0f));
      static const char* kDrop[9] = {"...o...", "..oHo..", "..oHo..", ".oHMMo.", "oHWMMSo", "oHMMMSo", "oMMMSSo", ".oSSSo.", "..ooo.."};
      const float bob = std::floor(std::sin(t_ * 3) * 1.0f);
      for (int j = 0; j < 9; j++)
        for (int q = 0; q < 7; q++) {
          const char ch = kDrop[j][q];
          if (ch == '.') continue;
          const Color c = ch == 'o' ? Color(0.10f, 0.02f, 0.03f) : ch == 'H' ? Color(1.0f, 0.45f + 0.2f * pu, 0.4f) : ch == 'W' ? Color(1, 0.9f, 0.86f) :
                          ch == 'M' ? Color(0.86f, 0.12f + 0.1f * pu, 0.12f) : Color(0.52f, 0.04f, 0.06f);
          P.rect(cx - 3 + q, cy - 6 + j + bob, 1, 1, c);
        }
    }
  }
}

// ---------------------------------------------------------------- graves
void View::graveDraws(Game& g, const Map& m, Vec2 cam, std::vector<int>& out) {
  out.clear();
  if (g.inside || m.kind != MapKind::Overworld || g.party.fallen.empty()) return;
  for (int fi = 0; fi < (int)g.party.fallen.size(); fi++) {
    if (!(g.party.fallen[(size_t)fi].graveFlags & party::GF_SITED)) continue;   // (not yet laid in the world)
    const Vec2 at = graveAt(g, g.party.fallen[(size_t)fi]);
    if (at.x < cam.x - 30 || at.x > cam.x + Pix::W + 30 || at.y < cam.y - 10 || at.y > cam.y + Pix::H + 40) continue;
    out.push_back(fi);
  }
}

void View::drawGrave(Game& g, int fi, Vec2 cam, bool shadow) {
  if (fi < 0 || fi >= (int)g.party.fallen.size()) return;
  Pix& P = *pix_;
  const party::Fallen& f = g.party.fallen[(size_t)fi];
  const Vec2 at = graveAt(g, f);
  if (shadow) {
    // (fixer M8 r3, review: "the round mound sits below the plot, not joined to it") the grave's earth, its kerb of
    // field stones and the marker are all the sprite (art::graveSprite); here only the soft shadow it throws on the
    // ground, down-right of the mound (the light is top-left)
    const float ox = std::floor(at.x - cam.x), oy = std::floor(at.y - cam.y - 9);
    P.blitEx(shadowBig_, 0, 0, 40, 12, ox - 8 + 2, oy - 6 + 3, 19, 14, false, Color(1, 1, 1, 0.5f));
    return;
  }
  const cult::Culture* C = f.culture && g.world.src ? &g.world.src->culture(f.culture) : nullptr;
  // the sim's record (weapon laid, fresh earth, flowers, seed); a person's rite and stone from their people
  // (art::graveLookFor), the bonded animal's a cairn
  const art::GraveLook S = g.partyGraveLook(fi);
  art::GraveLook L = f.species == 0xFF ? art::graveLookFor(C, S.seed) : S;
  L.weapon = S.weapon;
  L.fresh = S.fresh;
  L.flowers = false;   // (the bundle below, drawn bigger than the sprite's own sprig)
  L.seed = S.seed;
  const uint64_t key = vtag(0x61, L.key());
  auto it = partyTex_.find(key);
  if (it == partyTex_.end()) it = partyTex_.emplace(key, P.bake(art::graveSprite(L))).first;
  const Tex& t = it->second;
  P.blit(t, std::floor(at.x - t.w / 2.0f - cam.x), std::floor(at.y - t.h - cam.y + 1));
  if (f.graveFlags & party::GF_FLOWERS) {
    // (fixer M8 r2, review: "the flowers are a 2-3 pixel cluster you cannot see at 1x") the bundle laid on the mound
    // in front of the stone: green stems tied, bright heads (the culture's grave flowers' colours are the sprite's own)
    const float bx = std::floor(at.x - cam.x - 4), by = std::floor(at.y - cam.y - 10);   // (r3: on the mound's crown)
    static const char* kB[6] = {".r.y.w.", "rRyYwW.", ".r.y.wW", "..ggg..", ".g.t.g.", "..g.g.."};
    for (int j = 0; j < 6; j++)
      for (int q = 0; q < 7; q++) {
        const char ch = kB[j][q];
        if (ch == '.') continue;
        const Color c = ch == 'r' ? Color(0.86f, 0.22f, 0.24f) : ch == 'R' ? Color(1.0f, 0.46f, 0.42f) : ch == 'y' ? Color(0.95f, 0.78f, 0.22f) :
                        ch == 'Y' ? Color(1.0f, 0.94f, 0.55f) : ch == 'w' ? Color(0.88f, 0.86f, 0.92f) : ch == 'W' ? Color(1, 1, 1) :
                        ch == 't' ? Color(0.80f, 0.70f, 0.48f) : Color(0.24f, 0.48f, 0.20f);
        P.rect(bx + q, by + j, 1, 1, c);
      }
    P.rect(bx + 2, by + 6, 6, 1, Color(0, 0, 0, 0.3f));   // its shadow on the soil
  }
  // the name, quietly, when the player stands near (P2a: the UI layer, partyWorldText; the stone's top in map px)
  graveTops_.push_back({fi, at.x, at.y, at.y - (float)t.h});
}

// (P2a LENS) the party's words over the world, in the UI layer through toUi (STYLE_P2 18): the tapped card's name over
// its companion, a fallen companion's name over their grave when the player stands near
void View::partyWorldText(Game& g) {
  Pix& P = *pix_;
  const float k = uiPerWorld();
  if (g.mode == Mode::Play && !g.party.members.empty() && partyTagT_ > 0 && !radial_.open)
    for (size_t i = 1; i < g.actors.size(); i++) {
      const Actor& a = g.actors[i];
      const int mi = companionIndex(g, a);
      if (mi < 0 || mi != partySel_ || a.st == AState::Dead) continue;
      const party::Member& m = g.party.members[(size_t)mi];
      if (m.state != party::State::With) continue;
      const Vec2 s = toUi(a.p);
      const float lift = riding(g, m) ? (float)rigFor(a.look).markerY + 14.0f : a.human ? (float)rigFor(a.look).markerY + 6.0f : 24.0f;   // (v1: 44 / 36 / 24)
      float ty = std::floor(s.y - lift * k);
      const float half = (float)P.textW(m.name, 1) / 2.0f;
      if (tagOn_ && s.x + half + 3 > tagX0_ && s.x - half - 3 < tagX1_ && ty + 9 > tagY0_ && ty - 1 < tagY1_) ty = std::floor(tagY0_ - 12);   // over the faced one's tag
      P.rect(std::floor(s.x - half - 3), ty - 1, std::floor(half * 2 + 6), 10, Color(0.04f, 0.03f, 0.05f, 0.7f));
      P.textS(std::floor(s.x), ty, m.name, 1, Color(1.0f, 0.86f, 0.46f), 1);
    }
  if (g.mode == Mode::Play)
    for (const GraveTop& gt : graveTops_) {
      if (gt.fi < 0 || gt.fi >= (int)g.party.fallen.size()) continue;
      const party::Fallen& f = g.party.fallen[(size_t)gt.fi];
      const float d2 = len2(Vec2(gt.x, gt.feet) - Vec2(0, 6) - g.pl().p);
      int useIdx = -1;
      const bool prompt = partyUseHere(g, useIdx) == 1 && useIdx == gt.fi;   // (the VISIT prompt names them instead)
      if (d2 >= 56.0f * 56.0f || prompt) continue;
      const float a = clampf((56.0f - std::sqrt(d2)) / 20.0f, 0, 1);
      const Vec2 s = toUi(Vec2(gt.x, gt.top));
      P.textS(std::floor(s.x), std::floor(s.y - 9), f.name, 1, Color(0.92f, 0.88f, 0.78f, 0.9f * a), 1);
    }
  graveTops_.clear();
}

// ---------------------------------------------------------------- the maps
void View::drawPartyMinimap(Game& g, float x, float y, int size, int ox, int oy) {
  Pix& P = *pix_;
  const float k = size / 64.0f;
  if (!g.inside)
    for (const party::Fallen& f : g.party.fallen) {   // a small grey stone
      const int ax = f.gx - g.world.ox - ox, ay = f.gy - g.world.oy - oy;
      if (ax < 1 || ay < 1 || ax >= 63 || ay >= 63) continue;
      P.rect(x + ax * k - 1, y + ay * k - 2, 3, 4, Color(0.05f, 0.04f, 0.05f, 0.9f));
      P.rect(x + ax * k, y + ay * k - 1, 1, 2, Color(0.82f, 0.82f, 0.86f));
    }
  for (int mi = 0; mi < (int)g.party.members.size(); mi++) {
    const party::Member& m = g.party.members[(size_t)mi];
    int ax = 0, ay = 0;
    const int ai = mi < (int)partyActor_.size() ? partyActor_[(size_t)mi] : -1;
    if (ai > 0) { ax = (int)(g.actors[(size_t)ai].p.x / 16) - ox; ay = (int)(g.actors[(size_t)ai].p.y / 16) - oy; }
    else if (m.state == party::State::Waiting && m.mapKind == 0 && !g.inside) { ax = m.gx - g.world.ox - ox; ay = m.gy - g.world.oy - oy; }
    else continue;
    if (ax < 1 || ay < 1 || ax >= 63 || ay >= 63) continue;
    // a friendly green diamond (a waiting one pale), outlined so it reads on grass
    const Color c = m.state == party::State::Downed ? (((int)(t_ * 4)) & 1 ? Color(1, 0.3f, 0.25f) : Color(0.5f, 0.1f, 0.1f))
                  : m.state == party::State::Waiting ? Color(0.62f, 0.86f, 0.62f) : Color(0.36f, 0.95f, 0.45f);
    const float px = std::floor(x + ax * k), py = std::floor(y + ay * k);
    P.rect(px - 1, py - 2, 3, 5, Color(0.04f, 0.06f, 0.04f, 0.9f));
    P.rect(px - 2, py - 1, 5, 3, Color(0.04f, 0.06f, 0.04f, 0.9f));
    P.rect(px, py - 1, 1, 3, c);
    P.rect(px - 1, py, 3, 1, c);
  }
}

void View::drawPartyWorldMap(Game& g, Vec2 (*toScr)(void*, double, double), void* ctx, float w, float h) {
  Pix& P = *pix_;
  for (const party::Fallen& f : g.party.fallen) {
    const Vec2 s = toScr(ctx, f.gx + 0.5, f.gy + 0.5);
    if (s.x < -6 || s.y < -6 || s.x > w + 6 || s.y > h + 6) continue;
    const float cx = std::floor(s.x), cy = std::floor(s.y);
    // a small headstone: dark outline, a pale stone lit on its left, a shaded right side
    P.rect(cx - 3, cy - 5, 7, 8, Color(0.08f, 0.06f, 0.06f, 0.95f));
    P.rect(cx - 2, cy - 4, 5, 6, Color(0.80f, 0.80f, 0.84f));
    P.rect(cx + 1, cy - 4, 1, 6, Color(0.56f, 0.56f, 0.62f));
    P.rect(cx - 1, cy - 5, 3, 1, Color(0.08f, 0.06f, 0.06f, 0.95f));
    P.rect(cx - 1, cy - 3, 1, 1, Color(0.96f, 0.96f, 1.0f));
  }
  for (const party::Member& m : g.party.members) {
    if (m.state != party::State::Waiting || m.mapKind != 0) continue;
    const Vec2 s = toScr(ctx, m.gx + 0.5, m.gy + 0.5);
    if (s.x < -6 || s.y < -6 || s.x > w + 6 || s.y > h + 6) continue;
    const float cx = std::floor(s.x), cy = std::floor(s.y);
    P.rect(cx - 2, cy - 4, 5, 9, Color(0.04f, 0.06f, 0.04f, 0.95f));
    P.rect(cx - 4, cy - 2, 9, 5, Color(0.04f, 0.06f, 0.04f, 0.95f));
    P.rect(cx - 1, cy - 3, 3, 7, Color(0.40f, 0.92f, 0.48f));
    P.rect(cx - 3, cy - 1, 7, 3, Color(0.40f, 0.92f, 0.48f));
    P.rect(cx - 1, cy - 1, 1, 1, Color(0.85f, 1, 0.85f));
  }
}
