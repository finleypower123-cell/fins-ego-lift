// Renders every preset at three Ego settings on a synthetic sung vowel and checks the engine stays sane.
// Run after building:  ./FelEngineTest   (exit code 0 = pass)
#include "FelModel.h"
#include <cstdio>
#include <vector>

static std::vector<float> testVoice (double sr, double seconds)
{
    std::vector<float> v ((size_t) (sr * seconds));
    double ph = 0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / sr, gate = std::fmod (t, 2.0) < 1.4 ? 1.0 : 0.0;
        const double f0 = 196.0 * (1 + 0.006 * std::sin (fel::TAU * 5.4 * t));
        ph += f0 / sr; ph -= std::floor (ph);
        double s = 0;
        for (int h = 1; h <= 20; ++h) s += std::sin (fel::TAU * ph * h) / (h * h) * (h == 4 || h == 6 ? 2.5 : 1.0);
        v[i] = (float) (0.25 * s * gate);
    }
    return v;
}

int main()
{
    int failures = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const auto in = testVoice (sr, 6.0);
        for (int p = 0; p < fel::NUM_PRESETS; ++p)
            for (double ego : { 0.0, 0.5, 1.0 })
            {
                fel::Engine e; e.prepare (sr); e.setParams (fel::applyEgo (fel::presetParams (p), ego), true);
                const size_t n = in.size() + (size_t) (sr * 4);
                std::vector<float> L (n), R (n), buf (n, 0.0f);
                std::copy (in.begin(), in.end(), buf.begin());
                for (size_t s = 0; s < n; s += 256) { const int c = (int) std::min<size_t> (256, n - s); e.process (buf.data() + s, buf.data() + s, L.data() + s, R.data() + s, c); }
                double peak = 0, energy = 0; bool finite = true;
                for (size_t i = 0; i < n; ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false; peak = std::max (peak, (double) std::abs (L[i])); energy += (double) L[i] * L[i]; }
                const double rms = std::sqrt (energy / (double) n);
                const bool ok = finite && peak <= 0.892 && rms > 1e-3;
                if (! ok) { ++failures; std::printf ("FAIL %6.0f Hz  %-22s ego %.1f  peak %.3f  rms %.4f  finite %d\n", sr, fel::PRESETS[p].name, ego, peak, rms, finite); }
            }
        fel::Engine e; e.prepare (sr);
        std::printf ("%6.0f Hz: latency %d samples (%.1f ms)\n", sr, e.getLatency(), 1000.0 * e.getLatency() / sr);
    }
    std::printf (failures == 0 ? "All presets pass.\n" : "%d failures.\n", failures);
    return failures == 0 ? 0 : 1;
}
