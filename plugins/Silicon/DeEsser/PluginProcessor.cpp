#include "PluginProcessor.h"
#include "PluginEditor.h"

DeEsserSiliconProcessor::DeEsserSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    deesser_init(&dspState);
}

DeEsserSiliconProcessor::~DeEsserSiliconProcessor()
{
    if (dspPrepared)
        deesser_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout DeEsserSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Narrow", "Normal", "Broad"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"diffMode", 1}, "Diff Mode", false));

    return {params.begin(), params.end()};
}

void DeEsserSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        deesser_free(&dspState);

    deesser_init(&dspState);
    deesser_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(deesser_get_latency(&dspState));
}

void DeEsserSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        deesser_free(&dspState);
        deesser_init(&dspState);
        dspPrepared = false;
    }
}

void DeEsserSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    auto diff = apvts.getRawParameterValue("diffMode")->load() > 0.5f;

    deesser_set_processing(&dspState, amount);
    deesser_set_mode(&dspState, static_cast<DeEsserMode>(mode));
    deesser_set_diff_mode(&dspState, diff);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    deesser_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* DeEsserSiliconProcessor::createEditor()
{
    return new DeEsserSiliconEditor(*this);
}

void DeEsserSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DeEsserSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeEsserSiliconProcessor();
}
