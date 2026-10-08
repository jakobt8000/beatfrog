// BEAT FROG · REZONANZA
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour ink { 0xff000000 }, paper { 0xffffffff }, panel { 0xfff3f3f1 }, grey { 0xff9a9a96 }, dim { 0xff6e6e6a },
                   light { 0xffd6d6d2 }, line { 0xffdddddA }, cellLine { 0xffe2e2df }, numGrey { 0xffbdbdba };

struct Fonts
{
    juce::Typeface::Ptr regular, bold;
    Fonts();
    juce::Font get (float px, bool isBold = false, float spacingPx = 0.0f) const;
    static Fonts& instance();
};

// Pixel knob (ring + needle built from square pixels), bound to a parameter
class PixelKnob : public juce::Component
{
public:
    PixelKnob (juce::RangedAudioParameter& p, juce::String label, int knobSize, float labelPx, int labelGap);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<float()> resetValue;
    int knobSize;
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attach;
    juce::String label;
    float labelPx; int labelGap;
    float value = 0, dragStart = 0;
};

// Big LED-ring knob: drag = filter, click = play / stop
class LedKnob : public juce::Component
{
public:
    LedKnob (juce::RangedAudioParameter& p, BeatFrogProcessor& proc);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool playing = false;
private:
    juce::RangedAudioParameter& param;
    BeatFrogProcessor& proc;
    juce::ParameterAttachment attach;
    float value = 0.5f, dragStart = 0; bool moved = false;
};

// Small square toggle (M / S) bound to a bool parameter
class PixelToggle : public juce::Component
{
public:
    PixelToggle (juce::RangedAudioParameter& p, juce::String text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attach;
    juce::String text; bool on = false;
};

// Square text button
class TextBtn : public juce::Component
{
public:
    TextBtn (juce::String t, float px, bool boxed) : text (t), px (px), boxed (boxed) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
    juce::String text; float px; bool boxed; bool selected = false; juce::Colour colour = ink;
};

class InfoOverlay : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (BeatFrogProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { paintRow = -1; }
private:
    void timerCallback() override;
    juce::Rectangle<float> cellRect (int j, int row) const;
    int stripVisible (int v) const;

    BeatFrogProcessor& proc;
    std::unique_ptr<LedKnob> big;
    juce::OwnedArray<PixelKnob> globals, voiceKnobs;
    std::unique_ptr<PixelKnob> output;
    juce::OwnedArray<PixelToggle> mutes, solos;
    TextBtn prevKit { "<", 20, false }, nextKit { ">", 20, false }, info { "i", 22, true };
    TextBtn len16 { "16", 12, true }, len32 { "32", 12, true }, pagePrev { "<", 15, false }, pageNext { ">", 15, false };
    TextBtn clearBtn { "CLEAR", 12, true }, rndBtn { "RND", 12, true };
    InfoOverlay infoBox;
    int page = 0, lastStep = -2, lastKit = -1, paintRow = -1; bool paintVal = false, lastRun = false;
};
} // namespace ui

class BeatFrogEditor : public juce::AudioProcessorEditor
{
public:
    explicit BeatFrogEditor (BeatFrogProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (ui::paper); }
private:
    ui::Content content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatFrogEditor)
};
