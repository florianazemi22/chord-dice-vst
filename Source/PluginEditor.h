#pragma once
#include "PluginProcessor.h"

/* A strip under the interface: drag it onto FL Studio's Playlist or Piano roll to drop the MIDI. */
class MidiDragStrip : public juce::Component
{
public:
    explicit MidiDragStrip (ChordDiceProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
private:
    ChordDiceProcessor& proc;
    bool dragging = false;
};

class ChordDiceEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChordDiceEditor (ChordDiceProcessor&);
    ~ChordDiceEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0b0c)); }

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    ChordDiceProcessor& proc;
    juce::WebBrowserComponent web;
    MidiDragStrip strip;
    double lastStep = -2.0;
    bool lastPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordDiceEditor)
};
