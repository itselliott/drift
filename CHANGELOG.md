# Changelog

All notable changes to **DRIFT** will be recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Pre-filled bug-report mailto in the About overlay — auto-includes version, OS, host, sample rate, current preset, mode/state/voicing.
- GitHub Issue Templates (`bug_report.yml`, `feature_request.yml`).
- "Don't show on launch" checkbox on the About overlay; preference persists in `%APPDATA%/DRIFT/DRIFT.settings`.
- `OPT` button on the chassis header to open the Options menu (so VST3 users aren't stuck on the `O` hotkey).
- `i` info button on the header to re-open the About overlay at any time.
- Etched "itsELLIOTT" maker's-plate on the bottom bezel, in the themable accent colour.
- `HOLD` button + `H` hotkey — dedicated dispatch for OVERLOAD (MOD), buffer-freeze (SHORT/LONG), DELETE (LOOP).
- Loop carry-over: cycling MODE from LONG to LOOP preserves the buffer as the initial loop.
- Loop-playback signal chain: HP/LP roll-off + TILT + VOICING + soft saturate on the read tap.
- Input-stage soft clip catches hot signals before the preamp.

### Changed
- Bottom chassis grew 24 px to host the silver bezel and signature.
- SQUARE motion now slews the LFO ~3 ms to remove the read-position click.
- DIGITAL state S&H gated to `texture > 0.4` so default presets stay transparent.
- SATURATED limiter replaced with a static tanh waveshaper (no audio-rate gain modulation).
- 2-pole feedback HF damping at -12 dB/oct instead of -6 dB/oct.
- Cluster zone weights halved so the wet path doesn't permanently saturate.
- Pre-emphasis shelf reduced; matching de-emph on output to stay flat.
- DRY CLEAN actually routes the dry signal around the preamp now.
- Bypass footswitch reverted to simple toggle (gestures moved to dedicated HOLD button).
- LINK rebranded to **LINQ**; status indicator is the pill's colour (off / amber / green / red).
- 35 factory presets ship with the engine (was 10).

### Fixed
- Dry path no longer collapses stereo via MISO when LO-FI L-only detection fires.
- "Bit-crush layer" audible in DIGITAL-state default presets — S&H now off-by-default.
- High-end buzz tracking with the FEEDBACK fader.
- Various clicks/artifacts from sample-and-hold aliasing folding into audible range.

## [1.0.0] - 2026-05-25

### Added
- First public release.
- Six motorised faders: COLOR, TIME, CLUSTER, TILT, FEEDBACK, WET.
- SHIFT Alt Menu (TEXTURE, RATE, DEPTH, CROSSOVER, DIFFUSE, DRY).
- Four MODE ranges: MOD / SHORT / LONG / LOOP.
- Four STATE characters: DIGITAL / COMPRESSED / SATURATED / BIAS.
- Four VOICINGs: HIFI / FOCUS / WARM / ANALOG.
- Four SCALE quantisations + three MOTION shapes + three SPREAD modes.
- CLUSTER three-zone behaviour (synced multi-tap, scattered, drifting diffusion).
- Schroeder DIFFUSE allpass network.
- MIDI Clock in/out (24 PPQ).
- Program Change (PC 0-29 mapped to preset slots).
- CC for every fader and cycle button (CC 14-32).
- LINQ shared-memory link to SP·L.
- Options Menu: TRAILS / DRY KILL / DRY CLEAN / SCALE IGNORE / STEP / CLOCK OUT.
