#include "PluginProcessor.h"
#ifndef CD_TEST_NO_EDITOR
 #include "PluginEditor.h"
#endif

ChordDiceProcessor::ChordDiceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    active.reserve (256);
    auditions.reserve (64);
}

void ChordDiceProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
}

//==============================================================================
// Swing follows the web player exactly: within each beat, the first two
// sixteenths are stretched by (2 + s) / 2 and the last two squeezed by (2 - s) / 2.
double ChordDiceProcessor::stepToPpq (double step, const Pattern& p)
{
    const double sw = juce::jlimit (0.0, 1.0, p.swing) * 0.66;
    const double beat = std::floor (step / 4.0);
    const double r = step - beat * 4.0;
    const double t = r <= 2.0 ? r * (2.0 + sw) / 2.0
                              : (2.0 + sw) + (r - 2.0) * (2.0 - sw) / 2.0;
    const double stepPpq = p.halfTime ? 0.5 : 0.25;
    return (beat * 4.0 + t) * stepPpq;
}

void ChordDiceProcessor::allNotesOff (juce::MidiBuffer& midi, int sample)
{
    for (auto& a : active)
        midi.addEvent (juce::MidiMessage::noteOff (a.ch, a.n), sample);
    active.clear();
}

void ChordDiceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();   // FL's own piano-roll notes are not passed on; this plugin only plays its pattern

    const int numSamples = buffer.getNumSamples();

    {   // pick up a new pattern if the interface sent one
        const juce::SpinLock::ScopedTryLockType lock (patternLock);
        if (lock.isLocked() && hasPending)
        {
            live = std::move (pending);
            pending = {};
            hasPending = false;
        }
    }

    // ---- auditions: single chords or notes played when you click in the interface ----
    {
        const juce::SpinLock::ScopedTryLockType lock (auditionLock);
        if (lock.isLocked() && ! auditionQueue.empty())
        {
            for (auto& a : auditionQueue) auditions.push_back (a);
            auditionQueue.clear();
        }
    }
    for (auto& a : auditions)
    {
        if (! a.started)
        {
            if (a.startIn < numSamples)
            {
                midi.addEvent (juce::MidiMessage::noteOn (a.ch, a.n, juce::jlimit (0.05f, 1.0f, a.v)), juce::jmax (0, a.startIn));
                a.started = true;
                a.samplesLeft += a.startIn;
            }
            else { a.startIn -= numSamples; continue; }
        }
        if (a.samplesLeft < numSamples)
        {
            midi.addEvent (juce::MidiMessage::noteOff (a.ch, a.n), juce::jlimit (0, numSamples - 1, a.samplesLeft));
            a.samplesLeft = -1;
        }
        else a.samplesLeft -= numSamples;
    }
    auditions.erase (std::remove_if (auditions.begin(), auditions.end(), [] (const Audition& a) { return a.started && a.samplesLeft < 0; }), auditions.end());

    // ---- where are we? ----
    bool hostPlaying = false;
    double hostPpq = 0.0, bpm = live.bpm > 0 ? live.bpm : 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; uiHostBpm.store (*b); }
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) { hostPlaying = true; hostPpq = *ppq; }
        }

    const bool preview = ! hostPlaying && previewPlaying.load();
    if (! preview) previewPpq = 0.0;
    const bool running = hostPlaying || preview;
    if (! hostPlaying) bpm = live.bpm > 0 ? live.bpm : 120.0;

    if (! running)
    {
        if (wasRunning) allNotesOff (midi, 0);
        wasRunning = false;
        expectedPpq = -1.0;
        uiStep.store (-1.0);
        uiHostPlaying.store (false);
        return;
    }

    const double ppqStart = hostPlaying ? hostPpq : previewPpq;
    const double blockPpq = numSamples / sr * bpm / 60.0;
    const double ppqEnd = ppqStart + blockPpq;

    // the host jumped (loop, scrub, restart): close everything that was sounding
    if (wasRunning && std::abs (ppqStart - expectedPpq) > 1.0e-3)
        allNotesOff (midi, 0);
    wasRunning = true;
    expectedPpq = ppqEnd;
    if (preview) previewPpq = ppqEnd;

    const auto toSample = [&] (double ppq) { return juce::jlimit (0, numSamples - 1, (int) ((ppq - ppqStart) / blockPpq * numSamples)); };

    // note-offs first, so a re-struck pitch is released before it sounds again
    for (auto it = active.begin(); it != active.end();)
    {
        if (it->offPpq < ppqEnd) { midi.addEvent (juce::MidiMessage::noteOff (it->ch, it->n), toSample (it->offPpq)); it = active.erase (it); }
        else ++it;
    }

    const double loopPpq = stepToPpq (live.totalSteps, live);
    if (loopPpq > 0.0 && ! live.notes.empty())
    {
        const auto first = (long long) std::floor (ppqStart / loopPpq);
        const auto last  = (long long) std::floor (ppqEnd / loopPpq);
        for (auto L = first; L <= last; ++L)
        {
            const double base = (double) L * loopPpq;
            for (const auto& nt : live.notes)
            {
                const double on = base + stepToPpq (nt.g, live);
                if (on < ppqStart || on >= ppqEnd) continue;
                double off = base + stepToPpq (nt.g + nt.d, live) - 0.01;
                if (off <= on) off = on + 0.02;

                for (auto it = active.begin(); it != active.end();)   // same pitch still ringing
                {
                    if (it->n == nt.n && it->ch == nt.ch) { midi.addEvent (juce::MidiMessage::noteOff (it->ch, it->n), toSample (on)); it = active.erase (it); }
                    else ++it;
                }
                midi.addEvent (juce::MidiMessage::noteOn (nt.ch, nt.n, juce::jlimit (0.05f, 1.0f, nt.v)), toSample (on));
                if (off < ppqEnd) midi.addEvent (juce::MidiMessage::noteOff (nt.ch, nt.n), toSample (off));
                else if (active.size() < 512) active.push_back ({ nt.n, nt.ch, off });
            }
        }

        // position for the interface's playhead, in steps (swing ignored)
        const double stepPpq = live.halfTime ? 0.5 : 0.25;
        const double inLoop = std::fmod (ppqStart, loopPpq);
        uiStep.store ((inLoop < 0 ? inLoop + loopPpq : inLoop) / stepPpq);
    }
    uiHostPlaying.store (hostPlaying);
}

