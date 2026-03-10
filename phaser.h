#pragma once

struct Phase {
  unsigned long elapsed, duration, totalElapsed, totalDuration;
  float progress() const { return duration ? (float)elapsed / duration : 1.0f; }
};

namespace PhaserImpl {

struct PhaserBase {
  static constexpr unsigned long duration() { return 0; }
  static bool _run(unsigned long, unsigned long) { return false; }
  void run(unsigned long) {}
};

template<typename P, typename F> struct PhaserEnd;

template<typename P, typename F>
struct PhaserAnim {
  P prev; unsigned long dur; F func;
  unsigned long duration() const { return prev.duration() + dur; }
  template<typename G> PhaserAnim<PhaserAnim, G> anim(unsigned long d, G g) { return {*this, d, g}; }
  template<typename G> PhaserEnd<PhaserAnim, G> complete(G g) { return {*this, g}; }
  bool _run(unsigned long t, unsigned long total) {
    if (prev._run(t, total)) return true;
    auto s = prev.duration();
    if (t >= s && t < s + dur) { func(Phase{t - s, dur, t, total}); return true; }
    return false;
  }
  void run(unsigned long t) { _run(t, duration()); }
};

template<typename P, typename F>
struct PhaserEnd {
  P prev; F func;
  unsigned long duration() const { return prev.duration(); }
  void run(unsigned long t) {
    auto d = prev.duration();
    if (!prev._run(t, d)) func(Phase{t - d, 0, t, d});
  }
};

} // namespace PhaserImpl

struct Phaser : PhaserImpl::PhaserBase {
  template<typename F> PhaserImpl::PhaserAnim<PhaserImpl::PhaserBase, F> anim(unsigned long d, F f) { return {{}, d, f}; }
};
