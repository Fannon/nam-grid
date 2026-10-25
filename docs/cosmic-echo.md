# Cosmic Echo

Six ambient echo and reverb styles, from rhythmic repeats to vast washes. Find **Cosmic Echo** in the Reverb category.

The module is directly inspired by the algorithms of [Valhalla Supermassive](https://valhalladsp.com/shop/reverb/valhalla-supermassive/), a free plugin by Valhalla DSP that is well worth trying on its own. It is adapted from David Braun's independent, MIT-licensed [Faust reconstruction](https://faustlibraries.grame.fr/libs/reverbs/#revalhallasupermassive). These are six modes of that model, not every mode of the commercial plugin, and they do not claim to sound identical.

![Cosmic Echo editor](screenshots/modules/cosmic-echo.png)

## Styles

| Style | Character |
| --- | --- |
| **Gemini** | Quick, dense and smooth ambience |
| **Sagittarius** | Slow, swelling build-up |
| **Virgo** | Sparse echoes that get darker with every repeat (the starting style) |
| **Centaurus** | Swelling, enveloping ambience |
| **Great Annihilator** | Delays and diffusion in one large feedback loop |
| **Lyra** | Clearer, sparse echoes |

Changing the style fades out the old tail before the new style comes in.

## Shape the sound

| Control | What to listen for |
| --- | --- |
| **Time** | Overall delay scale, from 20 ms to 8 s. Sagittarius can take several intervals to build up. |
| **Warp** | At 0, echoes are evenly spaced. Higher values spread them into uneven, reverb-like spacing. |
| **Feedback** | Sustain. In Sagittarius and Centaurus it also changes how the sound builds up, not only how long it lasts. |
| **Density** | From distinct echoes toward a dense stereo field. It works together with Warp and changes little at Warp 0. |
| **High cut** | Treble of the effect. In Virgo every repeat gets darker. |
| **Modulation** | From still echoes toward obvious pitch movement and chorus. |
| **Mix** | Balance between dry sound and effect. |

Some things are fixed: the modulation runs at 0.3 Hz, the effect is always fully stereo with an 80 Hz low cut, and there is no tempo sync. Automating Time or Warp bends the pitch of the sounding tail. Ranges and starting values are in the [module guide](modules/reverb.md#cosmic-echo).

## Tips

- Start with low Warp for rhythmic echoes, then raise Warp and Density for an ambient wash.
- Gemini is dense straight away; lower Warp and Density to hear its echo structure.
- For Sagittarius, try lower Feedback: it changes the swell, not just the length.
- Compare at matched loudness against Cloud, Diffused Delay and Galactic Space, with short attacks, held chords and high-gain guitar.

## The display

The display shows the measured response of the current settings to a single click, on a dB scale. Separate spikes are distinct echoes; Warp spreads them into a wash, and Density then fills it in. It is measured again whenever a setting changes, so it shows what you will hear rather than an estimate.
