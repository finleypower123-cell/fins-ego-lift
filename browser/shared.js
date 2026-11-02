/* Shared between the page and the offline tests: controls, presets, the EGO macro and the demo voice. */
const FEL_DIV_LABELS = ['1/16', '1/8T', '1/8', '1/4T', '1/8.', '1/4', '1/4.', '1/2', '1/2.'];
const FEL_SLAP_LABELS = ['45 ms', '60 ms', '75 ms', '90 ms', '105 ms', '120 ms', '140 ms', '160 ms', '190 ms'];

const FEL_SECTIONS = [
  { id: 'pre', on: 'preOn', name: 'Mic Technique', what: 'Preamp · gate · presence', knobs: [
    { id: 'inGain', joke: 'Swallows the mic', what: 'Input gain', min: -12, max: 18, fmt: 'db' },
    { id: 'gate', joke: 'Silence is art', what: 'Gate / expander', min: -80, max: -20, fmt: 'gate' },
    { id: 'hpf', joke: "Low end isn't my job", what: 'High-pass', min: 30, max: 400, curve: 'log', fmt: 'hz' },
    { id: 'presence', joke: 'Listen to me', what: 'Presence', min: 0, max: 1, fmt: 'pct' },
    { id: 'warmth', joke: 'Warm personality', what: 'Preamp saturation', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.55, floor: 0.1 } }
  ] },
  { id: 'dist', on: 'distOn', name: 'Tortured', what: 'Distortion · crush', type: { id: 'distType', label: 'Pain type', opts: ['Warm', 'Digital', 'Fuzz', 'Asym'] }, knobs: [
    { id: 'drive', joke: 'Tortured artist', what: 'Drive', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.85 } },
    { id: 'crush', joke: 'Recorded on a flip phone', what: 'Bit + rate crush', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.3 } },
    { id: 'tone', joke: 'Sharp tongue', what: 'Tone', min: 1500, max: 14000, curve: 'log', fmt: 'hz' },
    { id: 'distMix', joke: 'Selling out', what: 'Parallel blend', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.7 } }
  ] },
  { id: 'char', on: 'charOn', name: 'Persona', what: 'Pitch · formant · machine', wide: true, knobs: [
    { id: 'pitch', joke: 'Transposed (for art)', what: 'Pitch', min: -12, max: 12, step: 1, fmt: 'st' },
    { id: 'formant', joke: 'Changed voice on album two', what: 'Formant', min: -6, max: 6, fmt: 'fmt' },
    { id: 'robot', joke: 'Autotune? Never.', what: 'Pitch snap / robot', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.4 } },
    { id: 'metal', joke: 'Heart of steel', what: 'Metallic resonance', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.35 } },
    { id: 'octDown', joke: 'Octave below my station', what: 'Octave-down layer', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.3 } },
    { id: 'octUp', joke: "Falsetto I can't hit", what: 'Octave-up layer', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.25 } }
  ] },
  { id: 'mod', on: 'modOn', name: 'Still in his Y2K phase', what: 'Chorus · flanger · doubler', type: { id: 'modType', label: 'Which era', opts: ['Chorus', 'Flange', 'Double'] }, knobs: [
    { id: 'modAmt', joke: 'Main character energy', what: 'Depth + mix', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.8 } },
    { id: 'modRate', joke: 'Mood swings', what: 'Rate', min: 0.05, max: 6, curve: 'log', fmt: 'rate' },
    { id: 'modFb', joke: 'Sees himself everywhere', what: 'Feedback', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.55 } },
    { id: 'modWidth', joke: 'Too big for mono', what: 'Width', min: 0, max: 2, fmt: 'width', ego: { t: 'width', e: 1.8 } }
  ] },
  { id: 'dly', on: 'dlyOn', name: 'Echo Chamber', what: 'Delay', type: { id: 'dlyType', label: 'Repeat style', opts: ['Ping-pong', 'Tape', 'Dark', 'Slap'] }, bpm: true, knobs: [
    { id: 'division', joke: 'Delayed gratification', what: 'Time', min: 0, max: 8, step: 1, fmt: 'div' },
    { id: 'feedback', joke: 'Repeats himself in interviews', what: 'Feedback', min: 0, max: 0.95, fmt: 'pct', ego: { t: 'amt', e: 0.65 } },
    { id: 'wow', joke: 'Only listens on vinyl', what: 'Wow + flutter', min: 0, max: 1, fmt: 'pct' },
    { id: 'dlyMix', joke: 'Echo chamber of yes-men', what: 'Delay level', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.45 } }
  ] },
  { id: 'rev', on: 'revOn', name: 'The Cathedral', what: 'Reverb', wide: true, type: { id: 'revType', label: 'Venue', opts: ['Hall', 'Plate', 'Shimmer', 'Reverse'] }, knobs: [
    { id: 'decay', joke: 'Cathedral-sized ego', what: 'Decay', min: 0.4, max: 20, curve: 'log', fmt: 'sec', ego: { t: 'decay', e: 12 } },
    { id: 'predelay', joke: 'Fashionably late', what: 'Pre-delay', min: 0, max: 300, fmt: 'ms' },
    { id: 'damping', joke: 'Moody lighting', what: 'Damping (darker)', min: 0, max: 1, fmt: 'pct' },
    { id: 'shimmer', joke: 'Nobody understands me', what: 'Shimmer halo', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.5 } },
    { id: 'revWidth', joke: 'Plays to the back row', what: 'Reverb width', min: 0, max: 1.5, fmt: 'width' },
    { id: 'duck', joke: 'Pauses for applause', what: 'Ducking', min: 0, max: 1, fmt: 'pct' },
    { id: 'revMix', joke: 'Head in the clouds', what: 'Reverb level', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.6 } }
  ] },
  { id: 'out', on: 'outOn', name: 'Mastered by me', what: 'Multiband sat · width · output', knobs: [
    { id: 'mbSat', joke: 'Mastered it myself', what: 'Multiband saturation', min: 0, max: 1, fmt: 'pct', ego: { t: 'amt', e: 0.5 } },
    { id: 'outWidth', joke: 'Stadium in my head', what: 'Stereo width', min: 0, max: 2, fmt: 'width', ego: { t: 'width', e: 1.6 } },
    { id: 'mix', joke: 'I am the mix', what: 'Dry / wet', min: 0, max: 1, fmt: 'pct' },
    { id: 'outGain', joke: 'Volume of opinions', what: 'Output', min: -24, max: 12, fmt: 'db' }
  ] }
];

