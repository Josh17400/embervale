#include "engine/gfx.h"
#include <SDL3/SDL.h>
#include <cstring>

namespace {
// 5x7 bitmap font, uppercase + digits + a little punctuation. Each glyph = 7 rows, 5 bits (MSB left).
struct Glyph { char ch; uint8_t rows[7]; };
const Glyph kFont[] = {
  {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}}, {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
  {'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}}, {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
  {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}}, {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
  {'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}}, {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
  {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}}, {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
  {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}}, {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
  {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}}, {'N',{0x11,0x11,0x19,0x15,0x13,0x11,0x11}},
  {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
  {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}}, {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
  {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}}, {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
  {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}}, {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
  {'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}}, {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
  {'Y',{0x11,0x11,0x11,0x0A,0x04,0x04,0x04}}, {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
  {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}}, {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
  {'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}}, {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
  {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}}, {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
  {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}}, {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
  {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}}, {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
  {'.',{0,0,0,0,0,0x0C,0x0C}}, {',',{0,0,0,0,0x0C,0x04,0x08}}, {':',{0,0x0C,0x0C,0,0x0C,0x0C,0}},
  {'!',{0x04,0x04,0x04,0x04,0x04,0,0x04}}, {'?',{0x0E,0x11,0x01,0x02,0x04,0,0x04}},
  {'+',{0,0x04,0x04,0x1F,0x04,0x04,0}}, {'-',{0,0,0,0x1F,0,0,0}}, {'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
  {'%',{0x18,0x19,0x02,0x04,0x08,0x13,0x03}}, {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}},
  {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}}, {'x',{0,0,0x11,0x0A,0x04,0x0A,0x11}},
  {'>',{0x08,0x04,0x02,0x01,0x02,0x04,0x08}}, {'<',{0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
};
constexpr int kGlyphCount = sizeof(kFont) / sizeof(kFont[0]);
constexpr int kCellW = 8, kCellH = 8, kAtlasCols = 16, kAtlasRows = 6;

int glyphIndex(char c) {
  if (c >= 'a' && c <= 'z' && c != 'x') c = (char)(c - 32);
  for (int i = 0; i < kGlyphCount; i++) if (kFont[i].ch == c) return i;
  return -1;
}

SDL_Texture* makeTexture(SDL_Renderer* r, int w, int h, const uint32_t* px, SDL_BlendMode bm, bool linear) {
  SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, (void*)px, w * 4);
  SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
  SDL_DestroySurface(s);
  if (t) {
    SDL_SetTextureBlendMode(t, bm);
    SDL_SetTextureScaleMode(t, linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
  }
  return t;
}

SDL_Vertex vert(Vec2 p, Color c) {
  SDL_Vertex v;
  v.position = {p.x, p.y};
  v.color = {c.r, c.g, c.b, c.a};
  v.tex_coord = {0, 0};
  return v;
}

void sprite(SDL_Renderer* r, SDL_Texture* t, float cx, float cy, float rad, Color c) {
  SDL_SetTextureColorModFloat(t, c.r, c.g, c.b);
  SDL_SetTextureAlphaModFloat(t, c.a);
  SDL_FRect d{cx - rad, cy - rad, rad * 2, rad * 2};
  SDL_RenderTexture(r, t, nullptr, &d);
}
}  // namespace

bool Gfx::init(const char* title, int winW, int winH, bool vsync) {
  if (!SDL_CreateWindowAndRenderer(title, winW, winH, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &win_, &ren_)) {
    SDL_Log("window/renderer failed: %s", SDL_GetError());
    return false;
  }
  SDL_SetRenderLogicalPresentation(ren_, VIEW_W, VIEW_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(ren_, vsync ? 1 : 0);

  // soft glow (alpha falls off smoothly) + crisp disc
  const int N = 128;
  std::vector<uint32_t> g(N * N), d(N * N);
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      float dx = (x + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1;
      float r = std::sqrt(dx * dx + dy * dy);
      float a = clampf(1.0f - r, 0, 1);
      a = a * a * a;  // tight core, long soft tail
      g[y * N + x] = (uint32_t)(uint8_t)(a * 255) << 24 | 0x00FFFFFFu;
      float e = clampf((1.0f - r) * (N * 0.5f), 0, 1);  // 1px antialiased edge
      d[y * N + x] = (uint32_t)(uint8_t)(e * 255) << 24 | 0x00FFFFFFu;
    }
  glowTex_ = makeTexture(ren_, N, N, g.data(), SDL_BLENDMODE_ADD, true);
  discTex_ = makeTexture(ren_, N, N, d.data(), SDL_BLENDMODE_BLEND, true);

  // font atlas
  int aw = kAtlasCols * kCellW, ah = kAtlasRows * kCellH;
  std::vector<uint32_t> f((size_t)aw * ah, 0);
  for (int gi = 0; gi < kGlyphCount; gi++) {
    int ox = (gi % kAtlasCols) * kCellW, oy = (gi / kAtlasCols) * kCellH;
    for (int row = 0; row < 7; row++)
      for (int col = 0; col < 5; col++)
        if (kFont[gi].rows[row] & (0x10 >> col)) f[(size_t)(oy + row) * aw + ox + col] = 0xFFFFFFFFu;
  }
  fontTex_ = makeTexture(ren_, aw, ah, f.data(), SDL_BLENDMODE_BLEND, false);
  return glowTex_ && discTex_ && fontTex_;
}

void Gfx::shutdown() {
  if (glowTex_) SDL_DestroyTexture(glowTex_);
  if (discTex_) SDL_DestroyTexture(discTex_);
  if (fontTex_) SDL_DestroyTexture(fontTex_);
  if (ren_) SDL_DestroyRenderer(ren_);
  if (win_) SDL_DestroyWindow(win_);
  glowTex_ = discTex_ = fontTex_ = nullptr;
  ren_ = nullptr;
  win_ = nullptr;
}

void Gfx::begin(Color c) {
  SDL_SetRenderDrawColorFloat(ren_, c.r, c.g, c.b, 1.0f);
  SDL_RenderClear(ren_);
}
void Gfx::end() { SDL_RenderPresent(ren_); }

bool Gfx::onScreen(Vec2 w, float m) const {
  Vec2 s = toScreen(w);
  float mm = m * zoom;
  return s.x > -mm && s.x < VIEW_W + mm && s.y > -mm && s.y < VIEW_H + mm;
}

void Gfx::windowToLogical(float wx, float wy, float& lx, float& ly) const {
  SDL_RenderCoordinatesFromWindow(ren_, wx, wy, &lx, &ly);
}

bool Gfx::screenshot(const char* path) {
  SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr);
  if (!s) return false;
  bool ok = SDL_SavePNG(s, path);
  SDL_DestroySurface(s);
  return ok;
}

void Gfx::glow(Vec2 w, float radius, Color c) {
  Vec2 s = toScreen(w);
  sprite(ren_, glowTex_, s.x, s.y, radius * zoom, c);
}
void Gfx::disc(Vec2 w, float radius, Color c, bool additive) {
  Vec2 s = toScreen(w);
  if (additive) {
    SDL_SetTextureBlendMode(discTex_, SDL_BLENDMODE_ADD);
    sprite(ren_, discTex_, s.x, s.y, radius * zoom, c);
    SDL_SetTextureBlendMode(discTex_, SDL_BLENDMODE_BLEND);
  } else {
    sprite(ren_, discTex_, s.x, s.y, radius * zoom, c);
  }
}
void Gfx::glowS(float x, float y, float radius, Color c) { sprite(ren_, glowTex_, x, y, radius, c); }
void Gfx::discS(float x, float y, float r, Color c) { sprite(ren_, discTex_, x, y, r, c); }

void Gfx::geometry(const std::vector<SDL_Vertex>& v, const std::vector<int>& idx, bool additive) {
  if (v.empty() || idx.empty()) return;
  SDL_SetRenderDrawBlendMode(ren_, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_RenderGeometry(ren_, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void Gfx::line(Vec2 a, Vec2 b, float width, Color c, bool additive) {
  Vec2 sa = toScreen(a), sb = toScreen(b);
  Vec2 d = sb - sa;
  float l = len(d);
  if (l < 1e-3f) return;
  Vec2 n = Vec2(-d.y, d.x) * (width * zoom * 0.5f / l);
  std::vector<SDL_Vertex> v = {vert(sa + n, c), vert(sa - n, c), vert(sb - n, c), vert(sb + n, c)};
  geometry(v, {0, 1, 2, 0, 2, 3}, additive);
}

void Gfx::ring(Vec2 w, float radius, float thick, Color c, int segs) {
  Vec2 s = toScreen(w);
  float ro = (radius + thick * 0.5f) * zoom, ri = std::max(0.0f, (radius - thick * 0.5f) * zoom);
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  for (int i = 0; i <= segs; i++) {
    Vec2 d = fromAngle(TAU * i / segs);
    v.push_back(vert(s + d * ro, c));
    v.push_back(vert(s + d * ri, c));
  }
  for (int i = 0; i < segs; i++) {
    int k = i * 2;
    idx.insert(idx.end(), {k, k + 1, k + 2, k + 1, k + 3, k + 2});
  }
  geometry(v, idx, true);
}

void Gfx::ribbon(const std::vector<Vec2>& pts, const std::vector<float>& width, const std::vector<Color>& col, bool additive) {
  size_t n = pts.size();
  if (n < 2) return;
  std::vector<SDL_Vertex> v;
  std::vector<int> idx;
  v.reserve(n * 2);
  for (size_t i = 0; i < n; i++) {
    Vec2 prev = pts[i > 0 ? i - 1 : 0], next = pts[i + 1 < n ? i + 1 : n - 1];
    Vec2 d = next - prev;
    float l = len(d);
    Vec2 nrm = l > 1e-4f ? Vec2(-d.y, d.x) * (1.0f / l) : Vec2(0, 1);
    Vec2 s = toScreen(pts[i]);
    float hw = width[i] * zoom * 0.5f;
    v.push_back(vert(s + nrm * hw, col[i]));
    v.push_back(vert(s - nrm * hw, col[i]));
  }
  for (size_t i = 0; i + 1 < n; i++) {
    int k = (int)i * 2;
    idx.insert(idx.end(), {k, k + 1, k + 2, k + 1, k + 3, k + 2});
  }
  geometry(v, idx, additive);
}

// Ear-clipping triangulation (polygon may be concave). Falls back to a fan if it stalls.
void Gfx::polyFill(const std::vector<Vec2>& poly, Color c, bool additive) {
  int n = (int)poly.size();
  if (n < 3) return;
  std::vector<SDL_Vertex> v;
  for (const Vec2& p : poly) v.push_back(vert(toScreen(p), c));
  float area = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) area += cross(poly[j], poly[i]);
  float sgn = area >= 0 ? 1.0f : -1.0f;
  std::vector<int> rem(n), idx;
  for (int i = 0; i < n; i++) rem[i] = i;
  int guard = 0;
  while (rem.size() > 3 && guard++ < 4 * n) {
    bool clipped = false;
    int m = (int)rem.size();
    for (int i = 0; i < m; i++) {
      int a = rem[(i + m - 1) % m], b = rem[i], cc = rem[(i + 1) % m];
      Vec2 A = poly[a], B = poly[b], C = poly[cc];
      if (cross(B - A, C - B) * sgn <= 0) continue;  // reflex vertex
      bool inside = false;
      for (int k = 0; k < m && !inside; k++) {
        int q = rem[k];
        if (q == a || q == b || q == cc) continue;
        Vec2 P = poly[q];
        float d1 = cross(B - A, P - A), d2 = cross(C - B, P - B), d3 = cross(A - C, P - C);
        bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
        if (!(neg && pos)) inside = true;
      }
      if (inside) continue;
      idx.insert(idx.end(), {a, b, cc});
      rem.erase(rem.begin() + i);
      clipped = true;
      break;
    }
    if (!clipped) break;
  }
  if (rem.size() == 3) idx.insert(idx.end(), {rem[0], rem[1], rem[2]});
  else for (size_t i = 1; i + 1 < rem.size(); i++) idx.insert(idx.end(), {rem[0], rem[(int)i], rem[(int)i + 1]});
  geometry(v, idx, additive);
}

void Gfx::rectW(Vec2 tl, Vec2 br, Color c, bool additive) {
  Vec2 a = toScreen(tl), b = toScreen(br);
  rectS(a.x, a.y, b.x - a.x, b.y - a.y, c, additive);
}

void Gfx::rectS(float x, float y, float w, float h, Color c, bool additive) {
  SDL_SetRenderDrawBlendMode(ren_, additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColorFloat(ren_, c.r, c.g, c.b, c.a);
  SDL_FRect r{x, y, w, h};
  SDL_RenderFillRect(ren_, &r);
}
void Gfx::frameS(float x, float y, float w, float h, float t, Color c) {
  rectS(x, y, w, t, c);
  rectS(x, y + h - t, w, t, c);
  rectS(x, y, t, h, c);
  rectS(x + w - t, y, t, h, c);
}

float Gfx::textWidth(const std::string& s, float scale) const { return (float)s.size() * 6.0f * scale - scale; }

void Gfx::text(float x, float y, const std::string& s, float scale, Color c, int align) {
  float w = textWidth(s, scale);
  if (align == 1) x -= w * 0.5f;
  else if (align == 2) x -= w;
  SDL_SetTextureColorModFloat(fontTex_, c.r, c.g, c.b);
  SDL_SetTextureAlphaModFloat(fontTex_, c.a);
  for (char ch : s) {
    int gi = glyphIndex(ch);
    if (gi >= 0) {
      SDL_FRect src{(float)((gi % kAtlasCols) * kCellW), (float)((gi / kAtlasCols) * kCellH), 5, 7};
      SDL_FRect dst{x, y, 5 * scale, 7 * scale};
      SDL_RenderTexture(ren_, fontTex_, &src, &dst);
    }
    x += 6 * scale;
  }
}
