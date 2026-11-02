/* Fin's Ego Lift Vocal Lift Super Mega Sonic 5000 — DSP engine.
   Plain JS so the same code runs in an AudioWorklet, on the main thread (offline render) and in Node (tests). */
const FEL_TAU = Math.PI * 2;
function felDb(d) { return Math.pow(10, d / 20); }
function felClip(x) { return x > 1 ? 1 + Math.tanh(x - 1) * 0.5 : x < -1 ? -1 + Math.tanh(x + 1) * 0.5 : x; }
function felSoft(x) { const a = x < 0 ? -x : x; if (a <= 0.8) return x; const s = x < 0 ? -1 : 1; return s * (0.8 + 0.2 * Math.tanh((a - 0.8) / 0.2)); }

const FEL_DIV_BEATS = [0.25, 1 / 3, 0.5, 2 / 3, 0.75, 1, 1.5, 2, 3];
const FEL_SLAP_MS = [45, 60, 75, 90, 105, 120, 140, 160, 190];
const FEL_HALL_MS = [43.7, 51.3, 59.9, 67.1, 75.1, 84.3, 93.9, 107.3];
const FEL_PLATE_MS = [12.7, 17.3, 21.9, 26.3, 31.1, 35.7, 41.3, 47.9];
const FEL_AP_HALL = [4.77, 3.59, 12.73, 9.31];
const FEL_AP_PLATE = [2.9, 2.1, 7.3, 5.3];
const FEL_IN_SIGN = [1, -1, 1, 1, -1, 1, -1, -1];

class FelBiquad {
  constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
  set(type, f, q, g, fs) {
    f = Math.min(Math.max(f, 10), fs * 0.45);
    const w = FEL_TAU * f / fs, cw = Math.cos(w), sw = Math.sin(w), al = sw / (2 * q), A = Math.pow(10, g / 40);
    let b0, b1, b2, a0, a1, a2;
    if (type === 'lp') { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; }
    else if (type === 'hp') { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; }
    else if (type === 'bp') { b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; }
    else if (type === 'peak') { b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; }
    else if (type === 'hs') { const s = 2 * Math.sqrt(A) * al; b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s); a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; }
    else { const s = 2 * Math.sqrt(A) * al; b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s); a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; }
    this.b0 = b0 / a0; this.b1 = b1 / a0; this.b2 = b2 / a0; this.a1 = a1 / a0; this.a2 = a2 / a0;
  }
  p(x) { const y = this.b0 * x + this.z1; this.z1 = this.b1 * x - this.a1 * y + this.z2; this.z2 = this.b2 * x - this.a2 * y; return y; }
}

/* Ring delay. Read before write: rd(d) with d >= 1 returns the sample written d writes ago. */
class FelDelay {
  constructor(n) { let s = 1; while (s < n + 4) s <<= 1; this.b = new Float32Array(s); this.m = s - 1; this.w = 0; }
  rd(d) { const p = this.w - d; const i = Math.floor(p); const f = p - i; const a = this.b[i & this.m]; return a + (this.b[(i + 1) & this.m] - a) * f; }
  wr(x) { this.b[this.w] = x; this.w = (this.w + 1) & this.m; }
}

/* 2x oversampler, windowed-sinc halfband FIR, double-buffered history. */
class FelOS {
  constructor(h) { this.h = h; this.N = h.length; this.xu = new Float64Array(this.N * 2); this.xd = new Float64Array(this.N * 2); this.iu = 0; this.id = 0; }
  up(v) { const N = this.N; this.iu = this.iu === 0 ? N - 1 : this.iu - 1; this.xu[this.iu] = v; this.xu[this.iu + N] = v; let a = 0; const h = this.h, x = this.xu, o = this.iu; for (let k = 0; k < N; k++) a += h[k] * x[o + k]; return a; }
  down(v, out) { const N = this.N; this.id = this.id === 0 ? N - 1 : this.id - 1; this.xd[this.id] = v; this.xd[this.id + N] = v; if (!out) return 0; let a = 0; const h = this.h, x = this.xd, o = this.id; for (let k = 0; k < N; k++) a += h[k] * x[o + k]; return a; }
}

/* Delay-line granular pitch shifter (used for the shimmer halo). Write then read. */
class FelShift {
  constructor(fs, ratio, win) { this.dl = new FelDelay(Math.ceil(win * fs) + 8); this.win = win * fs; this.ph = 0; this.r = ratio; }
  p(x) {
    this.dl.wr(x);
    this.ph += (1 - this.r) / this.win; this.ph -= Math.floor(this.ph);
    const p1 = this.ph, p2 = p1 < 0.5 ? p1 + 0.5 : p1 - 0.5;
    const s = Math.sin(Math.PI * p1), w1 = s * s;
    return this.dl.rd(1 + p1 * this.win) * w1 + this.dl.rd(1 + p2 * this.win) * (1 - w1);
  }
}

