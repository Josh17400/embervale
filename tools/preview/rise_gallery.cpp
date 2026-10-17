// rise_gallery (M12 "Rise and Fall", VIEW lane): the rise art for review at 1x and zoomed.
//
//   rise_gallery <outdir>     writes (each at 4x and as _1x.png)
//     rise_props      Prop::LordBanner (every frame: the plain linen and three owners' arms), Prop::Watchtower, next to a
//                     village house for scale; the warlord's standard (every frame), the trophy pikes, the resettlers'
//                     stores, the boss's claw scars
//     rise_badges     the rank badges (recruit, sergeant, captain, marshal) at the HUD's, the journal's and the banner's
//                     sizes in three armies' colours
//     rise_people     the steward, the warlord and the envoy (several seeds, every facing), and the enlisted hero in the
//                     sworn realm's tabard with each rank's mark (sergeant's chevrons, captain's sash, marshal's cloak)
#include "tools/preview/preview_util.h"
#include <cstdio>
#include <string>
#include <vector>
#include "rpg/art/art_culture.h"
#include "rpg/art/art_rise.h"
#include "rpg/build/blueprint.h"
#include "rpg/culture/culture.h"
#include "rpg/culture/society.h"
#include "rpg/sim/world.h"

// rpg/sim/looks.cpp (the census): dress a person in a culture
namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner,
           uint64_t seed, const Rank& rank);
}  // namespace census

using art::Prop;

