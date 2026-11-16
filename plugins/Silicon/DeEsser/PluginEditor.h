#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class DeEsserSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeEsserSiliconEditor(DeEsserSiliconProcessor&);
    ~DeEsserSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DeEsserSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::ToggleButton diffButton{"Diff Mode"};

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diffAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserSiliconEditor)
};