/* YIN pitch tracker on a decimated copy of the preamp signal, with a short history of estimates. */
class FelPitch {
  constructor(fs) {
    this.fs = fs; this.D = Math.max(1, Math.round(fs / 12000)); this.dfs = fs / this.D;
    this.lpA = new FelBiquad(); this.lpB = new FelBiquad();
    this.lpA.set('lp', 1100, 0.707, 0, fs); this.lpB.set('lp', 1100, 0.707, 0, fs);
    this.W = Math.round(0.028 * this.dfs); this.minLag = Math.max(2, Math.floor(this.dfs / 1000)); this.maxLag = Math.ceil(this.dfs / 65);
    this.F = this.W + this.maxLag + 2; this.ring = new Float32Array(4096); this.rm = 4095; this.rw = 0;
    this.fr = new Float64Array(this.F); this.d = new Float64Array(this.maxLag + 2);
    this.hop = Math.max(16, Math.round(0.006 * this.dfs)); this.cnt = 0; this.dc = 0;
    this.T0 = fs / 150; this.voiced = false;
    this.hT = new Float64Array(64).fill(-1e12); this.hP = new Float64Array(64).fill(fs / 150); this.hV = new Uint8Array(64); this.hi = 0;
  }
  push(x, nAbs) {
    x = this.lpB.p(this.lpA.p(x));
    if (++this.dc < this.D) return; this.dc = 0;
    this.ring[this.rw] = x; this.rw = (this.rw + 1) & this.rm;
    if (++this.cnt >= this.hop) { this.cnt = 0; this.analyze(nAbs); }
  }
  analyze(nAbs) {
    const F = this.F, W = this.W, fr = this.fr, st = this.rw - F;
    for (let i = 0; i < F; i++) fr[i] = this.ring[(st + i) & this.rm];
    let e = 0; for (let i = 0; i < W; i++) e += fr[i] * fr[i];
    let voiced = false, T = this.T0;
    if (Math.sqrt(e / W) > 0.002) {
      const d = this.d, maxL = this.maxLag; let run = 0;
      for (let tau = 1; tau <= maxL; tau++) {
        let s = 0; for (let j = 0; j < W; j++) { const df = fr[j] - fr[j + tau]; s += df * df; }
        run += s; d[tau] = run > 0 ? s * tau / run : 1;
      }
      let sel = -1, best = -1, bestV = 1e9;
      for (let tau = this.minLag; tau <= maxL; tau++) {
        if (d[tau] < 0.15) { while (tau + 1 <= maxL && d[tau + 1] < d[tau]) tau++; sel = tau; break; }
        if (d[tau] < bestV) { bestV = d[tau]; best = tau; }
      }
      if (sel < 0 && best > 0 && bestV < 0.35) sel = best;
      if (sel > 0) {
        let t = sel;
        if (t > 1 && t < maxL) { const a = d[t - 1], b = d[t], c = d[t + 1], den = a - 2 * b + c; if (Math.abs(den) > 1e-12) t += 0.5 * (a - c) / den; }
        T = t * this.D; voiced = true;
      }
    }
    if (voiced) this.T0 = (this.voiced && Math.abs(T - this.T0) < 0.12 * this.T0) ? this.T0 + (T - this.T0) * 0.6 : T;
    this.voiced = voiced;
    this.hi = (this.hi + 1) & 63; this.hT[this.hi] = nAbs - (F / 2) * this.D; this.hP[this.hi] = this.T0; this.hV[this.hi] = voiced ? 1 : 0;
  }
  at(t) { let i = this.hi; for (let k = 0; k < 63; k++) { if (this.hT[i] <= t) return i; i = (i - 1) & 63; } return i; }
}

class FelPsola {
  constructor(size) { this.ola = new Float32Array(size); this.m = size - 1; this.init = false; this.mark = 0; this.am = 0; this.rOff = 0; }
}

/* Smoothed-parameter slots */
const FI = {
  inG: 0, warmW: 1, warmK: 2, dDrv: 3, dMix: 4, crushW: 5, cw: 6, odL: 7, ouL: 8, metW: 9, metFb: 10,
  modMix: 11, modDep: 12, modBL: 13, modBR: 14, modFb: 15, modW: 16,
  dlyT: 17, dlyTR: 18, dlyFb: 19, dlyWow: 20, dlyMix: 21,
  pre: 22, revMix: 23, shG: 24, revW: 25, sh5: 26, rev: 27,
  mb: 28, oW: 29, mix: 30, oG: 31, duck: 32, NS: 33
};

class FinEngine {
  static defaults() {
    return {
      preOn: 1, inGain: 0, gate: -80, hpf: 90, presence: 0.35, warmth: 0.2,
      distOn: 1, distType: 0, drive: 0.25, crush: 0, tone: 7000, distMix: 0.3,
      charOn: 1, pitch: 0, formant: 0, robot: 0, metal: 0, octDown: 0, octUp: 0,
      modOn: 1, modType: 0, modAmt: 0.3, modRate: 0.4, modFb: 0.2, modWidth: 1.3,
      dlyOn: 1, dlyType: 0, division: 4, bpm: 120, feedback: 0.35, wow: 0.15, dlyMix: 0.22,
      revOn: 1, revType: 0, decay: 4.5, predelay: 40, damping: 0.55, shimmer: 0.15, revWidth: 1.1, duck: 0.4, revMix: 0.3,
      outOn: 1, mbSat: 0.2, outWidth: 1.1, mix: 1, outGain: 0,
      bypass: 0, fxOnly: 0
    };
  }

  constructor(fs) {
    this.fs = fs;
    this.P = FinEngine.defaults();
    this._build();
    this.setParams(null, true);
  }

  reset() { this._build(); this.setParams(null, true); }

