// The Ego macro, preset state and value formatting. Mirrors felApplyEgo() and the browser formatters.
#pragma once
#include "FelTables.h"
#include <cstdio>
#include <string>

namespace fel
{
// User-facing state = Params (what the knobs show) + the Ego position.
// 0 = humble (natural, a little warmth), 0.5 = knobs exactly as set, 1 = every effect amount pushed hard.
inline Params applyEgo (const Params& user, double ego)
{
    Params e = user;
    const bool lo = ego <= 0.5;
    const double t = lo ? ego / 0.5 : (ego - 0.5) / 0.5, tp = std::pow (t, 1.4);
    for (const auto& k : KNOBS)
    {
        if (k.egoType == 0) continue;
        double* dst = paramPtr (e, k.id);
        const double v = *paramPtr (const_cast<Params&> (user), k.id), tgt = k.egoTarget;
        if (k.egoType == 1)
        {
            const double fl = std::min (v, (double) k.egoFloor);
            *dst = lo ? fl + (v - fl) * t : v + std::max (0.0, tgt - v) * tp;
        }
        else if (k.egoType == 2)
            *dst = lo ? 1 + (v - 1) * t : v + std::max (0.0, tgt - v) * tp;
        else
        {
            const double d0 = std::min (v, 1.0);
            *dst = lo ? std::exp (std::log (d0) + (std::log (v) - std::log (d0)) * t) : v + std::max (0.0, tgt - v) * std::pow (t, 1.6);
        }
    }
    if (! lo) e.outGain = user.outGain - 2 * t;
    return e;
}

inline Params presetParams (int index)
{
    Params p;
    const auto& pr = PRESETS[index];
    for (int i = 0; i < pr.numValues; ++i)
        if (double* d = paramPtr (p, pr.values[i].id)) *d = pr.values[i].value;
    return p;
}

static const char* const DIV_LABELS[9] = { "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2", "1/2." };
static const char* const SLAP_LABELS[9] = { "45 ms", "60 ms", "75 ms", "90 ms", "105 ms", "120 ms", "140 ms", "160 ms", "190 ms" };

// Formats a knob value the way the faceplate shows it. revType/dlyType change two labels.
inline std::string formatValue (const KnobDef& k, double v, int revType = 0, int dlyType = 0)
{
    char b[48];
    const std::string f = k.fmt;
    if (std::string (k.id) == "predelay" && revType == 3) std::snprintf (b, sizeof b, "%d ms", (int) jsRound (150 + v / 300 * 600));
    else if (f == "db") std::snprintf (b, sizeof b, "%s%.1f dB", v > 0 ? "+" : "", v);
    else if (f == "gate") { if (v <= -79.5) return "OFF"; std::snprintf (b, sizeof b, "%d dB", (int) jsRound (v)); }
    else if (f == "hz") { if (v >= 1000) std::snprintf (b, sizeof b, v >= 9950 ? "%.0f kHz" : "%.1f kHz", v / 1000); else std::snprintf (b, sizeof b, "%d Hz", (int) jsRound (v)); }
    else if (f == "pct" || f == "width") std::snprintf (b, sizeof b, "%d%%", (int) jsRound (v * 100));
    else if (f == "st") { const int s = (int) jsRound (v); std::snprintf (b, sizeof b, "%s%d st", s > 0 ? "+" : "", s); }
    else if (f == "fmt") std::snprintf (b, sizeof b, "%s%.1f st", v > 0.04 ? "+" : "", v);
    else if (f == "rate") std::snprintf (b, sizeof b, v < 1 ? "%.2f Hz" : "%.1f Hz", v);
    else if (f == "sec") std::snprintf (b, sizeof b, v < 10 ? "%.1f s" : "%.0f s", v);
    else if (f == "ms") std::snprintf (b, sizeof b, "%d ms", (int) jsRound (v));
    else if (f == "div") { const int i = (int) std::max (0.0, std::min (8.0, jsRound (v))); return dlyType == 3 ? SLAP_LABELS[i] : DIV_LABELS[i]; }
    else std::snprintf (b, sizeof b, "%.2f", v);
    return b;
}

inline const char* whatLabel (const KnobDef& k, int revType, int dlyType)
{
    if (std::string (k.id) == "predelay" && revType == 3) return "Grain length";
    if (std::string (k.id) == "division" && dlyType == 3) return "Slap time";
    return k.what;
}

inline const char* egoStage (double v)
{
    if (v < 0.12) return "HUMBLE";
    if (v < 0.37) return "CONFIDENT";
    if (v < 0.62) return "FRONTMAN";
    if (v < 0.87) return "INSUFFERABLE";
    return "FINAL FORM";
}
} // namespace fel