const FEL_KNOBS = [];
for (const s of FEL_SECTIONS) for (const k of s.knobs) FEL_KNOBS.push(k);

/* Presets: overrides on top of the engine defaults. ego = the EGO macro position. */
const FEL_PRESETS = [
  { name: 'Super Mega Sonic 5000', joke: "The signature sound. Fin's ego, bottled.", ego: 0.5, p: {
    warmth: 0.3, presence: 0.45, distType: 3, drive: 0.35, distMix: 0.35, crush: 0.04, tone: 7500,
    octDown: 0.08, octUp: 0.08, metal: 0.08,
    modType: 0, modAmt: 0.4, modRate: 0.35, modFb: 0.2, modWidth: 1.4,
    dlyType: 0, division: 6, feedback: 0.42, wow: 0.2, dlyMix: 0.26,
    revType: 0, decay: 6.5, predelay: 55, damping: 0.62, revWidth: 1.25, shimmer: 0.22, duck: 0.5, revMix: 0.38,
    mbSat: 0.35, outWidth: 1.3 } },
  { name: 'White Pony', joke: "Dreamy, dark, intimate. He's only heard it 400 times.", ego: 0.5, p: {
    warmth: 0.2, presence: 0.4, hpf: 100, distType: 0, drive: 0.15, distMix: 0.2,
    modType: 0, modAmt: 0.25, modRate: 0.3, modFb: 0.1, modWidth: 1.3,
    dlyType: 2, division: 6, feedback: 0.4, wow: 0.2, dlyMix: 0.25,
    revType: 0, decay: 5.5, predelay: 60, damping: 0.7, revWidth: 1.2, shimmer: 0.1, duck: 0.55, revMix: 0.38,
    mbSat: 0.2, outWidth: 1.2 } },
  { name: 'Digital Scream', joke: "Aggressive and metallic. It's called catharsis, Mum.", ego: 0.55, p: {
    inGain: 3, presence: 0.6, warmth: 0.4, distType: 1, drive: 0.75, crush: 0.12, tone: 6000, distMix: 0.7,
    metal: 0.25, octDown: 0.15,
    modType: 1, modAmt: 0.3, modRate: 0.2, modFb: 0.55, modWidth: 1.3,
    dlyType: 3, division: 2, feedback: 0.15, wow: 0.05, dlyMix: 0.2,
    revType: 1, decay: 1.8, predelay: 10, damping: 0.45, shimmer: 0, duck: 0.7, revMix: 0.2,
    mbSat: 0.5, outWidth: 1.1 } },
  { name: 'Y2K Machine', joke: 'Robotic and compressed. The autotune is ironic.', ego: 0.5, p: {
    presence: 0.5, robot: 1, formant: 1.5, metal: 0.3,
    distType: 1, drive: 0.2, crush: 0.2, distMix: 0.35,
    modType: 0, modAmt: 0.35, modRate: 1.2, modFb: 0.2, modWidth: 1.3,
    dlyType: 2, division: 2, feedback: 0.3, wow: 0.1, dlyMix: 0.2,
    revType: 1, decay: 1.6, predelay: 20, damping: 0.5, shimmer: 0, duck: 0.5, revMix: 0.18,
    mbSat: 0.35, outWidth: 1.2 } },
  { name: 'Shoegaze Ghost', joke: "Distant and washed out. You can't hear the words. That's the point.", ego: 0.55, p: {
    warmth: 0.25, presence: 0.25, distType: 0, drive: 0.2, distMix: 0.25, octUp: 0.15,
    modType: 0, modAmt: 0.6, modRate: 0.18, modFb: 0.25, modWidth: 1.6,
    dlyType: 1, division: 6, feedback: 0.55, wow: 0.45, dlyMix: 0.33,
    revType: 2, decay: 12, predelay: 80, damping: 0.6, revWidth: 1.4, shimmer: 0.45, duck: 0.35, revMix: 0.55,
    mbSat: 0.2, outWidth: 1.4 } },
  { name: 'Nu-Metal Chorus', joke: 'Wide, layered, powerful. Wallet chain sold separately.', ego: 0.55, p: {
    presence: 0.5, warmth: 0.3, distType: 3, drive: 0.55, distMix: 0.5, octDown: 0.25, octUp: 0.1,
    modType: 2, modAmt: 0.7, modRate: 0.25, modFb: 0.1, modWidth: 1.8,
    dlyType: 0, division: 4, feedback: 0.25, wow: 0.1, dlyMix: 0.18,
    revType: 0, decay: 2.8, predelay: 30, damping: 0.5, duck: 0.6, revMix: 0.25, shimmer: 0.05,
    mbSat: 0.45, outWidth: 1.5 } },
  { name: 'Broken Angel', joke: 'Whispered and haunting. Writes poetry in the tour van.', ego: 0.5, p: {
    gate: -55, presence: 0.55, warmth: 0.15, distType: 0, drive: 0.05, distMix: 0.1, formant: -1,
    modType: 0, modAmt: 0.55, modRate: 0.12, modFb: 0, modWidth: 1.3,
    dlyType: 2, division: 5, feedback: 0.45, wow: 0.3, dlyMix: 0.25,
    revType: 3, decay: 6, predelay: 220, damping: 0.75, shimmer: 0.1, duck: 0.3, revMix: 0.45, revWidth: 1.3,
    mbSat: 0.15, outWidth: 1.2 } },
  { name: 'Static Love', joke: "Dreamy, crunchy, flanged. It's about a feeling. You wouldn't get it.", ego: 0.5, p: {
    distType: 1, drive: 0.45, crush: 0.25, tone: 5000, distMix: 0.4,
    modType: 1, modAmt: 0.55, modRate: 0.15, modFb: 0.6, modWidth: 1.4,
    dlyType: 1, division: 5, feedback: 0.4, wow: 0.3, dlyMix: 0.25,
    revType: 0, decay: 6, predelay: 50, damping: 0.6, shimmer: 0.2, duck: 0.45, revMix: 0.4,
    mbSat: 0.3, outWidth: 1.3 } },
  { name: 'Underground 2001', joke: 'Raw, gritty, lo-fi. He was there, man.', ego: 0.5, p: {
    hpf: 180, presence: 0.3, distType: 2, drive: 0.6, crush: 0.45, tone: 3800, distMix: 0.6, metal: 0.15,
    modType: 2, modAmt: 0.3, modRate: 0.5, modFb: 0.1, modWidth: 1.1,
    dlyType: 1, division: 2, feedback: 0.35, wow: 0.55, dlyMix: 0.22,
    revType: 1, decay: 1.2, predelay: 5, damping: 0.65, shimmer: 0, duck: 0.3, revMix: 0.15,
    mbSat: 0.5, outWidth: 0.9 } },
  { name: 'Final Form', joke: 'Huge, polished, intense. Final form of what? Him.', ego: 0.6, p: {
    warmth: 0.3, presence: 0.55, distType: 3, drive: 0.4, distMix: 0.4, octDown: 0.18, octUp: 0.15,
    modType: 2, modAmt: 0.6, modRate: 0.3, modFb: 0.15, modWidth: 1.8,
    dlyType: 0, division: 6, feedback: 0.45, wow: 0.15, dlyMix: 0.28,
    revType: 2, decay: 8, predelay: 70, damping: 0.55, revWidth: 1.45, shimmer: 0.3, duck: 0.55, revMix: 0.45,
    mbSat: 0.45, outWidth: 1.6 } }
];

