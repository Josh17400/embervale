// rpg_test: the PNG reader (P2b phase A, lead). A small inflate (RFC 1951: stored, fixed and dynamic Huffman blocks) and
// the PNG filters, enough for the A3 reference frames (docs/ref/style_a3/frames: PIL's RGBA) and any 8-bit
// non-interlaced PNG.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "tools/tests/tests.h"

namespace {
struct Bits {
  const uint8_t* p;
  size_t n, at = 0;
  uint32_t buf = 0;
  int cnt = 0;
  bool bad = false;
  int bit() {
    if (!cnt) {
      if (at >= n) { bad = true; return 0; }
      buf = p[at++];
      cnt = 8;
    }
    const int b = (int)(buf & 1);
    buf >>= 1;
    cnt--;
    return b;
  }
  int bits(int k) { int v = 0; for (int i = 0; i < k; i++) v |= bit() << i; return v; }
};
struct Huff { int count[16] = {}; int sym[320] = {}; };
void build(Huff& h, const uint8_t* len, int n) {
  for (int& c : h.count) c = 0;
  for (int i = 0; i < n; i++) h.count[len[i]]++;
  h.count[0] = 0;
  int offs[16] = {};
  for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + h.count[i - 1];
  for (int i = 0; i < n; i++) if (len[i]) h.sym[offs[len[i]]++] = i;
}
int decodeSym(Bits& b, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len < 16; len++) {
    code |= b.bit();
    const int count = h.count[len];
    if (code - count < first) return h.sym[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
    if (b.bad) return -1;
  }
  return -1;
}
bool inflate(const uint8_t* src, size_t n, std::vector<uint8_t>& out) {
  static const int lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const int lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const int dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                4097, 6145, 8193, 12289, 16385, 24577};
  static const int dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  if (n < 2) return false;
  Bits b{src + 2, n - 2};   // (past the zlib header)
  int last = 0;
  while (!last) {
    last = b.bit();
    const int type = b.bits(2);
    if (type == 0) {
      b.cnt = 0;
      if (b.at + 4 > b.n) return false;
      const size_t len = (size_t)(b.p[b.at] | b.p[b.at + 1] << 8);
      b.at += 4;
      if (b.at + len > b.n) return false;
      out.insert(out.end(), b.p + b.at, b.p + b.at + len);
      b.at += len;
      continue;
    }
    Huff lit, dist;
    if (type == 1) {
      uint8_t l[288];
      for (int i = 0; i < 288; i++) l[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
      build(lit, l, 288);
      uint8_t d[30];
      for (uint8_t& x : d) x = 5;
      build(dist, d, 30);
    } else if (type == 2) {
      const int hlit = b.bits(5) + 257, hdist = b.bits(5) + 1, hclen = b.bits(4) + 4;
      static const int ord[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      uint8_t cl[19] = {};
      for (int i = 0; i < hclen; i++) cl[ord[i]] = (uint8_t)b.bits(3);
      Huff ch;
      build(ch, cl, 19);
      uint8_t lens[320] = {};
      int i = 0;
      while (i < hlit + hdist) {
        const int s = decodeSym(b, ch);
        if (s < 0) return false;
        if (s < 16) { lens[i++] = (uint8_t)s; continue; }
        int rep = 0;
        uint8_t v = 0;
        if (s == 16) { if (!i) return false; v = lens[i - 1]; rep = 3 + b.bits(2); }
        else if (s == 17) rep = 3 + b.bits(3);
        else rep = 11 + b.bits(7);
        while (rep-- > 0 && i < hlit + hdist) lens[i++] = v;
      }
      build(lit, lens, hlit);
      build(dist, lens + hlit, hdist);
    } else {
      return false;
    }
    for (;;) {
      const int s = decodeSym(b, lit);
      if (s < 0 || b.bad) return false;
      if (s < 256) { out.push_back((uint8_t)s); continue; }
      if (s == 256) break;
      const int li = s - 257;
      if (li >= 29) return false;
      const int len = lbase[li] + b.bits(lext[li]);
      const int ds = decodeSym(b, dist);
      if (ds < 0 || ds >= 30) return false;
      const size_t d = (size_t)(dbase[ds] + b.bits(dext[ds]));
      if (d > out.size()) return false;
      for (int k = 0; k < len; k++) out.push_back(out[out.size() - d]);
    }
  }
  return !b.bad;
}
}  // namespace

bool readPng(const char* path, int& w, int& h, std::vector<uint32_t>& rgba) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  std::vector<uint8_t> d;
  uint8_t buf[65536];
  size_t k;
  while ((k = std::fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + k);
  std::fclose(f);
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (d.size() < 8 || std::memcmp(d.data(), sig, 8)) return false;
  auto be = [&](size_t at) { return (uint32_t)d[at] << 24 | (uint32_t)d[at + 1] << 16 | (uint32_t)d[at + 2] << 8 | d[at + 3]; };
  std::vector<uint8_t> z, pal, trns;
  int depth = 0, ctype = 0, interlace = 0;
  size_t at = 8;
  w = h = 0;
  while (at + 8 <= d.size()) {
    const uint32_t len = be(at);
    const char* type = (const char*)&d[at + 4];
    if (at + 12 + len > d.size()) return false;
    const uint8_t* c = &d[at + 8];
    if (!std::memcmp(type, "IHDR", 4)) { w = (int)be(at + 8); h = (int)be(at + 12); depth = c[8]; ctype = c[9]; interlace = c[12]; }
    else if (!std::memcmp(type, "PLTE", 4)) pal.assign(c, c + len);
    else if (!std::memcmp(type, "tRNS", 4)) trns.assign(c, c + len);
    else if (!std::memcmp(type, "IDAT", 4)) z.insert(z.end(), c, c + len);
    else if (!std::memcmp(type, "IEND", 4)) break;
    at += 12 + len;
  }
  if (w <= 0 || h <= 0 || depth != 8 || interlace) return false;
  const int ch = ctype == 6 ? 4 : ctype == 2 ? 3 : ctype == 4 ? 2 : 1;
  std::vector<uint8_t> raw;
  if (!inflate(z.data(), z.size(), raw)) return false;
  const size_t stride = (size_t)w * ch;
  if (raw.size() < (stride + 1) * (size_t)h) return false;
  std::vector<uint8_t> img(stride * h), prev(stride, 0);
  for (int y = 0; y < h; y++) {
    const uint8_t ft = raw[(stride + 1) * y];
    const uint8_t* s = &raw[(stride + 1) * y + 1];
    uint8_t* o = &img[stride * y];
    for (size_t x = 0; x < stride; x++) {
      const int a = x >= (size_t)ch ? o[x - ch] : 0, up = prev[x], ul = x >= (size_t)ch ? prev[x - ch] : 0;
      int v = s[x];
      if (ft == 1) v += a;
      else if (ft == 2) v += up;
      else if (ft == 3) v += (a + up) / 2;
      else if (ft == 4) {
        const int p = a + up - ul, pa = std::abs(p - a), pb = std::abs(p - up), pc = std::abs(p - ul);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? up : ul;
      }
      o[x] = (uint8_t)v;
    }
    std::memcpy(prev.data(), o, stride);
  }
  rgba.assign((size_t)w * h, 0);
  for (size_t i = 0; i < (size_t)w * h; i++) {
    const uint8_t* p = &img[i * ch];
    uint32_t r = p[0], g = p[0], b = p[0], a = 255;
    if (ctype == 6) { g = p[1]; b = p[2]; a = p[3]; }
    else if (ctype == 2) { g = p[1]; b = p[2]; }
    else if (ctype == 4) a = p[1];
    else if (ctype == 3) {
      const size_t q = (size_t)p[0] * 3;
      if (q + 2 >= pal.size()) return false;
      r = pal[q]; g = pal[q + 1]; b = pal[q + 2];
      if (p[0] < trns.size()) a = trns[p[0]];
    }
    rgba[i] = r | g << 8 | b << 16 | a << 24;
  }
  return true;
}
