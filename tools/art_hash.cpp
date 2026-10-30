// EMBERVALE art hash: prints one FNV-1a hash per art category (and a total) over a fixed, deterministic set of
// sprites. Used to prove a refactor is pixel-identical, and to see which categories a change touched.
//   art_hash            -> per-category lines + "TOTAL <hash>"
//   art_hash -v         -> one line per sprite
//   art_hash --dump DIR -> also writes every sprite as DIR/<category>/<name>.png (1x; '/' in a name becomes '_')
//   art_hash --rig=1|2  -> the human rig (EMB_RIG=1|2 as the game reads it; the flag wins). (P2c flip, D100-D102)
// No SDL window is opened; only the CPU painters run.
// (P2c flip, 2026-10-30, D102) At rig 2, THE DEFAULT, `doll` takes `humans`' place in TOTAL: every atlas the game bakes
// for each line of tools/doll/hash_looks.txt (LOCO, RUN, DRAWN, ACTION, DEATH through humanRig, each with its glow plane,
// then the POSTURE set through humanPosture2 where the doll covers it), in file order; `humans+new` is not printed. At
// rig 1 (the opt-in v1 fallback until the owner's retirement OK) every line prints as before the flip.
// Rig 1 (b9e0108, kept behind --rig=1): humans bec3b12c01605e67 monsters 607e3543a29be1ec props b5397faadf627d55
//   buildings 2aabcae2a6bd5417 walls a0f74be3c081af13 icons 246f135187536eb5 fx e0ecd0fcb344c159 TOTAL 317eef6efe4d8229
// Rig 2, the default (the flip's record, 2026-10-30, over tools/doll/hash_looks.txt): doll 1812 sprites d8818081069d5e68
//   monsters 607e3543a29be1ec props 8cf048d0ba49e6fe buildings cbcceb30b152b6d5 walls b317366cc39c18b7 icons 246f135187536eb5
//   fx e0ecd0fcb344c159 TOTAL 5e4a8a57bb4212f8 (monsters, icons, fx and their +new equal at both rigs; props, buildings and
//   walls carry the k-scaled holders)
// The main categories (and TOTAL) cover the sprites that existed before M0, using the pre-M0 enum sizes, so appending
// enum values never moves them; anything appended later is hashed in the "+new" lines (not part of TOTAL).
// Pre-M0 baseline: humans 3a7e04166da3888e monsters 861869e52656df48 props 8c9c5159f8c5843c buildings aca5bc7c4bef688c
//                  walls cd867c5f650fd69b icons 0fd3e91f6e4dca99 fx e0ecd0fcb344c159 TOTAL 46d7a9fe07d4f7b0
#include <algorithm>
#include <cstdint>
#include "rpg/build/blueprint.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "engine/pix.h"
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"

