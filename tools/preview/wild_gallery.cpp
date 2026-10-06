// Wild gallery (M2 VIEW lane): every M2 wayside and wonder prop (rpg/art/art_wild.cpp), every Peak variant in its
// three lands, a mountain range composed the way the game y-sorts it, and the whole board again by night.
//   wild_gallery <outDir>      writes wild_day.png, wild_night.png (3x) and wild_1x.png (day | night at 1x)
#include "tools/preview/preview_util.h"

namespace {

using art::Prop;

// the ground a range stands on: grass with the darker cast shadows under each peak
void shadowUnder(Board& b, int cx, int by, int w) {
  for (int y = by - 4; y <= by + 3; y++)
    for (int x = cx - w / 2; x <= cx + w / 2 + 6; x++) {
      const float dx = (x - (cx + 3.0f)) / (w * 0.5f + 3), dy = (y - (float)by) / 4.0f;
      if (dx * dx + dy * dy > 1) continue;
      b.c.set(x, y, art::shade(b.c.get(x, y), 0.72f));
    }
}

// night: a cool, dark grade; the glowing pixels (fire, star crystal) keep their light and throw a soft pool
Canvas night(const Canvas& day, const std::vector<std::pair<int, int>>& lights) {
  Canvas n = day;
  for (int y = 0; y < n.h; y++)
    for (int x = 0; x < n.w; x++) {
      const uint32_t p = day.get(x, y);
      const int r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
      const bool glow = (r > 200 && g > 120 && b < 120) || (b > 200 && g > 150 && r < 200);
      if (glow) continue;
      n.set(x, y, rgba((int)(r * 0.30f), (int)(g * 0.36f), (int)(b * 0.58f + 14)));
    }
  for (auto& L : lights)
    for (int y = L.second - 30; y <= L.second + 30; y++)
      for (int x = L.first - 30; x <= L.first + 30; x++) {
        const float d = std::hypot((float)(x - L.first), (float)(y - L.second)) / 30.0f;
        if (d >= 1 || x < 0 || y < 0 || x >= n.w || y >= n.h) continue;
        const uint32_t p = n.get(x, y);
        const float k = (1 - d) * (1 - d) * 0.9f;
        n.set(x, y, art::mix(p, art::mix(day.get(x, y), rgba(255, 200, 150), 0.25f), k));
      }
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  const int W = 640, H = 600;
  Board b(W, H);
  std::vector<std::pair<int, int>> lights;
  // peaks: 8 variants x 3 lands
  const char* lands[] = {"GREY ROCK", "SNOW-CAPPED", "SANDSTONE"};
  const int pw = art::propW(Prop::Peak), ph = art::propH(Prop::Peak);
  for (int land = 0; land < 3; land++) {
    b.text(4, 4 + land * (ph + 12), lands[land]);
    for (int v = 0; v < 8; v++) {
      Canvas k = art::peakVariant(v, land);
      const int x = 4 + v * (pw + 4), y = 12 + land * (ph + 12);
      shadowUnder(b, x + pw / 2, y + ph - 2, pw - 8);
      b.put(k, x, y);
    }
  }
  // the other props, one each (animated ones: frame 0)
  int x = 4, y = 12 + 3 * (ph + 12) + 4;
  const Prop rest[] = {Prop::StandingStone, Prop::CaravanWreck, Prop::WatchtowerRuin, Prop::FishingShack, Prop::TollPost, Prop::HerbBed,
                       Prop::GraveCairn, Prop::Bedroll, Prop::StarShard};
  int rowH = 0;
  for (Prop p : rest) {
    Canvas s = art::propSprite(p);
    const int fw = art::propW(p), fh = art::propH(p);
    Canvas cell(fw, fh);
    for (int j = 0; j < fh; j++) for (int i = 0; i < fw; i++) cell.set(i, j, s.get(i, j));
    shadowUnder(b, x + fw / 2, y + fh - 2, fw - 4);
    b.put(cell, x, y);
    if (p == Prop::StarShard || p == Prop::Bedroll) lights.push_back({x + fw * 3 / 4, y + fh - 4});
    x += fw + 6;
    rowH = std::max(rowH, fh);
  }
  // star shard frames and bedroll frames side by side
  for (Prop p : {Prop::StarShard, Prop::Bedroll}) {
    Canvas s = art::propSprite(p);
    b.put(s, x, y + (p == Prop::Bedroll ? 40 : 0));
    x += s.w + 6;
  }
  // the wonders
  y += rowH + 8;
  x = 4;
  for (Prop p : {Prop::ElderTree, Prop::Colossus, Prop::DragonBones}) {
    Canvas s = art::propSprite(p);
    shadowUnder(b, x + s.w / 2, y + s.h - 2, s.w - 8);
    b.put(s, x, y);
    x += s.w + 8;
  }
  // a range: peaks of three variants overlapping as the game y-sorts them (back rows first)
  {
    const int rx = x + 4, ry = y + 4;
    struct P { int v, dx, dy; } pk[] = {{2, 0, 0}, {5, 40, -6}, {0, 82, 2}, {7, 20, 22}, {3, 64, 26}, {4, 104, 20}};
    for (auto& q : pk) {
      Canvas k = art::peakVariant(q.v, 1);
      shadowUnder(b, rx + q.dx + pw / 2, ry + q.dy + ph - 2, pw - 8);
      b.put(k, rx + q.dx, ry + q.dy);
    }
  }
  savePng(b.c, dir + "/wild_day.png", kScale);
  Canvas n = night(b.c, lights);
  savePng(n, dir + "/wild_night.png", kScale);
  Canvas both(W * 2 + 4, H);
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++) { both.set(i, j, b.c.get(i, j)); both.set(W + 4 + i, j, n.get(i, j)); }
  savePng(both, dir + "/wild_1x.png", 1);
  return 0;
}
