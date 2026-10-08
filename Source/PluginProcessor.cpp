// BEAT FROG · REZONANZA
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

const char* BeatFrogProcessor::globalIds[10] = { "swing", "drive", "room", "echo", "crush", "pitch", "gdecay", "comp", "width", "drift" };
const char* BeatFrogProcessor::globalNames[10] = { "SWING", "DRIVE", "ROOM", "ECHO", "CRUSH", "PITCH", "DECAY", "COMP", "WIDTH", "DRIFT" };
const char* BeatFrogProcessor::voiceKeys[4] = { "tune", "decay", "tone", "level" };

static juce::String vid (int v, const char* key) { return "v" + juce::String (v + 1) + "_" + key; }

juce::AudioProcessorValueTreeState::ParameterLayout BeatFrogProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using P = juce::AudioParameterFloat;
    l.add (std::make_unique<P> (juce::ParameterID { "filter", 1 }, "Filter", 0.0f, 1.0f, 0.5f));
    l.add (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output", 0.0f, 1.0f, 0.75f));
    for (int i = 0; i < 10; ++i)
        l.add (std::make_unique<P> (juce::ParameterID { globalIds[i], 1 }, juce::String (globalNames[i]).substring (0, 1) + juce::String (globalNames[i]).substring (1).toLowerCase(), 0.0f, 1.0f, bf::GDEF[i]));
    const char* keyNames[4] = { "Tune", "Decay", "Tone", "Level" };
    for (int v = 0; v < kVoices; ++v)
    {
        const auto& d = bf::KITS[0].v[v];
        const float defs[4] = { d.tune, d.decay, d.tone, d.level };
        for (int k = 0; k < 4; ++k)
            l.add (std::make_unique<P> (juce::ParameterID { vid (v, voiceKeys[k]), 1 }, "Voice " + juce::String (v + 1) + " " + keyNames[k], 0.0f, 1.0f, defs[k]));
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { vid (v, "mute"), 1 }, "Voice " + juce::String (v + 1) + " Mute", false));
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { vid (v, "solo"), 1 }, "Voice " + juce::String (v + 1) + " Solo", false));
    }
    return l;
}

static uint32_t patBits (const char* s)
{
    uint32_t b = 0;
    for (int i = 0; i < 32 && s[i]; ++i) if (s[i] == 'x') b |= (1u << i);
    return b;
}

BeatFrogProcessor::BeatFrogProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BEATFROG", createLayout())
{
    filterP = apvts.getRawParameterValue ("filter");
    outputP = apvts.getRawParameterValue ("output");
    for (int i = 0; i < 10; ++i) gp[(size_t) i] = apvts.getRawParameterValue (globalIds[i]);
    for (int v = 0; v < kVoices; ++v)
    {
        for (int k = 0; k < 4; ++k) vp[(size_t) v][(size_t) k] = apvts.getRawParameterValue (vid (v, voiceKeys[k]));
        muteP[(size_t) v] = apvts.getRawParameterValue (vid (v, "mute"));
        soloP[(size_t) v] = apvts.getRawParameterValue (vid (v, "solo"));
        pattern[(size_t) v] = patBits (bf::KITS[0].pat[v]);
    }
}

bool BeatFrogProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float BeatFrogProcessor::kitVoiceDefault (int v, int key) const
{
    const auto& d = bf::KITS[kit.load()].v[v];
    const float defs[4] = { d.tune, d.decay, d.tone, d.level };
    return defs[key];
}

void BeatFrogProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    bus.setSize (2, samplesPerBlock);
    wet.setSize (2, samplesPerBlock);
    for (auto& v : voices) v.active = false;

    // reverb impulse: 1.6 s of decaying noise, normalised like Web Audio's ConvolverNode
    const int len = (int) (sampleRate * 1.6);
    juce::AudioBuffer<float> ir (2, len);
    bf::Rng r; double pow = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < len; ++i)
        {
            const float x = r.next() * std::pow (1.0f - (float) i / (float) len, 3.0f);
            ir.setSample (ch, i, x); pow += x * x;
        }
    const float power = (float) std::max (0.000125, std::sqrt (pow / (2.0 * len)));
    ir.applyGain (0.00125f / power * (float) (44100.0 / sampleRate));
    reverb.reset();
    reverb.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    reverb.loadImpulseResponse (std::move (ir), sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);

    for (auto& b : echoBuf) b.assign ((size_t) (sampleRate * 2.5) + 8, 0.0f);
    echoW = 0;
    for (int c = 0; c < 2; ++c) { lp[c].reset(); hp[c].reset(); echoLp[c].reset(); echoLp[c].set (bf::LP, 3000.0f, 1.0f, (float) sr); }
    for (auto* s : { &lpF, &hpF }) s->reset (sampleRate, 0.05);
    for (auto* s : { &dryG, &wetG, &sendG, &fbG, &outG }) s->reset (sampleRate, 0.06);
    compEnv = 0;
    // Web Audio's compressor looks 6 ms ahead
    lookLen = std::max (1, (int) std::round (0.006 * sampleRate));
    for (auto& b : lookBuf) b.assign ((size_t) lookLen, 0.0f);
    lookW = 0;
    setLatencySamples (lookLen);
}

