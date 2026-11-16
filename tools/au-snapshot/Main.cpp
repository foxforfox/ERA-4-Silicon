// au-snapshot <plugin.component> <out.png> [--wait ms] [--scale n] [--audio dB]
//             [--set index=normalised ...] [--late] [--click x,y ...] [--list]
// au-snapshot --svg <in.svg> <out.png> [scale]
//
// Hosts one Audio Unit, shows its editor, and writes a PNG of the editor (drawn
// by the view itself, so it needs no screen-recording permission). Used with
// tools/ui-compare to match the ERA 4.5 editors to the original ERA 4 ones.
//
//   --scale n   pixels per point (default 2; 4 gives quarter-point precision)
//   --audio dB  feeds white noise at that peak level so meters and displays move
//   --feed f.wav [start s]  instead plays that file through in real time (snapshot after --wait)
//   --set       sets parameter <index> (see --list) to a normalised 0..1 value
//   --late      applies the --set values after the editor is open, not before
//   --click     clicks editor points in order, 400 ms apart, before the snapshot
//   --svg       instead rasterises one SVG the way JUCE draws it
//
// The ERA 4 originals are x86_64 only: run this under `arch -x86_64` for them.
// They are fragile there. Opening one and taking a picture works; changing
// parameters on the De-Esser Pro crashes or hangs it (the watchdog below ends
// the process after 40 s), and the De-Clipper does not load at all. Captures of
// the originals are already saved in tools/ui-compare/reference: use those
// rather than launching the originals again.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

extern "C" bool captureNSViewToPNG (void* viewPtr, const char* path, double scale);
extern "C" void clickInNSView (void* viewPtr, double x, double y);

// Old plugins can hang or crash when poked; never let that leave a window behind.
struct Watchdog : juce::Thread
{
    Watchdog() : Thread ("watchdog") { startThread(); }
    ~Watchdog() override { stopThread (1000); }

    void run() override
    {
        if (! wait (40000))
        {
            std::cerr << "timed out" << std::endl;
            std::_Exit (2);
        }
    }
};

// Feeds noise, or a file in real time, through the plugin so its meters and displays move.
struct NoiseFeeder : juce::Thread
{
    NoiseFeeder (juce::AudioPluginInstance& p, float gainDb, juce::AudioBuffer<float> fileToPlay = {})
        : Thread ("noise"), plugin (p), gain (juce::Decibels::decibelsToGain (gainDb)), file (std::move (fileToPlay)) {}

    void run() override
    {
        const int channels = juce::jmax (2, plugin.getTotalNumInputChannels(),
                                            plugin.getTotalNumOutputChannels());
        juce::AudioBuffer<float> buffer (channels, 512);
        juce::MidiBuffer midi;
        juce::Random random;

        while (! threadShouldExit())
        {
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (ch, i, file.getNumSamples() > 0
                                                 ? file.getSample (juce::jmin (ch, file.getNumChannels() - 1),
                                                                   (position + i) % file.getNumSamples())
                                                 : (random.nextFloat() * 2.0f - 1.0f) * gain);
            position += 512;

            plugin.processBlock (buffer, midi);
            wait (file.getNumSamples() > 0 ? 11 : 10);   // 512 samples at 44.1 kHz is 11.6 ms
        }
    }

    juce::AudioPluginInstance& plugin;
    float gain;
    juce::AudioBuffer<float> file;
    int position = 0;
};

