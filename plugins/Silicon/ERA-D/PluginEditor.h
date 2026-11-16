#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class EraDSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit EraDSiliconEditor(EraDSiliconProcessor&);
    ~EraDSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EraDSiliconProcessor& processor;

    juce::Label titleLabel;

    juce::Label noiseLabel;
    juce::Slider noiseSlider;
    juce::ComboBox noiseFocusBox;
    juce::Label noiseFocusLabel;

    juce::Label reverbLabel;
    juce::Slider reverbSlider;
    juce::ComboBox reverbFocusBox;
    juce::Label reverbFocusLabel;

    juce::Label outputGainLabel;
    juce::Slider outputGainSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseFocusAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverbFocusAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EraDSiliconEditor)
};
