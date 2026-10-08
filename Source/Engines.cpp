// BEAT FROG · REZONANZA
// Every drum engine, ported 1:1 from the browser prototype.
#include "Dsp.h"
#include <cstring>
#include <initializer_list>

namespace bf
{
namespace
{
struct B
{
    Voice& v;
    const VoiceParams& p;
    float T, r, ds;
    float dec (float a, float b) const { return (a + p.decay * b) * ds; }
    Layer& osc (Wave w, float f, float d)
    {
        v.layers.emplace_back();
        Layer& l = v.layers.back();
        l.wave = w; l.freqs[0] = f; l.stopT = d + 0.1f;
        return l;
    }
    Layer& ns (float d) { return osc (NOISE, 0, d); }
    Layer& metal (float base, float d)
    {
        Layer& l = osc (SQUARE, base * 2, d);
        const float m[6] = { 2, 3, 4.16f, 5.43f, 6.79f, 8.21f };
        for (int k = 0; k < 6; ++k) l.freqs[k] = base * m[k];
        l.nfreq = 6; l.mix = 0.25f;
        return l;
    }
};

Layer& F (Layer& l, FType t, float f, float q = 0.7f) { if (l.nfilt < 2) { l.filt[l.nfilt].type = t; l.filt[l.nfilt].f = f; l.filt[l.nfilt].q = q; ++l.nfilt; } return l; }
Layer& E (Layer& l, float t0, float peak, float d, float att = 0.002f)
{
    l.env.segs = { { t0, 1e-4f, 0 }, { t0 + att, std::max (2e-4f, peak), 1 }, { t0 + att + d, 1e-4f, 1 } };
    return l;
}
Layer& S (Layer& l, float a, float b, float d) { l.freqs[0] = a; l.f1 = std::max (1.0f, b); l.sweepT = d; return l; }

void partials (B& b, float f, std::initializer_list<float> ks, std::initializer_list<float> amps, float d0, std::initializer_list<float> decs)
{
    auto k = ks.begin(); auto a = amps.begin(); auto dd = decs.begin();
    for (; k != ks.end(); ++k, ++a, ++dd)
    {
        Layer& l = b.osc (SINE, f * *k, d0 * *dd);
        E (l, 0, *a, d0 * *dd);
    }
}

void bursts (Lane& lane, std::initializer_list<float> offs, float peak, float low, float len)
{
    lane.segs.push_back ({ 0, 1e-4f, 0 });
    for (float o : offs)
    {
        lane.segs.push_back ({ o, 1e-4f, 0 });
        lane.segs.push_back ({ o + 0.001f, peak, 1 });
        lane.segs.push_back ({ o + len, low, 1 });
    }
}
} // namespace

void buildHit (const char* id, const VoiceParams& p, const GlobalParams& g, Rng& rng, Voice& v)
{
    v.layers.clear();
    v.layers.reserve (24);
    v.chokable = false;
    const float r = std::pow (2.0f, (p.tune - 0.5f) * 2 + (g.pitch - 0.5f) * 2) * (1.0f + rng.next() * g.drift * 0.025f);
    const float ds = std::pow (2.5f, (g.gdecay - 0.5f) * 2);
    B b { v, p, p.tone, r, ds };
    const float T = p.tone;
    auto is = [id] (const char* s) { return std::strcmp (id, s) == 0; };

    // --- classic ---
    if (is ("kick")) { float d = b.dec (0.12f, 1.3f), f0 = 50 * r; { auto& o = b.osc (SINE, f0 * 5, d); S (o, f0 * 5, f0, 0.045f); E (o, 0, 1, d); } if (T > 0.02f) { auto& n = b.ns (0.03f); F (n, HP, 1800 + T * 4000); E (n, 0, T * 0.5f, 0.012f); } }
    else if (is ("snare")) { float f0 = 185 * r; { auto& o = b.osc (TRI, f0 * 1.6f, 0.4f); S (o, f0 * 1.6f, f0, 0.03f); E (o, 0, (1 - T) * 0.8f + 0.15f, b.dec (0.07f, 0.15f)); } float d = b.dec (0.08f, 0.45f); auto& n = b.ns (d); F (n, HP, (900 + T * 3000) * r); E (n, 0, 0.35f + T * 0.5f, d); }
    else if (is ("clap")) { float d = b.dec (0.08f, 0.5f); auto& n = b.ns (d + 0.1f); F (n, BP, (700 + T * 1800) * r, 1.4f); bursts (n.env, { 0.0f, 0.011f, 0.022f }, 0.9f, 0.03f, 0.009f); n.env.segs.push_back ({ 0.031f, 1e-4f, 0 }); n.env.segs.push_back ({ 0.032f, 0.7f, 1 }); n.env.segs.push_back ({ 0.032f + d, 1e-4f, 1 }); }
    else if (is ("hat") || is ("ohat")) { bool open = is ("ohat"); float d = open ? b.dec (0.15f, 0.9f) : b.dec (0.025f, 0.12f); auto& m = b.metal (40 * r, d); F (m, BP, 10000, 0.8f); F (m, HP, 5000 + T * 5000); E (m, 0, 2.2f, d); v.chokable = open; }
    else if (is ("tom")) { float f0 = 100 * r, d = b.dec (0.15f, 0.9f); { auto& o = b.osc (SINE, f0 * 2, d); S (o, f0 * 2, f0, 0.1f); E (o, 0, 0.9f, d); } if (T > 0.02f) { auto& n = b.ns (0.08f); F (n, LP, 3000); E (n, 0, T * 0.3f, 0.05f); } }
    else if (is ("rim")) { float d = b.dec (0.02f, 0.08f); { auto& o = b.osc (TRI, 1700 * r, d); F (o, HP, 300 + T * 1500); E (o, 0, 0.8f, d); } auto& q = b.osc (SQUARE, 455 * r, d); q.mix = 0.3f; F (q, HP, 300 + T * 1500); E (q, 0, 0.8f, d); }
    else if (is ("bell")) { float d = b.dec (0.08f, 0.6f); for (float f : { 540.0f, 800.0f }) { auto& o = b.osc (SQUARE, f * r, d); F (o, BP, (900 + T * 2000) * r, 2); E (o, 0, 0.8f, d); } }
    // --- bahamas percussion ---
    else if (is ("lowdrum")) { float f0 = 62 * r, d = b.dec (0.25f, 0.8f); { auto& o = b.osc (SINE, f0 * 1.6f, d); S (o, f0 * 1.6f, f0, 0.04f); E (o, 0, 0.9f, d); } { auto& o = b.osc (TRI, f0 * 2, 0.1f); E (o, 0, 0.2f, 0.08f); } auto& n = b.ns (0.04f); F (n, LP, 600); E (n, 0, T * 0.5f, 0.03f); }
    else if (is ("conga") || is ("bongo")) { bool bo = is ("bongo"); float f0 = (bo ? 360.0f : 210.0f) * r, d = bo ? b.dec (0.06f, 0.25f) : b.dec (0.12f, 0.5f); { auto& o = b.osc (SINE, f0 * 1.4f, d); S (o, f0 * 1.4f, f0, 0.012f); E (o, 0, 0.8f, d); } auto& n = b.ns (0.03f); F (n, BP, 1800 + T * 2500, 1.5f); E (n, 0, 0.2f + T * 0.5f, 0.02f); }
    else if (is ("shaker")) { float d = b.dec (0.06f, 0.2f); auto& n = b.ns (d + 0.04f); F (n, BP, 4000 + T * 5000, 1.2f); E (n, 0, 1.4f, d, 0.012f + (1 - T) * 0.02f); }
    else if (is ("cabasa")) { float d = b.dec (0.12f, 0.4f); auto& n = b.ns (d + 0.04f); F (n, HP, 7000 + T * 3000); E (n, 0, 0.6f, d, 0.03f); }
    else if (is ("steelpan")) { float d0 = 0.4f + p.decay * 1.6f * ds; partials (b, 330 * r, { 1, 2, 3.01f, 1.98f }, { 0.5f, 0.3f, 0.12f, 0.15f }, d0, { 1, 0.6f, 0.3f, 0.8f }); auto& o = b.osc (SINE, 330 * r * 4.1f, 0.04f); E (o, 0, T * 0.3f, 0.03f); }
    else if (is ("clave")) { float d = b.dec (0.02f, 0.08f); { auto& o = b.osc (SINE, 2500 * r, d); E (o, 0, 0.8f, d); } auto& o = b.osc (TRI, 5000 * r, d); E (o, 0, T * 0.2f, d * 0.6f); }
    else if (is ("agogo")) { float d = b.dec (0.1f, 0.5f); { auto& o = b.osc (TRI, 870 * r, d); E (o, 0, 0.5f, d); } { auto& o = b.osc (TRI, 1305 * r, d); E (o, 0, 0.35f, d * 0.8f); } auto& o = b.osc (SQUARE, 870 * r, d); E (o, 0, T * 0.15f, d * 0.5f); }
    // --- industrial ---
    else if (is ("ikick")) { float f0 = 45 * r, d = b.dec (0.15f, 1.0f); { auto& o = b.osc (SINE, 300 * r, d); S (o, 300 * r, f0, 0.05f); o.shaperK = 30; E (o, 0, 0.8f, d); } auto& n = b.ns (0.08f); F (n, LP, 1500); n.mix = 0.3f + T * 0.4f; n.shaperK = 30; E (n, 0, 0.8f, d); }
    else if (is ("metal")) { float d = b.dec (0.1f, 0.8f); { auto& o = b.osc (SQUARE, 280 * r, d); const float fs[5] = { 280, 413, 587, 733, 1020 }; for (int k = 0; k < 5; ++k) o.freqs[k] = fs[k] * r; o.nfreq = 5; o.mix = 0.15f; F (o, BP, 3000 + T * 4000, 3); E (o, 0, 0.9f, d); } auto& n = b.ns (0.05f); F (n, HP, 4000); E (n, 0, 0.4f, 0.04f); }
    else if (is ("hammer")) { float d = b.dec (0.08f, 0.2f); { auto& o = b.osc (SINE, 160 * r, d); S (o, 160 * r, 90 * r, 0.02f); E (o, 0, 0.9f, d); } auto& n = b.ns (0.2f); F (n, BP, 1200 + T * 2000, 5); E (n, 0, 1, b.dec (0.05f, 0.1f)); }
    else if (is ("steam")) { float d = b.dec (0.08f, 0.3f); auto& n = b.ns (d + 0.02f); F (n, HP, 2500 + T * 4000); E (n, 0, 0.6f, d, 0.004f); }
    else if (is ("valve")) { float d = b.dec (0.2f, 0.8f); auto& n = b.ns (d + 0.02f); F (n, BP, 9000 * r, 1 + T * 8); n.filt[0].f1 = 900 * r; n.filt[0].sweepT = d; E (n, 0, 0.8f, d, 0.01f); }
    else if (is ("pipe")) { float d0 = (0.2f + p.decay * 1.2f) * ds; partials (b, 170 * r, { 1, 2.76f, 5.4f, 8.93f }, { 0.6f, 0.35f * (0.4f + T), 0.2f * (0.3f + T), 0.1f * T + 0.01f }, d0, { 1, 0.6f, 0.4f, 0.25f }); }
    else if (is ("anvil")) { float d0 = (0.05f + p.decay * 0.4f) * ds; partials (b, 1180 * r, { 1, 2.53f, 4.13f }, { 0.5f, 0.3f, 0.2f }, d0, { 1, 0.7f, 0.5f }); auto& n = b.ns (0.02f); F (n, HP, 6000); E (n, 0, 0.3f + T * 0.4f, 0.01f); }
    else if (is ("grind")) { float d = b.dec (0.08f, 0.4f); for (int k = 0; k < 3; ++k) { auto& o = k == 2 ? b.ns (d) : b.osc (SAW, (k == 0 ? 55.0f : 82.0f) * r, d); o.mix = 0.35f; o.amRate = 30; o.amBias = 0.5f; o.amDepth = 0.5f; F (o, BP, 700 + T * 2500, 4); E (o, 0, 0.9f, d); } }
    // --- outerspace ---
    else if (is ("pulsar")) { float d = b.dec (0.3f, 1.2f), sd = 0.3f + p.decay * 0.4f; { auto& o = b.osc (SINE, 420 * r, d); S (o, 420 * r, 32 * r, sd); E (o, 0, 1, d); } auto& o = b.osc (TRI, 840 * r, d); S (o, 840 * r, 64 * r, sd); E (o, 0, T * 0.3f, d * 0.5f); }
    else if (is ("laser")) { float d = b.dec (0.1f, 0.35f); auto& o = b.osc (SQUARE, 2400 * r, d); S (o, 2400 * r, 90 * r, 0.08f + p.decay * 0.25f); F (o, LP, 1500 + T * 6000); E (o, 0, 0.5f, d); }
    else if (is ("static")) { float d = b.dec (0.08f, 0.4f); auto& n = b.ns (d); F (n, BP, 3000 + T * 4000); n.amRate = 23 + T * 60; n.amBias = 0; n.amDepth = 1; E (n, 0, 0.9f, d); }
    else if (is ("blip")) { float d = b.dec (0.015f, 0.06f); { auto& o = b.osc (SINE, 3200 * r, d); E (o, 0, 1, d); } auto& o = b.osc (SINE, 6400 * r, d); E (o, 0, T * 0.3f, d); }
    else if (is ("sweep")) { float d = b.dec (0.15f, 0.6f); auto& n = b.ns (d); F (n, BP, 400 * r, 4); n.filt[0].f1 = 9000 * r; n.filt[0].sweepT = d; n.env.segs = { { 0, 1e-4f, 0 }, { d * 0.7f, 0.5f + T * 0.4f, 2 }, { d, 1e-4f, 1 } }; }
    else if (is ("ufo")) { float d = b.dec (0.2f, 0.9f); auto& o = b.osc (SINE, 330 * r, d); o.vibRate = 9 + T * 14; o.vibDepth = 40 * r; E (o, 0, 0.5f, d); }
    else if (is ("beep")) { float d = b.dec (0.04f, 0.15f); auto& o = b.osc (SQUARE, 1000 * r, d); F (o, LP, 2000 + T * 4000); E (o, 0, 0.35f, d); }
    else if (is ("star")) { float d = b.dec (0.3f, 1.4f), f = 880 * r; auto& o = b.osc (SINE, f, d); o.fmRatio = 1.41f; o.fmStart = f * (1 + T * 4); o.fmEnd = 1; o.fmT = d * 0.5f; E (o, 0, 0.45f, d); }
    // --- memory ---
    else if (is ("tapekick")) { float f0 = 56 * r, d = b.dec (0.15f, 0.8f); { auto& o = b.osc (SINE, f0 * 2, d); S (o, f0 * 2, f0, 0.04f); F (o, LP, 600 + T * 1500); E (o, 0, 1, d); } auto& o = b.osc (TRI, f0, d); o.mix = 0.3f; F (o, LP, 600 + T * 1500); E (o, 0, 1, d); }
    else if (is ("brush")) { float d = b.dec (0.15f, 0.5f); auto& n = b.ns (d + 0.03f); F (n, BP, 2500 + T * 3000, 0.8f); E (n, 0, 0.4f, d, 0.025f); }
    else if (is ("snap")) { { auto& n = b.ns (0.1f); F (n, BP, (2200 + T * 1500) * r, 4); E (n, 0, 2.2f, b.dec (0.03f, 0.05f)); } auto& o = b.osc (SINE, 1600 * r, 0.02f); E (o, 0, 0.3f, 0.015f); }
    else if (is ("tick")) { float d = b.dec (0.008f, 0.03f); { auto& o = b.osc (SINE, 7000 * r, d); E (o, 0, 0.6f, d); } auto& n = b.ns (d + 0.02f); F (n, HP, 9000); E (n, 0, 0.7f + T * 0.6f, b.dec (0.01f, 0.04f)); }
    else if (is ("tamb")) { float d = b.dec (0.1f, 0.3f); auto& m = b.metal (55 * r, d + 0.05f); F (m, HP, 6000 + T * 3000); bursts (m.env, { 0.0f, 0.012f, 0.025f }, 1.8f, 0.12f, 0.01f); m.env.segs.push_back ({ 0.035f + d, 1e-4f, 1 }); }
    else if (is ("woodtom")) { float f0 = 140 * r, d = b.dec (0.1f, 0.5f); auto& o = b.osc (TRI, f0 * 1.25f, d); S (o, f0 * 1.25f, f0, 0.05f); F (o, LP, 1200 + T * 2000); E (o, 0, 0.9f, d); }
    else if (is ("musicbox")) { float f = 1046 * r, d = b.dec (0.3f, 1.2f); { auto& o = b.osc (SINE, f, d); E (o, 0, 0.4f, d); } { auto& o = b.osc (SINE, f * 3, 0.12f); E (o, 0, 0.1f + T * 0.15f, 0.1f); } auto& o = b.osc (SINE, f * 5.4f, 0.06f); E (o, 0, T * 0.06f + 0.001f, 0.05f); }
    // --- slow jamz ---
    else if (is ("boom808")) { float f0 = 46 * r, d = b.dec (0.4f, 2.2f); { auto& o = b.osc (SINE, f0 * 1.5f, d); S (o, f0 * 1.5f, f0, 0.06f); o.shaperK = 1 + T * 8; E (o, 0, 0.9f, d); } auto& n = b.ns (0.02f); F (n, HP, 3000); E (n, 0, 0.15f, 0.006f); }
    else if (is ("snare808")) { float d1 = b.dec (0.08f, 0.12f); { auto& o = b.osc (TRI, 238 * r, d1); E (o, 0, 0.5f, d1); } { auto& o = b.osc (TRI, 476 * r, d1); E (o, 0, 0.3f, d1 * 0.7f); } float d = b.dec (0.12f, 0.3f); auto& n = b.ns (d); F (n, HP, 1800 + T * 2500); E (n, 0, 0.5f, d); }
    // --- acoustic / real instruments ---
    else if (is ("akick")) { float f0 = 55 * r, d = b.dec (0.25f, 0.6f); { auto& o = b.osc (SINE, 130 * r, d); S (o, 130 * r, f0, 0.03f); E (o, 0, 1, d); } { auto& o = b.osc (SINE, f0 * 1.6f, d); E (o, 0, 0.22f, d * 0.4f); } { auto& n = b.ns (0.03f); F (n, BP, 2500 + T * 3000, 1); E (n, 0, 0.25f + T * 0.45f, 0.008f); } auto& n = b.ns (0.08f); F (n, LP, 300); E (n, 0, 0.5f, 0.04f); }
    else if (is ("asnare")) { float d1 = b.dec (0.08f, 0.1f); { auto& o = b.osc (SINE, 230 * r, d1); S (o, 230 * r, 190 * r, 0.015f); E (o, 0, 0.7f, d1); } { auto& o = b.osc (SINE, 330 * r, 0.1f); E (o, 0, 0.35f, 0.06f); } float d = b.dec (0.12f, 0.3f); { auto& n = b.ns (d); F (n, BP, 4500, 0.6f); F (n, HP, 1200); E (n, 0, 0.9f + T * 0.5f, d, 0.003f); } auto& n = b.ns (0.02f); F (n, BP, 5000, 2); E (n, 0, 0.4f, 0.005f); }
    else if (is ("ahat") || is ("ahatopen")) { bool open = is ("ahatopen"); float d = open ? b.dec (0.3f, 0.9f) : b.dec (0.03f, 0.1f); { auto& m = b.metal (40 * r, d); F (m, BP, 10000, 0.6f); F (m, HP, 6000 + T * 2500); E (m, 0, 1.6f, d); } auto& n = b.ns (d + 0.02f); F (n, HP, 9000); E (n, 0, 0.5f, d * 0.8f); v.chokable = open; }
    else if (is ("ride")) { float d0 = (0.8f + p.decay * 2.2f) * ds; partials (b, 380 * r, { 1, 1.47f, 2.06f, 2.78f, 3.42f, 4.12f, 5.2f }, { 0.08f, 0.07f, 0.07f, 0.06f, 0.05f, 0.04f, 0.03f }, d0, { 1, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f }); { auto& n = b.ns (0.04f); F (n, BP, 5000, 3); E (n, 0, 0.3f, 0.03f); } auto& m = b.metal (60 * r, d0); F (m, HP, 6000); E (m, 0, 0.35f + T * 0.3f, d0 * 0.7f, 0.01f); }
    else if (is ("crash")) { float d = b.dec (0.8f, 2.2f); { auto& n = b.ns (d); F (n, HP, 3500); E (n, 0, 0.5f, d, 0.004f); } { auto& m = b.metal (52 * r, d); F (m, BP, 6000, 0.5f); E (m, 0, 0.9f, d * 0.8f, 0.004f); } auto& n = b.ns (d * 0.3f); F (n, BP, 2500, 0.8f); E (n, 0, 0.25f, d * 0.3f); }
    else if (is ("atom")) { float f0 = 120 * r, d = b.dec (0.2f, 0.8f); { auto& o = b.osc (SINE, f0 * 1.35f, d); S (o, f0 * 1.35f, f0, 0.12f); E (o, 0, 0.9f, d); } { auto& o = b.osc (SINE, f0 * 1.59f, d); E (o, 0, 0.25f, d * 0.35f); } { auto& n = b.ns (0.02f); F (n, BP, 3000, 1); E (n, 0, 0.25f, 0.01f); } auto& n = b.ns (d * 0.3f); F (n, LP, 1500); E (n, 0, 0.02f + T * 0.12f, d * 0.3f); }
    else if (is ("rimclick")) { float d = b.dec (0.02f, 0.05f); { auto& n = b.ns (d + 0.02f); F (n, BP, 1700 * r, 9); E (n, 0, 2.5f + T, d); } { auto& o = b.osc (TRI, 1100 * r, 0.04f); E (o, 0, 0.4f, 0.03f); } auto& o = b.osc (SINE, 450 * r, 0.03f); E (o, 0, 0.2f, 0.02f); }
    else if (is ("brushsnare")) { float d = b.dec (0.15f, 0.4f); { auto& n = b.ns (d + 0.05f); F (n, BP, 3500 + T * 2000, 0.6f); E (n, 0, 0.35f, d, 0.04f); } { auto& n = b.ns (0.03f); F (n, BP, 4000, 0.8f); E (n, 0, 0.25f, 0.02f); } auto& o = b.osc (SINE, 200 * r, 0.06f); E (o, 0, 0.15f, 0.05f); }
    else if (is ("cajonbass")) { float d = b.dec (0.15f, 0.3f); { auto& o = b.osc (SINE, 110 * r, d); S (o, 110 * r, 80 * r, 0.02f); E (o, 0, 1, d); } { auto& n = b.ns (0.06f); F (n, LP, 250); E (n, 0, 0.5f, 0.05f); } auto& n = b.ns (0.1f); F (n, BP, 3500, 1); E (n, 0, 0.08f, 0.08f, 0.01f); }
    else if (is ("cajonslap")) { { auto& o = b.osc (SINE, 220 * r, 0.06f); E (o, 0, 0.35f, 0.05f); } { auto& n = b.ns (0.1f); F (n, BP, (2200 + T * 1500) * r, 1.2f); E (n, 0, 0.8f, b.dec (0.03f, 0.06f)); } float d = b.dec (0.06f, 0.15f); auto& n = b.ns (d); F (n, HP, 3000); E (n, 0, 0.3f + T * 0.3f, d, 0.004f); }
    else if (is ("djembe")) { float d = b.dec (0.15f, 0.35f); { auto& o = b.osc (SINE, 95 * r, d); S (o, 95 * r, 72 * r, 0.03f); E (o, 0, 1, d); } auto& n = b.ns (0.05f); F (n, LP, 500); E (n, 0, 0.3f + T * 0.3f, 0.03f); }
    else if (is ("djembeslap")) { { auto& o = b.osc (SINE, 390 * r, 0.05f); E (o, 0, 0.3f, 0.04f); } { auto& o = b.osc (SINE, 610 * r, 0.04f); E (o, 0, 0.15f, 0.03f); } auto& n = b.ns (0.05f); F (n, BP, 3000 + T * 1500, 1.5f); E (n, 0, 0.9f, b.dec (0.02f, 0.04f)); }
    else if (is ("timpani")) { float d0 = (0.8f + p.decay * 2.5f) * ds; partials (b, 98 * r, { 1, 1.5f, 1.99f, 2.44f, 2.98f }, { 0.7f, 0.35f, 0.2f, 0.1f, 0.05f }, d0, { 1, 0.8f, 0.6f, 0.5f, 0.4f }); auto& n = b.ns (0.05f); F (n, LP, 800 + T * 1500); E (n, 0, 0.4f, 0.03f); }
    else if (is ("triangle")) { float d0 = (0.6f + p.decay * 3) * ds; partials (b, 1290 * r, { 1, 2.31f, 3.68f, 5.05f, 6.4f }, { 0.3f, 0.2f, 0.12f, 0.08f + T * 0.05f, 0.05f + T * 0.05f }, d0, { 1, 0.9f, 0.8f, 0.7f, 0.6f }); auto& n = b.ns (0.02f); F (n, HP, 8000); E (n, 0, 0.2f, 0.005f); }
    else if (is ("castanets")) { for (int k = 0; k < 2; ++k) { float o = k ? 0.018f + T * 0.02f : 0.0f; auto& n = b.ns (0.08f + o); F (n, BP, 2300 * r, 6); E (n, o, k ? 1.6f : 2.4f, b.dec (0.015f, 0.03f)); } }
    else if (is ("woodblock")) { float d = b.dec (0.03f, 0.08f); { auto& o = b.osc (SINE, 800 * r, d); E (o, 0, 1.4f, d); } { auto& o = b.osc (SINE, 2000 * r, 0.04f); E (o, 0, 0.25f, 0.03f); } auto& n = b.ns (0.02f); F (n, BP, 2000, 4); E (n, 0, 0.3f + T * 0.4f, 0.01f); }
    else if (is ("bayan")) { float d = b.dec (0.3f, 0.9f); { auto& o = b.osc (SINE, 88 * r, d); o.f1 = 98 * r; o.sweepT = 0.25f; o.linSweep = true; E (o, 0, 1, d); } { auto& o = b.osc (SINE, 176 * r, d); E (o, 0, 0.15f, d * 0.4f); } auto& n = b.ns (0.04f); F (n, LP, 300); E (n, 0, 0.3f, 0.03f); }
    else if (is ("bayanslide")) { float d = b.dec (0.4f, 0.6f); auto& o = b.osc (SINE, 80 * r, d); S (o, 80 * r, 160 * r, 0.35f); E (o, 0, 0.9f, d); }
    else if (is ("dayan")) { float d0 = (0.2f + p.decay * 0.8f) * ds; partials (b, 280 * r, { 1, 2, 3, 4 }, { 0.5f, 0.35f, 0.2f, 0.1f }, d0, { 1, 0.8f, 0.6f, 0.4f }); auto& n = b.ns (0.02f); F (n, BP, 3000, 2); E (n, 0, 0.4f + T * 0.4f, 0.01f); }
    else if (is ("dayanhigh")) { float d0 = (0.3f + p.decay * 1) * ds; partials (b, 280 * r, { 2, 3, 4 }, { 0.35f, 0.25f, 0.15f }, d0, { 1, 0.8f, 0.6f }); auto& n = b.ns (0.02f); F (n, BP, 4000, 2); E (n, 0, 0.2f + T * 0.3f, 0.008f); }
    else if (is ("dayanmute")) { { auto& o = b.osc (SINE, 280 * r, 0.05f); E (o, 0, 0.5f, 0.04f); } auto& n = b.ns (0.04f); F (n, BP, 1500, 1.5f); E (n, 0, 1.2f + T * 0.6f, b.dec (0.02f, 0.03f)); }
    else if (is ("manjira")) { float d0 = (0.5f + p.decay * 2) * ds; partials (b, 2100 * r, { 1, 1.52f, 2.76f, 3.9f }, { 0.25f, 0.18f, 0.12f, 0.08f }, d0, { 1, 0.9f, 0.7f, 0.5f }); partials (b, 2112 * r, { 1, 1.52f }, { 0.15f, 0.1f }, d0, { 1, 0.8f }); auto& n = b.ns (0.02f); F (n, HP, 7000); E (n, 0, 0.2f + T * 0.3f, 0.005f); }

    // output stage of the hit: level, velocity applied by caller, random stereo position
    v.gain = p.level * p.level * 1.3f;
    if (g.width > 0.01f)
    {
        const float pan = rng.next() * g.width * 0.9f, x = (pan + 1) * 0.5f;
        v.panL = std::cos (x * kPi * 0.5f); v.panR = std::sin (x * kPi * 0.5f);
    }
    else { v.panL = v.panR = 1; }
}
} // namespace bf
