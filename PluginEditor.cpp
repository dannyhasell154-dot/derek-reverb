#include "PluginEditor.h"
#if HAS_BG
 #include <BinaryData.h>
#endif

using namespace juce;

// ---- layout (768 x 512 window; coordinates are knob centres) ----
static const char* ids[7]    = { "mix", "decay", "predelay", "size", "damping", "width", "wetdry" };
static const char* names[7]  = { "Mix", "Decay", "Pre Delay", "Size", "Damping", "Width", "Wet / Dry" };
static const Point<float> centres[7] = { { 76, 243 }, { 178, 243 }, { 275, 243 },
                                          { 530, 243 }, { 623, 243 }, { 710, 243 }, { 604, 395 } };
static const int diam[7] = { 82, 72, 72, 76, 72, 74, 104 };

static const Colour purple (0xffa05cd6);

void DerekLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // arc track + value arc
    const float ar = r + 5.0f;
    Path track;  track.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff2a2a30));
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val;    val.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, angle, true);
    g.setColour (purple);
    g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body
    g.setColour (Colour (0xff050507));
    g.fillEllipse (c.x - r - 2, c.y - r - 2, 2 * r + 4, 2 * r + 4);
    g.setGradientFill (ColourGradient (Colour (0xff3b3b42), c.x - r * 0.5f, c.y - r * 0.7f,
                                       Colour (0xff0d0d10), c.x + r * 0.6f, c.y + r * 0.9f, true));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (Colour (0x30ffffff));
    g.drawEllipse (c.x - r + 1, c.y - r + 1, 2 * r - 2, 2 * r - 2, 1.0f);

    // pointer
    const auto p1 = c.getPointOnCircumference (r * 0.45f, angle);
    const auto p2 = c.getPointOnCircumference (r * 0.88f, angle);
    g.setColour (Colour (0xffe9e4f2));
    g.drawLine (Line<float> (p1, p2), 2.4f);
}

DerekReverbEditor::DerekReverbEditor (DerekReverbProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (768, 512);
    setLookAndFeel (&laf);

   #if HAS_BG
    bg = ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
   #endif

    for (int i = 0; i < numKnobs; ++i)
    {
        auto& s = sliders[(size_t) i];
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
        s.setBounds (Rectangle<int> (diam[i] + 16, diam[i] + 16).withCentre (centres[i].toInt()));
        s.setDoubleClickReturnValue (true, 0.0); // overwritten below via attachment default
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
        attachments[(size_t) i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], s);
        if (auto* prm = proc.apvts.getParameter (ids[i]))
            s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    // output knob (small, under the meters)
    outputSlider.setSliderStyle (Slider::LinearVertical);
    outputSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    outputSlider.setBounds (698, 338, 40, 96);
    outputSlider.setAlpha (0.0f); // invisible drag area over the meter: drag to set output
    outputSlider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (outputSlider);
    outputAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "output", outputSlider);
    outputSlider.setDoubleClickReturnValue (true, 0.0);

    presetBox.addItemList ({ "Derek Hall", "Small Room", "Plate", "Cathedral", "Vocal Space" }, 1);
    presetBox.setSelectedId (1, dontSendNotification);
    presetBox.setBounds (499, 54, 165, 32);
    presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
    presetBox.setColour (ComboBox::backgroundColourId, Colour (0xff111114));
    presetBox.setColour (ComboBox::outlineColourId, Colour (0xff3a3a42));
    presetBox.setColour (ComboBox::textColourId, Colours::white);
    addAndMakeVisible (presetBox);

    resetButton.setBounds (676, 54, 56, 32);
    resetButton.onClick = [this] { applyPreset (0); presetBox.setSelectedId (1, dontSendNotification); };
    resetButton.setColour (TextButton::buttonColourId, Colour (0xff111114));
    addAndMakeVisible (resetButton);

    startTimerHz (30);
}

DerekReverbEditor::~DerekReverbEditor() { setLookAndFeel (nullptr); }

