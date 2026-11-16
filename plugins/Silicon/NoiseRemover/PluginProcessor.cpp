#include "PluginProcessor.h"
#include "PluginEditor.h"

NoiseRemoverSiliconProcessor::NoiseRemoverSiliconProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    noise_remover_init(&dspState);
}

NoiseRemoverSiliconProcessor::~NoiseRemoverSiliconProcessor()
{
    if (dspPrepared)
        noise_remover_free(&dspState);
}

juce::AudioProcessorValueTreeState::ParameterLayout NoiseRemoverSiliconProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"processing", 1}, "Processing",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"frequencyFocus", 1}, "Frequency Focus",
        juce::StringArray{"All", "Hi", "Low", "HiLow", "Mid", "HumBuzz"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    return {params.begin(), params.end()};
}

void NoiseRemoverSiliconProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (dspPrepared)
        noise_remover_free(&dspState);

    noise_remover_init(&dspState);
    noise_remover_prepare(&dspState, sampleRate, samplesPerBlock, getTotalNumInputChannels());
    dspPrepared = true;

    setLatencySamples(noise_remover_get_latency(&dspState));
}

void NoiseRemoverSiliconProcessor::releaseResources()
{
    if (dspPrepared)
    {
        noise_remover_free(&dspState);
        noise_remover_init(&dspState);
        dspPrepared = false;
    }
}

void NoiseRemoverSiliconProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!dspPrepared)
        return;

    auto amount = apvts.getRawParameterValue("processing")->load();
    auto focus = static_cast<int>(apvts.getRawParameterValue("frequencyFocus")->load());
    auto outGain = apvts.getRawParameterValue("outputGain")->load();

    noise_remover_set_processing(&dspState, amount);
    noise_remover_set_frequency_focus(&dspState, static_cast<NoiseFrequencyFocus>(focus));
    noise_remover_set_output_gain(&dspState, outGain);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    float* channelPtrs[2] = {nullptr, nullptr};
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        channelPtrs[ch] = buffer.getWritePointer(ch);

    noise_remover_process(&dspState, channelPtrs, numChannels, numSamples);
}

juce::AudioProcessorEditor* NoiseRemoverSiliconProcessor::createEditor()
{
    return new NoiseRemoverSiliconEditor(*this);
}

void NoiseRemoverSiliconProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NoiseRemoverSiliconProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseRemoverSiliconProcessor();
}
