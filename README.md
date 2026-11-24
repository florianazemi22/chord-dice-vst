# Chord Dice (VST3)

The Chord Dice generator as a plugin for FL Studio. The window is the same Chord Dice
interface; the plugin sends its bass, chords and melody out as MIDI so you hear them
through any synth in your project.

- Chords play on MIDI channel 1, melody on 2, bass on 3. Tracks you switch off are not sent.
- While FL Studio plays, Chord Dice follows FL's tempo and song position and loops its pattern.
- With FL stopped, the Play button in the plugin plays the pattern at the plugin's own tempo.
- The bar at the bottom of the window is a drag handle: drag it onto the Playlist or Piano roll
  to drop the pattern in as MIDI.
- Everything is saved with your FL project.

## Get the plugin file

Every push to GitHub builds the plugin (see `.github/workflows/build.yml`):

1. Open the repository on GitHub > **Actions** > the latest **Build Chord Dice** run.
2. Download **ChordDice-Windows** (or **ChordDice-macOS**) under *Artifacts* and unzip it.

To build it yourself on Windows: install Visual Studio 2022 (Desktop C++), CMake and the
WebView2 SDK (`nuget install Microsoft.Web.WebView2 -OutputDirectory webview2`), then

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_WEBVIEW2_PACKAGE_LOCATION=webview2
    cmake --build build --config Release --target ChordDice_VST3

## Install in FL Studio 21 (Windows)

1. Copy the whole **Chord Dice.vst3** folder to `C:\Program Files\Common Files\VST3\`.
2. In FL Studio: **Options > Manage plugins > Find installed plugins**. Chord Dice appears under
   Generators. Add it to the Channel rack like any instrument.
3. Load the synth you want to hear (Serum, Vital, Omnisphere and so on) on another channel.

### Send Chord Dice into your synth

1. Open Chord Dice, click the **gear icon** at the top left of its window (plugin wrapper settings)
   and set **MIDI > Output port** to a number, for example **1**.
2. Open the synth's wrapper settings (gear icon) and set **MIDI > Input port** to the same number.
3. Press Play in FL Studio. The synth now plays the Chord Dice pattern.

To play bass, chords and melody on different synths, add a Chord Dice per synth with a different
output port each, and switch off the tracks you don't want in each one.

If nothing reaches the synth, older FL versions had trouble passing MIDI out of VST3 plugins.
Load Chord Dice and the synth inside **Patcher**, connect Chord Dice's MIDI output to the
synth's MIDI input, and set the output port as above. Dragging the MIDI from the bottom bar
always works.

## macOS

Copy **Chord Dice.vst3** to `~/Library/Audio/Plug-Ins/VST3/`. The build is not signed by Apple,
so run this once in Terminal before scanning:

    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Chord Dice.vst3"

## Updating the interface

`web/index.html` is the Chord Dice page. Inside the plugin it detects JUCE's bridge and sends
its pattern to the plugin instead of playing through Web Audio. Edit it, push, and the build
picks it up.

Built with JUCE 8 (AGPLv3 or a JUCE licence). Check JUCE's licence terms before selling the plugin.
