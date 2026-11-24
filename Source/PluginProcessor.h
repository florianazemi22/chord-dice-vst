#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>

/*  Chord Dice plays a looping pattern that the web interface builds.
    The pattern lives in sixteenth-note steps; the processor turns it into MIDI,
    locked to FL Studio's transport while FL plays, or to its own clock for the
    Play button in the plugin window. Chords go out on channel 1, melody on 2, bass on 3. */
class ChordDiceProcessor : public juce::AudioProcessor
{
public:
    struct Note { int g = 0, d = 1, n = 60, ch = 1; float v = 0.8f; };
    struct Pattern
    {
        std::vector<Note> notes;
        int totalSteps = 64;
        double swing = 0.0, bpm = 120.0;
        bool halfTime = false;
    };

    ChordDiceProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chord Dice"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- called from the editor (message thread) ----
    void setPatternFromVar (const juce::var& obj);
    void setPreviewPlaying (bool shouldPlay)       { previewPlaying.store (shouldPlay); }
    bool isPreviewPlaying() const                 { return previewPlaying.load(); }
    void queueAudition (const juce::var& notes);
    juce::String getUiState() const;
    juce::MemoryBlock getMidiFileForDrag() const;
    juce::String getMidiFileName() const;

    // ---- read by the editor's timer ----
    std::atomic<double> uiStep { -1.0 };
    std::atomic<bool> uiHostPlaying { false };
    std::atomic<double> uiHostBpm { 0.0 };

private:
    static Pattern parsePattern (const juce::var& obj);
    static double stepToPpq (double step, const Pattern&);
    void allNotesOff (juce::MidiBuffer&, int sample);

    // pattern hand-over: message thread writes `pending`, audio thread swaps it in
    juce::SpinLock patternLock;
    Pattern pending;
    bool hasPending = false;
    Pattern live;

    struct Active { int n, ch; double offPpq; };
    std::vector<Active> active;
    struct Audition { int n, ch; float v; int startIn, samplesLeft; bool started; };
    juce::SpinLock auditionLock;
    std::vector<Audition> auditionQueue, auditions;

    std::atomic<bool> previewPlaying { false };
    bool wasRunning = false;
    double previewPpq = 0.0, expectedPpq = -1.0, sr = 44100.0;

    mutable juce::CriticalSection stateLock;
    juce::String uiStateJson, patternJson, midiName { "chord-dice" };
    juce::MemoryBlock midiBytes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordDiceProcessor)
};
