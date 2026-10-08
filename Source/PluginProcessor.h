// BEAT FROG · REZONANZA
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Dsp.h"
#include "Kits.h"

class BeatFrogProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kVoices = 8;
    static const char* globalIds[10];
    static const char* globalNames[10];
    static const char* voiceKeys[4];

    BeatFrogProcessor();
    ~BeatFrogProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BEAT FROG"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return bf::NUM_KITS; }
    int getCurrentProgram() override { return kit.load(); }
    void setCurrentProgram (int index) override { loadKit (index); }
    const juce::String getProgramName (int index) override { return bf::KITS[juce::jlimit (0, bf::NUM_KITS - 1, index)].name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- called from the editor (message thread) ---
    void loadKit (int index);
    bool getStep (int v, int step) const { return (pattern[(size_t) v].load() >> step) & 1u; }
    void setStep (int v, int step, bool on);
    void clearPage (int page);
    void randomisePage (int page);
    void audition (int v) { auditionMask.fetch_or (1u << v); }
    void toggleInternalPlay() { internalPlay = ! internalPlay.load(); }
    bool isRunning() const { return running.load(); }
    int getCurrentStep() const { return curStep.load(); }
    int getLength() const { return length.load(); }
    void setLength (int l) { length = l; }
    int getKit() const { return kit.load(); }
    float kitVoiceDefault (int v, int key) const;
    float kitGlobalDefault (int g) const { return bf::KITS[kit.load()].g[g]; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void trigger (int v, int offset, float vel);
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    std::array<std::atomic<uint32_t>, kVoices> pattern {};
    std::atomic<int> length { 16 }, kit { 0 }, curStep { -1 };
    std::atomic<bool> internalPlay { false }, running { false };
    std::atomic<uint32_t> auditionMask { 0 };

    std::array<std::atomic<float>*, 10> gp {};
    std::array<std::array<std::atomic<float>*, 4>, kVoices> vp {};
    std::array<std::atomic<float>*, kVoices> muteP {}, soloP {};
    std::atomic<float>* filterP = nullptr; std::atomic<float>* outputP = nullptr;

    std::array<bf::Voice, 64> voices;
    bf::Rng rng;
    double sr = 44100.0, intPpq = 0.0;
    bool wasInternal = false;

    // master chain
    juce::AudioBuffer<float> bus, wet;
    bf::Biquad lp[2], hp[2], echoLp[2];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lpF { 20000.0f }, hpF { 20.0f };
    juce::SmoothedValue<float> dryG { 1.0f }, wetG { 0.0f }, sendG { 0.0f }, fbG { 0.0f }, outG { 1.0f };
    juce::dsp::Convolution reverb;
    std::vector<float> echoBuf[2];
    int echoW = 0;
    float compEnv = 0.0f;
    float cThr = 1e9f, cRatio = 0, cLinThr = 1, cKneeThr = 1, cK = 1, cKneeDb = 0, cMakeup = 1;
    void updateCompCurve (float thr, float ratio, float knee);
    float saturate (float x) const;
    std::vector<float> lookBuf[2];
    int lookW = 0, lookLen = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatFrogProcessor)
};
