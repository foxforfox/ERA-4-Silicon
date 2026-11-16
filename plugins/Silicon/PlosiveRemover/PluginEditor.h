#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PlosiveRemoverSiliconEditor : public juce::AudioProcessorEditor
{
public:
    explicit PlosiveRemoverSiliconEditor(PlosiveRemoverSiliconProcessor&);
    ~PlosiveRemoverSiliconEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PlosiveRemoverSiliconProcessor& processor;

    juce::Label titleLabel;
    juce::Slider processingSlider;
    juce::Label processingLabel;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::ToggleButton diffButton{"Diff Mode"};

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> processingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diffAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlosiveRemoverSiliconEditor)
};
