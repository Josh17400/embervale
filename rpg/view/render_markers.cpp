// World-space markers over actors: a bobbing "!" speech bubble over quest givers with a reward waiting
// (Game::rewardWaiting). M0 town-defence lane.
// Pixel art at 1x (11 x 14): a cream bubble with a near-black outline reads on grass, snow, stone and warm interior
// wood alike; the "!" is orange-gold, lit from the top-left like every other sprite, the bubble's lower-right edge is
// shaded, a 1 px drop shadow lifts it off the scene, and the tail points down at the giver's head.
#include <cmath>
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
}  // namespace

void View::drawMarkers(Game& g, Vec2 cam) {
  Pix& P = *pix_;
  const Color outline(0.09f, 0.06f, 0.08f), paper(1.0f, 0.97f, 0.88f), paperShade(0.82f, 0.74f, 0.62f);
  const Color hi(1.0f, 0.86f, 0.38f), gold(1.0f, 0.64f, 0.10f), goldShade(0.74f, 0.33f, 0.05f);
  for (const Actor& a : g.actors) {
    if (!a.npc || a.st == AState::Dead) continue;
    if (a.p.x < cam.x - 20 || a.p.x > cam.x + Pix::W + 20 || a.p.y < cam.y - 10 || a.p.y > cam.y + Pix::H + 40) continue;
    if (!g.rewardWaiting(a)) continue;
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
}
