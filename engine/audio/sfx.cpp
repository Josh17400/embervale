// Every sound effect: its voice (Audio::fx<name>, one per SFX_LIST entry in engine/sound_ids.h, in the order the old
// switch had them), its trim and its name, all three generated from SFX_LIST; the spell motifs and the rise's stings.
#include "engine/audio/audio_internal.h"

// ---- (Witness 0.6 sound events) the names a listener hears (engine/audio.h setPlayListener): "HitHeavy" -> "hit_heavy"
namespace {
constexpr const char* kSfxIds[] = {
#define EMB_SFX_ID(name, trim) #name,
    SFX_LIST(EMB_SFX_ID)
#undef EMB_SFX_ID
};
static_assert(sizeof(kSfxIds) / sizeof(kSfxIds[0]) == (size_t)Sfx::COUNT, "a name for every Sfx");
struct SfxNames { char text[(int)Sfx::COUNT * 24] = {}; int at[(int)Sfx::COUNT] = {}; };
constexpr SfxNames snakeNames() {
  SfxNames r{};
  int o = 0;
  for (int i = 0; i < (int)Sfx::COUNT; i++) {
    r.at[i] = o;
    for (const char* c = kSfxIds[i]; *c; c++) {
      if (*c >= 'A' && *c <= 'Z') {
        if (c != kSfxIds[i]) r.text[o++] = '_';
        r.text[o++] = (char)(*c - 'A' + 'a');
      } else {
        r.text[o++] = *c;
      }
    }
    r.text[o++] = 0;
  }
  return r;
}
constexpr SfxNames kSfxNames = snakeNames();
}  // namespace
const char* Audio::sfxName(Sfx s) {
  return (int)s < (int)Sfx::COUNT ? kSfxNames.text + kSfxNames.at[(int)s] : "sfx";
}
const char* Audio::stingName(Sting s) {
  static const char* const n[] = {"sting_drum_roll", "sting_fanfare", "sting_horn_call", "sting_war_alarm", "sting_seal"};
  static_assert(sizeof(n) / sizeof(n[0]) == (size_t)Sting::COUNT, "a name for every Sting");
  return (int)s < (int)Sting::COUNT ? n[(int)s] : "sting";
}

// ------------------------------------------------------------------------------------------ sound effects
// Per-effect trim, calibrated with tools/audio_preview so single effects peak below the limiter (0.8 at
// master 0.6): impacts ~0.7, spells/fanfares ~0.45-0.65, pickups ~0.35, UI and footsteps deliberately soft.
const float kSfxTrim[(int)Sfx::COUNT] = {
#define EMB_SFX_TRIM(name, trim) trim,
    SFX_LIST(EMB_SFX_TRIM)
#undef EMB_SFX_TRIM
};
static_assert(sizeof(kSfxTrim) / sizeof(kSfxTrim[0]) == (size_t)Sfx::COUNT, "a trim for every Sfx");

void Audio::trigger(Sfx s, float k, float V, uint32_t arg) {
  static constexpr void (Audio::*kArm[])(float, float, uint32_t) = {
#define EMB_SFX_ARM(name, trim) &Audio::fx##name,
      SFX_LIST(EMB_SFX_ARM)
#undef EMB_SFX_ARM
  };
  static_assert(sizeof(kArm) / sizeof(kArm[0]) == (size_t)Sfx::COUNT, "a voice for every Sfx");
  V *= kSfxTrim[(int)s];
  if ((int)s < (int)Sfx::COUNT) (this->*kArm[(int)s])(k, V, arg);
}

// Bit identity under /fp:fast (MSVC only; the clang and web builds do not use fast-math): these voices were the arms of
// one 725-line switch, where MSVC left the calls out of line, so it never folded rnd()'s constants into the arm's own
// float code. Compiled as small functions they would round differently, so inlining is off over them (inline_depth 0),
// LevelUp builds /fp:precise and Cheer keeps the one call the switch did not inline (rndCall). tools/gates/audio_pack.sh
// and a per-voice sweep (every Sfx at 7 pitches x 3 volumes) prove the output identical.
#ifdef _MSC_VER
#pragma inline_depth(0)
#endif
// rnd(lo, hi), never inlined (MSVC): fxCheer's first draw, which the old switch called out of line
#ifdef _MSC_VER
__declspec(noinline)
#endif
float Audio::rndCall(float lo, float hi) { return rnd(lo, hi); }