  _build() {
    const fs = this.fs, bq = () => new FelBiquad();
    // halfband FIR for the 2x oversampler
    const N = 31, h = new Float64Array(N); let hs = 0;
    for (let i = 0; i < N; i++) {
      const k = i - (N - 1) / 2, x = Math.PI * k / 2;
      const sinc = k === 0 ? 1 : Math.sin(x) / x;
      const w = 0.42 - 0.5 * Math.cos(FEL_TAU * i / (N - 1)) + 0.08 * Math.cos(2 * FEL_TAU * i / (N - 1));
      h[i] = sinc * w; hs += h[i];
    }
    for (let i = 0; i < N; i++) h[i] /= hs;
    this.osL = new FelOS(h); this.osR = new FelOS(h);
    { // measure oversampler latency with an impulse
      const t = new FelOS(h); let best = 0, bi = 0;
      for (let i = 0; i < 64; i++) { const x = i === 0 ? 1 : 0; t.down(t.up(2 * x), false); const y = Math.abs(t.down(t.up(0), true)); if (y > best) { best = y; bi = i; } }
      this.osLat = Math.max(1, bi);
    }
    // PSOLA geometry
    this.Tmax = fs / 65 + 8; this.hOutMax = this.Tmax / 0.7;
    this.L = Math.ceil(this.hOutMax + 1.5 * this.Tmax) + 8;
    this.Tunv = Math.round(0.0055 * fs);
    let rs = 1; while (rs < this.L * 4) rs <<= 1;
    this.inBuf = new Float32Array(rs); this.inM = rs - 1;
    let os = 1; while (os < (this.hOutMax * 2 + this.Tmax * 2) * 2) os <<= 1;
    this.ps0 = new FelPsola(os); this.ps1 = new FelPsola(os); this.ps2 = new FelPsola(os);
    this.pitch = new FelPitch(fs);
    this.N = 0;
    this.preLat = this.osLat + this.L;

    // smoothing
    const NS = FI.NS; this.S = new Float64Array(NS); this.T = new Float64Array(NS); this.K = new Float64Array(NS);
    const kf = 1 - Math.exp(-1 / (0.015 * fs)), ks = 1 - Math.exp(-1 / (0.12 * fs)), km = 1 - Math.exp(-1 / (0.06 * fs));
    this.K.fill(kf); this.K[FI.dlyT] = ks; this.K[FI.dlyTR] = ks; this.K[FI.pre] = ks; this.K[FI.modBL] = km; this.K[FI.modBR] = km;
    this.kLen = km;

    // dry / latency
    this.dryL = new FelDelay(this.preLat + 8); this.dryR = new FelDelay(this.preLat + 8);
    // preamp
    this.hpL1 = bq(); this.hpL2 = bq(); this.hpR1 = bq(); this.hpR2 = bq();
    this.prL = [bq(), bq(), bq()]; this.prR = [bq(), bq(), bq()];
    this.gEnv = 0; this.gG = 1; this.gHold = 0; this.gHoldN = Math.round(0.04 * fs); this.gateOn = false; this.gThr = 0;
    this.gAttK = 1 - Math.exp(-1 / (0.001 * fs)); this.gRelK = 1 - Math.exp(-1 / (0.06 * fs));
    this.gOpenK = 1 - Math.exp(-1 / (0.0015 * fs)); this.gCloseK = 1 - Math.exp(-1 / (0.09 * fs));
    // ducking
    this.dEnv = 0; this.duckG = 1; this.duckT = 1;
    this.dAttK = 1 - Math.exp(-1 / (0.01 * fs)); this.dRelK = 1 - Math.exp(-1 / (0.3 * fs));
    this.dgAttK = 1 - Math.exp(-1 / (0.025 * fs)); this.dgRelK = 1 - Math.exp(-1 / (0.28 * fs));
    // distortion
    this.dDistL = new FelDelay(this.osLat + 8); this.dDistR = new FelDelay(this.osLat + 8);
    this.deHpL = bq(); this.deHpR = bq(); this.dePkL = bq(); this.dePkR = bq();
    this.dToneL = bq(); this.dToneR = bq(); this.dHpL = bq(); this.dHpR = bq();
    this.dType = 0; this.crPh = 0; this.crInc = 1; this.crQ = 32768; this.crHL = 0; this.crHR = 0;
    this.agIn = 0; this.agOut = 0; this.ag = 1; this.agT = 1;
    this.agK = 1 - Math.exp(-1 / (0.25 * fs)); this.agSK = 1 - Math.exp(-1 / (0.08 * fs));
    // character
    this.dChL = new FelDelay(this.L + 8); this.dChR = new FelDelay(this.L + 8);
    this.pRatio = 1; this.fmt = 1; this.robot = 0;
    this.cmbA = new FelDelay(Math.ceil(0.01 * fs)); this.cmbB = new FelDelay(Math.ceil(0.01 * fs));
    this.cmbAd = 0.0029 * fs; this.cmbBd = 0.0043 * fs; this.cLpA = 0; this.cLpB = 0;
    // modulation
    this.mdL = new FelDelay(Math.ceil(0.07 * fs)); this.mdR = new FelDelay(Math.ceil(0.07 * fs));
    this.mPh = 0; this.mInc = 0; this.dr = 0; this.drT = 0; this.drC = 0; this.drN = Math.round(0.35 * fs);
    this.drK = 1 - Math.exp(-1 / (0.4 * fs)); this.seed = 0x2468ace;
    // delay
    this.dlL = new FelDelay(Math.ceil(7.2 * fs)); this.dlR = new FelDelay(Math.ceil(7.2 * fs));
    this.dPreHp = bq(); this.dPreLp = bq(); this.lpfL = bq(); this.hpfL = bq(); this.lpfR = bq(); this.hpfR = bq();
    this.dCross = true; this.dSat = 0; this.dCrush = 0; this.dCrushQ = 512;
    this.wPh = 0; this.fPh = 0; this.wInc = 0.55 / fs; this.fInc = 6.3 / fs;
    // reverb
    this.rPreHp = bq(); this.rPreLp = bq();
    this.pdl = new FelDelay(Math.ceil(0.36 * fs));
    this.rvl = new FelDelay(Math.ceil(1.6 * fs)); this.rvPh = 0; this.rvS = 0.3 * fs; this.rvInc = 1 / this.rvS;
    this.ap = []; this.apD = new Float64Array(4); this.apG = 0.62;
    for (let k = 0; k < 4; k++) this.ap.push(new FelDelay(Math.ceil(0.016 * fs)));
    this.fdn = []; this.fLen = new Float64Array(8); this.fLenS = new Float64Array(8); this.fG = new Float64Array(8);
    this.fLp = new Float64Array(8); this.fS = new Float64Array(8); this.fDampA = 0.5;
    for (let k = 0; k < 8; k++) this.fdn.push(new FelDelay(Math.ceil(FEL_HALL_MS[7] * fs / 1000) + 64));
    this.fmP = new Float64Array([0, 0.25, 0.5, 0.75]); this.fmI = new Float64Array([0.31 / fs, 0.47 / fs, 0.73 / fs, 0.91 / fs]);
    this.fModD = 0.00022 * fs;
    this.gs2 = new FelShift(fs, 2, 0.07); this.gs15 = new FelShift(fs, 1.5, 0.07);
    this.shHp = bq(); this.shLp = bq(); this.shHp.set('hp', 450, 0.7, 0, fs); this.shLp.set('lp', 7000, 0.7, 0, fs); this.shFb = 0;
    this.revOut = 0;
    // final
    this.mbSt = [0, 1].map(() => ({ l1: bq(), l2: bq(), m1: bq(), m2: bq() }));
    for (const s of this.mbSt) { s.l1.set('lp', 250, 0.7071, 0, fs); s.l2.set('lp', 250, 0.7071, 0, fs); s.m1.set('lp', 3500, 0.7071, 0, fs); s.m2.set('lp', 3500, 0.7071, 0, fs); }
    this.fxOnly = false;
    // limiter
    this.lD = Math.max(1, Math.round(0.0015 * fs)); this.lRN = this.lD + 1; this.lRing = new Float64Array(this.lRN).fill(1); this.lIdx = 0;
    this.lDL = new FelDelay(this.lD + 4); this.lDR = new FelDelay(this.lD + 4);
    this.lG = 1; this.lCeil = felDb(-1); this.lAttK = 1 - Math.exp(-1 / (this.lD * 0.22)); this.lRelK = 1 - Math.exp(-1 / (0.09 * fs));
    this.latency = this.preLat + this.lD;
    // meters
    this.mIn = 0; this.mOut = 0; this.mGr = 1;
  }