//==============================================================================
ChordDiceProcessor::Pattern ChordDiceProcessor::parsePattern (const juce::var& obj)
{
    Pattern p;
    p.totalSteps = juce::jlimit (1, 4096, (int) obj.getProperty ("total", 64));
    p.swing      = juce::jlimit (0.0, 1.0, (double) obj.getProperty ("swing", 0.0));
    p.halfTime   = (bool) obj.getProperty ("half", false);
    p.bpm        = juce::jlimit (20.0, 400.0, (double) obj.getProperty ("bpm", 120.0));
    if (auto* arr = obj.getProperty ("notes", {}).getArray())
        for (const auto& item : *arr)
            if (auto* a = item.getArray(); a != nullptr && a->size() >= 5)
            {
                Note n;
                n.g  = (int) (*a)[0];
                n.d  = juce::jmax (1, (int) (*a)[1]);
                n.n  = juce::jlimit (0, 127, (int) (*a)[2]);
                n.v  = (float) (double) (*a)[3];
                n.ch = juce::jlimit (1, 16, (int) (*a)[4]);
                if (n.g >= 0 && n.g < p.totalSteps) p.notes.push_back (n);
            }
    return p;
}

void ChordDiceProcessor::setPatternFromVar (const juce::var& obj)
{
    auto p = parsePattern (obj);
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        pending = std::move (p);
        hasPending = true;
    }
    const juce::ScopedLock sl (stateLock);
    if (obj.hasProperty ("state")) uiStateJson = obj.getProperty ("state", {}).toString();
    if (obj.hasProperty ("name"))  midiName = juce::File::createLegalFileName (obj.getProperty ("name", "chord-dice").toString());
    if (obj.hasProperty ("midi"))
    {
        juce::MemoryOutputStream out;
        if (juce::Base64::convertFromBase64 (out, obj.getProperty ("midi", {}).toString()))
            midiBytes = out.getMemoryBlock();
    }
    juce::DynamicObject::Ptr copy = new juce::DynamicObject();
    for (auto key : { "total", "swing", "half", "bpm", "notes" })
        copy->setProperty (key, obj.getProperty (key, {}));
    patternJson = juce::JSON::toString (juce::var (copy.get()), true);
}