function felPresetState(i) {
  const pr = FEL_PRESETS[i];
  return Object.assign(FinEngine.defaults(), pr.p, { ego: pr.ego });
}

/* THE EGO macro. 0 = humble (natural, a little warmth), 0.5 = the knobs exactly as set,
   1 = every effect amount pushed toward its maximum-transformation value. */
function felApplyEgo(P) {
  const M = P.ego == null ? 0.5 : P.ego;
  const E = Object.assign({}, P);
  const lo = M <= 0.5, t = lo ? M / 0.5 : (M - 0.5) / 0.5, tp = Math.pow(t, 1.4);
  for (const k of FEL_KNOBS) {
    if (!k.ego) continue;
    const v = P[k.id], e = k.ego.e;
    if (k.ego.t === 'amt') {
      const fl = Math.min(v, k.ego.floor || 0);
      E[k.id] = lo ? fl + (v - fl) * t : v + Math.max(0, e - v) * tp;
    } else if (k.ego.t === 'width') {
      E[k.id] = lo ? 1 + (v - 1) * t : v + Math.max(0, e - v) * tp;
    } else if (k.ego.t === 'decay') {
      const d0 = Math.min(v, 1);
      E[k.id] = lo ? Math.exp(Math.log(d0) + (Math.log(v) - Math.log(d0)) * t) : v + Math.max(0, e - v) * Math.pow(t, 1.6);
    }
  }
  if (!lo) E.outGain = P.outGain - 2 * t;
  return E;
}

