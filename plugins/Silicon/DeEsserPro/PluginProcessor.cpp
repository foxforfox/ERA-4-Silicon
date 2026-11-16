#include "PluginProcessor.h"
#include "PluginEditor.h"

DeEsserProSiliconProcessor::DeEsserProSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    deesser_pro_init(&dspState);
}

DeEsserProSiliconProcessor::~DeEsserProSiliconProcessor()
{
    if (dspPrepared)
        deesser_pro_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout DeEsserProSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"focus", 1}, "Focus",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"shaping", 1}, "Shaping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"diffMode", 1}, "Diff Mode", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freqListen", 1}, "Freq Listen", false));

    return {params.begin(), params.end()};
}

void DeEsserProSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        deesser_pro_free(&dspState);

    deesser_pro_init(&dspState);
    deesser_pro_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(deesser_pro_get_latency(&dspState));
}

void DeEsserProSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        deesser_pro_free(&dspState);
        deesser_pro_init(&dspState);
        dspPrepared = false;
    }
}

void DeEsserProSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto focus = apvts.getRawParameterValue("focus")->load();
    auto shaping = apvts.getRawParameterValue("shaping")->load();
    auto outGain = apvts.getRawParameterValue("outputGain")->load();
    auto diff = apvts.getRawParameterValue("diffMode")->load() > 0.5f;
    auto freqListen = apvts.getRawParameterValue("freqListen")->load() > 0.5f;

    deesser_pro_set_processing(&dspState, amount);
    deesser_pro_set_focus(&dspState, focus);
    deesser_pro_set_shaping(&dspState, shaping);
    deesser_pro_set_output_gain(&dspState, outGain);
    deesser_pro_set_diff_mode(&dspState, diff);
    deesser_pro_set_freq_listen(&dspState, freqListen);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    deesser_pro_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* DeEsserProSiliconProcessor::createEditor()
{
    return new DeEsserProSiliconEditor(*this);
}

void DeEsserProSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DeEsserProSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeEsserProSiliconProcessor();
}
