// Fin's Ego Lift Vocal Lift Super Mega Sonic 5000 — DSP engine (C++ port of the browser engine).
// Header-only, no JUCE dependency, so it can be unit-tested on its own.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fel
{
constexpr double TAU = 6.283185307179586;
constexpr double PI = 3.141592653589793;

inline double dbg (double d) { return std::pow (10.0, d / 20.0); }
inline double jsRound (double x) { return std::floor (x + 0.5); }
inline double clipFb (double x) { return x > 1 ? 1 + std::tanh (x - 1) * 0.5 : x < -1 ? -1 + std::tanh (x + 1) * 0.5 : x; }
inline double softClip (double x)
{
    const double a = x < 0 ? -x : x;
    if (a <= 0.8) return x;
    const double s = x < 0 ? -1.0 : 1.0;
    return s * (0.8 + 0.2 * std::tanh ((a - 0.8) / 0.2));
}

static const double DIV_BEATS[9] = { 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1, 1.5, 2, 3 };
static const double SLAP_MS[9] = { 45, 60, 75, 90, 105, 120, 140, 160, 190 };
static const double HALL_MS[8] = { 43.7, 51.3, 59.9, 67.1, 75.1, 84.3, 93.9, 107.3 };
static const double PLATE_MS[8] = { 12.7, 17.3, 21.9, 26.3, 31.1, 35.7, 41.3, 47.9 };
static const double AP_HALL[4] = { 4.77, 3.59, 12.73, 9.31 };
static const double AP_PLATE[4] = { 2.9, 2.1, 7.3, 5.3 };
static const double IN_SIGN[8] = { 1, -1, 1, 1, -1, 1, -1, -1 };

struct Biquad
{
    enum Type { LP, HP, BP, PEAK, HS, LS };
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (Type type, double f, double q, double g, double fs)
    {
        f = std::min (std::max (f, 10.0), fs * 0.45);
        const double w = TAU * f / fs, cw = std::cos (w), sw = std::sin (w), al = sw / (2 * q), A = std::pow (10.0, g / 40.0);
        double B0, B1, B2, A0, A1, A2;
        switch (type)
        {
            case LP: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case HP: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case BP: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
            case PEAK: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
            case HS:
            {
                const double s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break;
            }
            case LS:
            default:
            {
                const double s = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    inline double p (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Ring delay. Read before write: rd(d) with d >= 1 returns the sample written d writes ago.
struct Delay
{
    std::vector<float> b;
    int m = 0, w = 0;
    void init (double n)
    {
        int s = 1;
        while (s < (int) n + 4) s <<= 1;
        b.assign ((size_t) s, 0.0f); m = s - 1; w = 0;
    }
    inline double rd (double d) const
    {
        const double p = (double) w - d, fi = std::floor (p);
        const int i = (int) fi;
        const double f = p - fi, a = b[(size_t) (i & m)];
        return a + ((double) b[(size_t) ((i + 1) & m)] - a) * f;
    }
    inline void wr (double x) { b[(size_t) w] = (float) x; w = (w + 1) & m; }
};

struct OS2
{
    const double* h = nullptr;
    int N = 0, iu = 0, id = 0;
    std::vector<double> xu, xd;
    void init (const double* hh, int n) { h = hh; N = n; xu.assign ((size_t) (2 * N), 0.0); xd.assign ((size_t) (2 * N), 0.0); iu = id = 0; }
    inline double up (double v)
    {
        iu = iu == 0 ? N - 1 : iu - 1; xu[(size_t) iu] = v; xu[(size_t) (iu + N)] = v;
        double a = 0; const double* x = xu.data() + iu;
        for (int k = 0; k < N; ++k) a += h[k] * x[k];
        return a;
    }
    inline double down (double v, bool out)
    {
        id = id == 0 ? N - 1 : id - 1; xd[(size_t) id] = v; xd[(size_t) (id + N)] = v;
        if (! out) return 0;
        double a = 0; const double* x = xd.data() + id;
        for (int k = 0; k < N; ++k) a += h[k] * x[k];
        return a;
    }
};

// Delay-line granular pitch shifter (shimmer halo). Write then read.
struct Shift
{
    Delay dl; double win = 1, ph = 0, r = 1;
    void init (double fs, double ratio, double winSec) { dl.init (std::ceil (winSec * fs) + 8); win = winSec * fs; ph = 0; r = ratio; }
    inline double p (double x)
    {
        dl.wr (x);
        ph += (1 - r) / win; ph -= std::floor (ph);
        const double p1 = ph, p2 = p1 < 0.5 ? p1 + 0.5 : p1 - 0.5;
        const double s = std::sin (PI * p1), w1 = s * s;
        return dl.rd (1 + p1 * win) * w1 + dl.rd (1 + p2 * win) * (1 - w1);
    }
};

// YIN pitch tracker on a decimated copy of the preamp signal, with a short history of estimates.
struct Pitch
{
    double fs = 48000, dfs = 12000, T0 = 320;
    int D = 4, W = 0, minLag = 2, maxLag = 0, F = 0, rw = 0, hop = 16, cnt = 0, dc = 0, hi = 0;
    bool voiced = false;
    Biquad lpA, lpB;
    std::vector<float> ring;
    std::vector<double> fr, d;
    double hT[64], hP[64]; uint8_t hV[64];

    void init (double sr)
    {
        fs = sr; D = std::max (1, (int) jsRound (fs / 12000.0)); dfs = fs / D;
        lpA = Biquad(); lpB = Biquad();
        lpA.set (Biquad::LP, 1100, 0.707, 0, fs); lpB.set (Biquad::LP, 1100, 0.707, 0, fs);
        W = (int) jsRound (0.028 * dfs); minLag = std::max (2, (int) std::floor (dfs / 1000.0)); maxLag = (int) std::ceil (dfs / 65.0);
        F = W + maxLag + 2; ring.assign (4096, 0.0f); rw = 0;
        fr.assign ((size_t) F, 0.0); d.assign ((size_t) (maxLag + 2), 0.0);
        hop = std::max (16, (int) jsRound (0.006 * dfs)); cnt = 0; dc = 0;
        T0 = fs / 150.0; voiced = false;
        for (int i = 0; i < 64; ++i) { hT[i] = -1e12; hP[i] = fs / 150.0; hV[i] = 0; }
        hi = 0;
    }
    inline void push (double x, int64_t nAbs)
    {
        x = lpB.p (lpA.p (x));
        if (++dc < D) return;
        dc = 0;
        ring[(size_t) rw] = (float) x; rw = (rw + 1) & 4095;
        if (++cnt >= hop) { cnt = 0; analyze (nAbs); }
    }
    void analyze (int64_t nAbs)
    {
        const int st = rw - F;
        for (int i = 0; i < F; ++i) fr[(size_t) i] = ring[(size_t) ((st + i) & 4095)];
        double e = 0;
        for (int i = 0; i < W; ++i) e += fr[(size_t) i] * fr[(size_t) i];
        bool v = false; double T = T0;
        if (std::sqrt (e / W) > 0.002)
        {
            double run = 0;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                double s = 0;
                for (int j = 0; j < W; ++j) { const double df = fr[(size_t) j] - fr[(size_t) (j + tau)]; s += df * df; }
                run += s; d[(size_t) tau] = run > 0 ? s * tau / run : 1;
            }
            int sel = -1, best = -1; double bestV = 1e9;
            for (int tau = minLag; tau <= maxLag; ++tau)
            {
                if (d[(size_t) tau] < 0.15) { while (tau + 1 <= maxLag && d[(size_t) (tau + 1)] < d[(size_t) tau]) ++tau; sel = tau; break; }
                if (d[(size_t) tau] < bestV) { bestV = d[(size_t) tau]; best = tau; }
            }
            if (sel < 0 && best > 0 && bestV < 0.35) sel = best;
            if (sel > 0)
            {
                double t = sel;
                if (sel > 1 && sel < maxLag)
                {
                    const double a = d[(size_t) (sel - 1)], b = d[(size_t) sel], c = d[(size_t) (sel + 1)], den = a - 2 * b + c;
                    if (std::abs (den) > 1e-12) t += 0.5 * (a - c) / den;
                }
                T = t * D; v = true;
            }
        }
        if (v) T0 = (voiced && std::abs (T - T0) < 0.12 * T0) ? T0 + (T - T0) * 0.6 : T;
        voiced = v;
        hi = (hi + 1) & 63; hT[hi] = (double) nAbs - (F / 2.0) * D; hP[hi] = T0; hV[hi] = v ? 1 : 0;
    }
    inline int at (double t) const
    {
        int i = hi;
        for (int k = 0; k < 63; ++k) { if (hT[i] <= t) return i; i = (i - 1) & 63; }
        return i;
    }
};

struct Psola
{
    std::vector<float> ola;
    int m = 0; bool init = false; double mark = 0, am = 0, rOff = 0;
    void alloc (int size) { ola.assign ((size_t) size, 0.0f); m = size - 1; init = false; }
};

// Every control, by its browser name. Values are the effective values (after The Ego).
struct Params
{
    double preOn = 1, inGain = 0, gate = -80, hpf = 90, presence = 0.35, warmth = 0.2;
    double distOn = 1, distType = 0, drive = 0.25, crush = 0, tone = 7000, distMix = 0.3;
    double charOn = 1, pitch = 0, formant = 0, robot = 0, metal = 0, octDown = 0, octUp = 0;
    double modOn = 1, modType = 0, modAmt = 0.3, modRate = 0.4, modFb = 0.2, modWidth = 1.3;
    double dlyOn = 1, dlyType = 0, division = 4, bpm = 120, feedback = 0.35, wow = 0.15, dlyMix = 0.22;
    double revOn = 1, revType = 0, decay = 4.5, predelay = 40, damping = 0.55, shimmer = 0.15, revWidth = 1.1, duck = 0.4, revMix = 0.3;
    double outOn = 1, mbSat = 0.2, outWidth = 1.1, mix = 1, outGain = 0;
    double bypass = 0, fxOnly = 0;
};

class Engine
{
public:
    enum { inG, warmW, warmK, dDrv, dMix, crushW, cw, odL, ouL, metW, metFb,
           modMix, modDep, modBL, modBR, modFbS, modW,
           dlyT, dlyTR, dlyFb, dlyWow, dlyMixS,
           pre, revMixS, shG, revW, sh5, rev,
           mb, oW, mixS, oG, duckS, NS };

    struct Meters { double in = 0, out = 0, gr = 1; };

    void prepare (double sampleRate) { fs = sampleRate; build(); setParams (P, true); }
    void reset() { build(); setParams (P, true); }
    int getLatency() const { return latency; }
    Meters takeMeters() { Meters r { mIn, mOut, mGr }; mIn = 0; mOut = 0; mGr = 1; return r; }

    void setParams (const Params& p, bool init = false)
    {
        P = p;
        double* T = Tg;
        // --- preamp
        const bool preOn = P.preOn > 0.5;
        T[inG] = dbg (P.inGain);
        const double hpf = preOn ? P.hpf : 20;
        hpL1.set (Biquad::HP, hpf, 0.5412, 0, fs); hpL2.set (Biquad::HP, hpf, 1.3066, 0, fs);
        hpR1.set (Biquad::HP, hpf, 0.5412, 0, fs); hpR2.set (Biquad::HP, hpf, 1.3066, 0, fs);
        const double pr = preOn ? P.presence : 0;
        for (Biquad* a : { prL, prR })
        {
            a[0].set (Biquad::PEAK, 3200, 0.9, 7 * pr, fs);
            a[1].set (Biquad::HS, 9500, 0.7, 4 * pr, fs);
            a[2].set (Biquad::PEAK, 330, 1.0, -3 * pr, fs);
        }
        const double wm = preOn ? P.warmth : 0;
        T[warmW] = std::min (1.0, wm * 3); T[warmK] = 1 + wm * 5;
        gateOn = preOn && P.gate > -79.5; gThr = dbg (P.gate);
        // --- distortion
        const bool don = P.distOn > 0.5;
        dType = (int) P.distType;
        static const double dDb[4] = { 36, 30, 42, 36 };
        const double driveDb = (dType >= 0 && dType < 4) ? dDb[dType] : 36;
        T[dDrv] = dbg (P.drive * driveDb);
        T[dMix] = don ? P.distMix : 0;
        T[crushW] = don ? std::min (1.0, P.crush * 4) : 0;
        { const double g = dbg (P.drive * driveDb), x = 0.08; agEst = std::min (2.0, 0.85 * x / std::max (1e-3, std::abs (shape (g * x * 1.4)))); }
        crQ = std::pow (2.0, 15 - P.crush * 12); crInc = 1 / (1 + std::pow (P.crush, 1.6) * 15);
        deHpL.set (Biquad::HP, 160, 0.6, 0, fs); deHpR.set (Biquad::HP, 160, 0.6, 0, fs);
        dePkL.set (Biquad::PEAK, 2000, 0.8, 5, fs); dePkR.set (Biquad::PEAK, 2000, 0.8, 5, fs);
        dToneL.set (Biquad::LP, P.tone, 0.6, 0, fs); dToneR.set (Biquad::LP, P.tone, 0.6, 0, fs);
        dHpL.set (Biquad::HP, 100, 0.707, 0, fs); dHpR.set (Biquad::HP, 100, 0.707, 0, fs);
        // --- character
        const bool con = P.charOn > 0.5;
        pRatio = std::pow (2.0, P.pitch / 12);
        fmt = std::min (1.42, std::max (0.7, std::pow (2.0, P.formant / 12)));
        robot = con ? P.robot : 0;
        const bool act = std::abs (P.pitch) > 0.01 || std::abs (P.formant) > 0.01 || robot > 0.001;
        T[cw] = con && act ? 1 : 0;
        T[odL] = con ? P.octDown * 0.9 : 0; T[ouL] = con ? P.octUp * 0.7 : 0;
        T[metW] = con ? P.metal : 0; T[metFb] = 0.45 + 0.45 * P.metal;
        // --- modulation
        const bool mon = P.modOn > 0.5; const double amt = P.modAmt; const int mt = (int) P.modType;
        double mbL, mbR, mdep, mfb, mmix, mrate;
        if (mt == 1) { mbL = 0.0012; mbR = 0.0017; mdep = 0.0035 * (0.3 + 0.7 * amt); mfb = 0.85; mmix = std::min (1.0, amt * 2) * 0.7; mrate = 1; }
        else if (mt == 2) { mbL = 0.024; mbR = 0.035; mdep = 0.0006 + 0.0014 * amt; mfb = 0.3; mmix = std::min (1.0, amt * 1.5) * 0.75; mrate = 0.5; }
        else { mbL = 0.011; mbR = 0.0145; mdep = 0.0035 * amt + 0.0005; mfb = 0.35; mmix = std::min (1.0, amt * 1.8) * 0.55; mrate = 1; }
        T[modMix] = mon ? mmix : 0; T[modDep] = mdep; T[modBL] = mbL; T[modBR] = mbR;
        T[modFbS] = P.modFb * mfb; T[modW] = mon ? P.modWidth : 1;
        mInc = P.modRate * mrate / fs;
        // --- delay
        const bool yon = P.dlyOn > 0.5; const int dt = (int) P.dlyType;
        const int div = (int) std::max (0.0, std::min (8.0, jsRound (P.division)));
        double t = dt == 3 ? SLAP_MS[div] / 1000.0 : DIV_BEATS[div] * 60.0 / std::max (30.0, P.bpm);
        double tR = dt == 0 ? t : dt == 1 ? t + 0.013 : dt == 2 ? t * 1.5 : t + 0.009;
        t = std::min (t, 6.9); tR = std::min (tR, 6.9);
        T[dlyT] = t * fs; T[dlyTR] = tR * fs;
        dCross = dt == 0;
        struct DT { double lp, hp, sat; int cr; double ws; };
        static const DT dts[4] = { { 7500, 180, 0.15, 0, 0.35 }, { 4200, 260, 1, 0, 1 }, { 2300, 320, 0.3, 1, 0.45 }, { 6000, 200, 0.5, 0, 0.15 } };
        const DT& D = dts[(dt >= 0 && dt < 4) ? dt : 0];
        lpfL.set (Biquad::LP, D.lp, 0.6, 0, fs); lpfR.set (Biquad::LP, D.lp, 0.6, 0, fs);
        hpfL.set (Biquad::HP, D.hp, 0.6, 0, fs); hpfR.set (Biquad::HP, D.hp, 0.6, 0, fs);
        dPreHp.set (Biquad::HP, 150, 0.7, 0, fs); dPreLp.set (Biquad::LP, std::min (14000.0, D.lp * 1.4), 0.7, 0, fs);
        dSat = D.sat; dCrush = D.cr;
        T[dlyFb] = dt == 3 ? std::min (P.feedback, 0.35) : std::min (0.95, P.feedback);
        T[dlyWow] = P.wow * D.ws * 0.0035;
        T[dlyMixS] = yon ? P.dlyMix : 0;
        // --- reverb
        const bool ron = P.revOn > 0.5; const int rt = (int) P.revType; const bool plate = rt == 1;
        const double* MS = plate ? PLATE_MS : HALL_MS;
        const double rt60 = std::max (0.2, P.decay);
        for (int k = 0; k < 8; ++k) { fLen[k] = MS[k] * fs / 1000.0; fG[k] = std::pow (10.0, -3 * fLen[k] / (rt60 * fs)); }
        const double fc = 1500 * std::pow (8.0, 1 - P.damping) * (plate ? 1.25 : 1) * (rt == 2 ? 1.2 : 1);
        fDampA = std::exp (-TAU * std::min (fc, fs * 0.45) / fs);
        const double* AP = plate ? AP_PLATE : AP_HALL;
        for (int k = 0; k < 4; ++k) apD[k] = std::max (1.0, jsRound (AP[k] * fs / 1000.0));
        apG = plate ? 0.7 : 0.62;
        rPreHp.set (Biquad::HP, 170, 0.7, 0, fs); rPreLp.set (Biquad::LP, rt == 2 ? 12000 : plate ? 10000 : 8500, 0.7, 0, fs);
        T[pre] = std::max (1.0, rt == 3 ? 0.01 * fs : P.predelay / 1000.0 * fs);
        T[rev] = rt == 3 ? 1 : 0;
        rvS = (0.15 + std::min (300.0, std::max (0.0, P.predelay)) / 300.0 * 0.6) * fs; rvInc = 1 / rvS;
        T[revMixS] = ron ? P.revMix : 0;
        T[revW] = P.revWidth;
        T[shG] = ron ? P.shimmer * 0.55 : 0; T[sh5] = rt == 2 ? 0.6 : 0;
        T[duckS] = P.duck;
        // --- final
        const bool oon = P.outOn > 0.5;
        T[mb] = oon ? P.mbSat : 0; T[oW] = oon ? P.outWidth : 1;
        fxOnly = P.fxOnly > 0.5;
        T[mixS] = P.bypass > 0.5 ? 0 : (fxOnly ? 1 : P.mix);
        T[oG] = dbg (P.outGain);
        if (init)
        {
            for (int i = 0; i < NS; ++i) S[i] = T[i];
            for (int k = 0; k < 8; ++k) fLenS[k] = fLen[k];
            ag = agT = agEst;
        }
    }

    template <typename F>
    void process (const F* inL, const F* inR, F* outL, F* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < NS; ++j) S[j] += (Tg[j] - S[j]) * K[j];
            double xL = (double) inL[i], xR = (double) inR[i];
            if (std::isnan (xL)) xL = 0;
            if (std::isnan (xR)) xR = 0;
            { const double ia = std::max (std::abs (xL), std::abs (xR)); if (ia > mIn) mIn = ia; }
            const double dryLv = dryL.rd (preLat), dryRv = dryR.rd (preLat);
            dryL.wr (xL); dryR.wr (xR);

            // ---------- PREAMP
            double a = xL * S[inG], b = xR * S[inG];
            a = hpL2.p (hpL1.p (a)); b = hpR2.p (hpR1.p (b));
            {
                const double m = std::abs (a + b) * 0.5;
                gEnv += (m - gEnv) * (m > gEnv ? gAttK : gRelK);
                double tg = 1;
                if (gateOn)
                {
                    if (gEnv >= gThr) gHold = gHoldN;
                    else if (gHold > 0) --gHold;
                    else { const double below = 20 * std::log10 (gThr / (gEnv + 1e-9)); tg = dbg (-std::min (40.0, below * 2)); }
                }
                gG += (tg - gG) * (tg > gG ? gOpenK : gCloseK);
                a *= gG; b *= gG;
            }
            a = prL[2].p (prL[1].p (prL[0].p (a))); b = prR[2].p (prR[1].p (prR[0].p (b)));
            {
                const double w = S[warmW];
                if (w > 1e-4) { const double k = S[warmK]; a += (std::tanh (k * a) / k - a) * w; b += (std::tanh (k * b) / k - b) * w; }
            }
            const double pm = 0.5 * (a + b);
            pitch.push (pm, N);
            {
                const double m = std::abs (pm);
                dEnv += (m - dEnv) * (m > dEnv ? dAttK : dRelK);
                if ((N & 7) == 0)
                {
                    const double lv = 20 * std::log10 (dEnv + 1e-9);
                    const double over = std::min (1.0, std::max (0.0, (lv + 44) / 28));
                    duckT = dbg (-S[duckS] * over * 20);
                }
                duckG += (duckT - duckG) * (duckT < duckG ? dgAttK : dgRelK);
            }

            // ---------- DISTORTION (2x oversampled)
            {
                const double dA = dDistL.rd (osLat), dB = dDistR.rd (osLat);
                dDistL.wr (a); dDistR.wr (b);
                const double mix = S[dMix];
                if (mix > 1e-4 || Tg[dMix] > 0)
                {
                    const double g = S[dDrv];
                    const double pa = dePkL.p (deHpL.p (a)) * g, pb = dePkR.p (deHpR.p (b)) * g;
                    osL.down (shape (osL.up (2 * pa)), false); double ya = osL.down (shape (osL.up (0)), true);
                    osR.down (shape (osR.up (2 * pb)), false); double yb = osR.down (shape (osR.up (0)), true);
                    const double cwv = S[crushW];
                    if (cwv > 1e-4)
                    {
                        crPh += crInc;
                        if (crPh >= 1) { crPh -= 1; crHL = jsRound (ya * crQ) / crQ; crHR = jsRound (yb * crQ) / crQ; }
                        ya += (crHL - ya) * cwv; yb += (crHR - yb) * cwv;
                    }
                    ya = dHpL.p (dToneL.p (ya)); yb = dHpR.p (dToneR.p (yb));
                    agIn += ((dA * dA + dB * dB) * 0.5 - agIn) * agK;
                    agOut += ((ya * ya + yb * yb) * 0.5 - agOut) * agK;
                    if (agIn > 1e-6) { const double t = 0.85 * std::sqrt (agIn / (agOut + 1e-12)); agT = t < 0.05 ? 0.05 : t > 2 ? 2 : t; }
                    else { agT = agEst; ag += (agEst - ag) * 0.01; }
                    ag += (agT - ag) * agSK;
                    a = dA + (ya * ag - dA) * mix; b = dB + (yb * ag - dB) * mix;
                }
                else { a = dA; b = dB; }
            }

            // ---------- CHARACTER
            {
                inBuf[(size_t) (N & inM)] = (float) (0.5 * (a + b));
                const double dA = dChL.rd (L), dB = dChR.rd (L);
                dChL.wr (a); dChR.wr (b);
                double ca = dA, cb = dB;
                const double cwv = S[cw];
                if (cwv > 1e-4 || Tg[cw] > 0) { const double y = psola (ps0, pRatio, fmt); ca = dA + (y - dA) * cwv; cb = dB + (y - dB) * cwv; }
                else ps0.init = false;
                const double od = S[odL];
                if (od > 1e-4 || Tg[odL] > 0) { const double y = psola (ps1, pRatio * 0.5, fmt) * od; ca += y; cb += y; }
                else ps1.init = false;
                const double ou = S[ouL];
                if (ou > 1e-4 || Tg[ouL] > 0) { const double y = psola (ps2, pRatio * 2, fmt) * ou; ca += y; cb += y; }
                else ps2.init = false;
                a = ca; b = cb;
                const double mw = S[metW];
                if (mw > 1e-4 || Tg[metW] > 0)
                {
                    const double fb = S[metFb], m = 0.5 * (a + b);
                    const double vA = cmbA.rd (cmbAd), vB = cmbB.rd (cmbBd);
                    cLpA += (vA - cLpA) * 0.55; cLpB += (vB - cLpB) * 0.55;
                    cmbA.wr (m + cLpA * fb + 1e-18); cmbB.wr (m + cLpB * fb + 1e-18);
                    const double y = (vA + vB) * 0.75 * (1 - fb) * 2;
                    a = a * (1 - 0.35 * mw) + y * mw; b = b * (1 - 0.35 * mw) + y * mw;
                }
                ++N;
            }

            // ---------- Y2K MODULATION
            {
                const double mix = S[modMix];
                if (mix > 1e-4 || Tg[modMix] > 0)
                {
                    mPh += mInc; if (mPh >= 1) mPh -= 1;
                    if (--drC <= 0) { drC = drN; seed = seed * 1664525u + 1013904223u; drT = seed / 4294967296.0 * 2 - 1; }
                    dr += (drT - dr) * drK;
                    const double lL = std::sin (TAU * mPh), lR = std::sin (TAU * (mPh + 0.25)), dep = S[modDep];
                    double dL = (S[modBL] + dep * (0.5 + 0.5 * (0.78 * lL + 0.22 * dr))) * fs;
                    double dR = (S[modBR] + dep * (0.5 + 0.5 * (0.78 * lR - 0.22 * dr))) * fs;
                    if (dL < 1) dL = 1;
                    if (dR < 1) dR = 1;
                    const double vL = mdL.rd (dL), vR = mdR.rd (dR), fb = S[modFbS];
                    double wL = a + vL * fb, wR = b + vR * fb;
                    if (wL > 1.5 || wL < -1.5) wL = std::tanh (wL / 1.5) * 1.5;
                    if (wR > 1.5 || wR < -1.5) wR = std::tanh (wR / 1.5) * 1.5;
                    mdL.wr (wL + 1e-18); mdR.wr (wR + 1e-18);
                    const double g = 1 / (1 + 0.5 * mix);
                    a = (a + vL * mix) * g; b = (b + vR * mix) * g;
                }
                else { mdL.wr (a); mdR.wr (b); }
                const double w = S[modW];
                if (w < 0.999 || w > 1.001) { const double Mm = (a + b) * 0.5, Sd = (a - b) * 0.5 * w; a = Mm + Sd; b = Mm - Sd; }
            }

            // ---------- DELAY
            double eL = 0, eR = 0;
            {
                const double mix = S[dlyMixS];
                if (mix > 1e-4 || Tg[dlyMixS] > 0)
                {
                    const double x = dPreLp.p (dPreHp.p (0.5 * (a + b)));
                    wPh += wInc; if (wPh >= 1) wPh -= 1;
                    fPh += fInc; if (fPh >= 1) fPh -= 1;
                    const double wm = S[dlyWow] * fs * (std::sin (TAU * wPh) + 0.25 * std::sin (TAU * fPh) + 0.3 * dr);
                    double tL = S[dlyT] + wm, tR = S[dlyTR] + wm * 0.85;
                    if (tL < 2) tL = 2;
                    if (tR < 2) tR = 2;
                    const double yL = dlL.rd (tL), yR = dlR.rd (tR), fb = S[dlyFb];
                    double wL, wR;
                    if (dCross) { const double s = std::sqrt (fb); wL = x + dproc (lpfL, hpfL, yR) * s; wR = yL * s; }
                    else { wL = x + dproc (lpfL, hpfL, yL) * fb; wR = x + dproc (lpfR, hpfR, yR) * fb; }
                    dlL.wr (clipFb (wL) + 1e-18); dlR.wr (clipFb (wR) + 1e-18);
                    const double dg = mix * duckG; eL = yL * dg; eR = yR * dg;
                }
                else { dlL.wr (0); dlR.wr (0); }
            }

            // ---------- REVERB
            double rL = 0, rR = 0;
            {
                const double lvl = S[revMixS];
                if (lvl > 1e-4 || Tg[revMixS] > 0)
                {
                    double x = 0.5 * (a + b) + (eL + eR) * 0.22 + shFb;
                    x = rPreLp.p (rPreHp.p (x));
                    const double pd = S[pre]; const double pv = pdl.rd (pd < 1 ? 1 : pd); pdl.wr (x); x = pv;
                    const double rv = S[rev];
                    rvl.wr (x);
                    if (rv > 1e-4)
                    {
                        rvPh += rvInc; if (rvPh >= 1) rvPh -= 1;
                        const double p1 = rvPh, p2 = p1 < 0.5 ? p1 + 0.5 : p1 - 0.5;
                        const double s1 = std::sin (PI * p1), w1 = s1 * s1;
                        const double y = rvl.rd (1 + 2 * rvS * p1) * w1 + rvl.rd (1 + 2 * rvS * p2) * (1 - w1);
                        x += (y * 1.25 - x * 0.85) * rv;
                    }
                    for (int k = 0; k < 4; ++k) { const double v = ap[k].rd (apD[k]), w = x + apG * v; x = v - apG * w; ap[k].wr (w); }
                    double s[8];
                    for (int k = 0; k < 8; ++k)
                    {
                        fLenS[k] += (fLen[k] - fLenS[k]) * kLen;
                        double d = fLenS[k];
                        if (k < 4) { fmP[k] += fmI[k]; if (fmP[k] >= 1) fmP[k] -= 1; d += fModD * std::sin (TAU * fmP[k]); }
                        s[k] = fdn[k].rd (d);
                    }
                    double v[8];
                    for (int k = 0; k < 8; ++k) { fLp[k] = s[k] + (fLp[k] - s[k]) * fDampA; v[k] = fLp[k] * fG[k]; }
                    const double a0 = v[0] + v[1], a1 = v[0] - v[1], a2 = v[2] + v[3], a3 = v[2] - v[3], a4 = v[4] + v[5], a5 = v[4] - v[5], a6 = v[6] + v[7], a7 = v[6] - v[7];
                    const double b0 = a0 + a2, b1 = a1 + a3, b2 = a0 - a2, b3 = a1 - a3, b4 = a4 + a6, b5 = a5 + a7, b6 = a4 - a6, b7 = a5 - a7;
                    const double hn = 0.35355339, xi = x * 0.5;
                    fdn[0].wr (xi * IN_SIGN[0] + (b0 + b4) * hn + 1e-18); fdn[1].wr (xi * IN_SIGN[1] + (b1 + b5) * hn + 1e-18);
                    fdn[2].wr (xi * IN_SIGN[2] + (b2 + b6) * hn + 1e-18); fdn[3].wr (xi * IN_SIGN[3] + (b3 + b7) * hn + 1e-18);
                    fdn[4].wr (xi * IN_SIGN[4] + (b0 - b4) * hn + 1e-18); fdn[5].wr (xi * IN_SIGN[5] + (b1 - b5) * hn + 1e-18);
                    fdn[6].wr (xi * IN_SIGN[6] + (b2 - b6) * hn + 1e-18); fdn[7].wr (xi * IN_SIGN[7] + (b3 - b7) * hn + 1e-18);
                    const double wL = (s[0] - s[3] + s[4] - s[7]) * 0.5, wR = (s[1] - s[2] + s[5] - s[6]) * 0.5;
                    const double sg = S[shG];
                    if (sg > 1e-4)
                    {
                        const double mono = (wL + wR) * 0.5;
                        double sh = gs2.p (mono) + gs15.p (mono) * S[sh5];
                        sh = shLp.p (shHp.p (sh)) * sg;
                        shFb = (sh > 0.6 || sh < -0.6) ? std::tanh (sh / 0.6) * 0.6 : sh;
                    }
                    else shFb = 0;
                    const double Mw = (wL + wR) * 0.5, Sw = (wL - wR) * 0.5 * S[revW], rg = lvl * duckG;
                    rL = (Mw + Sw) * rg; rR = (Mw - Sw) * rg;
                }
            }

            // ---------- FINAL
            double oLv, oRv;
            {
                double xa = fxOnly ? eL + rL : a + eL + rL, xb = fxOnly ? eR + rR : b + eR + rR;
                const double mbv = S[mb];
                if (mbv > 1e-4) { xa = multiband (xa, mbSt[0], mbv); xb = multiband (xb, mbSt[1], mbv); }
                xa = softClip (xa); xb = softClip (xb);
                const double w = S[oW];
                if (w < 0.999 || w > 1.001) { const double Mm = (xa + xb) * 0.5, Sd = (xa - xb) * 0.5 * w; xa = Mm + Sd; xb = Mm - Sd; }
                const double mx = S[mixS], og = S[oG];
                oLv = (dryLv + (xa - dryLv) * mx) * og; oRv = (dryRv + (xb - dryRv) * mx) * og;
            }

            // ---------- LIMITER (1.5 ms lookahead, -1 dBFS ceiling)
            {
                const double pk = std::max (std::abs (oLv), std::abs (oRv)), c = lCeil;
                lRing[(size_t) lIdx] = pk > c ? c / pk : 1; lIdx = lIdx + 1 == lRN ? 0 : lIdx + 1;
                double mn = 1;
                for (int k = 0; k < lRN; ++k) if (lRing[(size_t) k] < mn) mn = lRing[(size_t) k];
                lG += (mn - lG) * (mn < lG ? lAttK : lRelK);
                const double yL = lDL.rd (lD), yR = lDR.rd (lD);
                lDL.wr (oLv); lDR.wr (oRv);
                double zL = yL * lG, zR = yR * lG;
                zL = zL > c ? c : zL < -c ? -c : zL; zR = zR > c ? c : zR < -c ? -c : zR;
                outL[i] = (F) zL; outR[i] = (F) zR;
                const double op = std::max (std::abs (zL), std::abs (zR)); if (op > mOut) mOut = op;
                if (lG < mGr) mGr = lG;
            }
        }
        if (n > 0 && ! (std::isfinite ((double) outL[n - 1]) && std::isfinite ((double) outR[n - 1])))
        {
            reset();
            std::fill (outL, outL + n, (F) 0); std::fill (outR, outR + n, (F) 0);
        }
    }

private:
    struct MB { Biquad l1, l2, m1, m2; };

    double fs = 48000;
    Params P;
    double S[NS] {}, Tg[NS] {}, K[NS] {};
    double kLen = 0.001;
    int64_t N = 0;
    int osLat = 15, L = 2200, latency = 0, inM = 0;
    double preLat = 0, Tmax = 0, hOutMax = 0, Tunv = 0;
    double hfir[31] {};
    OS2 osL, osR;
    std::vector<float> inBuf;
    Psola ps0, ps1, ps2;
    Pitch pitch;
    Delay dryL, dryR;
    // preamp
    Biquad hpL1, hpL2, hpR1, hpR2, prL[3], prR[3];
    double gEnv = 0, gG = 1, gThr = 0, gAttK = 0, gRelK = 0, gOpenK = 0, gCloseK = 0; int gHold = 0, gHoldN = 0; bool gateOn = false;
    double dEnv = 0, duckG = 1, duckT = 1, dAttK = 0, dRelK = 0, dgAttK = 0, dgRelK = 0;
    // distortion
    Delay dDistL, dDistR;
    Biquad deHpL, deHpR, dePkL, dePkR, dToneL, dToneR, dHpL, dHpR;
    int dType = 0; double crPh = 0, crInc = 1, crQ = 32768, crHL = 0, crHR = 0;
    double agIn = 0, agOut = 0, ag = 1, agT = 1, agEst = 1, agK = 0, agSK = 0;
    // character
    Delay dChL, dChR, cmbA, cmbB;
    double pRatio = 1, fmt = 1, robot = 0, cmbAd = 0, cmbBd = 0, cLpA = 0, cLpB = 0;
    // modulation
    Delay mdL, mdR;
    double mPh = 0, mInc = 0, dr = 0, drT = 0, drK = 0; int drC = 0, drN = 0; uint32_t seed = 0x2468ace;
    // delay
    Delay dlL, dlR;
    Biquad dPreHp, dPreLp, lpfL, hpfL, lpfR, hpfR;
    bool dCross = true; double dSat = 0, dCrushQ = 512; int dCrush = 0;
    double wPh = 0, fPh = 0, wInc = 0, fInc = 0;
    // reverb
    Biquad rPreHp, rPreLp, shHp, shLp;
    Delay pdl, rvl, ap[4], fdn[8];
    double rvPh = 0, rvS = 1, rvInc = 1, apD[4] {}, apG = 0.62;
    double fLen[8] {}, fLenS[8] {}, fG[8] {}, fLp[8] {}, fDampA = 0.5, fmP[4] {}, fmI[4] {}, fModD = 0;
    Shift gs2, gs15; double shFb = 0;
    // final
    MB mbSt[2]; bool fxOnly = false;
    // limiter
    int lD = 72, lRN = 73, lIdx = 0; std::vector<double> lRing; Delay lDL, lDR;
    double lG = 1, lCeil = 0.891, lAttK = 0, lRelK = 0;
    double mIn = 0, mOut = 0, mGr = 1;

    void build()
    {
        const int NF = 31; double hs = 0;
        for (int i = 0; i < NF; ++i)
        {
            const double k = i - (NF - 1) / 2.0, x = PI * k / 2;
            const double sinc = i == (NF - 1) / 2 ? 1 : std::sin (x) / x;
            const double w = 0.42 - 0.5 * std::cos (TAU * i / (NF - 1)) + 0.08 * std::cos (2 * TAU * i / (NF - 1));
            hfir[i] = sinc * w; hs += hfir[i];
        }
        for (int i = 0; i < NF; ++i) hfir[i] /= hs;
        osL.init (hfir, NF); osR.init (hfir, NF);
        {
            OS2 t; t.init (hfir, NF); double best = 0; int bi = 0;
            for (int i = 0; i < 64; ++i)
            {
                const double x = i == 0 ? 1 : 0;
                t.down (t.up (2 * x), false);
                const double y = std::abs (t.down (t.up (0), true));
                if (y > best) { best = y; bi = i; }
            }
            osLat = std::max (1, bi);
        }
        Tmax = fs / 65 + 8; hOutMax = Tmax / 0.7;
        L = (int) std::ceil (hOutMax + 1.5 * Tmax) + 8;
        Tunv = jsRound (0.0055 * fs);
        int rs = 1; while (rs < L * 4) rs <<= 1;
        inBuf.assign ((size_t) rs, 0.0f); inM = rs - 1;
        int os = 1; while (os < (hOutMax * 2 + Tmax * 2) * 2) os <<= 1;
        ps0.alloc (os); ps1.alloc (os); ps2.alloc (os);
        pitch.init (fs);
        N = 0;
        preLat = osLat + L;

        const double kf = 1 - std::exp (-1 / (0.015 * fs)), ks = 1 - std::exp (-1 / (0.12 * fs)), km = 1 - std::exp (-1 / (0.06 * fs));
        for (int i = 0; i < NS; ++i) { K[i] = kf; S[i] = 0; Tg[i] = 0; }
        K[dlyT] = ks; K[dlyTR] = ks; K[pre] = ks; K[modBL] = km; K[modBR] = km;
        kLen = km;

        dryL.init (preLat + 8); dryR.init (preLat + 8);
        hpL1 = hpL2 = hpR1 = hpR2 = Biquad();
        for (int k = 0; k < 3; ++k) { prL[k] = Biquad(); prR[k] = Biquad(); }
        gEnv = 0; gG = 1; gHold = 0; gHoldN = (int) jsRound (0.04 * fs); gateOn = false; gThr = 0;
        gAttK = 1 - std::exp (-1 / (0.001 * fs)); gRelK = 1 - std::exp (-1 / (0.06 * fs));
        gOpenK = 1 - std::exp (-1 / (0.0015 * fs)); gCloseK = 1 - std::exp (-1 / (0.09 * fs));
        dEnv = 0; duckG = 1; duckT = 1;
        dAttK = 1 - std::exp (-1 / (0.01 * fs)); dRelK = 1 - std::exp (-1 / (0.3 * fs));
        dgAttK = 1 - std::exp (-1 / (0.025 * fs)); dgRelK = 1 - std::exp (-1 / (0.28 * fs));

        dDistL.init (osLat + 8); dDistR.init (osLat + 8);
        deHpL = deHpR = dePkL = dePkR = dToneL = dToneR = dHpL = dHpR = Biquad();
        dType = 0; crPh = 0; crInc = 1; crQ = 32768; crHL = crHR = 0;
        agIn = agOut = 0; ag = agT = 1;
        agK = 1 - std::exp (-1 / (0.25 * fs)); agSK = 1 - std::exp (-1 / (0.08 * fs));

        dChL.init (L + 8); dChR.init (L + 8);
        pRatio = 1; fmt = 1; robot = 0;
        cmbA.init (std::ceil (0.01 * fs)); cmbB.init (std::ceil (0.01 * fs));
        cmbAd = 0.0029 * fs; cmbBd = 0.0043 * fs; cLpA = cLpB = 0;

        mdL.init (std::ceil (0.07 * fs)); mdR.init (std::ceil (0.07 * fs));
        mPh = 0; mInc = 0; dr = drT = 0; drC = 0; drN = (int) jsRound (0.35 * fs);
        drK = 1 - std::exp (-1 / (0.4 * fs)); seed = 0x2468ace;

        dlL.init (std::ceil (7.2 * fs)); dlR.init (std::ceil (7.2 * fs));
        dPreHp = dPreLp = lpfL = hpfL = lpfR = hpfR = Biquad();
        dCross = true; dSat = 0; dCrush = 0; dCrushQ = 512;
        wPh = fPh = 0; wInc = 0.55 / fs; fInc = 6.3 / fs;

        rPreHp = rPreLp = Biquad();
        pdl.init (std::ceil (0.36 * fs));
        rvl.init (std::ceil (1.6 * fs)); rvPh = 0; rvS = 0.3 * fs; rvInc = 1 / rvS;
        for (int k = 0; k < 4; ++k) { ap[k].init (std::ceil (0.016 * fs)); apD[k] = 1; }
        apG = 0.62;
        for (int k = 0; k < 8; ++k) { fdn[k].init (std::ceil (HALL_MS[7] * fs / 1000.0) + 64); fLen[k] = fLenS[k] = fG[k] = fLp[k] = 0; }
        fDampA = 0.5;
        const double mp[4] = { 0, 0.25, 0.5, 0.75 }, mi[4] = { 0.31, 0.47, 0.73, 0.91 };
        for (int k = 0; k < 4; ++k) { fmP[k] = mp[k]; fmI[k] = mi[k] / fs; }
        fModD = 0.00022 * fs;
        gs2.init (fs, 2, 0.07); gs15.init (fs, 1.5, 0.07);
        shHp = shLp = Biquad(); shHp.set (Biquad::HP, 450, 0.7, 0, fs); shLp.set (Biquad::LP, 7000, 0.7, 0, fs); shFb = 0;

        for (auto& s : mbSt)
        {
            s.l1 = s.l2 = s.m1 = s.m2 = Biquad();
            s.l1.set (Biquad::LP, 250, 0.7071, 0, fs); s.l2.set (Biquad::LP, 250, 0.7071, 0, fs);
            s.m1.set (Biquad::LP, 3500, 0.7071, 0, fs); s.m2.set (Biquad::LP, 3500, 0.7071, 0, fs);
        }
        fxOnly = false;

        lD = std::max (1, (int) jsRound (0.0015 * fs)); lRN = lD + 1; lRing.assign ((size_t) lRN, 1.0); lIdx = 0;
        lDL.init (lD + 4); lDR.init (lD + 4);
        lG = 1; lCeil = dbg (-1); lAttK = 1 - std::exp (-1 / (lD * 0.22)); lRelK = 1 - std::exp (-1 / (0.09 * fs));
        latency = (int) preLat + lD;
        mIn = mOut = 0; mGr = 1;
    }

    inline double shape (double v) const
    {
        switch (dType)
        {
            case 1: return v > 1 ? 1 : v < -1 ? -1 : v;
            case 2: { const double a = v * 1.6, sc = a / (1 + std::abs (a)); return 0.85 * sc + 0.4 * (std::abs (sc) - 0.35); }
            case 3: return v > 0 ? std::tanh (1.2 * v) : 1.6 * std::tanh (0.45 * v);
            default: return std::tanh (v + 0.08) - 0.07983;
        }
    }

    double psola (Psola& s, double ratioMul, double f)
    {
        const double lim = (double) N + hOutMax;
        if (! s.init) { s.init = true; s.mark = (double) N; s.am = (double) (N - L); std::fill (s.ola.begin(), s.ola.end(), 0.0f); s.rOff = 0; }
        while (s.mark <= lim)
        {
            const double d = s.mark - L;
            const int hi = pitch.at (d - osLat);
            const bool voiced = pitch.hV[hi] == 1;
            const double T = voiced ? pitch.hP[hi] : Tunv;
            double ratio = ratioMul;
            if (voiced && robot > 0.001)
            {
                const double midi = 69 + 12 * std::log2 ((fs / T) * ratioMul / 440);
                const double off = (jsRound (midi) - midi) * std::min (1.0, robot * 1.15);
                s.rOff += (off - s.rOff) * (0.12 + 0.88 * robot * robot);
                ratio *= std::pow (2.0, s.rOff / 12);
            }
            if (std::abs (s.am - d) > 2 * T) s.am = d;
            else { while (s.am + 0.5 * T < d) s.am += T; while (s.am - 0.5 * T > d) s.am -= T; }
            const double hOut = T / f, Tout = voiced ? T / ratio : T;
            const double sc = std::min (1.6, Tout / hOut), invH = PI / hOut;
            int64_t k0 = (int64_t) std::ceil (s.mark - hOut);
            const int64_t k1 = (int64_t) std::floor (s.mark + hOut);
            if (k0 < N) k0 = N;
            for (int64_t k = k0; k <= k1; ++k)
            {
                const double t = (double) k - s.mark, w = 0.5 + 0.5 * std::cos (t * invH);
                const double pos = s.am + t * f, fp = std::floor (pos);
                const int64_t ip = (int64_t) fp;
                const double fr = pos - fp, x0 = inBuf[(size_t) (ip & inM)];
                float& o = s.ola[(size_t) (k & s.m)];
                o = (float) ((double) o + (x0 + ((double) inBuf[(size_t) ((ip + 1) & inM)] - x0) * fr) * w * sc);
            }
            s.mark += Tout;
        }
        const size_t idx = (size_t) (N & s.m);
        const double y = s.ola[idx]; s.ola[idx] = 0;
        return y;
    }

    inline double dproc (Biquad& lp, Biquad& hp, double y)
    {
        double v = lp.p (hp.p (y));
        if (dSat > 0) v += (std::tanh (v * 1.5) / 1.5 - v) * dSat;
        if (dCrush) v = jsRound (v * dCrushQ) / dCrushQ;
        return v;
    }

    inline double multiband (double x, MB& st, double amt)
    {
        const double low = st.l2.p (st.l1.p (x)), rest = x - low, mid = st.m2.p (st.m1.p (rest)), high = rest - mid;
        const double kl = 1 + amt * 1.5, km = 1 + amt * 4, kh = 1 + amt * 2.5;
        return std::tanh (low * kl) / kl + std::tanh (mid * km) / km + std::tanh (high * kh) / kh;
    }
};
} // namespace fel
