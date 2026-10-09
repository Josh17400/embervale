// M6 Steel script commands (BEASTS lane): stage ranked foes with explicit variant looks for screenshots and checks of the
// monster art, the auras, the name plates, the world boss's bar and the boss phases (rpg/view/render.cpp).
//   beast <monster> [n] [elite|champion|named|boss|worldboss] [overlay words] [tint ...] [affix words] [still]
//         [name WORDS_WITH_UNDERSCORES] [hp PCT] [at DX DY]
//         spawn n (1) monsters (default 70 px east of the player, a row going east), with:
//           overlays: horns spikes crystal moss rime ember plates eyes mask (or all)
//           tint: ashen | crystal | clay | iron | frost | moss | blood | RRGGBB (hex)
//           affix words: foes::affixInfo names (swift armoured vampiric ...)
//           still: they stand where they are put (no aggro, no walking: a line-up)
//           name: the actor's name ("name OLD_NINEFANGS" -> "OLD NINEFANGS")
//           hp: start at that percent of their health (the plates' and the boss bar's health)
//   beastclear: the monsters near the player are gone (between line-ups)
//   bossphase <n>: the nearest boss, world boss or named foe passes to phase n (0-2) and drops to its mark's health
//   beastlineup <rank words / overlay words / tint ...>: one of every family in two rows round the player, standing still
//   expect beasts [auras N] [plates N] [glows N] [bossbar] [ms X]: the last frame drew at least N auras / plates / glow
//         sheets, a world boss's bar, and at most X ms of ranked-foe overlay work
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "rpg/art.h"
#include "rpg/sim/foes.h"
#include "rpg/sim/game.h"
#include "rpg/story/dsl.h"
#include "rpg/view/script_api.h"

void beastsViewStats(int& auras, int& plates, int& glows, int& bossBar, double& ms);   // render.cpp

