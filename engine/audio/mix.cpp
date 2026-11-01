// The mixer and the device: renderBlock (voices, buses, ducking, reverb, echo, master, limiter), render and the SDL stream.
#include "engine/audio/audio_internal.h"

// ------------------------------------------------------------------------------------------ device
namespace {
void SDLCALL streamCb(void* ud, SDL_AudioStream* s, int additional, int) {
  Audio* a = (Audio*)ud;
  int frames = additional / (int)sizeof(float);
  float buf[1024];
  while (frames > 0) {
    int n = frames > 1024 ? 1024 : frames;
    a->render(buf, n);
    SDL_PutAudioStreamData(s, buf, n * (int)sizeof(float));
    frames -= n;
  }
}
}  // namespace

bool Audio::init() {
  if (stream_) return true;
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_Log("audio unavailable: %s", SDL_GetError());
    return false;
  }
  SDL_AudioSpec spec{SDL_AUDIO_F32, 1, (int)FS};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, streamCb, this);
  if (!stream_) {
    SDL_Log("audio unavailable: %s", SDL_GetError());
    return false;
  }
  SDL_ResumeAudioStreamDevice(stream_);
  return true;
}

void Audio::shutdown() {
  if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }  // stops the callback before returning
}

void Audio::renderBlock(float* out, int n) {
  updateMusic((float)n * DT);
  const float lg[2] = {std::sin(seq_[0].x * PI * 0.5f), std::sin(seq_[1].x * PI * 0.5f)};
  float sfx[BLOCK] = {}, mus[BLOCK] = {}, sndS[BLOCK] = {}, sndM[BLOCK] = {}, amb[BLOCK] = {}, crowd[BLOCK] = {};
  for (Voice& v : v_)
    if (v.on) renderVoice(v, n, lg, sfx, mus, sndS, sndM);
  renderAmbient(amb, n, (float)n * DT);
  renderSea(amb, n, (float)n * DT);   // (M10) the sea's bed on the ambient bus
  renderFalls(amb, n, (float)n * DT); // (M11) the falls' roar on the ambient bus
  renderCrowd(crowd, n, (float)n * DT);
  // (M5) the score's level and muffle (the bard heard from the street), eased over ~0.4 s; the crowd goes through them
  const float blockSec = (float)n * DT, kg = 1.0f - std::exp(-blockSec / 0.4f);
  musGain_ += (clampf(wantMusGain_.load(std::memory_order_relaxed), 0.0f, 1.0f) - musGain_) * kg;
  muffle_ += (clampf(wantMuffle_.load(std::memory_order_relaxed), 0.0f, 1.0f) - muffle_) * kg;
  const float mcut = 16000.0f * std::exp2(-muffle_ * 5.0f);   // 16 kHz open .. 500 Hz through a wall
  const float mc = 1.0f - std::exp(-TAU * mcut / FS);

  static constexpr int combLen[4] = {1215, 1293, 1390, 1476}, apLen[2] = {605, 480};
  const float master = clampf(master_.load(std::memory_order_relaxed), 0.0f, 1.5f);
  const float mv = clampf(musicVol_.load(std::memory_order_relaxed), 0.0f, 1.5f);
  for (int i = 0; i < n; i++) {
    // music ducks gently under loud effects
    const float a = std::fabs(sfx[i]);
    duckEnv_ = a > duckEnv_ ? duckEnv_ + (a - duckEnv_) * 0.02f : duckEnv_ * 0.99995f;
    const float duck = 1.0f - 0.35f * clampf((duckEnv_ - 0.35f) * 2.0f, 0.0f, 1.0f);   // footsteps/UI never duck
    float m = (mus[i] * mv * duck + crowd[i]) * musGain_;
    if (muffle_ > 0.01f) {   // two poles: the walls take the top off
      muffLp_[0] += mc * (m - muffLp_[0]);
      muffLp_[1] += mc * (muffLp_[0] - muffLp_[1]);
      m = muffLp_[1] * (1.0f + muffle_ * 0.4f);   // (a little of the lost loudness back: the body of it carries)
    } else { muffLp_[0] = muffLp_[1] = m; }
    const float send = sndS[i] + (sndM[i] * mv * duck + crowd[i] * 0.6f) * musGain_ * (1.0f - 0.7f * muffle_) + amb[i] * 0.15f;

    // reverb: 4 damped feedback combs + 2 allpasses (mono Freeverb)
    const float in = send * 0.22f + 1e-18f;   // tiny offset keeps the tails out of denormals
    float rv = 0;
    for (int c = 0; c < 4; c++) {
      float y = comb_[c][combPos_[c]];
      combLp_[c] = y * 0.7f + combLp_[c] * 0.3f;
      comb_[c][combPos_[c]] = in + combLp_[c] * 0.83f;
      if (++combPos_[c] >= combLen[c]) combPos_[c] = 0;
      rv += y;
    }
    for (int k = 0; k < 2; k++) {
      float b = ap_[k][apPos_[k]];
      ap_[k][apPos_[k]] = rv + b * 0.5f;
      rv = b - rv;
      if (++apPos_[k] >= apLen[k]) apPos_[k] = 0;
    }
    // echo: one damped feedback delay (~300 ms) for open-air / cavern space
    const float e = echo_[(echoPos_ - ECHO_TAP) & (ECHO - 1)];
    echoLp_ += (e - echoLp_) * 0.35f;
    echo_[echoPos_] = send * echoAmt_ + echoLp_ * 0.42f + 1e-18f;
    echoPos_ = (echoPos_ + 1) & (ECHO - 1);

    float x = (sfx[i] + m + amb[i] + rv * 0.3f + e * 0.6f) * master;
    // peak limiter: instant attack, 150 ms release; output can never exceed 0.8
    const float ax = std::fabs(x);
    limEnv_ = ax > limEnv_ ? ax : limEnv_ * 0.99986f;
    if (limEnv_ > 0.8f) x *= 0.8f / limEnv_;
    out[i] = x;
  }
}

void Audio::render(float* out, int frames) {
  if (!out || frames <= 0) return;
  uint32_t tail = qTail_.load(std::memory_order_relaxed);
  const uint32_t head = qHead_.load(std::memory_order_acquire);
  for (; tail != head; tail++) {
    const Event& e = q_[tail % QN];
    trigger(e.s, e.pitch, e.vol, e.arg);
  }
  qTail_.store(tail, std::memory_order_release);
  for (int done = 0; done < frames;) {
    const int n = frames - done < BLOCK ? frames - done : BLOCK;
    renderBlock(out + done, n);
    done += n;
  }
}
