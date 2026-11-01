// The voice pool: Audio::renderVoice plays one voice (Audio::add, which starts a note on a free voice, is inline in audio_internal.h).
#include "engine/audio/audio_internal.h"

// ------------------------------------------------------------------------------------------ voices
void Audio::renderVoice(Voice& v, int n, const float* layerGain, float* sfx, float* mus, float* sndS, float* sndM) {
  Note& p = v.n;
  int s0 = 0;
  if (p.delay > 0) {
    int ds = (int)(p.delay * FS);
    if (ds >= n) { p.delay -= (float)n * DT; return; }
    s0 = ds;
    p.delay = 0;
  }
  const int cnt = n - s0;
  const float t1 = v.t + (float)cnt * DT, tm = v.t + 0.5f * (float)cnt * DT;

  // control rate: pitch
  float f = p.freq;
  if (p.slide != 0) f *= std::exp2(p.slide * tm);
  if (p.pEnv != 0) f *= 1.0f + p.pEnv * std::exp(-tm / p.pDec);
  if (p.vibDepth != 0) {
    float fade = p.vibDelay > 0 ? clampf((tm - p.vibDelay) / p.vibDelay, 0.0f, 1.0f) : 1.0f;
    f *= std::exp2(p.vibDepth * (1.0f / 12.0f) * fade * sinc(p.vibRate * tm));
  }
  f = clampf(f, 1.0f, 20000.0f);
  const float inc = f * DT;

  // control rate: amplitude (ramped linearly across the block)
  const float env = NB::envAt(p, t1);
  float amp = p.vol * env;
  const bool done = t1 > p.atk && (env < 1e-3f || amp < 2e-4f) && (p.sus <= 0 || t1 > p.hold);  // -60 dB or inaudible
  if (p.amDepth > 0) amp *= 1.0f - p.amDepth * (0.5f + 0.5f * sinc(p.amRate * t1));
  if (p.bus == 1 || p.bus == 2) amp *= layerGain[p.bus - 1];
  const float g1 = done ? 0.0f : amp;
  float g = v.gain;
  const float dg = (g1 - g) / (float)cnt;

  // oscillator
  float osc[BLOCK];
  switch (p.wave) {
    case Sine:
      for (int i = 0; i < cnt; i++) { osc[i] = sinc(v.ph[0]); v.ph[0] += inc; }
      v.ph[0] -= std::floor(v.ph[0]);
      break;
    case Tri:
      for (int i = 0; i < cnt; i++) {
        osc[i] = 4.0f * std::fabs(v.ph[0] - 0.5f) - 1.0f;
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    case Saw:
      for (int i = 0; i < cnt; i++) {
        osc[i] = 2.0f * v.ph[0] - 1.0f - blep(v.ph[0], inc);
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    case Square: {
      const float pw = clampf(p.pw, 0.05f, 0.95f);
      for (int i = 0; i < cnt; i++) {
        float ph = v.ph[0], ph2 = ph + 1.0f - pw;
        if (ph2 >= 1.0f) ph2 -= 1.0f;
        osc[i] = (ph < pw ? 1.0f : -1.0f) + blep(ph, inc) - blep(ph2, inc);
        v.ph[0] += inc;
        if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
      }
      break;
    }
    case Noise:
      if (p.color > 0) {   // one-pole lowpass before the main filter: steeper skirts, less hiss
        const float a = 1.0f - clampf(p.color, 0.0f, 0.98f);
        for (int i = 0; i < cnt; i++) { v.nlp += a * (white(v.rng) - v.nlp); osc[i] = v.nlp; }
      } else {
        for (int i = 0; i < cnt; i++) osc[i] = white(v.rng);
      }
      break;
    case Fm: {
      const float idx = p.fmIndex * std::exp(-tm / p.fmDec) * (1.0f / TAU), minc = inc * p.fmRatio;
      for (int i = 0; i < cnt; i++) {
        osc[i] = sinc(v.ph[0] + idx * sinc(v.mph));
        v.ph[0] += inc;
        v.mph += minc;
      }
      v.ph[0] -= std::floor(v.ph[0]);
      v.mph -= std::floor(v.mph);
      break;
    }
    case Pad: {
      const float incs[3] = {inc * (1.0f - p.detune), inc, inc * (1.0f + p.detune)};
      for (int i = 0; i < cnt; i++) {
        float s = 0;
        for (int k = 0; k < 3; k++) {
          s += 2.0f * v.ph[k] - 1.0f - blep(v.ph[k], incs[k]);
          v.ph[k] += incs[k];
          if (v.ph[k] >= 1.0f) v.ph[k] -= 1.0f;
        }
        osc[i] = s * 0.45f;
      }
      break;
    }
    case Pluck: {
      float* buf = str_[v.str];
      const float h0 = 1.0f - v.ksH1;
      for (int i = 0; i < cnt; i++) {
        float cur = buf[v.ksPos];
        float lpf = h0 * cur + v.ksH1 * v.ksPrev;
        v.ksPrev = cur;
        float y = v.ksC * lpf + v.apX - v.ksC * v.apY;   // first-order allpass: fractional tuning
        v.apX = lpf;
        v.apY = y;
        buf[v.ksPos] = y * v.ksRho;
        if (++v.ksPos >= v.ksLen) v.ksPos = 0;
        osc[i] = cur;
      }
      break;
    }
  }
  if (p.breath > 0)
    for (int i = 0; i < cnt; i++) osc[i] += p.breath * white(v.rng);

  // filter (Simper/Cytomic trapezoidal SVF; stable under fast modulation)
  if (p.filt != Off) {
    float oct = p.fSlide * tm;
    if (p.fEnv != 0) oct += p.fEnv * std::exp(-tm / p.fDec);
    if (p.fBump != 0) oct += p.fBump * sinc(0.5f * std::fmin(tm / p.hold, 1.0f));
    const float fc = clampf(p.cutoff * std::exp2(oct), 20.0f, 20000.0f);
    const float gg = std::tan(PI * fc / FS), k = 1.0f / std::fmax(p.q, 0.1f);
    const float a1 = 1.0f / (1.0f + gg * (gg + k)), a2 = gg * a1, a3 = gg * a2;
    for (int i = 0; i < cnt; i++) {
      float x = osc[i];
      float v3 = x - v.ic2;
      float v1 = a1 * v.ic1 + a2 * v3;
      float v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
      v.ic1 = 2.0f * v1 - v.ic1;
      v.ic2 = 2.0f * v2 - v.ic2;
      switch (p.filt) {
        case LP: osc[i] = v2; break;
        case BP: osc[i] = k * v1; break;
        case HP: osc[i] = x - k * v1 - v2; break;
        default: osc[i] = v2 + 0.8f * v1; break;   // Vox
      }
    }
  }
  if (p.drive > 0) {
    const float pre = 1.0f + p.drive;
    for (int i = 0; i < cnt; i++) osc[i] = softSat(osc[i] * pre);
  }

  float* bus = p.bus == 0 ? sfx : mus;
  float* snd = p.bus == 0 ? sndS : sndM;
  for (int i = 0; i < cnt; i++) {
    g += dg;
    float y = osc[i] * g;
    bus[s0 + i] += y;
    snd[s0 + i] += y * p.send;
  }
  v.t = t1;
  v.gain = g1;
  if (done) {
    v.on = false;
    if (v.str >= 0) { strUsed_[v.str] = false; v.str = -1; }
  }
}
