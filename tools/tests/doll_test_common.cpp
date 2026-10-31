// doll_test_common.cpp: the RPG_DOLL_GATE registry behind rpg_test --doll --gates (SOLID 1.7; doll_test_common.h says
// how a gate registers and in which order the gates run and print).
#include "tools/tests/doll_test_common.h"

#include <algorithm>

std::vector<DollGate>& dollGates() {
  static std::vector<DollGate> v;
  return v;
}

std::vector<DollGate> sortedDollGates(int* dupKeys) {
  std::vector<DollGate> v = dollGates();
  std::stable_sort(v.begin(), v.end(), [](const DollGate& a, const DollGate& b) { return a.order < b.order; });
  for (size_t i = 1; i < v.size(); i++)
    if (v[i].order == v[i - 1].order) {
      std::printf("FAIL: doll gate registry: %s and %s share order key %d\n", v[i - 1].name, v[i].name, v[i].order);
      if (dupKeys) ++*dupKeys;
    }
  return v;
}
