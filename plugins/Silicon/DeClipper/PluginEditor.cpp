#include "PluginEditor.h"

DeClipperSiliconEditor::DeClipperSiliconEditor(DeClipperSiliconProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 360);

    titleLabel.setText("De-Clipper ERA 4 Silicon", juce::dontSendNotification);
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

    typeLabel.setText("Type:", juce::dontSendNotification);
    addAndMakeVisible(typeLabel);
    typeBox.addItemList({"Standard", "Heavy"}, 1);
    addAndMakeVisible(typeBox);
    typeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "type", typeBox);

    qualityLabel.setText("Quality:", juce::dontSendNotification);
    addAndMakeVisible(qualityLabel);
    qualityBox.addItemList({"Standard", "High"}, 1);
    addAndMakeVisible(qualityBox);
    qualityAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "quality", qualityBox);

    outputGainLabel.setText("Output Gain", juce::dontSendNotification);
    addAndMakeVisible(outputGainLabel);
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outputGainSlider);
}

void DeClipperSiliconEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));
    g.setColour(juce::Colour(0xff3a3a4a));
    g.fillRoundedRectangle(10.0f, 55.0f, (float)getWidth() - 20.0f, 280.0f, 6.0f);
}

void DeClipperSiliconEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(50));

    auto content = area.reduced(20, 10);
    processingLabel.setBounds(content.removeFromTop(20));
    processingSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(10);

    auto typeRow = content.removeFromTop(28);
    typeLabel.setBounds(typeRow.removeFromLeft(70));
    typeBox.setBounds(typeRow.removeFromLeft(150));
    content.removeFromTop(8);

    auto qualityRow = content.removeFromTop(28);
    qualityLabel.setBounds(qualityRow.removeFromLeft(70));
    qualityBox.setBounds(qualityRow.removeFromLeft(150));
    content.removeFromTop(10);

    outputGainLabel.setBounds(content.removeFromTop(20));
    outputGainSlider.setBounds(content.removeFromTop(30));
}
