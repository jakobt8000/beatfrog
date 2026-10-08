// BEAT FROG · REZONANZA
#include "PluginEditor.h"
#include "BinaryData.h"

namespace ui
{
// ---------- fonts ----------
Fonts::Fonts()
{
    regular = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenRegular_ttf, BinaryData::SilkscreenRegular_ttfSize);
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::SilkscreenBold_ttf, BinaryData::SilkscreenBold_ttfSize);
}
Fonts& Fonts::instance() { static Fonts f; return f; }
juce::Font Fonts::get (float px, bool isBold, float spacingPx) const
{
    juce::Font f { juce::FontOptions (isBold ? bold : regular).withPointHeight (px) };
    if (spacingPx != 0.0f) f.setExtraKerningFactor (spacingPx * 0.5f / px);
    return f;
}
static void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float px, bool bold, float sp, juce::Justification j, juce::Colour c)
{
    g.setColour (c);
    g.setFont (Fonts::instance().get (px, bold, sp));
    g.drawText (s, r, j, false);
}
static float textW (const juce::String& s, float px, bool bold, float sp)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (Fonts::instance().get (px, bold, sp), s, 0, 0);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// ---------- pixel art ----------
static void drawPixelKnob (juce::Graphics& g, float x0, float y0, int n, float ps, float v)
{
    g.setColour (ink);
    const float c = (n - 1) * 0.5f, R = c - 0.6f, a = (-135.0f + v * 270.0f) * juce::MathConstants<float>::pi / 180.0f;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (std::abs (std::hypot (x - c, y - c) - R) < 0.75f) g.fillRect (x0 + x * ps, y0 + y * ps, ps, ps);
    int lx = -99, ly = -99;
    for (float t = R * 0.2f; t < R * 0.82f; t += 0.4f)
    {
        const int px = juce::roundToInt (c + std::sin (a) * t), py = juce::roundToInt (c - std::cos (a) * t);
        if (px == lx && py == ly) continue;
        lx = px; ly = py;
        g.fillRect (x0 + px * ps, y0 + py * ps, ps, ps);
    }
}

static void drawLedKnob (juce::Graphics& g, float x0, float y0, float v, bool playing)
{
    const int N = 36; const float PS = 4, C = 17.5f;
    const float a = -135.0f + v * 270.0f, pi = juce::MathConstants<float>::pi;
    auto sq = [&] (float r, float deg, juce::Colour col) {
        const float rad = deg * pi / 180.0f;
        const int px = juce::roundToInt (C + std::sin (rad) * r), py = juce::roundToInt (C - std::cos (rad) * r);
        g.setColour (col);
        g.fillRect (x0 + px * PS, y0 + py * PS, PS * 2, PS * 2);
    };
    for (int k = 0; k < 24; ++k)
    {
        const float d = -135.0f + k * (270.0f / 23.0f);
        sq (16, d, d <= a + 0.1f ? (playing ? ink : grey) : light);
    }
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
        {
            const float dd = std::hypot (x - C, y - C);
            if (dd < 10.5f) { g.setColour (paper); g.fillRect (x0 + x * PS, y0 + y * PS, PS, PS); }
            if (std::abs (dd - 10.5f) < 0.9f) { g.setColour (ink); g.fillRect (x0 + x * PS, y0 + y * PS, PS, PS); }
        }
    sq (7, a, ink);
}

// ---------- PixelKnob ----------
PixelKnob::PixelKnob (juce::RangedAudioParameter& p, juce::String l, int ks, float lpx, int gap)
    : knobSize (ks), param (p), attach (p, [this] (float v) { value = param.convertTo0to1 (v); repaint(); }), label (l), labelPx (lpx), labelGap (gap)
{
    attach.sendInitialUpdate();
}
void PixelKnob::paint (juce::Graphics& g)
{
    const int n = knobSize / 4;
    const float x0 = (getWidth() - n * 4) * 0.5f;
    drawPixelKnob (g, x0, 0, n, 4, value);
    text (g, label, { 0.0f, (float) (n * 4 + labelGap), (float) getWidth(), labelPx + 3 }, labelPx, false, 1.0f, juce::Justification::centredTop, ink);
}
void PixelKnob::mouseDown (const juce::MouseEvent&) { dragStart = value; attach.beginGesture(); }
void PixelKnob::mouseDrag (const juce::MouseEvent& e)
{
    const float speed = e.mods.isShiftDown() ? 0.0015f : 0.006f;
    value = juce::jlimit (0.0f, 1.0f, dragStart - e.getDistanceFromDragStartY() * speed);
    attach.setValueAsPartOfGesture (param.convertFrom0to1 (value));
    repaint();
}
void PixelKnob::mouseUp (const juce::MouseEvent&) { attach.endGesture(); }
void PixelKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    const float v = resetValue ? resetValue() : param.getDefaultValue();
    attach.setValueAsCompleteGesture (param.convertFrom0to1 (v));
}

