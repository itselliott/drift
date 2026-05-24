# DRIFT

> **`DF-T`** — a hybrid analog/digital echo plugin in the spirit of the Chase Bliss × Electronic Audio Experiments [**Big Time**](https://www.chasebliss.com/). Six motorised faders, four feedback states, infinite hold, and a shared-memory link to its sibling plugin [**SP·L**](https://github.com/itselliott/spool).
>
> Open source. GPL-3.0. VST3 + Standalone. Windows-first (macOS / Linux build from source).

**[→ drift website + full manual](https://itselliott.github.io/drift/)**

---

## What it does

A character-driven delay engine modelled on the early-80s rack delay lineage (PCM 42 / SDD-3000) routed through a deliberately mis-calibrated feedback limiter — pushed-up feedback gets compressed, re-amplified, compressed again, creating exponentially-evolving smear that slowly eats itself.

**Six motorised faders** with a SHIFT alt-menu that gives you twelve audible parameters from six physical lanes.

| Fader | Main | SHIFT alt |
|---|---|---|
| **COLOR** | Preamp drive (JFET-style soft clip + body boost) | **TEXTURE** — state-character knob |
| **TIME** | Delay-time clock (range depends on MODE) | **RATE** — motion speed / glide time |
| **CLUSTER** | Three-zone tap scatter (synced → unsynced → drift) | **DEPTH** — motion amplitude |
| **TILT** | Feedback-path tone, dark↔bright | **CROSSOVER** — TILT split frequency |
| **FEEDBACK** | Loop gain. Caps depend on STATE | **DIFFUSE** — Schroeder allpass diffusion |
| **WET** | Wet/dry crossfade (equal-power) | **DRY** — standalone dry-level control |

**Five cycle buttons** + dedicated SHIFT, plus the alts.

| Button | Main | SHIFT alt |
|---|---|---|
| **SCALE** | OFF / CHROMATIC / OCT·4·5 / OCTAVE — TIME quantisation | **SPREAD** — OFF / SUBTLE / PING-PONG |
| **MOTION** | OFF / SINE / SQUARE / ENV (transient-stepped) | **0.5X** — halves SR + drops to 12-bit |
| **MODE** | MOD / SHORT / LONG / LOOP — delay-time range | **DIFFUSE TYPE** — doubles DIFFUSE strength |
| **VOICING** | HIFI / FOCUS / WARM / ANALOG — fixed filter character | **+12 dB** — preamp boost |
| **STATE** | DIGITAL / COMPRESSED / SATURATED / BIAS — feedback character | — |

**Footswitches.** BYPASS (left, **B**) and TAP TEMPO (right, **Space**).

**HOLD button** (dedicated, **H**). Sticky toggle, mode-dispatched:
- MOD → **OVERLOAD** (ramps COLOR + FEEDBACK to max while held)
- SHORT/LONG → **HOLD** (freezes buffer + infinite feedback)
- LOOP → **DELETE** (wipes loop + snaps TIME to centre)

## Time ranges (per MODE)

| Mode | Range |
|---|---|
| **MOD** | 3 – 46 ms (chorus, flanger, doubling) |
| **SHORT** | 46 – 736 ms (slapback, tape echo) |
| **LONG** | 0.7 – 12.2 s (washes, ambient repeats) |
| **LOOP** | 1 – 29.5 s phrase looper. Carries over from LONG. |

## States

| State | Limiter behaviour | TEXTURE alt |
|---|---|---|
| **DIGITAL** | None — clean, transparent | Aliasing + bit-depth crush |
| **COMPRESSED** | Soft compression + ducking sag | Squeeze amount |
| **SATURATED** | Pure static tanh waveshaper (no envelope) | Clip symmetry |
| **BIAS** | Creeping DC misbias → crumbling | Creep speed + clip depth |

## LINQ — works with SP·L

Run DRIFT.exe and [SPOOL.exe](https://github.com/itselliott/spool) as standalones, click the **LINQ pill** in each (next to the wordmark) — audio streams from SP·L's output directly into DRIFT's input via a custom Windows shared-memory ring buffer. No virtual audio cable, no DAW, ~one audio-block of latency.

Pill colours:
- **dim grey** — LINQ off
- **amber** — waiting for partner
- **green** — linked, audio flowing
- **red** — sample-rate mismatch

## Other features

- **35 factory presets** covering every state / mode / motion / voicing combo. User edits persist.
- **MIDI Clock in/out** at 24 PPQ — sync delay to host tempo or emit clock for chained pedals.
- **MIDI CC** for every fader + cycle button (CC 14–32).
- **MIDI Program Change** recalls preset slots 0–34.
- **Options Menu** (`O` key): TRAILS · DRY KILL · DRY CLEAN · SCALE IGNORE · STEP · CLOCK OUT.
- **DRIFT chassis**: charcoal faceplate, wood cheeks, brushed-metal motorised faders, twin red-7-seg OLED (delay-time + preset slot).

## Build

Requires **CMake 3.22+** and a C++17 toolchain. JUCE 8.0.4 is fetched via CMake's FetchContent.

```bash
git clone https://github.com/itselliott/drift.git
cd drift
cmake -S . -B build
cmake --build build --config Release
```

Artefacts land under `build/DRIFT_artefacts/Release/`:
- `Standalone/DRIFT.exe` — runs as its own app (Windows)
- `VST3/DRIFT.vst3/` — drop in your VST3 folder
- `AU/` (macOS only) — drop in `~/Library/Audio/Plug-Ins/Components/`

Windows convenience script:

```powershell
.\build.ps1                # configure + Release build
.\build.ps1 -Clean -Run    # clean rebuild + launch standalone
```

## Hotkeys (standalone + most VST3 hosts)

| Key | Action |
|---|---|
| `Space` | TAP tempo (or looper-state cycle in LOOP mode) |
| `B` | Toggle bypass |
| `H` | Toggle HOLD gesture (mode-dispatched) |
| `D` | Delete loop (LOOP mode) |
| `O` | Open Options menu |
| `1`–`9`, `0` | Direct-load preset slot 1–10 |
| `Shift`+`S` | Save current state to active preset slot |
| `← / →` chevrons (UI) | Cycle presets |

## Support

- **Bug reports** → [GitHub Issues](https://github.com/itselliott/drift/issues) or [elliottdevs@gmail.com](mailto:elliottdevs@gmail.com?subject=DRIFT%20Bug%20Report)
- **Tips** → [Ko-fi](https://ko-fi.com/itselliott) · [GitHub Sponsors](https://github.com/sponsors/itselliott)

## License

GPL-3.0. JUCE 8 is GPL-3.0 too — visit [juce.com/get-juce/](https://juce.com/get-juce/) for commercial licensing options.

Not affiliated with Chase Bliss Audio or Electronic Audio Experiments — DRIFT is an independent software reimagining of the Big Time control surface and signal flow.