namespace {

std::string low(std::string s) {
  for (char& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
  return s;
}

struct BeastArgs {
  int n = 1;
  foes::Rank rank = foes::Rank::Elite;
  uint16_t overlays = 0, affixes = 0;
  uint32_t tint = 0;
  bool still = false;
  std::string name;
  float hp = 1.0f;
  float dx = 70.0f, dy = -6.0f;
};

uint16_t overlayWord(const std::string& w) {
  if (w == "horns") return art::MO_HORNS;
  if (w == "spikes") return art::MO_SPIKES;
  if (w == "crystal") return art::MO_CRYSTAL;
  if (w == "moss") return art::MO_MOSS;
  if (w == "rime") return art::MO_RIME;
  if (w == "ember") return art::MO_EMBER;
  if (w == "plates") return art::MO_PLATES;
  if (w == "eyes") return art::MO_EYES;
  if (w == "mask" || w == "bonemask") return art::MO_BONEMASK;
  if (w == "all") return art::MO_ALL;
  return 0;
}
uint32_t tintWord(const std::string& w) {
  if (w == "ashen") return rgba(84, 76, 82);
  if (w == "crystal") return art::GOLEM_CRYSTAL;
  if (w == "clay") return art::GOLEM_CLAY;
  if (w == "iron") return art::GOLEM_IRON;
  if (w == "frost") return rgba(150, 190, 230);
  if (w == "moss") return rgba(96, 130, 70);
  if (w == "blood") return rgba(150, 46, 44);
  if (w.size() == 6) {
    const unsigned long v = std::strtoul(w.c_str(), nullptr, 16);
    return rgba((int)(v >> 16) & 255, (int)(v >> 8) & 255, (int)v & 255);
  }
  return 0;
}

BeastArgs parse(const ScriptCtx& c, size_t from) {
  BeastArgs A;
  for (size_t i = from; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    if (!w.empty() && std::isdigit((unsigned char)w[0])) { A.n = std::clamp(std::atoi(w.c_str()), 1, 24); continue; }
    if (w == "elite") { A.rank = foes::Rank::Elite; continue; }
    if (w == "champion") { A.rank = foes::Rank::Champion; continue; }
    if (w == "named") { A.rank = foes::Rank::Named; continue; }
    if (w == "boss") { A.rank = foes::Rank::Boss; continue; }
    if (w == "worldboss") { A.rank = foes::Rank::WorldBoss; continue; }
    if (w == "normal") { A.rank = foes::Rank::Normal; continue; }
    if (w == "still") { A.still = true; continue; }
    if (w == "tint" && i + 1 < c.a.size()) { A.tint = tintWord(low(c.a[++i])); continue; }
    if (w == "name" && i + 1 < c.a.size()) {
      A.name = c.a[++i];
      for (char& ch : A.name) { if (ch == '_') ch = ' '; else ch = (char)std::toupper((unsigned char)ch); }
      continue;
    }
    if (w == "hp" && i + 1 < c.a.size()) { A.hp = std::clamp(std::atoi(c.a[++i].c_str()), 1, 100) / 100.0f; continue; }
    if (w == "at" && i + 2 < c.a.size()) { A.dx = (float)std::atof(c.a[i + 1].c_str()); A.dy = (float)std::atof(c.a[i + 2].c_str()); i += 2; continue; }
    if (const uint16_t o = overlayWord(w)) { A.overlays |= o; continue; }
    bool aff = false;
    for (int f = 0; f < (int)foes::Affix::COUNT; f++)
      if (low(foes::affixInfo((foes::Affix)f).name) == w) { A.affixes |= foes::affixBit((foes::Affix)f); aff = true; }
    if (aff) continue;
    if (const uint32_t t = tintWord(w)) A.tint = t;
  }
  return A;
}

void stage(Game& g, art::Monster m, const BeastArgs& A, Vec2 at, int k) {
  // (M6 fixer r2) a world boss's big body stands on open ground, never on a wall or a column (as the game places it)
  if (A.rank == foes::Rank::WorldBoss) at = g.freeSpotClear((int)std::floor(at.x / TILE), (int)std::floor(at.y / TILE), 1);
  const int id = g.debugSpawnAt(m, at, std::max(1, g.plLevel));
  for (Actor& a : g.actors)
    if (a.id == id) {
      a.rank = (uint8_t)A.rank;
      a.affixes = A.affixes;
      // (M6 fixer round 2, review: "the script stages world bosses at 200% while the game uses 160%") the rank's own
      // size as the game gives it (variantFor): its scale, and a world-boss lurker's GREAT form
      const foes::Variant rv = foes::variantFor(m, -1, 0, A.rank, 0);
      a.overlays = (uint16_t)(A.overlays | (rv.overlays & art::MO_GREAT));
      a.bodyTint = A.tint;
      a.pack = A.rank == foes::Rank::Champion ? 900 + k / 3 : 0;
      if (rv.scalePct) a.scalePct = rv.scalePct;
      if (A.rank == foes::Rank::WorldBoss) a.maxHp *= 25.0f;
      if (A.rank == foes::Rank::Named) a.maxHp *= 6.0f;
      if (A.rank == foes::Rank::Boss) a.boss = true;
      a.hp = a.maxHp * A.hp;
      if (!A.name.empty()) a.name = A.name;
      if (A.still) { a.aggro = false; a.aggroR = 0; a.speed = 0; }
    }
}

bool cmdBeast(ScriptCtx& c) {
  const int m = story::dsl::monsterWord(c.arg(1));
  if (m < 0 || m >= (int)art::Monster::COUNT) { c.fail("beast: unknown monster '" + c.arg(1) + "'"); return true; }
  const BeastArgs A = parse(c, 2);
  Game& g = c.game;
  for (int k = 0; k < A.n; k++)
    stage(g, (art::Monster)m, A, g.pl().p + Vec2(A.dx + 30.0f * (k % 6), A.dy + 26.0f * (k / 6)), k);
  return true;
}
EMB_SCRIPT_CMD("beast", "beast <monster> [n] [elite|champion|named|boss|worldboss] [overlays] [tint t] [affixes] [still] [name X_Y] [hp P] [at DX DY]: stage ranked foes (M6 BEASTS)", cmdBeast);

bool cmdLineup(ScriptCtx& c) {
  BeastArgs A = parse(c, 1);
  A.still = true;
  Game& g = c.game;
  const int N = (int)art::Monster::COUNT;
  int k = 0;
  for (int m = 0; m < N; m++) {
    if (m == (int)art::Monster::Dragon) continue;
    const int col = k % 8, row = k / 8;
    stage(g, (art::Monster)m, A, g.pl().p + Vec2(-180.0f + col * 50.0f, -70.0f + row * 50.0f), k);
    k++;
  }
  return true;
}
EMB_SCRIPT_CMD("beastlineup", "beastlineup [rank] [overlays] [tint t] [affixes]: every family standing in rows round the player (M6 BEASTS)", cmdLineup);

bool cmdBossPhase(ScriptCtx& c) {
  Game& g = c.game;
  const int ph = std::clamp(std::atoi(c.arg(1).c_str()), 0, 2);
  Actor* best = nullptr;
  float bd = 1e18f;
  for (Actor& a : g.actors) {
    if (a.st == AState::Dead || !a.hostile || a.player) continue;
    if (!(a.boss || a.rank == (uint8_t)foes::Rank::WorldBoss || a.rank == (uint8_t)foes::Rank::Named || a.rank == (uint8_t)foes::Rank::Boss)) continue;
    const float d = len2(a.p - g.pl().p);
    if (d < bd) { bd = d; best = &a; }
  }
  if (!best) { c.fail("bossphase: no boss near"); return true; }
  best->bossPhase = (uint8_t)ph;
  best->hp = std::min(best->hp, best->maxHp * (ph == 0 ? 1.0f : ph == 1 ? 0.6f : 0.3f));
  return true;
}
EMB_SCRIPT_CMD("bossphase", "bossphase <0-2>: the nearest boss passes to that phase (the flash, the shake, the roar; M6 BEASTS)", cmdBossPhase);

bool cmdClear(ScriptCtx& c) {
  Game& g = c.game;
  for (Actor& a : g.actors)
    if (!a.player && a.hostile && !a.human && len2(a.p - g.pl().p) < 420.0f * 420.0f) { a.hp = 0; a.st = AState::Dead; a.stT = 99.0f; }
  return true;
}
EMB_SCRIPT_CMD("beastclear", "beastclear: every monster near the player gone (between line-ups; M6 BEASTS)", cmdClear);

bool expBeasts(ScriptCtx& c) {
  int au = 0, pl = 0, gl = 0, bb = 0;
  double ms = 0;
  beastsViewStats(au, pl, gl, bb, ms);
  for (size_t i = 2; i < c.a.size(); i++) {
    const std::string w = low(c.a[i]);
    const bool has = i + 1 < c.a.size();
    if (w == "auras" && has && au < std::atoi(c.a[i + 1].c_str())) c.fail("expect beasts: auras " + std::to_string(au) + " < " + c.a[i + 1]);
    if (w == "plates" && has && pl < std::atoi(c.a[i + 1].c_str())) c.fail("expect beasts: plates " + std::to_string(pl) + " < " + c.a[i + 1]);
    if (w == "glows" && has && gl < std::atoi(c.a[i + 1].c_str())) c.fail("expect beasts: glows " + std::to_string(gl) + " < " + c.a[i + 1]);
    if (w == "bossbar" && !bb) c.fail("expect beasts: no world boss bar drawn");
    if (w == "ms" && has && ms > std::atof(c.a[i + 1].c_str())) c.fail("expect beasts: overlays took " + std::to_string(ms) + " ms > " + c.a[i + 1]);
  }
  std::printf("beasts: auras %d plates %d glows %d bossbar %d  %.3f ms\n", au, pl, gl, bb, ms);
  return true;
}
EMB_SCRIPT_CMD("expect:beasts", "expect beasts [auras N] [plates N] [glows N] [bossbar] [ms X]: the last frame's ranked-foe overlays (M6 BEASTS)", expBeasts);

}  // namespace