// ---------- LedKnob ----------
LedKnob::LedKnob (juce::RangedAudioParameter& p, BeatFrogProcessor& pr)
    : param (p), proc (pr), attach (p, [this] (float v) { value = param.convertTo0to1 (v); repaint(); })
{
    attach.sendInitialUpdate();
}
void LedKnob::paint (juce::Graphics& g) { drawLedKnob (g, 0, 0, value, playing); }
void LedKnob::mouseDown (const juce::MouseEvent&) { dragStart = value; moved = false; attach.beginGesture(); }
void LedKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (std::abs (e.getDistanceFromDragStartY()) > 3) moved = true;
    if (! moved) return;
    value = juce::jlimit (0.0f, 1.0f, dragStart - e.getDistanceFromDragStartY() * 0.004f);
    attach.setValueAsPartOfGesture (param.convertFrom0to1 (value));
    repaint();
}
void LedKnob::mouseUp (const juce::MouseEvent&)
{
    attach.endGesture();
    if (! moved) proc.toggleInternalPlay();
}

// ---------- PixelToggle ----------
PixelToggle::PixelToggle (juce::RangedAudioParameter& p, juce::String t)
    : param (p), attach (p, [this] (float v) { on = v > 0.5f; repaint(); if (auto* c = getParentComponent()) c->repaint(); }), text (t)
{
    attach.sendInitialUpdate();
}
void PixelToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (on ? paper : ink); g.fillRect (r);
    g.setColour (paper); g.drawRect (r, 2.0f);
    ui::text (g, text, r, 11, true, 0, juce::Justification::centred, on ? ink : paper);
}
void PixelToggle::mouseDown (const juce::MouseEvent&) { attach.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

// ---------- TextBtn ----------
void TextBtn::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (boxed)
    {
        g.setColour (selected ? ink : paper); g.fillRect (r);
        g.setColour (ink); g.drawRect (r, text == "i" ? 3.0f : 2.0f);
    }
    ui::text (g, text, r, px, true, 1, juce::Justification::centred, boxed && selected ? paper : colour);
}

// ---------- info ----------
void InfoOverlay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRect (r.translated (0, 10).expanded (6));
    g.setColour (paper); g.fillRect (r);
    g.setColour (ink); g.drawRect (r, 3.0f);
    text (g, "BEAT FROG", { 18, 0, 440, r.getHeight() }, 58, false, 2, juce::Justification::centredLeft, ink);
    juce::Rectangle<float> side { 452, 3, r.getWidth() - 455, r.getHeight() - 6 };
    g.setColour (panel); g.fillRect (side);
    const char* rows[8][2] = { { "TYPE", "DRUM MACHINE" }, { "VOICES", "8" }, { "KNOBS", "44" }, { "STEPS", "16 / 32" },
                               { "KITS", "16" }, { "SAMPLES", "0" }, { "MAKER", "REZONANZA" }, { "NEIGHBOURS", "NOTIFIED" } };
    for (int i = 0; i < 8; ++i)
    {
        const float y = 16 + i * 17.0f;
        text (g, rows[i][0], { side.getX() + 20, y, 120, 16 }, 13, false, 1, juce::Justification::centredLeft, dim);
        text (g, rows[i][1], { side.getX() + 132, y, 120, 16 }, 13, false, 1, juce::Justification::centredLeft, juce::Colour (0xff333333));
    }
}

// ---------- layout constants ----------
static constexpr float W = 1620, TOP = 220, STRIP_Y = 220, STRIP_H = 240, BAR_Y = 460, ROWS_Y = 494, ROW_H = 34, BOT_Y = 766;
static constexpr float GX = 180, GW = 1410; // pattern grid
static float colW() { return GW / 16.0f; }
static float stripW() { return W / 8.0f; }