  setParams(p, init) {
    if (p) for (const k in p) if (k in this.P) this.P[k] = +p[k];
    const P = this.P, T = this.T, fs = this.fs;
    // --- preamp
    const pre = !!P.preOn;
    T[FI.inG] = felDb(P.inGain);
    const hpf = pre ? P.hpf : 20;
    this.hpL1.set('hp', hpf, 0.5412, 0, fs); this.hpL2.set('hp', hpf, 1.3066, 0, fs);
    this.hpR1.set('hp', hpf, 0.5412, 0, fs); this.hpR2.set('hp', hpf, 1.3066, 0, fs);
    const pr = pre ? P.presence : 0;
    for (const a of [this.prL, this.prR]) { a[0].set('peak', 3200, 0.9, 7 * pr, fs); a[1].set('hs', 9500, 0.7, 4 * pr, fs); a[2].set('peak', 330, 1.0, -3 * pr, fs); }
    const wm = pre ? P.warmth : 0; T[FI.warmW] = Math.min(1, wm * 3); T[FI.warmK] = 1 + wm * 5;
    this.gateOn = pre && P.gate > -79.5; this.gThr = felDb(P.gate);
    // --- distortion
    const don = !!P.distOn; this.dType = P.distType | 0;
    T[FI.dDrv] = felDb(P.drive * ([36, 30, 42, 36][this.dType] || 36));
    T[FI.dMix] = don ? P.distMix : 0;
    T[FI.crushW] = don ? Math.min(1, P.crush * 4) : 0;
    { const g = felDb(P.drive * ([36, 30, 42, 36][this.dType] || 36)), x = 0.08; this.agEst = Math.min(2, 0.85 * x / Math.max(1e-3, Math.abs(this._shape(g * x * 1.4)))); }
    this.crQ = Math.pow(2, 15 - P.crush * 12); this.crInc = 1 / (1 + Math.pow(P.crush, 1.6) * 15);
    this.deHpL.set('hp', 160, 0.6, 0, fs); this.deHpR.set('hp', 160, 0.6, 0, fs);
    this.dePkL.set('peak', 2000, 0.8, 5, fs); this.dePkR.set('peak', 2000, 0.8, 5, fs);
    this.dToneL.set('lp', P.tone, 0.6, 0, fs); this.dToneR.set('lp', P.tone, 0.6, 0, fs);
    this.dHpL.set('hp', 100, 0.707, 0, fs); this.dHpR.set('hp', 100, 0.707, 0, fs);
    // --- character
    const con = !!P.charOn;
    this.pRatio = Math.pow(2, P.pitch / 12);
    this.fmt = Math.min(1.42, Math.max(0.7, Math.pow(2, P.formant / 12)));
    this.robot = con ? P.robot : 0;
    const act = Math.abs(P.pitch) > 0.01 || Math.abs(P.formant) > 0.01 || this.robot > 0.001;
    T[FI.cw] = con && act ? 1 : 0;
    T[FI.odL] = con ? P.octDown * 0.9 : 0; T[FI.ouL] = con ? P.octUp * 0.7 : 0;
    T[FI.metW] = con ? P.metal : 0; T[FI.metFb] = 0.45 + 0.45 * P.metal;
    // --- modulation
    const mon = !!P.modOn, amt = P.modAmt, mt = P.modType | 0;
    const M = mt === 1 ? { bL: 0.0012, bR: 0.0017, dep: 0.0035 * (0.3 + 0.7 * amt), fb: 0.85, mix: Math.min(1, amt * 2) * 0.7, rate: 1 }
      : mt === 2 ? { bL: 0.024, bR: 0.035, dep: 0.0006 + 0.0014 * amt, fb: 0.3, mix: Math.min(1, amt * 1.5) * 0.75, rate: 0.5 }
        : { bL: 0.011, bR: 0.0145, dep: 0.0035 * amt + 0.0005, fb: 0.35, mix: Math.min(1, amt * 1.8) * 0.55, rate: 1 };
    T[FI.modMix] = mon ? M.mix : 0; T[FI.modDep] = M.dep; T[FI.modBL] = M.bL; T[FI.modBR] = M.bR;
    T[FI.modFb] = P.modFb * M.fb; T[FI.modW] = mon ? P.modWidth : 1;
    this.mInc = P.modRate * M.rate / fs;
    // --- delay
    const yon = !!P.dlyOn, dt = P.dlyType | 0, div = Math.max(0, Math.min(8, Math.round(P.division)));
    let t = dt === 3 ? FEL_SLAP_MS[div] / 1000 : FEL_DIV_BEATS[div] * 60 / Math.max(30, P.bpm);
    let tR = dt === 0 ? t : dt === 1 ? t + 0.013 : dt === 2 ? t * 1.5 : t + 0.009;
    t = Math.min(t, 6.9); tR = Math.min(tR, 6.9);
    T[FI.dlyT] = t * fs; T[FI.dlyTR] = tR * fs;
    this.dCross = dt === 0;
    const DT = [{ lp: 7500, hp: 180, sat: 0.15, cr: 0, ws: 0.35 }, { lp: 4200, hp: 260, sat: 1, cr: 0, ws: 1 }, { lp: 2300, hp: 320, sat: 0.3, cr: 1, ws: 0.45 }, { lp: 6000, hp: 200, sat: 0.5, cr: 0, ws: 0.15 }][dt] || { lp: 7500, hp: 180, sat: 0.15, cr: 0, ws: 0.35 };
    this.lpfL.set('lp', DT.lp, 0.6, 0, fs); this.lpfR.set('lp', DT.lp, 0.6, 0, fs);
    this.hpfL.set('hp', DT.hp, 0.6, 0, fs); this.hpfR.set('hp', DT.hp, 0.6, 0, fs);
    this.dPreHp.set('hp', 150, 0.7, 0, fs); this.dPreLp.set('lp', Math.min(14000, DT.lp * 1.4), 0.7, 0, fs);
    this.dSat = DT.sat; this.dCrush = DT.cr;
    T[FI.dlyFb] = dt === 3 ? Math.min(P.feedback, 0.35) : Math.min(0.95, P.feedback);
    T[FI.dlyWow] = P.wow * DT.ws * 0.0035;
    T[FI.dlyMix] = yon ? P.dlyMix : 0;
    // --- reverb
    const ron = !!P.revOn, rt = P.revType | 0, plate = rt === 1;
    const MS = plate ? FEL_PLATE_MS : FEL_HALL_MS;
    const rt60 = Math.max(0.2, P.decay);
    for (let k = 0; k < 8; k++) { this.fLen[k] = MS[k] * fs / 1000; this.fG[k] = Math.pow(10, -3 * this.fLen[k] / (rt60 * fs)); }
    const fc = 1500 * Math.pow(8, 1 - P.damping) * (plate ? 1.25 : 1) * (rt === 2 ? 1.2 : 1);
    this.fDampA = Math.exp(-FEL_TAU * Math.min(fc, fs * 0.45) / fs);
    const AP = plate ? FEL_AP_PLATE : FEL_AP_HALL;
    for (let k = 0; k < 4; k++) this.apD[k] = Math.max(1, Math.round(AP[k] * fs / 1000));
    this.apG = plate ? 0.7 : 0.62;
    this.rPreHp.set('hp', 170, 0.7, 0, fs); this.rPreLp.set('lp', rt === 2 ? 12000 : plate ? 10000 : 8500, 0.7, 0, fs);
    T[FI.pre] = Math.max(1, rt === 3 ? 0.01 * fs : P.predelay / 1000 * fs);
    T[FI.rev] = rt === 3 ? 1 : 0;
    this.rvS = (0.15 + Math.min(300, Math.max(0, P.predelay)) / 300 * 0.6) * fs; this.rvInc = 1 / this.rvS;
    T[FI.revMix] = ron ? P.revMix * 1.0 : 0;
    T[FI.revW] = P.revWidth;
    T[FI.shG] = ron ? P.shimmer * 0.55 : 0; T[FI.sh5] = rt === 2 ? 0.6 : 0;
    T[FI.duck] = P.duck;
    // --- final
    const oon = !!P.outOn;
    T[FI.mb] = oon ? P.mbSat : 0; T[FI.oW] = oon ? P.outWidth : 1;
    this.fxOnly = !!P.fxOnly;
    T[FI.mix] = P.bypass ? 0 : (this.fxOnly ? 1 : P.mix);
    T[FI.oG] = felDb(P.outGain);
    if (init) { this.S.set(T); this.fLenS.set(this.fLen); this.ag = this.agT = this.agEst; }
  }

