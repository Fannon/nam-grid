# Cosmic Echo

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
| Modulation | 0–100% | Shared 0.3 Hz modulation depth, from still echoes toward obvious pitch movement |
| Mix | 0–100% | Standard linear dry/wet blend; zero is exact stereo dry |

The insertion and dial-reset settings are Virgo, Time 280 ms, Warp 51.6%,
Feedback 30.9%, Density 49.2%, High cut 7461 Hz, Modulation 29.5% and Mix 18%.
Modulation rate is fixed at 0.3 Hz, width at full stereo
and low cut at 80 Hz. Tempo sync is not available for this module.

**Gemini** gives quick, dense ambience; reduce Warp/Density to hear the echo
structure. **Sagittarius** is the slow swelling option: try lower Feedback to hear its
different attack rather than assuming Feedback only extends the end. **Virgo**
leaves more space between echoes, with filters inside the loop. Compare at
matched wet loudness, particularly against Cloud, Diffused Delay and Galactic
Space, and with short attacks, held chords and high-gain guitar.

Style changes fade and clear the old tail before bringing in the new style.
Time/Warp automation moves allpass-interpolated reads and can bend the sounding
tail. The display shows the module's measured wet response to one impulse, rendered by a
private copy of the DSP whenever a setting changes. With Warp at 0 the lines align into a
few distinct echoes and Density changes little; Warp spreads them into a wash, and Density
then fills it. It is a measurement of the current settings, not an RT60 estimate.

## Styles and implementation

| Mode | Character and topology |
| --- | --- |
| Gemini | Dense ambience from sixteen scattering delays |
| Sagittarius | Slow buildup from eight serial stereo allpasses |
| Virgo | Sparse, darker echoes with both cuts inside feedback |
| Centaurus | Swelling ambience from four serial four-channel allpasses |
| Great Annihilator | A delay stage and three matrix allpasses in global feedback |
| Lyra | Clearer sparse echoes from four scattering delays, filters at output |

Warp, Density, Feedback
and modulation retain the source model's different interactions per topology.
These are six modes of the independent model, not every mode in the
commercial plugin, and do not claim identical sound.

[Faust reference](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive)
