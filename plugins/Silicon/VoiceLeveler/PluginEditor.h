#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class VoiceLevelerSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoiceLevelerSiliconEditor(VoiceLevelerSiliconProcessor&);
    ~VoiceLevelerSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VoiceLevelerSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::Slider breathSlider;
    juce::Label breathLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> breathAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceLevelerSiliconEditor)
};
