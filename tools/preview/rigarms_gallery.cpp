// rigarms_gallery (P2a RIG-ARMS; skeleton by the phase A lead, handed to RIG-ARMS): the swings, their weapons and
// their effects (STYLE_P2 10-12). Every board at 1x and 4x (the slice's hero holding each family's weapon).
//   rigarms_gallery <outDir> [woman]
//     rigarms_<family>.png      every frame: rows hit 1 / 2 / 3 x facing down, up, right, left; columns the LOCO idle
//                               frame, then Windup 2, Strike 2, Follow 2 (the bow: Draw 3, Release 1); Cast 2 and Roll 4
//                               below
//     rigarms_motion_<family>.png  each blow as it plays at 60 Hz (every second sim frame shown) at the hero's timing
//                               (windup 0.07, strike 0.11, recover 0.13 s x the family's pace), the family's effect laid
//                               where the renderer lays it (render_anim.cpp drawSwingFx), four facings
//     rigarms_strikes.png       side by side per facing: every family's strike frames (hit 1), then each family's three
//                               hits' first strike frame (R7 / R9 by eye)
//     rigarms_weapons.png       the base weapons at their 16 angles (the grip at each cell's centre)
//     rigarms_fx.png            the 9 family effects, every frame
//   rigarms_gallery <outDir> tips   R8's tip paths and the R7 / R9 / R10 numbers per family (stdout)
#include <map>
#include "tools/preview/rig_preview.h"
#include "rpg/art/rig/rig_internal.h"

