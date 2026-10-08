// BEAT FROG · REZONANZA
// Small DSP toolkit that mirrors the Web Audio graph the design prototype used,
// so every drum sounds like it did in the browser test version.
#pragma once
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace bf
{
constexpr float kPi = 3.14159265358979f;

enum Wave { SINE, TRI, SQUARE, SAW, NOISE };
enum FType { LP, HP, BP };

// Web Audio style automation event: 0 = setValue, 1 = exponential ramp, 2 = linear ramp
struct Seg { float t; float v; int type; };

// Automation lane evaluated with Web Audio semantics
struct Lane
{
    std::vector<Seg> segs;
    float valueAt (float t) const
    {
        if (segs.empty()) return 1.0f;
        if (t <= segs.front().t) return segs.front().v;
        for (size_t i = 1; i < segs.size(); ++i)
        {
            const Seg& b = segs[i];
            if (t < b.t)
            {
                const Seg& a = segs[i - 1];
                if (b.type == 0) return a.v;
                const float f = (t - a.t) / std::max (1e-6f, b.t - a.t);
                if (b.type == 2) return a.v + (b.v - a.v) * f;
                const float v0 = std::max (1e-7f, a.v), v1 = std::max (1e-7f, b.v);
                return v0 * std::pow (v1 / v0, f);
            }
        }
        return segs.back().v;
    }
    float endTime() const { return segs.empty() ? 0.0f : segs.back().t; }
};

struct FiltDef { FType type = LP; float f = 1000.0f, q = 0.7f, f1 = 0.0f, sweepT = 0.0f; };

// One sound source: one or more oscillators (or noise), filters, optional extras, gain envelope
struct Layer
{
    Wave wave = SINE;
    float freqs[6] = { 440, 0, 0, 0, 0, 0 };
    int nfreq = 1;
    float mix = 1.0f;          // gain applied to the summed oscillators
    float startT = 0.0f;       // source start (seconds after hit)
    float stopT = 1.0f;        // source stop
    // frequency automation of freqs[0]
    float f1 = 0.0f, sweepT = 0.0f; bool linSweep = false;
    // extras
    float vibRate = 0, vibDepth = 0;                    // ufo
    float fmRatio = 0, fmStart = 0, fmEnd = 0, fmT = 0; // star
    float amRate = 0, amBias = 0, amDepth = 0;          // static / grind gate
    float shaperK = 0;                                  // ikick / boom808 saturation
    FiltDef filt[2]; int nfilt = 0;
    Lane env;
};

// RBJ biquad with Web Audio's parameter meaning (LP/HP Q is resonance in dB)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (FType type, float f, float q, float sr)
    {
        f = std::clamp (f, 10.0f, sr * 0.49f);
        const float w0 = 2.0f * kPi * f / sr, cw = std::cos (w0), sw = std::sin (w0);
        float alpha, nb0, nb1, nb2, na0, na1, na2;
        if (type == BP)
        {
            alpha = sw / (2.0f * std::max (0.0001f, q));
            nb0 = alpha; nb1 = 0; nb2 = -alpha;
        }
        else
        {
            const float g = std::pow (10.0f, q / 20.0f);
            alpha = sw / (2.0f * g);
            if (type == LP) { nb0 = (1 - cw) * 0.5f; nb1 = 1 - cw; nb2 = (1 - cw) * 0.5f; }
            else            { nb0 = (1 + cw) * 0.5f; nb1 = -(1 + cw); nb2 = (1 + cw) * 0.5f; }
        }
        na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

struct Rng
{
    uint32_t s = 0x12345678u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; }
    inline float uni() { return (next() + 1.0f) * 0.5f; }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// A playing drum hit
struct Voice
{
    struct LState { double ph[6] = {}; double fmPh = 0, vibPh = 0, amPh = 0; Biquad bq[2]; int coefCount = 0; };
    std::vector<Layer> layers;
    std::vector<LState> st;
    float gain = 1, panL = 1, panR = 1;
    float t = 0, endT = 0, dt = 1.0f / 44100.0f, sr = 44100.0f;
    int delaySamples = 0;
    bool active = false, chokable = false;
    float chokeAt = -1;
    Rng rng;

    void start (float sampleRate, int delay)
    {
        sr = sampleRate; dt = 1.0f / sr; t = 0; delaySamples = delay; active = true; chokeAt = -1;
        st.assign (layers.size(), LState());
        endT = 0;
        for (auto& l : layers) endT = std::max (endT, std::min (l.stopT, l.env.endTime() + 0.02f));
        for (size_t i = 0; i < layers.size(); ++i) updateFilters (i, 0.0f);
    }

    void updateFilters (size_t i, float time)
    {
        auto& l = layers[i];
        for (int k = 0; k < l.nfilt; ++k)
        {
            const auto& fd = l.filt[k];
            float f = fd.f;
            if (fd.f1 > 0 && fd.sweepT > 0)
            {
                const float x = std::clamp (time / fd.sweepT, 0.0f, 1.0f);
                f = fd.f * std::pow (fd.f1 / fd.f, x);
            }
            st[i].bq[k].set (fd.type, f, fd.q, sr);
        }
    }

    void render (float* L, float* R, int n)
    {
        for (int s = 0; s < n; ++s)
        {
            if (delaySamples > 0) { --delaySamples; continue; }
            float sum = 0;
            for (size_t i = 0; i < layers.size(); ++i)
            {
                const Layer& l = layers[i];
                if (t < l.startT || t > l.stopT) continue;
                LState& ls = st[i];
                if (l.nfilt && l.filt[0].sweepT > 0 && (++ls.coefCount & 15) == 0) updateFilters (i, t);
                float x = 0;
                if (l.wave == NOISE) x = rng.next();
                else
                {
                    for (int k = 0; k < l.nfreq; ++k)
                    {
                        float f = l.freqs[k];
                        if (k == 0)
                        {
                            const float lt = t - l.startT;
                            if (l.sweepT > 0)
                            {
                                const float u = std::clamp (lt / l.sweepT, 0.0f, 1.0f);
                                f = l.linSweep ? l.freqs[0] + (l.f1 - l.freqs[0]) * u : l.freqs[0] * std::pow (l.f1 / l.freqs[0], u);
                            }
                            if (l.vibDepth > 0) { ls.vibPh += l.vibRate * dt; f += std::sin (2 * kPi * (float) ls.vibPh) * l.vibDepth; }
                            if (l.fmRatio > 0)
                            {
                                const float u = std::clamp (lt / l.fmT, 0.0f, 1.0f);
                                const float mg = l.fmStart * std::pow (l.fmEnd / l.fmStart, u);
                                ls.fmPh += l.freqs[0] * l.fmRatio * dt;
                                f += std::sin (2 * kPi * (float) ls.fmPh) * mg;
                            }
                        }
                        const float inc = std::max (0.0f, f) * dt;
                        double& ph = ls.ph[k];
                        const float p = (float) ph;
                        float o;
                        switch (l.wave)
                        {
                            case SINE: o = std::sin (2 * kPi * p); break;
                            case TRI: o = 1.0f - 4.0f * std::fabs (p - 0.5f); o = -o; break;
                            case SQUARE: o = (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, inc) - polyBlep (std::fmod (p + 0.5f, 1.0f), inc); break;
                            default: o = 2.0f * p - 1.0f - polyBlep (p, inc); break;
                        }
                        x += o;
                        ph += inc; if (ph >= 1.0) ph -= std::floor (ph);
                    }
                }
                x *= l.mix;
                if (l.amRate > 0)
                {
                    ls.amPh += l.amRate * dt; if (ls.amPh >= 1.0) ls.amPh -= 1.0;
                    x *= l.amBias + l.amDepth * (ls.amPh < 0.5 ? 1.0f : -1.0f);
                }
                for (int k = 0; k < l.nfilt; ++k) x = ls.bq[k].process (x);
                if (l.shaperK > 0) x = std::tanh (l.shaperK * std::clamp (x, -1.0f, 1.0f)) / std::tanh (l.shaperK);
                sum += x * l.env.valueAt (t);
            }
            float g = gain;
            if (chokeAt >= 0 && t > chokeAt) g *= std::exp (-(t - chokeAt) / 0.006f);
            L[s] += sum * g * panL;
            R[s] += sum * g * panR;
            t += dt;
            if (t > endT || (chokeAt >= 0 && t > chokeAt + 0.06f)) { active = false; return; }
        }
    }
};

struct VoiceParams { float tune, decay, tone, level; };
struct GlobalParams { float pitch, gdecay, drift, width; };

// Builds the layers for one hit of engine `id` (ported 1:1 from the prototype)
void buildHit (const char* id, const VoiceParams& p, const GlobalParams& g, Rng& rng, Voice& out);
}