void Audio::fxSwing(float k, float V, uint32_t arg) { // band-passed noise whose centre sweeps up and back down: a blade cutting air
  float r = k * rnd(0.9f, 1.12f);
  add(NB(Noise, 0, 2.0f * V).env(0.03f, 1, 1, 0.14f, 0.035f).bp(520 * r, 1.6f).bump(1.8f).color(0.8f));
  add(NB(Noise, 0, 0.05f * V).env(0.05f, 1, 1, 0.11f, 0.03f).hp(3800 * r).bump(0.6f));
  add(NB(Sine, 190 * r, 0.12f * V).env(0.02f, 1, 1, 0.1f, 0.03f).slide(-1.5f));
}
void Audio::fxHit(float k, float V, uint32_t arg) { // thump + crunch + crack
  float r = k * rnd(0.92f, 1.08f);
  add(NB(Sine, 118 * r, 0.85f * V).perc(0.085f).pitch(2.5f, 0.018f));
  add(NB(Noise, 0, 0.75f * V).perc(0.03f).lp(3000 * r, 0.9f));
  add(NB(Square, 82 * r, 0.30f * V).perc(0.06f).lp(900 * r, 1.2f).fenv(1.5f, 0.02f).drive(1.5f));
  add(NB(Noise, 0, 0.30f * V).perc(0.008f).hp(5000));
}
void Audio::fxHitHeavy(float k, float V, uint32_t arg) {
  float r = k * rnd(0.94f, 1.06f);
  add(NB(Sine, 74 * r, 0.95f * V).perc(0.2f).pitch(3, 0.03f).drive(0.5f));
  add(NB(Noise, 0, 0.85f * V).perc(0.07f).lp(2200 * r, 0.9f).fenv(1, 0.03f));
  add(NB(Saw, 52 * r, 0.45f * V).perc(0.13f).lp(600 * r).fenv(2, 0.03f).drive(2));
  add(NB(Noise, 0, 0.40f * V).perc(0.012f).hp(4000));
  add(NB(Noise, 0, 0.30f * V).env(0.01f, 0.22f).lp(380).send(0.3f).at(0.01f));
}
void Audio::fxBlock(float k, float V, uint32_t arg) { // inharmonic FM partials: steel on steel
  float r = k * rnd(0.97f, 1.03f);
  add(NB(Fm, 640 * r, 0.42f * V).perc(0.35f).fm(1.414f, 3.5f, 0.06f).send(0.3f));
  add(NB(Fm, 1490 * r, 0.28f * V).perc(0.22f).fm(2.76f, 2.0f, 0.05f).send(0.3f));
  add(NB(Sine, 2650 * r, 0.12f * V).perc(0.12f));
  add(NB(Noise, 0, 0.55f * V).perc(0.012f).hp(3000, 0.9f));
  add(NB(Sine, 180 * r, 0.35f * V).perc(0.04f).pitch(1, 0.01f));
}
void Audio::fxHurt(float k, float V, uint32_t arg) { // enemy: short formant grunt on a meaty thud
  float r = k * rnd(0.9f, 1.12f);
  add(NB(Saw, 230 * r, 0.42f * V).env(0.004f, 0.11f).slide(-1.6f).vox(900 * r, 2.5f).drive(1));
  add(NB(Noise, 0, 0.55f * V).perc(0.04f).lp(2000 * r));
  add(NB(Sine, 130 * r, 0.65f * V).perc(0.07f).pitch(1.5f, 0.015f));
}
void Audio::fxPlayerHurt(float k, float V, uint32_t arg) { // heavier thud + "oof" + a dissonant sting so the player always notices
  add(NB(Sine, 150 * k, 0.85f * V).perc(0.1f).pitch(2, 0.02f));
  add(NB(Noise, 0, 0.6f * V).perc(0.05f).lp(2600 * k));
  add(NB(Square, 300 * k, 0.30f * V).env(0.003f, 0.15f).slide(-1.2f).pw(0.3f).lp(1800 * k, 1.5f).drive(0.5f));
  add(NB(Tri, 932 * k, 0.17f * V).perc(0.14f).vib(0.3f, 14));
  add(NB(Tri, 988 * k, 0.17f * V).perc(0.14f).vib(0.3f, 14));
}
void Audio::fxEnemyDie(float k, float V, uint32_t arg) { // rattling groan falls away, then the body hits the ground
  add(NB(Saw, 200 * k, 0.42f * V).env(0.005f, 0.32f).slide(-1.1f).vox(800 * k, 2).vib(0.6f, 28).drive(1).send(0.2f));
  add(NB(Noise, 0, 0.40f * V).env(0.02f, 0.2f).lp(700 * k).send(0.2f));
  add(NB(Sine, 70 * k, 0.85f * V).perc(0.16f).pitch(1.5f, 0.03f).at(0.14f));
  add(NB(Noise, 0, 0.45f * V).perc(0.05f).lp(1500).at(0.14f));
}
void Audio::fxDeath(float k, float V, uint32_t arg) { // boom, a falling D-minor choir and a low bell toll
  add(NB(Sine, 55 * k, 1.0f * V).perc(0.9f).pitch(2, 0.05f));
  add(NB(Noise, 0, 0.5f * V).env(0.005f, 0.5f).lp(500 * k).fenv(2, 0.1f).send(0.3f));
  const float chord[4] = {146.8f, 174.6f, 220.0f, 293.7f};
  for (float c : chord)
    add(NB(Pad, c * k, 0.13f * V).detune(0.006f).env(0.06f, 1, 1, 1.3f, 0.9f).slide(-0.12f).vox(1200, 1.2f)
            .fslide(-0.8f).vib(0.15f, 4.5f).send(0.55f).at(0.05f));
  add(NB(Fm, 73.4f * k, 0.5f * V).perc(1.4f).fm(1.4f, 2.5f, 0.5f).send(0.5f).at(0.05f));
}
void Audio::fxArrow(float k, float V, uint32_t arg) { // bowstring twang, then the shaft whistling away
  add(NB(Pluck, 196 * k, 0.45f * V).bright(0.75f).ring(0.3f).env(0.001f, 1, 1, 0.25f, 0.06f));
  add(NB(Saw, 310 * k, 0.22f * V).perc(0.07f).slide(-1.5f).lp(2400 * k, 2));
  add(NB(Noise, 0, 0.80f * V).env(0.012f, 1, 1, 0.17f, 0.04f).bp(1800 * k, 1.5f).bump(1.2f).at(0.015f));
}
void Audio::fxArrowHit(float k, float V, uint32_t arg) { // thunk into wood / flesh, with a short shaft quiver
  float r = k * rnd(0.93f, 1.07f);
  add(NB(Sine, 210 * r, 0.75f * V).perc(0.045f).pitch(1.2f, 0.01f));
  add(NB(Noise, 0, 0.55f * V).perc(0.025f).lp(1600 * r));
  add(NB(Noise, 0, 1.6f * V).perc(0.07f).bp(760 * r, 6));
  add(NB(Tri, 420 * r, 0.10f * V).perc(0.12f).vib(1.5f, 32));
}
void Audio::fxFireball(float k, float V, uint32_t arg) { // roaring whoosh with a fiery rumble and crackle
  add(NB(Noise, 0, 1.0f * V).env(0.06f, 1, 1, 0.42f, 0.22f).lp(450 * k, 1.1f).bump(2.2f).color(0.6f).drive(0.6f).send(0.3f));
  add(NB(Saw, 65 * k, 0.35f * V).env(0.08f, 1, 1, 0.4f, 0.2f).slide(0.7f).lp(500, 0.9f).am(24, 0.5f).drive(1));
  add(NB(Noise, 0, 0.28f * V).env(0.03f, 1, 1, 0.35f, 0.15f).hp(2500).am(37, 0.8f));
}
void Audio::fxExplode(float k, float V, uint32_t arg) {
  add(NB(Sine, 58 * k, 1.0f * V).perc(0.4f).pitch(2.5f, 0.04f).drive(0.5f));
  add(NB(Noise, 0, 1.0f * V).env(0.002f, 0.45f).lp(350 * k, 0.8f).fenv(3.5f, 0.15f).drive(1).send(0.35f));
  add(NB(Noise, 0, 0.5f * V).perc(0.015f).hp(3000));
  add(NB(Noise, 0, 0.45f * V).env(0.05f, 0.8f).lp(250).send(0.5f).at(0.03f));
}
void Audio::fxFrost(float k, float V, uint32_t arg) { // cascade of glassy FM bells over an icy, shimmering hiss
  const float fr[6] = {2093, 2637, 3136, 3520, 2794, 3951};
  for (int i = 0; i < 6; i++)
    add(NB(Fm, fr[i] * k, 0.15f * V).perc(0.28f).fm(2.0f, 1.2f, 0.1f).send(0.55f).at(0.035f * (float)i));
  add(NB(Noise, 0, 0.16f * V).env(0.04f, 1, 1, 0.25f, 0.12f).hp(6000).am(19, 0.6f).send(0.5f));
  add(NB(Sine, 1046 * k, 0.15f * V).env(0.01f, 0.4f).vib(0.3f, 9));
}
void Audio::fxHeal(float k, float V, uint32_t arg) { // rising major-pentatonic sparkle over a soft swell
  const float fr[6] = {523.3f, 659.3f, 784.0f, 1046.5f, 1318.5f, 1568.0f};
  for (int i = 0; i < 6; i++) {
    add(NB(Sine, fr[i] * k, 0.17f * V).env(0.004f, 0.35f).vib(0.15f, 6).send(0.55f).at(0.055f * (float)i));
    add(NB(Sine, fr[i] * 2 * k, 0.05f * V).perc(0.2f).send(0.55f).at(0.055f * (float)i));
  }
  add(NB(Tri, 523.3f * k, 0.15f * V).env(0.08f, 1, 1, 0.35f, 0.25f).slide(0.6f).send(0.5f));
}
void Audio::fxRoll(float k, float V, uint32_t arg) { // cloth-and-dirt tumble with two soft bumps
  add(NB(Noise, 0, 0.75f * V).env(0.02f, 1, 1, 0.22f, 0.08f).lp(1300 * k, 0.8f).bump(0.9f).color(0.5f));
  add(NB(Sine, 95 * k, 0.40f * V).perc(0.05f).pitch(1, 0.01f).at(0.04f));
  add(NB(Sine, 85 * k, 0.32f * V).perc(0.05f).pitch(1, 0.01f).at(0.2f));
}
void Audio::fxHoof(float k, float V, uint32_t arg) { // (M7) a hoof on packed earth: a hollow tuned knock (the hoof wall), its overtone, grit, the weight
  float r = k * rnd(0.9f, 1.1f);
  add(NB(Sine, 380 * r, 0.42f * V).perc(0.035f).pitch(1.5f, 0.006f).send(0.04f));
  add(NB(Tri, 760 * r, 0.14f * V).perc(0.02f).send(0.04f));
  add(NB(Noise, 0, 0.34f * V).perc(0.022f).bp(1900 * r, 1.1f).send(0.03f));
  add(NB(Sine, 92 * r, 0.30f * V).perc(0.05f).pitch(0.8f, 0.01f));
}
void Audio::fxStep(float k, float V, uint32_t arg) { // very soft scuff + thud; randomised so repeats never sound mechanical
  float r = k * rnd(0.85f, 1.15f);
  add(NB(Noise, 0, 0.30f * V).perc(0.02f).bp(1500 * r, 0.8f).send(0.02f));            // grit: what phones hear
  add(NB(Noise, 0, 0.22f * V).perc(0.03f).lp(700 * r, 0.8f).send(0.02f));             // soft scuff
  add(NB(Sine, 105 * r, 0.16f * V).perc(0.025f).pitch(0.8f, 0.008f).send(0.02f));    // weight
}
void Audio::fxPickup(float k, float V, uint32_t arg) {
  add(NB(Sine, 620 * k, 0.35f * V).perc(0.07f).slide(2.5f));
  add(NB(Tri, 1318.5f * k, 0.22f * V).perc(0.12f).at(0.05f));
  add(NB(Tri, 1760 * k, 0.20f * V).perc(0.18f).at(0.1f).send(0.3f));
}
void Audio::fxCoin(float k, float V, uint32_t arg) { // classic two-note ding (B5 -> E6) with a metallic shimmer
  add(NB(Square, 987.8f * k, 0.15f * V).env(0.002f, 0.06f).lp(5000));
  add(NB(Square, 1318.5f * k, 0.15f * V).perc(0.3f).lp(6000).at(0.07f).send(0.25f));
  add(NB(Fm, 2637 * k, 0.10f * V).perc(0.15f).fm(3.5f, 1.5f, 0.05f).at(0.07f));
}
void Audio::fxChest(float k, float V, uint32_t arg) { // hinge creak, lid thunk, then the treasure jingles
  add(NB(Saw, 120 * k, 0.32f * V).env(0.04f, 1, 1, 0.32f, 0.08f).slide(0.5f).vox(900 * k, 4).am(26, 0.75f));
  add(NB(Sine, 90 * k, 0.6f * V).perc(0.12f).pitch(1, 0.02f).at(0.34f));
  add(NB(Noise, 0, 0.4f * V).perc(0.05f).lp(700).at(0.34f));
  const float jf[5] = {1568, 2093, 2349, 2637, 3136};
  for (int i = 0; i < 5; i++)
    add(NB(Fm, jf[i] * k, 0.12f * V).perc(0.2f).fm(3.5f, 1.2f, 0.06f).send(0.4f).at(0.4f + 0.05f * (float)i));
  const float cf[3] = {1046.5f, 1318.5f, 1568.0f};
  for (float c : cf) add(NB(Sine, c * k, 0.08f * V).env(0.01f, 0.6f).send(0.6f).at(0.58f));
}
void Audio::fxDoor(float k, float V, uint32_t arg) { // slow creak, then the heavy wooden thud
  add(NB(Saw, 90 * k, 0.32f * V).env(0.06f, 1, 1, 0.42f, 0.1f).slide(0.8f).vox(700 * k, 5).am(19, 0.7f).drive(0.5f));
  add(NB(Sine, 68 * k, 0.9f * V).perc(0.2f).pitch(1.5f, 0.02f).at(0.42f).send(0.3f));
  add(NB(Noise, 0, 0.5f * V).perc(0.08f).lp(500).at(0.42f).send(0.3f));
}
void Audio::fxBuy(float k, float V, uint32_t arg) { // coins tumbling into a purse + a bright "sold" ping
  for (int i = 0; i < 3; i++)
    add(NB(Fm, (2400 + 260 * (float)i) * k * rnd(0.97f, 1.03f), 0.13f * V).perc(0.09f).fm(3.5f, 1.6f, 0.04f)
            .at(0.045f * (float)i));
  add(NB(Square, 1568 * k, 0.12f * V).perc(0.25f).lp(5000).at(0.14f));
  add(NB(Fm, 2093 * k, 0.12f * V).perc(0.4f).fm(2, 1, 0.1f).at(0.14f).send(0.35f));
}
void Audio::fxTalk(float k, float V, uint32_t arg) { // soft vowel-ish blip; caller's pitch = speaker voice, small random inflection
  float f = 320 * k * rnd(0.94f, 1.08f);
  add(NB(Square, f, 0.15f * V).env(0.004f, 0.045f).pw(0.3f).vox(900 * rnd(0.85f, 1.2f), 1.8f).send(0.05f));
}
#ifdef _MSC_VER
#pragma float_control(precise, on, push)
#endif
void Audio::fxLevelUp(float k, float V, uint32_t arg) { // brass fanfare G-C-E-G up to a held C over a strings chord, timpani and sparkle
  const float run[4] = {392.0f, 523.3f, 659.3f, 784.0f};
  for (int i = 0; i < 4; i++) {
    float t = 0.11f * (float)i;
    add(NB(Saw, run[i] * k, 0.22f * V).env(0.01f, 0.3f, 0.6f, 0.09f, 0.06f).lp(1300, 1.1f).fenv(1.2f, 0.08f).at(t).send(0.3f));
  }
  add(NB(Saw, 1046.5f * k, 0.22f * V).env(0.02f, 0.4f, 0.7f, 0.85f, 0.35f).lp(1500, 1.1f).fenv(1, 0.15f)
          .vib(0.2f, 5.5f, 0.25f).at(0.44f).send(0.45f));
  add(NB(Square, 523.3f * k, 0.08f * V).env(0.02f, 0.4f, 0.7f, 0.85f, 0.35f).pw(0.3f).lp(2200).at(0.44f).send(0.4f));
  const float ch[3] = {261.6f, 329.6f, 392.0f};
  for (float c : ch) add(NB(Pad, c * k, 0.11f * V).detune(0.007f).lp(2000).env(0.05f, 1, 1, 0.9f, 0.5f).at(0.44f).send(0.5f));
  add(NB(Sine, 65.4f * k, 0.7f * V).perc(0.35f).pitch(0.3f, 0.04f));
  add(NB(Sine, 65.4f * k, 0.8f * V).perc(0.6f).pitch(0.3f, 0.04f).at(0.44f).send(0.3f));
  for (int i = 0; i < 4; i++)
    add(NB(Sine, 2093.0f * k * std::exp2((float)i * 4.0f / 12.0f), 0.06f * V).perc(0.3f).at(0.5f + 0.06f * (float)i).send(0.6f));
}
#ifdef _MSC_VER
#pragma float_control(pop)
#endif
void Audio::fxQuestStart(float k, float V, uint32_t arg) { // nordic horn call: D up to A, low horn doubling, frame drum
  add(NB(Saw, 293.7f * k, 0.25f * V).env(0.04f, 0.4f, 0.75f, 0.32f, 0.12f).lp(700, 1.1f).fenv(1.2f, 0.12f).send(0.5f));
  add(NB(Saw, 440.0f * k, 0.25f * V).env(0.05f, 0.5f, 0.75f, 0.75f, 0.35f).lp(800, 1.1f).fenv(1.2f, 0.15f)
          .vib(0.15f, 5, 0.3f).at(0.36f).send(0.55f));
  add(NB(Saw, 146.8f * k, 0.18f * V).env(0.06f, 0.5f, 0.8f, 1.05f, 0.4f).lp(450).send(0.5f));
  add(NB(Sine, 98 * k, 0.6f * V).perc(0.15f).pitch(0.9f, 0.02f));
  add(NB(Sine, 98 * k, 0.6f * V).perc(0.15f).pitch(0.9f, 0.02f).at(0.36f));
}
void Audio::fxQuestDone(float k, float V, uint32_t arg) { // "da-da-da DAAA": G-G-G then a full C-major brass chord, bells, timpani roll
  for (int i = 0; i < 3; i++)
    add(NB(Saw, 392.0f * k, 0.2f * V).env(0.008f, 0.2f, 0.6f, 0.08f, 0.05f).lp(1400, 1.1f).fenv(1, 0.06f)
            .at(0.12f * (float)i).send(0.3f));
  const float ch[3] = {523.3f, 659.3f, 784.0f};
  for (float c : ch)
    add(NB(Saw, c * k, 0.15f * V).env(0.03f, 0.5f, 0.75f, 1.0f, 0.5f).lp(1500, 1.0f).fenv(1, 0.15f)
            .vib(0.15f, 5.2f, 0.3f).at(0.36f).send(0.5f));
  add(NB(Pad, 130.8f * k, 0.15f * V).detune(0.006f).lp(900).env(0.05f, 1, 1, 1.0f, 0.6f).at(0.36f).send(0.4f));
  for (int i = 0; i < 6; i++)
    add(NB(Sine, 65.4f * k, (0.18f + 0.06f * (float)i) * V).perc(0.12f).pitch(0.3f, 0.03f).at(0.04f * (float)i));
  add(NB(Sine, 65.4f * k, 0.8f * V).perc(0.7f).pitch(0.3f, 0.04f).at(0.36f).send(0.3f));
  const float bells[4] = {1568, 2093, 2637, 3136};
  for (int i = 0; i < 4; i++) add(NB(Fm, bells[i] * k, 0.08f * V).perc(0.5f).fm(3, 1, 0.2f).at(0.45f + 0.08f * (float)i).send(0.6f));
}
void Audio::fxDiscover(float k, float V, uint32_t arg) { // majestic two-chord swell (Bb -> D major) with timpani and a high bell
  const float c1[4] = {116.5f, 174.6f, 233.1f, 293.7f};
  const float c2[4] = {146.8f, 220.0f, 293.7f, 370.0f};
  for (float c : c1) add(NB(Pad, c * k, 0.11f * V).detune(0.006f).vox(900, 1.2f).env(0.12f, 1, 1, 0.45f, 0.25f).send(0.6f));
  for (float c : c2)
    add(NB(Pad, c * k, 0.12f * V).detune(0.006f).vox(1000, 1.2f).env(0.15f, 1, 1, 1.5f, 0.9f).vib(0.1f, 4.8f, 0.5f)
            .at(0.55f).send(0.65f));
  add(NB(Saw, 293.7f * k, 0.14f * V).env(0.1f, 0.5f, 0.8f, 1.4f, 0.7f).lp(800, 1.1f).fenv(1, 0.2f).at(0.55f).send(0.55f));
  add(NB(Sine, 58.3f * k, 0.75f * V).perc(0.5f).pitch(0.3f, 0.05f).send(0.3f));
  add(NB(Sine, 73.4f * k, 0.9f * V).perc(0.8f).pitch(0.3f, 0.05f).at(0.55f).send(0.4f));
  add(NB(Fm, 587.3f * k, 0.12f * V).perc(1.2f).fm(3.5f, 1.4f, 0.4f).at(0.6f).send(0.7f));
  add(NB(Fm, 880.0f * k, 0.08f * V).perc(1.0f).fm(3.5f, 1.2f, 0.4f).at(0.72f).send(0.7f));
}
void Audio::fxMenuMove(float k, float V, uint32_t arg) {
  add(NB(Sine, 1100 * k, 0.16f * V).perc(0.03f));
  add(NB(Noise, 0, 0.06f * V).perc(0.004f).hp(4000));
}
void Audio::fxMenuSelect(float k, float V, uint32_t arg) {
  add(NB(Tri, 660 * k, 0.24f * V).perc(0.07f));
  add(NB(Tri, 990 * k, 0.24f * V).perc(0.12f).at(0.05f).send(0.15f));
  add(NB(Sine, 1980 * k, 0.06f * V).perc(0.1f).at(0.05f));
}
void Audio::fxMenuBack(float k, float V, uint32_t arg) {
  add(NB(Tri, 620 * k, 0.22f * V).perc(0.07f));
  add(NB(Tri, 415 * k, 0.22f * V).perc(0.1f).at(0.05f));
}
void Audio::fxRoar(float k, float V, uint32_t arg) { // dragon: growling saws with a rise-and-fall contour, rasping breath, sub
  add(NB(Saw, 52 * k, 0.5f * V).env(0.18f, 1, 1, 1.4f, 0.5f).vib(3, 0.35f).lp(420, 1.4f).bump(1.6f).am(27, 0.55f).drive(1.4f).send(0.45f));
  add(NB(Saw, 78.5f * k, 0.36f * V).env(0.2f, 1, 1, 1.35f, 0.5f).vib(3, 0.35f).lp(620, 1.2f).bump(1.4f).am(31, 0.5f).drive(1.2f).send(0.45f));
  add(NB(Noise, 0, 0.65f * V).env(0.15f, 1, 1, 1.3f, 0.5f).bp(480 * k, 1.1f).bump(1.5f).am(23, 0.4f).color(0.7f).send(0.45f));
  add(NB(Saw, 104 * k, 0.3f * V).env(0.22f, 1, 1, 1.3f, 0.45f).vib(3, 0.35f).vox(720 * k, 3).bump(0.8f).am(27, 0.6f).drive(1.5f).send(0.45f));  // throat
  add(NB(Sine, 38 * k, 0.45f * V).env(0.2f, 1, 1, 1.3f, 0.6f).vib(3, 0.35f));
}
void Audio::fxSplash(float k, float V, uint32_t arg) { // body of water, spray, then a few bubbles
  add(NB(Noise, 0, 0.9f * V).env(0.003f, 0.18f).lp(3500 * k, 0.7f).send(0.3f));
  add(NB(Noise, 0, 0.7f * V).env(0.02f, 0.35f).bp(1100 * k, 0.8f).at(0.02f).send(0.3f));
  for (int i = 0; i < 4; i++)
    add(NB(Sine, rnd(500, 1100) * k, 0.18f * V).perc(0.05f).slide(2.5f).at(0.08f + 0.06f * (float)i + rnd(0, 0.03f)));
}
void Audio::fxBell(float k, float V, uint32_t arg) { // bronze alarm bell, struck twice: inharmonic partials (hum, prime, minor-third tierce,
                      // quint, nominal) ringing out at different rates, a clank at the strike and a metallic shimmer
  const float f0 = 392.0f * k;
  static const float part[7][3] = {   // ratio, level, decay (s)
      {0.5f, 0.30f, 2.6f}, {1.0f, 0.34f, 1.8f}, {1.19f, 0.22f, 1.3f}, {1.5f, 0.12f, 1.0f}, {2.0f, 0.20f, 0.9f}, {2.52f, 0.08f, 0.5f}, {3.0f, 0.06f, 0.35f}};
  for (int hit = 0; hit < 2; hit++) {
    float d = hit * 0.42f, a = hit ? 0.8f : 1.0f;
    for (auto& pt : part) add(NB(Sine, f0 * pt[0], pt[1] * a * V).perc(pt[2]).at(d).send(0.45f));
    add(NB(Fm, f0 * 2.0f, 0.07f * a * V).perc(0.6f).fm(2.76f, 2.2f, 0.25f).at(d).send(0.5f));
    add(NB(Noise, 0, 0.35f * a * V).perc(0.025f).bp(2600 * k, 1.4f).at(d));
    add(NB(Sine, f0 * 0.25f, 0.18f * a * V).perc(0.08f).pitch(0.6f, 0.02f).at(d));
  }
}
// M5 (AMBIENCE lane): the village's animals and the tavern's crowd
void Audio::fxBark(float k, float V, uint32_t arg) { // a dog: "rruf-ruf": a rough, chesty buzz through an open-mouth formant, the pitch falling off
  const float r = k * rnd(0.92f, 1.08f);
  const int n = rnd() < 0.35f ? 3 : 2;
  for (int i = 0; i < n; i++) {
    const float d = 0.19f * (float)i + rnd(0, 0.02f), a = i == 0 ? 1.0f : 0.8f, f = (i == 0 ? 330.0f : 300.0f) * r;
    add(NB(Saw, f, 0.42f * a * V).env(0.006f, 0.07f, 0.0f, 0.06f, 0.04f).vox(760 * r, 2.4f).fenv(0.9f, 0.05f).pitch(0.5f, 0.02f).slide(-1.4f).drive(1.2f).at(d).send(0.18f));
    add(NB(Noise, 0, 0.30f * a * V).env(0.003f, 0.05f).bp(1500 * r, 1.2f).color(0.3f).at(d));   // the breath's rasp
    add(NB(Sine, f * 0.5f, 0.22f * a * V).env(0.004f, 0.06f).slide(-1.0f).at(d));                  // the chest
  }
}
void Audio::fxCluck(float k, float V, uint32_t arg) { // a hen: three or four quick throaty "buk"s, the last drawn out and rising ("buk-buk-bu-KAAK")
  const float r = k * rnd(0.94f, 1.06f);
  const int n = 3 + (rnd() < 0.5f ? 1 : 0);
  float d = 0;
  for (int i = 0; i < n; i++) {
    const bool last = i == n - 1 && rnd() < 0.6f;
    const float f = (last ? 760.0f : 560.0f + rnd(-40, 40)) * r;
    add(NB(Square, f, (last ? 0.30f : 0.24f) * V).pw(0.3f).env(0.003f, last ? 0.16f : 0.04f, 0.0f, last ? 0.12f : 0.03f, 0.03f)
            .vox(1250 * r, 3.0f).pitch(last ? -0.3f : 0.6f, 0.02f).slide(last ? 0.5f : -0.8f).at(d));
    add(NB(Noise, 0, 0.12f * V).perc(0.015f).bp(2200 * r, 2.0f).at(d));   // the beak's click
    d += last ? 0.2f : 0.085f + rnd(0, 0.03f);
  }
}
void Audio::fxMeow(float k, float V, uint32_t arg) { // a cat: "mi-aow": the voice rises then falls while the mouth opens (the formant sweeps up and back)
  const float r = k * rnd(0.9f, 1.12f);
  add(NB(Saw, 560 * r, 0.30f * V).env(0.05f, 0.5f, 0.6f, 0.42f, 0.12f).vox(900 * r, 4.0f).bump(0.9f).vib(0.35f, 6.5f, 0.12f)
          .pitch(0.25f, 0.18f).slide(-0.7f).send(0.2f));
  add(NB(Noise, 0, 0.05f * V).env(0.05f, 0.4f).bp(3200 * r, 1.5f).at(0.02f));   // breath
}
#ifdef _MSC_VER
#pragma inline_depth()
#endif
void Audio::fxCheer(float k, float V, uint32_t arg) { // a room's cheer: a dozen voices shouting "hey!" a little apart, a wash of breath, clapping
  const float r = k * rndCall(0.96f, 1.04f);
  for (int i = 0; i < 9; i++) {
    const float f = rnd(150, 300) * r, d = rnd(0, 0.18f), len = rnd(0.25f, 0.6f);
    add(NB(Saw, f, 0.075f * V).env(0.03f, len, 0.4f, len * 0.7f, 0.15f).vox(rnd(650, 1100) * r, 2.0f).pitch(0.35f, 0.08f).slide(-0.25f).vib(0.3f, 5.5f).at(d).send(0.35f));
  }
  add(NB(Noise, 0, 0.32f * V).env(0.06f, 0.7f).bp(1300 * r, 0.7f).am(7, 0.4f).send(0.4f));
  for (int i = 0; i < 10; i++) add(NB(Noise, 0, rnd(0.16f, 0.26f) * V).perc(0.025f).bp(rnd(1100, 1900), 1.3f).at(0.25f + 0.12f * (float)i + rnd(0, 0.06f)).send(0.3f));
}
#ifdef _MSC_VER
#pragma inline_depth(0)
#endif
// M6 Steel (BEASTS lane): the new families and the ranked foes
void Audio::fxHarpyShriek(float k, float V, uint32_t arg) { // a raking scream: two buzzing voices through a high, shifting formant, wavering fast,
                           // the pitch flung up then falling away, with a breathy rasp
  const float r = k * rnd(0.94f, 1.06f);
  add(NB(Saw, 860 * r, 0.26f * V).env(0.015f, 0.32f, 0.35f, 0.38f, 0.14f).vox(2300 * r, 3.2f).fenv(0.5f, 0.2f).vib(1.4f, 11.5f).pitch(0.55f, 0.07f).slide(-0.85f).drive(1.3f).send(0.35f));
  add(NB(Square, 1290 * r, 0.11f * V).pw(0.22f).env(0.02f, 0.3f, 0.3f, 0.34f, 0.12f).vox(3200 * r, 4.0f).vib(1.8f, 13.0f).pitch(0.4f, 0.06f).slide(-1.0f).send(0.35f));
  add(NB(Noise, 0, 0.22f * V).env(0.01f, 0.36f).bp(3300 * r, 1.8f).am(23, 0.5f).color(0.2f).send(0.3f));
  add(NB(Saw, 430 * r, 0.10f * V).env(0.02f, 0.25f).vox(1100 * r, 2.0f).slide(-0.6f).at(0.02f));
}
void Audio::fxGolemSlam(float k, float V, uint32_t arg) { // stone fists into the ground: a deep thump falling in pitch, a crunch of grinding rock,
                           // the ground's rumble, then rubble pattering down
  add(NB(Sine, 64 * k, 0.75f * V).env(0.002f, 0.45f).pitch(1.4f, 0.05f).slide(-0.7f));
  add(NB(Noise, 0, 0.85f * V).env(0.003f, 0.28f).lp(760 * k, 0.9f).send(0.35f));
  add(NB(Noise, 0, 0.9f * V).env(0.004f, 0.16f).bp(1300 * k, 1.1f).color(0.3f));   // (integration: the crunch carries on a phone)
  add(NB(Saw, 46 * k, 0.30f * V).env(0.01f, 0.35f).lp(240, 1.0f).drive(1.6f));
  add(NB(Sine, 34 * k, 0.3f * V).env(0.06f, 0.9f).send(0.3f));
  for (int i = 0; i < 7; i++)
    add(NB(Noise, 0, rnd(0.10f, 0.22f) * V).perc(0.03f).bp(rnd(1400, 3600) * k, 2.2f).at(0.10f + 0.055f * (float)i + rnd(0, 0.03f)).send(0.25f));
}
void Audio::fxEliteSting(float k, float V, uint32_t arg) { // a dissonant swell (a minor second, low and dark) under a cold bell: something stronger is here
  add(NB(Pad, 110 * k, 0.20f * V).env(0.22f, 0.7f, 0.0f, 0.55f, 0.4f).lp(900 * k, 1.3f).fenv(0.6f, 0.4f).send(0.5f));
  add(NB(Pad, 116.5f * k, 0.17f * V).env(0.26f, 0.7f, 0.0f, 0.55f, 0.4f).lp(820 * k, 1.2f).send(0.5f));
  add(NB(Sine, 55 * k, 0.30f * V).env(0.2f, 0.8f).send(0.3f));
  add(NB(Fm, 880 * k, 0.11f * V).perc(1.3f).fm(3.5f, 3.0f, 0.4f).at(0.14f).send(0.6f));
  add(NB(Fm, 932 * k, 0.06f * V).perc(1.1f).fm(2.76f, 2.0f, 0.3f).at(0.16f).send(0.6f));
  add(NB(Noise, 0, 0.16f * V).env(0.3f, 0.4f).bp(1300 * k, 0.8f).send(0.5f));
}
void Audio::fxBossRoar(float k, float V, uint32_t arg) { // a world boss: deep growling saws rising and falling, a rasping wall of breath, a sub that
                           // drops away at the end (longer and lower than the dragon's Roar)
  add(NB(Saw, 41 * k, 0.5f * V).env(0.22f, 1, 1, 1.8f, 0.7f).vib(3.5f, 0.3f).lp(380, 1.4f).bump(1.7f).am(24, 0.55f).drive(1.6f).slide(-0.15f).send(0.5f));
  add(NB(Saw, 61.5f * k, 0.36f * V).env(0.25f, 1, 1, 1.75f, 0.7f).vib(3.5f, 0.3f).lp(560, 1.2f).bump(1.5f).am(29, 0.5f).drive(1.3f).send(0.5f));
  add(NB(Saw, 83 * k, 0.42f * V).env(0.28f, 1, 1, 1.7f, 0.6f).vib(3.0f, 0.35f).vox(640 * k, 3).bump(0.9f).am(24, 0.6f).drive(1.6f).send(0.5f));
  add(NB(Noise, 0, 0.8f * V).env(0.18f, 1, 1, 1.7f, 0.7f).bp(620 * k, 1.0f).bump(1.6f).am(19, 0.45f).color(0.75f).send(0.5f));
  add(NB(Sine, 30 * k, 0.38f * V).env(0.25f, 1, 1, 1.7f, 0.9f).vib(3, 0.3f).slide(-0.3f));
}
// M8 Fellowship (ART lane)
void Audio::fxRevive(float k, float V, uint32_t arg) { // a friend's wounds bound: linen torn and wound round (two swishes), drawn tight with a creak,
                      // a caught breath as they come round, then a warm major swell rising under a soft shimmer
  const float r = k * rnd(0.96f, 1.04f);
  add(NB(Noise, 0, 0.55f * V).env(0.02f, 1, 1, 0.12f, 0.05f).bp(2300 * r, 1.4f).bump(0.8f).color(0.5f));            // the tear
  add(NB(Noise, 0, 0.42f * V).env(0.03f, 1, 1, 0.16f, 0.06f).bp(1500 * r, 1.2f).bump(-0.6f).color(0.6f).at(0.16f));  // wound round
  add(NB(Noise, 0, 0.50f * V).env(0.01f, 1, 1, 0.07f, 0.03f).bp(3100 * r, 2.4f).at(0.34f));                           // pulled tight
  add(NB(Saw, 190 * r, 0.10f * V).env(0.005f, 0.06f).vox(1200 * r, 3.0f).slide(0.6f).at(0.34f));                     // the knot's creak
  add(NB(Noise, 0, 0.36f * V).env(0.07f, 1, 1, 0.16f, 0.12f).vox(1100 * r, 2.2f).bump(0.7f).color(0.4f).at(0.46f));   // the caught breath
  const float ch[4] = {261.6f, 329.6f, 392.0f, 523.3f};
  for (int i = 0; i < 4; i++)
    add(NB(Pad, ch[i] * r, 0.11f * V).detune(0.005f).env(0.35f, 1, 1, 0.45f, 0.7f).slide(0.08f).lp(1800 * r, 0.9f).fslide(0.8f).send(0.5f).at(0.55f + 0.06f * (float)i));
  add(NB(Sine, 1046.5f * r, 0.06f * V).env(0.2f, 1, 1, 0.3f, 0.5f).vib(0.2f, 6).send(0.6f).at(0.8f));
  add(NB(Sine, 1568.0f * r, 0.04f * V).env(0.25f, 1, 1, 0.25f, 0.5f).vib(0.2f, 7).send(0.6f).at(0.9f));
}
void Audio::fxKnell(float k, float V, uint32_t arg) { // a companion has died: one slow, low funeral bell (its partials ringing out long, the hum last)
                      // over a hushed choir that breathes in after the strike and fades with the bell
  const float f0 = 110.0f * k;
  static const float part[7][3] = {   // ratio, level, decay (s)
      {0.5f, 0.34f, 5.5f}, {1.0f, 0.30f, 4.0f}, {1.19f, 0.18f, 3.0f}, {1.5f, 0.10f, 2.2f}, {2.0f, 0.16f, 2.0f}, {2.52f, 0.06f, 1.0f}, {3.0f, 0.04f, 0.6f}};
  for (auto& pt : part) add(NB(Sine, f0 * pt[0], pt[1] * V).perc(pt[2]).send(0.55f));
  add(NB(Fm, f0 * 2.0f, 0.06f * V).perc(1.6f).fm(2.76f, 2.0f, 0.6f).send(0.6f));
  add(NB(Noise, 0, 0.22f * V).perc(0.03f).bp(1500 * k, 1.2f));                 // the clapper's strike, muffled
  add(NB(Sine, f0 * 0.25f, 0.20f * V).perc(0.12f).pitch(0.5f, 0.03f));
  const float choir[4] = {146.8f, 174.6f, 220.0f, 293.7f};   // D minor, low and soft
  for (int i = 0; i < 4; i++)
    add(NB(Pad, choir[i] * k, 0.07f * V).detune(0.007f).env(0.9f, 1, 1, 1.6f, 1.8f).vox(i < 2 ? 650.0f : 900.0f, 1.3f).vib(0.12f, 4.6f, 0.6f)
            .send(0.65f).at(0.25f + 0.08f * (float)i));
  add(NB(Noise, 0, 0.05f * V).env(0.9f, 1, 1, 1.4f, 1.6f).bp(900, 0.7f).send(0.6f).at(0.3f));   // the breath of the voices
}
void Audio::fxHowl(float k, float V, uint32_t arg) { // the bonded hound's or wolf's howl: a breathy "oo" voice rising into a long wavering call that
                      // sinks away at its end; k is the pitch (the view raises it for a pup, whose howl breaks up high)
  const float r = k * rnd(0.97f, 1.03f);
  const bool young = k > 1.25f;
  add(NB(Saw, 330 * r, 0.30f * V).env(0.12f, 1, 1, 0.5f, 0.06f).vox(700 * r, 3.0f).slide(0.6f).vib(0.15f, 5.0f).send(0.45f));          // the rise
  add(NB(Saw, 500 * r, 0.32f * V).env(0.04f, 1, 1, young ? 0.5f : 1.0f, 0.35f).vox(820 * r, 3.0f).vib(0.35f, 5.5f, 0.15f).slide(-0.5f)
          .fslide(-0.4f).send(0.5f).at(0.48f));                                                                                     // the long call
  add(NB(Sine, 1000 * r, 0.07f * V).env(0.05f, 1, 1, young ? 0.45f : 0.9f, 0.3f).vib(0.35f, 5.5f, 0.15f).slide(-0.5f).send(0.5f).at(0.5f));   // its ring
  add(NB(Noise, 0, 0.12f * V).env(0.1f, 1, 1, young ? 0.9f : 1.4f, 0.3f).bp(900 * r, 1.4f).am(5.5f, 0.3f).send(0.4f));                // breath
  if (young) add(NB(Saw, 620 * r, 0.16f * V).env(0.02f, 0.18f).vox(950 * r, 3.0f).slide(0.4f).at(1.0f).send(0.4f));                 // the voice breaking
}
// M8b Treasure (POWERS lane)
void Audio::fxZap(float k, float V, uint32_t arg) { // chain lightning / a star's strike: a white crack, a buzzing arc that snaps downward, a
                      // crackling sizzle that dies off, and a faint thunder roll under it
  const float r = k * rnd(0.94f, 1.06f);
  add(NB(Noise, 0, 0.85f * V).perc(0.012f).hp(2600 * r, 0.8f));                                              // the crack
  add(NB(Saw, 1650 * r, 0.20f * V).env(0.002f, 0.16f).slide(-2.2f).am(90, 0.8f).bp(2400 * r, 1.4f).drive(1.4f).send(0.35f));   // the arc
  add(NB(Square, 220 * r, 0.12f * V).env(0.003f, 0.14f).am(120, 0.9f).lp(3000 * r, 1.2f).drive(1.2f));       // its buzz
  add(NB(Noise, 0, 0.32f * V).env(0.005f, 1, 1, 0.12f, 0.12f).hp(5200).am(43, 0.9f).send(0.4f).at(0.02f));   // the sizzle
  add(NB(Noise, 0, 0.28f * V).env(0.03f, 1, 1, 0.12f, 0.45f).lp(260, 0.9f).send(0.5f).at(0.04f));            // the far roll
  add(NB(Sine, 90 * r, 0.30f * V).perc(0.09f).pitch(2.0f, 0.02f));
}
void Audio::fxRebirth(float k, float V, uint32_t arg) { // the phoenix: a deep whoomph, a roaring rush of fire rising in pitch, crackle, then a bright
                      // major chord blooming over it with a high shimmer (death refused)
  const float r = k * rnd(0.98f, 1.02f);
  add(NB(Sine, 55 * r, 0.85f * V).perc(0.45f).pitch(0.4f, 0.06f).drive(0.4f));                                // the whoomph
  add(NB(Noise, 0, 0.55f * V).env(0.05f, 1, 1, 0.55f, 0.35f).bp(500 * r, 0.9f).fslide(1.6f).bump(1.2f).color(0.7f).send(0.4f));   // the rush
  add(NB(Noise, 0, 0.22f * V).env(0.02f, 1, 1, 0.75f, 0.3f).hp(3500).am(23, 0.85f).send(0.3f));               // the crackle
  const float ch[4] = {293.7f, 370.0f, 440.0f, 587.3f};   // D major
  for (int i = 0; i < 4; i++)
    add(NB(Saw, ch[i] * r, 0.07f * V).detune(0.006f).env(0.08f, 1, 1, 0.7f, 0.6f).lp(1500 * r, 1.1f).fenv(1.2f, 0.25f)
            .slide(0.05f).send(0.55f).at(0.32f + 0.05f * (float)i));
  add(NB(Pad, 146.8f * r, 0.12f * V).detune(0.006f).env(0.12f, 1, 1, 0.8f, 0.7f).lp(1200).send(0.55f).at(0.32f));
  add(NB(Fm, 1174.7f * r, 0.08f * V).perc(1.0f).fm(3.0f, 1.4f, 0.4f).send(0.7f).at(0.5f));
  add(NB(Sine, 1760.0f * r, 0.05f * V).env(0.1f, 1, 1, 0.4f, 0.5f).vib(0.3f, 7).send(0.7f).at(0.6f));
}
void Audio::fxLootChime(float k, float V, uint32_t arg) { // a fine piece found: a glassy rising arpeggio of bell partials (k the pitch: the VIEW lane
                         // raises it by rarity), a fifth note for the brightest finds (k >= 1.2), a soft air shimmer
  const float r = k;
  const float run[5] = {659.3f, 830.6f, 987.8f, 1318.5f, 1661.2f};   // E major, rising
  const int n = k >= 1.2f ? 5 : 4;
  for (int i = 0; i < n; i++) {
    const float t = 0.07f * (float)i;
    add(NB(Fm, run[i] * r, 0.12f * V).perc(0.55f + 0.1f * (float)i).fm(3.5f, 1.1f, 0.25f).send(0.65f).at(t));    // glass
    add(NB(Sine, run[i] * 2.0f * r, 0.04f * V).perc(0.3f).send(0.7f).at(t + 0.004f));                         // its ring
  }
  add(NB(Noise, 0, 0.05f * V).env(0.15f, 1, 1, 0.25f, 0.3f).hp(7000).send(0.6f));
}
// ---- M9 Arcana & Mastery (SPELLFX lane): the spells' voices, one family per school / essence, played by the view
//      from the Ev::Spell phases (render_magic.cpp). k: the pitch (the school's register, lower for bigger spells)
void Audio::fxSpellHum(float k, float V, uint32_t arg) { // the wind-up: a breathy hum swelling under a glassy shimmer (a bed under the cast)
  const float r = k * rnd(0.98f, 1.02f);
  add(NB(Pad, 196 * r, 0.10f * V).detune(0.009f).env(0.22f, 1, 1, 0.5f, 0.3f).lp(800 * r, 1.3f).fslide(1.1f).vib(0.2f, 5).send(0.5f));
  add(NB(Sine, 392 * r, 0.09f * V).env(0.25f, 1, 1, 0.45f, 0.3f).vib(0.35f, 6).am(9, 0.3f).send(0.4f));
  add(NB(Noise, 0, 0.16f * V).env(0.28f, 1, 1, 0.45f, 0.25f).bp(1100 * r, 2.2f).fslide(1.3f).color(0.5f).send(0.4f));
  add(NB(Fm, 1568 * r, 0.035f * V).env(0.35f, 1, 1, 0.4f, 0.35f).fm(2.01f, 0.8f, 1.0f).vib(0.3f, 7).send(0.6f));
}
void Audio::fxSpellCharge(float k, float V, uint32_t arg) { // an upcast step: a rising swell, then a bright tick as the level locks in
  const float r = k;
  add(NB(Saw, 220 * r, 0.12f * V).detune(0.01f).env(0.02f, 1, 1, 0.2f, 0.08f).slide(1.3f).lp(1400 * r, 2.0f).fenv(1.0f, 0.2f).send(0.4f));
  add(NB(Noise, 0, 0.14f * V).env(0.14f, 1, 1, 0.12f, 0.05f).hp(2800 * r).fslide(1.0f).send(0.3f));
  add(NB(Fm, 1760 * r, 0.13f * V).perc(0.22f).fm(3.0f, 1.6f, 0.12f).send(0.55f).at(0.2f));
  add(NB(Sine, 880 * r, 0.10f * V).perc(0.3f).send(0.5f).at(0.2f));
}
void Audio::fxSpellFizzle(float k, float V, uint32_t arg) { // a damp pfft and a falling sputter: the magic would not come
  add(NB(Noise, 0, 0.55f * V).env(0.004f, 0.16f).bp(950 * k, 1.2f).fslide(-1.6f).color(0.4f));
  add(NB(Square, 330 * k, 0.09f * V).env(0.002f, 0.14f).slide(-2.0f).am(32, 0.9f).lp(1300 * k));
  add(NB(Sine, 240 * k, 0.16f * V).env(0.003f, 0.12f).slide(-1.2f));
  add(NB(Noise, 0, 0.12f * V).env(0.02f, 0.2f).hp(4000).am(17, 0.8f).at(0.05f));
}
void Audio::fxFireCast(float k, float V, uint32_t arg) { // a roaring whoosh, a throaty rumble and crackle as the fire leaves the hands
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Noise, 0, 1.0f * V).env(0.025f, 1, 1, 0.26f, 0.22f).lp(520 * r, 1.2f).bump(2.4f).color(0.55f).drive(0.7f).send(0.3f));
  add(NB(Noise, 0, 0.35f * V).env(0.01f, 1, 1, 0.18f, 0.12f).bp(900 * r, 1.4f).fslide(1.0f).send(0.25f));
  add(NB(Saw, 72 * r, 0.32f * V).env(0.03f, 1, 1, 0.26f, 0.2f).slide(0.8f).lp(520, 0.9f).am(26, 0.5f).drive(1.2f));
  add(NB(Noise, 0, 0.26f * V).env(0.02f, 1, 1, 0.3f, 0.15f).hp(2600).am(41, 0.85f));
}
void Audio::fxFireBurst(float k, float V, uint32_t arg) { // a deep whump, a boiling roar, crackle and pattering debris
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Sine, 52 * r, 1.0f * V).perc(0.45f).pitch(2.6f, 0.04f).drive(0.6f));
  add(NB(Noise, 0, 0.95f * V).env(0.002f, 0.55f).lp(320 * r, 0.8f).fenv(3.6f, 0.14f).drive(1.2f).send(0.4f));
  add(NB(Noise, 0, 0.42f * V).env(0.04f, 1, 1, 0.35f, 0.5f).lp(650 * r, 1.0f).am(13, 0.5f).send(0.45f).at(0.03f));
  add(NB(Noise, 0, 0.24f * V).env(0.04f, 1, 1, 0.45f, 0.3f).hp(3000).am(47, 0.9f).at(0.05f));
  for (int i = 0; i < 3; i++)
    add(NB(Noise, 0, 0.22f * V).perc(0.02f).bp(rnd(1600, 2800), 2.5f).at(rnd(0.12f, 0.45f)));
}
void Audio::fxFrostCast(float k, float V, uint32_t arg) { // a crisp crystalline zing and a breath of cold air
  const float r = k * rnd(0.97f, 1.03f);
  add(NB(Fm, 2637 * r, 0.15f * V).perc(0.35f).fm(1.5f, 2.0f, 0.15f).send(0.5f));
  add(NB(Fm, 3520 * r, 0.08f * V).perc(0.25f).fm(2.7f, 1.4f, 0.1f).send(0.55f).at(0.03f));
  add(NB(Saw, 1320 * r, 0.07f * V).env(0.004f, 0.12f).slide(0.6f).hp(1500));
  add(NB(Noise, 0, 0.22f * V).env(0.03f, 1, 1, 0.22f, 0.2f).hp(5200).am(15, 0.4f).send(0.45f));
  add(NB(Sine, 3951 * r, 0.06f * V).perc(0.09f).at(0.02f));
}
void Audio::fxFrostShatter(float k, float V, uint32_t arg) { // glass shattering, tinkling shards and an icy hiss over a dull crack
  const float r = k;
  add(NB(Noise, 0, 0.75f * V).perc(0.012f).hp(2800));
  add(NB(Sine, 160 * r, 0.42f * V).perc(0.07f).pitch(1.2f, 0.01f));
  for (int i = 0; i < 7; i++)
    add(NB(Fm, rnd(2200, 4400) * r, 0.11f * V).perc(rnd(0.12f, 0.3f)).fm(2.7f, 2.0f, 0.08f).send(0.55f).at(rnd(0.0f, 0.12f)));
  for (int i = 0; i < 5; i++)
    add(NB(Sine, rnd(3000, 6000) * r, 0.05f * V).perc(0.05f).send(0.6f).at(rnd(0.06f, 0.38f)));
  add(NB(Noise, 0, 0.16f * V).env(0.01f, 1, 1, 0.25f, 0.3f).hp(6000).send(0.5f));
}
void Audio::fxStormCast(float k, float V, uint32_t arg) { // a bright electric snap and a buzzing arc
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Noise, 0, 0.8f * V).perc(0.009f).hp(3000 * r));
  add(NB(Square, 110 * r, 0.15f * V).env(0.002f, 0.2f).am(100, 0.9f).lp(2600 * r, 1.2f).drive(1.5f));
  add(NB(Saw, 2200 * r, 0.16f * V).env(0.002f, 0.11f).slide(-1.5f).bp(3000 * r, 1.4f).drive(1.0f).send(0.3f));
  add(NB(Noise, 0, 0.22f * V).env(0.004f, 1, 1, 0.1f, 0.1f).hp(6000).am(60, 0.9f).send(0.35f).at(0.01f));
}
// ---- M10 Beyond the Mist: the sea's moments
void Audio::fxWaveCrash(float k, float V, uint32_t arg) { // a wave breaking: a low thump of water, the crash's rush, the spray's hiss falling away
  const float r = k * rnd(0.9f, 1.1f);
  add(NB(Sine, 70 * r, 0.6f * V).perc(0.25f).pitch(1.2f, 0.04f));
  add(NB(Noise, 0, 0.9f * V).env(0.02f, 0.5f).lp(900 * r, 0.8f).fenv(1.5f, 0.15f).send(0.35f));
  add(NB(Noise, 0, 0.45f * V).env(0.08f, 1, 1, 0.3f, 0.9f).hp(2600 * r).send(0.3f).at(0.05f));
}
void Audio::fxSailFlap(float k, float V, uint32_t arg) { // canvas cracking full: quick flaps and a taut, low thrum
  const float r = k * rnd(0.92f, 1.08f);
  for (int i = 0; i < 3; i++) add(NB(Noise, 0, (0.55f - 0.12f * i) * V).perc(0.045f).bp(420 * r, 1.4f).at(0.07f * (float)i + rnd(0, 0.02f)));
  add(NB(Saw, 62 * r, 0.12f * V).env(0.02f, 0.4f).lp(260 * r).at(0.18f));
}
void Audio::fxOarStroke(float k, float V, uint32_t arg) { // the blade dipping (a plunge), the pull through the water, the lock's wooden knock
  const float r = k * rnd(0.9f, 1.1f);
  add(NB(Noise, 0, 0.5f * V).env(0.005f, 0.09f).bp(900 * r, 0.9f).send(0.2f));
  add(NB(Noise, 0, 0.35f * V).env(0.08f, 1, 1, 0.25f, 0.2f).lp(600 * r).at(0.05f));
  add(NB(Sine, 340 * r, 0.22f * V).perc(0.04f).pitch(1.2f, 0.01f).at(0.02f));
  add(NB(Noise, 0, 0.15f * V).perc(0.012f).bp(1800 * r, 2.0f).at(0.02f));
}
void Audio::fxReelClick(float k, float V, uint32_t arg) { // the ratchet: a dense run of small dry clicks, the pawl's tick-tack
  const float r = k, gap = 0.0135f / r;   // (P3 E) fourteen clicks in ~180 ms at k 1, closer together with the tension
  for (int i = 0; i < 14; i++) {
    const float accent = (i & 1) ? 0.8f : 1.0f;
    add(NB(Noise, 0, 0.30f * accent * V).perc(0.012f).bp(3200 * r * rnd(0.95f, 1.05f), 3.0f).at(gap * (float)i + rnd(0, 0.003f) / r));
  }
}
void Audio::fxLineSnap(float k, float V, uint32_t arg) { // a twang that whips away
  add(NB(Pluck, 660 * k, 0.5f * V).ring(0.25f).slide(-1.5f));
  add(NB(Noise, 0, 0.4f * V).env(0.002f, 0.12f).hp(2500).fslide(-2.0f));
}
void Audio::fxFishPlop(float k, float V, uint32_t arg) { // a small plop and a bubble
  add(NB(Sine, 520 * k, 0.4f * V).perc(0.06f).slide(2.2f));
  add(NB(Noise, 0, 0.3f * V).perc(0.04f).bp(1500 * k, 1.2f));
  add(NB(Sine, 900 * k, 0.12f * V).perc(0.04f).slide(2.5f).at(0.09f));
}
void Audio::fxSerpentRoar(float k, float V, uint32_t arg) { // water boiling off it, a hissing intake, a bubbling bellow sinking to a deep growl
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Noise, 0, 0.5f * V).env(0.05f, 1, 1, 0.25f, 0.3f).hp(1800 * r).bump(1.0f));
  add(NB(Saw, 130 * r, 0.55f * V).env(0.08f, 1, 1, 0.9f, 0.5f).slide(-0.9f).vox(620 * r, 2.2f).vib(1.2f, 7).am(23, 0.35f).drive(1.6f).send(0.4f).at(0.2f));
  add(NB(Sine, 55 * r, 0.7f * V).env(0.1f, 1, 1, 0.9f, 0.6f).slide(-0.4f).at(0.25f));
  add(NB(Noise, 0, 0.35f * V).env(0.1f, 1, 1, 0.8f, 0.5f).bp(380 * r, 1.0f).am(15, 0.7f).at(0.25f));
}
void Audio::fxKrakenRumble(float k, float V, uint32_t arg) { // a deep groan under the water, the arm tearing up through the surface, water pouring off
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Sine, 38 * r, 0.9f * V).env(0.2f, 1, 1, 0.8f, 0.8f).vib(0.8f, 1.5f));
  add(NB(Saw, 72 * r, 0.3f * V).env(0.25f, 1, 1, 0.6f, 0.6f).lp(240 * r).vib(0.5f, 3).drive(1));
  add(NB(Noise, 0, 0.7f * V).env(0.02f, 0.5f).lp(1400 * r).send(0.4f).at(0.35f));
  add(NB(Noise, 0, 0.4f * V).env(0.15f, 1, 1, 0.5f, 0.9f).hp(1800).at(0.45f));
}
void Audio::fxMerrowChatter(float k, float V, uint32_t arg) { // clicks and gurgling, rising chirps
  const float r = k * rnd(0.9f, 1.15f);
  for (int i = 0; i < 5; i++) {
    const float t = 0.07f * (float)i + rnd(0, 0.03f);
    if (i & 1) add(NB(Noise, 0, 0.25f * V).perc(0.01f).bp(2200 * r, 3.0f).at(t));
    else add(NB(Sine, rnd(700, 1100) * r, 0.22f * V).env(0.005f, 0.06f).slide(rnd(1.5f, 3.0f)).am(40, 0.6f).at(t));
  }
}
void Audio::fxHullCrack(float k, float V, uint32_t arg) { // timber splitting: a hard crack, splinters, the frames groaning
  const float r = k * rnd(0.92f, 1.08f);
  add(NB(Noise, 0, 0.9f * V).perc(0.03f).bp(1500 * r, 0.9f));
  add(NB(Noise, 0, 0.5f * V).perc(0.12f).bp(700 * r, 1.4f).at(0.01f));
  add(NB(Saw, 110 * r, 0.3f * V).env(0.05f, 0.5f).slide(-0.3f).vox(500 * r, 3.0f).am(12, 0.5f).at(0.08f));
  add(NB(Sine, 80 * r, 0.6f * V).perc(0.2f).pitch(1.5f, 0.02f));
}
void Audio::fxMistWhisper(float k, float V, uint32_t arg) { // breaths of whispering over a cold beating drone
  const float r = k;
  add(NB(Sine, 98 * r, 0.25f * V).env(0.8f, 1, 1, 1.6f, 1.2f).send(0.6f));
  add(NB(Sine, 99.7f * r, 0.25f * V).env(0.8f, 1, 1, 1.6f, 1.2f).send(0.6f));
  for (int i = 0; i < 4; i++)
    add(NB(Noise, 0, 0.22f * V).env(0.15f, 1, 1, 0.25f, 0.3f).vox(rnd(900, 2400) * r, 4.0f).am(rnd(5, 9), 0.6f).send(0.7f).at(0.3f + 0.45f * (float)i));
}
void Audio::fxMistChime(float k, float V, uint32_t arg) { // a rising shimmer of bells into a warm open chord
  const float r = k;
  const float ch[5] = {523.3f, 659.3f, 784.0f, 987.8f, 1318.5f};
  for (int i = 0; i < 5; i++) add(NB(Fm, ch[i] * r, 0.16f * V).perc(1.6f).fm(3.5f, 1.2f, 0.4f).send(0.6f).at(0.12f * (float)i));
  add(NB(Pad, 261.6f * r, 0.12f * V).env(0.6f, 1, 1, 1.6f, 1.4f).lp(1800).send(0.5f).at(0.3f));
  add(NB(Pad, 392.0f * r, 0.10f * V).env(0.6f, 1, 1, 1.6f, 1.4f).lp(1800).send(0.5f).at(0.3f));
}
void Audio::fxShipBell(float k, float V, uint32_t arg) { // a ship's bell: two bright strokes
  const float f0 = 880.0f * k;
  for (int s2 = 0; s2 < 2; s2++) {
    const float t = 0.38f * (float)s2;
    add(NB(Fm, f0, 0.3f * V).perc(1.4f).fm(2.76f, 1.5f, 0.2f).send(0.4f).at(t));
    add(NB(Sine, f0 * 2.4f, 0.12f * V).perc(0.6f).at(t));
    add(NB(Noise, 0, 0.18f * V).perc(0.01f).hp(4000).at(t));
  }
}
void Audio::fxThunder(float k, float V, uint32_t arg) { // a white crack, a snapping arc, then thunder rolling away
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Noise, 0, 1.0f * V).perc(0.016f).hp(2000 * r));
  add(NB(Saw, 1800 * r, 0.2f * V).env(0.002f, 0.15f).slide(-2.5f).am(90, 0.8f).bp(2500 * r, 1.3f).drive(1.4f));
  add(NB(Sine, 46 * r, 0.9f * V).perc(0.7f).pitch(1.0f, 0.05f).at(0.03f));
  add(NB(Noise, 0, 0.8f * V).env(0.05f, 1, 1, 0.4f, 1.3f).lp(210 * r, 0.9f).am(6, 0.5f).send(0.6f).at(0.06f));
  add(NB(Noise, 0, 0.6f * V).env(0.2f, 1, 1, 0.45f, 1.2f).lp(120 * r, 0.9f).am(3.5f, 0.6f).send(0.6f).at(0.35f));
}
void Audio::fxHolyCast(float k, float V, uint32_t arg) { // a rising choir swell under three bells
  const float r = k;
  const float ch[3] = {440.0f, 554.4f, 659.3f};   // A major
  for (int i = 0; i < 3; i++)
    add(NB(Pad, ch[i] * r, 0.06f * V).detune(0.006f).env(0.15f, 1, 1, 0.4f, 0.5f).vox(1000, 1.5f).slide(0.05f).send(0.6f));
  const float bell[3] = {1318.5f, 1661.2f, 1975.5f};
  for (int i = 0; i < 3; i++)
    add(NB(Fm, bell[i] * r, 0.11f * V).perc(0.6f).fm(3.5f, 1.1f, 0.25f).send(0.65f).at(0.07f * (float)i));
  add(NB(Noise, 0, 0.05f * V).env(0.2f, 1, 1, 0.3f, 0.4f).hp(6000).send(0.6f));
}
void Audio::fxHolyBloom(float k, float V, uint32_t arg) { // a warm major chord, a soft rising shimmer and a sparkle on top
  const float r = k;
  const float ch[4] = {293.7f, 370.0f, 440.0f, 587.3f};   // D major
  for (int i = 0; i < 4; i++)
    add(NB(Saw, ch[i] * r, 0.055f * V).detune(0.006f).env(0.05f, 1, 1, 0.45f, 0.7f).lp(1400 * r, 1.0f).send(0.55f).at(0.02f * (float)i));
  add(NB(Sine, 147 * r, 0.3f * V).env(0.05f, 0.6f).send(0.3f));
  add(NB(Sine, 1174.7f * r, 0.07f * V).env(0.1f, 1, 1, 0.3f, 0.6f).vib(0.2f, 6).send(0.6f));
  add(NB(Fm, 2349.3f * r, 0.07f * V).perc(0.8f).fm(3.0f, 1.4f, 0.3f).send(0.7f).at(0.1f));
}
void Audio::fxRiftTear(float k, float V, uint32_t arg) { // the air tearing: a ripping downward groan into a reversed swell and a sub
  const float r = k * rnd(0.96f, 1.04f);
  add(NB(Noise, 0, 0.85f * V).env(0.01f, 1, 1, 0.38f, 0.2f).bp(420 * r, 1.4f).fslide(-1.5f).am(35, 0.8f).drive(1.5f).send(0.35f));
  add(NB(Saw, 92 * r, 0.3f * V).env(0.05f, 1, 1, 0.45f, 0.3f).slide(-0.8f).vox(520 * r, 2.0f).drive(1.0f).send(0.4f));
  add(NB(Pad, 146.8f * r, 0.12f * V).detune(0.012f).env(0.42f, 1, 1, 0.42f, 0.05f).lp(1100, 1.2f).fslide(1.6f).send(0.5f));
  add(NB(Sine, 55 * r, 0.5f * V).env(0.3f, 1, 1, 0.4f, 0.4f).send(0.3f));
}
void Audio::fxSummonRise(float k, float V, uint32_t arg) { // something climbs out: a low growl rising over a choir of whispers
  const float r = k * rnd(0.96f, 1.04f);
  add(NB(Saw, 70 * r, 0.34f * V).env(0.1f, 1, 1, 0.6f, 0.3f).slide(1.2f).vox(600 * r, 2.5f).am(25, 0.6f).drive(1.5f).send(0.4f));
  add(NB(Noise, 0, 0.3f * V).env(0.2f, 1, 1, 0.5f, 0.4f).bp(2000 * r, 4.0f).am(7, 0.8f).send(0.6f));
  add(NB(Pad, 110 * r, 0.08f * V).detune(0.01f).env(0.2f, 1, 1, 0.6f, 0.6f).lp(900).send(0.55f));
  add(NB(Pad, 130.8f * r, 0.07f * V).detune(0.01f).env(0.2f, 1, 1, 0.6f, 0.6f).lp(900).send(0.55f));
}
void Audio::fxShimmer(float k, float V, uint32_t arg) { // illusion: a detuned glassy warble, wavering in pitch
  const float r = k;
  add(NB(Fm, 880 * r, 0.09f * V).env(0.05f, 1, 1, 0.35f, 0.4f).fm(2.0f, 1.5f, 0.6f).vib(0.6f, 9).send(0.6f));
  add(NB(Fm, 893 * r, 0.09f * V).env(0.05f, 1, 1, 0.35f, 0.4f).fm(2.0f, 1.5f, 0.6f).vib(0.6f, 7).send(0.6f));
  add(NB(Fm, 2093 * r, 0.07f * V).perc(0.45f).fm(1.5f, 1.0f, 0.2f).vib(0.8f, 6).send(0.7f).at(0.04f));
  add(NB(Tri, 1320 * r, 0.07f * V).env(0.02f, 0.4f).slide(-0.3f).vib(1.0f, 11).send(0.7f));
  add(NB(Noise, 0, 0.07f * V).env(0.1f, 1, 1, 0.3f, 0.3f).hp(5000).am(13, 0.7f).send(0.6f));
}
void Audio::fxDreamPulse(float k, float V, uint32_t arg) { // a reversed swell into a soft sub thump, an echoing bell and a wobble
  const float r = k;
  add(NB(Noise, 0, 0.42f * V).env(0.3f, 1, 1, 0.3f, 0.03f).bp(1500 * r, 1.6f).fslide(1.5f).send(0.5f));
  add(NB(Sine, 70 * r, 0.75f * V).perc(0.3f).pitch(1.5f, 0.03f).at(0.32f));
  add(NB(Fm, 659.3f * r, 0.12f * V).perc(1.0f).fm(1.41f, 1.5f, 0.4f).send(0.9f).at(0.32f));
  add(NB(Tri, 330 * r, 0.09f * V).env(0.01f, 0.6f).vib(1.5f, 4).send(0.7f).at(0.33f));
}
void Audio::fxStoneGrind(float k, float V, uint32_t arg) { // alteration: stone grinding over a low pulse, a few rock clicks
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Noise, 0, 0.75f * V).env(0.04f, 1, 1, 0.38f, 0.2f).lp(700 * r, 1.1f).am(18, 0.9f).drive(2.0f).color(0.7f));
  add(NB(Sine, 65 * r, 0.5f * V).env(0.05f, 1, 1, 0.35f, 0.2f).am(6, 0.8f));
  add(NB(Saw, 98 * r, 0.12f * V).env(0.05f, 1, 1, 0.3f, 0.2f).lp(400).am(18, 0.6f).drive(1.0f));
  for (int i = 0; i < 4; i++) add(NB(Noise, 0, 0.25f * V).perc(0.015f).bp(rnd(1200, 2000), 3.0f).at(rnd(0.05f, 0.4f)));
}
void Audio::fxStoneSlam(float k, float V, uint32_t arg) { // a rock slam, rubble pattering down, a slosh of water
  const float r = k * rnd(0.95f, 1.05f);
  add(NB(Sine, 55 * r, 1.0f * V).perc(0.4f).pitch(2.5f, 0.04f).drive(0.6f));
  add(NB(Noise, 0, 0.8f * V).perc(0.12f).lp(900 * r, 0.9f).fenv(2.0f, 0.03f));
  for (int i = 0; i < 6; i++) add(NB(Noise, 0, rnd(0.15f, 0.3f) * V).perc(0.02f).bp(rnd(1200, 2500), 2.5f).at(rnd(0.08f, 0.45f)));
  add(NB(Noise, 0, 0.25f * V).env(0.02f, 1, 1, 0.15f, 0.2f).bp(600 * r, 1.5f).fslide(-1.0f).send(0.4f).at(0.04f));
}
void Audio::fxStarChime(float k, float V, uint32_t arg) { // starcalling: high bells drawn out like a constellation, a cold shimmer
  const float r = k;
  const float run[5] = {1568.0f, 1760.0f, 2093.0f, 2349.3f, 2637.0f};
  for (int i = 0; i < 5; i++)
    add(NB(Fm, run[i] * r, 0.08f * V).perc(1.2f).fm(3.5f, 1.0f, 0.5f).send(0.8f).at(0.08f * (float)i));
  add(NB(Sine, 3136 * r, 0.035f * V).env(0.2f, 1, 1, 0.5f, 0.8f).vib(0.2f, 5).send(0.8f));
  add(NB(Noise, 0, 0.035f * V).env(0.3f, 1, 1, 0.5f, 0.6f).hp(8000).send(0.7f));
  add(NB(Sine, 392 * r, 0.12f * V).env(0.1f, 1, 1, 0.5f, 0.8f).send(0.6f));
}
void Audio::fxMeteorFall(float k, float V, uint32_t arg) { // a descending shriek of air building into a roar
  const float r = k;
  add(NB(Saw, 2400 * r, 0.09f * V).env(0.15f, 1, 1, 0.55f, 0.1f).slide(-2.2f).bp(2000 * r, 1.5f).fslide(-1.5f).send(0.4f));
  add(NB(Noise, 0, 0.6f * V).env(0.3f, 1, 1, 0.5f, 0.1f).bp(1200 * r, 1.2f).fslide(-1.0f).bump(1.0f).send(0.4f));
  add(NB(Noise, 0, 0.5f * V).env(0.4f, 1, 1, 0.4f, 0.1f).lp(400 * r, 1.0f).drive(1.0f).send(0.3f));
  add(NB(Sine, 80 * r, 0.3f * V).env(0.45f, 1, 1, 0.3f, 0.1f).slide(-0.5f));
}
void Audio::fxWardRing(float k, float V, uint32_t arg) { // a ward raised or struck: a glassy ring over a resonant hum
  const float r = k;
  add(NB(Fm, 1046.5f * r, 0.16f * V).perc(1.0f).fm(1.5f, 2.0f, 0.3f).send(0.6f));
  add(NB(Fm, 1568 * r, 0.09f * V).perc(0.7f).fm(2.0f, 1.2f, 0.2f).send(0.65f).at(0.02f));
  add(NB(Sine, 261.6f * r, 0.22f * V).env(0.05f, 1, 1, 0.4f, 0.5f).am(7, 0.3f).send(0.4f));
  add(NB(Noise, 0, 0.05f * V).env(0.02f, 1, 1, 0.2f, 0.3f).hp(6000).send(0.6f));
}
void Audio::fxSizzle(float k, float V, uint32_t arg) { // a surface combo: a short whoomph and steam hissing out
  add(NB(Noise, 0, 0.6f * V).env(0.01f, 1, 1, 0.5f, 0.4f).hp(3000 * k).fslide(-0.5f).am(23, 0.3f).send(0.4f));
  add(NB(Sine, 80 * k, 0.45f * V).perc(0.25f).pitch(1.2f, 0.03f));
  for (int i = 0; i < 4; i++) add(NB(Sine, rnd(600, 1200) * k, 0.08f * V).perc(0.04f).pitch(1.0f, 0.01f).at(rnd(0.05f, 0.4f)));
}
void Audio::fxSpellMotif(float k, float V, uint32_t arg) {
  if ((arg & (1u << 30)) && !(arg & (1u << 31))) { riseSting((int)(arg & 255), k, V); return; }   // (M12) playSting
  spellMotif(arg ? arg : (72u << 16), V);
}

