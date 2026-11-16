#include "PluginEditor.h"

PlosiveRemoverSiliconEditor::PlosiveRemoverSiliconEditor(PlosiveRemoverSiliconProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 300);

    titleLabel.setText("Plosive Remover ERA 4 Silicon", juce::dontSendNotification);
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

    modeLabel.setText("Mode:", juce::dontSendNotification);
    addAndMakeVisible(modeLabel);
    modeBox.addItemList({"Normal", "Extreme"}, 1);
    addAndMakeVisible(modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "mode", modeBox);

    addAndMakeVisible(diffButton);
    diffAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "diffMode", diffButton);
}

void PlosiveRemoverSiliconEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));
    g.setColour(juce::Colour(0xff3a3a4a));
    g.fillRoundedRectangle(10.0f, 55.0f, (float)getWidth() - 20.0f, 220.0f, 6.0f);
}

void PlosiveRemoverSiliconEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(50));

    auto content = area.reduced(20, 10);
    processingLabel.setBounds(content.removeFromTop(20));
    processingSlider.setBounds(content.removeFromTop(30));
    content.removeFromTop(10);

    auto modeRow = content.removeFromTop(28);
    modeLabel.setBounds(modeRow.removeFromLeft(60));
    modeBox.setBounds(modeRow.removeFromLeft(150));
    content.removeFromTop(10);

    diffButton.setBounds(content.removeFromTop(28));
}
