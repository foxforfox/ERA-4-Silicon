#include "PluginProcessor.h"
#include "PluginEditor.h"

EraDSiliconProcessor::EraDSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    era_d_init(&dspState);
}

EraDSiliconProcessor::~EraDSiliconProcessor()
{
    if (dspPrepared)
        era_d_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout EraDSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"noiseProcessing", 1}, "Noise Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"noiseFrequencyFocus", 1}, "Noise Frequency Focus",
        juce::StringArray{"All", "Hi", "Low", "HiLow", "Mid", "HumBuzz"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"reverbProcessing", 1}, "Reverb Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"reverbFrequencyFocus", 1}, "Reverb Frequency Focus",
        juce::StringArray{"All", "Hi", "Low", "HiLow", "Mid"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

void EraDSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        era_d_free(&dspState);

    era_d_init(&dspState);
    era_d_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(era_d_get_latency(&dspState));
}

void EraDSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        era_d_free(&dspState);
        era_d_init(&dspState);
        dspPrepared = false;
    }
}

void EraDSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto noiseAmt = apvts.getRawParameterValue("noiseProcessing")->load();
    auto noiseFocus = static_cast<int>(apvts.getRawParameterValue("noiseFrequencyFocus")->load());
    auto reverbAmt = apvts.getRawParameterValue("reverbProcessing")->load();
    auto reverbFocus = static_cast<int>(apvts.getRawParameterValue("reverbFrequencyFocus")->load());
    auto outGain = apvts.getRawParameterValue("outputGain")->load();

    era_d_set_noise_processing(&dspState, noiseAmt);
    era_d_set_noise_frequency_focus(&dspState, static_cast<NoiseFrequencyFocus>(noiseFocus));
    era_d_set_reverb_processing(&dspState, reverbAmt);
    era_d_set_reverb_frequency_focus(&dspState, static_cast<ReverbFrequencyFocus>(reverbFocus));
    era_d_set_output_gain(&dspState, outGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    era_d_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* EraDSiliconProcessor::createEditor()
{
    return new EraDSiliconEditor(*this);
}

void EraDSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EraDSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EraDSiliconProcessor();
}
