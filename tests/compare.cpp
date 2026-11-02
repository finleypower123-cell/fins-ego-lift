#include "../Source/FelModel.h"
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstdlib>
static std::vector<float> load (const char* p) { std::vector<float> v; FILE* f = std::fopen (p, "rb"); if (! f) return v; std::fseek (f, 0, SEEK_END); long n = std::ftell (f) / 4; std::fseek (f, 0, SEEK_SET); v.resize ((size_t) n); std::fread (v.data(), 4, (size_t) n, f); std::fclose (f); return v; }
int main (int argc, char** argv)
{
    const char* dir = argv[1]; char p[512];
    std::snprintf (p, sizeof p, "%s/demo.f32", dir); auto demo = load (p);
    const double SR = 48000; int fails = 0; double cpu = 0, secs = 0;
    for (int i = 0; i < fel::NUM_PRESETS; ++i)
        for (double ego : { 0.5, 1.0 })
        {
            std::snprintf (p, sizeof p, "%s/js_%d_%g.f32", dir, i, ego); auto jl = load (p);
            std::snprintf (p, sizeof p, "%s/jsR_%d_%g.f32", dir, i, ego); auto jr = load (p);
            fel::Engine e; e.prepare (SR); { auto up = fel::presetParams (i); if (std::getenv ("NOCRUSH")) up.crush = 0; auto ee = fel::applyEgo (up, ego); if (std::getenv ("NOCRUSH")) ee.crush = 0; e.setParams (ee, true); }
            const size_t n = jl.size(); std::vector<float> L (n), R (n), in (n, 0.0f);
            for (size_t k = 0; k < demo.size(); ++k) in[k] = demo[k];
            auto t0 = std::chrono::steady_clock::now();
            for (size_t s = 0; s < n; s += 128) { int c = (int) std::min<size_t> (128, n - s); e.process (in.data() + s, in.data() + s, L.data() + s, R.data() + s, c); }
            cpu += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count(); secs += n / SR;
            double err = 0, ref = 0, mx = 0; size_t firstBad = n;
            for (size_t k = 0; k < n; ++k) { double d1 = L[k] - jl[k], d2 = R[k] - jr[k]; err += d1 * d1 + d2 * d2; ref += (double) jl[k] * jl[k] + (double) jr[k] * jr[k]; double a = std::max (std::abs (d1), std::abs (d2)); if (a > mx) mx = a; if (a > 1e-3 && firstBad == n) firstBad = k; }
            const double rel = 10 * std::log10 (err / (ref + 1e-30) + 1e-30);
            if (rel > -60) ++fails;
            std::printf ("%-24s ego %.1f  error %7.1f dB  max %.2e  first>1e-3 @ %s\n", fel::PRESETS[i].name, ego, rel, mx, firstBad == n ? "never" : std::to_string (firstBad / SR).c_str());
        }
    std::printf ("C++ latency %d samples, CPU %.1f%% of realtime, presets off by > -60 dB: %d\n", fel::Engine().getLatency(), 100 * cpu / secs, fails);
    return 0;
}
