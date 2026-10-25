// rpg_test --forge-ramps (forge fix i1, ENGINE): the resolved ramps of looks, so a metal's read can be judged in numbers.
//
//   rpg_test --forge-ramps --look SPEC [--look SPEC ...] [--slots Metal,Metal2,Leather,...]
//       every named slot's five steps (hex, luma, Lab L C h) of art::doll::rampsOf on the look (applyLookSpec), and the
//       metal's verdicts (metalRead against the look's skin, metalRead2)
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "rpg/art.h"
#include "rpg/art/doll/doll.h"
#include "tools/tests/tests.h"

namespace forgeramps {
namespace {
double lin(double c) { c /= 255.0; return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
void lab(uint32_t c, double& L, double& C, double& H) {
  const double r = lin(c & 255), g = lin(c >> 8 & 255), b = lin(c >> 16 & 255);
  const double X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b,
               Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
  auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0; };
  const double fx = f(X), fy = f(Y), fz = f(Z), a = 500 * (fx - fy), bb = 200 * (fy - fz);
  L = 116 * fy - 16; C = std::hypot(a, bb); H = std::fmod(std::atan2(bb, a) * 57.29578 + 360.0, 360.0);
}
double luma(uint32_t c) { return (c & 255) * 0.299 + (c >> 8 & 255) * 0.587 + (c >> 16 & 255) * 0.114; }

int cmd(int argc, char** argv) {
  std::vector<std::string> looks;
  std::string slots = "metal,metal2,glow,leather,cloth1,cloth2,accent,skin";
  for (int i = 2; i < argc; i++) {
    if (!std::strcmp(argv[i], "--look") && i + 1 < argc) looks.push_back(argv[++i]);
    else if (!std::strcmp(argv[i], "--slots") && i + 1 < argc) slots = argv[++i];
  }
  const int was = art::rigVersion();
  art::setRigVersion(2);
  for (const std::string& spec : looks) {
    art::HumanLook L;
    std::string bad;
    art::doll::applyLookSpec(L, spec, nullptr, nullptr, &bad);
    const art::doll::DollRamps R = art::doll::rampsOf(L);
    std::printf("look %s  (metalKind %d sheen %d alloyFam %d)%s%s\n", spec.c_str(), (int)L.metalKind, (int)L.sheen, (int)L.alloyFam,
                bad.empty() ? "" : " bad: ", bad.c_str());
    {
      const art::doll::DollRecipe rc = art::doll::resolve(L);
      std::printf("  flags:");
      for (const auto& f : rc.flags) std::printf(" %s", std::string(f).c_str());
      std::printf("\n");
    }
    size_t p = 0;
    while (p <= slots.size()) {
      const size_t q = std::min(slots.find(',', p), slots.size());
      const std::string nm = slots.substr(p, q - p);
      p = q + 1;
      for (int s = 0; s < art::doll::SLOT_COUNT; s++) {
        { std::string a = nm, b = art::doll::slotName((art::doll::Slot)s); for (char& ch : a) ch = (char)std::tolower((unsigned char)ch); if (a != b) continue; }
        std::printf("  %-8s", nm.c_str());
        for (int k = 0; k < 5; k++) {
          const uint32_t c = R.c[s][k];
          double l, cc, h;
          lab(c, l, cc, h);
          std::printf(" #%02X%02X%02X y%3.0f L%2.0f C%2.0f h%3.0f |", c & 255, c >> 8 & 255, c >> 16 & 255, luma(c), l, cc, h);
        }
        std::printf("\n");
      }
    }
    std::string why1, why2;
    const bool r1 = art::doll::metalRead(R.c[(int)art::doll::Slot::Metal], R.c[(int)art::doll::Slot::Skin][2], &why1);
    const bool r2 = art::doll::metalRead2(R.c[(int)art::doll::Slot::Metal], &why2);
    std::printf("  metalRead %s%s  metalRead2 %s%s\n", r1 ? "ok" : "NO ", why1.c_str(), r2 ? "ok" : "NO ", why2.c_str());
  }
  art::setRigVersion(was);
  return 0;
}
}  // namespace
}  // namespace forgeramps

RPG_TEST_CMD("--forge-ramps", "forge fix: a look's resolved ramps and metal verdicts --look SPEC [--slots a,b]", forgeramps::cmd);