void BeatFrogProcessor::trigger (int v, int offset, float vel)
{
    const int k = kit.load();
    const char* engine = bf::KITS[k].v[v].engine;
    bf::VoiceParams p { vp[(size_t) v][0]->load(), vp[(size_t) v][1]->load(), vp[(size_t) v][2]->load(), vp[(size_t) v][3]->load() };
    bf::GlobalParams g { gp[5]->load(), gp[6]->load(), gp[9]->load(), gp[8]->load() };

    if (std::strcmp (engine, "hat") == 0 || std::strcmp (engine, "ahat") == 0)
        for (auto& o : voices)
            if (o.active && o.chokable && o.chokeAt < 0) o.chokeAt = o.t + (float) (offset + o.delaySamples) / (float) sr;

    bf::Voice* slot = nullptr;
    for (auto& o : voices) if (! o.active) { slot = &o; break; }
    if (slot == nullptr)
    {
        slot = &voices[0];
        for (auto& o : voices) if (o.t > slot->t) slot = &o;
    }
    slot->rng.s = rng.s ^ 0x9e3779b9u; rng.next();
    bf::buildHit (engine, p, g, rng, *slot);
    slot->gain *= vel;
    slot->start ((float) sr, offset);
}

void BeatFrogProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (bus.getNumSamples() < n) { bus.setSize (2, n, false, false, true); wet.setSize (2, n, false, false, true); }

    // --- transport: follow the DAW, or the big knob's internal play when the DAW is stopped ---
    double bpm = 120.0, ppq0 = 0.0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ppq0 = *q;
        }
    const double ppqLen = (double) n / sr * bpm / 60.0;
    const bool internal = ! hostPlaying && internalPlay.load();
    if (internal) { if (! wasInternal) intPpq = 0.0; ppq0 = intPpq; intPpq += ppqLen; }
    wasInternal = internal;
    const bool run = hostPlaying || internal;
    running = run;

    if (run)
    {
        const double sw = gp[0]->load() * 0.4;
        const int len = length.load();
        auto stepPpq = [sw] (long long k) {
            const long long pair = (long long) std::floor ((double) k / 2.0);
            return (double) pair * 0.5 + ((k - pair * 2) == 1 ? 0.25 * (1.0 + sw) : 0.0);
        };
        const double ppq1 = ppq0 + ppqLen;
        bool anySolo = false;
        for (int v = 0; v < kVoices; ++v) anySolo = anySolo || soloP[(size_t) v]->load() > 0.5f;
        const float drift = gp[9]->load();
        for (long long k = (long long) std::floor (ppq0 * 4.0) - 2; k <= (long long) std::floor (ppq1 * 4.0) + 1; ++k)
        {
            const double s = stepPpq (k);
            if (s < ppq0 || s >= ppq1) continue;
            const int offset = juce::jlimit (0, n - 1, (int) ((s - ppq0) / ppqLen * n));
            const int step = (int) (((k % len) + len) % len);
            curStep = step;
            for (int v = 0; v < kVoices; ++v)
            {
                if (! getStep (v, step) || muteP[(size_t) v]->load() > 0.5f) continue;
                if (anySolo && soloP[(size_t) v]->load() < 0.5f) continue;
                trigger (v, offset, 1.0f - rng.uni() * drift * 0.35f);
            }
        }
    }
    else curStep = -1;

    // --- MIDI: notes C1..G1 (36..43) play the 8 voices ---
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() < 36 + kVoices)
            trigger (m.getNoteNumber() - 36, meta.samplePosition, m.getFloatVelocity());
    }
    const uint32_t aud = auditionMask.exchange (0);
    for (int v = 0; v < kVoices; ++v) if (aud & (1u << v)) trigger (v, 0, 1.0f);

    // --- render voices ---
    bus.clear();
    float* L = bus.getWritePointer (0); float* R = bus.getWritePointer (1);
    for (auto& v : voices) if (v.active) v.render (L, R, n);

    // --- master chain (bus → drive → crush → filter → room + echo → comp → output) ---
    const float drive = gp[1]->load(), room = gp[2]->load(), echo = gp[3]->load(), crush = gp[4]->load(), comp = gp[7]->load();
    const float k = 1.0f + drive * 18.0f, tk = std::tanh (k);
    const int bits = crush < 0.02f ? 0 : (int) std::round (12.0f - crush * 9.0f);
    const float lv = bits ? std::pow (2.0f, (float) (bits - 1)) : 1.0f;
    const float h = filterP->load() * 100.0f;
    lpF.setTargetValue (h < 50 ? std::min (20000.0f, 160.0f * std::pow (2.0f, h / 50.0f * 7.0f)) : 20000.0f);
    hpF.setTargetValue (h > 50 ? 20.0f * std::pow (2.0f, (h - 50.0f) / 50.0f * 7.5f) : 20.0f);
    dryG.setTargetValue (1.0f - room * 0.25f);
    wetG.setTargetValue (room * room * 0.9f);
    sendG.setTargetValue (echo * 0.8f);
    fbG.setTargetValue (echo * 0.55f);
    const float ov = outputP->load();
    outG.setTargetValue (ov * ov * 1.6f * (1.0f + comp * 0.8f));

    for (int i = 0; i < n; ++i)
    {
        if ((i & 31) == 0)
        {
            const float lf = lpF.getNextValue(), hf = hpF.getNextValue();
            lpF.skip (31); hpF.skip (31);
            for (int c = 0; c < 2; ++c) { lp[c].set (bf::LP, lf, 0.9f, (float) sr); hp[c].set (bf::HP, hf, 0.9f, (float) sr); }
        }
        for (int c = 0; c < 2; ++c)
        {
            float* d = bus.getWritePointer (c);
            float x = d[i] * 0.8f;
            x = std::tanh (k * juce::jlimit (-1.0f, 1.0f, x)) / tk;
            if (bits) x = std::round (juce::jlimit (-1.0f, 1.0f, x) * lv) / lv;
            d[i] = hp[c].process (lp[c].process (x));
        }
    }

    for (int c = 0; c < 2; ++c) wet.copyFrom (c, 0, bus, c, 0, n);
    {
        juce::dsp::AudioBlock<float> blk (wet.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // compressor: Chromium's DynamicsCompressor curve (linear knee, auto make-up, 6 ms look-ahead)
    const float thr = -4.0f - comp * 30.0f, ratio = 2.0f + comp * 10.0f, knee = 30.0f;
    if (thr != cThr || ratio != cRatio) updateCompCurve (thr, ratio, knee);
    const float atk = std::exp (-1.0f / (0.003f * (float) sr)), rel = std::exp (-1.0f / (0.15f * (float) sr));
    const int echoLen = (int) echoBuf[0].size();
    const int dSamp = juce::jlimit (1, echoLen - 2, (int) (60.0 / bpm * 0.75 * sr));
    float* oL = buffer.getWritePointer (0);
    float* oR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float dg = dryG.getNextValue(), wg = wetG.getNextValue(), sg = sendG.getNextValue(), fg = fbG.getNextValue(), og = outG.getNextValue();
        float y[2];
        const int rIdx = (echoW - dSamp + echoLen) % echoLen;
        for (int c = 0; c < 2; ++c)
        {
            const float pre = bus.getSample (c, i);
            const float dl = echoLp[c].process (echoBuf[c][(size_t) rIdx]);
            echoBuf[c][(size_t) echoW] = pre * sg + dl * fg;
            y[c] = pre * dg + wet.getSample (c, i) * wg + dl * 0.8f;
        }
        echoW = (echoW + 1) % echoLen;
        const float lvl = std::max (std::fabs (y[0]), std::fabs (y[1]));
        const float gr = lvl > 1e-6f ? juce::Decibels::gainToDecibels (saturate (lvl) / lvl) : 0.0f;
        compEnv = gr < compEnv ? atk * compEnv + (1 - atk) * gr : rel * compEnv + (1 - rel) * gr;
        const float gain = juce::Decibels::decibelsToGain (compEnv) * cMakeup * og;
        const float dL = lookBuf[0][(size_t) lookW], dR = lookBuf[1][(size_t) lookW];
        lookBuf[0][(size_t) lookW] = y[0]; lookBuf[1][(size_t) lookW] = y[1];
        lookW = (lookW + 1) % lookLen;
        oL[i] = dL * gain;
        if (oR) oR[i] = dR * gain;
    }
}

