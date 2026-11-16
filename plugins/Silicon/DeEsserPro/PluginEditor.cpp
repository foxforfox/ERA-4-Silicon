#include "PluginEditor.h"

DeEsserProSiliconEditor::DeEsserProSiliconEditor(DeEsserProSiliconProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 420);

    titleLabel.setText("De-Esser Pro ERA 4 Silicon", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    processingLabel.setText("Processing", juce::dontSendNotification);
    addAndMakeVisible(processingLabel);
    processingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    processingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(processingSlider);
    processingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "processing", processingSlider);

    focusLabel.setText("Focus", juce::dontSendNotification);
    addAndMakeVisible(focusLabel);
    focusSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    focusSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(focusSlider);
    focusAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "focus", focusSlider);

    shapingLabel.setText("Shaping", juce::dontSendNotification);
    addAndMakeVisible(shapingLabel);
    shapingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    shapingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(shapingSlider);
    shapingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "shaping", shapingSlider);

    outputGainLabel.setText("Output Gain", juce::dontSendNotification);
    addAndMakeVisible(outputGainLabel);
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outputGainSlider);

    addAndMakeVisible(diffButton);
    diffAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "diffMode", diffButton);

    addAndMakeVisible(freqListenButton);
    freqListenAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "freqListen", freqListenButton);
}

void DeEsserProSiliconEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));
    g.setColour(juce::Colour(0xff3a3a4a));
    g.fillRoundedRectangle(10.0f, 55.0f, (float)getWidth() - 20.0f, 340.0f, 6.0f);
}

void DeEsserProSiliconEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(50));

    auto content = area.reduced(20, 10);

    processingLabel.setBounds(content.removeFromTop(20));
    processingSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(5);

    focusLabel.setBounds(content.removeFromTop(20));
    focusSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(5);

    shapingLabel.setBounds(content.removeFromTop(20));
    shapingSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(5);

    outputGainLabel.setBounds(content.removeFromTop(20));
    outputGainSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(10);

    auto buttonRow = content.removeFromTop(28);
    diffButton.setBounds(buttonRow.removeFromLeft(150));
    freqListenButton.setBounds(buttonRow.removeFromLeft(150));
}