void DerekReverbEditor::setParam (const String& id, float v)
{
    if (auto* prm = proc.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void DerekReverbEditor::applyPreset (int index)
{
    //                      mix  decay pre   size  damp  width wetdry
    static const float presets[5][7] = {
        { 0.37f, 2.9f, 12.0f, 0.76f, 0.62f, 1.00f, 0.64f },   // Derek Hall
        { 0.30f, 0.8f,  4.0f, 0.30f, 0.50f, 0.80f, 0.35f },   // Small Room
        { 0.40f, 1.8f,  0.0f, 0.55f, 0.35f, 1.00f, 0.40f },   // Plate
        { 0.50f, 6.5f, 30.0f, 1.00f, 0.70f, 1.00f, 0.50f },   // Cathedral
        { 0.30f, 1.6f, 20.0f, 0.60f, 0.50f, 0.90f, 0.30f }    // Vocal Space
    };
    index = jlimit (0, 4, index);
    for (int i = 0; i < numKnobs; ++i)
        setParam (ids[i], presets[index][i]);
}

String DerekReverbEditor::formatValue (int k, double v) const
{
    if (k == 1) return String (v, 1) + "s";
    if (k == 2) return String (roundToInt (v)) + "ms";
    return String (roundToInt (v * 100.0)) + "%";
}

void DerekReverbEditor::timerCallback()
{
    const float pl = proc.peakL.exchange (0.0f);
    const float pr = proc.peakR.exchange (0.0f);
    levelL = jmax (pl, levelL * 0.88f);
    levelR = jmax (pr, levelR * 0.88f);
    repaint (700, 330, 70, 120);
}

void DerekReverbEditor::drawDecayGraph (Graphics& g, Rectangle<float> r)
{
    g.setColour (Colour (0xff060608));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colour (0xff2e2e36));
    g.drawRoundedRectangle (r, 4.0f, 2.0f);

    const float decay = (float) sliders[1].getValue();
    const float size  = (float) sliders[3].getValue();
    const float damp  = (float) sliders[4].getValue();
    auto inner = r.reduced (8.0f);
    Random rng (1234);

    const float tMax = jmax (decay * 1.35f, 0.5f);
    for (float x = 0; x < inner.getWidth(); x += 1.5f)
    {
        const float t = x / inner.getWidth() * tMax;
        float env = std::exp (-6.9f * t / decay);
        env *= (0.55f + 0.45f * rng.nextFloat());
        env *= (0.8f + 0.2f * std::sin (t * (30.0f + 60.0f * size)));
        const float hf = 1.0f - damp * 0.35f;
        const float h = inner.getHeight() * jlimit (0.0f, 1.0f, env * hf);
        g.setColour (purple.withAlpha (0.35f + 0.65f * env));
        g.fillRect (inner.getX() + x, inner.getBottom() - h, 1.2f, h);
    }
}

void DerekReverbEditor::drawMeter (Graphics& g, Rectangle<float> r)
{
    const int segs = 20;
    const float segH = r.getHeight() / (float) segs;
    auto drawBar = [&] (float x, float level)
    {
        const float db = Decibels::gainToDecibels (level, -60.0f);
        const float norm = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        for (int i = 0; i < segs; ++i)
        {
            const float frac = (float) i / (float) segs;
            const bool lit = norm > frac;
            Colour c = frac > 0.85f ? Colour (0xffe04040) : frac > 0.6f ? Colour (0xffd8c63a) : Colour (0xff5fd23c);
            g.setColour (lit ? c : Colour (0xff1b1b1f));
            g.fillRect (x, r.getBottom() - (float) (i + 1) * segH + 1.0f, 12.0f, segH - 2.0f);
        }
    };
    drawBar (r.getX(), levelL);
    drawBar (r.getX() + 16.0f, levelR);
}

void DerekReverbEditor::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // background
    if (bg.isValid())
    {
        g.drawImage (bg, bounds, RectanglePlacement::fillDestination | RectanglePlacement::centred);
        g.fillAll (Colour (0x99000000));
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff17171b), 0, 0, Colour (0xff060607), 0, bounds.getHeight(), false));
        g.fillAll();
        Random rng (42);
        for (int i = 0; i < 2500; ++i)
        {
            g.setColour (Colour::fromFloatRGBA (1, 1, 1, rng.nextFloat() * 0.06f));
            g.fillRect (rng.nextFloat() * bounds.getWidth(), rng.nextFloat() * bounds.getHeight(), 1.0f, 1.0f);
        }
    }

    // title
    g.setColour (Colour (0xffc9c4d2));
    g.setFont (FontOptions (64.0f, Font::bold | Font::italic));
    g.drawText ("DEREK", 78, 40, 300, 66, Justification::left);
    g.setFont (FontOptions (50.0f, Font::bold | Font::italic));
    g.drawText ("REVERB", 150, 92, 260, 54, Justification::left);
    g.setColour (Colour (0xff8d8896));
    g.setFont (FontOptions (20.0f, Font::italic));
    g.drawText ("by nobullnobullnobull", 128, 160, 260, 26, Justification::left);
    g.drawText ("reverb so good...", 560, 100, 200, 28, Justification::left);

    // stars
    g.setColour (Colour (0xff9a95a4));
    for (auto s : { Point<float> (46, 90), Point<float> (720, 152) })
    {
        Path star;
        star.addStar (s, 5, 10.0f, 26.0f, 0.0f);
        g.strokePath (star, PathStrokeType (2.0f));
    }

    // knob labels
    g.setFont (FontOptions (19.0f, Font::italic));
    for (int i = 0; i < numKnobs; ++i)
    {
        const int labelY = (int) centres[i].y + diam[i] / 2 + 6;
        g.setColour (Colour (0xffcfcad8));
        g.drawText (names[i], Rectangle<int> (110, 20).withCentre ({ (int) centres[i].x, labelY + 10 }), Justification::centred);
        g.setColour (Colour (0xffa9a4b4));
        g.drawText (formatValue (i, sliders[(size_t) i].getValue()),
                    Rectangle<int> (110, 20).withCentre ({ (int) centres[i].x, labelY + 31 }), Justification::centred);
    }

    drawDecayGraph (g, { 35, 337, 290, 102 });
    g.setColour (Colour (0xff8d8896));
    g.setFont (FontOptions (20.0f, Font::italic));
    g.drawText ("Derek Reverb v1.0", 36, 452, 220, 26, Justification::left);

    // output meter + label
    drawMeter (g, { 702.0f, 342.0f, 28.0f, 86.0f });
    g.setColour (Colour (0xffcfcad8));
    g.drawText ("Output", Rectangle<int> (90, 20).withCentre ({ 710, 455 }), Justification::centred);
    g.setColour (Colour (0xffa9a4b4));
    g.drawText (String (outputSlider.getValue(), 1) + " dB", Rectangle<int> (90, 20).withCentre ({ 710, 476 }), Justification::centred);
}