static float kneeCurve (float x, float k, float lt) { return x < lt ? x : lt + (1.0f - std::exp (-k * (x - lt))) / k; }

void BeatFrogProcessor::updateCompCurve (float thr, float ratio, float knee)
{
    cThr = thr; cRatio = ratio;
    cLinThr = juce::Decibels::decibelsToGain (thr);
    cKneeThr = juce::Decibels::decibelsToGain (thr + knee);
    auto slopeAt = [this] (float x, float k) {
        const float x2 = x * 1.001f;
        const float y = kneeCurve (x, k, cLinThr), y2 = kneeCurve (x2, k, cLinThr);
        return (juce::Decibels::gainToDecibels (y2) - juce::Decibels::gainToDecibels (y)) / (juce::Decibels::gainToDecibels (x2) - juce::Decibels::gainToDecibels (x));
    };
    float minK = 0.1f, maxK = 10000.0f, k = 5.0f;
    for (int i = 0; i < 15; ++i)
    {
        k = std::sqrt (minK * maxK);
        if (slopeAt (cKneeThr, k) < 1.0f / ratio) maxK = k; else minK = k;
    }
    cK = k;
    cKneeDb = juce::Decibels::gainToDecibels (kneeCurve (cKneeThr, k, cLinThr));
    cMakeup = std::pow (1.0f / saturate (1.0f), 0.6f);
}