namespace {
bool g_verbose = false;
std::string g_dump;   // --dump DIR

void dumpPng(const Canvas& c, const std::string& cat, std::string name) {
  for (char& ch : name) if (ch == '/' || ch == ' ' || ch == ':') ch = '_';
  const std::string dir = g_dump + "/" + cat;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (c.w <= 0 || c.h <= 0) return;
  std::vector<uint32_t> px(c.px.begin(), c.px.end());
  SDL_Surface* s = SDL_CreateSurfaceFrom(c.w, c.h, SDL_PIXELFORMAT_RGBA32, px.data(), c.w * 4);
  if (!s) return;
  SDL_SavePNG(s, (dir + "/" + name + ".png").c_str());
  SDL_DestroySurface(s);
}

struct H {
  uint64_t h = 1469598103934665603ull;
  void byte(uint8_t b) { h ^= b; h *= 1099511628211ull; }
  void u32(uint32_t v) { for (int i = 0; i < 4; i++) byte((uint8_t)(v >> (i * 8))); }
  void canvas(const Canvas& c) {
    u32((uint32_t)c.w); u32((uint32_t)c.h);
    for (uint32_t p : c.px) u32(p);
  }
};

struct Cat {
  const char* name;
  H h;
  int n = 0;
  void add(const Canvas& c, const std::string& what) {
    h.canvas(c);
    n++;
    if (!g_dump.empty()) dumpPng(c, name, what);
    if (g_verbose) {
      H one; one.canvas(c);
      std::printf("  %-10s %-28s %3dx%-3d %016llx\n", name, what.c_str(), c.w, c.h, (unsigned long long)one.h);
    }
  }
};

// pre-M0 enum sizes
constexpr int kOutfits = 10, kHairs = 6, kMonsters = 16, kProps = 74, kBuildings = 10, kIcons = 35, kFx = 12;

// (P2c flip) a hash_looks.txt field: the doll's looks fields (doll::applyLookField), then the ladder fields art_hash reads
// itself (the sim's macros are not linked here; the specs are already expanded)
bool hashLookField(art::HumanLook& L, const std::string& k, const std::string& v) {
  if (art::doll::applyLookField(L, k, v)) return true;
  const long long n = std::strtoll(v.c_str(), nullptr, 0);
  auto col = [&]() {
    std::string s = v;
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    const uint32_t x = (uint32_t)std::strtoul(s.c_str(), nullptr, 16);
    return rgba((int)(x >> 16 & 255), (int)(x >> 8 & 255), (int)(x & 255));
  };
  static const char* const kTier[8] = {"tierBody", "tierHelm", "tierShield", "tierWeapon", "tierGloves", "tierBoots", "tierCloak", "tierBack"};
  static const char* const kForge[8] = {"forgeBody", "forgeHelm", "forgeShield", "forgeWeapon", "forgeGloves", "forgeBoots", "forgeCloak", "forgeBack"};
  for (int s = 0; s < 8; s++) {
    if (k == kTier[s]) { L.tier[s] = (uint8_t)(n < 0 ? 0 : n > 6 ? 6 : n); return true; }
    if (k == kForge[s]) { L.forge[s] = std::strtoull(v.c_str(), nullptr, 0); return true; }
  }
  if (k == "rare") L.rare = (decltype(L.rare))n;
  else if (k == "mythic") L.mythic = (decltype(L.mythic))n;
  else if (k == "metalKind") L.metalKind = (decltype(L.metalKind))n;
  else if (k == "bladeKind") L.bladeKind = (decltype(L.bladeKind))n;
  else if (k == "motif") L.motif = (decltype(L.motif))n;
  else if (k == "kitVar") L.kitVar = (decltype(L.kitVar))n;
  else if (k == "floor") L.floor = (decltype(L.floor))n;
  else if (k == "alloyFam") L.alloyFam = (decltype(L.alloyFam))n;
  else if (k == "gemColor") L.gemColor = col();
  else if (k == "king") L.king = (decltype(L.king))n;
  else if (k == "forgeKit") L.forgeKit = (decltype(L.forgeKit))n;
  else if (k == "alloyRank") L.alloyRank = (decltype(L.alloyRank))n;
  else if (k == "forgeGuise") L.forgeGuise = (decltype(L.forgeGuise))n;
  else if (k == "forgeShape") L.forgeShape = (decltype(L.forgeShape))std::strtoull(v.c_str(), nullptr, 0);
  else return false;
  return true;
}
struct HashLook { std::string tag; art::HumanLook L; };
// tools/doll/hash_looks.txt: `tag who spec` per line (# comments); every field must be read, or the run fails (exit 2)
bool readHashLooks(const char* path, std::vector<HashLook>& out) {
  FILE* f = std::fopen(path, "rb");
  if (!f) { std::printf("FAIL: art_hash: cannot read %s\n", path); return false; }
  char line[4096];
  bool ok = true;
  while (std::fgets(line, sizeof line, f)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
    char tag[128] = {}, who[32] = {}, spec[3800] = {};
    if (std::sscanf(line, "%127s %31s %3799s", tag, who, spec) < 3) continue;
    HashLook h;
    h.tag = tag;
    const std::string s = !std::strcmp(spec, "-") ? std::string() : std::string(spec);
    size_t a = 0;
    while (a < s.size()) {
      size_t b = s.find(',', a);
      if (b == std::string::npos) b = s.size();
      const std::string kv = s.substr(a, b - a);
      const size_t eq = kv.find('=');
      if (eq != std::string::npos) {
        const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
        if (k != "palette" && k != "bare" && !hashLookField(h.L, k, v)) { std::printf("FAIL: art_hash: %s: unknown field %s\n", tag, k.c_str()); ok = false; }
      }
      a = b + 1;
    }
    if (!std::strcmp(who, "woman")) h.L.body = 1;
    out.push_back(h);
  }
  std::fclose(f);
  if (out.empty()) { std::printf("FAIL: art_hash: no looks in %s\n", path); ok = false; }
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  // (P2c flip, D101) the rig as the game reads it: EMB_RIG, then --rig=1|2 (the flag wins). The default is 2
  if (const char* e = std::getenv("EMB_RIG")) art::setRigVersion(std::atoi(e));
  const char* looksPath = "tools/doll/hash_looks.txt";
  for (int i = 1; i < argc; i++) {
    if (!std::strcmp(argv[i], "-v")) g_verbose = true;
    else if (!std::strncmp(argv[i], "--rig=", 6)) art::setRigVersion(std::atoi(argv[i] + 6));
    else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) g_dump = argv[++i];
    else if (!std::strcmp(argv[i], "--looks") && i + 1 < argc) looksPath = argv[++i];
  }
  const bool rig2 = art::rigVersion() >= 2;
  // (P2a) --legacy-outline: the character sheets with the pre-P2a selective outline: every line must then equal the
  // pre-P2a record (docs/PLAN.md P2a), proving the ink outline is the only pixel change of phase A
  for (int i = 1; i < argc; i++) if (!std::strcmp(argv[i], "--legacy-outline")) art::outlineLegacyForTests() = true;
  using namespace art;
  Cat humans{"humans"}, monsters{"monsters"}, props{"props"}, buildings{"buildings"}, walls{"walls"}, icons{"icons"}, fx{"fx"};
  Cat humansNew{"humans+new"}, monstersNew{"monst+new"}, propsNew{"props+new"}, buildingsNew{"bldgs+new"}, iconsNew{"icons+new"},
      fxNew{"fx+new"};
  Cat doll{"doll"};

