#include "PluginProcessor.h"
#include "PluginEditor.h"

VoiceLevelerSiliconProcessor::VoiceLevelerSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    voice_leveler_init(&dspState);
}

VoiceLevelerSiliconProcessor::~VoiceLevelerSiliconProcessor()
{
    if (dspPrepared)
        voice_leveler_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoiceLevelerSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mode", 1}, "Mode",
        juce::StringArray{"Normal", "Tight"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"breathControl", 1}, "Breath Control",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

void VoiceLevelerSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        voice_leveler_free(&dspState);

    voice_leveler_init(&dspState);
    voice_leveler_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(voice_leveler_get_latency(&dspState));
}

void VoiceLevelerSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        voice_leveler_free(&dspState);
        voice_leveler_init(&dspState);
        dspPrepared = false;
    }
}

void VoiceLevelerSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    auto breath = apvts.getRawParameterValue("breathControl")->load();
    auto outGain = apvts.getRawParameterValue("outputGain")->load();

    voice_leveler_set_processing(&dspState, amount);
    voice_leveler_set_mode(&dspState, static_cast<VoiceLevelerMode>(mode));
    voice_leveler_set_breath_control(&dspState, breath);
    voice_leveler_set_output_gain(&dspState, outGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    voice_leveler_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* VoiceLevelerSiliconProcessor::createEditor()
{
    return new VoiceLevelerSiliconEditor(*this);
}

void VoiceLevelerSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VoiceLevelerSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoiceLevelerSiliconProcessor();
}