  _shape(v) {
    switch (this.dType) {
      case 1: return v > 1 ? 1 : v < -1 ? -1 : v;
      case 2: { const a = v * 1.6, sc = a / (1 + (a < 0 ? -a : a)); return 0.85 * sc + 0.4 * ((sc < 0 ? -sc : sc) - 0.35); }
      case 3: return v > 0 ? Math.tanh(1.2 * v) : 1.6 * Math.tanh(0.45 * v);
      default: return Math.tanh(v + 0.08) - 0.07983;
    }
  }

  _ps(s, ratioMul, f) {
    const N = this.N, L = this.L, buf = this.inBuf, m = this.inM, pt = this.pitch, fs = this.fs;
    if (!s.init) { s.init = true; s.mark = N; s.am = N - L; s.ola.fill(0); s.rOff = 0; }
    const lim = N + this.hOutMax;
    while (s.mark <= lim) {
      const d = s.mark - L;
      const hi = pt.at(d - this.osLat);
      const voiced = pt.hV[hi] === 1;
      const T = voiced ? pt.hP[hi] : this.Tunv;
      let ratio = ratioMul;
      if (voiced && this.robot > 0.001) {
        const midi = 69 + 12 * Math.log2((fs / T) * ratioMul / 440);
        const off = (Math.round(midi) - midi) * Math.min(1, this.robot * 1.15);
        s.rOff += (off - s.rOff) * (0.12 + 0.88 * this.robot * this.robot);
        ratio *= Math.pow(2, s.rOff / 12);
      }
      if (Math.abs(s.am - d) > 2 * T) s.am = d;
      else { while (s.am + 0.5 * T < d) s.am += T; while (s.am - 0.5 * T > d) s.am -= T; }
      const hOut = T / f, Tout = voiced ? T / ratio : T;
      const sc = Math.min(1.6, Tout / hOut), invH = Math.PI / hOut;
      let k0 = Math.ceil(s.mark - hOut); const k1 = Math.floor(s.mark + hOut);
      if (k0 < N) k0 = N;
      const ola = s.ola, om = s.m;
      for (let k = k0; k <= k1; k++) {
        const t = k - s.mark, w = 0.5 + 0.5 * Math.cos(t * invH);
        const pos = s.am + t * f, ip = Math.floor(pos), fr = pos - ip;
        const x0 = buf[ip & m];
        ola[k & om] += (x0 + (buf[(ip + 1) & m] - x0) * fr) * w * sc;
      }
      s.mark += Tout;
    }
    const idx = N & s.m, y = s.ola[idx]; s.ola[idx] = 0; return y;
  }