void ChordDiceProcessor::queueAudition (const juce::var& notes)
{
    std::vector<Audition> list;
    if (auto* arr = notes.getArray())
        for (const auto& item : *arr)
            if (auto* a = item.getArray(); a != nullptr && a->size() >= 5)
            {
                const double dur = juce::jlimit (0.03, 4.0, (double) (*a)[2]);
                const double delay = juce::jlimit (0.0, 2.0, (double) (*a)[4]);
                list.push_back ({ juce::jlimit (0, 127, (int) (*a)[0]), juce::jlimit (1, 16, (int) (*a)[3]),
                                  (float) (double) (*a)[1], (int) (delay * sr), (int) (dur * sr), false });
            }
    const juce::SpinLock::ScopedLockType lock (auditionLock);
    for (auto& a : list) if (auditionQueue.size() < 128) auditionQueue.push_back (a);
}

juce::String ChordDiceProcessor::getUiState() const
{
    const juce::ScopedLock sl (stateLock);
    return uiStateJson;
}

juce::MemoryBlock ChordDiceProcessor::getMidiFileForDrag() const
{
    const juce::ScopedLock sl (stateLock);
    return midiBytes;
}

juce::String ChordDiceProcessor::getMidiFileName() const
{
    const juce::ScopedLock sl (stateLock);
    return midiName;
}

//==============================================================================
// Saved with the FL Studio project: the interface state, the pattern (so MIDI plays
// even if the plugin window is never opened) and the last MIDI file for dragging.
void ChordDiceProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    const juce::ScopedLock sl (stateLock);
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("ui", uiStateJson);
    o->setProperty ("pattern", patternJson);
    o->setProperty ("midi", juce::Base64::toBase64 (midiBytes.getData(), midiBytes.getSize()));
    o->setProperty ("name", midiName);
    juce::MemoryOutputStream out (dest, false);
    out.writeString (juce::JSON::toString (juce::var (o.get()), true));
}

void ChordDiceProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream in (data, (size_t) size, false);
    const auto parsed = juce::JSON::parse (in.readString());
    if (! parsed.isObject()) return;

    const auto pat = juce::JSON::parse (parsed.getProperty ("pattern", {}).toString());
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    if (pat.isObject())
        if (auto* src = pat.getDynamicObject())
            for (auto& p : src->getProperties()) o->setProperty (p.name, p.value);
    o->setProperty ("state", parsed.getProperty ("ui", {}));
    o->setProperty ("midi", parsed.getProperty ("midi", {}));
    o->setProperty ("name", parsed.getProperty ("name", "chord-dice"));
    setPatternFromVar (juce::var (o.get()));
}

#ifndef CD_TEST_NO_EDITOR
juce::AudioProcessorEditor* ChordDiceProcessor::createEditor() { return new ChordDiceEditor (*this); }
#else
juce::AudioProcessorEditor* ChordDiceProcessor::createEditor() { return nullptr; }
#endif

#ifndef CD_TEST_NO_EDITOR
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordDiceProcessor(); }
#endif