float BeatFrogProcessor::saturate (float x) const
{
    if (x < cKneeThr) return kneeCurve (x, cK, cLinThr);
    const float xDb = juce::Decibels::gainToDecibels (x);
    return juce::Decibels::decibelsToGain (cKneeDb + (xDb - (cThr + 30.0f)) / cRatio);
}

void BeatFrogProcessor::setStep (int v, int step, bool on)
{
    auto& p = pattern[(size_t) v];
    uint32_t cur = p.load();
    while (! p.compare_exchange_weak (cur, on ? (cur | (1u << step)) : (cur & ~(1u << step)))) {}
}

void BeatFrogProcessor::clearPage (int page)
{
    const uint32_t mask = page ? 0xffff0000u : 0x0000ffffu;
    for (auto& p : pattern) p = p.load() & ~mask;
}

void BeatFrogProcessor::randomisePage (int page)
{
    const float dens[8] = { 0.3f, 0.2f, 0.12f, 0.6f, 0.12f, 0.12f, 0.15f, 0.1f };
    juce::Random r;
    for (int v = 0; v < kVoices; ++v)
        for (int j = 0; j < 16; ++j)
        {
            const int s = page * 16 + j;
            const bool on = (j == 0 && v == 0) || r.nextFloat() < dens[v] * (j % 4 == 0 ? 1.8f : 1.0f);
            setStep (v, s, on);
        }
}

void BeatFrogProcessor::loadKit (int index)
{
    index = ((index % bf::NUM_KITS) + bf::NUM_KITS) % bf::NUM_KITS;
    kit = index;
    const auto& K = bf::KITS[index];
    auto set = [this] (const juce::String& id, float v) {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    for (int i = 0; i < 10; ++i) set (globalIds[i], K.g[i]);
    for (int v = 0; v < kVoices; ++v)
    {
        const float defs[4] = { K.v[v].tune, K.v[v].decay, K.v[v].tone, K.v[v].level };
        for (int k = 0; k < 4; ++k) set (vid (v, voiceKeys[k]), defs[k]);
        pattern[(size_t) v] = patBits (K.pat[v]);
    }
    updateHostDisplay();
}

void BeatFrogProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("kit", kit.load(), nullptr);
    state.setProperty ("length", length.load(), nullptr);
    for (int v = 0; v < kVoices; ++v) state.setProperty ("pat" + juce::String (v), (int) pattern[(size_t) v].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BeatFrogProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        kit = juce::jlimit (0, bf::NUM_KITS - 1, (int) state.getProperty ("kit", 0));
        length = (int) state.getProperty ("length", 16) == 32 ? 32 : 16;
        for (int v = 0; v < kVoices; ++v)
            if (state.hasProperty ("pat" + juce::String (v)))
                pattern[(size_t) v] = (uint32_t) (int) state.getProperty ("pat" + juce::String (v));
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* BeatFrogProcessor::createEditor() { return new BeatFrogEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BeatFrogProcessor(); }