  _dproc(lp, hp, y) {
    let v = lp.p(hp.p(y));
    if (this.dSat > 0) v += (Math.tanh(v * 1.5) / 1.5 - v) * this.dSat;
    if (this.dCrush) v = Math.round(v * this.dCrushQ) / this.dCrushQ;
    return v;
  }

  _mb(x, st, amt) {
    const low = st.l2.p(st.l1.p(x)), rest = x - low, mid = st.m2.p(st.m1.p(rest)), high = rest - mid;
    const kl = 1 + amt * 1.5, km = 1 + amt * 4, kh = 1 + amt * 2.5;
    return Math.tanh(low * kl) / kl + Math.tanh(mid * km) / km + Math.tanh(high * kh) / kh;
  }

  takeMeters() { const r = { i: this.mIn, o: this.mOut, g: this.mGr }; this.mIn = 0; this.mOut = 0; this.mGr = 1; return r; }

  process(inL, inR, outL, outR, n) {
    const S = this.S, T = this.T, K = this.K, NS = FI.NS, fs = this.fs;
    for (let i = 0; i < n; i++) {
      for (let j = 0; j < NS; j++) S[j] += (T[j] - S[j]) * K[j];
      let xL = inL[i], xR = inR[i];
      if (xL !== xL) xL = 0; if (xR !== xR) xR = 0;
      const ia = Math.max(xL < 0 ? -xL : xL, xR < 0 ? -xR : xR); if (ia > this.mIn) this.mIn = ia;
      const dryL = this.dryL.rd(this.preLat), dryR = this.dryR.rd(this.preLat); this.dryL.wr(xL); this.dryR.wr(xR);

      // ---------- PREAMP
      let a = xL * S[FI.inG], b = xR * S[FI.inG];
      a = this.hpL2.p(this.hpL1.p(a)); b = this.hpR2.p(this.hpR1.p(b));
      {
        const m = Math.abs(a + b) * 0.5;
        this.gEnv += (m - this.gEnv) * (m > this.gEnv ? this.gAttK : this.gRelK);
        let tg = 1;
        if (this.gateOn) {
          if (this.gEnv >= this.gThr) this.gHold = this.gHoldN;
          else if (this.gHold > 0) this.gHold--;
          else { const below = 20 * Math.log10(this.gThr / (this.gEnv + 1e-9)); tg = felDb(-Math.min(40, below * 2)); }
        }
        this.gG += (tg - this.gG) * (tg > this.gG ? this.gOpenK : this.gCloseK);
        a *= this.gG; b *= this.gG;
      }
      a = this.prL[2].p(this.prL[1].p(this.prL[0].p(a))); b = this.prR[2].p(this.prR[1].p(this.prR[0].p(b)));
      { const w = S[FI.warmW]; if (w > 1e-4) { const k = S[FI.warmK]; a += (Math.tanh(k * a) / k - a) * w; b += (Math.tanh(k * b) / k - b) * w; } }
      const pm = 0.5 * (a + b);
      this.pitch.push(pm, this.N);
      {
        const m = pm < 0 ? -pm : pm;
        this.dEnv += (m - this.dEnv) * (m > this.dEnv ? this.dAttK : this.dRelK);
        if ((this.N & 7) === 0) { const lv = 20 * Math.log10(this.dEnv + 1e-9); const over = Math.min(1, Math.max(0, (lv + 44) / 28)); this.duckT = felDb(-S[FI.duck] * over * 20); }
        this.duckG += (this.duckT - this.duckG) * (this.duckT < this.duckG ? this.dgAttK : this.dgRelK);
      }

      // ---------- DISTORTION (2x oversampled)
      {
        const dA = this.dDistL.rd(this.osLat), dB = this.dDistR.rd(this.osLat); this.dDistL.wr(a); this.dDistR.wr(b);
        const mix = S[FI.dMix];
        if (mix > 1e-4 || T[FI.dMix] > 0) {
          const g = S[FI.dDrv];
          const pa = this.dePkL.p(this.deHpL.p(a)) * g, pb = this.dePkR.p(this.deHpR.p(b)) * g;
          let o = this.osL; o.down(this._shape(o.up(2 * pa)), false); let ya = o.down(this._shape(o.up(0)), true);
          o = this.osR; o.down(this._shape(o.up(2 * pb)), false); let yb = o.down(this._shape(o.up(0)), true);
          const cw = S[FI.crushW];
          if (cw > 1e-4) {
            this.crPh += this.crInc;
            if (this.crPh >= 1) { this.crPh -= 1; const q = this.crQ; this.crHL = Math.round(ya * q) / q; this.crHR = Math.round(yb * q) / q; }
            ya += (this.crHL - ya) * cw; yb += (this.crHR - yb) * cw;
          }
          ya = this.dHpL.p(this.dToneL.p(ya)); yb = this.dHpR.p(this.dToneR.p(yb));
          this.agIn += ((dA * dA + dB * dB) * 0.5 - this.agIn) * this.agK;
          this.agOut += ((ya * ya + yb * yb) * 0.5 - this.agOut) * this.agK;
          if (this.agIn > 1e-6) { let t = 0.85 * Math.sqrt(this.agIn / (this.agOut + 1e-12)); this.agT = t < 0.05 ? 0.05 : t > 2 ? 2 : t; }
          else { this.agT = this.agEst; this.ag += (this.agEst - this.ag) * 0.01; }
          this.ag += (this.agT - this.ag) * this.agSK;
          a = dA + (ya * this.ag - dA) * mix; b = dB + (yb * this.ag - dB) * mix;
        } else { a = dA; b = dB; }
      }

      // ---------- CHARACTER (PSOLA pitch / formant / robot, octave layers, metallic combs)
      {
        this.inBuf[this.N & this.inM] = 0.5 * (a + b);
        const dA = this.dChL.rd(this.L), dB = this.dChR.rd(this.L); this.dChL.wr(a); this.dChR.wr(b);
        let ca = dA, cb = dB;
        const cw = S[FI.cw];
        if (cw > 1e-4 || T[FI.cw] > 0) { const y = this._ps(this.ps0, this.pRatio, this.fmt); ca = dA + (y - dA) * cw; cb = dB + (y - dB) * cw; } else this.ps0.init = false;
        const od = S[FI.odL];
        if (od > 1e-4 || T[FI.odL] > 0) { const y = this._ps(this.ps1, this.pRatio * 0.5, this.fmt) * od; ca += y; cb += y; } else this.ps1.init = false;
        const ou = S[FI.ouL];
        if (ou > 1e-4 || T[FI.ouL] > 0) { const y = this._ps(this.ps2, this.pRatio * 2, this.fmt) * ou; ca += y; cb += y; } else this.ps2.init = false;
        a = ca; b = cb;
        const mw = S[FI.metW];
        if (mw > 1e-4 || T[FI.metW] > 0) {
          const fb = S[FI.metFb], m = 0.5 * (a + b);
          const vA = this.cmbA.rd(this.cmbAd), vB = this.cmbB.rd(this.cmbBd);
          this.cLpA += (vA - this.cLpA) * 0.55; this.cLpB += (vB - this.cLpB) * 0.55;
          this.cmbA.wr(m + this.cLpA * fb + 1e-18); this.cmbB.wr(m + this.cLpB * fb + 1e-18);
          const y = (vA + vB) * 0.75 * (1 - fb) * 2;
          a = a * (1 - 0.35 * mw) + y * mw; b = b * (1 - 0.35 * mw) + y * mw;
        }
        this.N++;
      }

      // ---------- Y2K MODULATION
      {
        const mix = S[FI.modMix];
        if (mix > 1e-4 || T[FI.modMix] > 0) {
          this.mPh += this.mInc; if (this.mPh >= 1) this.mPh -= 1;
          if (--this.drC <= 0) { this.drC = this.drN; this.seed = (Math.imul(this.seed, 1664525) + 1013904223) >>> 0; this.drT = this.seed / 4294967296 * 2 - 1; }
          this.dr += (this.drT - this.dr) * this.drK;
          const lL = Math.sin(FEL_TAU * this.mPh), lR = Math.sin(FEL_TAU * (this.mPh + 0.25)), dep = S[FI.modDep];
          let dL = (S[FI.modBL] + dep * (0.5 + 0.5 * (0.78 * lL + 0.22 * this.dr))) * fs;
          let dR = (S[FI.modBR] + dep * (0.5 + 0.5 * (0.78 * lR - 0.22 * this.dr))) * fs;
          if (dL < 1) dL = 1; if (dR < 1) dR = 1;
          const vL = this.mdL.rd(dL), vR = this.mdR.rd(dR), fb = S[FI.modFb];
          let wL = a + vL * fb, wR = b + vR * fb;
          if (wL > 1.5 || wL < -1.5) wL = Math.tanh(wL / 1.5) * 1.5; if (wR > 1.5 || wR < -1.5) wR = Math.tanh(wR / 1.5) * 1.5;
          this.mdL.wr(wL + 1e-18); this.mdR.wr(wR + 1e-18);
          const g = 1 / (1 + 0.5 * mix);
          a = (a + vL * mix) * g; b = (b + vR * mix) * g;
        } else { this.mdL.wr(a); this.mdR.wr(b); }
        const w = S[FI.modW];
        if (w < 0.999 || w > 1.001) { const Mm = (a + b) * 0.5, Sd = (a - b) * 0.5 * w; a = Mm + Sd; b = Mm - Sd; }
      }

      // ---------- DELAY
      let eL = 0, eR = 0;
      {
        const mix = S[FI.dlyMix];
        if (mix > 1e-4 || T[FI.dlyMix] > 0) {
          const x = this.dPreLp.p(this.dPreHp.p(0.5 * (a + b)));
          this.wPh += this.wInc; if (this.wPh >= 1) this.wPh -= 1;
          this.fPh += this.fInc; if (this.fPh >= 1) this.fPh -= 1;
          const wm = S[FI.dlyWow] * fs * (Math.sin(FEL_TAU * this.wPh) + 0.25 * Math.sin(FEL_TAU * this.fPh) + 0.3 * this.dr);
          let tL = S[FI.dlyT] + wm, tR = S[FI.dlyTR] + wm * 0.85;
          if (tL < 2) tL = 2; if (tR < 2) tR = 2;
          const yL = this.dlL.rd(tL), yR = this.dlR.rd(tR), fb = S[FI.dlyFb];
          let wL, wR;
          if (this.dCross) { const s = Math.sqrt(fb); wL = x + this._dproc(this.lpfL, this.hpfL, yR) * s; wR = yL * s; }
          else { wL = x + this._dproc(this.lpfL, this.hpfL, yL) * fb; wR = x + this._dproc(this.lpfR, this.hpfR, yR) * fb; }
          this.dlL.wr(felClip(wL) + 1e-18); this.dlR.wr(felClip(wR) + 1e-18);
          const dg = mix * this.duckG; eL = yL * dg; eR = yR * dg;
        } else { this.dlL.wr(0); this.dlR.wr(0); }
      }

      // ---------- REVERB (8-line FDN, shimmer loop, reverse grains)
      let rL = 0, rR = 0;
      {
        const lvl = S[FI.revMix];
        if (lvl > 1e-4 || T[FI.revMix] > 0) {
          let x = 0.5 * (a + b) + (eL + eR) * 0.22 + this.shFb;
          x = this.rPreLp.p(this.rPreHp.p(x));
          const pd = S[FI.pre]; const pv = this.pdl.rd(pd < 1 ? 1 : pd); this.pdl.wr(x); x = pv;
          const rv = S[FI.rev];
          this.rvl.wr(x);
          if (rv > 1e-4) {
            this.rvPh += this.rvInc; if (this.rvPh >= 1) this.rvPh -= 1;
            const p1 = this.rvPh, p2 = p1 < 0.5 ? p1 + 0.5 : p1 - 0.5, Sg = this.rvS;
            const s1 = Math.sin(Math.PI * p1), w1 = s1 * s1;
            const y = this.rvl.rd(1 + 2 * Sg * p1) * w1 + this.rvl.rd(1 + 2 * Sg * p2) * (1 - w1);
            x += (y * 1.25 - x * 0.85) * rv;
          }
          const apG = this.apG;
          for (let k = 0; k < 4; k++) { const ap = this.ap[k], v = ap.rd(this.apD[k]), w = x + apG * v; x = v - apG * w; ap.wr(w); }
          const Ld = this.fdn, len = this.fLenS, tl = this.fLen, g = this.fG, lp = this.fLp, s = this.fS, da = this.fDampA, kl = this.kLen;
          for (let k = 0; k < 8; k++) {
            len[k] += (tl[k] - len[k]) * kl;
            let d = len[k];
            if (k < 4) { this.fmP[k] += this.fmI[k]; if (this.fmP[k] >= 1) this.fmP[k] -= 1; d += this.fModD * Math.sin(FEL_TAU * this.fmP[k]); }
            s[k] = Ld[k].rd(d);
          }
          const v0 = (lp[0] = s[0] + (lp[0] - s[0]) * da) * g[0], v1 = (lp[1] = s[1] + (lp[1] - s[1]) * da) * g[1];
          const v2 = (lp[2] = s[2] + (lp[2] - s[2]) * da) * g[2], v3 = (lp[3] = s[3] + (lp[3] - s[3]) * da) * g[3];
          const v4 = (lp[4] = s[4] + (lp[4] - s[4]) * da) * g[4], v5 = (lp[5] = s[5] + (lp[5] - s[5]) * da) * g[5];
          const v6 = (lp[6] = s[6] + (lp[6] - s[6]) * da) * g[6], v7 = (lp[7] = s[7] + (lp[7] - s[7]) * da) * g[7];
          const a0 = v0 + v1, a1 = v0 - v1, a2 = v2 + v3, a3 = v2 - v3, a4 = v4 + v5, a5 = v4 - v5, a6 = v6 + v7, a7 = v6 - v7;
          const b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3, b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
          const hn = 0.35355339, xi = x * 0.5;
          Ld[0].wr(xi * FEL_IN_SIGN[0] + (b0 + b4) * hn + 1e-18); Ld[1].wr(xi * FEL_IN_SIGN[1] + (b1 + b5) * hn + 1e-18);
          Ld[2].wr(xi * FEL_IN_SIGN[2] + (b2 + b6) * hn + 1e-18); Ld[3].wr(xi * FEL_IN_SIGN[3] + (b3 + b7) * hn + 1e-18);
          Ld[4].wr(xi * FEL_IN_SIGN[4] + (b0 - b4) * hn + 1e-18); Ld[5].wr(xi * FEL_IN_SIGN[5] + (b1 - b5) * hn + 1e-18);
          Ld[6].wr(xi * FEL_IN_SIGN[6] + (b2 - b6) * hn + 1e-18); Ld[7].wr(xi * FEL_IN_SIGN[7] + (b3 - b7) * hn + 1e-18);
          let wL = (s[0] - s[3] + s[4] - s[7]) * 0.5, wR = (s[1] - s[2] + s[5] - s[6]) * 0.5;
          const sg = S[FI.shG];
          if (sg > 1e-4) {
            const mono = (wL + wR) * 0.5;
            let sh = this.gs2.p(mono) + this.gs15.p(mono) * S[FI.sh5];
            sh = this.shLp.p(this.shHp.p(sh)) * sg;
            this.shFb = sh > 0.6 || sh < -0.6 ? Math.tanh(sh / 0.6) * 0.6 : sh;
          } else this.shFb = 0;
          const rw = S[FI.revW], Mw = (wL + wR) * 0.5, Sw = (wL - wR) * 0.5 * rw, rg = lvl * this.duckG;
          rL = (Mw + Sw) * rg; rR = (Mw - Sw) * rg;
        }
      }

      // ---------- FINAL
      let oL, oR;
      {
        let xa = this.fxOnly ? eL + rL : a + eL + rL, xb = this.fxOnly ? eR + rR : b + eR + rR;
        const mb = S[FI.mb];
        if (mb > 1e-4) { xa = this._mb(xa, this.mbSt[0], mb); xb = this._mb(xb, this.mbSt[1], mb); }
        xa = felSoft(xa); xb = felSoft(xb);
        const w = S[FI.oW];
        if (w < 0.999 || w > 1.001) { const Mm = (xa + xb) * 0.5, Sd = (xa - xb) * 0.5 * w; xa = Mm + Sd; xb = Mm - Sd; }
        const mx = S[FI.mix], og = S[FI.oG];
        oL = (dryL + (xa - dryL) * mx) * og; oR = (dryR + (xb - dryR) * mx) * og;
      }

      // ---------- LIMITER (1.5 ms lookahead, -1 dBFS ceiling)
      {
        const pk = Math.max(oL < 0 ? -oL : oL, oR < 0 ? -oR : oR), c = this.lCeil;
        this.lRing[this.lIdx] = pk > c ? c / pk : 1; this.lIdx = this.lIdx + 1 === this.lRN ? 0 : this.lIdx + 1;
        let mn = 1; const rg = this.lRing; for (let k = 0; k < this.lRN; k++) if (rg[k] < mn) mn = rg[k];
        this.lG += (mn - this.lG) * (mn < this.lG ? this.lAttK : this.lRelK);
        const yL = this.lDL.rd(this.lD), yR = this.lDR.rd(this.lD); this.lDL.wr(oL); this.lDR.wr(oR);
        let zL = yL * this.lG, zR = yR * this.lG;
        zL = zL > c ? c : zL < -c ? -c : zL; zR = zR > c ? c : zR < -c ? -c : zR;
        outL[i] = zL; outR[i] = zR;
        const op = Math.max(zL < 0 ? -zL : zL, zR < 0 ? -zR : zR); if (op > this.mOut) this.mOut = op;
        if (this.lG < this.mGr) this.mGr = this.lG;
      }
    }
    if (n > 0 && (outL[n - 1] !== outL[n - 1] || outR[n - 1] !== outR[n - 1])) { this.reset(); outL.fill(0); outR.fill(0); }
  }
}
