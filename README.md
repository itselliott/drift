# DRIFT

A hybrid analog/digital echo plugin in the spirit of the early-80s rack delays — Lexicon PCM 70/42, Korg SDD-3000 — pushed into mis-calibrated, self-eating territory. Companion to [SP·L](https://github.com/itselliott/spool). Windows VST3 + Standalone.

> Inspired by Chase Bliss + Electronic Audio Experiments' **Big Time** pedal: analog preamp into a digital delay engine into an analog feedback limiter. The limiter is the secret — pushed-up feedback gets compressed, then re-amplified, then compressed again, creating exponentially-evolving smear that slowly eats itself.

## v1.0 controls

Six faders, four modes, three limiter states. Six faders run CC 14–19 in DAWs.

| Fader | What it does |
|---|---|
| **COLOR** | Analog preamp drive. Cranks into harmonic saturation. |
| **TIME** | Delay time. Range depends on MODE. |
| **CLUSTER** | Extra delay taps at non-integer ratios → reverb-like smudge. |
| **TILT** | Filter tilt on the feedback path. Below 0.5 darkens each repeat, above brightens. |
| **FEEDBACK** | Repeats. With Compressed/Bias state engaged → infinite, self-oscillating. |
| **WET** | Wet/dry mix. |

| MODE | Range | Character |
|---|---|---|
| **MOD** | 1–50 ms | Chorus, flanger, doubling with LFO on by default. |
| **SHORT** | 20–600 ms | Slapback, tape-style short delay. |
| **LONG** | 100–3000 ms | Long meandering repeats. |
| **LOOP** | up to 30 s | Infinite-hold mode (full looper in v1.1). |

| STATE | Limiter character |
|---|---|
| **CLEAN** | Transparent peak limiter on the feedback path. |
| **COMP** | 5:1 compression — the "mis-calibrated PCM42" trick. |
| **BIAS** | Asymmetric tanh + DC bias creep. Crumbling. |

Footswitches: **BYPASS** (left) and **TAP / TEMPO** (right).

## Build

Requires **CMake 3.22+** and **Visual Studio 2022 Build Tools** on Windows. JUCE 8.0.4 fetched via CMake.

```powershell
.\build.ps1                # configure + Release build
.\build.ps1 -Clean -Run    # clean rebuild, then launch standalone
```

Artefacts land under `build/DRIFT_artefacts/Release/`:
- `Standalone/DRIFT.exe`
- `VST3/DRIFT.vst3/`

## Roadmap (v1.1+)

- Full looper (record / overdub / playback) in LOOP mode
- Scale-locked time-stepping (Thermae-style)
- CV / expression-pedal emulation via DAW automation
- Motorised-fader visualisation
- MIDI Clock sync, Program Change recall
- Preset bank

## Support

Bug reports → [elliottdevs@gmail.com](mailto:elliottdevs@gmail.com?subject=DRIFT%20Bug%20Report).

Tips: [Ko-fi](https://ko-fi.com/itselliott) · [GitHub Sponsors](https://github.com/sponsors/itselliott).

## License

GPLv3, inherited from JUCE 8. See [juce.com/get-juce/](https://juce.com/get-juce/) for commercial licensing options.