#ifdef _MSC_VER
#pragma inline_depth()
#endif

// (M9 SPELLFX) a spell's motif: 3 or 4 notes in the culture's scale (its melody notes: a pentatonic culture's motif
// stays pentatonic), in the school's voice. The contour and rhythm come from the motif number, so every school and
// tradition is recognisable by ear; the last note rings longest.
void Audio::spellMotif(uint32_t arg, float V) {
  const int motif = (int)(arg & 255), scale = (int)((arg >> 8) & 15) % (int)Scale::COUNT, timbre = (int)((arg >> 12) & 15);
  int root = (int)((arg >> 16) & 127);
  if (root < 36) root = 72;
  int pcsList[12], np = 0;
  for (int pc = 0; pc < 12; pc++) if (kMelMask[scale] & (1u << pc)) pcsList[np++] = pc;
  if (np == 0) { pcsList[0] = 0; np = 1; }
  static const int8_t kContour[8][4] = {{0, 2, 4, 7}, {4, 2, 0, -3}, {0, 4, 2, 5}, {0, 1, 0, 4},
                                         {7, 4, 5, 0}, {0, -1, 2, 4}, {2, 0, 4, 3}, {0, 3, 1, 5}};
  static const float kRhythm[4][4] = {{0, 0.08f, 0.16f, 0.24f}, {0, 0.12f, 0.18f, 0.3f}, {0, 0.06f, 0.18f, 0.24f}, {0, 0.1f, 0.2f, 0.36f}};
  const int8_t* ct = kContour[motif & 7];
  const float* rh = kRhythm[(motif >> 3) & 3];
  const int n = 3 + ((motif >> 5) & 1);
  for (int i = 0; i < n; i++) {
    const int d = ct[i];
    const int oct = d >= 0 ? d / np : -((-d + np - 1) / np);
    const int idx = d - oct * np;
    const float f = mtof(root + oct * 12 + pcsList[idx]);
    const bool last = i == n - 1;
    const float t = rh[i], v = (last ? 0.12f : 0.09f) * V;
    switch (timbre) {
      case 0:   // brass: a bright saw stab through a closing filter (destruction)
        add(NB(Saw, f, v).detune(0.006f).env(0.01f, last ? 0.45f : 0.16f).lp(f * 5, 1.2f).fenv(1.2f, 0.08f).send(0.45f).at(t));
        break;
      case 1:   // bells (restoration)
        add(NB(Fm, f * 2, v).perc(last ? 1.1f : 0.5f).fm(3.5f, 1.0f, 0.3f).send(0.7f).at(t));
        break;
      case 2:   // organ: a low hollow reed (conjuration)
        add(NB(Square, f * 0.5f, v * 0.8f).pw(0.3f).env(0.02f, 1, 1, last ? 0.35f : 0.12f, 0.2f).lp(f * 3, 1.0f).vib(0.15f, 5).send(0.6f).at(t));
        break;
      case 3:   // glass: a wavering FM tone (illusion)
        add(NB(Fm, f * 2, v).env(0.01f, last ? 0.6f : 0.25f).fm(2.0f, 1.4f, 0.4f).vib(0.5f, 7).send(0.75f).at(t));
        break;
      case 4:   // wood: a marimba-like knock (alteration)
        add(NB(Sine, f, v * 1.2f).perc(last ? 0.5f : 0.25f).pitch(0.3f, 0.005f).send(0.4f).at(t));
        add(NB(Fm, f * 4, v * 0.3f).perc(0.04f).fm(1.0f, 2.0f, 0.02f).at(t));
        break;
      case 5:   // choir: a sung vowel
        add(NB(Saw, f, v * 0.8f).detune(0.008f).env(0.04f, 1, 1, last ? 0.4f : 0.14f, 0.25f).vox(900, 1.8f).vib(0.2f, 5.5f, 0.05f).send(0.65f).at(t));
        break;
      default:  // harp: a plucked string
        add(NB(Pluck, f, v * 1.5f).bright(0.7f).ring(last ? 0.9f : 0.5f).send(0.5f).at(t));
        break;
    }
  }
}

