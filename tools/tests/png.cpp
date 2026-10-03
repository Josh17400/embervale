// rpg_test: PNG writer and overview-map colours.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "tools/tests/tests.h"

namespace {
uint32_t crcTable[256];
void crcInit() {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crcTable[n] = c;
  }
}
uint32_t crc(const uint8_t* b, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; i++) c = crcTable[(c ^ b[i]) & 255] ^ (c >> 8);
  return c;
}
void be32(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
  std::vector<uint8_t> b;
  be32(b, (uint32_t)data.size());
  b.insert(b.end(), type, type + 4);
  b.insert(b.end(), data.begin(), data.end());
  uint32_t c = crc(b.data() + 4, b.size() - 4) ^ 0xFFFFFFFFu;
  be32(b, c);
  fwrite(b.data(), 1, b.size(), f);
}
}  // namespace

bool writePng(const char* path, int w, int h, const std::vector<uint32_t>& rgba) {
  crcInit();
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ih;
  be32(ih, w); be32(ih, h);
  ih.push_back(8); ih.push_back(6); ih.push_back(0); ih.push_back(0); ih.push_back(0);
  chunk(f, "IHDR", ih);
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; y++) {
    raw.push_back(0);
    for (int x = 0; x < w; x++) { uint32_t c = rgba[(size_t)y * w + x]; raw.push_back(c & 255); raw.push_back((c >> 8) & 255); raw.push_back((c >> 16) & 255); raw.push_back(255); }
  }
  std::vector<uint8_t> z{0x78, 0x01};
  size_t pos = 0;
  while (pos < raw.size()) {
    size_t n = std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  be32(z, (b << 16) | a);
  chunk(f, "IDAT", z);
  chunk(f, "IEND", {});
  fclose(f);
  return true;
}

uint32_t groundColor(Ground g) {
  switch (g) {
    case Ground::DeepWater: return rgba(30, 60, 120);
    case Ground::Water: return rgba(50, 100, 170);
    case Ground::Sand: return rgba(220, 200, 140);
    case Ground::Swamp: return rgba(80, 96, 60);
    case Ground::Grass: return rgba(96, 160, 70);
    case Ground::Meadow: return rgba(120, 175, 80);
    case Ground::ForestFloor: return rgba(60, 110, 50);
    case Ground::Autumn: return rgba(170, 110, 50);
    case Ground::Tundra: return rgba(120, 140, 110);
    case Ground::Snow: return rgba(235, 240, 245);
    case Ground::Dirt: return rgba(140, 110, 70);
    case Ground::Farmland: return rgba(110, 80, 50);
    case Ground::Road: return rgba(190, 170, 130);
    case Ground::Plaza: return rgba(170, 170, 170);
    case Ground::Rock: return rgba(110, 104, 100);
    case Ground::Bridge: return rgba(150, 100, 60);
    case Ground::StoneFloor: return rgba(150, 150, 160);
    default: return rgba(255, 0, 255);
  }
}
