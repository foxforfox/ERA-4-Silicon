// au-render <plugin.component> <in.wav> <out.wav> [--set index=normalised ...] [--list] [--block n]
//
// Renders in.wav (mono or stereo) through the Audio Unit offline and writes a
// 32-bit float out.wav at the same rate. Prints the latency the plugin reports;
// the measuring script aligns by cross-correlation anyway, since old plugins
// do not always report it. --list prints the parameters (index, name, value).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

static int fail (const juce::String& message)
{
    std::cerr << message << std::endl;
    return 1;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::CharPointer_UTF8 (argv[i]));

    if (args.size() < 3)
        return fail ("usage: au-render <plugin.component> <in.wav> <out.wav> [--set index=value ...] [--list] [--block n]");

    int blockSize = 512;
    bool list = false;
    juce::Array<std::pair<int, float>> sets;
    for (int i = 3; i < args.size(); ++i)
    {
        if (args[i] == "--list") list = true;
        else if (args[i] == "--block" && i + 1 < args.size()) blockSize = args[++i].getIntValue();
        else if (args[i] == "--set" && i + 1 < args.size())
        {
            const auto pair = args[++i];
            sets.add ({ pair.upToFirstOccurrenceOf ("=", false, false).getIntValue(),
                        pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
        }
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (args[1])));
    if (reader == nullptr)
        return fail ("cannot read " + args[1]);

    const auto sampleRate = reader->sampleRate;
    const auto length = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> input (2, length);
    reader->read (&input, 0, length, 0, true, true);   // mono files fill both channels

    juce::AudioUnitPluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile (found, args[0]);
    if (found.isEmpty())
        return fail ("no Audio Unit found in " + args[0]);

    juce::String error;
    auto plugin = format.createInstanceFromDescription (*found[0], sampleRate, blockSize, error);
    if (plugin == nullptr)
        return fail ("could not load: " + error);

    plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    const auto& params = plugin->getParameters();
    for (auto [index, value] : sets)
        if (juce::isPositiveAndBelow (index, params.size()))
            params[index]->setValueNotifyingHost (value);

    if (list)
        for (int i = 0; i < params.size(); ++i)
            std::cout << i << "\t" << params[i]->getName (64) << "\t" << params[i]->getValue()
                      << "\t" << params[i]->getCurrentValueAsText() << std::endl;

    std::cout << "latency " << plugin->getLatencySamples() << std::endl;

    juce::AudioBuffer<float> output (2, length);
    juce::AudioBuffer<float> block (juce::jmax (2, plugin->getTotalNumInputChannels(),
                                                   plugin->getTotalNumOutputChannels()), blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < length; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, length - pos);
        block.clear();
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, input, ch, pos, n);

        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), block.getNumChannels(), n);
        plugin->processBlock (view, midi);

        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, pos, block, ch, 0, n);
    }

    plugin->releaseResources();

    juce::File outFile (args[2]);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (new juce::FileOutputStream (outFile), sampleRate, 2, 32, {}, 0));
    if (writer == nullptr)
        return fail ("cannot write " + args[2]);

    writer->writeFromAudioSampleBuffer (output, 0, length);
    return 0;
}
