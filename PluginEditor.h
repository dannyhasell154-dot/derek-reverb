#pragma once
#include "PluginProcessor.h"
#include <array>

class DerekLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class DerekReverbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DerekReverbEditor (DerekReverbProcessor&);
    ~DerekReverbEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override;
    void applyPreset (int index);
    void setParam (const juce::String& id, float plainValue);
    juce::String formatValue (int knob, double v) const;
    void drawDecayGraph (juce::Graphics&, juce::Rectangle<float>);
    void drawMeter (juce::Graphics&, juce::Rectangle<float>);

    DerekReverbProcessor& proc;
    DerekLookAndFeel laf;
    juce::Image bg;

    static constexpr int numKnobs = 7;
    std::array<juce::Slider, numKnobs> sliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, numKnobs> attachments;

    juce::Slider outputSlider; // hidden-style small knob under meter
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttach;

    juce::ComboBox presetBox;
    juce::TextButton resetButton { "Reset" };
    float levelL = 0.0f, levelR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DerekReverbEditor)
};
