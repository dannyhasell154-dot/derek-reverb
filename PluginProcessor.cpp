#include "PluginProcessor.h"
#include "PluginEditor.h"

static const int combBase[8]  = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int apBase[4]    = { 556, 441, 341, 225 };
static const int stereoSpread = 23;

juce::AudioProcessorValueTreeState::ParameterLayout DerekReverbProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.37f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay",
                 NormalisableRange<float> (0.2f, 10.0f, 0.01f, 0.5f), 2.9f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "predelay", 1 }, "Pre Delay",
                 NormalisableRange<float> (0.0f, 200.0f, 0.1f), 12.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "size", 1 }, "Size",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.76f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "damping", 1 }, "Damping",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.62f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wetdry", 1 }, "Wet / Dry",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.64f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                 NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    return { p.begin(), p.end() };
}

DerekReverbProcessor::DerekReverbProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool DerekReverbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DerekReverbProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const double k = sr / 44100.0;

    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < 8; ++i)
        {
            auto& cb = ch[(size_t) c].combs[(size_t) i];
            const double maxLen = (combBase[i] + stereoSpread) * k * 1.7 + 8.0;
            cb.buf.assign ((size_t) maxLen, 0.0f);
            cb.w = 0;
            cb.lp = 0.0f;
            cb.len.reset (sr, 0.08);
            cb.len.setCurrentAndTargetValue ((float) (combBase[i] * k));
        }
        for (int i = 0; i < 4; ++i)
        {
            auto& ap = ch[(size_t) c].aps[(size_t) i];
            ap.buf.assign ((size_t) ((apBase[i] + (c ? stereoSpread : 0)) * k) + 1, 0.0f);
            ap.w = 0;
        }
    }

    pre.assign ((size_t) (sr * 0.25) + 4, 0.0f);
    preW = 0;

    outGain.reset (sr, 0.03); wetDry.reset (sr, 0.03); sendAmt.reset (sr, 0.03);
    outGain.setCurrentAndTargetValue (1.0f);
    wetDry.setCurrentAndTargetValue (apvts.getRawParameterValue ("wetdry")->load());
    sendAmt.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 2.0f);
}

void DerekReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;

    const float mix      = apvts.getRawParameterValue ("mix")->load();
    const float decay    = apvts.getRawParameterValue ("decay")->load();
    const float preMs    = apvts.getRawParameterValue ("predelay")->load();
    const float size     = apvts.getRawParameterValue ("size")->load();
    const float damping  = apvts.getRawParameterValue ("damping")->load();
    const float width    = apvts.getRawParameterValue ("width")->load();
    const float wd       = apvts.getRawParameterValue ("wetdry")->load();
    const float outDb    = apvts.getRawParameterValue ("output")->load();

    const float scale = 0.4f + size * 1.2f;       // 0.4x .. 1.6x room length
    const float damp  = 0.05f + damping * 0.75f;
    const double k    = sr / 44100.0;

    std::array<std::array<float, 8>, 2> fb {};
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 8; ++i)
        {
            const float target = (float) ((combBase[i] + (c ? stereoSpread : 0)) * k) * scale;
            ch[(size_t) c].combs[(size_t) i].len.setTargetValue (target);
            const float seconds = target / (float) sr;
            fb[(size_t) c][(size_t) i] = juce::jmin (0.997f, std::pow (10.0f, -3.0f * seconds / decay));
        }

    sendAmt.setTargetValue (mix * 2.0f);
    wetDry.setTargetValue (wd);
    outGain.setTargetValue (juce::Decibels::decibelsToGain (outDb));

    const int preSamples = juce::jlimit (0, (int) pre.size() - 2, (int) (preMs * 0.001 * sr));
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    float pkL = 0.0f, pkR = 0.0f;

    for (int s = 0; s < n; ++s)
    {
        const float dl = L[s], dr = R[s];

        pre[(size_t) preW] = (dl + dr) * 0.5f;
        int rp = preW - preSamples;
        if (rp < 0) rp += (int) pre.size();
        const float delayed = pre[(size_t) rp];
        if (++preW >= (int) pre.size()) preW = 0;

        const float in = delayed * 0.015f * sendAmt.getNextValue();
        float wet[2];
        for (int c = 0; c < 2; ++c)
        {
            float o = 0.0f;
            for (int i = 0; i < 8; ++i)
                o += ch[(size_t) c].combs[(size_t) i].process (in, fb[(size_t) c][(size_t) i], damp);
            for (int i = 0; i < 4; ++i)
                o = ch[(size_t) c].aps[(size_t) i].process (o);
            wet[c] = o * 3.0f;
        }

        const float mid  = (wet[0] + wet[1]) * 0.5f;
        const float side = (wet[0] - wet[1]) * 0.5f * width;
        const float wl = mid + side, wr = mid - side;

        const float w = wetDry.getNextValue();
        const float g = outGain.getNextValue();
        L[s] = (dl * (1.0f - w) + wl * w) * g;
        R[s] = (dr * (1.0f - w) + wr * w) * g;

        pkL = juce::jmax (pkL, std::abs (L[s]));
        pkR = juce::jmax (pkR, std::abs (R[s]));
    }

    peakL = juce::jmax (peakL.load(), pkL);
    peakR = juce::jmax (peakR.load(), pkR);
}

juce::AudioProcessorEditor* DerekReverbProcessor::createEditor() { return new DerekReverbEditor (*this); }

void DerekReverbProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void DerekReverbProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DerekReverbProcessor(); }
