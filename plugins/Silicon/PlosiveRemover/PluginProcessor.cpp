#include "PluginProcessor.h"
#include "PluginEditor.h"

PlosiveRemoverSiliconProcessor::PlosiveRemoverSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    plosive_init(&dspState);
}

PlosiveRemoverSiliconProcessor::~PlosiveRemoverSiliconProcessor()
{
    if (dspPrepared)
        plosive_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout PlosiveRemoverSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Normal", "Extreme"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"diffMode", 1}, "Diff Mode", false));

    return {params.begin(), params.end()};
}

void PlosiveRemoverSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        plosive_free(&dspState);

    plosive_init(&dspState);
    plosive_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(plosive_get_latency(&dspState));
}

void PlosiveRemoverSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        plosive_free(&dspState);
        plosive_init(&dspState);
        dspPrepared = false;
    }
}

void PlosiveRemoverSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    auto diff = apvts.getRawParameterValue("diffMode")->load() > 0.5f;

    plosive_set_processing(&dspState, amount);
    plosive_set_mode(&dspState, static_cast<PlosiveMode>(mode));
    plosive_set_diff_mode(&dspState, diff);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    plosive_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* PlosiveRemoverSiliconProcessor::createEditor()
{
    return new PlosiveRemoverSiliconEditor(*this);
}

void PlosiveRemoverSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PlosiveRemoverSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlosiveRemoverSiliconProcessor();
}