Content::Content (BeatFrogProcessor& p) : proc (p)
{
    auto& ap = proc.apvts;
    big = std::make_unique<LedKnob> (*ap.getParameter ("filter"), proc);
    addAndMakeVisible (*big);
    big->setBounds (68, 38, 144, 144);

    for (int i = 0; i < 10; ++i)
    {
        auto* k = globals.add (new PixelKnob (*ap.getParameter (BeatFrogProcessor::globalIds[i]), BeatFrogProcessor::globalNames[i], 84, 12, 6));
        k->resetValue = [this, i] { return proc.kitGlobalDefault (i); };
        addAndMakeVisible (k);
        const float gap = (1075.0f - 840.0f) / 9.0f;
        k->setBounds ((int) (340 + i * (84 + gap)) - 8, 30, 100, 84 + 6 + 16);
    }
    output = std::make_unique<PixelKnob> (*ap.getParameter ("output"), "OUTPUT", 84, 12, 6);
    output->resetValue = [] { return 0.75f; };
    addAndMakeVisible (*output);
    output->setBounds (1478 + (142 - 100) / 2, 24, 100, 84 + 6 + 16);
    addAndMakeVisible (info);
    info.setBounds (1478 + (142 - 44) / 2, 142, 44, 44);
    info.onClick = [this] { infoBox.setVisible (true); infoBox.toFront (false); };

    addAndMakeVisible (prevKit); addAndMakeVisible (nextKit);
    prevKit.setBounds (613, 159, 44, 36); nextKit.setBounds (1097, 159, 44, 36);
    prevKit.onClick = [this] { proc.loadKit (proc.getKit() - 1); repaint(); };
    nextKit.onClick = [this] { proc.loadKit (proc.getKit() + 1); repaint(); };

    const char* keyNames[4] = { "TUNE", "DECAY", "TONE", "LEVEL" };
    for (int v = 0; v < 8; ++v)
    {
        const float x = v * stripW();
        for (int k = 0; k < 4; ++k)
        {
            juce::String id = "v" + juce::String (v + 1) + "_" + BeatFrogProcessor::voiceKeys[k];
            auto* kn = voiceKnobs.add (new PixelKnob (*ap.getParameter (id), keyNames[k], 60, 11, 6));
            kn->resetValue = [this, v, k] { return proc.kitVoiceDefault (v, k); };
            addAndMakeVisible (kn);
            const float innerW = stripW() - 2 - 20, cw = innerW / 2;
            const float cx = x + 10 + cw * (0.5f + (k % 2));
            kn->setBounds ((int) (cx - 40), k < 2 ? 273 : 365, 80, 60 + 6 + 14);
        }
        auto* s = solos.add (new PixelToggle (*ap.getParameter ("v" + juce::String (v + 1) + "_solo"), "S"));
        auto* m = mutes.add (new PixelToggle (*ap.getParameter ("v" + juce::String (v + 1) + "_mute"), "M"));
        addAndMakeVisible (s); addAndMakeVisible (m);
        const float sx = x + stripW() - 2 - 8 - 24;
        s->setBounds ((int) sx, 227, 24, 22);
        m->setBounds ((int) (sx - 4 - 24), 227, 24, 22);
    }

    for (auto* b : { &len16, &len32, &pagePrev, &pageNext, &clearBtn, &rndBtn }) addAndMakeVisible (*b);
    len16.setBounds (70, 466, 30, 22); len32.setBounds (104, 466, 30, 22);
    len16.onClick = [this] { proc.setLength (16); page = 0; repaint(); };
    len32.onClick = [this] { proc.setLength (32); repaint(); };
    pagePrev.setBounds (66, 768, 26, 30); pageNext.setBounds (132, 768, 26, 30);
    pagePrev.onClick = pageNext.onClick = [this] { if (proc.getLength() == 32) { page = 1 - page; repaint(); } };
    const float rw = textW ("RND B", 12, true, 1) + 22, cw2 = textW ("CLEAR B", 12, true, 1) + 22;
    rndBtn.setBounds ((int) (1590 - rw), 770, (int) rw, 26);
    clearBtn.setBounds ((int) (1590 - rw - 8 - cw2), 770, (int) cw2, 26);
    clearBtn.onClick = [this] { proc.clearPage (page); repaint(); };
    rndBtn.onClick = [this] { proc.randomisePage (page); repaint(); };

    addChildComponent (infoBox);
    infoBox.setBounds (460, 100, 702, 165);
    startTimerHz (30);
}

