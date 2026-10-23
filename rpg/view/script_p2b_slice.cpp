// P2b style slice test commands (rpg/view/script_p2b_slice.cpp; FOLK-BODY, wave 1; VISION_PLAN 15.36 "The slice gate", STYLE_P2 19). Test only: they set
// the doll's look fields directly (FOLK-SIM fills them from the census and the Appearance in wave 1) and stage the owner
// pack's lineups. Nothing here is saved; a recalc of the hero's look (gear, appearance) undoes `slicelook`.
//   slicelook <preset|field=value ...>   the hero's look: presets a3 (A3's hero: tousled copper hair, the scarf, the
//                                        sword at the hip), a3woman (the woman hero: wheat braid, the shirt and trousers,
//                                        the sword); fields head nose brow marks hairfamily hairvar beard beardstyle neck
//                                        garb age people build outfit cut headwear hood cloak cape bodyform helmform weapon civil
//                                        (1: a civil trade, its tool at rest: the elder with the stick, civil=1 weapon=9), and the
//                                        colours skin hair top bottom accent eyes (hex RRGGBB)
//   slicelineup variety|armour|elders|swaps|clear   a row of the slice's people on clear ground before the hero, facing
//                                        the camera (ids from 930000; a new row replaces the last): variety = 12 people of
//                                        three peoples' ramps (heads, noses, brows, ears, hair, beards, garments); armour =
//                                        mail and the nasal helm on a man and a woman, the hooded woman, the A3 pair;
//                                        elders = the elder man beside an adult; swaps = A3's four villager palettes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "rpg/art.h"
#include "rpg/culture/culture.h"
#include "rpg/sim/game.h"
#include "rpg/view/script_api.h"

namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

