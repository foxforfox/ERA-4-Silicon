#include "PluginProcessor.h"
#include "PluginEditor.h"

ReverbRemoverSiliconProcessor::ReverbRemoverSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    reverb_remover_init(&dspState);
}

ReverbRemoverSiliconProcessor::~ReverbRemoverSiliconProcessor()
{
    if (dspPrepared)
        reverb_remover_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReverbRemoverSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"frequencyFocus", 1}, "Frequency Focus",
        juce::StringArray{"All", "Hi", "Low", "HiLow", "Mid"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"autoGain", 1}, "Auto Gain", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

void ReverbRemoverSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        reverb_remover_free(&dspState);

    reverb_remover_init(&dspState);
    reverb_remover_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(reverb_remover_get_latency(&dspState));
}

void ReverbRemoverSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        reverb_remover_free(&dspState);
        reverb_remover_init(&dspState);
        dspPrepared = false;
    }
}

void ReverbRemoverSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto focus = static_cast<int>(apvts.getRawParameterValue("frequencyFocus")->load());
    auto autoGain = apvts.getRawParameterValue("autoGain")->load() > 0.5f;
    auto outGain = apvts.getRawParameterValue("outputGain")->load();

    reverb_remover_set_processing(&dspState, amount);
    reverb_remover_set_frequency_focus(&dspState, static_cast<ReverbFrequencyFocus>(focus));
    reverb_remover_set_auto_gain(&dspState, autoGain);
    reverb_remover_set_output_gain(&dspState, outGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    reverb_remover_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* ReverbRemoverSiliconProcessor::createEditor()
{
    return new ReverbRemoverSiliconEditor(*this);
}

void ReverbRemoverSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ReverbRemoverSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbRemoverSiliconProcessor();
}
