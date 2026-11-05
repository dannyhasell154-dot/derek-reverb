#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class DerekReverbProcessor : public juce::AudioProcessor
{
public:
    DerekReverbProcessor();
    ~DerekReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Derek Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Comb
    {
        std::vector<float> buf;
        int w = 0;
        float lp = 0.0f;
        juce::SmoothedValue<float> len;

        float process (float in, float fb, float damp)
        {
            const int size = (int) buf.size();
            float rp = (float) w - len.getNextValue();
            while (rp < 0.0f) rp += (float) size;
            int i0 = (int) rp % size;
            const float fr = rp - std::floor (rp);
            const int i1 = (i0 + 1) % size;
            const float o = buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
            lp = o * (1.0f - damp) + lp * damp;
            buf[(size_t) w] = in + lp * fb;
            if (++w >= size) w = 0;
            return o;
        }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int w = 0;
        float process (float in)
        {
            const float bo = buf[(size_t) w];
            const float out = -in + bo;
            buf[(size_t) w] = in + bo * 0.5f;
            if (++w >= (int) buf.size()) w = 0;
            return out;
        }
    };

    struct Channel
    {
        std::array<Comb, 8> combs;
        std::array<Allpass, 4> aps;
    };

    std::array<Channel, 2> ch;
    std::vector<float> pre;
    int preW = 0;
    double sr = 44100.0;
    juce::SmoothedValue<float> outGain, wetDry, sendAmt;
};