namespace {

uint32_t hexc(uint32_t rgb) { return rgba((int)(rgb >> 16 & 255), (int)(rgb >> 8 & 255), (int)(rgb & 255)); }
uint32_t parseHex(const std::string& s) { return hexc((uint32_t)std::strtoul(s.c_str(), nullptr, 16)); }

// the slice's covered values only: everything else a census look may carry is cleared
void plain(art::HumanLook& L) {
  L.jewellery = 0; L.amulet = false; L.facePaint = 0; L.cloak = 0; L.cape = false; L.headwear = 0; L.hood = false;
  L.shield = false; L.shieldStyle = 0; L.shieldForm = 0; L.helmet = false; L.helmStyle = 0; L.helmForm = 0; L.armorStyle = 0;
  L.bodyForm = 0; L.pauldron = 0; L.cut = 0; L.garb = 0; L.neck = 0; L.marks = 0; L.weapon = 0; L.pattern = 0; L.ring = false;
  L.gloves = 0; L.boots = 0; L.skirt = 0; L.crest = 0; L.bladeForm = 0; L.polearmForm = 0; L.bowForm = 0; L.civil = 0;
}

void a3Hero(art::HumanLook& L) {
  plain(L);
  L.body = 0; L.age = 0; L.people = 0; L.build = 0; L.head = 0; L.nose = 0; L.brow = 0; L.outfit = art::Outfit::Tunic;
  L.hair = art::Hair::Short; L.hairVar = 2; L.beard = false; L.neck = 1; L.weapon = 1;
  L.skin = hexc(0xE9A077); L.hairColor = hexc(0xAA4A2C); L.topColor = hexc(0x28728C); L.bottomColor = hexc(0x4F4B6B);
  L.tabardColor = hexc(0xC83E38); L.eyeColor = hexc(0x3A78B0);
}
void a3Woman(art::HumanLook& L, bool hero) {
  plain(L);
  L.body = 1; L.age = 0; L.people = 0; L.build = 0; L.head = 0; L.nose = 0; L.brow = 0;
  L.outfit = hero ? art::Outfit::Tunic : art::Outfit::Dress;
  L.hair = art::Hair::Braids; L.hairVar = 0; L.beard = false; L.weapon = hero ? 1 : 0;
  L.skin = hexc(0xEEAA86); L.hairColor = hexc(0xC79346); L.topColor = hero ? hexc(0x28728C) : hexc(0x922F60);
  L.bottomColor = hexc(0x4F4B6B); L.tabardColor = hero ? hexc(0xC83E38) : hexc(0x28728C); L.eyeColor = hexc(0x3F8A5A);
}

bool setField(art::HumanLook& L, const std::string& k, const std::string& v) {
  const int n = std::atoi(v.c_str());
  if (k == "head") L.head = (uint8_t)n;
  else if (k == "nose") L.nose = (uint8_t)n;
  else if (k == "brow") L.brow = (uint8_t)n;
  else if (k == "marks") L.marks = (uint8_t)n;
  else if (k == "hairfamily") L.hair = (art::Hair)std::clamp(n, 0, (int)art::Hair::COUNT - 1);
  else if (k == "hairvar") L.hairVar = (uint8_t)n;
  else if (k == "beard") L.beard = n != 0;
  else if (k == "beardstyle") L.beardStyle = (uint8_t)n;
  else if (k == "neck") L.neck = (uint8_t)n;
  else if (k == "garb") L.garb = (uint8_t)n;
  else if (k == "age") L.age = (uint8_t)n;
  else if (k == "people") L.people = (uint8_t)n;
  else if (k == "build") L.build = (uint8_t)n;
  else if (k == "body") L.body = (uint8_t)n;
  else if (k == "outfit") L.outfit = (art::Outfit)n;
  else if (k == "cut") L.cut = (uint8_t)n;
  else if (k == "headwear") L.headwear = (uint8_t)n;
  else if (k == "hood") L.hood = n != 0;
  else if (k == "cloak") L.cloak = (uint8_t)n;   // (OUTER, P2b wave 2: the cloaks)
  else if (k == "cape") L.cape = n != 0;
  else if (k == "bodyform") L.bodyForm = (uint8_t)n;
  else if (k == "helmform") { L.helmForm = (uint8_t)n; L.helmet = n != 0; }
  else if (k == "weapon") L.weapon = (uint8_t)n;
  else if (k == "civil") L.civil = (uint8_t)(n != 0);   // (MOTION_P2 10) a civil trade: its tool at rest (the elder's stick)
  else if (k == "skin") L.skin = parseHex(v);
  else if (k == "hair") L.hairColor = parseHex(v);
  else if (k == "top") L.topColor = parseHex(v);
  else if (k == "bottom") L.bottomColor = parseHex(v);
  else if (k == "accent") L.tabardColor = parseHex(v);
  else if (k == "eyes") L.eyeColor = parseHex(v);
  else return false;
  return true;
}

bool cmdSliceLook(ScriptCtx& c) {
  art::HumanLook& L = c.game.pl().look;
  for (size_t i = 1; i < c.a.size(); i++) {
    const std::string w = c.arg(i);
    if (w == "a3") { a3Hero(L); continue; }
    if (w == "a3woman") { a3Woman(L, true); continue; }
    const size_t eq = w.find('=');
    if (eq == std::string::npos || !setField(L, w.substr(0, eq), w.substr(eq + 1))) { c.fail("slicelook: unknown '" + w + "'"); return true; }
  }
  return true;
}
EMB_SCRIPT_CMD("slicelook", "slicelook <a3|a3woman|field=value ...>: the hero's doll look fields (P2b slice, test only)", cmdSliceLook);

struct Person { int arch; bool female; int role; const char* fields; };

// the owner pack's rows (STYLE_P2 19 item 4): every value here is one the slice draws
const Person kVariety[] = {
    {2, false, 0, "head=0 nose=0 brow=0 hairfamily=1 hairvar=2"},                 // Heartland: Oval, tousled
    {2, true, 0, "head=0 nose=0 brow=0 hairfamily=5 hairvar=0"},                  // Oval, braid, the dress
    {2, false, 0, "head=1 nose=1 brow=1 hairfamily=1 hairvar=1 beard=1 beardstyle=0"},   // Square, cropped, full beard
    {2, true, 0, "head=1 nose=2 brow=2 hairfamily=2 hairvar=0 outfit=0"},         // Heart, upturned, thin, long loose, shirt and trousers
    {1, false, 0, "head=3 nose=3 brow=2 hairfamily=2 hairvar=0 outfit=2 beard=1 beardstyle=0"},   // Highland: Long, long loose, full beard, the robe
    {1, true, 0, "head=2 nose=3 brow=3 hairfamily=5 hairvar=1 people=1"},         // Round, twin braids, half-breed
    {1, false, 0, "head=1 nose=0 brow=2 hairfamily=1 hairvar=2 beard=1 beardstyle=6"},   // Square, tousled, moustache
    {1, true, 0, "head=0 nose=3 brow=3 hairfamily=6 hairvar=0 people=2"},         // Oval, aquiline, full, bun, elf
    {4, false, 0, "head=3 nose=1 brow=0 hairfamily=1 hairvar=1 people=2"},        // Dune: Long, straight, cropped, elf
    {4, true, 0, "head=1 nose=0 brow=0 hairfamily=5 hairvar=0 outfit=0"},         // Heart, braid, shirt and trousers
    {4, false, 0, "head=0 nose=3 brow=1 hairfamily=2 hairvar=0 people=1 beard=1 beardstyle=6"},   // Oval, long loose, moustache
    {4, true, 0, "head=2 nose=2 brow=3 hairfamily=5 hairvar=1"},                  // Round, twin braids
};
const Person kArmour[] = {
    {-1, false, 0, "a3"}, {-1, false, 0, "bodyform=3 helmform=1 weapon=1 head=1 hairfamily=1 hairvar=1"},
    {-1, true, 0, "bodyform=3 helmform=1 weapon=1 outfit=0 hairfamily=5 hairvar=0"}, {-1, true, 0, "hood=1 hairfamily=5 hairvar=0"},
    {-1, true, 0, "a3woman"},
};

void spawnRow(Game& g, const Person* ps, int n, const std::string& tag, bool stage = true) {
  g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return a.id >= 930000 && a.id < 940000; }), g.actors.end());
  const float pitch = 22.0f, rowW = pitch * (float)(n - 1);
  Vec2 centre = g.pl().p + Vec2(0, -2.0f * TILE);
  if (!g.inside && stage) {
    const Map& m = g.map();
    const int px = (int)std::floor(g.pl().p.x / TILE), py = (int)std::floor(g.pl().p.y / TILE);
    const int half = (int)std::ceil(rowW * 0.5f / TILE) + 1;
    bool found = false;
    for (int r = 0; r <= 90 && !found; r++)
      for (int dy = -r; dy <= r && !found; dy++)
        for (int dx = -r; dx <= r && !found; dx++) {
          if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
          const int cx = px + dx, cy = py + dy;
          bool clear = true;
          for (int y = cy - 3; y <= cy + 3 && clear; y++)
            for (int x = cx - half; x <= cx + half && clear; x++) {
              if (!m.in(x, y) || m.blocked(x, y)) { clear = false; break; }
              if (m.propAt(x, y)) clear = false;   // (stalls, carts, crates: the row stands on open ground)
            }
          if (clear) { found = true; centre = Vec2(cx * TILE + 8.0f, cy * TILE + 10.0f); }
        }
    g.pl().p = centre + Vec2(0, 2.0f * TILE);
  }
  const Vec2 o = centre - Vec2(rowW * 0.5f, 0);
  for (int k = 0; k < n; k++) {
    const Person& P = ps[k];
    Actor a;
    a.id = 930000 + k;
    a.human = true; a.npc = true; a.role = (Role)P.role; a.hostile = false;
    a.p = o + Vec2((float)k * pitch, 0);
    a.home = a.p; a.goal = a.p; a.face = 0; a.speed = 0; a.special = 0.01f;
    a.thinkT = 1.0e9f;   // (no stroll picked: the row keeps facing the camera, its goal its own spot)
    if (P.arch >= 0) {
      const cult::Culture C = cult::Atlas::make((cult::Archetype)P.arch, 4242u + (uint32_t)P.arch * 97u);
      census::dress(a.look, a.name, (Role)P.role, P.female, C, C, 0x5E1CEull * (uint64_t)(k + 1) ^ (uint64_t)P.arch << 32, census::Rank{});
      a.name = std::string(cult::archetypeName(C.archetype)) + " " + tag;
    } else {
      a.name = "slice " + tag;
      a3Hero(a.look);
      if (P.female) a3Woman(a.look, false);
    }
    const art::Outfit keep = P.female ? art::Outfit::Dress : art::Outfit::Tunic;
    plain(a.look);
    a.look.body = P.female ? 1 : 0;
    a.look.outfit = keep;
    a.look.beard = false;
    a.look.age = 0;
    a.look.people = 0;
    if (P.female) a.look.outfit = art::Outfit::Dress;
    std::string f = P.fields;
    size_t s = 0;
    while (s < f.size()) {
      size_t e = f.find(' ', s);
      if (e == std::string::npos) e = f.size();
      const std::string w = f.substr(s, e - s);
      if (w == "a3") a3Hero(a.look);
      else if (w == "a3woman") a3Woman(a.look, false);
      else if (!w.empty()) { const size_t eq = w.find('='); if (eq != std::string::npos) setField(a.look, w.substr(0, eq), w.substr(eq + 1)); }
      s = e + 1;
    }
    g.actors.push_back(a);
  }
}

