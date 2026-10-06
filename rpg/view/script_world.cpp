// M2 WORLD lane script commands (rpg/view/script_api.h; `embervale --script-help` lists them):
//   gotopoi <kind> [n]   stand just south of the n-th nearest (default 0) place of that kind, searching the region plans
//                        within 8 regions: caravan, hunter, stones, watchtower, fishing, toll, herbs, grave, wayrest
//                        (vignettes), eldertree, colossus, starfall, dragonbones (wonders), wonder (any wonder),
//                        vignette (any wayside place), range or peak (a named range's crest / a peak label, a few tiles
//                        south of it), pass (a named pass), snowpeak (a range or peak in cold country), ecotone (the
//                        nearest chunk with a broad band of biome blend)
//   expect poi <kind>    a place of that kind lies within 24 tiles of the player
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"
#include "rpg/view/view.h"
#include "rpg/world/source.h"

namespace {

struct PoiWord { const char* w; int type; int kind; };   // type: 0 vignette, 1 wonder, 2 landmark (kind: LandmarkKind)
const PoiWord kWords[] = {
    {"caravan", 0, (int)ew::VignetteKind::Caravan},       {"hunter", 0, (int)ew::VignetteKind::HunterCamp},
    {"stones", 0, (int)ew::VignetteKind::StandingStones}, {"watchtower", 0, (int)ew::VignetteKind::Watchtower},
    {"fishing", 0, (int)ew::VignetteKind::FishingHut},    {"toll", 0, (int)ew::VignetteKind::TollBridge},
    {"herbs", 0, (int)ew::VignetteKind::HerbGarden},      {"grave", 0, (int)ew::VignetteKind::LoneGrave},
    {"wayrest", 0, (int)ew::VignetteKind::Wayrest},       {"vignette", 0, -1},
    {"eldertree", 1, (int)ew::WonderKind::ElderTree},     {"colossus", 1, (int)ew::WonderKind::Colossus},
    {"starfall", 1, (int)ew::WonderKind::Starfall},       {"dragonbones", 1, (int)ew::WonderKind::DragonBones},
    {"wonder", 1, -1},
    {"range", 2, (int)ew::LandmarkKind::Range},           {"peak", 2, (int)ew::LandmarkKind::Peak},
    {"pass", 2, (int)ew::LandmarkKind::Pass},
    {"snowpeak", 3, (int)ew::LandmarkKind::Range},          // a range or peak label in cold country (snow caps)
    {"ecotone", 4, 0},                                       // a broad biome border (the ecotone band)
};

const PoiWord* wordOf(const std::string& s) {
  for (const PoiWord& w : kWords) if (s == w.w) return &w;
  return nullptr;
}

struct Found { int32_t x = 0, y = 0, gx = 0, gy = 0, w = 1, h = 1; std::string name; double d = 0; };

// every place of that kind in the region plans within R regions of the player, nearest first
std::vector<Found> findPois(Game& g, const PoiWord& pw, int R) {
  std::vector<Found> out;
  if (!g.world.src) return out;
  ew::EndlessSource& A = *g.world.src;
  const int32_t px = g.world.ox + (int32_t)std::floor(g.pl().p.x / TILE), py = g.world.oy + (int32_t)std::floor(g.pl().p.y / TILE);
  const int32_t rx0 = ew::regionOf(px), ry0 = ew::regionOf(py);
  for (int ry = ry0 - R; ry <= ry0 + R; ry++)
    for (int rx = rx0 - R; rx <= rx0 + R; rx++) {
      const ew::RegionPlan& P = A.region(rx, ry);
      if (pw.type == 4) continue;
      if (pw.type == 2 || pw.type == 3) {
        for (const ew::LandmarkPlan& l : P.landmarks) {
          if (pw.type == 2 && (int)l.kind != pw.kind) continue;
          if (pw.type == 3 && ((l.kind != ew::LandmarkKind::Range && l.kind != ew::LandmarkKind::Peak) || A.macro(l.x, l.y).temp > 22000)) continue;
          Found f;
          f.x = l.x; f.y = l.y; f.gx = l.x; f.gy = l.y; f.name = l.name;
          f.d = std::hypot((double)(l.x - px), (double)(l.y - py));
          out.push_back(f);
        }
        continue;
      }
      for (const ew::SitePlan& s : P.sites) {
        const SiteType want = pw.type == 0 ? SiteType::Vignette : SiteType::Wonder;
        if (s.type != want || (pw.kind >= 0 && s.kind != (uint8_t)pw.kind)) continue;
        Found f;
        f.x = s.ex; f.y = s.ey; f.gx = s.gx; f.gy = s.gy; f.w = s.w; f.h = s.h; f.name = s.name;
        f.d = std::hypot((double)(s.ex - px), (double)(s.ey - py));
        out.push_back(f);
      }
    }
  if (pw.type == 4) {
    // the chunks round the player, nearest first: the first one where many tiles carry a strong blend
    ew::ChunkData c;
    const int32_t cx0 = ew::chunkOf(px), cy0 = ew::chunkOf(py);
    for (int r = 0; r <= 12 && out.empty(); r++)
      for (int dy = -r; dy <= r && out.empty(); dy++)
        for (int dx = -r; dx <= r && out.empty(); dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          A.chunk(cx0 + dx, cy0 + dy, c);
          int n = 0, sx = 0, sy = 0;
          for (int i = 0; i < ew::ChunkData::N; i++)
            if ((c.blend[i] >> 4) >= 2) { n++; sx += i % ew::CHUNK; sy += i / ew::CHUNK; }
          if (n < 90) continue;
          Found f;
          f.x = c.cx * ew::CHUNK + sx / n; f.y = c.cy * ew::CHUNK + sy / n;
          f.gx = f.x; f.gy = f.y;
          f.name = "an ecotone";
          f.d = std::hypot((double)(f.x - px), (double)(f.y - py));
          out.push_back(f);
        }
  }
  std::sort(out.begin(), out.end(), [](const Found& a, const Found& b) { return a.d < b.d; });
  return out;
}

bool cmdGotoPoi(ScriptCtx& c) {
  Game& g = c.game;
  const PoiWord* pw = wordOf(c.arg(1));
  if (!pw) { c.fail("gotopoi: caravan|hunter|stones|watchtower|fishing|toll|herbs|grave|wayrest|vignette|eldertree|colossus|starfall|dragonbones|wonder|range|peak|pass|snowpeak|ecotone"); return true; }
  const int nth = c.arg(2).empty() ? 0 : std::max(0, std::atoi(c.arg(2).c_str()));
  // (wonders are rare: about one per 2.5 x 2.5 regions, fewer of the desert and snow kinds near a temperate start)
  const int R = pw->type == 1 ? 14 : 8;
  std::vector<Found> fs = findPois(g, *pw, R);
  if ((int)fs.size() <= nth) { c.fail("gotopoi " + c.arg(1) + ": none within " + std::to_string(R) + " regions"); return true; }
  const Found& f = fs[(size_t)nth];
  if (g.inside) g.debugLeave();
  // a few tiles south of its heart, looking north at it (the camera centres on the player: the place fills the view)
  const int32_t ty = pw->type >= 2 ? f.y + (pw->type == 4 ? 0 : 6) : std::min(f.gy + f.h + 1, f.y + 4);
  g.teleportGlobal(f.x, ty);
  g.pl().aim = Vec2(0, -1);
  g.pl().face = 1;
  g.mode = Mode::Play;
  c.view.snap(g);
  std::printf("script: gotopoi %s -> %s at %d,%d (%.0f tiles away)\n", c.arg(1).c_str(), f.name.c_str(), f.x, f.y, f.d);
  return true;
}
EMB_SCRIPT_CMD("gotopoi", "gotopoi <kind> [n]: stand south of the n-th nearest vignette / wonder / landmark of a kind (WORLD lane, M2)", cmdGotoPoi);

bool expPoi(ScriptCtx& c) {
  Game& g = c.game;
  const PoiWord* pw = wordOf(c.arg(2));
  if (!pw) { c.fail("expect poi <kind>"); return true; }
  std::vector<Found> fs = findPois(g, *pw, 1);
  if (fs.empty() || fs[0].d > 24) c.fail("expected a " + c.arg(2) + " within 24 tiles");
  return true;
}
EMB_SCRIPT_CMD("expect:poi", "expect poi <kind>: a vignette / wonder / landmark of that kind within 24 tiles (WORLD lane, M2)", expPoi);

}  // namespace
