#include "PluginEditor.h"

NoiseRemoverSiliconEditor::NoiseRemoverSiliconEditor(NoiseRemoverSiliconProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 340);

    titleLabel.setText("Noise Remover ERA 4 Silicon", juce::dontSendNotification);
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

    focusLabel.setText("Frequency Focus:", juce::dontSendNotification);
    addAndMakeVisible(focusLabel);
    focusBox.addItemList({"All", "Hi", "Low", "HiLow", "Mid", "HumBuzz"}, 1);
    addAndMakeVisible(focusBox);
    focusAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "frequencyFocus", focusBox);

    outputGainLabel.setText("Output Gain", juce::dontSendNotification);
    addAndMakeVisible(outputGainLabel);
    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outputGainSlider);
}

void NoiseRemoverSiliconEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));
    g.setColour(juce::Colour(0xff3a3a4a));
    g.fillRoundedRectangle(10.0f, 55.0f, (float)getWidth() - 20.0f, 260.0f, 6.0f);
}

void NoiseRemoverSiliconEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(50));

    auto content = area.reduced(20, 10);
    processingLabel.setBounds(content.removeFromTop(20));
    processingSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(10);

    auto focusRow = content.removeFromTop(28);
    focusLabel.setBounds(focusRow.removeFromLeft(120));
    focusBox.setBounds(focusRow.removeFromLeft(150));
    content.removeFromTop(10);

    outputGainLabel.setBounds(content.removeFromTop(20));
    outputGainSlider.setBounds(content.removeFromTop(30));
}
