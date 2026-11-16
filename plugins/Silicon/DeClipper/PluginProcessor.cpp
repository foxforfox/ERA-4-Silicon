#include "PluginProcessor.h"
#include "PluginEditor.h"

DeClipperSiliconProcessor::DeClipperSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    declipper_init(&dspState);
}

DeClipperSiliconProcessor::~DeClipperSiliconProcessor()
{
    if (dspPrepared)
        declipper_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout DeClipperSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"type", 1}, "Type",
        juce::StringArray{"Standard", "Heavy"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"quality", 1}, "Quality",
        juce::StringArray{"Standard", "High"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

void DeClipperSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        declipper_free(&dspState);

    declipper_init(&dspState);
    declipper_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(declipper_get_latency(&dspState));
}

void DeClipperSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        declipper_free(&dspState);
        declipper_init(&dspState);
        dspPrepared = false;
    }
}

void DeClipperSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto type = static_cast<int>(apvts.getRawParameterValue("type")->load());
    auto quality = static_cast<int>(apvts.getRawParameterValue("quality")->load());
    auto outGain = apvts.getRawParameterValue("outputGain")->load();

    declipper_set_processing(&dspState, amount);
    declipper_set_type(&dspState, static_cast<DeClipperType>(type));
    declipper_set_quality(&dspState, static_cast<DeClipperQuality>(quality));
    declipper_set_output_gain(&dspState, outGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    declipper_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* DeClipperSiliconProcessor::createEditor()
{
    return new DeClipperSiliconEditor(*this);
}

void DeClipperSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DeClipperSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeClipperSiliconProcessor();
}
