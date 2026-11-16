#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class DeEsserProSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeEsserProSiliconEditor(DeEsserProSiliconProcessor&);
    ~DeEsserProSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DeEsserProSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::Slider focusSlider;
    juce::Label focusLabel;
    juce::Slider shapingSlider;
    juce::Label shapingLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::ToggleButton diffButton{"Diff Mode"};
    juce::ToggleButton freqListenButton{"Freq Listen"};

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> focusAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shapingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diffAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freqListenAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserProSiliconEditor)
};