bool cmdSliceLineup(ScriptCtx& c) {
  Game& g = c.game;
  const std::string w = c.arg(1);
  if (w == "variety") spawnRow(g, kVariety, (int)(sizeof kVariety / sizeof kVariety[0]), "folk");
  else if (w == "armour") spawnRow(g, kArmour, (int)(sizeof kArmour / sizeof kArmour[0]), "armour");
  else if (w == "elders") {
    // (every face value named: the census's own picks may be ones the slice does not draw; the girl in the child's tunic)
    static const Person e[] = {{1, false, 0, "head=0 nose=1 brow=1 hairfamily=1 hairvar=4 age=3 marks=16 hair=C8C4BC"},
                               {1, false, 0, "head=0 nose=0 brow=0 hairfamily=1 hairvar=1"},
                               {2, false, 8, "age=1 head=0 nose=0 brow=0 hairfamily=1 hairvar=0"},
                               {2, true, 8, "age=1 head=0 nose=0 brow=0 hairfamily=1 hairvar=2 outfit=0"},
                               {2, true, 0, "head=1 nose=2 brow=2 hairfamily=6 hairvar=0 age=3 marks=16 hair=D8D4CC"},
                               {2, false, 0, "head=3 nose=3 brow=1 hairfamily=1 hairvar=4 age=3 marks=16 beard=1 beardstyle=0 hair=E8E4DC"}};
    spawnRow(g, e, 6, "elder");
  } else if (w == "swaps") {
    static const Person sw[] = {{-1, false, 0, "a3 skin=B07A5A hair=1E1A20 top=A04A2E"}, {-1, false, 0, "a3 skin=C88A64 hair=7A3418 top=5E7A3A"},
                                {-1, true, 0, "a3woman skin=8A5A42 hair=2A2024 top=3A5A9A"}, {-1, true, 0, "a3woman skin=E8B090 hair=8A3A1E top=C89A3A"}};
    spawnRow(g, sw, 4, "swap");
  } else if (w == "scene") {
    // A3's scene (docs/ref/style_a3/scene_phone_zoom.png) where the hero stands: the A3 woman a step east facing him, a
    // villager (red bun, the mustard dress) at the stall behind her, facing away; the hero keeps his spot
    static const Person sc[] = {{-1, true, 0, "a3woman"}, {2, true, 0, "head=2 nose=0 brow=3 hairfamily=6 hairvar=0 hair=B4442A top=B88A2A"}};
    spawnRow(g, sc, 2, "scene", false);
    const Vec2 h = g.pl().p;
    const Vec2 at[2] = {h + Vec2(20.0f, 0.0f), h + Vec2(54.0f, -26.0f)};
    const uint8_t face[2] = {3, 1};
    for (Actor& a : g.actors)
      if (a.id >= 930000 && a.id < 930002) {
        const int k = a.id - 930000;
        a.p = a.home = a.goal = at[k]; a.face = face[k];
        a.name = k == 0 ? "Elin" : "Marta";   // (plain names: the owner pack's in-world shots show the talk plate)
      }
  } else if (w == "face") {   // the row turned to one facing (0 down, 1 up, 2 right, 3 left) for the owner pack's sheets
    const int f = std::clamp(std::atoi(c.arg(2).c_str()), 0, 3);
    for (Actor& a : g.actors) if (a.id >= 930000 && a.id < 940000) a.face = (uint8_t)f;
  } else if (w == "clear") {
    g.actors.erase(std::remove_if(g.actors.begin() + 1, g.actors.end(), [](const Actor& a) { return a.id >= 930000 && a.id < 940000; }), g.actors.end());
  } else c.fail("slicelineup variety|armour|elders|swaps|scene|face <0-3>|clear");
  return true;
}
EMB_SCRIPT_CMD("slicelineup", "slicelineup variety|armour|elders|swaps|scene|clear: the P2b slice's owner-pack rows before the hero; scene: A3's scene where he stands; face <0-3>: turn the row (test only)", cmdSliceLineup);

}  // namespace
