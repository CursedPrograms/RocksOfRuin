// Rocks of Ruin's sound effects, made with the Web Audio API (no files). The browser only allows sound after a key
// or tap, so the audio starts on the first one. M mutes (remembered between visits).
var Sfx = (function () {
  "use strict";
  var ctx = null, out = null, thrustGain = null;
  var muted = localStorage.getItem("rocksOfRuinMuted") === "1";

  function wake() {
    if (!ctx) {
      var AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return;
      ctx = new AC();
      out = ctx.createGain();
      out.gain.value = muted ? 0 : 0.32;
      out.connect(ctx.destination);
      // the engine: a loop of noise through a low band-pass, faded in while thrusting
      var n = ctx.sampleRate, buf = ctx.createBuffer(1, n, n), d = buf.getChannelData(0);
      for (var i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
      var src = ctx.createBufferSource(), bp = ctx.createBiquadFilter();
      src.buffer = buf; src.loop = true;
      bp.type = "bandpass"; bp.frequency.value = 180; bp.Q.value = 0.8;
      thrustGain = ctx.createGain(); thrustGain.gain.value = 0;
      src.connect(bp); bp.connect(thrustGain); thrustGain.connect(out);
      src.start();
    }
    if (ctx.state === "suspended") ctx.resume();
  }
  ["keydown", "pointerdown", "touchstart"].forEach(function (e) { document.addEventListener(e, wake, true); });

  function tone(type, f0, f1, at, len, vol) {
    var o = ctx.createOscillator(), g = ctx.createGain();
    o.type = type;
    o.frequency.setValueAtTime(f0, at);
    o.frequency.exponentialRampToValueAtTime(Math.max(1, f1), at + len);
    g.gain.setValueAtTime(vol, at);
    g.gain.exponentialRampToValueAtTime(0.001, at + len);
    o.connect(g); g.connect(out);
    o.start(at); o.stop(at + len + 0.02);
  }

  function noise(at, len, vol, freq) {
    var n = Math.floor(ctx.sampleRate * len), buf = ctx.createBuffer(1, n, ctx.sampleRate), d = buf.getChannelData(0);
    for (var i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / n, 2);
    var s = ctx.createBufferSource(), f = ctx.createBiquadFilter(), g = ctx.createGain();
    s.buffer = buf; f.type = "lowpass"; f.frequency.value = freq || 2000; g.gain.value = vol;
    s.connect(f); f.connect(g); g.connect(out);
    s.start(at);
  }

  var lastShot = 0, lastPick = 0;
  function ok() { return ctx && !muted; }
  return {
    shoot: function () {                  // a short laser blip
      if (!ok()) return;
      var t = ctx.currentTime;
      if (t - lastShot < 0.05) return;
      lastShot = t;
      tone("square", 1100 + Math.random() * 120, 260, t, 0.09, 0.09);
    },
    boom: function (size) {               // rocks break: bigger rocks, deeper booms (size 0 = big)
      if (!ok()) return;
      var t = ctx.currentTime, k = [1, 0.7, 0.45, 0.25][size] || 0.3;
      noise(t, 0.25 + k * 0.5, 0.5 * k + 0.1, 500 + (1 - k) * 2500);
      tone("sine", 140 * (1.6 - k), 40, t, 0.3 + k * 0.4, 0.5 * k);
    },
    crack: function () {                  // a nest splits open: a dissonant shriek sliding down
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("sawtooth", 1400, 300, t, 0.6, 0.12);
      tone("sawtooth", 1480, 290, t + 0.03, 0.6, 0.1);
      tone("square", 90, 60, t, 0.7, 0.2);
      noise(t, 0.4, 0.3, 4000);
    },
    spit: function () {                   // an alien spits
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("sawtooth", 300, 900, t, 0.12, 0.08);
      noise(t, 0.08, 0.15, 3000);
    },
    squelch: function () {                // an alien dies
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("sawtooth", 600, 70, t, 0.35, 0.18);
      tone("square", 220, 50, t, 0.3, 0.12);
      noise(t, 0.25, 0.3, 1200);
    },
    pick: function () {                   // salvage collected: a chime, higher on a run
      if (!ok()) return;
      var t = ctx.currentTime, up = t - lastPick < 0.4 ? 1.15 : 1;
      lastPick = t;
      tone("triangle", 1320 * up, 1320 * up, t, 0.08, 0.3);
      tone("sine", 1980 * up, 1980 * up, t + 0.05, 0.12, 0.25);
    },
    hit: function () {                    // the hull takes damage
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("square", 160, 40, t, 0.35, 0.3);
      noise(t, 0.3, 0.5, 900);
    },
    shield: function () {                 // the shield soaks a hit
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("sine", 700, 1400, t, 0.18, 0.25);
      tone("triangle", 350, 700, t, 0.18, 0.2);
    },
    buy: function () {                    // an upgrade bought: a rising arpeggio
      if (!ok()) return;
      var t = ctx.currentTime;
      [523, 659, 784, 1047].forEach(function (f, i) { tone("square", f, f, t + i * 0.06, 0.12, 0.12); });
    },
    deny: function () {
      if (!ok()) return;
      tone("square", 150, 140, ctx.currentTime, 0.15, 0.15);
    },
    storm: function () {                  // a debris storm is coming: a two-tone siren
      if (!ok()) return;
      var t = ctx.currentTime;
      for (var i = 0; i < 4; i++) tone("square", i % 2 ? 520 : 780, i % 2 ? 520 : 780, t + i * 0.22, 0.2, 0.12);
    },
    over: function () {
      if (!ok()) return;
      var t = ctx.currentTime;
      tone("square", 440, 55, t, 1.4, 0.25);
      tone("triangle", 220, 30, t, 1.6, 0.4);
      noise(t, 1, 0.5, 600);
    },
    thrust: function (on) {
      if (!ctx || !thrustGain) return;
      thrustGain.gain.setTargetAtTime(on && !muted ? 0.5 : 0, ctx.currentTime, 0.05);
    },
    toggleMute: function () {
      muted = !muted;
      localStorage.setItem("rocksOfRuinMuted", muted ? "1" : "0");
      if (out) out.gain.value = muted ? 0 : 0.32;
      if (muted && thrustGain) thrustGain.gain.value = 0;
      return muted;
    },
    isMuted: function () { return muted; }
  };
})();