class SnapshotApp : public juce::JUCEApplication,
                    private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "au-snapshot"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();

        if (args.size() >= 3 && args[0] == "--svg")
            return renderSvg (args[1], args[2], args.size() > 3 ? args[3].getFloatValue() : 2.0f);

        if (args.size() < 2)
            return fail ("usage: au-snapshot <plugin.component> <out.png> [--wait ms] [--scale n] "
                         "[--audio dB] [--set index=value ...] [--click x,y ...] [--list]");

        outFile = args[1];
        int waitMs = 1500;
        bool list = false, audio = false;
        float audioDb = -18.0f;

        for (int i = 2; i < args.size(); ++i)
        {
            if (args[i] == "--wait")        waitMs = args[++i].getIntValue();
            else if (args[i] == "--scale")  scale = args[++i].getDoubleValue();
            else if (args[i] == "--audio")  { audio = true; audioDb = args[++i].getFloatValue(); }
            else if (args[i] == "--list")   list = true;
            else if (args[i] == "--feed")
            {
                audio = true;
                feedFile = args[++i];
                if (i + 2 < args.size() && args[i + 1] == "start") { feedStart = args[i + 2].getDoubleValue(); i += 2; }
            }
            else if (args[i] == "--late")   setsAfterOpen = true;
            else if (args[i] == "--click")
            {
                const auto pair = args[++i];
                clicks.add ({ pair.upToFirstOccurrenceOf (",", false, false).getDoubleValue(),
                              pair.fromFirstOccurrenceOf (",", false, false).getDoubleValue() });
            }
            else if (args[i] == "--set")
            {
                const auto pair = args[++i];
                sets.add ({ pair.upToFirstOccurrenceOf ("=", false, false).getIntValue(),
                            pair.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
            }
        }

        juce::AudioUnitPluginFormat format;
        juce::OwnedArray<juce::PluginDescription> found;
        format.findAllTypesForFile (found, args[0]);
        if (found.isEmpty())
            return fail ("no Audio Unit found in " + args[0]);

        juce::String error;
        plugin = format.createInstanceFromDescription (*found[0], 44100.0, 512, error);
        if (plugin == nullptr)
            return fail ("could not load: " + error);

        plugin->prepareToPlay (44100.0, 512);

        const auto& params = plugin->getParameters();

        // Some plugins crash if parameters move before the editor exists.
        if (! setsAfterOpen)
            applySets();

        if (list)
            for (int i = 0; i < params.size(); ++i)
                std::cout << i << "\t" << params[i]->getName (64) << "\t" << params[i]->getValue()
                          << "\t" << params[i]->getCurrentValueAsText() << std::endl;

        editor.reset (plugin->createEditorIfNeeded());
        if (editor == nullptr)
            return fail ("plugin has no editor");

        std::cout << "editor " << editor->getWidth() << "x" << editor->getHeight() << std::endl;

        holder.setSize (editor->getWidth(), editor->getHeight());
        holder.addAndMakeVisible (*editor);
        holder.setTopLeftPosition (60, 60);
        holder.addToDesktop (juce::ComponentPeer::windowHasTitleBar);
        holder.setVisible (true);

        if (audio)
        {
            juce::AudioBuffer<float> fileBuffer;
            if (feedFile.isNotEmpty())
            {
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (feedFile)));
                if (reader == nullptr)
                    return fail ("cannot read " + feedFile);
                const auto start = (juce::int64) (feedStart * reader->sampleRate);
                const auto length = (int) juce::jmax ((juce::int64) 1, reader->lengthInSamples - start);
                fileBuffer.setSize ((int) juce::jmin (2u, reader->numChannels), length);
                reader->read (&fileBuffer, 0, length, start, true, true);
            }
            feeder = std::make_unique<NoiseFeeder> (*plugin, audioDb, std::move (fileBuffer));
            feeder->startThread();
        }

        startTimer (waitMs);
    }

    void applySets()
    {
        const auto& params = plugin->getParameters();
        for (auto [index, value] : sets)
            if (juce::isPositiveAndBelow (index, params.size()))
                params[index]->setValueNotifyingHost (value);
        sets.clear();
    }

    void timerCallback() override
    {
        if (! sets.isEmpty())
        {
            applySets();
            startTimer (700);
            return;
        }

        if (auto* peer = holder.getPeer(); peer != nullptr && nextClick < clicks.size())
        {
            const auto point = clicks[nextClick++];
            clickInNSView (peer->getNativeHandle(), point.x, point.y);
            startTimer (400);
            return;
        }

        stopTimer();
        holder.setSize (editor->getWidth(), editor->getHeight());

        bool ok = false;
        if (auto* peer = holder.getPeer())
            ok = captureNSViewToPNG (peer->getNativeHandle(), outFile.toRawUTF8(), scale);

        std::cout << (ok ? "wrote " : "FAILED ") << outFile << std::endl;
        setApplicationReturnValue (ok ? 0 : 1);
        quit();
    }

    void shutdown() override
    {
        if (feeder != nullptr)
            feeder->stopThread (2000);
        feeder.reset();
        holder.removeFromDesktop();
        editor.reset();
        plugin.reset();
    }

private:
    // au-snapshot --svg in.svg out.png [scale]: rasterises one SVG the way JUCE
    // draws it (viewBox at the origin, transparent background) and prints the
    // bounds of what it actually paints.
    void renderSvg (const juce::String& svgPath, const juce::String& pngPath, float svgScale)
    {
        const auto xml = juce::XmlDocument::parse (juce::File (svgPath));
        const auto drawable = xml != nullptr ? juce::Drawable::createFromSVG (*xml) : nullptr;
        if (drawable == nullptr)
            return fail ("could not read " + svgPath);

        const auto viewBox = juce::StringArray::fromTokens (xml->getStringAttribute ("viewBox"), " ", "");
        const auto width  = viewBox[2].getFloatValue();
        const auto height = viewBox[3].getFloatValue();

        juce::Image image (juce::Image::ARGB, juce::roundToInt (std::ceil (width) * svgScale),
                           juce::roundToInt (std::ceil (height) * svgScale), true);
        {
            juce::Graphics g (image);
            drawable->draw (g, 1.0f, juce::AffineTransform::scale (svgScale));
        }

        juce::File out (pngPath);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (image, stream);

        std::cout << svgPath << " viewBox " << width << "x" << height
                  << " painted " << drawable->getDrawableBounds().toString() << std::endl;
        quit();
    }

    void fail (const juce::String& message)
    {
        std::cerr << message << std::endl;
        setApplicationReturnValue (1);
        quit();
    }

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<NoiseFeeder> feeder;
    Watchdog watchdog;
    juce::Component holder;
    juce::String outFile, feedFile;
    double feedStart = 0.0;
    juce::Array<std::pair<int, float>> sets;
    bool setsAfterOpen = false;
    juce::Array<juce::Point<double>> clicks;
    int nextClick = 0;
    double scale = 2.0;
};

START_JUCE_APPLICATION (SnapshotApp)