  // (P2c flip) the doll: every atlas the game bakes for each reference look, in file order
  if (rig2) {
    std::vector<HashLook> looks;
    if (!readHashLooks(looksPath, looks)) return 2;
    static const RigPart parts[5] = {RigPart::Loco, RigPart::Run, RigPart::Drawn, RigPart::Action, RigPart::Death};
    static const char* const partName[5] = {"loco", "run", "drawn", "action", "death"};
    for (const HashLook& h : looks) {
      if (!g_dump.empty()) {   // (the flip's board) the line's v1 idle cell beside the doll's idle frame, D and R, feet level
        std::vector<Canvas> cells;
        {
          const RigScope v1(1);
          cells.push_back(humanCellRaw(h.L, 0, 0));
          cells.push_back(humanCellRaw(h.L, 2, 0));
        }
        const RigSheet lo = humanRig(h.L, RigPart::Loco);
        for (int facing : {0, 2}) {
          const FrameRect* r = lo.frame(facing, Clip::Idle, 0, 0);
          Canvas c(r ? r->w : 1, r ? r->h : 1);
          if (r)
            for (int y = 0; y < r->h; y++)
              for (int x = 0; x < r->w; x++) c.px[(size_t)y * c.w + x] = lo.atlas.get(r->x + x, r->y + y);
          cells.push_back(c);
        }
        int W = 4, H = 0;
        for (const Canvas& c : cells) { W += c.w + 6; H = std::max(H, c.h); }
        Canvas board(W, H + 8);
        for (uint32_t& p : board.px) p = rgba(96, 104, 92);
        int x = 4;
        for (const Canvas& c : cells) {
          for (int y = 0; y < c.h; y++)
            for (int xx = 0; xx < c.w; xx++)
              if (c.get(xx, y) >> 24) board.px[(size_t)(4 + H - c.h + y) * board.w + x + xx] = c.get(xx, y);
          x += c.w + 6;
        }
        dumpPng(board, "pairs", h.tag);
      }
      for (int k = 0; k < 5; k++) {
        const RigSheet s = humanRig(h.L, parts[k]);
        doll.add(s.atlas, h.tag + "/" + partName[k]);
        if (!s.glow.empty()) doll.add(s.glow.atlas, h.tag + "/" + partName[k] + "_glow");
      }
      for (int p = 1; p < (int)Posture::COUNT; p++) {
        if ((Posture)p == Posture::Sleep) continue;   // the berth's (sleeper2), not a posture atlas
        const int variants = (Posture)p == Posture::Carry ? 3 : (Posture)p == Posture::Kneel ? 2 : 1;
        for (int v = 0; v < variants; v++) {
          const std::string nm = h.tag + "/posture" + std::to_string(p) + "v" + std::to_string(v);
          if (!doll::dollCoversPosture(h.L, (Posture)p, (uint8_t)v)) { doll.add(Canvas(), nm + "_none"); continue; }
          doll.add(humanPosture2(h.L, (Posture)p, (uint8_t)v).atlas, nm);
        }
      }
    }
  }