/* Synthetic sung "aah" phrase so the plug-in has something to chew on before a stem is loaded. */
function felSynthDemo(sr) {
  const notes = [
    [0.35, 0.75, 57, 'a'], [1.1, 0.45, 55, 'o'], [1.55, 1.35, 52, 'a'],
    [3.45, 0.4, 55, 'e'], [3.85, 0.4, 57, 'a'], [4.25, 0.55, 59, 'o'], [4.8, 1.6, 62, 'a'],
    [7.3, 0.5, 52, 'o'], [7.8, 0.5, 55, 'a'], [8.3, 1.7, 50, 'a']
  ];
  const V = { a: [730, 1090, 2440, 1, 0.5, 0.25], o: [570, 840, 2410, 1, 0.45, 0.15], e: [530, 1840, 2480, 1, 0.4, 0.3] };
  const len = Math.round(11.6 * sr), out = new Float32Array(len);
  const bw = [80, 100, 140];
  const bp = [new FelBiquad(), new FelBiquad(), new FelBiquad()], nh = new FelBiquad(); nh.set('hp', 1800, 0.7, 0, sr);
  let ph = 0, midiS = notes[0][2], tilt = 0, seed = 12345;
  const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296 * 2 - 1; };
  const glideK = 1 - Math.exp(-1 / (0.06 * sr)), envAtt = 1 - Math.exp(-1 / (0.06 * sr)), envRel = 1 - Math.exp(-1 / (0.18 * sr));
  let env = 0, ni = 0, F = V.a.slice(), peak = 0;
  for (let i = 0; i < len; i++) {
    const t = i / sr;
    while (ni < notes.length - 1 && t >= notes[ni + 1][0]) ni++;
    const nt = notes[ni], on = t >= nt[0] && t < nt[0] + nt[1];
    let target = nt[2];
    if (ni === 6 && t > nt[0] + 1.1) target = 59 + (62 - 59) * Math.max(0, 1 - (t - nt[0] - 1.1) / 0.3);
    midiS += (target - midiS) * glideK;
    const age = t - nt[0];
    const vib = Math.min(1, Math.max(0, (age - 0.25) / 0.4)) * 0.35 * Math.sin(FEL_TAU * 5.4 * t);
    const f0 = 440 * Math.pow(2, (midiS + vib - 69) / 12) * (1 + rnd() * 0.002);
    env += ((on ? 1 : 0) - env) * (on ? envAtt : envRel);
    const dt = f0 / sr; ph += dt; if (ph >= 1) ph -= 1;
    let s = 2 * ph - 1;
    if (ph < dt) { const x = ph / dt; s -= x + x - x * x - 1; } else if (ph > 1 - dt) { const x = (ph - 1) / dt; s -= x * x + x + x + 1; }
    tilt += (s - tilt) * 0.06;
    if ((i & 63) === 0) {
      const tv = V[nt[3]];
      for (let k = 0; k < 6; k++) F[k] += (tv[k] - F[k]) * 0.12;
      for (let k = 0; k < 3; k++) bp[k].set('bp', F[k], F[k] / bw[k], 0, sr);
    }
    const src = tilt * env;
    let y = bp[0].p(src) * F[3] + bp[1].p(src) * F[4] + bp[2].p(src) * F[5];
    y = y * 3 + src * 0.04 + nh.p(rnd()) * (0.012 * env + (age < 0.06 && age > 0 ? 0.02 : 0));
    out[i] = y; const a = y < 0 ? -y : y; if (a > peak) peak = a;
  }
  const g = 0.5 / (peak || 1);
  for (let i = 0; i < len; i++) out[i] *= g;
  return out;
}
