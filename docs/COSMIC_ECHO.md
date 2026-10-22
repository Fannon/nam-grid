# Cosmic Echo audition prototype

Find **Cosmic Echo** in the Reverb menu. The Style control offers **Gemini,
Sagittarius, Virgo, Centaurus, Great Annihilator and Lyra**. The module is
**Valhalla Supermassive-inspired**, adapted from David Braun's independent MIT
Faust reconstruction. Keep Galactic Space alongside it while comparing textures.

| Control | Range | What to listen for |
| --- | --- | --- |
| Time | 20–8000 ms (quadratic dial) | Delay-line scale; Sagittarius can take several intervals to build up |
| Warp | 0–100% | Equal-length echoes spread into uneven, reverb-like spacing |
| Feedback | 0–98% | Sustain; in Sagittarius/Centaurus it changes the buildup as well as decay |
| Density | 0–100% | Scattering between lines: distinct echoes toward a denser stereo field |
| High cut | 200 Hz–16 kHz | Wet treble; Virgo also darkens each feedback pass |
| Motion | 0–100% | Shared 0.3 Hz modulation depth, from still echoes toward obvious pitch movement |
| Mix | 0–100% | Standard linear dry/wet blend; zero is exact stereo dry |

The initial settings are 500 ms, Warp 65%, Feedback 65%, Density 80%, High cut 6 kHz,
Motion 25% and Mix 25%. Modulation rate is fixed at 0.3 Hz, width at full stereo
and low cut at 80 Hz. Tempo sync is not included in this first prototype.

**Gemini** gives quick, dense ambience; reduce Warp/Density to hear the echo
structure. **Sagittarius** is the slow swelling option: try lower Feedback to hear its
different attack rather than assuming Feedback only extends the end. **Virgo**
leaves more space between echoes, with filters inside the loop. Compare at
matched wet loudness, particularly against Cloud, Diffused Delay and Galactic
Space, and with short attacks, held chords and high-gain guitar.

Style changes fade and clear the old tail before bringing in the new style.
Time/Warp automation moves allpass-interpolated reads and can bend the sounding
tail. The display's upper sketch shows actual nominal line lengths and Density's
scattering; lower gray/colored traces show live input/output levels. It does not
claim an exponential RT60 or show a fabricated impulse response.

## Files and checks

Windows Release CLAP/VST3 builds are in `build-grid/out/`. Three `.ngrid` presets,
plucked-chord WAVs, fully wet impulse WAVs and a measurements CSV are in
`build-grid/cosmic-audition/`. The earlier nine-choice package remains available
as `build-grid/nam-grid-fx-audition-windows-x64-20261021.zip`. Presets saved by its
schema-20 build can be recalled by the current Cosmic build.

Reproduce the new renders:

```powershell
cmake --build build-grid --target nam-grid-clap nam-grid-vst3 nam-fx-audition
& .\build-grid\NeuralAmpModeler\nam-fx-audition.exe .\build-grid\out\nam-grid.clap .\build-grid\cosmic-audition cosmic
```

Historical three-mode audition: one offline 48 kHz/128-frame callback run measured about 1.60% CPU for Smooth,
1.54% for Bloom and 0.96% for Sparse, including the grid. Default wet impulse
RT60 estimates were 5.45, 4.71 and 5.25 seconds. These are eight-second,
−5 to −25 dB energy-decay fits and a single timing run; attack and long tails
make them approximate, not control calibrations or DAW guarantees.

DSP tests pass at 44.1/48/96/192 kHz for all styles, including unequal buffers,
range/Style automation, exact dry stereo, impulse timing, bounded maximum
feedback at both 20 ms and 8-second Time and zero audio allocations/deletions.
The focused DSP suite passed in 26.18 seconds after adding the long endpoint
checks. All five CTest suites passed, including the full native CLAP host/editor
suite in 553.36 seconds. It verifies Style automation/recall in first and last
slots and preservation of the previous audition build's Galactic parameters.
The native timeout is now 900 seconds to allow headroom for slower CI hosts.
Native screenshots of the initial three
styles were inspected with audio feeding the live histories. Final musical
voicing and low-buffer DAW audition remain open.

This build has 49 built-in choices, state schema **23**, **3,099 parameters** and
173 FX controls per slot. Buffer storage is about 47.2 MiB per Cosmic module at
48 kHz, increasing with sample rate. Extreme feedback can produce tails longer
than the plugin's existing 60-second host render-tail policy. Detailed source,
memory and transition notes are in
[COSMIC.md](../NeuralAmpModeler/dsp/reverb/COSMIC.md).

The range/label update preserves schema-21 cutoff values, including performance scenes. Its new default and scale supersede the dated 300 ms audition measurements above.

## Six-mode audition update

Cosmic Echo is **Valhalla Supermassive-inspired**, using David Braun's MIT Faust
reconstruction rather than Valhalla's original code. All six source modes are now
available. The old Smooth/Bloom/Sparse choices keep their integer positions but
are named Gemini/Sagittarius/Virgo so comparisons are easier.

| Mode | Character and topology |
| --- | --- |
| Gemini | Dense ambience from sixteen scattering delays |
| Sagittarius | Slow buildup from eight serial stereo allpasses |
| Virgo | Sparse, darker echoes with both cuts inside feedback |
| Centaurus | Swelling ambience from four serial four-channel allpasses |
| Great Annihilator | A delay stage and three matrix allpasses in global feedback |
| Lyra | Clearer sparse echoes from four scattering delays, filters at output |

All modes share the prepared sixteen-line bank and the fade/clear/fade switching
policy; adding modes does not multiply buffer memory. Warp, Density, Feedback
and modulation retain the source model's different interactions per topology.
The current audition exporter writes six presets and paired pluck/wet-impulse
renders. These are six modes of the independent model, not every mode in the
commercial plugin, and do not claim identical sound.

[Faust reference](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive)
