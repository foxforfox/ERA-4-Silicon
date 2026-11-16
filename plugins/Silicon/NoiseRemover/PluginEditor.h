#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NoiseRemoverSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit NoiseRemoverSiliconEditor(NoiseRemoverSiliconProcessor&);
    ~NoiseRemoverSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    NoiseRemoverSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::ComboBox focusBox;
    juce::Label focusLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> focusAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseRemoverSiliconEditor)
};