// ------------------------------------------------------------------------------------------ (M12) the rise's stings
// The herald's stings (Audio::playSting, rpg/view/render_rise.cpp): short, unmistakable, under the limiter.
void Audio::riseSting(int which, float k, float V) {
  switch ((Sting)which) {
    case Sting::DrumRoll: {   // an oath: a snare roll swelling over a second, then the bass drum and a cymbal's wash
      for (int i = 0; i < 26; i++) {
        const float t = 0.034f * (float)i, a = 0.18f + 0.55f * (float)i / 25.0f;
        add(NB(Noise, 0, 0.42f * a * V).perc(0.045f).bp(1900 * k * rnd(0.95f, 1.05f), 1.1f).color(0.15f).at(t + rnd(0, 0.006f)));
        add(NB(Sine, 220 * k, 0.10f * a * V).perc(0.03f).pitch(0.6f, 0.01f).at(t));
      }
      add(NB(Sine, 62 * k, 0.95f * V).perc(0.55f).pitch(0.9f, 0.04f).at(0.9f).send(0.35f));
      add(NB(Noise, 0, 0.55f * V).perc(0.09f).bp(1700 * k, 0.9f).at(0.9f));
      add(NB(Noise, 0, 0.22f * V).env(0.002f, 1.1f).hp(5200 * k).send(0.5f).at(0.9f));
      add(NB(Saw, 146.8f * k, 0.14f * V).env(0.03f, 0.5f, 0.7f, 0.55f, 0.4f).lp(700, 1.1f).at(0.9f).send(0.45f));   // a low horn under the hit
      break;
    }
    case Sting::Fanfare: {    // a rank: "ta-ta-ta TAAA ta-DAAA" on two trumpets, a snare flam, timpani
      const float n1[6] = {392.0f, 392.0f, 392.0f, 523.3f, 392.0f, 659.3f};
      const float tm[6] = {0.0f, 0.1f, 0.2f, 0.3f, 0.62f, 0.74f};
      const float ln[6] = {0.06f, 0.06f, 0.06f, 0.26f, 0.08f, 0.95f};
      for (int i = 0; i < 6; i++) {
        add(NB(Saw, n1[i] * k, 0.20f * V).env(0.012f, 0.3f, 0.7f, ln[i], 0.12f).lp(1500, 1.1f).fenv(1.2f, 0.07f)
                .vib(i == 5 ? 0.18f : 0.0f, 5.4f, 0.25f).at(tm[i]).send(0.35f));
        add(NB(Saw, n1[i] * 0.75f * k, 0.10f * V).env(0.015f, 0.3f, 0.7f, ln[i], 0.12f).lp(1100, 1.0f).at(tm[i]).send(0.35f));   // the second trumpet a fourth under
      }
      const float ch[3] = {261.6f, 329.6f, 392.0f};
      for (float c : ch) add(NB(Pad, c * k, 0.09f * V).detune(0.007f).lp(1800).env(0.06f, 1, 1, 0.9f, 0.6f).at(0.74f).send(0.5f));
      add(NB(Sine, 98 * k, 0.7f * V).perc(0.3f).pitch(0.4f, 0.03f).at(0.3f));
      add(NB(Sine, 65.4f * k, 0.85f * V).perc(0.7f).pitch(0.4f, 0.04f).at(0.74f).send(0.3f));
      add(NB(Noise, 0, 0.32f * V).perc(0.07f).bp(2100 * k, 1.0f).at(0.71f));
      add(NB(Noise, 0, 0.36f * V).perc(0.09f).bp(1900 * k, 1.0f).at(0.74f));
      for (int i = 0; i < 3; i++) add(NB(Fm, 2093.0f * k * std::exp2((float)i * 7.0f / 12.0f), 0.05f * V).perc(0.5f).fm(3, 1, 0.2f).at(0.8f + 0.07f * (float)i).send(0.6f));
      break;
    }
    case Sting::HornCall: {   // independence: two great horns calling over a low drone, a war drum, the hall's echo
      const float call[4] = {146.8f, 220.0f, 196.0f, 293.7f};
      const float tm[4] = {0.0f, 0.42f, 0.86f, 1.12f};
      const float ln[4] = {0.34f, 0.36f, 0.2f, 1.25f};
      for (int i = 0; i < 4; i++) {
        add(NB(Saw, call[i] * k, 0.26f * V).env(0.07f, 0.5f, 0.8f, ln[i], 0.45f).lp(620, 1.2f).fenv(1.3f, 0.18f)
                .vib(i == 3 ? 0.16f : 0.06f, 4.8f, 0.3f).at(tm[i]).send(0.7f));
        add(NB(Saw, call[i] * 0.5f * k, 0.16f * V).env(0.09f, 0.6f, 0.8f, ln[i], 0.5f).lp(380, 1.0f).at(tm[i] + 0.02f).send(0.6f));
      }
      add(NB(Pad, 73.4f * k, 0.13f * V).detune(0.006f).lp(500).env(0.4f, 1, 1, 2.2f, 1.2f).send(0.6f));
      add(NB(Sine, 55 * k, 0.9f * V).perc(0.8f).pitch(0.7f, 0.05f).at(1.12f).send(0.45f));
      add(NB(Sine, 55 * k, 0.6f * V).perc(0.5f).pitch(0.7f, 0.05f).at(0.0f).send(0.4f));
      add(NB(Noise, 0, 0.18f * V).env(0.003f, 1.6f).hp(4800 * k).send(0.6f).at(1.12f));
      break;
    }
    case Sting::WarAlarm: {   // a warlord or a beast: two harsh horn blasts a tritone apart, war drums, a clanging bell
      for (int b = 0; b < 2; b++) {
        const float t = 0.34f * (float)b;
        add(NB(Saw, 116.5f * k, 0.30f * V).env(0.02f, 0.3f, 0.85f, 0.24f, 0.12f).lp(900, 1.4f).drive(1.3f).at(t).send(0.4f));
        add(NB(Saw, 164.8f * k, 0.22f * V).env(0.02f, 0.3f, 0.85f, 0.24f, 0.12f).lp(1000, 1.3f).drive(1.2f).at(t).send(0.4f));
      }
      for (int i = 0; i < 4; i++) add(NB(Sine, 70 * k, (0.6f + 0.1f * (float)i) * V).perc(0.22f).pitch(1.0f, 0.03f).at(0.72f + 0.14f * (float)i).send(0.3f));
      add(NB(Noise, 0, 0.3f * V).perc(0.06f).bp(900 * k, 1.0f).at(1.14f));
      const float f0 = 523.3f * k;
      add(NB(Fm, f0, 0.16f * V).perc(1.0f).fm(2.76f, 2.0f, 0.3f).at(0.7f).send(0.5f));
      add(NB(Sine, f0 * 1.19f, 0.10f * V).perc(0.9f).at(0.7f).send(0.4f));
      break;
    }
    case Sting::Seal: {       // a seal pressed into warm wax on a folded letter: a paper rustle, a soft thump, a click
      add(NB(Noise, 0, 0.28f * V).env(0.01f, 0.08f).bp(3200 * k, 0.8f).color(0.1f));
      add(NB(Noise, 0, 0.18f * V).env(0.01f, 0.06f).bp(2400 * k, 0.8f).at(0.09f));
      add(NB(Sine, 150 * k, 0.55f * V).perc(0.09f).pitch(0.8f, 0.015f).at(0.2f));
      add(NB(Noise, 0, 0.22f * V).perc(0.02f).lp(1400 * k).at(0.2f));
      add(NB(Sine, 1900 * k, 0.05f * V).perc(0.03f).at(0.22f));
      break;
    }
    default: break;
  }
}
