// doll_test_common.h: the doll tests' shared pieces (SOLID 1.7). The looks-line type and file reader whose copies were
// byte-identical, the embedded looks-text accessors and every entry point one tools/tests/test_doll*.cpp calls in
// another (they were hand-written prototypes), and the RPG_DOLL_GATE registry that rpg_test --doll --gates iterates.
// doll_motion.h stays the GATES-LIMBS / GATES-WEAR contract; doll_test_common.cpp holds the registry.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "rpg/art/art_human.h"
#include "tools/tests/tests.h"

enum class Role : uint8_t;   // rpg/sim/world.h
namespace cult { struct Culture; }

// ---------------------------------------------------------------- the copies that were byte-identical
namespace dolltest {
struct Look { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; };   // test_doll_arms.cpp, test_doll_ladder.cpp
// test_doll_envelope.cpp, test_doll_ladder.cpp: a whole file as text ("" when it cannot be read)
inline std::string readFile(const char* file) {
  std::string t;
  FILE* f = std::fopen(file, "rb");
  if (!f) return t;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) t.append(buf, n);
  std::fclose(f);
  return t;
}
}  // namespace dolltest

// ---------------------------------------------------------------- in their own namespaces, and outside the doll tests
namespace dollmotion { void setMapsRoot(const std::string& dir); }   // test_doll_motion.cpp (--maps)
namespace wear { std::vector<int> frozenLookCells(const char* manifest); }   // test_doll_wear.cpp (test_doll_shapes.cpp's --forge-frozen)

// rpg/sim/looks.cpp: the census (test_doll.cpp dresses its residents with it)
namespace census {
struct Rank { bool royal = false, capital = false; };
void dress(art::HumanLook& L, std::string& name, Role r, bool female, const cult::Culture& C, const cult::Culture& owner, uint64_t seed,
           const Rank& rank);
}  // namespace census

// ---------------------------------------------------------------- the embedded looks texts
std::string dollSliceLooksText();   // test_doll_arms.cpp: the gates' matrix (the ladder looks, then the forge looks whose tags it lacks)
std::string dollArmsLooksText();    // test_doll_arms.cpp: tools/doll/arms_looks.txt, embedded; the matrices M8-M12
std::string dollForgeLooksText();   // test_doll_forge.cpp: tools/doll/forge_looks.txt, embedded
std::string dollSourceLText(int a, int b);   // test_doll_envelope.cpp: C1 source L's looks, then the envelope looks
std::vector<std::pair<std::string, art::HumanLook>> dollSpecLooks(const std::string& text);   // test_doll_envelope.cpp (D79)
void dollCensusLooks(uint64_t a, uint64_t b, std::vector<std::pair<char, art::HumanLook>>& out);   // test_doll.cpp

// ---------------------------------------------------------------- the gate signatures
int dollArmsGates(bool report, std::vector<std::string>& printed);   // test_doll_arms.cpp: C3 gear, C4 armour change, C5 families, C6 weapons
int dollLadderGates(bool report, std::vector<std::string>& printed);   // test_doll_ladder.cpp: C12 C13 C14 and their selftests
int dollForgeGates(bool report, std::vector<std::string>& printed);   // test_doll_forge.cpp: C16 a b c s, C17, C1 F, C12a
int dollEnvelopeGates(bool report, std::vector<std::string>& printed);   // test_doll_envelope.cpp: C12f C3b C13 B10 by tier, C19
int dollHeadGates(bool report, bool selftest, std::vector<std::string>& printed);   // test_doll_envelope.cpp: C16k
int dollShapeGates(bool report, std::vector<std::string>& printed, const std::vector<std::pair<std::string, art::HumanLook>>& forgeLooks);   // test_doll_shapes.cpp
int forgeShapeData(int w0, int w1, int s0, int s1, bool report, bool selftest, std::vector<std::string>& printed);   // test_doll_shapes.cpp
int forgeSilhouettes(int w0, int w1, bool report, bool selftest, std::vector<std::string>& printed);   // test_doll_shapes.cpp
int dollLadderCoverage(int argc, char** argv);   // test_doll_ladder.cpp: --doll-coverage --scope ladder (C1 source L)
int dollForgeCoverage(int argc, char** argv);   // test_doll_forge.cpp: --doll-coverage --scope forge
int dollEnvelopeCoverage(int argc, char** argv);   // test_doll_envelope.cpp: --doll-coverage --scope envelope

// ---------------------------------------------------------------- the --doll --gates registry
// A file registers its gate lines once, next to the function:
//     RPG_DOLL_GATE(110, "forge", dollForgeGates);
// --doll --gates runs them in order-key order. A gate keyed below kDollGateAfterTable runs before the B table
// (B2 ... B11 selftest) is printed and a gate keyed at or above it runs after; every gate's lines print after the
// table, in key order. Keys are unique (a repeated key is a FAIL line). Today: arms 10, ladder 20, forge 110, envelope 120.
struct DollGate {
  int order;
  const char* name;
  int (*run)(bool report, std::vector<std::string>& printed);
};
constexpr int kDollGateAfterTable = 100;
std::vector<DollGate>& dollGates();          // registration order
std::vector<DollGate> sortedDollGates(int* dupKeys = nullptr);   // by order key; a repeated key prints a FAIL and counts in *dupKeys
inline int registerDollGate(const DollGate& g) { dollGates().push_back(g); return (int)dollGates().size(); }
#define RPG_DOLL_GATE(order, name, fn) static const int RPG_TEST_CAT(rpgDollGateReg_, __LINE__) = registerDollGate(DollGate{order, name, fn})
