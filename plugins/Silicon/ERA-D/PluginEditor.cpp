#include "PluginEditor.h"

EraDSiliconEditor::EraDSiliconEditor(EraDSiliconProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(500, 450);

    titleLabel.setText("ERA-D ERA 4 Silicon", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // --- Noise section ---
    noiseLabel.setText("Noise Reduction", juce::dontSendNotification);
    noiseLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(noiseLabel);

    noiseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    noiseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(noiseSlider);
    noiseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "noiseProcessing", noiseSlider);

    noiseFocusLabel.setText("Frequency Focus:", juce::dontSendNotification);
    addAndMakeVisible(noiseFocusLabel);
    noiseFocusBox.addItemList({"All", "Hi", "Low", "HiLow", "Mid", "HumBuzz"}, 1);
    addAndMakeVisible(noiseFocusBox);
    noiseFocusAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "noiseFrequencyFocus", noiseFocusBox);

    // --- Reverb section ---
    reverbLabel.setText("Reverb Reduction", juce::dontSendNotification);
    reverbLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(reverbLabel);

    reverbSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    reverbSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(reverbSlider);
    reverbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "reverbProcessing", reverbSlider);

    reverbFocusLabel.setText("Frequency Focus:", juce::dontSendNotification);
    addAndMakeVisible(reverbFocusLabel);
    reverbFocusBox.addItemList({"All", "Hi", "Low", "HiLow", "Mid"}, 1);
    addAndMakeVisible(reverbFocusBox);
    reverbFocusAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.getAPVTS(), "reverbFrequencyFocus", reverbFocusBox);

    // --- Output gain ---
    outputGainLabel.setText("Output Gain", juce::dontSendNotification);
    outputGainLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(outputGainLabel);

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outputGainSlider);
}

void EraDSiliconEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e2e));
    g.setColour(juce::Colour(0xff3a3a4a));

    // Noise section background
    g.fillRoundedRectangle(10.0f, 50.0f, (float)getWidth() - 20.0f, 120.0f, 6.0f);
    // Reverb section background
    g.fillRoundedRectangle(10.0f, 185.0f, (float)getWidth() - 20.0f, 120.0f, 6.0f);
    // Output section background
    g.fillRoundedRectangle(10.0f, 320.0f, (float)getWidth() - 20.0f, 80.0f, 6.0f);
}

void EraDSiliconEditor::resized()
{
    auto area = getLocalBounds();

    titleLabel.setBounds(area.removeFromTop(45));

    // Noise section
    auto noiseArea = area.removeFromTop(130).reduced(20, 5);
    noiseLabel.setBounds(noiseArea.removeFromTop(25));
    noiseSlider.setBounds(noiseArea.removeFromTop(30));
    auto nfRow = noiseArea.removeFromTop(28);
    noiseFocusLabel.setBounds(nfRow.removeFromLeft(120));
    noiseFocusBox.setBounds(nfRow.removeFromLeft(150));

    area.removeFromTop(10);

    // Reverb section
    auto reverbArea = area.removeFromTop(130).reduced(20, 5);
    reverbLabel.setBounds(reverbArea.removeFromTop(25));
    reverbSlider.setBounds(reverbArea.removeFromTop(30));
    auto rfRow = reverbArea.removeFromTop(28);
    reverbFocusLabel.setBounds(rfRow.removeFromLeft(120));
    reverbFocusBox.setBounds(rfRow.removeFromLeft(150));

    area.removeFromTop(10);

    // Output gain
    auto gainArea = area.removeFromTop(80).reduced(20, 5);
    outputGainLabel.setBounds(gainArea.removeFromTop(25));
    outputGainSlider.setBounds(gainArea.removeFromTop(30));
}