namespace {

const uint8_t kWeaponOf[art::FAMILY_COUNT] = {0, 5, 1, art::WEAPON_GREATSWORD, 2, 6, 8, 4, 3};
const float kPace[art::FAMILY_COUNT] = {0.9f, 0.8f, 1.0f, 1.18f, 1.08f, 1.25f, 1.1f, 1.0f, 1.0f};

art::HumanLook lookOf(art::Family f, bool woman) {
  art::HumanLook L = rigpv::sliceHero(woman);
  L.weapon = kWeaponOf[(int)f];
  return L;
}

// a frame stood on a ground line by its pivot
void stand(Board& b, const art::RigSheet& s, int facing, art::Clip c, int v, int i, int x, int groundY) {
  int px, py;
  const Canvas f = rigpv::frameCanvas(s, facing, c, v, i, px, py);
  if (f.w <= 0) { b.text(x - 8, groundY - 12, "--", rgba(232, 96, 60)); return; }
  b.put(f, x - px, groundY - py);
}

// ---- the renderer's effect placement (render_anim.cpp drawSwingFx's generic path; the sword's Steel Arc its own)
Canvas turned(const Canvas& s, int fw, int fh, int n, int rot) {   // rot 1 clockwise (facing down), 3 counter-clockwise (up)
  Canvas o(n * fh, fw);
  for (int k = 0; k < n; k++)
    for (int y = 0; y < fh; y++)
      for (int x = 0; x < fw; x++) {
        const uint32_t c = s.get(k * fw + x, y);
        if (!c) continue;
        const int nx = rot == 1 ? fh - 1 - y : y, ny = rot == 1 ? x : fw - 1 - x;
        o.set(k * fh + nx, ny, c);
      }
  return o;
}
void putFx(Board& b, art::Family fam, int facing, int variant, bool fin, float prog, int px, int py) {
  const art::Fx f = fam == art::Family::Unarmed ? art::Fx::PunchStar : fam == art::Family::Dagger ? art::Fx::StabGlint
                  : fam == art::Family::Sword ? art::Fx::SwingArc : fam == art::Family::Greatsword ? art::Fx::HeavyArc
                  : fam == art::Family::Axe ? art::Fx::ChopArc : fam == art::Family::Mace ? art::Fx::SmashRing
                  : fam == art::Family::Spear ? art::Fx::ThrustStreak : fam == art::Family::Staff ? art::Fx::BluntSwish : art::Fx::Twang;
  const art::RigMetrics& M = art::rig(2);
  static const int ax4[4] = {0, 0, 1, -1}, ay4[4] = {1, -1, 0, 0};
  const float aimX = (float)ax4[facing], aimY = (float)ay4[facing];
  if (f == art::Fx::SwingArc) {   // (render_anim.cpp's Steel Arc on a v2 body)
    const int base = art::fxH(f), sz = fin ? (int)std::lround(base * 1.2f) : base, n = art::fxFrames(f);
    const int fr = std::clamp((int)(prog * n), 0, n - 1);
    const bool upright = variant == 2 && facing <= 1;
    const bool back = upright ? facing == 1 : variant == (facing == 3 ? 1 : 0);
    const bool flip = upright ? facing == 0 : facing == 3;
    const int rot = upright ? 0 : facing == 0 ? 1 : facing == 1 ? 3 : 0;
    const Canvas sheet = art::fxSpriteSized(f, sz, sz);
    Canvas cell(sz, sz);
    for (int y = 0; y < sz; y++)
      for (int x = 0; x < sz; x++) {
        const uint32_t c = sheet.get(fr * sz + x, back ? sz - 1 - y : y);
        if (!c) continue;
        const int nx = rot == 1 ? sz - 1 - y : rot == 3 ? y : x, ny = rot == 1 ? x : rot == 3 ? sz - 1 - x : y;
        cell.set(flip ? sz - 1 - nx : nx, ny, fin ? art::mix(c, rgba(255, 230, 160), 0.0f) : c);
      }
    static const int kArcX[4] = {0, 0, -1, 1}, kArcY[4] = {-32, -16, -17, -17};
    static const int kDirX[4] = {0, 0, 1, -1}, kDirY[4] = {1, -1, 0, 0};
    const int axx = upright ? (facing == 0 ? 6 : -11) : kArcX[facing], ayy = upright ? (facing == 0 ? -18 : -26) : kArcY[facing];
    const int dx = upright ? (facing == 0 ? -1 : 1) : kDirX[facing], dy = upright ? 0 : kDirY[facing];
    const float off = sz * 0.5f + sz / 32.0f;
    const float cx = px + axx + dx * off, cy = py + ayy + dy * off;
    b.put(cell, (int)std::floor(cx - sz * 0.5f + 0.5f), (int)std::floor(cy - sz * 0.5f + 0.5f));
    return;
  }
  const int fw = art::fxW(f), fh = art::fxH(f), n = std::max(1, art::fxFrames(f));
  const int fr = std::clamp((int)(prog * n), 0, n - 1);
  const bool turns = (f == art::Fx::HeavyArc || f == art::Fx::ChopArc || f == art::Fx::StabGlint || f == art::Fx::ThrustStreak ||
                      f == art::Fx::BluntSwish) && facing <= 1;
  const Canvas sheet = turns ? turned(art::fxSprite(f), fw, fh, n, facing == 0 ? 1 : 3) : art::fxSprite(f);
  const int sw = turns ? fh : fw, sh = turns ? fw : fh;
  const float reach = (float)M.reach[(int)fam];
  float cx, cy;
  if (f == art::Fx::SmashRing) { cx = px + aimX * reach; cy = py + aimY * reach - 1; }
  else if (f == art::Fx::Twang) { cx = px + aimX * 6; cy = py - M.fxLift + aimY * 6; }
  else { cx = px + aimX * reach; cy = py - M.fxLift + aimY * reach; }
  const float sc = fin ? 1.2f : 1.0f;
  const int dw = (int)std::round(sw * sc), dh = (int)std::round(sh * sc);
  Canvas cell(dw, dh);
  const bool flip = facing == 3 && !turns;
  for (int y = 0; y < dh; y++)
    for (int x = 0; x < dw; x++) {
      const int sx = std::min(sw - 1, (int)(x / sc)), sy = std::min(sh - 1, (int)(y / sc));
      uint32_t c = sheet.get(fr * sw + (flip ? sw - 1 - sx : sx), sy);
      if (!c) continue;
      if (fin) c = art::withA(art::mix(c, rgba(255, 229, 153), 0.35f), (int)(c >> 24));
      else c = art::withA(c, (int)((c >> 24) * 0.9f));
      cell.set(x, y, c);
    }
  b.put(cell, (int)std::floor(cx - dw / 2.0f + 0.5f), (int)std::floor(cy - dh / 2.0f + 0.5f));
}

const char* kFacingName[4] = {"DOWN", "UP", "RIGHT", "LEFT"};
const int kFacingOrder[4] = {0, 1, 2, 3};

void familyBoard(const std::string& dir, art::Family fam, bool woman) {
  const art::HumanLook L = lookOf(fam, woman);
  const art::RigSheet s = art::humanRig(L, art::RigPart::Action);
  const art::RigSheet lo = art::humanRig(L, art::RigPart::Loco);
  const bool bow = fam == art::Family::Bow;
  const int CW = 72, RH = 74, top = 24, left = 54;
  const int rows = bow ? 4 : 12;
  Board b(left + CW * 16 + 10, top + RH * (rows + 3) + 10);
  b.text(4, 4, std::string("FAMILY ") + art::familyName(fam) + (woman ? "  (WOMAN)" : "") + "   IDLE | WINDUP 2 | STRIKE 2 | FOLLOW 2");
  int row = 0;
  for (int v = 0; v < (bow ? 1 : 3); v++)
    for (int fi = 0; fi < 4; fi++, row++) {
      const int fc = kFacingOrder[fi];
      const int gy = top + row * RH + 60;
      b.text(4, gy - 40, (bow ? std::string("BOW") : std::string("HIT ") + std::to_string(v + 1)));
      b.text(4, gy - 30, kFacingName[fc]);
      stand(b, lo, fc, art::Clip::Idle, 0, 0, left + CW / 2, gy);
      if (bow) {
        for (int i = 0; i < 3; i++) stand(b, s, fc, art::Clip::Draw, 0, i, left + CW * (i + 1) + CW / 2, gy);
        stand(b, s, fc, art::Clip::Release, 0, 0, left + CW * 4 + CW / 2, gy);
      } else {
        static const art::Clip clips[3] = {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow};
        for (int i = 0; i < 6; i++) stand(b, s, fc, clips[i / 2], v, i % 2, left + CW * (i + 1) + CW / 2, gy);
      }
      for (int x = left; x < left + CW * 7; x += 2) b.c.set(x, gy + 1, rgba(70, 116, 52));
    }
  // Cast 2 in each facing
  const int gy = top + row * RH + 60;
  b.text(4, gy - 40, "CAST");
  for (int fi = 0; fi < 4; fi++)
    for (int i = 0; i < 2; i++) stand(b, s, kFacingOrder[fi], art::Clip::Cast, 0, i, left + CW * (fi * 2 + i) + CW / 2, gy);
  // Roll 4 in each facing (crouch, tuck, over, rise)
  const int gy2 = gy + RH;
  b.text(4, gy2 - 40, "ROLL");
  for (int fi = 0; fi < 4; fi++)
    for (int i = 0; i < 4; i++) stand(b, s, kFacingOrder[fi], art::Clip::Roll, 0, i, left + CW * (fi * 4 + i) + CW / 2, gy2);
  rigpv::saveBoth(b, dir, std::string("rigarms_") + art::familyName(fam) + (woman ? "_woman" : ""));
}

// each blow as it plays, every second sim frame, with the effect where the renderer lays it
void motionBoard(const std::string& dir, art::Family fam) {
  if (fam == art::Family::Bow) return;
  const art::HumanLook L = lookOf(fam, false);
  const art::RigSheet s = art::humanRig(L, art::RigPart::Action);
  const float pace = kPace[(int)fam];
  const int CW = 60, RH = 80, top = 24, left = 54;
  const int maxCols = 16;
  Board b(left + CW * maxCols + 10, top + RH * 12 + 10);
  b.text(4, 4, std::string("MOTION ") + art::familyName(fam) + "  every 2nd frame at 60 HZ, the hero's timing x pace, the effect as drawn");
  int row = 0;
  for (int v = 0; v < 3; v++)
    for (int fi = 0; fi < 4; fi++, row++) {
      const int fc = kFacingOrder[fi];
      const int gy = top + row * RH + 62;
      b.text(4, gy - 44, std::string("HIT ") + std::to_string(v + 1));
      b.text(4, gy - 34, kFacingName[fc]);
      const float wind = (fam == art::Family::Unarmed ? 0.05f : 0.07f) * pace, strike = 0.11f * pace;
      const float rec = (v == 2 ? 0.28f : fam == art::Family::Unarmed ? 0.08f : 0.13f) * pace;
      int col = 0;
      for (int k = 0; col < maxCols; k += 2, col++) {
        const float t = k / 60.0f;
        art::Clip c;
        int i;
        float prog = -1;
        if (t < wind) { c = art::Clip::Windup; i = t < 0.45f * wind ? 0 : 1; }
        else if (t < wind + strike) { c = art::Clip::Strike; i = (t - wind) < 0.40f * strike ? 0 : 1; prog = (t - wind) / strike; }
        else if (t < wind + strike + rec) { c = art::Clip::Follow; i = (t - wind - strike) < 0.5f * rec ? 0 : 1; }
        else break;
        const int x = left + CW * col + CW / 2;
        stand(b, s, fc, c, v, i, x, gy);
        if (prog >= 0) putFx(b, fam, fc, v, v == 2, prog, x, gy);
        b.c.set(x, gy + 2, c == art::Clip::Windup ? rgba(120, 160, 240) : c == art::Clip::Strike ? rgba(240, 90, 60) : rgba(240, 220, 120));
      }
    }
  rigpv::saveBoth(b, dir, std::string("rigarms_motion_") + art::familyName(fam));
}

void strikesBoard(const std::string& dir) {
  const int CW = 70, RH = 78, top = 24, left = 54;
  Board b(left + CW * 16 + 10, top + RH * 8 + 10);
  b.text(4, 4, "STRIKES: hit 1 strike 0 / 1 of every family; then hits 1 2 3 strike 0 per family");
  std::vector<art::RigSheet> act;
  for (int f = 0; f < art::FAMILY_COUNT; f++) act.push_back(art::humanRig(lookOf((art::Family)f, false), art::RigPart::Action));
  for (int fi = 0; fi < 4; fi++) {
    const int fc = kFacingOrder[fi];
    int gy = top + fi * 2 * RH + 62;
    b.text(4, gy - 34, kFacingName[fc]);
    for (int f = 0; f < 8; f++)
      for (int i = 0; i < 2; i++) stand(b, act[(size_t)f], fc, art::Clip::Strike, 0, i, left + CW * (f * 2 + i) + CW / 2, gy);
    gy += RH;
    for (int f = 0; f < 8; f++) {
      if (f == 2 || f == 5) continue;
      const int slot = f < 2 ? f : f < 5 ? f - 1 : f - 2;
      for (int v = 0; v < 3; v++) {
        const int col = slot * 3 + v;
        if (col < 16) stand(b, act[(size_t)f], fc, art::Clip::Strike, v, 0, left + CW * col + CW / 2, gy);
      }
    }
  }
  rigpv::saveBoth(b, dir, "rigarms_strikes");
}

void weaponsBoard(const std::string& dir) {
  using namespace art::rig2;
  static const int ws[] = {5, 1, 7, 2, 6, 8, 4, 9};
  const int CW = 52, RH = 52, top = 20, left = 40;
  Board b(left + CW * 16 + 10, top + RH * 8 + 10);
  b.text(4, 4, "WEAPONS at 16 angles (0 = right, clockwise); the grip at the cell centre");
  art::HumanLook L = rigpv::sliceHero(false);
  for (int wi = 0; wi < 8; wi++) {
    L.weapon = (uint8_t)ws[wi];
    Kit K;
    makeKit(L, K);
    b.text(4, top + wi * RH + RH / 2, std::to_string(ws[wi]));
    for (int d = 0; d < 16; d++) {
      Fig f(48, 48, 24, 24);
      f.begin();
      int tx, ty;
      paintArms(f, K, ws[wi], -0.5f + 0.0f, -0.5f, d, 0, 1, tx, ty);
      Canvas c(48, 48);
      c.px = f.px;
      art::outlineInk(c);
      b.put(c, left + d * CW + CW / 2 - 24, top + wi * RH + RH / 2 - 24);
      b.c.set(left + d * CW + CW / 2, top + wi * RH + RH / 2, rgba(255, 60, 200));
    }
  }
  rigpv::saveBoth(b, dir, "rigarms_weapons");
}

void fxBoard(const std::string& dir) {
  Board b(420, 400);
  int y = 6;
  b.text(4, y, "FAMILY EFFECTS (as painted, facing right)");
  y += 14;
  for (int fx = (int)art::Fx::SwingArc; fx < (int)art::Fx::COUNT; fx++) {
    const Canvas c = art::fxSprite((art::Fx)fx);
    b.text(4, y + 4, std::to_string(fx - (int)art::Fx::SwingArc + 1));
    const int w = art::fxW((art::Fx)fx);
    for (int k = 0; k < art::fxFrames((art::Fx)fx); k++) {
      Canvas cell(w, art::fxH((art::Fx)fx));
      for (int yy = 0; yy < cell.h; yy++) for (int xx = 0; xx < w; xx++) cell.set(xx, yy, c.get(k * w + xx, yy));
      b.frame(30 + k * (w + 6) - 1, y - 1, w + 2, cell.h + 2);
      b.put(cell, 30 + k * (w + 6), y);
    }
    y += std::max(14, art::fxH((art::Fx)fx) + 6);
  }
  rigpv::saveBoth(b, dir, "rigarms_fx");
}

// R7 / R8 / R9 / R10 as rpg_test --rig measures them (a quick read while painting; the harness is the judge's)
float differ(const art::RigSheet& sa, int fa, art::Clip ca, int va, int ia, const art::RigSheet& sb, int fb, art::Clip cb, int vb, int ib) {
  int pax, pay, pbx, pby;
  const Canvas a = rigpv::frameCanvas(sa, fa, ca, va, ia, pax, pay), b = rigpv::frameCanvas(sb, fb, cb, vb, ib, pbx, pby);
  if (a.w <= 0 || b.w <= 0) return -1;
  int un = 0, dif = 0;
  const int x0 = -std::max(pax, pbx), x1 = std::max(a.w - pax, b.w - pbx), y0 = -std::max(pay, pby), y1 = std::max(a.h - pay, b.h - pby);
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) {
      const uint32_t ca2 = a.get(pax + x, pay + y), cb2 = b.get(pbx + x, pby + y);
      const bool oa = (ca2 >> 24) != 0, ob = (cb2 >> 24) != 0;
      if (oa || ob) un++;
      if (oa != ob || (oa && ca2 != cb2)) dif++;
    }
  return un ? (float)dif / un : 0;
}
void tips() {
  std::vector<art::RigSheet> act;
  for (int f = 0; f < art::FAMILY_COUNT; f++) act.push_back(art::humanRig(lookOf((art::Family)f, false), art::RigPart::Action));
  for (int f = 0; f < art::FAMILY_COUNT; f++) {
    const art::RigSheet& s = act[(size_t)f];
    std::printf("%-10s atlas %4dx%-4d %6.1f KB\n", art::familyName((art::Family)f), s.atlas.w, s.atlas.h, s.bytes() / 1024.0);
    for (int v = 0; v < 3; v++) {
      std::printf("  hit %d right tips:", v + 1);
      float tot = 0, minY = 1e9f, maxY = -1e9f, maxX = 0, pa = 0;
      bool first = true;
      int n = 0;
      float ax0 = 0, ay0 = 0, bx = 0, by = 0;
      for (art::Clip c : {art::Clip::Windup, art::Clip::Strike, art::Clip::Follow})
        for (int i = 0; i < s.count(2, c, v); i++) {
          const art::RigAnchors* an = s.anchorsOf(2, c, v, i);
          if (!an || an->tipX == art::kNoAnchor) { std::printf(" --"); continue; }
          std::printf(" (%d,%d)", an->tipX, an->tipY);
          const float a = std::atan2((float)(an->tipY - an->gripY), (float)(an->tipX - an->gripX));
          if (!first) { float d = a - pa; while (d > 3.14159f) d -= 6.28318f; while (d < -3.14159f) d += 6.28318f; tot += d; }
          first = false; pa = a;
          minY = std::min(minY, (float)an->tipY); maxY = std::max(maxY, (float)an->tipY); maxX = std::max(maxX, (float)std::abs(an->tipX));
          if (n == 0) { ax0 = an->tipX; ay0 = an->tipY; }
          if (n == 3) { bx = an->tipX; by = an->tipY; }
          n++;
        }
      std::printf("\n      turn %.0f deg, fall %.0f, lowest %.0f, reach %.0f, along %.0f side %.0f\n", std::fabs(tot) * 57.3f, maxY - minY, -maxY, maxX,
                  std::fabs(bx - ax0), std::fabs(by - ay0));
    }
  }
  // hands over the face (side views and facing down: the hand is drawn over the head) and tips under the ground line
  for (int f = 0; f < art::FAMILY_COUNT; f++) {
    const art::RigSheet& s = act[(size_t)f];
    for (int fc = 0; fc < 4; fc++)
      for (int c = 0; c < art::CLIP_COUNT; c++)
        for (int v = 0; v < 3; v++)
          for (int i = 0; i < s.spans[art::RigSheet::slot(fc, (art::Clip)c, v)].count; i++) {
            const art::RigAnchors* an = s.anchorsOf(fc, (art::Clip)c, v, i);
            if (!an || an->headX == art::kNoAnchor) continue;
            // the face: side views the front half of the head box, facing down its middle; rows brows .. chin
            int fx0, fx1;
            const int fy0 = an->headY + 4, fy1 = an->headY + 10;
            if (fc == 2) { fx0 = an->headX + 6; fx1 = an->headX + 11; }
            else if (fc == 3) { fx0 = an->headX; fx1 = an->headX + 5; }
            else if (fc == 0) { fx0 = an->headX + 1; fx1 = an->headX + 10; }
            else continue;
            for (int h = 0; h < 2; h++) {
              const int hx = h ? an->handLX : an->handRX, hy = h ? an->handLY : an->handRY;
              if (hx == art::kNoAnchor) continue;
              if (hx + 1 >= fx0 && hx <= fx1 && hy + 1 >= fy0 && hy <= fy1)
                std::printf("FACE %s %s f%d v%d #%d: the %s hand (%d,%d) over the face\n", art::familyName((art::Family)f),
                            art::clipName((art::Clip)c), fc, v, i, h ? "off" : "weapon", hx, hy);
            }
            if (an->tipX != art::kNoAnchor && an->tipY > 0 && fc >= 2)
              std::printf("GROUND %s %s f%d v%d #%d: the tip %d px under the ground\n", art::familyName((art::Family)f), art::clipName((art::Clip)c), fc, v, i,
                          an->tipY);
          }
  }
  // R7: strike frames of every pair of melee families (hit 1), per facing; R9: a family's variants pairwise
  for (int fc = 0; fc < 4; fc++) {
    float worst7 = 2, worst9 = 2;
    std::string w7, w9;
    for (int a = 0; a < 8; a++)
      for (int b2 = a + 1; b2 < 8; b2++)
        for (int i = 0; i < 2; i++) {
          const float d = differ(act[(size_t)a], fc, art::Clip::Strike, 0, i, act[(size_t)b2], fc, art::Clip::Strike, 0, i);
          if (d < worst7) { worst7 = d; w7 = std::string(art::familyName((art::Family)a)) + "/" + art::familyName((art::Family)b2); }
        }
    for (int f = 0; f < 8; f++)
      for (int va = 0; va < 3; va++)
        for (int vb = va + 1; vb < 3; vb++) {
          const float d = differ(act[(size_t)f], fc, art::Clip::Strike, va, 0, act[(size_t)f], fc, art::Clip::Strike, vb, 0);
          if (d < worst9) { worst9 = d; w9 = std::string(art::familyName((art::Family)f)) + " " + std::to_string(va) + "/" + std::to_string(vb); }
        }
    std::printf("facing %d: R7 worst %.0f %% (%s), R9 worst %.0f %% (%s)\n", fc, worst7 * 100, w7.c_str(), worst9 * 100, w9.c_str());
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::printf("usage: rigarms_gallery <outDir> [woman | tips]\n"); return 2; }
  const std::string dir = argv[1];
  art::setRigVersion(2);
  const bool woman = argc > 2 && std::string(argv[2]) == "woman";
  if (argc > 2 && std::string(argv[2]) == "tips") { tips(); return 0; }
  for (int f = 0; f < art::FAMILY_COUNT; f++) familyBoard(dir, (art::Family)f, woman);
  if (woman) return 0;
  for (int f = 0; f < art::FAMILY_COUNT; f++) motionBoard(dir, (art::Family)f);
  strikesBoard(dir);
  weaponsBoard(dir);
  fxBoard(dir);
  return 0;
}
