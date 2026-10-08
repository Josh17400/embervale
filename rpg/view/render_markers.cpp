// World-space markers over actors: a bobbing "!" speech bubble over quest givers with a reward waiting
// (Game::rewardWaiting). M0 town-defence lane.
// M2 (UI lane): the wayside people (a vignette's hunter, fisher, herbalist or traveller) who have work to offer wear a
// bare gold "!" (no bubble: the bubble means a reward is waiting), and the person a Missing quest has the player walk
// home (the escort, whose name is the quest's subject) wears a teal chevron and a ring at the feet, so they can be
// told from a crowd and seen to follow.
// Pixel art at 1x (11 x 14): a cream bubble with a near-black outline reads on grass, snow, stone and warm interior
// wood alike; the "!" is orange-gold, lit from the top-left like every other sprite, the bubble's lower-right edge is
// shaded, a 1 px drop shadow lifts it off the scene, and the tail points down at the giver's head.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "rpg/view/view.h"

namespace {
// 'o' outline, 'W' bubble, 'D' bubble shade, 'H' "!" highlight, 'Y' "!" gold, 'S' "!" shade, '.' empty
const char* kBubble[14] = {
    "..ooooooo..",
    ".oWWWWWWWo.",
    "oWWWHYSWWWo",
    "oWWWHYSWWWo",
    "oWWWHYSWWDo",
    "oWWWWYSWWDo",
    "oWWWWYWWWDo",
    "oWWWWWWWWDo",
    "oWWWHYSWWDo",
    "oWWWYSSWWDo",
    ".oDDDDDDDo.",
    "..oooDooo..",
    "....oDo....",
    ".....o.....",
};
// a bare gold "!" with a dark outline over the head (work on offer)
const char* kBang[12] = {".ooo.", "oHYSo", "oHYSo", "oHYSo", "oHYSo", ".oYSo", ".oYo.", ".oYo.", "..o..", ".ooo.", "oHYSo", ".ooo."};
void drawOfferMark(Pix& P, const Actor& a, Vec2 cam, float t, Color outline, Color hi, Color gold, Color goldShade) {
  const int bob = (int)std::lround(std::sin(t * 3.0f) * 1.2f);
  const int x0 = (int)std::floor(a.p.x - cam.x) - 2, y0 = (int)std::floor(a.p.y - cam.y) - art::HUMAN_H - 13 + bob;
  for (int pass = 0; pass < 2; pass++)
    for (int r = 0; r < 12; r++)
      for (int c = 0; c < 5; c++) {
        const char ch = kBang[r][c];
        if (ch == '.') continue;
        if (pass == 0) { P.rect((float)(x0 + c + 1), (float)(y0 + r + 1), 1, 1, Color(0, 0, 0, 0.35f)); continue; }
        P.rect((float)(x0 + c), (float)(y0 + r), 1, 1, ch == 'o' ? outline : ch == 'H' ? hi : ch == 'S' ? goldShade : gold);
      }
}
// the escort: a teal chevron over the head and a soft ring at the feet
void drawEscortMark(Pix& P, const Actor& a, Vec2 cam, float t, Color outline) {
  const Color hi(0.62f, 1.0f, 0.92f), mid(0.22f, 0.78f, 0.70f), lo(0.10f, 0.46f, 0.44f);
  const float fx = std::floor(a.p.x - cam.x), fy = std::floor(a.p.y - cam.y);
  for (int k = 0; k < 48; k++) {   // the ring (an ellipse, brighter at the front)
    const float an = k / 48.0f * 6.2831853f;
    if (std::sin(an) < -0.15f) continue;   // the back half would cross the legs (markers draw over the sprites)
    const float x = fx + std::cos(an) * 9.0f, y = fy + 1 + std::sin(an) * 4.0f;
    P.rect(std::floor(x), std::floor(y), 1, 1, Color(mid.r, mid.g, mid.b, std::sin(an) > 0 ? 0.85f : 0.45f));
  }
  static const char* kChev[6] = {"ooooooo", "oHMMMLo", ".oHMLo.", "..oMo..", "...o...", "......."};
  const int bob = (int)std::lround(std::sin(t * 3.4f) * 1.2f);
  const int x0 = (int)fx - 3, y0 = (int)fy - art::HUMAN_H - 9 + bob;
  for (int pass = 0; pass < 2; pass++)
    for (int r = 0; r < 6; r++)
      for (int c = 0; c < 7; c++) {
        const char ch = kChev[r][c];
        if (ch == '.') continue;
        if (pass == 0) { P.rect((float)(x0 + c + 1), (float)(y0 + r + 1), 1, 1, Color(0, 0, 0, 0.35f)); continue; }
        P.rect((float)(x0 + c), (float)(y0 + r), 1, 1, ch == 'o' ? outline : ch == 'H' ? hi : ch == 'L' ? lo : mid);
      }
}
}  // namespace