int Content::stripVisible (int v) const
{
    bool anySolo = false;
    for (auto* s : solos) anySolo = anySolo || s->isOn();
    if (mutes[v]->isOn()) return 0;
    if (anySolo && ! solos[v]->isOn()) return 0;
    return 1;
}

void Content::timerCallback()
{
    const bool run = proc.isRunning();
    if (run != big->playing) { big->playing = run; big->repaint(); }
    const int st = proc.getCurrentStep();
    if (st != lastStep || run != lastRun || proc.getKit() != lastKit)
    {
        lastStep = st; lastRun = run;
        if (proc.getKit() != lastKit) { lastKit = proc.getKit(); repaint(); }
        else repaint (0, (int) BAR_Y, (int) W, (int) (800 - BAR_Y));
    }
    for (int v = 0; v < 8; ++v)
    {
        const float a = stripVisible (v) ? 1.0f : 0.4f;
        for (int k = 0; k < 4; ++k) voiceKnobs[v * 4 + k]->setAlpha (a);
    }
    const bool two = proc.getLength() == 32;
    len16.selected = ! two; len32.selected = two;
    if (! two) page = 0;
    pagePrev.colour = pageNext.colour = two ? ink : juce::Colour (0xffcfcfcc);
    clearBtn.text = two ? (page ? "CLEAR B" : "CLEAR A") : "CLEAR";
    rndBtn.text = two ? (page ? "RND B" : "RND A") : "RND";
    for (auto* b : { &len16, &len32, &pagePrev, &pageNext, &clearBtn, &rndBtn }) b->repaint();
}

juce::Rectangle<float> Content::cellRect (int j, int row) const
{
    return { GX + j * colW(), ROWS_Y + row * ROW_H, colW(), ROW_H - 1 };
}

