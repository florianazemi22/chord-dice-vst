#include "PluginEditor.h"
#include "BinaryData.h"

static juce::File webDataFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Chord Dice").getChildFile ("WebView");
}

ChordDiceEditor::ChordDiceEditor (ChordDiceProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (juce::WebBrowserComponent::Options{}
               .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                            .withUserDataFolder (webDataFolder())
                                            .withBackgroundColour (juce::Colour (0xff0b0b0c))
                                            .withStatusBarDisabled())
               .withNativeIntegrationEnabled()
               .withKeepPageLoadedWhenBrowserIsHidden()
               .withInitialisationData ("cdPlugin", true)
               .withInitialisationData ("cdState", p.getUiState())
               .withEventListener ("cdPattern",   [this] (const juce::var& v) { proc.setPatternFromVar (v); })
               .withEventListener ("cdTransport", [this] (const juce::var& v) { proc.setPreviewPlaying ((bool) v.getProperty ("play", false)); })
               .withEventListener ("cdAudition",  [this] (const juce::var& v) { proc.queueAudition (v.getProperty ("notes", {})); })
               .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })),
      strip (p)
{
    addAndMakeVisible (web);
    addAndMakeVisible (strip);
    setResizable (true, true);
    setResizeLimits (720, 560, 2000, 1500);
    setSize (1120, 820);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (30);
}

ChordDiceEditor::~ChordDiceEditor()
{
    stopTimer();
    proc.setPreviewPlaying (false);
}

void ChordDiceEditor::resized()
{
    auto r = getLocalBounds();
    strip.setBounds (r.removeFromBottom (40));
    web.setBounds (r);
}

std::optional<juce::WebBrowserComponent::Resource> ChordDiceEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);
    if (path == "index.html")
    {
        const auto* bytes = reinterpret_cast<const std::byte*> (BinaryData::index_html);
        return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + BinaryData::index_htmlSize), "text/html" };
    }
    return std::nullopt;
}

// Tell the interface where the playhead is, whether FL or the Play button is driving it.
void ChordDiceEditor::timerCallback()
{
    const double step = proc.uiStep.load();
    const bool playing = step >= 0.0;
    if (std::abs (step - lastStep) < 1.0e-6 && playing == lastPlaying) return;
    lastStep = step; lastPlaying = playing;
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("step", step);
    o->setProperty ("playing", playing);
    o->setProperty ("host", proc.uiHostPlaying.load());
    o->setProperty ("bpm", proc.uiHostBpm.load());
    web.emitEventIfBrowserIsVisible ("cdPos", juce::var (o.get()));
}

//==============================================================================
void MidiDragStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0b0b0c));
    auto pill = r.reduced (10.0f, 6.0f);
    g.setColour (dragging ? juce::Colour (0xff3a1f17) : juce::Colour (0xff1a1a1d));
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (dragging ? juce::Colour (0xffdf5830) : juce::Colour (0xff2e2e33));
    g.drawRoundedRectangle (pill, 8.0f, 1.0f);
    g.setColour (juce::Colour (0xffededef));
    g.setFont (juce::FontOptions (14.0f));
    const bool ready = proc.getMidiFileForDrag().getSize() > 0;
    g.drawText (ready ? juce::String::fromUTF8 ("\xe2\x87\xa3  Drag MIDI into FL Studio (bass, chords and melody that are switched on)")
                      : juce::String ("Roll a progression first, then drag it from here"),
                pill.toNearestInt(), juce::Justification::centred);
}

void MidiDragStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    const auto bytes = proc.getMidiFileForDrag();
    if (bytes.getSize() == 0) return;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Chord Dice");
    dir.createDirectory();
    auto file = dir.getChildFile (proc.getMidiFileName() + ".mid");
    if (! file.replaceWithData (bytes.getData(), bytes.getSize())) return;
    dragging = true;
    repaint();
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [this] { dragging = false; repaint(); });
}