namespace {

struct Arms { uint32_t field, field2, charge; uint8_t division, emblem; const char* name; };
const Arms kArms[] = {{rgba(168, 36, 40), 0, rgba(236, 196, 92), 0, 0, "RED/GOLD"},
                      {rgba(46, 78, 156), rgba(232, 232, 220), rgba(232, 232, 220), 1, 7, "BLUE/WHITE"},
                      {rgba(40, 104, 64), 0, rgba(30, 30, 36), 7, 4, "GREEN/BLACK"}};

cult::Heraldry heraldryOf(const Arms& a) {
  cult::Heraldry h;
  h.field = a.field; h.field2 = a.field2; h.charge = a.charge; h.division = a.division; h.emblem = a.emblem; h.chargeKind = 0;
  return h;
}
void saveBoth(const Board& b, const std::string& dir, const char* name) {
  savePng(b.c, dir + "/" + name + ".png", 4);
  savePng(b.c, dir + "/" + name + "_1x.png", 1);
}
Canvas frameOf(const Canvas& sheet, int fw, int f) {
  Canvas s(fw, sheet.h);
  for (int y = 0; y < sheet.h; y++) for (int x = 0; x < fw; x++) s.set(x, y, sheet.get(f * fw + x, y));
  return s;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  // ---- 1. the props and the view's pieces
  {
    Board b(560, 300);
    const int bw = art::risePropW(Prop::LordBanner), nf = art::risePropFrames(Prop::LordBanner);
    // the banners: plain + three owners, every frame
    b.text(4, 4, "LORD BANNER: PLAIN + 3 OWNERS, 4 FRAMES");
    const Canvas plain = art::propSprite(Prop::LordBanner);
    for (int f = 0; f < nf; f++) b.put(frameOf(plain, bw, f), 6 + f * 24, 16);
    for (int a = 0; a < 3; a++) {
      const Canvas sheet = art::lordBannerSprite(heraldryOf(kArms[a]));
      for (int f = 0; f < nf; f++) b.put(frameOf(sheet, bw, f), 6 + (a + 1) * 104 + f * 24, 16);
    }
    // the watchtower beside a village house (scale) and a banner before it
    b.text(4, 74, "WATCHTOWER, A HOUSE FOR SCALE, A BANNER");
    const cult::Culture K = cult::Atlas::make(cult::Archetype::Heartland, 2024u, 2);
    art::BuildingFacts bf;
    const Canvas house = art::buildingSprite(bld::design(bld::simpleRequest(art::Building::House, 4, 3, cult::buildingArch(K, 2, 1, 1, 500u), 500u, bf)));
    b.put(house, 8, 84 + 80 - house.h);
    const Canvas tower = art::propSprite(Prop::Watchtower);
    b.put(tower, 90, 84 + 80 - tower.h);
    b.put(frameOf(art::lordBannerSprite(heraldryOf(kArms[0])), bw, 1), 128, 84 + 80 - 48);
    b.put(tower, 160, 84 + 80 - tower.h);
    // the warlord's standard (every frame), the trophy pikes, the stores, the claw scars
    b.text(204, 74, "WARLORD STANDARD, TROPHIES, STORES");
    const Canvas ws = art::warlordStandard(rgba(110, 30, 34), rgba(30, 26, 30));
    for (int f = 0; f < 4; f++) b.put(frameOf(ws, art::kWarlordStdW, f), 204 + f * 28, 84 + 80 - art::kWarlordStdH);
    const Canvas ws2 = art::warlordStandard(rgba(60, 64, 40), rgba(200, 180, 120));
    b.put(frameOf(ws2, art::kWarlordStdW, 0), 320, 84 + 80 - art::kWarlordStdH);
    for (int v = 0; v < 4; v++) b.put(art::trophyPike(v, rgba(110, 30, 34)), 352 + v * 16, 84 + 80 - art::kTrophyH);
    for (int v = 0; v < 3; v++) b.put(art::resettlerStores(v), 422 + v * 32 - (v == 2 ? 0 : 0), 84 + 80 - art::kStoresH);
    b.text(4, 176, "CLAW SCARS");
    for (int v = 0; v < 4; v++) b.put(art::clawScar(v), 8 + v * 46, 190);
    saveBoth(b, dir, "rise_props");
  }
  // ---- 2. the rank badges
  {
    Board b(300, 150);
    const int sizes[3] = {11, 16, 22};
    for (int a = 0; a < 3; a++) {
      b.text(4, 6 + a * 46, kArms[a].name);
      for (int s = 0; s < 3; s++)
        for (int r = 1; r <= 4; r++) {
          const Canvas c = art::rankBadge(r, kArms[a].field, kArms[a].charge, sizes[s]);
          b.put(c, 80 + s * 72 + (r - 1) * (sizes[s] + 3) - (s == 2 ? 0 : 0), 18 + a * 46 - c.h / 2 + 12);
        }
    }
    saveBoth(b, dir, "rise_badges");
  }
  // ---- 3. the people: the steward, the warlord, the envoy in four cultures (down, side, up, a walk frame), then the
  //      enlisted hero in the sworn realm's tabard at each rank
  {
    const cult::Archetype arcs[4] = {cult::Archetype::Fjordfolk, cult::Archetype::Heartland, cult::Archetype::Dune, cult::Archetype::Jade};
    const Role roles[3] = {Role::Steward, Role::Warlord, Role::Envoy};
    const char* rn[3] = {"STEWARD", "WARLORD", "ENVOY"};
    const int cw = 4 * 18 + 8;
    Board b(70 + 3 * cw * 2 + 10, 20 + 4 * 34 + 70);
    auto put4 = [&](const art::HumanLook& L, int x, int y) {
      const Canvas sh = art::humanSheet(L);
      const int rows[4] = {0, 2, 1, 0}, frames[4] = {0, 0, 0, 2};
      for (int i = 0; i < 4; i++) {
        Canvas c(art::HUMAN_W, art::HUMAN_H);
        for (int yy = 0; yy < c.h; yy++) for (int xx = 0; xx < c.w; xx++) c.set(xx, yy, sh.get(frames[i] * art::HUMAN_W + xx, rows[i] * art::HUMAN_H + yy));
        b.put(c, x + i * 18, y);
      }
    };
    for (int r = 0; r < 3; r++) b.text(70 + r * cw * 2 + 10, 4, rn[r]);
    for (int a = 0; a < 4; a++) {
      const cult::Culture C = cult::Atlas::make(arcs[a], 1234u + (uint32_t)a * 7919u);
      b.text(2, 22 + a * 34 + 10, cult::archetypeName(arcs[a]));
      for (int r = 0; r < 3; r++)
        for (int sex = 0; sex < 2; sex++) {
          art::HumanLook L;
          std::string name;
          census::Rank rk;
          census::dress(L, name, roles[r], sex == 1, C, C, 900u + (uint64_t)a * 31 + (uint64_t)r * 7 + (uint64_t)sex, rk);
          if (roles[r] == Role::Envoy) { L.topColor = kArms[a % 3].field; L.headColor = kArms[a % 3].charge; }
          if (roles[r] == Role::Warlord) { L.tabardColor = kArms[a % 3].field; L.plumeColor = kArms[a % 3].charge; }
          put4(L, 70 + (r * 2 + sex) * cw, 18 + a * 34);
        }
    }
    // the enlisted hero: iron harness, a helm, a sword and a heater shield, in three armies' tabards x rank 1..4
    const int hy0 = 22 + 4 * 34 + 4;
    b.text(2, hy0 + 10, "SWORN HERO");
    for (int rank = 1; rank <= 4; rank++) {
      art::HumanLook L;
      L.armorStyle = 2; L.helmStyle = 2; L.helmet = true; L.weapon = 1; L.shield = true; L.shieldStyle = 2; L.gloves = 1; L.boots = 2;
      L.beard = true;
      const Arms& A = kArms[(rank - 1) % 3];
      L.tabardColor = A.field; L.patternColor = A.charge;
      L.armsOrnament = (uint16_t)(art::RISE_TABARD | art::riseRankBits(rank));
      if (rank == 4) { L.cloak = 3; L.cloakColor = A.field; }
      put4(L, 70 + (rank - 1) * cw * 3 / 2, hy0);
    }
    saveBoth(b, dir, "rise_people");
  }
  return 0;
}
