// rpg_test --marks: save compatibility of Game::marks (docs/SOLID_PLAN.md 2a.4 SIM-SEAMS). The marks map is saved as
// u64 -> i32 under keys markKey(id, tag) (rpg/sim/mark_tags.h), so a key that hashes differently loses every saved mark
// of that kind. This hashes 12 known (id, tag) keys, across the lanes' tags, against values hard-coded here (computed
// once, outside the engine, from the splitmix64 finaliser and the mix constant), and checks the tag register: the
// lane constants this test can see equal their register entries, and every register value is still what it was.
//   rpg_test --marks        one PASS/FAIL line per check, then "marks: OK" or "marks: FAIL n"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "rpg/sim/foes.h"
#include "rpg/sim/mark_tags.h"
#include "tools/tests/tests.h"

namespace {

// the lane constants this test can include agree with the register (compile time)
constexpr uint64_t regValue(const char* name) {
  for (const gsim::MarkTag& t : gsim::kMarkTags) {
    const char* a = t.name; const char* b = name;
    while (*a && *b && *a == *b) { a++; b++; }
    if (*b == 0 && (*a == 0 || *a == ' ')) return t.value;
  }
  return ~0ull;
}
static_assert(regValue("foes::MK_FOES_NAMED_SLAIN") == foes::MK_FOES_NAMED_SLAIN, "register: FOES named slain");
static_assert(regValue("foes::MK_FOES_BOSS_SLAIN") == foes::MK_FOES_BOSS_SLAIN, "register: FOES boss slain");
static_assert(regValue("foes::MK_FOES_RUMOUR") == foes::MK_FOES_RUMOUR, "register: FOES rumour");
static_assert(regValue("foes::MK_FOES_RAIDDAY") == foes::MK_FOES_RAIDDAY, "register: FOES raid day");
static_assert(regValue("Mk::StoryGiver") == (uint64_t)gsim::Mk::StoryGiver, "register: Mk::StoryGiver");
static_assert(regValue("Mk::BoardTaken") == (uint64_t)gsim::Mk::BoardTaken, "register: Mk::BoardTaken");

struct KeyCase { uint64_t id; uint64_t tag; uint64_t want; const char* what; };
const KeyCase kKeys[12] = {
    {0x0000000000000000ull, 1, 0x2D0F28C7E7E786B2ull, "Mk::Stones, id 0"},
    {0x00001F0000234001ull, 4, 0xCDBDA9E544470BC1ull, "Mk::News, an endless-world site gid"},
    {0x000000000100100Aull, 8, 0x85F28D0DBEE55F49ull, "Mk::StoryGiver, a classic-island npcKey"},
    {0x000000000007E003ull, 8, 0xAF60E7C6AC589326ull, "the view's biome-seen key, eco 3"},
    {0xFFFFFFFFFFFFFFFFull, 13, 0x11FE9B5D050A4C48ull, "story::MK_P1B_PLAYED, all bits"},
    {0xDEADBEEFCAFEF00Dull, 64, 0x69C2B1853B17DCD2ull, "foes::MK_FOES_NAMED_SLAIN"},
    {0x9E3779B97F4A7C15ull, 80, 0xEEAC6747411E23C2ull, "MK_THEFT / LifeOps::markOf 0"},
    {0x0000000000003039ull, 97, 0x8432745BD87AC756ull, "HOME beast patted"},
    {0x123456789ABCDEF1ull, 112, 0x9B8AA5EBC3CF549Full, "PartyOps::MK_HIRED"},
    {0x00000001000F1003ull, 128, 0xF186F43D83A12993ull, "MK_JOB_WEEK"},
    {0x000000000005A6A9ull, 145, 0x60390DAF9D160C36ull, "LifeOps::mkLife(1)"},
    {0x0000000000000000ull, 200, 0x8180E3962A23FB4Bull, "MK_SEA_EARLYSALE, key 0"},
};

// the register's values as recorded at 4c0c9a1 (a renumbered tag orphans saved marks)
struct RegCase { const char* name; uint64_t value; };
const RegCase kReg[] = {
    {"Mk::Stones", 1}, {"Mk::Toll", 2}, {"Mk::Grave", 3}, {"Mk::News", 4}, {"Mk::HeirChest", 5}, {"Mk::LastOffer", 6},
    {"Mk::Ambush", 7}, {"Mk::StoryGiver", 8}, {"Mk::StoryMark", 9}, {"Mk::BoardTaken", 10},
    {"story::MK_TALE_TELLER", 11}, {"story::MK_TALE_HEARD", 12}, {"story::MK_P1B_PLAYED", 13}, {"story::MK_P1B_COOLDOWN", 14},
    {"story::MK_P1B_GOSSIP", 15}, {"story::MK_P1B_BEACON", 16}, {"story::MK_P1B_CALM", 17},
    {"foes::MK_FOES_NAMED_SLAIN", 64}, {"foes::MK_FOES_BOSS_SLAIN", 65}, {"foes::MK_FOES_RUMOUR", 66}, {"foes::MK_FOES_RAIDDAY", 67},
    {"MK_THEFT", 80}, {"MK_BARRED", 81}, {"PartyOps::MK_HIRED", 112}, {"PartyOps::MK_PUP", 113}, {"MK_JOB_WEEK", 128},
    {"MK_REQ_RAISE", 176}, {"MK_SEA_EARLYSALE", 200}, {"view biome seen", 8},
};

int marksCmd(int, char**) {
  int fails = 0;
  for (const KeyCase& k : kKeys) {
    const uint64_t got = gsim::markKey(k.id, (gsim::Mk)k.tag);
    const bool ok = got == k.want;
    printf("%s key %-45s id=%016llx tag=%3llu -> %016llx%s\n", ok ? "PASS" : "FAIL", k.what, (unsigned long long)k.id,
           (unsigned long long)k.tag, (unsigned long long)got, ok ? "" : " (want a different value: saves would not load)");
    if (!ok) fails++;
  }
  int regFails = 0;
  for (const RegCase& r : kReg) {
    const uint64_t v = regValue(r.name);
    if (v != r.value) { printf("FAIL register %s = %llu, recorded %llu\n", r.name, (unsigned long long)v, (unsigned long long)r.value); regFails++; }
  }
  printf("%s register: %d recorded values checked, %zu tags listed\n", regFails ? "FAIL" : "PASS", (int)(sizeof(kReg) / sizeof(kReg[0])),
         gsim::kMarkTagCount);
  fails += regFails;
  const bool biome = gsim::markKey(gsim::kBiomeSeenIdBase + 3, (gsim::Mk)gsim::kBiomeSeenTag) == kKeys[3].want;
  printf("%s biome-seen key: markKey(kBiomeSeenIdBase + eco, kBiomeSeenTag)\n", biome ? "PASS" : "FAIL");
  if (!biome) fails++;
  if (fails) printf("marks: FAIL %d\n", fails);
  else printf("marks: OK\n");
  return fails ? 1 : 0;
}

}  // namespace

RPG_TEST_CMD("--marks", "Game::marks save compatibility: 12 known (id, tag) keys hashed against recorded values; the tag register", marksCmd);