void Content::paint (juce::Graphics& g)
{
    const auto& kit = bf::KITS[proc.getKit()];
    g.fillAll (paper);
    // top band
    g.setColour (panel); g.fillRect (0.0f, 0.0f, 280.0f, TOP - 3); g.fillRect (1475.0f, 0.0f, 145.0f, TOP - 3);
    g.setColour (ink); g.fillRect (277.0f, 0.0f, 3.0f, TOP - 3); g.fillRect (1475.0f, 0.0f, 3.0f, TOP - 3); g.fillRect (0.0f, TOP - 3, W, 3.0f);
    const juce::String kitName = juce::String (proc.getKit() + 1).paddedLeft ('0', 2) + "  " + kit.name;
    text (g, kitName, { 657, 159, 440, 36 }, 18, true, 3, juce::Justification::centred, ink);

    // channel strips
    for (int v = 0; v < 8; ++v)
    {
        const float x = v * stripW();
        const bool vis = stripVisible (v);
        g.setColour (ink);
        g.fillRect (x + stripW() - 2, STRIP_Y, 2.0f, STRIP_H);
        g.setColour (vis ? ink : ink.withAlpha (0.4f));
        g.fillRect (x, STRIP_Y, stripW() - 2, 36.0f);
        text (g, kit.v[v].name, { x + 12, STRIP_Y, stripW() - 80, 36 }, 13, true, 1, juce::Justification::centredLeft, paper);
    }

    // step bar
    const bool two = proc.getLength() == 32;
    const int off = page * 16, pos = proc.getCurrentStep();
    g.setColour (ink); g.fillRect (0.0f, BAR_Y, W, 3.0f); g.fillRect (0.0f, ROWS_Y - 2, W, 2.0f);
    g.setColour (panel); g.fillRect (0.0f, BAR_Y + 3, 147.0f, ROWS_Y - BAR_Y - 5);
    g.setColour (ink); g.fillRect (147.0f, BAR_Y + 3, 3.0f, ROWS_Y - BAR_Y - 5);
    text (g, "STEPS", { 10, BAR_Y + 3, 60, 29 }, 12, false, 1, juce::Justification::centredLeft, ink);
    for (int j = 0; j < 16; ++j)
    {
        const juce::Colour c = pos == off + j ? ink : (j % 4 == 0 ? dim : numGrey);
        text (g, juce::String (off + j + 1), { GX + j * colW() + 6, BAR_Y + 3, colW(), 29 }, 12, false, 1, juce::Justification::centredLeft, c);
    }

    // pattern rows
    bool anySolo = false;
    for (auto* s : solos) anySolo = anySolo || s->isOn();
    for (int v = 0; v < 8; ++v)
    {
        const float y = ROWS_Y + v * ROW_H;
        const bool vis = stripVisible (v);
        g.setColour (panel); g.fillRect (0.0f, y, 147.0f, ROW_H - 1);
        g.setColour (ink); g.fillRect (147.0f, y, 3.0f, ROW_H - 1);
        g.setColour (line); g.fillRect (0.0f, y + ROW_H - 1, W, 1.0f);
        text (g, kit.v[v].name, { 16, y, 128, ROW_H - 1 }, 13, true, 1, juce::Justification::centredLeft, vis ? ink : grey);
        const float alpha = vis ? 1.0f : 0.35f;
        for (int j = 0; j < 16; ++j)
        {
            auto r = cellRect (j, v);
            const int s = off + j;
            const bool head = pos == s, alt = (j / 4) % 2 == 1;
            if (head) { g.setColour (ink.withAlpha (0.08f * alpha)); g.fillRect (r); }
            else if (alt) { g.setColour (ink.withAlpha (0.025f * alpha)); g.fillRect (r); }
            g.setColour ((j % 4 == 0 ? ink : cellLine).withMultipliedAlpha (alpha));
            g.fillRect (r.getX(), r.getY(), j % 4 == 0 ? 2.0f : 1.0f, r.getHeight());
            if (proc.getStep (v, s))
            {
                g.setColour ((head ? grey : ink).withMultipliedAlpha (alpha));
                g.fillRect (r.getX() + 3 + (j % 4 == 0 ? 1 : 0), r.getY() + 6, r.getWidth() - 6, r.getHeight() - 12);
            }
        }
        g.setColour (ink.withMultipliedAlpha (alpha)); g.fillRect (GX + GW - 2, y, 2.0f, ROW_H - 1);
    }

    // bottom bar
    g.setColour (ink); g.fillRect (0.0f, BOT_Y, W, 2.0f);
    text (g, "PAGE", { 16, BOT_Y + 2, 50, 32 }, 12, false, 1, juce::Justification::centredLeft, ink);
    text (g, "A", { 94, BOT_Y + 2, 14, 32 }, 13, true, 0, juce::Justification::centred, page == 0 ? ink : numGrey);
    text (g, "B", { 114, BOT_Y + 2, 14, 32 }, 13, true, 0, juce::Justification::centred, ! two ? juce::Colour (0xffe2e2df) : (page == 1 ? ink : numGrey));
    if (two && proc.isRunning())
    {
        g.setColour (ink);
        g.fillRect (pos >= 16 ? 114.0f : 94.0f, BOT_Y + 25, 14.0f, 2.0f);
    }
}

void Content::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    // audition from strip header names or row labels
    if (p.y >= STRIP_Y && p.y < STRIP_Y + 36) { const int v = (int) (p.x / stripW()); if (p.x < v * stripW() + stripW() - 70) proc.audition (v); return; }
    if (p.x < 147 && p.y >= ROWS_Y && p.y < ROWS_Y + 8 * ROW_H) { proc.audition ((int) ((p.y - ROWS_Y) / ROW_H)); return; }
    if (p.x >= GX && p.x < GX + GW && p.y >= ROWS_Y && p.y < ROWS_Y + 8 * ROW_H)
    {
        const int row = (int) ((p.y - ROWS_Y) / ROW_H), j = (int) ((p.x - GX) / colW()), s = page * 16 + j;
        paintRow = row; paintVal = ! proc.getStep (row, s);
        proc.setStep (row, s, paintVal);
        if (paintVal && ! proc.isRunning()) proc.audition (row);
        repaint();
    }
}

void Content::mouseDrag (const juce::MouseEvent& e)
{
    if (paintRow < 0) return;
    const auto p = e.position;
    if (p.x < GX || p.x >= GX + GW) return;
    const int j = (int) ((p.x - GX) / colW()), s = page * 16 + j;
    if (proc.getStep (paintRow, s) != paintVal) { proc.setStep (paintRow, s, paintVal); repaint(); }
}
} // namespace ui

// ---------- editor ----------
BeatFrogEditor::BeatFrogEditor (BeatFrogProcessor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (810, 400, 2430, 1200);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1620.0 / 800.0);
    setSize (1296, 640);
}

void BeatFrogEditor::resized()
{
    content.setBounds (0, 0, 1620, 800);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / 1620.0f));
}
