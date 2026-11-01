// SpriteCache (sprite_cache.h): the caches' bookkeeping: the people textures' size, the night lift's masks, the pose
// bake budget, the v2 atlases' byte cap, the wall tiles' keys and the lanes' keyed sprites. The getters that paint stay
// beside their painters (render.cpp, render_anim.cpp, realm_render.cpp, party_hud.cpp, loot_ui.cpp ...).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>
#include "rpg/art/art_parts.h"
#include "rpg/view/sprite_cache.h"
#include "rpg/view/view.h"

// the v2 atlases past their byte cap: the least recently used (unused for 4 s or more) go, to three quarters of the cap
void SpriteCache::trimRigCaches(View& v) {
  double b = 0;
  for (const auto& kv : rigTex_) b += (double)kv.second.tex.w * kv.second.tex.h * 4;
  v.lensStats.rigMB = b / (1024.0 * 1024.0);
  if (v.lensStats.rigMB <= v.rigCapMB()) return;
  std::vector<std::pair<float, uint64_t>> age;
  for (const auto& kv : rigTex_) age.push_back({kv.second.used, kv.first});
  std::sort(age.begin(), age.end());
  for (const auto& a : age) {
    if (b <= v.rigCapMB() * 0.75 * 1024.0 * 1024.0 || v.t_ - a.first < 4.0f) break;
    auto it = rigTex_.find(a.second);
    if (it == rigTex_.end()) continue;
    b -= (double)it->second.tex.w * it->second.tex.h * 4;
    v.lensStats.rigEvictions++;
    if (rigEvicted_.size() > 4096) rigEvicted_.clear();
    rigEvicted_.insert(a.second);
    dropLiftMask(v, it->second.tex);   // (a later texture may reuse the pointer)
    v.pix_->destroy(it->second.tex);
    if (it->second.glowTex.t) v.pix_->destroy(it->second.glowTex);
    rigTex_.erase(it);
  }
  v.lensStats.rigMB = b / (1024.0 * 1024.0);
}

double SpriteCache::peopleMB() const {
  double b = 0;
  for (const auto& kv : humans_) b += (double)kv.second.w * kv.second.h * 4;
  for (const auto& kv : poseTex_) b += (double)kv.second.w * kv.second.h * 4;
  for (const auto& kv : rigTex_) b += (double)kv.second.tex.w * kv.second.tex.h * 4;
  for (const auto& kv : liftMask_) if (kv.second.tex.t) b += (double)kv.second.tex.w * kv.second.tex.h * 4;   // (P2a LENS) the night lift's masks
  return b / (1024.0 * 1024.0);
}

// (P2a LENS) the night lift's interior mask of a people sheet: set where the sheet is opaque and all four neighbours are
// too (inside the outer ink outline). Kept as bytes; drawNightLift bakes it the first night it is drawn.
void SpriteCache::liftMaskFor(View& v, const Canvas& sheet, const Tex& t) {
  if (!t.t || sheet.w <= 0) return;
  dropLiftMask(v, t);
  SpriteCache::LiftMask& lm = liftMask_[t.t];
  lm.w = sheet.w; lm.h = sheet.h;
  lm.m.assign((size_t)sheet.w * sheet.h, 0);
  const uint32_t* p = sheet.px.data();
  const int w = sheet.w, h = sheet.h;
  auto op = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && (p[(size_t)y * w + x] >> 24) >= 128; };
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if ((p[(size_t)y * w + x] >> 24) >= 128 && op(x - 1, y) && op(x + 1, y) && op(x, y - 1) && op(x, y + 1)) lm.m[(size_t)y * w + x] = 1;
}

void SpriteCache::dropLiftMask(View& v, const Tex& t) {
  auto it = liftMask_.find(t.t);
  if (it == liftMask_.end()) return;
  if (it->second.tex.t) v.pix_->destroy(it->second.tex);
  liftMask_.erase(it);
}

// (M5) a pose sheet (art::humanPostureSheet: POSTURE_FRAMES x 3 facings) by look and posture. A painted pose costs about
// a character sheet; a tavern filling up asks for a dozen at once, so a frame bakes poses only within ~2.5 ms (the web:
// 1.5 ms; at least one per frame) and returns nullptr for the rest, which stand on their standing sheet a frame or two.
bool SpriteCache::poseBudget(View& v) {
  if (v.witness) return true;   // (Witness) every pose a shot shows is baked inline: no picture depends on CPU speed
  if (poseFrameT_ != v.t_) { poseFrameT_ = v.t_; poseFrameMs_ = 0; }
#ifdef __EMSCRIPTEN__
  const double budget = 1.5;
#else
  static const double budget = std::getenv("EMB_WEBSIM") ? 1.5 : 2.5;
#endif
  return poseFrameMs_ < budget;
}

void SpriteCache::poseSpent(View& v, std::chrono::steady_clock::time_point t0) {
  poseFrameMs_ += std::max(0.05, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
  v.life_.m5Count_.poseBakes++;
}

// (M3b) a wall tile's texture key: the key itself (M3 look), or the key without its look slot mixed with the look's
// parts (the slot's meaning changes from window to window; the parts do not)
uint64_t SpriteCache::wallTexKey(uint32_t key) const {
  const uint32_t slot = (key & art::WALL_LOOK_MASK) >> art::WALL_LOOK_SHIFT;
  if (!slot || slot >= fortLooks_.size()) return key & ~art::WALL_LOOK_MASK;
  return (ew::mix64(((uint64_t)(key & ~art::WALL_LOOK_MASK) << 20) ^ bld::fortKey(fortLooks_[slot])) | (1ull << 63));
}

const Tex& SpriteCache::cachedTex(View& v, uint64_t key, Canvas (*paint)(uint64_t key)) {
  auto it = laneTex_.find(key);
  if (it != laneTex_.end()) return it->second;
  return laneTex_[key] = v.pix_->bake(paint(key));
}
