// war_gallery (M4 "Banners", WARDS lane): the war made visible, for review at 1x and zoomed.
//
//   war_gallery <outdir>      writes
//     war_props.png / _1x.png      every war prop (WarTent .. Barricade) in three kingdom palettes and the neutral look
//     war_camp.png / _1x.png       a siege camp and a refugee camp laid out as the overlays lay them (palisade runs with
//                                  diagonal steps and corners, tents, the catapult, rubble, ash, scaffolds, a checkpoint)
//     war_charred.png / _1x.png    buildings of seven cultures, whole / scorched / burned out / rebuilding (charred 0..3),
//                                  each burned-out one also at night (no lit glass), and the garrison tower each culture
//                                  builds when it holds a conquered town
#include "tools/preview/preview_util.h"
#include <cstdio>
#include <string>
#include <vector>
#include "rpg/build/blueprint.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"

using art::Prop;

namespace {

struct Pal { uint32_t field, trim; const char* name; };
const Pal kPals[] = {{0, 0, "NEUTRAL"}, {rgba(168, 36, 40), rgba(236, 196, 92), "RED/GOLD"}, {rgba(46, 78, 156), rgba(232, 232, 220), "BLUE/WHITE"},
                     {rgba(40, 104, 64), rgba(30, 30, 36), "GREEN/BLACK"}};

void saveBoth(const Board& b, const std::string& dir, const char* name) {
  savePng(b.c, dir + "/" + name + ".png", 4);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}

// a prop placed as the renderer places it: bottom-centred on its anchor tile (tile top-left at tx, ty in px); a sheet
// of frames shows its first frame
void placeProp(Board& b, const Canvas& sheet, int tileX, int tileY, int frameW = 0) {
  Canvas s = sheet;
  if (frameW > 0 && frameW < sheet.w) {
    s = Canvas(frameW, sheet.h);
    for (int y = 0; y < sheet.h; y++) for (int x = 0; x < frameW; x++) s.set(x, y, sheet.get(x, y));
  }
  b.put(s, tileX + 8 - s.w / 2, tileY + 16 - s.h);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  // ---- 1. every prop in every palette
  {
    const int cols = 9, cw = 72, rh = 86;
    Board b(cols * cw + 90, (int)(sizeof(kPals) / sizeof(kPals[0])) * rh + 20);
    for (int r = 0; r < (int)(sizeof(kPals) / sizeof(kPals[0])); r++) {
      b.text(4, 14 + r * rh + 30, kPals[r].name);
      for (int k = 0; k < cols; k++) {
        const Prop p = (Prop)((int)Prop::WarTent + k);
        const Canvas s = art::warPropSprite(p, kPals[r].field, kPals[r].trim);
        const int x = 90 + k * cw + (cw - s.w) / 2, y = 14 + r * rh + (rh - 8 - s.h);
        b.put(s, x, y);
      }
    }
    saveBoth(b, dir, "war_props");
  }
  // ---- 2. the camps as the overlays lay them (a tile grid of 16 px)
  {
    const int TW = 44, TH = 26;
    Board b(TW * 16, TH * 16);
    std::vector<std::pair<std::pair<int, int>, Prop>> props;
    auto put = [&](int tx, int ty, Prop p) { props.push_back({{tx, ty}, p}); };
    // the siege camp: the command tent behind the fire, tents round it, the catapult in front, a palisade arc with
    // diagonal steps (the corner tile of each step filled, as the overlay does) and gaps
    const int cx = 12, cy = 12;
    put(cx, cy, Prop::Campfire);
    put(cx, cy - 4, Prop::CommandTent);
    put(cx - 5, cy - 1, Prop::WarTent); put(cx + 5, cy - 1, Prop::WarTent); put(cx - 5, cy + 4, Prop::WarTent); put(cx + 5, cy + 4, Prop::WarTent);
    put(cx, cy + 5, Prop::Catapult);
    put(cx - 1, cy + 2, Prop::Crate); put(cx + 1, cy + 2, Prop::Barrel);
    {
      int px = -1000, py = -1000, run = 0;
      for (float a = -1.25f; a <= 1.25f; a += 0.04f) {
        const int x = cx + (int)std::lround(std::sin(a) * 9.5f), y = cy + (int)std::lround(std::cos(a) * 8.5f);
        if (x == px && y == py) continue;
        auto stake = [&](int qx, int qy) { run++; if (run % 7 == 0) return; put(qx, qy, Prop::Palisade); };
        if (px != -1000 && x != px && y != py) stake(x, py);
        stake(x, y);
        px = x; py = y;
      }
    }
    // the refugee camp
    const int rx = 32, ry = 9;
    put(rx, ry, Prop::Campfire);
    put(rx - 4, ry - 1, Prop::RefugeeTent); put(rx + 4, ry - 1, Prop::RefugeeTent); put(rx - 1, ry + 4, Prop::RefugeeTent);
    put(rx + 2, ry - 1, Prop::Sacks); put(rx + 1, ry + 2, Prop::Bedroll);
    // a burned lot: rubble, ash, a scaffold; a checkpoint barricade by a road
    put(30, 18, Prop::Rubble); put(31, 19, Prop::Ash); put(28, 19, Prop::Ash); put(29, 20, Prop::Ash); put(34, 18, Prop::Scaffold);
    put(38, 21, Prop::Barricade); put(40, 21, Prop::Barricade);
    std::stable_sort(props.begin(), props.end(), [](const auto& a, const auto& b2) { return a.first.second < b2.first.second; });
    for (int y = 0; y < TH; y++)   // a dirt road under the checkpoint
      for (int x = 36; x < 43; x++) if (y >= 22 && y <= 23) for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) b.c.set(x * 16 + i, y * 16 + j, rgba(150, 120, 84));
    // flat ones first (ash, bedrolls), then by row
    for (auto& e : props)
      if (e.second == Prop::Ash || e.second == Prop::Bedroll) placeProp(b, art::propSprite(e.second), e.first.first * 16, e.first.second * 16, art::propW(e.second));
    for (auto& e : props) {
      if (e.second == Prop::Ash || e.second == Prop::Bedroll) continue;
      const bool war = art::isWarProp(e.second);
      const bool refuge = e.first.first > 26;
      const Canvas s = war ? art::warPropSprite(e.second, refuge ? 0 : kPals[1].field, refuge ? 0 : kPals[1].trim) : art::propSprite(e.second);
      placeProp(b, s, e.first.first * 16, e.first.second * 16, war ? 0 : art::propW(e.second));
    }
    saveBoth(b, dir, "war_camp");
  }
  // ---- 3. charred buildings of seven cultures, and each culture's garrison tower
  {
    const cult::Archetype arcs[] = {cult::Archetype::Fjordfolk, cult::Archetype::Heartland, cult::Archetype::Imperial, cult::Archetype::Dune,
                                    cult::Archetype::Steppe, cult::Archetype::Jade, cult::Archetype::Sylvan};
    const int nA = (int)(sizeof(arcs) / sizeof(arcs[0]));
    struct Kind { art::Building t; int w, h; };
    const Kind kinds[] = {{art::Building::House, 4, 3}, {art::Building::Inn, 6, 4}};
    const int cellW = 116, cellH = 150;
    Board b(60 + (2 * 5) * cellW + cellW, nA * cellH + 30);
    for (int k = 0; k < 4; k++) b.text(60 + k * cellW + 30, 4, k == 0 ? "WHOLE" : k == 1 ? "SCORCHED" : k == 2 ? "BURNED" : "REBUILD");
    b.text(60 + 4 * cellW + 30, 4, "NIGHT");
    b.text(60 + 10 * cellW + 20, 4, "GARRISON");
    for (int a = 0; a < nA; a++) {
      const cult::Culture K = cult::Atlas::make(arcs[a], 2024u + (uint32_t)a * 7u, 2);
      b.text(4, 20 + a * cellH + 50, cult::archetypeName(arcs[a]));
      for (int ki = 0; ki < 2; ki++) {
        for (int ch = 0; ch <= 4; ch++) {
          art::BuildingFacts f;
          f.storeys = ki == 1 ? 2 : 1;
          f.charred = ch == 4 ? 2 : ch;
          const uint32_t seed = 500u + (uint32_t)a * 31u + (uint32_t)ki * 7u;
          bld::Request r = bld::simpleRequest(kinds[ki].t, kinds[ki].w, kinds[ki].h, cult::buildingArch(K, 2, 1, 1, seed), seed, f);
          const bld::Blueprint bp = bld::design(r);
          art::BuildingInfo info;
          Canvas s = art::buildingSprite(bp, &info);
          if (ch == 4) {   // the night look (only lit glass changes): a burned-out shell shows none
            s = art::buildingNight(s, info.glass, seed);
            for (auto& px : s.px) if (px >> 24) px = art::mix(px, rgba(30, 30, 70), 0.45f);
          }
          const int x = 60 + (ki * 5 + ch) * cellW + (cellW - s.w) / 2, y = 20 + a * cellH + (cellH - 6 - s.h);
          b.put(s, x, y);
        }
      }
      // the garrison tower this culture raises in a conquered town (a Barracks of Form::Tower, 3 x 3, three storeys)
      art::BuildingFacts f;
      f.storeys = 3; f.hearth = false; f.banner = kPals[2].field; f.banner2 = kPals[2].trim;
      bld::Request r = bld::simpleRequest(art::Building::Barracks, 3, 3, cult::buildingArch(K, 2, 1, 2, 4242u + (uint32_t)a), 4243u + (uint32_t)a, f);
      r.wealth = 2; r.urban = 1; r.form = bld::Form::Tower;
      const Canvas s = art::buildingSprite(bld::design(r));
      b.put(s, 60 + 10 * cellW + (cellW - s.w) / 2, 20 + a * cellH + (cellH - 6 - s.h));
    }
    saveBoth(b, dir, "war_charred");
  }
  return 0;
}