// (M5, VISION_PLAN 10.3) the townsfolk's speech bubbles (Actor::bubble: art::bubbleSprite, about 11 x 10, its tail at the
// bottom centre) over their heads, drawn after the night so they read in the dark: a two-frame pop-in when one appears
// (a 5 x 3 puff, then the bubble a pixel high, then settled), a slow one-pixel bob, a fade over its last third of a
// second (Actor::bubbleT: the seconds it has left). Whole pixels only (crisp at 1x on the phone). Over a quest giver's
// "!" it steps aside to the right.
void View::drawBubbles(Game& g, Vec2 cam) {
  Pix& P = *pix_;
  static std::vector<int> live;
  live.clear();
  for (const Actor& a : g.actors) {
    if (a.bubble == art::Bubble::None || !(a.bubbleT > 0) || a.st == AState::Dead) continue;
    if (a.p.x < cam.x - 24 || a.p.x > cam.x + Pix::W + 24 || a.p.y < cam.y - 8 || a.p.y > cam.y + Pix::H + 48) continue;
    live.push_back(a.id);
    BubbleSeen& bs = bubbleSeen_[a.id];
    if (bs.kind != (uint8_t)a.bubble || a.bubbleT > bs.maxT + 0.05f || t_ - bs.seen > 0.5f) { bs.kind = (uint8_t)a.bubble; bs.t0 = t_; bs.maxT = a.bubbleT; }
    bs.maxT = std::min(bs.maxT, a.bubbleT);
    bs.seen = t_;
    const float age = t_ - bs.t0;
    const uint64_t key = 0x4Dull << 56 | (uint64_t)a.bubble;
    const Tex& t = cachedTex(key, [](uint64_t k) { return art::bubbleSprite((art::Bubble)(k & 255)); });
    // the head: the standing figure's top (a seated or sleeping body sits lower: its pose's dy)
    float dy = 0;
    if (a.human && a.posture != art::Posture::None) dy = (float)art::postureInfo(a.posture).dy;
    const float headY = a.p.y - (a.human ? (float)art::HUMAN_H - 3.0f : (a.critter ? (float)art::critterCellH((art::Critter)(a.critter - 1)) : 18.0f)) + dy;
    float ax = a.p.x, hy = headY;
    bool seatedDrawn = a.human && art::postureInfo(a.posture).seated;
    // (fixer M5 r3) a human's bubble rides on the head as it was drawn this frame (a posture's sink or a seat's fit is
    // applied only when the figure is drawn in it: a seated posture drawn standing put the bubble over the face)
    if (a.human) {
      auto dh = drawnHead_.find(a.id);
      if (dh != drawnHead_.end() && dh->second.t == t_) {
        ax = dh->second.x;
        hy = dh->second.y + 1.0f;
        seatedDrawn = seatedDrawn && hy > a.p.y - (float)art::HUMAN_H + 4.0f;   // drawn sunk: it really sits
      }
    }
    int cx = (int)std::floor(ax - cam.x) + (g.rewardWaiting(a) ? 9 : 2);
    int by = (int)std::floor(hy - cam.y) - 1 + (int)std::lround(std::sin(t_ * 2.2f + a.id * 0.9f) * 0.6f);
    // (M5 fixer r2) a sitter with its back to us faces a table: its bubble rises beside the head, not over the tabletop
    if (a.human && a.face == 1 && a.useX >= 0 && seatedDrawn) { cx += 9; by += 4; }
    // a sleeper's bubble floats up from the pillow, beside the head (never over the face)
    if (a.human && a.posture == art::Posture::Sleep && a.useX >= 0 && a.useY >= 0 && g.map().in(a.useX, a.useY)) {
      const int pr = g.map().propAt(a.useX, a.useY);
      const art::Berth bk = pr == (int)art::Prop::BunkBed + 1 ? art::Berth::BunkLow : pr == (int)art::Prop::Hammock + 1 ? art::Berth::Hammock :
                            pr == (int)art::Prop::SleepingMat + 1 ? art::Berth::Mat : pr == (int)art::Prop::Bedroll + 1 ? art::Berth::Bedroll :
                            pr ? art::Berth::Bed : art::Berth::Ground;
      const art::BedFit bf = art::bedFit(bk, -1);
      const float bx = a.useX * 16.0f + 8.0f - bf.w / 2.0f, btop = a.useY * 16.0f + 16.0f - bf.h;
      cx = (int)std::floor(bx + bf.headX + 12 - cam.x);
      by = (int)std::floor(btop + bf.headY - 8 - cam.y) + (int)std::lround(std::sin(t_ * 1.4f + a.id * 0.9f) * 0.8f);
    }
    const float alpha = clampf(a.bubbleT / 0.33f, 0, 1);
    if (age < 0.05f) {   // the puff
      P.rect((float)cx - 2, (float)by - 3, 5, 3, Color(0.09f, 0.06f, 0.08f, alpha));
      P.rect((float)cx - 1, (float)by - 2, 3, 1, Color(1.0f, 0.97f, 0.88f, alpha));
      m5Count_.bubbles++;
      continue;
    }
    if (age < 0.11f) by -= 1;   // overshoots a pixel, then settles
    const int x0 = cx - t.w / 2, y0 = by - t.h;
    P.blitEx(t, 0, 0, t.w, t.h, (float)x0 + 1, (float)y0 + 1, (float)t.w, (float)t.h, false, Color(0, 0, 0, 0.30f * alpha));   // drop shadow
    P.blitEx(t, 0, 0, t.w, t.h, (float)x0, (float)y0, (float)t.w, (float)t.h, false, Color(1, 1, 1, alpha));
    m5Count_.bubbles++;
  }
  // forget the actors whose bubbles have gone
  if (drawnHead_.size() > 512)
    for (auto it = drawnHead_.begin(); it != drawnHead_.end();) {
      if (t_ - it->second.t > 1.0f) it = drawnHead_.erase(it); else ++it;
    }
  if (bubbleSeen_.size() > live.size() + 32)
    for (auto it = bubbleSeen_.begin(); it != bubbleSeen_.end();) {
      if (t_ - it->second.seen > 1.0f) it = bubbleSeen_.erase(it); else ++it;
    }
}