  // humans: every outfit x a few hair/flag/weapon combinations
  if (!rig2) {
  for (int o = 0; o < kOutfits; o++)
    for (int v = 0; v < 4; v++) {
      HumanLook L;
      L.outfit = (Outfit)o;
      L.hair = (Hair)((o + v) % kHairs);
      L.beard = v & 1;
      L.helmet = v == 2;
      L.hood = v == 3 && o == (int)Outfit::Robe;
      L.cape = v == 1;
      L.shield = v >= 2;
      L.weapon = (uint8_t)((o + v) % 7);
      L.topColor = rgba(40 + o * 17, 90 + v * 30, 160 - o * 9);
      humans.add(humanSheet(L), "outfit" + std::to_string(o) + "/v" + std::to_string(v));
    }
  for (int o = kOutfits; o < (int)Outfit::COUNT; o++) {
    HumanLook L;
    L.outfit = (Outfit)o;
    humansNew.add(humanSheet(L), "outfit" + std::to_string(o));
  }
  for (int h = kHairs; h < (int)Hair::COUNT; h++) {
    HumanLook L;
    L.hair = (Hair)h;
    humansNew.add(humanSheet(L), "hair" + std::to_string(h));
  }
  // M6 Steel: the culture arms grammar (polearms, bows, alloy sheens, legendary glow) and the culture icons
  for (int s = 0; s <= 8; s++) {
    HumanLook L;
    L.armorStyle = L.helmStyle = L.gloves = L.boots = 3;
    L.bodyForm = (uint8_t)(1 + s % 8); L.helmForm = (uint8_t)(1 + s); L.shieldForm = (uint8_t)(1 + s % 8);
    L.pauldron = (uint8_t)(s % 5); L.skirt = (uint8_t)((s + 2) % 5); L.crest = (uint8_t)(s % 4);
    L.weapon = s < 3 ? 8 : (s < 6 ? 3 : 1);
    L.polearmForm = (uint8_t)(1 + s % 3); L.bowForm = (uint8_t)(1 + s % 5); L.backItem = s >= 6 ? 1 : 0;
    if (s) { L.armourTint = rgba(180, 150 + s * 8, 120); L.armourTint2 = rgba(70, 50, 40 + s * 6); L.sheen = (uint8_t)s; }
    if (s == 8) { L.glow = 31; L.glowColor = rgba(140, 220, 255); L.cloak = 1; }
    humansNew.add(humanSheet(L), "m6arms" + std::to_string(s));
  }
  }   // (P2c flip) the v1 humans: rig 1 only
  for (int f = 0; f <= 10; f++) {
    static const Icon ic[6] = {Icon::Helmet, Icon::Armor, Icon::Shield, Icon::Sword, Icon::Spear, Icon::Bow};
    for (int k = 0; k < 6; k++) {
      IconLook l;
      l.icon = ic[k]; l.form = (uint8_t)f; l.tint = rgba(214, 156, 88); l.tint2 = rgba(104, 58, 34);
      l.sheen = (uint8_t)(f % 9); l.ornament = (uint16_t)(f * 53 % 1024); l.accent = rgba(40, 70, 150); l.rarity = (uint8_t)(f % 5);
      iconsNew.add(itemIconLook(l), "m6icon" + std::to_string(k) + "/" + std::to_string(f));
    }
  }
  for (int m = 0; m < (int)Monster::COUNT; m++) (m < kMonsters ? monsters : monstersNew).add(monsterSheet((Monster)m), "monster" + std::to_string(m));
  for (int p = 0; p < (int)Prop::COUNT; p++) (p < kProps ? props : propsNew).add(propSprite((Prop)p), "prop" + std::to_string(p));
  for (int b = 0; b < (int)Building::COUNT; b++) {
    static const int sizes[4][2] = {{4, 3}, {5, 4}, {6, 4}, {7, 5}};
    for (int s = 0; s < 4; s++)
      for (int roof = 0; roof < 2; roof++)
        (b < kBuildings ? buildings : buildingsNew)
            .add(buildingSprite(bld::design(bld::classicRequest((Building)b, sizes[s][0], sizes[s][1], roof ? rgba(150, 60, 50) : 0, 1234u + s * 77u))),
                 "bldg" + std::to_string(b) + "/" + std::to_string(sizes[s][0]) + "x" + std::to_string(sizes[s][1]) + (roof ? "/tint" : ""));
  }
  for (int m = 0; m < 16; m++) walls.add(wallPiece(m), "wall" + std::to_string(m));
  walls.add(gatePiece(), "gate");
  for (int i = 0; i < (int)Icon::COUNT; i++) {
    Cat& c = i < kIcons ? icons : iconsNew;
    c.add(itemIcon((Icon)i), "icon" + std::to_string(i));
    c.add(itemIcon((Icon)i, rgba(120, 200, 110)), "icon" + std::to_string(i) + "/tint");
  }
  for (int f = 0; f < (int)Fx::COUNT; f++) (f < kFx ? fx : fxNew).add(fxSprite((Fx)f), "fx" + std::to_string(f));

  H total;
  for (Cat* c : {rig2 ? &doll : &humans, &monsters, &props, &buildings, &walls, &icons, &fx}) {
    std::printf("%-10s %4d sprites  %016llx\n", c->name, c->n, (unsigned long long)c->h.h);
    total.u32((uint32_t)c->h.h); total.u32((uint32_t)(c->h.h >> 32));
  }
  std::printf("TOTAL %016llx\n", (unsigned long long)total.h);
  for (Cat* c : {&humansNew, &monstersNew, &propsNew, &buildingsNew, &iconsNew, &fxNew})
    if (c->n) std::printf("%-10s %4d sprites  %016llx\n", c->name, c->n, (unsigned long long)c->h.h);
  if (rig2) std::printf("(rig 2, the default; --rig=1: the v1 fallback's lines)\n");
  return 0;
}
