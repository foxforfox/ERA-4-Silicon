#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class DeClipperSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeClipperSiliconEditor(DeClipperSiliconProcessor&);
    ~DeClipperSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DeClipperSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::ComboBox typeBox;
    juce::Label typeLabel;
    juce::ComboBox qualityBox;
    juce::Label qualityLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeClipperSiliconEditor)
};
