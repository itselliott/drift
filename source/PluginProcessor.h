#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>

#include "DriftLink.h"

//==============================================================================
/**
    DRIFT — hybrid analog/digital echo, modelled on the Chase Bliss × EAE
    Big Time pedal (see [[reference-bigtime-spec]] in agent memory).

    INPUT ▶ analog preamp (COLOR + optional +12 dB boost) ▶ digital delay
    ▶ analog limiter (state-dependent: DIGITAL / COMPRESSED / SATURATED /
    #!&%) ▶ voicing filter chain ▶ tilt EQ ▶ wet/dry → OUTPUT.

    Six "motorised" faders: COLOR / TIME / CLUSTER / TILT / FEEDBACK / WET.
    Five cycle buttons: SCALE / MOTION / MODE / VOICING / STATE.
    One SHIFT button — enters Alt Menu where the six faders rebind to
    TEXTURE / RATE / DEPTH / CROSSOVER / DIFFUSE / DRY and the five cycle
    buttons rebind to SPREAD / 0.5X / DIFFUSE TYPE / +12 dB / (Options).
    Two footswitches with mode-dependent gestures.
*/
class DriftAudioProcessor : public juce::AudioProcessor
{
public:
    DriftAudioProcessor();
    ~DriftAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "DRIFT"; }
    bool   acceptsMidi() const override  { return true; }
    bool   producesMidi() const override { return true; }   // optional MIDI Clock out
    bool   isMidiEffect() const override { return false; }
    // Long tail accommodates LONG-mode echoes (up to 12.2 s) + cluster +
    // diffusion + bypass-trails decay. 30 s is comfortably above the
    // longest audible tail while still bounded.
    double getTailLengthSeconds() const override { return 30.0; }

    int  getNumPrograms() override    { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // --- Parameters (0..1 normalised) ----------------------------------------
    void  setColor    (float v) noexcept { color   .store (juce::jlimit (0.0f, 1.0f, v)); }
    void  setTime     (float v) noexcept { time    .store (juce::jlimit (0.0f, 1.0f, v)); }
    void  setCluster  (float v) noexcept { cluster .store (juce::jlimit (0.0f, 1.0f, v)); }
    void  setTilt     (float v) noexcept { tilt    .store (juce::jlimit (0.0f, 1.0f, v)); }
    void  setFeedback (float v) noexcept { feedback.store (juce::jlimit (0.0f, 1.0f, v)); }
    void  setWet      (float v) noexcept { wet     .store (juce::jlimit (0.0f, 1.0f, v)); }

    float getColor()    const noexcept { return color   .load(); }
    float getTime()     const noexcept { return time    .load(); }
    float getCluster()  const noexcept { return cluster .load(); }
    float getTilt()     const noexcept { return tilt    .load(); }
    float getFeedback() const noexcept { return feedback.load(); }
    float getWet()      const noexcept { return wet     .load(); }

    // --- MODE (4) and STATE (4 — DIGITAL/COMPRESSED/SATURATED/#!&%) ----------
    static constexpr int kNumModes  = 4;
    static constexpr int kNumStates = 4;
    enum Mode  { ModeMod = 0, ModeShort = 1, ModeLong = 2, ModeLoop = 3 };
    enum State { StateDigital = 0, StateCompressed = 1, StateSaturated = 2, StateBias = 3 };

    void setMode  (int m) noexcept { mode .store (juce::jlimit (0, kNumModes  - 1, m)); }
    int  getMode() const  noexcept { return mode .load(); }
    void setState (int s) noexcept { state.store (juce::jlimit (0, kNumStates - 1, s)); }
    int  getState() const noexcept { return state.load(); }
    static const char* getModeLabel  (int m) noexcept;
    static const char* getStateLabel (int s) noexcept;

    // --- SCALE / MOTION / VOICING cycle buttons ------------------------------
    static constexpr int kNumScales       = 4;
    static constexpr int kNumMotionShapes = 4;
    static constexpr int kNumVoicings     = 4;
    static constexpr int kNumSpreadModes  = 3;
    enum Scale   { ScaleOff = 0, ScaleChromatic, ScaleOct45, ScaleOctave };
    enum Motion  { MotionOff = 0, MotionSine, MotionSquare, MotionEnv };
    enum Voicing { VoicingHiFi = 0, VoicingFocus, VoicingWarm, VoicingAnalog };
    enum Spread  { SpreadOff = 0, SpreadSubtle, SpreadPingPong };

    void setScale   (int v) noexcept { scaleMode.store (juce::jlimit (0, kNumScales       - 1, v)); }
    int  getScale() const noexcept   { return scaleMode.load(); }
    void setMotion  (int v) noexcept { motionType.store (juce::jlimit (0, kNumMotionShapes - 1, v)); }
    int  getMotion() const noexcept  { return motionType.load(); }
    void setVoicing (int v) noexcept { voicing.store   (juce::jlimit (0, kNumVoicings     - 1, v)); }
    int  getVoicing() const noexcept { return voicing.load(); }
    void setSpreadMode (int v) noexcept { spreadMode.store (juce::jlimit (0, kNumSpreadModes - 1, v)); }
    int  getSpreadMode() const noexcept { return spreadMode.load(); }
    static const char* getScaleLabel   (int v) noexcept;
    static const char* getMotionLabel  (int v) noexcept;
    static const char* getVoicingLabel (int v) noexcept;
    static const char* getSpreadLabel  (int v) noexcept;

    // --- Alt-menu (SHIFT) fader bindings -------------------------------------
    // COLOR  → TEXTURE   (state-dependent character knob)
    // TIME   → motionRate
    // CLUSTER→ motionDepth (kept as the existing modDepth atomic)
    // TILT   → crossover (TILT-EQ pivot frequency)
    // FB     → diffuse
    // WET    → dry level
    void  setTexture    (float v) noexcept { texture   .store (juce::jlimit (0.0f, 1.0f, v)); }
    float getTexture() const noexcept      { return texture   .load(); }
    void  setMotionRate (float v) noexcept { motionRate.store (juce::jlimit (0.0f, 1.0f, v)); }
    float getMotionRate() const noexcept   { return motionRate.load(); }
    void  setModDepth   (float v) noexcept { modDepth  .store (juce::jlimit (0.0f, 1.0f, v)); }
    float getModDepth() const noexcept     { return modDepth  .load(); }
    void  setCrossover  (float v) noexcept { crossover .store (juce::jlimit (0.0f, 1.0f, v)); }
    float getCrossover() const noexcept    { return crossover .load(); }
    void  setDiffuse    (float v) noexcept { diffuse   .store (juce::jlimit (0.0f, 1.0f, v)); }
    float getDiffuse() const noexcept      { return diffuse   .load(); }
    void  setDryLevel   (float v) noexcept { dryLevel  .store (juce::jlimit (0.0f, 1.0f, v)); }
    float getDryLevel() const noexcept     { return dryLevel  .load(); }

    // --- Alt-menu (SHIFT) cycle-button bindings ------------------------------
    // SCALE  → SPREAD (handled by setSpreadMode above)
    // MOTION → 0.5X (toggle)
    // MODE   → DIFFUSE TYPE (toggle)
    // VOICING→ +12 dB (toggle)
    void  setBitCrush05 (bool b) noexcept { bitCrush05X.store (b); }
    bool  is05XActive() const noexcept    { return bitCrush05X.load(); }
    void  setDiffuseType (bool b) noexcept { diffuseType.store (b); }
    bool  isDiffuseTypeDoubled() const noexcept { return diffuseType.load(); }
    void  setPreampBoost (bool b) noexcept { preampBoost.store (b); }
    bool  isPreampBoosted() const noexcept { return preampBoost.load(); }

    // --- Tap-tempo plumbing --------------------------------------------------
    // When TAP TEMPO fires (in SHORT/LONG), the editor sets these to lock the
    // delay's centre to the tap interval. With tap active, the TIME fader
    // maps to ±octave around the tap centre (snap-to-centre lets the user
    // adjust freely and return). Press MODE to deactivate tap tempo.
    void   setTapCentreSeconds (double s) noexcept { tapCentreSeconds.store (juce::jlimit (0.001, 30.0, s)); }
    double getTapCentreSeconds() const noexcept    { return tapCentreSeconds.load(); }
    void   setTapTempoActive (bool b) noexcept     { tapTempoActive.store (b); }
    bool   isTapTempoActive() const noexcept       { return tapTempoActive.load(); }

    // --- Looper (LOOP mode) --------------------------------------------------
    // State machine: Stopped → Recording → Playing → Overdubbing → Playing…
    // TAP in LOOP cycles states. HOLD-right in LOOP = delete.
    // Buffer reuses the main delay buffer; max length tracks TIME fader
    // resolution per the Big Time manual (12 s at high res → 3.2 min at low).
    enum LooperState { LoopStopped = 0, LoopRecording, LoopPlaying, LoopOverdubbing };
    void setLooperState (int s) noexcept { looperState.store (juce::jlimit (0, 3, s)); }
    int  getLooperState() const noexcept { return looperState.load(); }
    int  getLoopLengthSamples() const noexcept { return loopLengthSamples.load(); }
    int  getLoopPlayPos() const noexcept       { return loopPlayPos.load(); }

    // Editor → audio-thread DELETE request. Audio thread clears the loop
    // buffer + resets state on its next block.
    void requestLoopDelete() noexcept { loopDeletePending.store (true); }

    // Set by the editor when the MODE cycle button transitions from LONG
    // to LOOP — the audio thread carries the LONG-mode delay buffer over
    // as the initial loop on the next block. Preset loads INTENTIONALLY
    // don't set this so LOOP-mode presets start idle (LoopStopped),
    // waiting for the user to press TAP to begin recording.
    void requestLoopCarryover() noexcept { loopCarryoverPending.store (true); }

    // Bypass-footswitch gesture dispatch — called from the editor's
    // hold-detect callbacks. The audio thread reads overloadActive /
    // holdActive each block and adjusts COLOR/FEEDBACK accordingly.
    void setOverloadActive (bool active) noexcept { overloadActive.store (active); }
    void setHoldActive     (bool active) noexcept { holdActive.store (active); }
    bool isOverloadActive() const noexcept { return overloadActive.load(); }
    bool isHoldActive    () const noexcept { return holdActive.load(); }
    // Editor → audio-thread cycle gesture (TAP in LOOP mode). The audio
    // thread advances the state machine on its next block.
    void requestLoopCycle() noexcept  { loopCyclePending.store (true); }

    // --- Bypass --------------------------------------------------------------
    void setBypassed (bool b) noexcept { bypass.store (b); }
    bool isBypassed() const noexcept   { return bypass.load(); }

    // --- LINK: pull audio from a companion standalone (SP·L) via shared
    // memory instead of the host audio input. When LINK is ON and the
    // producer (SP·L) is alive, the input buffer is overwritten with the
    // linked audio at the top of processBlock. When the producer is stale
    // or LINK is OFF, the host input is used as normal.
    void  setLinkEnabled (bool b) noexcept { linkEnabled.store (b); }
    bool  isLinkEnabled() const noexcept   { return linkEnabled.load(); }
    // UI status helpers (lock-free reads of the shared header).
    bool   isLinkProducerAlive() const noexcept   { return linkConsumer.isProducerAlive(); }
    double getLinkProducerSampleRate() const noexcept { return linkConsumer.getProducerSampleRate(); }
    bool   isLinkSampleRateMatched() const noexcept
    {
        const double sr = linkConsumer.getProducerSampleRate();
        return sr > 0.0 && std::abs (sr - hostSampleRate) < 0.5;
    }

    // --- Tempo (internal BPM, used for tap tempo; host playhead overrides) ---
    void   setBpm (double v) noexcept { internalBpm.store (juce::jlimit (40.0, 240.0, v)); }
    double getBpm() const noexcept    { return internalBpm.load(); }

    // Tail-length of the current effective delay time, in milliseconds —
    // the editor shows this in the OLED. Computed in the audio thread per
    // mode + time atomic and stored here for lock-free read.
    float getCurrentDelayMs() const noexcept { return currentDelayMs.load(); }

    // Peak level (0..1) of the input bus over the last processBlock — for a
    // small VU on the chassis so the user can see audio is arriving.
    float getInputPeakLevel() const noexcept { return inputPeakLevel.load(); }
    int   getInputChannelCount() const noexcept { return inputChannelCount.load(); }

    // MIDI CC map. CC 14-19 cover the 6 main faders (unchanged from Phase 1).
    // CC 20-23 cycle the four buttons that have discrete states; CC 24-30
    // expose the alt-menu params for per-knob automation; CC 31 toggles
    // bypass and CC 32 triggers tap-tempo. PROGRAM CHANGE (0xC) recalls
    // presets 0..9. MIDI CLOCK (0xF8) updates the internal BPM.
    static constexpr int kCcColor       = 14;
    static constexpr int kCcTime        = 15;
    static constexpr int kCcCluster     = 16;
    static constexpr int kCcTilt        = 17;
    static constexpr int kCcFeedback    = 18;
    static constexpr int kCcWet         = 19;
    static constexpr int kCcScale       = 20;
    static constexpr int kCcMotion      = 21;
    static constexpr int kCcMode        = 22;
    static constexpr int kCcVoicing     = 23;
    static constexpr int kCcStateBtn    = 24;
    static constexpr int kCcMotionRate  = 25;
    static constexpr int kCcMotionDepth = 26;
    static constexpr int kCcTexture     = 27;
    static constexpr int kCcCrossover   = 28;
    static constexpr int kCcDiffuse     = 29;
    static constexpr int kCcDryLevel    = 30;
    static constexpr int kCcBypass      = 31;
    static constexpr int kCcTapTempo    = 32;

    // Reset every fader / mode / state to factory defaults.
    void resetAllParameters();

    // ---- Preset bank (10 internal slots) ------------------------------------
    static constexpr int kNumPresets = 35;
    void savePresetSlot (int slot);
    bool loadPresetSlot (int slot);
    bool isPresetSlotFilled (int slot) const noexcept;
    // Last successfully-loaded slot, for the UI to display "Preset 3".
    // -1 = no preset has been loaded since launch (or it was reset).
    int  getCurrentPresetSlot() const noexcept { return currentPresetSlot.load(); }

    // Factory preset definitions — 10 hand-picked starting points that each
    // exercise a different corner of the feature set. Installed into empty
    // preset slots on prepareToPlay; user can save over them.
    struct PresetSpec
    {
        const char* name;
        float color, time, cluster, tilt, feedback, wet;
        int   mode, state;
        float modDepth;
        int   scale, motion, voicing, spreadMode;
        float texture, motionRate, crossover, diffuse, dryLevel;
        bool  bitCrush05X, diffuseType, preampBoost;
    };
    static const PresetSpec& getFactoryPreset      (int slot) noexcept;
    static const char*       getFactoryPresetName  (int slot) noexcept;

    // Apply a PresetSpec to the live atomics (used by installFactoryPresets
    // and exposed so the editor can also "preview" a factory spec without
    // touching the slot's saved blob).
    void applyPresetSpec (const PresetSpec& spec);
    // Fills any empty preset slot with the corresponding factory spec.
    // Idempotent given a stable factoryPresetsInstalled flag. Called from
    // prepareToPlay so the user can also save over a factory preset and
    // have their override persist instead of being re-baked.
    void installFactoryPresetsIfEmpty();

    // ---- Options Menu toggles (per Big Time manual) --------------------------
    // These behaviours are simple flags wired into the DSP / signal flow:
    //   TRAILS     — let the wet tail decay after bypass (vs hard cut)
    //   DRY KILL   — output the wet signal only, no dry through
    //   DRY CLEAN  — dry path bypasses the preamp's saturation
    //   SCALE IGNORE — MOTION ignores SCALE (stays smooth even when SCALE is on)
    //   STEP       — TAP footswitch creates momentary movement bursts
    void setTrails       (bool b) noexcept { optTrails.store (b); }
    void setDryKill      (bool b) noexcept { optDryKill.store (b); }
    void setDryClean     (bool b) noexcept { optDryClean.store (b); }
    void setScaleIgnore  (bool b) noexcept { optScaleIgnore.store (b); }
    void setStepMode     (bool b) noexcept { optStep.store (b); }
    bool isTrailsOn()        const noexcept { return optTrails.load(); }
    bool isDryKillOn()       const noexcept { return optDryKill.load(); }
    bool isDryCleanOn()      const noexcept { return optDryClean.load(); }
    bool isScaleIgnoreOn()   const noexcept { return optScaleIgnore.load(); }
    bool isStepModeOn()      const noexcept { return optStep.load(); }

    // ---- MIDI Clock OUT ------------------------------------------------------
    // Emit 24 PPQ clock messages (0xF8) on the MIDI output bus. Useful for
    // syncing downstream pedals/DAW slaves to DRIFT's internal BPM.
    void setMidiClockOut (bool b) noexcept { optClockOut.store (b); }
    bool isMidiClockOutOn() const noexcept { return optClockOut.load(); }

    // Tail-out check: lets the standalone session-restore safely skip a
    // saved-state load if the file is corrupt without throwing.
    static juce::File getLastSessionFile();

private:
    //==============================================================================
    // --- Audio-thread DSP helpers --------------------------------------------
    // 200 s of stereo float buffer = ~73 MB. Covers MOD/SHORT/LONG with
    // room to spare AND the looper's 3.2-min max length.
    static constexpr float kMaxDelaySeconds = 200.0f;
    static constexpr int   kMaxChannels     = 2;
    static constexpr int   kNumClusterTaps  = 4;

    // Read a fractional-sample value from the delay buffer with linear
    // interpolation. delaySamples is the read OFFSET behind writePos (always
    // positive; >= 1 to avoid reading the just-written sample).
    static float readDelay (const float* buf, int bufLen,
                            int writePos, double delaySamples) noexcept;

    // Map normalised time (0..1) → delay seconds, mode-dependent. Uses
    // exponential mapping inside each range so the knob feel is musical.
    // When tap-tempo is active, the mapping switches to ±octave around the
    // tap centre (snap-to-centre = the tapped delay). When SCALE is on,
    // the fader quantises to musical-interval ratios (chromatic / oct+4+5 /
    // octave) of the centre.
    static double timeForMode (int mode, float normTime,
                               double tapCentreSec, bool tapActive,
                               int scaleMode) noexcept;

    // ---- State --------------------------------------------------------------
    // 6 user-facing fader atomics. Defaults form a TRUE PASSTHROUGH: with
    // wet=0 the wet/dry crossfade outputs pure dry signal regardless of
    // anything else, so a freshly-loaded DRIFT has zero audible effect on
    // the host input until the user starts rolling faders up.
    std::atomic<float>  color    { 0.00f };
    std::atomic<float>  time     { 0.50f };
    std::atomic<float>  cluster  { 0.00f };
    std::atomic<float>  tilt     { 0.50f };
    std::atomic<float>  feedback { 0.00f };
    std::atomic<float>  wet      { 0.00f };

    // Mode + state cycle buttons.
    std::atomic<int>    mode     { ModeShort };
    std::atomic<int>    state    { StateSaturated };   // Big Time's "default" state

    // Cycle buttons (Scale / Motion / Voicing) + Spread (alt of Scale).
    std::atomic<int>    scaleMode  { ScaleOff };
    std::atomic<int>    motionType { MotionOff };
    std::atomic<int>    voicing    { VoicingHiFi };
    std::atomic<int>    spreadMode { SpreadOff };

    // Alt-menu fader params.
    std::atomic<float>  texture    { 0.50f };
    std::atomic<float>  motionRate { 0.30f };
    std::atomic<float>  modDepth   { 0.50f };   // bound to CLUSTER fader in alt
    std::atomic<float>  crossover  { 0.50f };
    std::atomic<float>  diffuse    { 0.00f };
    std::atomic<float>  dryLevel   { 1.00f };

    // Alt-menu cycle-button toggles.
    std::atomic<bool>   bitCrush05X { false };  // 0.5X (12-bit / 24 kHz)
    std::atomic<bool>   diffuseType { false };  // doubles DIFFUSE strength
    std::atomic<bool>   preampBoost { false };  // +12 dB

    // Tap-tempo plumbing.
    std::atomic<double> tapCentreSeconds { 0.250 };
    std::atomic<bool>   tapTempoActive   { false };

    // Looper (LOOP mode).
    std::atomic<int>    looperState        { LoopStopped };
    std::atomic<int>    loopLengthSamples  { 0 };
    std::atomic<int>    loopPlayPos        { 0 };
    std::atomic<bool>   loopDeletePending  { false };
    std::atomic<bool>   loopCarryoverPending { false };
    std::atomic<bool>   loopCyclePending   { false };
    // Audio-thread only:
    int loopRecordStartPos = 0;

    // Preset bank — 10 MemoryBlocks, each holding a serialised state. The
    // bank itself is persisted in get/setStateInformation so the whole
    // session survives DAW save/load.
    juce::MemoryBlock  presetSlots [kNumPresets] {};
    std::atomic<int>   currentPresetSlot { -1 };
    bool               factoryPresetsInstalled = false;

    // Options Menu toggles.
    std::atomic<bool>  optTrails       { false };
    std::atomic<bool>  optDryKill      { false };
    std::atomic<bool>  optDryClean     { false };
    std::atomic<bool>  optScaleIgnore  { false };
    std::atomic<bool>  optStep         { false };
    std::atomic<bool>  optClockOut     { false };
    // MIDI Clock output state — sub-sample accumulator so emitted ticks
    // land at the right sample offset within each processBlock.
    double midiClockOutAccum = 0.0;

    // Bypass + tempo.
    std::atomic<bool>   bypass      { false };
    std::atomic<bool>   linkEnabled { false };
    std::atomic<double> internalBpm { 120.0 };

    // Big Time gesture states — driven by the bypass footswitch hold-detect.
    //   overloadActive : MOD-mode hold. While true, processBlock forces
    //                    colorT and feedbackT to 1.0 for momentary chaos.
    //   holdActive     : SHORT/LONG-mode hold. While true, processBlock
    //                    stops writing fresh audio into the delay buffer and
    //                    forces feedbackSm to ~1.0 so existing content
    //                    recirculates indefinitely.
    std::atomic<bool>   overloadActive { false };
    std::atomic<bool>   holdActive     { false };

    // Shared-memory consumer pulling audio from SP·L (or any future
    // DriftLink-aware producer).
    DriftLink::Consumer linkConsumer;

    // Read-only meters for the editor.
    std::atomic<float>  currentDelayMs    { 0.0f };
    std::atomic<float>  inputPeakLevel    { 0.0f };
    std::atomic<int>    inputChannelCount { 0 };

    // ---- Audio-thread DSP state ---------------------------------------------
    double  hostSampleRate = 48000.0;
    float   smoothCoef     = 0.01f;     // 1-pole smoothing TC (computed in prepareToPlay)

    // Smoothed targets — never used outside processBlock.
    float   colorSm      = 0.30f;
    double  delaySm      = 24000.0;       // smoothed delay length in fractional samples
    float   clusterSm    = 0.0f;
    float   tiltSm       = 0.5f;
    float   feedbackSm   = 0.4f;
    float   wetSm        = 0.0f;
    float   modDepthSm   = 0.0f;
    // Phase 2 alt params, smoothed.
    float   textureSm    = 0.5f;
    float   motionRateSm = 0.3f;
    float   crossoverSm  = 0.5f;
    float   diffuseSm    = 0.0f;
    float   dryLevelSm   = 1.0f;

    // Envelope follower — used by ENV motion shape. Tracks input + delay
    // loop peak; rising edges advance the motion step.
    float   envFollowerSm = 0.0f;
    float   envBaselineSm = 0.0f;
    int     envStepIndex  = 0;
    bool    envTransientArmed = true;
    // Slewed value of the current step's sine-table position. ENV motion
    // is event-driven (transient bumps stepIndex); this glides smoothly
    // between steps, with the glide speed taken from the RATE alt-control.
    float   envStepValueSm = 0.0f;

    // Slewed LFO output — SQUARE motion would otherwise jump the delay tap
    // by 6% instantly, causing an audible click at the buffer-read shift.
    // A ~3 ms slew turns the square step into a fast ramp so the read
    // position transitions smoothly without losing the square character.
    float   lfoValSm = 0.0f;

    // Loop-playback band-pass filter state — HP at ~80 Hz removes DC and
    // rumble, LP at ~12 kHz removes HF artifacts from the raw buffer
    // contents. Applied only on LOOP-mode playback so the loop sounds
    // smooth and clean, not raw.
    float   loopHpState [kMaxChannels] {};
    float   loopHpPrev  [kMaxChannels] {};
    float   loopLpState [kMaxChannels] {};

    // Tracks the mode used by the previous processBlock so we can detect
    // a transition into LOOP mode. On that transition, if the looper is
    // idle, we carry the LONG-mode buffer over as the initial loop
    // (per the Big Time manual — "LOOP mode carries audio over from LONG
    // mode on entry"). Initialised to -1 so the very first block doesn't
    // count as a transition.
    int     prevMode = -1;

    // DIFFUSE allpass — Schroeder-style stereo diffuser for the DIFFUSE
    // alt-control. State lives in `diffuseBuf` declared above.
    static constexpr int kDiffuseDelayL = 1543;
    static constexpr int kDiffuseDelayR = 1727;
    static constexpr float kDiffuseK   = 0.62f;

    // Mono-in-stereo-out auto-detect — smoothed crossfade so a flickering
    // R-channel noise floor doesn't toggle the broadcast on/off every block
    // (which would crack the audio). 0 = full stereo, 1 = mono broadcast.
    float monoBlendSm    = 0.0f;
    bool  monoDetectHyst = false;     // current hysteretic state of detection

    // VOICING — 4 fixed filter arrangements that sit AFTER the delay read.
    //   HIFI:   pass-through
    //   FOCUS:  gentle band-pass (cut highs and lows simultaneously over time)
    //   WARM:   PCM-style elliptical-ripple emulation (1-pole LP at ~4 kHz)
    //   ANALOG: BBD-style aggressive HF rolloff + low-shelf cut
    float voicingLpState [kMaxChannels] {};
    float voicingHpState [kMaxChannels] {};
    float voicingHpPrev  [kMaxChannels] {};

    // Diffuse network — 2 short allpass-style modulated delays per channel
    // for the DIFFUSE alt control. Phase 2 wires a simple lattice; full
    // reverb-style diffusion lands in Phase 3.
    static constexpr int kDiffuseLen = 2048;
    float diffuseBuf [kMaxChannels][kDiffuseLen] {};
    int   diffuseWritePos = 0;

    // Stereo delay buffer (one juce::AudioBuffer<float> with two channels).
    juce::AudioBuffer<float> delayBuffer;
    int                      delayBufLen = 0;
    int                      writePos    = 0;

    // Preamp filter state (per-channel 1-pole DC blocker + HF rolloff +
    // low-shelf BASS state for the JFET-style body boost).
    float preampHpState[kMaxChannels] {};
    float preampHpPrev [kMaxChannels] {};
    float preampLpState[kMaxChannels] {};
    float preampBassState[kMaxChannels] {};

    // Pre/de-emphasis filters around the delay buffer — boost HF on write,
    // cut HF on read. Mismatched coefficients leave a "sparkle" that's
    // characteristic of the PCM 42 / SDD-3000 converter chains.
    float preEmphHpState[kMaxChannels] {};
    float preEmphHpPrev [kMaxChannels] {};
    float deEmphHpState [kMaxChannels] {};
    float deEmphHpPrev  [kMaxChannels] {};

    // Tilt EQ state — single 1-pole low-pass + 1-pole high-pass per channel.
    // Tilt knob crossfades between them: <0.5 = lp dominant (darken),
    // >0.5 = hp dominant (brighten), 0.5 = flat.
    float tiltLpState[kMaxChannels] {};
    float tiltHpState[kMaxChannels] {};
    float tiltHpPrev [kMaxChannels] {};

    // Feedback limiter state — peak envelope follower per channel + DC bias
    // creep (used in StateBias mode).
    float limiterEnv  [kMaxChannels] {};
    float biasDcState [kMaxChannels] {};

    // HF damping lowpass in the feedback path. The Schroeder/Moorer reverb
    // literature is unanimous: without an LP in the feedback loop, every
    // echo is as bright as the original — that's what made our high-FB
    // presets sound harsh. This 1-pole LP softens each repeat, so over
    // many feedback passes the tail gets progressively darker — the
    // "creamy decay" character every dreamy reverb relies on.
    float fbDampState  [kMaxChannels] {};
    float fbDampState2 [kMaxChannels] {};   // 2nd cascaded LP for -12 dB/oct HF rolloff

    // Sample-rate-crush state (subtle digital-vintage character — always on).
    float srrAccum                    = 0.0f;
    float srrHold [kMaxChannels]      {};

    // Modulation LFO — drives delay-time vibrato in MOD mode.
    double lfoPhase = 0.0;

    // MIDI Clock derivation — averaged inter-tick interval over the last 8
    // 0xF8 messages gives a smooth BPM. 24 PPQ → BPM = 60000 / (avg ms * 24).
    double midiClockLastMs       = 0.0;
    double midiClockIntervalsMs[8] {};
    int    midiClockHistCount    = 0;
    int    midiClockHistIdx      = 0;
    // Editor → audio-thread tap-tempo trigger (via MIDI CC 32). Audio
    // thread fires a tap-tempo update on its next block.
    std::atomic<bool> midiTapPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftAudioProcessor)
};