void View::drawMarkers(Game& g, Vec2 cam) {
  Pix& P = *pix_;
  drawBubbles(g, cam);
  const Color outline(0.09f, 0.06f, 0.08f), paper(1.0f, 0.97f, 0.88f), paperShade(0.82f, 0.74f, 0.62f);
  const Color hi(1.0f, 0.86f, 0.38f), gold(1.0f, 0.64f, 0.10f), goldShade(0.74f, 0.33f, 0.05f);
  // the escorts: the subjects of the active Missing quests
  std::vector<const std::string*> escorts;
  for (const Quest& q : g.quests)
    if (q.type == QType::Missing && q.state == QState::Active && !q.subject.empty()) escorts.push_back(&q.subject);
  for (const Actor& a : g.actors) {
    if (!a.npc || a.st == AState::Dead) continue;
    if (a.p.x < cam.x - 20 || a.p.x > cam.x + Pix::W + 20 || a.p.y < cam.y - 10 || a.p.y > cam.y + Pix::H + 40) continue;
    bool escort = false;
    for (const std::string* s : escorts) if (a.name.find(*s) != std::string::npos || s->find(a.name) == 0) escort = true;
    if (escort) { drawEscortMark(P, a, cam, t_, outline); continue; }
    if (!g.rewardWaiting(a)) {
      const bool wayside = a.role == Role::Hunter || a.role == Role::Fisher || a.role == Role::Herbalist || a.role == Role::Traveller;
      if (wayside && g.offersWork(a)) drawOfferMark(P, a, cam, t_ + a.id * 0.57f, outline, hi, gold, goldShade);
      continue;
    }
    // a slow bob snapped to whole pixels (no shimmer), each giver on their own phase
    float ph = t_ * 3.0f + a.id * 1.7f;
    int bob = (int)std::lround(std::sin(ph) * 1.2f);
    int x0 = (int)std::floor(a.p.x - cam.x) - 5;
    int y0 = (int)std::floor(a.p.y - cam.y) - art::HUMAN_H - 15 + bob;
    for (int pass = 0; pass < 2; pass++)   // 0: the drop shadow, 1: the bubble
      for (int r = 0; r < 14; r++)
        for (int c = 0; c < 11; c++) {
          char ch = kBubble[r][c];
          if (ch == '.') continue;
          if (pass == 0) { P.rect((float)(x0 + c + 1), (float)(y0 + r + 1), 1, 1, Color(0, 0, 0, 0.35f)); continue; }
          Color col = ch == 'o' ? outline : ch == 'W' ? paper : ch == 'D' ? paperShade : ch == 'H' ? hi : ch == 'S' ? goldShade : gold;
          P.rect((float)(x0 + c), (float)(y0 + r), 1, 1, col);
        }
  }

  // M0b: the inn's rented rooms carry brass number plates over their doors, and the room you rented has a gold
  // marker bobbing over its door, so "room 7, the fourth door on the right" can be found at a glance
  if (g.inside && g.subBldg >= 0 && g.world.over.bldgs[(size_t)g.subBldg].type == art::Building::Inn) {
    const Map& m = g.sub;
    const bool rented = g.lodgingActive() && g.lodging.bldg == g.subBldg && g.lodging.floor == g.subFloor;
    for (size_t i = 0; i < m.rooms.size(); i++) {
      const RoomInfo& R = m.rooms[i];
      if (R.kind != RoomKind::GuestRoom || R.guest < 0 || R.doorX < 0) continue;
      bool h = m.propAt(R.doorX, R.doorY) == (int)art::Prop::DoorH + 1;
      // over the lintel: an E-W door's frame rises through the cap row above its face-row tile
      int cxp = (int)std::floor(R.doorX * 16 + 8 - cam.x);
      int cyp = (int)std::floor((R.doorY - (h ? 1 : 0)) * 16 - cam.y) + (h ? 6 : 1);
      std::string num = std::to_string((int)R.guest + 1);
      // fix round 3: the plate reads as a fitting on the wall, not a UI badge: a small dark oak board (rounded corners,
      // lit on its top-left edge, shaded bottom-right, a nail head either side) holding an inset brass plate whose
      // digits are punched in dark, a soft shadow on the wall
      const Color oakHi(0.55f, 0.36f, 0.20f), oak(0.40f, 0.25f, 0.13f), oakLo(0.26f, 0.15f, 0.08f);
      const Color plate(0.78f, 0.58f, 0.27f), plateHi(0.95f, 0.80f, 0.48f), plateLo(0.55f, 0.38f, 0.16f);
      const Color punch(0.24f, 0.13f, 0.06f), nail(0.70f, 0.66f, 0.60f);
      const int tw = P.textW(num, 1), w = tw + 8, ph = 13;
      int x0 = cxp - w / 2, y0 = cyp - 6;
      auto px = [&](int x, int y, Color c) { P.rect((float)x, (float)y, 1, 1, c); };
      // shadow on the wall (down-right, light from the top-left)
      P.rect((float)x0 + 2, (float)y0 + ph, (float)w - 2, 1, Color(0, 0, 0, 0.30f));
      P.rect((float)x0 + w, (float)y0 + 2, 1, (float)ph - 2, Color(0, 0, 0, 0.30f));
      // the board, outlined, corners rounded
      P.rect((float)x0 + 1, (float)y0, (float)w - 2, (float)ph, outline);
      P.rect((float)x0, (float)y0 + 1, (float)w, (float)ph - 2, outline);
      P.rect((float)x0 + 1, (float)y0 + 1, (float)w - 2, (float)ph - 2, oak);
      P.rect((float)x0 + 1, (float)y0 + 1, (float)w - 2, 1, oakHi);
      P.rect((float)x0 + 1, (float)y0 + 1, 1, (float)ph - 2, oakHi);
      P.rect((float)x0 + 1, (float)y0 + ph - 2, (float)w - 2, 1, oakLo);
      P.rect((float)x0 + w - 2, (float)y0 + 2, 1, (float)ph - 3, oakLo);
      // the inset brass plate: dark top/left lip (it sits below the board's face), lit bottom/right lip
      const int bx = x0 + 3, by = y0 + 2, bw = w - 6, bh = ph - 4;
      P.rect((float)bx, (float)by, (float)bw, (float)bh, plate);
      P.rect((float)bx, (float)by, (float)bw, 1, plateLo);
      P.rect((float)bx, (float)by, 1, (float)bh, plateLo);
      P.rect((float)bx + 1, (float)by + bh - 1, (float)bw - 1, 1, plateHi);
      P.rect((float)bx + bw - 1, (float)by + 1, 1, (float)bh - 1, plateHi);
      // nail heads at the board's ends
      px(x0 + 1, y0 + ph / 2, nail);
      px(x0 + w - 2, y0 + ph / 2, nail);
      // the digits, punched dark into the brass (a highlight copy under them closed the 6 into an 8)
      P.text((float)bx + 1, (float)by + 1, num, 1, punch, 0);
      y0 -= 1;
      if (!rented || (int)i != g.lodging.room) continue;
      // yours: a gold arrow bobbing over the plate
      int bob = (int)std::lround(std::sin(t_ * 3.2f) * 1.5f);
      int ay = y0 - 11 + bob;
      static const char* kArrow[7] = {"ooooooo", "oYYYYYo", "oHYYYSo", ".oHYSo.", ".oHYSo.", "..oYo..", "...o..."};
      for (int r = 0; r < 7; r++)
        for (int c = 0; c < 7; c++) {
          char ch = kArrow[r][c];
          if (ch == '.') continue;
          P.rect((float)(cxp - 3 + c + 1), (float)(ay + r + 1), 1, 1, Color(0, 0, 0, 0.35f));
          P.rect((float)(cxp - 3 + c), (float)(ay + r), 1, 1, ch == 'o' ? outline : ch == 'H' ? hi : ch == 'S' ? goldShade : gold);
        }
    }
    // M0b fix round 3: on another floor than your room, the same gold arrow bobs over the stairs that lead toward
    // it. The inn's flight often stands in a back corner under the HUD, so when the stairs are off screen or under
    // the HUD the arrow waits at the edge of the clear area and points at them.
    if (g.lodgingActive() && g.lodging.bldg == g.subBldg && g.lodging.floor != g.subFloor) {
      const Stairs& st = g.lodging.floor > g.subFloor ? m.up : m.down;
      if (st.valid()) {
        // the middle of the flight (both of its tiles), at the top of the steps
        const int sp = m.propAt(st.x, st.y);
        int x0 = st.x, x1 = st.x;
        while (m.propAt(x0 - 1, st.y) == sp) x0--;
        while (m.propAt(x1 + 1, st.y) == sp) x1++;
        float tx = (x0 + x1 + 1) * 8.0f - cam.x, ty = st.y * 16.0f - 6 - cam.y;
        const float L = 10.0f + Pix::SL, T = 54.0f + Pix::ST, R = Pix::W - Pix::SR - 142.0f, B = Pix::H - Pix::SB - (touchUI ? 56.0f : 12.0f);
        float cx = clampf(tx, L, R), cy = clampf(ty, T, B);
        bool off = std::fabs(cx - tx) > 0.5f || std::fabs(cy - ty) > 0.5f;
        // dir 0: down (at the stairs), 1: up, 2: left, 3: right (pointing out of the clear area at them)
        int dir = 0;
        if (off) {
          float dx = tx - cx, dy = ty - cy;
          dir = std::fabs(dx) > std::fabs(dy) ? (dx < 0 ? 2 : 3) : (dy < 0 ? 1 : 0);
        }
        float ph = std::sin(t_ * 3.2f) * 1.5f;
        int ox = (int)std::floor(cx) + (dir == 2 ? -(int)std::lround(ph) : dir == 3 ? (int)std::lround(ph) : 0);
        int oy = (int)std::floor(cy) + (dir <= 1 ? (dir == 1 ? -(int)std::lround(ph) : (int)std::lround(ph)) : 0);
        static const char* kArrow[7] = {"ooooooo", "oYYYYYo", "oHYYYSo", ".oHYSo.", ".oHYSo.", "..oYo..", "...o..."};
        for (int r = 0; r < 7; r++)
          for (int c = 0; c < 7; c++) {
            // rotate the down-pointing pattern: (r, c) is the cell of the drawn grid
            int rr = r, cc = c;
            if (dir == 1) { rr = 6 - r; cc = c; }
            else if (dir == 2) { rr = 6 - c; cc = r; }
            else if (dir == 3) { rr = c; cc = r; }
            char ch = kArrow[rr][cc];
            if (ch == '.') continue;
            int px = ox - 3 + c, py = oy - (dir == 0 ? 7 : 3) + r;
            P.rect((float)(px + 1), (float)(py + 1), 1, 1, Color(0, 0, 0, 0.35f));
            P.rect((float)px, (float)py, 1, 1, ch == 'o' ? outline : ch == 'H' ? hi : ch == 'S' ? goldShade : gold);
          }
      }
    }
  }
}
