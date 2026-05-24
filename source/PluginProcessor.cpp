#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
DriftAudioProcessor::DriftAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

DriftAudioProcessor::~DriftAudioProcessor() = default;

//==============================================================================
const char* DriftAudioProcessor::getModeLabel (int m) noexcept
{
    switch (m)
    {
        case ModeMod:   return "MOD";
        case ModeShort: return "SHORT";
        case ModeLong:  return "LONG";
        case ModeLoop:  return "LOOP";
        default:        return "?";
    }
}

const char* DriftAudioProcessor::getStateLabel (int s) noexcept
{
    switch (s)
    {
        case StateDigital:    return "DIGITAL";
        case StateCompressed: return "COMP";
        case StateSaturated:  return "SAT";
        case StateBias:       return "BIAS";
        default:              return "?";
    }
}

const char* DriftAudioProcessor::getScaleLabel (int v) noexcept
{
    switch (v)
    {
        case ScaleOff:       return "OFF";
        case ScaleChromatic: return "CHROM";
        case ScaleOct45:     return "4-5";
        case ScaleOctave:    return "OCT";
        default:             return "?";
    }
}

const char* DriftAudioProcessor::getMotionLabel (int v) noexcept
{
    switch (v)
    {
        case MotionOff:    return "OFF";
        case MotionSine:   return "SINE";
        case MotionSquare: return "SQR";
        case MotionEnv:    return "ENV";
        default:           return "?";
    }
}

const char* DriftAudioProcessor::getVoicingLabel (int v) noexcept
{
    switch (v)
    {
        case VoicingHiFi:   return "HIFI";
        case VoicingFocus:  return "FOCUS";
        case VoicingWarm:   return "WARM";
        case VoicingAnalog: return "ANALOG";
        default:            return "?";
    }
}

const char* DriftAudioProcessor::getSpreadLabel (int v) noexcept
{
    switch (v)
    {
        case SpreadOff:      return "MONO";
        case SpreadSubtle:   return "WIDE";
        case SpreadPingPong: return "PP";
        default:             return "?";
    }
}

double DriftAudioProcessor::timeForMode (int mode, float normTime,
                                         double tapCentreSec, bool tapActive,
                                         int scaleMode) noexcept
{
    const double t = juce::jlimit (0.0, 1.0, (double) normTime);

    // Mode-specific min/max range — the bounds for the smooth and tap paths.
    double lo = 0.046, hi = 0.736;
    switch (mode)
    {
        case ModeMod:   lo = 0.003;  hi = 0.046;  break;
        case ModeShort: lo = 0.046;  hi = 0.736;  break;
        case ModeLong:  lo = 0.736;  hi = 12.200; break;
        case ModeLoop:  lo = 1.000;  hi = 29.500; break;  // proper looper lands in Phase 3
        default: break;
    }

    // Centre delay around which musical-step quantisation / tap-tempo work.
    const double centre = (tapActive && tapCentreSec > 0.0)
                            ? tapCentreSec
                            : std::sqrt (lo * hi);   // geometric mid of the range

    // SCALE quantisation — TIME snaps to musical-interval ratios of the
    // centre. Number of steps depends on the scale.
    if (scaleMode != ScaleOff)
    {
        double ratio = 1.0;
        if (scaleMode == ScaleChromatic)
        {
            // 25 steps: −12 .. +12 semitones around the centre.
            const int idx = (int) std::round (t * 24.0) - 12;
            ratio = std::pow (2.0, idx / 12.0);
        }
        else if (scaleMode == ScaleOct45)
        {
            // 9 steps centred on unison: ±2 oct, ±5th, ±4th, unison.
            static const double scaleRatios [9] = {
                0.25, 1.0 / 1.5, 0.75, 1.0 / 1.3333333, 1.0,
                1.3333333, 1.5, 2.0, 4.0
            };
            const int idx = (int) std::round (t * 8.0);
            ratio = scaleRatios [juce::jlimit (0, 8, idx)];
        }
        else // ScaleOctave
        {
            // 5 steps: −2, −1, 0, +1, +2 octaves.
            static const double octRatios [5] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
            const int idx = (int) std::round (t * 4.0);
            ratio = octRatios [juce::jlimit (0, 4, idx)];
        }
        return juce::jlimit (lo * 0.25, hi * 4.0, centre * ratio);
    }

    // Tap-tempo, smooth: ±octave around tap centre.
    if (tapActive && tapCentreSec > 0.0)
        return tapCentreSec * std::pow (2.0, (t - 0.5) * 2.0);

    // Otherwise, exponential mapping inside the mode's range.
    return lo * std::pow (hi / lo, t);
}

float DriftAudioProcessor::readDelay (const float* buf, int bufLen,
                                      int writePos, double delaySamples) noexcept
{
    if (bufLen <= 0) return 0.0f;
    if (! std::isfinite (delaySamples) || delaySamples < 1.0)
        delaySamples = 1.0;
    if (delaySamples > (double) (bufLen - 1))
        delaySamples = (double) (bufLen - 1);

    const double readPosF = (double) writePos - delaySamples;
    double wrapped = readPosF;
    while (wrapped < 0.0)            wrapped += (double) bufLen;
    while (wrapped >= (double) bufLen) wrapped -= (double) bufLen;

    const int i0 = (int) wrapped;
    const int i1 = (i0 + 1) % bufLen;
    const float frac = (float) (wrapped - (double) i0);
    return buf[i0] + (buf[i1] - buf[i0]) * frac;
}

//==============================================================================
void DriftAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    if (! (sampleRate > 0.0)) sampleRate = 48000.0;
    hostSampleRate = sampleRate;

    // 1-pole smoothing TC of 3ms keeps fader sweeps zipper-free.
    smoothCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) sampleRate));

    delayBufLen = (int) (kMaxDelaySeconds * sampleRate) + 8;
    delayBuffer.setSize (kMaxChannels, delayBufLen, false, true, true);
    delayBuffer.clear();
    writePos = 0;

    for (int c = 0; c < kMaxChannels; ++c)
    {
        preampHpState[c]   = preampHpPrev[c]   = preampLpState[c]   = 0.0f;
        preampBassState[c] = 0.0f;
        preEmphHpState[c]  = preEmphHpPrev[c]  = 0.0f;
        deEmphHpState[c]   = deEmphHpPrev[c]   = 0.0f;
        tiltLpState[c]     = tiltHpState[c]    = tiltHpPrev[c]      = 0.0f;
        voicingLpState[c]  = voicingHpState[c] = voicingHpPrev[c]   = 0.0f;
        limiterEnv[c]      = biasDcState[c]    = 0.0f;
        fbDampState[c]     = 0.0f;
        fbDampState2[c]    = 0.0f;
        loopHpState[c]     = loopHpPrev[c] = loopLpState[c] = 0.0f;
        srrHold[c]         = 0.0f;
        std::memset (diffuseBuf[c], 0, sizeof (diffuseBuf[c]));
    }
    srrAccum         = 0.0f;
    lfoPhase         = 0.0;
    envFollowerSm    = 0.0f;
    envBaselineSm    = 0.0f;
    envStepValueSm   = 0.0f;
    lfoValSm         = 0.0f;
    envStepIndex     = 0;
    envTransientArmed = true;
    diffuseWritePos  = 0;
    monoBlendSm      = 0.0f;
    monoDetectHyst   = false;
    loopRecordStartPos = 0;
    looperState.store      (LoopStopped);
    loopLengthSamples.store (0);
    loopPlayPos.store       (0);
    midiClockLastMs    = 0.0;
    midiClockHistCount = 0;
    midiClockHistIdx   = 0;
    midiClockOutAccum  = 0.0;

    // Snap smoothed targets to current atomic values so the first block
    // doesn't sweep from 0 → setting (audible click on plugin load). Phase
    // 2 added a handful of new smoothed params — REFRESH them here too so
    // a saved-state restore picks up immediately instead of crossfading in.
    colorSm      = color.load();
    delaySm      = timeForMode (mode.load(), time.load(),
                                 tapCentreSeconds.load(),
                                 tapTempoActive.load(),
                                 scaleMode.load()) * sampleRate;
    clusterSm    = cluster.load();
    tiltSm       = tilt.load();
    feedbackSm   = feedback.load();
    wetSm        = wet.load();
    modDepthSm   = modDepth.load();
    textureSm    = texture.load();
    motionRateSm = motionRate.load();
    crossoverSm  = crossover.load();
    diffuseSm    = diffuse.load();
    dryLevelSm   = dryLevel.load();

    // Open the DriftLink shared-memory mapping. The consumer stays open the
    // whole session; LINK on/off just gates the per-block read in processBlock.
    if (! linkConsumer.isOpen())
        linkConsumer.open();
    linkConsumer.prepare();

    // First-run / fresh-state init — fill any empty preset slots with the
    // 10 factory specs. Idempotent across multiple prepareToPlay calls.
    installFactoryPresetsIfEmpty();
}

void DriftAudioProcessor::releaseResources()
{
    linkConsumer.shutdown();
    linkConsumer.close();
}

bool DriftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    // Match-in-out, mono or stereo only.
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void DriftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numChans = juce::jmin (numOut, kMaxChannels);

    inputChannelCount.store (numIn);

    // Zero any output channels that don't have a matching input bus.
    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, numSamples);

    // ---- Pull host BPM if available ---------------------------------------
    if (auto* ph = getPlayHead())
    {
        if (auto info = ph->getPosition())
        {
            if (auto bpm = info->getBpm())
                internalBpm.store (juce::jlimit (40.0, 240.0, *bpm));
        }
    }

    // ---- MIDI parsing -----------------------------------------------------
    //   Controllers   → fader atomics (CC 14-30) or commands (CC 31-32)
    //   Program Change → recall preset slot (PC 0..kNumPresets-1)
    //   Clock (0xF8)   → derive BPM from inter-tick interval
    //   We never consume the messages — downstream plugins still see them.
    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();

        if (msg.isMidiClock())
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (midiClockLastMs > 0.0)
            {
                const double interval = now - midiClockLastMs;
                // Sane range: 24 PPQ at 40..240 BPM → 6.25..62.5 ms/tick.
                if (interval > 4.0 && interval < 100.0)
                {
                    midiClockIntervalsMs[midiClockHistIdx] = interval;
                    midiClockHistIdx = (midiClockHistIdx + 1) & 7;
                    if (midiClockHistCount < 8) ++midiClockHistCount;
                    double avg = 0.0;
                    for (int i = 0; i < midiClockHistCount; ++i)
                        avg += midiClockIntervalsMs[i];
                    avg /= (double) midiClockHistCount;
                    const double newBpm = 60000.0 / (avg * 24.0);
                    internalBpm.store (juce::jlimit (40.0, 240.0, newBpm));
                }
            }
            midiClockLastMs = now;
            continue;
        }

        if (msg.isProgramChange())
        {
            const int pc = msg.getProgramChangeNumber();
            if (pc >= 0 && pc < kNumPresets)
                loadPresetSlot (pc);
            continue;
        }

        if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const int raw = msg.getControllerValue();
            const float v = (float) raw / 127.0f;
            switch (cc)
            {
                case kCcColor:       setColor       (v); break;
                case kCcTime:        setTime        (v); break;
                case kCcCluster:     setCluster     (v); break;
                case kCcTilt:        setTilt        (v); break;
                case kCcFeedback:    setFeedback    (v); break;
                case kCcWet:         setWet         (v); break;
                case kCcMotionRate:  setMotionRate  (v); break;
                case kCcMotionDepth: setModDepth    (v); break;
                case kCcTexture:     setTexture     (v); break;
                case kCcCrossover:   setCrossover   (v); break;
                case kCcDiffuse:     setDiffuse     (v); break;
                case kCcDryLevel:    setDryLevel    (v); break;
                // Discrete-state CCs — quantise the 0-127 range into the
                // right number of slots for each control.
                case kCcScale:       setScale       (raw * kNumScales       / 128); break;
                case kCcMotion:      setMotion      (raw * kNumMotionShapes / 128); break;
                case kCcMode:        setMode        (raw * kNumModes        / 128); break;
                case kCcVoicing:     setVoicing     (raw * kNumVoicings     / 128); break;
                case kCcStateBtn:    setState       (raw * kNumStates       / 128); break;
                // Commands — fire on rising edge (CC ≥ 64) so any "switch"
                // controller (sustain pedal, footswitch, etc.) works.
                case kCcBypass:      if (raw >= 64) setBypassed (! isBypassed()); break;
                case kCcTapTempo:    if (raw >= 64) midiTapPending.store (true);  break;
                default: break;
            }
        }
    }

    // Handle MIDI tap-tempo trigger from CC 32 — treat it like a footswitch
    // press by re-using internalBpm as the target. Inter-tap intervals are
    // measured on the editor side normally, but for MIDI we just trust the
    // current internalBpm (set externally) and snap TIME to centre.
    if (midiTapPending.exchange (false))
    {
        const int m = mode.load();
        if (m == ModeShort || m == ModeLong)
        {
            tapCentreSeconds.store (60.0 / juce::jmax (40.0, internalBpm.load()));
            tapTempoActive  .store (true);
            time.store (0.5f);
        }
    }

    // ---- MIDI Clock OUT — emit 24 PPQ tick messages at sample-accurate
    // offsets through the block, when the optClockOut toggle is enabled.
    // Downstream apps can slave their tempo to DRIFT this way.
    if (optClockOut.load())
    {
        const double bpm = juce::jlimit (40.0, 240.0, internalBpm.load());
        const double samplesPerTick = hostSampleRate * 60.0 / (bpm * 24.0);
        if (samplesPerTick > 1.0)
        {
            while (midiClockOutAccum < (double) numSamples)
            {
                const int sampleOffset = juce::jlimit (0, numSamples - 1,
                                                       (int) midiClockOutAccum);
                midi.addEvent (juce::MidiMessage::midiClock(), sampleOffset);
                midiClockOutAccum += samplesPerTick;
            }
            midiClockOutAccum -= (double) numSamples;
        }
    }
    else
    {
        midiClockOutAccum = 0.0;   // reset phase when disabled so re-enable starts clean
    }

    // ---- LINK: pull audio from the shared-memory ring buffer in place of
    // the host input. This MUST happen before the bypass branch — bypass
    // means "don't process," not "mute the signal." Linked audio still
    // needs to reach the output so SP-L → DRIFT (bypassed) → speakers
    // sounds like a real pedal chain with the effect taken out of circuit.
    // Falls through silently when LINK is off or the producer's heartbeat
    // is stale (Consumer::read returns 0 frames and we just keep the host
    // audio).
    if (linkEnabled.load() && linkConsumer.isOpen())
    {
        const int filled = linkConsumer.read (buffer, numChans);
        if (filled > 0 && filled < numSamples)
        {
            // Producer ran short — zero the tail so we don't smear stale
            // host audio into the link signal mid-block.
            for (int c = 0; c < numChans; ++c)
                buffer.clear (c, filled, numSamples - filled);
        }
    }

    // ---- Looper state machine — handle deferred cycle/delete gestures
    // from the editor. Done OUTSIDE the per-sample loop so each block
    // transitions cleanly with a single atomic snapshot.
    if (loopDeletePending.exchange (false))
    {
        looperState     .store (LoopStopped);
        loopLengthSamples.store (0);
        loopPlayPos     .store (0);
    }
    if (loopCyclePending.exchange (false))
    {
        const int s = looperState.load();
        switch (s)
        {
            case LoopStopped:
                looperState.store (LoopRecording);
                loopRecordStartPos = writePos;
                loopLengthSamples.store (0);
                break;
            case LoopRecording:
            {
                int len = writePos - loopRecordStartPos;
                if (len < 0) len += delayBufLen;
                // Clamp to delay-buffer capacity (looper can't exceed it).
                len = juce::jmax (1, juce::jmin (len, delayBufLen - 1));
                loopLengthSamples.store (len);
                loopPlayPos     .store (0);
                looperState     .store (LoopPlaying);
                break;
            }
            case LoopPlaying:      looperState.store (LoopOverdubbing); break;
            case LoopOverdubbing:  looperState.store (LoopPlaying);     break;
        }
    }

    // ---- Bypass: insert-fx passthrough (signal in the buffer — whether
    // from the host input bus or pulled from LINK above — passes unchanged
    // to the output). True bypass behaviour: dry path is preserved.
    //
    // TRAILS (Options Menu): when on, bypass doesn't return early. The
    // wet path keeps running with input MUTED so existing echoes fade
    // naturally through the feedback decay. The dry path is unaffected.
    const bool bypassNow         = bypass.load();
    const bool bypassTrailsActive = bypassNow && optTrails.load();
    if (bypassNow && ! bypassTrailsActive)
    {
        float peak = 0.0f;
        for (int c = 0; c < numChans; ++c)
            peak = juce::jmax (peak, buffer.getMagnitude (c, 0, numSamples));
        inputPeakLevel.store (peak);
        return;
    }

    // ---- Snapshot parameter targets ---------------------------------------
    float        colorT    = color.load();
    const int    modeT     = mode.load();
    const int    stateT    = state.load();
    const float  clusterT  = cluster.load();
    const float  tiltT     = tilt.load();
    const float  wetT      = wet.load();

    // Feedback target: in LOOP mode, force near-infinite hold (limiter
    // compressed/bias states cap the runaway). Otherwise, scale the fader
    // 0..1 → 0..0.985 (a little safety headroom in CLEAN to avoid runaway).
    float feedbackT = feedback.load();
    if (modeT == ModeLoop) feedbackT = juce::jmax (feedbackT, 0.92f);
    feedbackT = juce::jlimit (0.0f, 0.985f, feedbackT);
    // CLEAN state can't push as hard before the limiter just clamps — pull
    // back slightly so the fader's top half has musical headroom.
    // SATURATED is the "default" Big Time state and can self-oscillate at the
    // limiter; cap the feedback fader a hair below clipping. DIGITAL has no
    // limiter so it CAN run away — but the user explicitly chose that mode.
    if (stateT == StateSaturated) feedbackT = juce::jmin (feedbackT, 0.92f);

    // ---- Bypass-footswitch gesture overrides ------------------------------
    // OVERLOAD: user holding bypass in MOD mode → ramp COLOR + FEEDBACK to
    // max for momentary chaos. HOLD: user holding bypass in SHORT/LONG mode
    // → freeze the buffer (no fresh input written) and force feedback to
    // unity so the existing content recirculates as an infinite drone.
    const bool overloadOn = overloadActive.load();
    const bool holdOn     = holdActive.load();
    if (overloadOn)
    {
        colorT    = 1.0f;
        feedbackT = 0.985f;
    }
    else if (holdOn)
    {
        feedbackT = 0.985f;
    }

    const int    scaleT   = scaleMode.load();
    const int    motionT  = motionType.load();
    const int    voicingT = voicing.load();
    const float  textureT = texture.load();
    const float  motionRateT = motionRate.load();
    const float  crossoverT  = crossover.load();
    const float  diffuseT    = diffuse.load();
    const float  dryLevelT   = dryLevel.load();
    const float  motionDepthT = modDepth.load();
    const bool   bitCrush05T  = bitCrush05X.load();
    const bool   preampBoostT = preampBoost.load();
    const bool   diffuseTypeT = diffuseType.load();
    juce::ignoreUnused (diffuseTypeT);   // Phase 2A reserves the atomic; wiring lands with the diffusion network in Phase 2B.
    const double targetDelaySamps = timeForMode (modeT, time.load(),
                                                  tapCentreSeconds.load(),
                                                  tapTempoActive.load(),
                                                  scaleT) * hostSampleRate;
    currentDelayMs.store ((float) (targetDelaySamps / hostSampleRate * 1000.0));

    // LFO rate — driven by the MOTION button + RATE alt-control. Off when
    // MOTION = MotionOff, otherwise exponential mapping from motionRate
    // (0.2 Hz at 0 → 15 Hz at 1). MOD mode legacy 5 Hz default is preserved
    // when the user picks SINE in MOD without touching RATE (default 0.30
    // gives ~3.3 Hz, close enough — and the user can dial in 5 Hz exactly).
    const double lfoHz = (motionT == MotionOff)
                          ? 0.0
                          : (0.2 * std::pow (75.0, (double) motionRateT));
    const double lfoIncPerSm = juce::MathConstants<double>::twoPi * lfoHz / hostSampleRate;

    // ---- Mono-in-stereo-out auto-detect (Big Time's MISO behaviour) ------
    // A guitar plugged into the L input of a stereo interface gives us
    // signal on channel 0 and silence on channel 1. Without intervention
    // the wet path's R buffer stays empty and the user only hears feedback
    // in their L ear.
    //
    // Hysteresis: only enter mono when R is <5% of L; only exit mono when
    // R rises above 15% of L. This prevents the detector from flickering
    // each block when R is just noise-floor-hovering near the threshold.
    //
    // The actual broadcast is then crossfaded into the per-sample loop via
    // `monoBlendSm` so even a state change is glitch-free.
    float blockPeakL = 0.0f, blockPeakR = 0.0f;
    if (numChans > 0) blockPeakL = buffer.getMagnitude (0, 0, numSamples);
    if (numChans > 1) blockPeakR = buffer.getMagnitude (1, 0, numSamples);
    if (numChans > 1 && blockPeakL > 0.001f)
    {
        if (! monoDetectHyst && blockPeakR < blockPeakL * 0.05f) monoDetectHyst = true;
        if (  monoDetectHyst && blockPeakR > blockPeakL * 0.15f) monoDetectHyst = false;
    }

    // Per-block input peak for the meter.
    float blockInputPeak = juce::jmax (blockPeakL, blockPeakR);

    // Channel pointers.
    float* chanData[kMaxChannels] {};
    for (int c = 0; c < numChans; ++c)
        chanData[c] = buffer.getWritePointer (c);

    float* dlyL = delayBuffer.getWritePointer (0);
    float* dlyR = numChans > 1 ? delayBuffer.getWritePointer (1) : dlyL;
    const float* dlyLRead = delayBuffer.getReadPointer (0);
    const float* dlyRRead = numChans > 1 ? delayBuffer.getReadPointer (1) : dlyLRead;

    const float sc = smoothCoef;

    // ---- LOOP mode — dedicated looper path. The buffer functions as a
    // phrase loop (record / play / overdub) rather than a recirculating
    // delay line, so the regular delay-engine pipeline is bypassed here.
    // Wet output = the loop playback; dry path is preserved through the
    // standard crossfade with DRY level applied.
    if (modeT == ModeLoop)
    {
        // Carry-over from LONG mode — only fires when the editor explicitly
        // set loopCarryoverPending (i.e., the user pressed the MODE cycle
        // button to advance from LONG to LOOP). Preset loads do NOT trigger
        // this so LOOP-mode presets start idle (waiting for TAP to record).
        if (loopCarryoverPending.exchange (false)
            && looperState.load() == LoopStopped)
        {
            const double carryDelaySamps =
                timeForMode (ModeLong, time.load(),
                             tapCentreSeconds.load(),
                             tapTempoActive.load(),
                             scaleMode.load()) * hostSampleRate;
            int carryLen = (int) juce::jlimit (1.0,
                                               (double) (delayBufLen - numSamples - 16),
                                               carryDelaySamps);
            loopRecordStartPos = (writePos - carryLen + delayBufLen) % delayBufLen;
            loopLengthSamples.store (carryLen);
            loopPlayPos.store (0);
            looperState.store (LoopPlaying);
        }
        prevMode = ModeLoop;

        // Recording-overrun guard — if the user keeps recording past the
        // buffer's capacity, writePos would wrap around and start
        // overwriting the loop's beginning, corrupting the recording.
        // Auto-finalise the loop just before that happens.
        if (looperState.load() == LoopRecording)
        {
            const int recLen = (writePos - loopRecordStartPos + delayBufLen) % delayBufLen;
            if (recLen >= delayBufLen - numSamples - 8)
            {
                loopLengthSamples.store (juce::jmax (1, delayBufLen - numSamples - 16));
                loopPlayPos.store (0);
                looperState.store (LoopPlaying);
            }
        }

        // (Param targets — tiltT, voicingT, textureT — were already loaded
        //  in the snapshot block at the top of processBlock.)
        int loopPlayPosLocal = loopPlayPos.load();
        const int loopLenSamps = loopLengthSamples.load();
        float blockPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            // Smooth crossfade + shaping params.
            wetSm      += (wetT      - wetSm)      * sc;
            dryLevelSm += (dryLevelT - dryLevelSm) * sc;
            tiltSm     += (tiltT     - tiltSm)     * sc;
            textureSm  += (textureT  - textureSm)  * sc;

            const int looperT = looperState.load();
            const float inL = chanData[0][i];
            // Smoothly crossfade between actual R and broadcast-of-L based
            // on monoBlendSm (advanced toward the hysteretic detect state
            // each sample). 0 = pure stereo, 1 = full mono broadcast.
            const float monoTarget = monoDetectHyst ? 1.0f : 0.0f;
            monoBlendSm += sc * (monoTarget - monoBlendSm);
            const float rawR = (numChans > 1) ? chanData[1][i] : inL;
            const float inR  = rawR * (1.0f - monoBlendSm) + inL * monoBlendSm;
            blockPeak = juce::jmax (blockPeak, std::abs (inL));
            blockPeak = juce::jmax (blockPeak, std::abs (rawR));

            float wetL = 0.0f, wetR = 0.0f;
            if (looperT == LoopRecording)
            {
                dlyL[writePos] = inL;
                if (numChans > 1) dlyR[writePos] = inR;
                writePos = (writePos + 1) % delayBufLen;
            }
            else if ((looperT == LoopPlaying || looperT == LoopOverdubbing) && loopLenSamps > 0)
            {
                const int idx = (loopRecordStartPos + loopPlayPosLocal) % delayBufLen;
                wetL = dlyLRead[idx];
                wetR = (numChans > 1) ? dlyRRead[idx] : wetL;
                if (looperT == LoopOverdubbing)
                {
                    // Mix input with existing loop content, slight decay on
                    // the existing layer to prevent runaway buildup over
                    // many overdub passes.
                    dlyL[idx] = wetL * 0.96f + inL;
                    if (numChans > 1) dlyR[idx] = wetR * 0.96f + inR;
                }
                ++loopPlayPosLocal;
                if (loopPlayPosLocal >= loopLenSamps) loopPlayPosLocal = 0;

                // ---- Loop-playback shaping ------------------------------
                // (a) Roll-off filters — HP ~80 Hz, LP ~12 kHz — keep the
                //     raw buffer content tame at both ends.
                constexpr float loopHpCoef = 0.99f;
                constexpr float loopLpCoef = 0.65f;
                auto applyLoopBand = [&] (float x, int c) noexcept
                {
                    const float hp = loopHpCoef * (loopHpState[c] + x - loopHpPrev[c]);
                    loopHpPrev[c]  = x;
                    loopHpState[c] = hp;
                    loopLpState[c] += loopLpCoef * (hp - loopLpState[c]);
                    return loopLpState[c];
                };
                wetL = applyLoopBand (wetL, 0);
                wetR = (numChans > 1) ? applyLoopBand (wetR, 1) : wetL;

                // (b) TILT — same low/high shelving as the main DSP path,
                //     so the LOOP preset's TILT fader actually colours the
                //     playback.
                auto applyLoopTilt = [&] (float x, int c) noexcept
                {
                    constexpr float lpC  = 0.10f;     // ~750 Hz LP split
                    constexpr float hpC  = 0.92f;
                    tiltLpState[c] += lpC * (x - tiltLpState[c]);
                    const float low = tiltLpState[c];
                    const float hp = hpC * (tiltHpState[c] + x - tiltHpPrev[c]);
                    tiltHpPrev[c]  = x;
                    tiltHpState[c] = hp;
                    const float lowGain  = 1.5f - tiltSm;
                    const float highGain = 0.5f + tiltSm;
                    return low * lowGain + hp * highGain;
                };
                wetL = applyLoopTilt (wetL, 0);
                wetR = (numChans > 1) ? applyLoopTilt (wetR, 1) : wetL;

                // (c) VOICING — fixed filter character per VOICING button.
                auto applyLoopVoice = [&] (float x, int c) noexcept
                {
                    switch (voicingT)
                    {
                        case VoicingHiFi:
                        {
                            constexpr float lpC = 0.70f;
                            voicingLpState[c] += lpC * (x - voicingLpState[c]);
                            return voicingLpState[c];
                        }
                        case VoicingFocus:
                        {
                            constexpr float lpC = 0.30f;
                            constexpr float hpC = 0.95f;
                            voicingLpState[c] += lpC * (x - voicingLpState[c]);
                            const float hp = hpC * (voicingHpState[c] + voicingLpState[c] - voicingHpPrev[c]);
                            voicingHpPrev[c]  = voicingLpState[c];
                            voicingHpState[c] = hp;
                            return hp * 1.18f;
                        }
                        case VoicingWarm:
                        {
                            constexpr float lpC = 0.22f;
                            voicingLpState[c] += lpC * (x - voicingLpState[c]);
                            return voicingLpState[c] * 1.05f;
                        }
                        case VoicingAnalog:
                        default:
                        {
                            constexpr float lpC = 0.14f;
                            constexpr float hpC = 0.965f;
                            voicingLpState[c] += lpC * (x - voicingLpState[c]);
                            const float hp = hpC * (voicingHpState[c] + voicingLpState[c] - voicingHpPrev[c]);
                            voicingHpPrev[c]  = voicingLpState[c];
                            voicingHpState[c] = hp;
                            return hp * 1.10f;
                        }
                    }
                };
                wetL = applyLoopVoice (wetL, 0);
                wetR = (numChans > 1) ? applyLoopVoice (wetR, 1) : wetL;

                // (d) Soft saturate — tames any peak that survived the
                //     filters. tanh is gentle for normal levels, asymptotes
                //     to ±1 for extremes.
                wetL = std::tanh (wetL);
                wetR = std::tanh (wetR);
            }

            // Wet/dry crossfade with DRY-level alt + DRY KILL option.
            const float dryKillMul = optDryKill.load() ? 0.0f : 1.0f;
            const float dryGain = std::cos (wetSm * juce::MathConstants<float>::halfPi)
                                * dryLevelSm * dryKillMul;
            const float wetGain = std::sin (wetSm * juce::MathConstants<float>::halfPi);
            // DRY path uses the *raw* R input — see comment in the main DSP loop.
            float outL = inL  * dryGain + wetL * wetGain;
            float outR = rawR * dryGain + wetR * wetGain;
            // Soft-knee output limiter — keeps the combined dry + wet from
            // hard-clipping at the audio device, which is where most of
            // those crackles came from when cluster + diffuse stacked.
            auto softKnee = [] (float x) noexcept
            {
                const float ax = std::abs (x);
                if (ax <= 0.5f) return x;
                const float over = ax - 0.5f;
                const float comp = 0.5f + 0.5f * std::tanh (over * 2.0f);
                return std::copysign (comp, x);
            };
            outL = softKnee (outL);
            outR = softKnee (outR);
            if (! std::isfinite (outL)) outL = inL;
            if (! std::isfinite (outR)) outR = rawR;
            chanData[0][i] = outL;
            if (numChans > 1) chanData[1][i] = outR;
        }
        loopPlayPos    .store (loopPlayPosLocal);
        inputPeakLevel.store (blockPeak);
        return;
    }

    // ---- Per-sample DSP (non-LOOP modes) ----------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        // Smooth params.
        colorSm      += (colorT       - colorSm)      * sc;
        delaySm      += (targetDelaySamps - delaySm)  * sc;
        clusterSm    += (clusterT     - clusterSm)    * sc;
        tiltSm       += (tiltT        - tiltSm)       * sc;
        feedbackSm   += (feedbackT    - feedbackSm)   * sc;
        wetSm        += (wetT         - wetSm)        * sc;
        modDepthSm   += (motionDepthT - modDepthSm)   * sc;
        textureSm    += (textureT     - textureSm)    * sc;
        motionRateSm += (motionRateT  - motionRateSm) * sc;
        crossoverSm  += (crossoverT   - crossoverSm)  * sc;
        diffuseSm    += (diffuseT     - diffuseSm)    * sc;
        dryLevelSm   += (dryLevelT    - dryLevelSm)   * sc;

        // Snapshot raw input — needed by the envelope follower (for ENV
        // motion) BEFORE we compute the LFO value for this sample, so
        // transient stepping is sample-accurate.
        //
        // Smoothed mono broadcast: monoBlendSm slews toward monoDetectHyst
        // each sample. At 0 we use actual R input; at 1 we use L for R
        // (mono guitar mitigation). Avoids cracks when the detector
        // flips between blocks.
        const float monoTarget = monoDetectHyst ? 1.0f : 0.0f;
        monoBlendSm += sc * (monoTarget - monoBlendSm);
        const float inL  = chanData[0][i];
        const float rawR = (numChans > 1) ? chanData[1][i] : inL;
        const float inR  = rawR * (1.0f - monoBlendSm) + inL * monoBlendSm;
        const float absInL = std::abs (inL);
        const float absInR = std::abs (rawR);
        if (absInL > blockInputPeak) blockInputPeak = absInL;
        if (absInR > blockInputPeak) blockInputPeak = absInR;

        // Envelope follower — fast-attack peak chaser + slow-release
        // baseline. The ratio fast/slow detects transients: when the fast
        // env rises noticeably above the baseline, we treat that as a note
        // onset for ENV motion. ENV also benefits from feedback awareness,
        // so the follower watches input + feedback magnitude.
        const float feedbackPeakProxy = juce::jmax (std::abs (limiterEnv[0]),
                                                     std::abs (limiterEnv[1]));
        const float envInput = juce::jmax (juce::jmax (absInL, absInR),
                                            feedbackPeakProxy * 0.5f);
        constexpr float envFastCoef = 0.30f;
        constexpr float envFallCoef = 0.012f;
        constexpr float envBaseCoef = 0.0010f;
        if (envInput > envFollowerSm) envFollowerSm += envFastCoef * (envInput - envFollowerSm);
        else                          envFollowerSm += envFallCoef * (envInput - envFollowerSm);
        envBaselineSm += envBaseCoef * (envFollowerSm - envBaselineSm);

        const bool isTransient = envFollowerSm > envBaselineSm * 1.5f
                              && envFollowerSm > 0.04f;
        if (isTransient && envTransientArmed)
        {
            envStepIndex = (envStepIndex + 1) & 7;     // 8-step sequence
            envTransientArmed = false;
        }
        else if (envFollowerSm < envBaselineSm * 0.85f)
        {
            envTransientArmed = true;                  // ready for the next onset
        }

        // LFO tick — three shapes:
        //   SINE   = classic smooth sine
        //   SQUARE = step/atonal square wave
        //   ENV    = peak-follower triggered transient stepping
        if (motionT != MotionOff)
        {
            lfoPhase += lfoIncPerSm;
            if (lfoPhase >= juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;
        }
        float lfoVal = 0.0f;
        if (motionT == MotionSine)
        {
            lfoVal = (float) std::sin (lfoPhase) * modDepthSm;
        }
        else if (motionT == MotionSquare)
        {
            const float square = lfoPhase < juce::MathConstants<double>::pi ? 1.0f : -1.0f;
            lfoVal = square * modDepthSm;
        }
        else if (motionT == MotionEnv)
        {
            // Per the manual, ENV's RATE alt controls the GLIDE time — how
            // smoothly the modulation moves between steps. High rate =
            // snappy / step-like; low rate = liquid pitch-bend feel.
            const float target  = (float) std::sin (envStepIndex * juce::MathConstants<float>::pi / 4.0f)
                                * modDepthSm;
            const float glideCoef = juce::jmap (motionRateSm, 0.0004f, 0.05f);
            envStepValueSm += glideCoef * (target - envStepValueSm);
            lfoVal = envStepValueSm;
        }
        // ±6 % delay-time modulation at full depth — gives ~6 ms vibrato on
        // a 100 ms delay. MOD-mode short delays get audible chorus, longer
        // modes get subtle warble.
        //
        // Slew the LFO output (~3 ms tc) so SQUARE motion's instantaneous
        // step doesn't snap the buffer-read position by 6% in one sample.
        // The slew is short enough that SINE/ENV motion are unaffected,
        // but it eliminates the click on SQUARE step transitions.
        constexpr float lfoSlewCoef = 0.007f;
        lfoValSm += lfoSlewCoef * (lfoVal - lfoValSm);
        const double modulatedDelay = juce::jlimit (1.0,
            (double) (delayBufLen - 4),
            delaySm * (1.0 + (double) lfoValSm * 0.06));

        // 1) Analog preamp (COLOR) — JFET-style soft clip with low-shelf
        //    body boost, asymmetric saturation, and even-order harmonic
        //    enrichment. Drive maps color 0..1 → 0.5..6.0. Above unity the
        //    asymmetric clipper kicks in. Past ~3× drive the HF rolloff
        //    dominates and the signal gets darker (tape-loss feel).
        //    Modelled loosely after John Snyder's EAE Sending/Halberd
        //    preamp lineage — op-amp gain stage into a JFET-buffered clip.
        // +12 dB alt boost — bumps the preamp drive scale 4× (≈ +12 dB)
        // when the user has the alt-toggle on. Useful for quiet instruments
        // or going further into the saturator's territory.
        const float boostMul = preampBoostT ? 4.0f : 1.0f;
        const float drive    = (0.5f + colorSm * 5.5f) * boostMul;
        auto preamp = [] (float x, float driveAmt, float colorAmt,
                          float& bassState, float& hpState, float& hpPrev,
                          float& lpState) noexcept
        {
            // a) Low-shelf body boost — warms the signal before the clipper.
            //    1-pole LP integrator gives us the LF content; add a scaled
            //    fraction back to the input for a +2 dB shelf below ~250 Hz.
            constexpr float bassCoef = 0.045f;
            bassState += bassCoef * (x - bassState);
            const float boosted = x + bassState * 0.25f;

            // b) Asymmetric soft clip — positive lobe rolls off gently
            //    (x / (1 + 0.7|x|)), negative lobe slightly harder (x /
            //    (1 + 1.2|x|)). Net effect = 2nd-harmonic boost + a soft
            //    knee. Closer to JFET than the symmetric tanh DRIFT used to
            //    ship with.
            const float driven = boosted * driveAmt;
            float clipped;
            if (driven >= 0.0f)
                clipped = driven / (1.0f + 0.7f * driven);
            else
                clipped = driven / (1.0f - 1.2f * driven);

            // c) Even-order harmonic enrichment — the part pure tanh misses.
            //    Scaled small so it adds colour without bulldozing the
            //    fundamental. Drive-dependent so the harmonics ride the
            //    COLOR knob.
            clipped += clipped * std::abs (clipped) * (0.10f + colorAmt * 0.10f);

            // d) DC blocker — bias offset from the asymmetry would otherwise
            //    pile up in the delay buffer over time.
            constexpr float hpCoef = 0.998f;
            const float hpOut  = hpCoef * (hpState + clipped - hpPrev);
            hpPrev  = clipped;
            hpState = hpOut;

            // e) Drive-dependent HF rolloff — gets darker with COLOR up,
            //    same as a hot tape stage losing top end.
            const float lpCoef = juce::jlimit (0.10f, 0.55f, 0.55f - colorAmt * 0.40f);
            lpState += lpCoef * (hpOut - lpState);
            return lpState;
        };
        // Input-stage soft clip — catches hot input signals before they hit
        // the preamp's asymmetric clipper, which would otherwise distort
        // ugly on transients above ~|0.9|. Below 0.85 this is fully
        // transparent (returns x unchanged); above 0.85 it asymptotes
        // smoothly toward ±1.0 via tanh. Acts as a gentle "input ceiling"
        // so the user doesn't have to worry about input gain matching.
        auto inputSoftClip = [] (float x) noexcept
        {
            const float ax = std::abs (x);
            if (ax <= 0.85f) return x;
            const float over = ax - 0.85f;
            const float comp = 0.85f + 0.15f * std::tanh (over * 3.0f);
            return std::copysign (comp, x);
        };

        // When bypass-with-TRAILS is active, mute the wet-path input so no
        // new audio enters the delay buffer. Existing buffer contents keep
        // feeding back and decay naturally; the dry path uses the real inL/
        // inR via the wet/dry crossfade further down.
        const float wetInL = bypassTrailsActive ? 0.0f : inputSoftClip (inL);
        const float wetInR = bypassTrailsActive ? 0.0f : inputSoftClip (inR);
        const float drvRawL = preamp (wetInL, drive, colorSm,
                                      preampBassState[0],
                                      preampHpState[0], preampHpPrev[0], preampLpState[0]);
        const float drvRawR = numChans > 1
                            ? preamp (wetInR, drive, colorSm,
                                      preampBassState[1],
                                      preampHpState[1], preampHpPrev[1], preampLpState[1])
                            : drvRawL;

        // Pre-emphasis: 1-pole high-shelf BOOST applied before the signal
        // enters the delay buffer. The PCM 42 / SDD-3000 chains did this so
        // their quantization noise would sit in a band the de-emph could
        // cut. Inside the delay loop the SRR + bit-crush operate on this
        // boosted signal, so the artifacts get spectral shaping — that's
        // the "digital-vintage sparkle." De-emph on the wet output undoes
        // the boost so the audible signal is roughly flat.
        auto preEmph = [] (float x, float& hpState, float& hpPrev) noexcept
        {
            constexpr float hpCoef = 0.86f;
            const float hp = hpCoef * (hpState + x - hpPrev);
            hpPrev  = x;
            hpState = hp;
            return x + hp * 0.10f;                     // ~+0.8 dB HF shelf — even subtler, less HF for the feedback loop to accumulate as buzz
        };
        const float drvL = preEmph (drvRawL, preEmphHpState[0], preEmphHpPrev[0]);
        const float drvR = numChans > 1
                         ? preEmph (drvRawR, preEmphHpState[1], preEmphHpPrev[1])
                         : drvL;

        // 2) Read the main delay tap (per-channel).
        const float mainL = readDelay (dlyLRead, delayBufLen, writePos, modulatedDelay);
        const float mainR = readDelay (dlyRRead, delayBufLen, writePos, modulatedDelay);

        // 3) CLUSTER — three additive zones along fader travel (per manual):
        //   z1  0..25%   = synced multitap — one stereo echo at 0.5× the
        //                  main delay (small per-channel offset so it
        //                  reads as "two in stereo")
        //   z2  25..75%  = scattered ambience — three unsynced taps at
        //                  non-integer ratios of the delay, panned wider
        //   z3  75..100% = drifting diffusion — heavily LFO-modulated
        //                  extra reads layered on top
        // Zones are additive: clusterSm climbs and successive zones fade
        // in on top of each other.
        // CLUSTER zone weights are NORMALISED so that the maximum cluster
        // contribution (all three zones fully active) is ~1.0× the main
        // tap amplitude. The previous weights summed to 2.6× which pushed
        // the wet path above 0 dB and forced the output soft-knee limiter
        // to do continuous shaping — that shaping was the source of the
        // clicking on dense / high-cluster presets.
        float clusterL = 0.0f, clusterR = 0.0f;
        if (clusterSm > 0.001f)
        {
            const float c  = clusterSm;
            const float z1 = juce::jlimit (0.0f, 1.0f, c * 4.0f);
            const float z2 = juce::jlimit (0.0f, 1.0f, (c - 0.25f) * 2.0f);
            const float z3 = juce::jlimit (0.0f, 1.0f, (c - 0.75f) * 4.0f);

            if (z1 > 0.001f)
            {
                const double tdL = juce::jlimit (1.0, (double) (delayBufLen - 4),
                                                 modulatedDelay * 0.5);
                const double tdR = juce::jlimit (1.0, (double) (delayBufLen - 4),
                                                 modulatedDelay * 0.5 + 80.0);
                clusterL += z1 * 0.30f * readDelay (dlyLRead, delayBufLen, writePos, tdL);
                clusterR += z1 * 0.30f * readDelay (dlyRRead, delayBufLen, writePos, tdR);
            }
            if (z2 > 0.001f)
            {
                static constexpr double scatterRatios [3]  = { 0.31, 0.73, 1.13 };
                static constexpr float  scatterWeights [3] = { 0.22f, 0.16f, 0.12f };  // halved
                for (int t = 0; t < 3; ++t)
                {
                    const double tdBase = modulatedDelay * scatterRatios[t];
                    const double tdL = juce::jlimit (1.0, (double) (delayBufLen - 4), tdBase);
                    const double tdR = juce::jlimit (1.0, (double) (delayBufLen - 4), tdBase * 1.07);
                    clusterL += z2 * scatterWeights[t]
                              * readDelay (dlyLRead, delayBufLen, writePos, tdL);
                    clusterR += z2 * scatterWeights[t]
                              * readDelay (dlyRRead, delayBufLen, writePos, tdR);
                }
            }
            if (z3 > 0.001f)
            {
                const float lfoSlow = (float) std::sin (lfoPhase * 0.27);
                const double tdL = juce::jlimit (1.0, (double) (delayBufLen - 4),
                                                 modulatedDelay * (0.85 + lfoSlow * 0.12));
                const double tdR = juce::jlimit (1.0, (double) (delayBufLen - 4),
                                                 modulatedDelay * (1.17 + lfoSlow * 0.15));
                clusterL += z3 * 0.25f * readDelay (dlyLRead, delayBufLen, writePos, tdL);
                clusterR += z3 * 0.25f * readDelay (dlyRRead, delayBufLen, writePos, tdR);
            }
        }

        // 3b) DIFFUSE — Schroeder-style allpass smearing the main delay tap.
        // Applied after the tap read but before tilt/voicing, so each
        // repeat picks up cumulative diffusion. DIFFUSE TYPE doubles
        // strength when active (alt toggle).
        float diffusedL = mainL, diffusedR = mainR;
        if (diffuseSm > 0.001f)
        {
            const float diffuseStrength = juce::jlimit (0.0f, 1.0f,
                                                        diffuseSm * (diffuseTypeT ? 2.0f : 1.0f));
            const int rdIdxL = (diffuseWritePos - kDiffuseDelayL + kDiffuseLen) & (kDiffuseLen - 1);
            const int rdIdxR = (diffuseWritePos - kDiffuseDelayR + kDiffuseLen) & (kDiffuseLen - 1);
            const float delayedL = diffuseBuf[0][rdIdxL];
            const float delayedR = diffuseBuf[1][rdIdxR];
            const float allpassL = -kDiffuseK * mainL + delayedL;
            const float allpassR = -kDiffuseK * mainR + delayedR;
            diffuseBuf[0][diffuseWritePos] = mainL + kDiffuseK * allpassL;
            diffuseBuf[1][diffuseWritePos] = mainR + kDiffuseK * allpassR;
            diffuseWritePos = (diffuseWritePos + 1) & (kDiffuseLen - 1);
            diffusedL = mainL * (1.0f - diffuseStrength) + allpassL * diffuseStrength;
            diffusedR = mainR * (1.0f - diffuseStrength) + allpassR * diffuseStrength;
        }

        // 4) Tilt EQ on the main tap (the FEEDBACK path inherits this — so
        //    each repeat gets darker/brighter, cumulative tape-echo feel).
        //    low_gain  = 1.5 - tilt    (max 1.5 at tilt=0, min 0.5 at tilt=1)
        //    high_gain = 0.5 + tilt    (max 1.5 at tilt=1, min 0.5 at tilt=0)
        // CROSSOVER alt-control (TILT fader's alt) shifts the pivot
        // frequency. crossoverSm=0 → ~200 Hz (bass-heavy tilt),
        // crossoverSm=0.5 → ~800 Hz (default), crossoverSm=1 → ~4 kHz.
        // LP coefficient = 1 - exp(-2π·fc/sr); we recompute it each block.
        const float lpCoefTilt = juce::jlimit (0.01f, 0.35f,
            (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi
                                     * (200.0 * std::pow (20.0, (double) crossoverSm))
                                     / hostSampleRate)));
        auto tiltEq = [lpCoefTilt] (float x, float t,
                                    float& lpState, float& hpState, float& hpPrev) noexcept
        {
            constexpr float hpCoef = 0.92f;
            lpState += lpCoefTilt * (x - lpState);
            const float low = lpState;
            const float hp = hpCoef * (hpState + x - hpPrev);
            hpPrev  = x;
            hpState = hp;
            const float high = hp;
            const float lowGain  = 1.5f - t;
            const float highGain = 0.5f + t;
            return low * lowGain + high * highGain;
        };
        const float tiltedL = tiltEq (diffusedL, tiltSm,
                                      tiltLpState[0], tiltHpState[0], tiltHpPrev[0]);
        const float tiltedR = numChans > 1
                            ? tiltEq (diffusedR, tiltSm,
                                      tiltLpState[1], tiltHpState[1], tiltHpPrev[1])
                            : tiltedL;

        // VOICING — fixed filter chain that sits between the tilt EQ and
        // the limiter. 4 selectable characters (HIFI/FOCUS/WARM/ANALOG)
        // each give the device a different baseline tone.
        auto voiceFilter = [voicingT, this] (float x,
                                              float& lpState, float& hpState, float& hpPrev) noexcept
        {
            switch (voicingT)
            {
                case VoicingHiFi:
                {
                    // Subtle 1-pole LP at ~10 kHz. Pure passthrough sounded
                    // razor-bright in feedback territory; this takes the
                    // pin off the top without making the voice dark.
                    constexpr float lpC = 0.70f;
                    lpState += lpC * (x - lpState);
                    return lpState;
                }
                case VoicingFocus:
                {
                    // Band-pass-ish: gentle LP at 6 kHz + HP at 200 Hz.
                    constexpr float lpC = 0.30f;
                    constexpr float hpC = 0.95f;
                    lpState += lpC * (x - lpState);
                    const float hp = hpC * (hpState + lpState - hpPrev);
                    hpPrev  = lpState;
                    hpState = hp;
                    return hp * 1.18f;
                }
                case VoicingWarm:
                {
                    // ~4 kHz LP — PCM-style elliptical-ripple character.
                    constexpr float lpC = 0.22f;
                    lpState += lpC * (x - lpState);
                    return lpState * 1.05f;
                }
                case VoicingAnalog:
                default:
                {
                    // Aggressive HF rolloff + low-shelf cut: BBD-style.
                    constexpr float lpC = 0.14f;     // ~2 kHz LP
                    constexpr float hpC = 0.965f;    // ~150 Hz HP
                    lpState += lpC * (x - lpState);
                    const float hp = hpC * (hpState + lpState - hpPrev);
                    hpPrev  = lpState;
                    hpState = hp;
                    return hp * 1.10f;
                }
            }
        };
        const float voicedL = voiceFilter (tiltedL,
                                           voicingLpState[0], voicingHpState[0], voicingHpPrev[0]);
        const float voicedR = numChans > 1
                            ? voiceFilter (tiltedR,
                                           voicingLpState[1], voicingHpState[1], voicingHpPrev[1])
                            : voicedL;

        // 5) Feedback limiter — 3 states. Shapes the signal that's written
        //    back into the delay buffer. This is THE secret sauce: the
        //    limiter compresses → re-amplifies → compresses, creating the
        //    "exponentially evolving smear" that defines Big Time's long-
        //    feedback character.
        // TEXTURE alt-knob — state-specific character control. Big Time:
        //   DIGITAL    → TEXTURE adds aliasing/bit-depth (handled in SRR)
        //   COMPRESSED → TEXTURE = compression amount (subtle squeeze → sag)
        //   SATURATED  → TEXTURE = clip symmetry (more ragged as up)
        //   BIAS       → TEXTURE = misbiasing sensitivity
        const float tex = textureSm;
        auto limit = [stateT, tex] (float x, float& env, float& bias) noexcept
        {
            const float ax = std::abs (x);
            float gain = 1.0f;
            switch (stateT)
            {
                case StateDigital:
                {
                    // Limiter removed; SRR/bit-crush below carries the
                    // TEXTURE response for this state.
                    return x;
                }
                case StateSaturated:
                {
                    // PURE STATIC waveshaper — no envelope follower, no
                    // dynamic gain reduction. The previous version used a
                    // ~1 ms-release envelope that modulated gain at audio
                    // rate when feedback recirculated peaky buffer content;
                    // that audio-rate gain modulation created intermodulation
                    // distortion which the user heard as a persistent HF
                    // buzz tracking with the feedback slider.
                    // Tanh is its own soft limiter — asymptotes to ±1 with
                    // no envelope state, no time-varying gain, no buzz.
                    // tex=0 → fully symmetric; tex=1 → ragged asymmetric.
                    const float negDrive = 1.0f + tex * 0.6f;  // 1.0..1.6
                    const float shaped = x >= 0.0f
                                       ? std::tanh (x)
                                       : std::tanh (x * negDrive);
                    juce::ignoreUnused (env, ax);
                    return shaped * 0.95f;                    // touch under unity for stable feedback
                }
                case StateCompressed:
                {
                    // TEXTURE morphs subtle squeeze (tex=0) → heavy
                    // ducking sag (tex=1) by sweeping threshold, ratio,
                    // and release together.
                    const float attackC  = 0.025f;
                    const float releaseC = juce::jmap (tex, 0.0035f, 0.0014f);   // ~250ms → ~700ms
                    if (ax > env) env += attackC  * (ax - env);
                    else          env += releaseC * (ax - env);
                    const float thr   = juce::jmap (tex, 0.55f, 0.18f);
                    const float ratio = juce::jmap (tex, 3.0f,  10.0f);
                    if (env > thr)
                    {
                        const float overage   = env - thr;
                        const float reduction = overage * (1.0f - 1.0f / ratio);
                        gain = (env - reduction) / juce::jmax (env, 1.0e-6f);
                    }
                    const float makeup = juce::jmap (tex, 1.25f, 1.95f);
                    const float hot = x * gain * makeup;
                    return std::tanh (hot * 0.85f) * 1.18f;
                }
                case StateBias:
                default:
                {
                    // TEXTURE controls how fast the DC bias creeps and how
                    // hard the asymmetric clipper bites. tex=0 = subtle
                    // bias creep, mild clip. tex=1 = aggressive creep, deep
                    // asymmetry — fully sabotaged limiter.
                    const float creepCoef = juce::jmap (tex, 0.0008f, 0.0040f);
                    bias += (ax - bias) * creepCoef;
                    const float biasGain = juce::jmap (tex, 0.20f, 0.65f);
                    const float biased = x + bias * biasGain;
                    const float drive  = juce::jmap (tex, 1.40f, 2.20f);
                    const float stage1 = std::tanh (biased * drive) * 0.88f;
                    const float negStrength = juce::jmap (tex, 0.90f, 1.85f);
                    const float stage2 = stage1 >= 0.0f
                                       ? stage1 / (1.0f + 0.55f * stage1)
                                       : stage1 / (1.0f - negStrength * stage1);
                    return stage2 * 1.08f;
                }
            }
        };
        float fbL = limit (voicedL, limiterEnv[0], biasDcState[0]);
        float fbR = numChans > 1
                  ? limit (voicedR, limiterEnv[1], biasDcState[1])
                  : fbL;

        // Per-repeat soft saturation on the feedback path — too subtle to
        // hear on one pass, but accumulates over long feedback so each loop
        // gets a touch warmer/dirtier. Drive trimmed below unity so the
        // tanh adds minimal HF harmonics per pass.
        // SKIP for DIGITAL state — that state is meant to be the clean one,
        // and the per-repeat tanh was creating odd-harmonic HF buzz over
        // many feedback passes even with the rest of the loop clean.
        if (stateT != StateDigital)
        {
            fbL = std::tanh (fbL * 0.95f) * 0.96f;
            fbR = std::tanh (fbR * 0.95f) * 0.96f;
        }

        // HF damping in the feedback path — TWO cascaded 1-pole LPs for
        // -12 dB/oct rolloff. A single 1-pole at the same cutoff still
        // leaked enough HF that the SAT-clipper / asymmetric-shaper
        // harmonics could accumulate as audible buzz over many feedback
        // passes. Two poles puts a hard ceiling on the loop's HF content.
        // dark tilt (0)  → ~430 Hz cutoff (mellow, very damped)
        // neutral (0.5)  → ~1.7 kHz       (creamy "dreamy" default)
        // bright (1)     → ~3.0 kHz       (still rolled off enough to stop
        //                                  per-pass harmonic accumulation)
        const float fbDampCoef = juce::jmap (juce::jlimit (0.0f, 1.0f, tiltSm),
                                              0.06f, 0.30f);
        fbDampState[0] += fbDampCoef * (fbL - fbDampState[0]);
        fbDampState2[0] += fbDampCoef * (fbDampState[0] - fbDampState2[0]);
        fbL = fbDampState2[0];
        if (numChans > 1)
        {
            fbDampState[1]  += fbDampCoef * (fbR - fbDampState[1]);
            fbDampState2[1] += fbDampCoef * (fbDampState[1] - fbDampState2[1]);
            fbR = fbDampState2[1];
        }

        // 6) Sample-rate crush + bit-depth reduction on the feedback path.
        //    Vintage rack delays ran their AD/DA at far lower rates than
        //    modern gear — PCM 42 at ~18 kHz, SDD-3000 at ~16 kHz, and the
        //    longest delays often dropped lower still as the buffer ran
        //    out. We mirror that with a sample-and-hold + bit quantiser.
        //
        //    HOWEVER — the S&H creates aliasing images that fold into the
        //    audible range. Re-circulated through feedback, these stack
        //    up as a persistent high-end buzz. So S&H is ONLY enabled
        //    when the user has explicitly picked a "vintage" context:
        //      • LONG / LOOP modes  (PCM42 / SDD-3000 territory by spec)
        //      • DIGITAL state with TEXTURE > 0 (state's whole purpose)
        //      • 0.5X alt toggle    (user pressed it for the crunch)
        //    In MOD/SHORT modes with neutral state, we still apply the
        //    bit-quantise (transparent at 18-20 bit, ≤ -100 dB noise
        //    floor) but skip the S&H entirely. No aliasing buildup, no
        //    buzz — those modes were always supposed to be the clean
        //    ones per the Big Time manual.
        double srrTargetHz = 24000.0;
        float  bitDepth    = 20.0f;
        bool   doSampleHold = false;
        switch (modeT)
        {
            case ModeMod:   srrTargetHz = 24000.0; bitDepth = 20.0f;                       break;
            case ModeShort: srrTargetHz = 22000.0; bitDepth = 18.0f;                       break;
            case ModeLong:  srrTargetHz = 16000.0; bitDepth = 14.0f; doSampleHold = true;  break;
            case ModeLoop:  srrTargetHz = 12000.0; bitDepth = 13.0f; doSampleHold = true;  break;
            default: break;
        }
        if (stateT == StateDigital)
        {
            srrTargetHz  = juce::jmap (textureSm, 24000.0f, 6000.0f);
            bitDepth     = juce::jmap (textureSm, 20.0f,    10.0f);
            // Only engage S&H when the user has explicitly cranked TEXTURE
            // past the "clean" half of the range. Below 0.4 the loop stays
            // transparent — the previous threshold of 0.001 meant even
            // default-tex DIGITAL presets had audible bit-crush layered on.
            doSampleHold = textureSm > 0.40f;
        }
        if (bitCrush05T)
        {
            srrTargetHz *= 0.5;
            bitDepth     = 12.0f;
            doSampleHold = true;
        }
        const float srrPeriod = (float) (hostSampleRate / srrTargetHz);
        const float bitSteps  = std::pow (2.0f, bitDepth - 1.0f);
        if (doSampleHold && srrPeriod > 1.0f)
        {
            srrAccum += 1.0f;
            if (srrAccum >= srrPeriod)
            {
                srrAccum -= srrPeriod;
                srrHold[0] = std::round (fbL * bitSteps) / bitSteps;
                srrHold[1] = std::round (fbR * bitSteps) / bitSteps;
            }
            fbL = srrHold[0];
            fbR = srrHold[1];
        }
        else
        {
            // Quantise only — no aliasing, just bit-depth shaping.
            fbL = std::round (fbL * bitSteps) / bitSteps;
            fbR = std::round (fbR * bitSteps) / bitSteps;
        }

        // 7) Write to the delay buffer.
        // HOLD gesture (bypass held in SHORT/LONG mode) — skip the fresh
        // input contribution so the buffer freezes and only the feedback
        // path recirculates. Note feedbackSm is already forced to ~1.0
        // when holdOn is true, so the loop sustains indefinitely.
        const float scaledFbL = fbL * feedbackSm;
        const float scaledFbR = fbR * feedbackSm;
        const float freshL    = holdOn ? 0.0f : drvL;
        const float freshR    = holdOn ? 0.0f : drvR;
        float writeL = freshL + scaledFbL;
        float writeR = freshR + scaledFbR;
        // NaN/Inf guard on the loop — runaway feedback or filter overflow
        // here would corrupt the buffer for every subsequent block.
        if (! std::isfinite (writeL) || std::abs (writeL) > 8.0f) writeL = 0.0f;
        if (! std::isfinite (writeR) || std::abs (writeR) > 8.0f) writeR = 0.0f;
        dlyL[writePos] = writeL;
        if (numChans > 1) dlyR[writePos] = writeR;

        writePos = (writePos + 1) % delayBufLen;

        // 8) User-facing wet output: main tap + cluster smudge, both
        //    inheriting the tilt shaping. SPREAD mode (alt of SCALE) decides
        //    the stereo behaviour: MONO (no cross-mix) / WIDE (subtle stereo
        //    widening across the cluster taps) / PP (ping-pong, swapping
        //    main taps L/R). Phase 2 will refine the ping-pong with proper
        //    alternating-tap behaviour.
        const float wetMainL = voicedL;
        const float wetMainR = voicedR;
        const float wetClL   = clusterL * clusterSm;
        const float wetClR   = clusterR * clusterSm;
        float fullWetL, fullWetR;
        switch (spreadMode.load())
        {
            case SpreadOff:
                fullWetL = wetMainL + wetClL;
                fullWetR = wetMainR + wetClR;
                break;
            case SpreadSubtle:
            {
                // Cluster taps get a 30% mid-side widen.
                const float mid  = (wetClL + wetClR) * 0.5f;
                const float side = (wetClL - wetClR) * 0.5f;
                fullWetL = wetMainL + mid + side * 1.3f;
                fullWetR = wetMainR + mid - side * 1.3f;
                break;
            }
            case SpreadPingPong:
            default:
                // Main taps swap L/R; cluster taps stay normal.
                fullWetL = wetMainR + wetClL;
                fullWetR = wetMainL + wetClR;
                break;
        }

        // Soft-knee pre-limit on the summed wet signal. Main + cluster +
        // spread can exceed ±1.0 on transient hits, which used to clip
        // hard at the output stage and produce audible clicks on every
        // repeat. tanh past 0.7 gives a smooth, inaudible squash so the
        // reverb tail stays creamy instead of crackling.
        auto wetPreLimit = [] (float x) noexcept
        {
            // Raised threshold from 0.7 → 0.85 so this only catches true
            // peaks. At 0.7 it was tanh-shaping on every wet sample when
            // cluster + feedback were active, generating harmonics that
            // contributed to the persistent HF buzz.
            const float ax = std::abs (x);
            if (ax <= 0.85f) return x;
            const float over = ax - 0.85f;
            const float comp = 0.85f + 0.15f * std::tanh (over * 2.5f);
            return std::copysign (comp, x);
        };
        fullWetL = wetPreLimit (fullWetL);
        fullWetR = wetPreLimit (fullWetR);

        // De-emphasis on the user-facing wet output — undoes the pre-emph
        // shelf applied on the way INTO the buffer. The scale factor
        // (0.55 / 1.55 ≈ 0.355) is the exact inverse so the round-trip is
        // ~flat in linear terms; the "sparkle" comes from the SRR + bit-
        // crush + limiter operating on the boosted signal in between.
        auto deEmph = [] (float x, float& hpState, float& hpPrev) noexcept
        {
            constexpr float hpCoef = 0.86f;
            const float hp = hpCoef * (hpState + x - hpPrev);
            hpPrev  = x;
            hpState = hp;
            return x - hp * 0.091f;                    // inverse of 0.10 pre-emph (1 - 1/1.10) so wet output stays roughly flat
        };
        const float wetOutL = deEmph (fullWetL, deEmphHpState[0], deEmphHpPrev[0]);
        const float wetOutR = numChans > 1
                            ? deEmph (fullWetR, deEmphHpState[1], deEmphHpPrev[1])
                            : wetOutL;

        // 9) Final wet/dry crossfade. equal-power keeps wet=0.5 from feeling
        //    quieter than wet=0 or wet=1.
        // DRY alt (WET fader's alt) — separate dry-level control, multiplies
        // the dry signal independently of the wet/dry crossfade. DRY KILL
        // (Options Menu) forces the dry path to zero — wet-only output.
        const float dryKillMul = optDryKill.load() ? 0.0f : 1.0f;
        const float dryGain = std::cos (wetSm * juce::MathConstants<float>::halfPi)
                            * dryLevelSm * dryKillMul;
        const float wetGain = std::sin (wetSm * juce::MathConstants<float>::halfPi);
        // DRY path:
        //   default (DRY CLEAN off) → drvRawL/R (preamp output), so COLOR
        //     colours the dry signal too. Per Big Time manual: "the preamp
        //     processes BOTH dry and wet by default unless DRY CLEAN is
        //     toggled". WET=0 + COLOR up = preamp-only mode.
        //   DRY CLEAN on → raw input (inL / rawR). Always uses raw R to
        //     preserve the user's stereo source — MISO is wet-path-only.
        const bool dryClean = optDryClean.load();
        const float dryL = dryClean ? inL  : drvRawL;
        const float dryR = dryClean ? rawR : drvRawR;
        float outL = dryL * dryGain + wetOutL * wetGain;
        float outR = dryR * dryGain + wetOutR * wetGain;
        // Soft-knee output limiter — prevents the compound wet path gain
        // (cluster + diffuse + tilt + voicing) from hard-clipping at the
        // audio device. Linear below 0.5, tanh-shaped beyond. Output
        // asymptotes to ±1 instead of wrapping or saturating digitally.
        auto softKnee = [] (float x) noexcept
        {
            const float ax = std::abs (x);
            if (ax <= 0.5f) return x;
            const float over = ax - 0.5f;
            const float comp = 0.5f + 0.5f * std::tanh (over * 2.0f);
            return std::copysign (comp, x);
        };
        outL = softKnee (outL);
        outR = softKnee (outR);
        if (! std::isfinite (outL)) outL = inL;
        if (! std::isfinite (outR)) outR = rawR;
        chanData[0][i] = outL;
        if (numChans > 1) chanData[1][i] = outR;
    }

    inputPeakLevel.store (blockInputPeak);
    prevMode = modeT;
}

//==============================================================================
juce::AudioProcessorEditor* DriftAudioProcessor::createEditor()
{
    return new DriftAudioProcessorEditor (*this);
}

//==============================================================================
namespace
{
    constexpr int kStateMagic   = 0x44524654;   // 'DRFT'
    constexpr int kStateVersion = 3;            // v3: + preset bank + options menu
}

// Internal helper — write every param atomic into the stream. Used by both
// getStateInformation (full DAW state) and savePresetSlot (per-preset blob).
// The leading magic + version belong to the caller.
static void writeAllParams (juce::MemoryOutputStream& out, DriftAudioProcessor& p)
{
    out.writeFloat (p.getColor());
    out.writeFloat (p.getTime());
    out.writeFloat (p.getCluster());
    out.writeFloat (p.getTilt());
    out.writeFloat (p.getFeedback());
    out.writeFloat (p.getWet());
    out.writeInt   (p.getMode());
    out.writeInt   (p.getState());
    out.writeFloat (p.getModDepth());
    out.writeFloat (0.0f);                       // v1 spread placeholder
    out.writeDouble(p.getBpm());
    // v2 fields
    out.writeInt   (p.getScale());
    out.writeInt   (p.getMotion());
    out.writeInt   (p.getVoicing());
    out.writeInt   (p.getSpreadMode());
    out.writeFloat (p.getTexture());
    out.writeFloat (p.getMotionRate());
    out.writeFloat (p.getCrossover());
    out.writeFloat (p.getDiffuse());
    out.writeFloat (p.getDryLevel());
    out.writeBool  (p.is05XActive());
    out.writeBool  (p.isDiffuseTypeDoubled());
    out.writeBool  (p.isPreampBoosted());
    out.writeBool  (p.isTapTempoActive());
    out.writeDouble(p.getTapCentreSeconds());
    // v3 options menu
    out.writeBool  (p.isTrailsOn());
    out.writeBool  (p.isDryKillOn());
    out.writeBool  (p.isDryCleanOn());
    out.writeBool  (p.isScaleIgnoreOn());
    out.writeBool  (p.isStepModeOn());
    out.writeBool  (p.isMidiClockOutOn());
}

static void readAllParams (juce::MemoryInputStream& in, int version, DriftAudioProcessor& p)
{
    if (in.getNumBytesRemaining() >= 4) p.setColor    (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setTime     (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setCluster  (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setTilt     (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setFeedback (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setWet      (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) p.setMode     (in.readInt());
    if (in.getNumBytesRemaining() >= 4)
    {
        int rawState = in.readInt();
        if (version < 2)
        {
            if      (rawState == 2) rawState = DriftAudioProcessor::StateBias;
            else if (rawState == 1) rawState = DriftAudioProcessor::StateCompressed;
            else                    rawState = DriftAudioProcessor::StateSaturated;
        }
        p.setState (rawState);
    }
    if (in.getNumBytesRemaining() >= 4) p.setModDepth (in.readFloat());
    if (in.getNumBytesRemaining() >= 4) in.readFloat();                     // v1 spread placeholder
    if (in.getNumBytesRemaining() >= 8) p.setBpm (in.readDouble());
    if (version >= 2)
    {
        if (in.getNumBytesRemaining() >= 4) p.setScale       (in.readInt());
        if (in.getNumBytesRemaining() >= 4) p.setMotion      (in.readInt());
        if (in.getNumBytesRemaining() >= 4) p.setVoicing     (in.readInt());
        if (in.getNumBytesRemaining() >= 4) p.setSpreadMode  (in.readInt());
        if (in.getNumBytesRemaining() >= 4) p.setTexture     (in.readFloat());
        if (in.getNumBytesRemaining() >= 4) p.setMotionRate  (in.readFloat());
        if (in.getNumBytesRemaining() >= 4) p.setCrossover   (in.readFloat());
        if (in.getNumBytesRemaining() >= 4) p.setDiffuse     (in.readFloat());
        if (in.getNumBytesRemaining() >= 4) p.setDryLevel    (in.readFloat());
        if (in.getNumBytesRemaining() >= 1) p.setBitCrush05  (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setDiffuseType (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setPreampBoost (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setTapTempoActive (in.readBool());
        if (in.getNumBytesRemaining() >= 8) p.setTapCentreSeconds (in.readDouble());
    }
    if (version >= 3)
    {
        if (in.getNumBytesRemaining() >= 1) p.setTrails       (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setDryKill      (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setDryClean     (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setScaleIgnore  (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setStepMode     (in.readBool());
        if (in.getNumBytesRemaining() >= 1) p.setMidiClockOut (in.readBool());
    }
}

void DriftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream out (destData, false);
    out.writeInt (kStateMagic);
    out.writeInt (kStateVersion);

    writeAllParams (out, *this);

    // Bypass + LINK live in the DAW state but NOT in presets (user-managed
    // routing state, not audio params).
    out.writeBool (bypass.load());
    out.writeBool (linkEnabled.load());

    // v3: preset bank. 10 slots × variable-length blob each.
    out.writeInt (kNumPresets);
    for (int i = 0; i < kNumPresets; ++i)
    {
        const int sz = (int) presetSlots[i].getSize();
        out.writeInt (sz);
        if (sz > 0) out.write (presetSlots[i].getData(), (size_t) sz);
    }
}

void DriftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in (data, (size_t) sizeInBytes, false);
    if (in.getNumBytesRemaining() < 8) return;
    if (in.readInt() != kStateMagic) return;
    const int v = in.readInt();
    if (v > kStateVersion) return;

    readAllParams (in, v, *this);

    if (in.getNumBytesRemaining() >= 1) bypass.store      (in.readBool());
    if (in.getNumBytesRemaining() >= 1) linkEnabled.store (in.readBool());

    // v3: preset bank.
    if (v >= 3 && in.getNumBytesRemaining() >= 4)
    {
        const int n = juce::jmin (kNumPresets, in.readInt());
        for (int i = 0; i < n; ++i)
        {
            if (in.getNumBytesRemaining() < 4) break;
            const int sz = in.readInt();
            if (sz > 0 && in.getNumBytesRemaining() >= (juce::int64) sz)
            {
                presetSlots[i].setSize ((size_t) sz, false);
                in.read (presetSlots[i].getData(), sz);
            }
            else
            {
                presetSlots[i].reset();
            }
        }
    }
    // Re-evaluate factory install on next prepareToPlay — the just-loaded
    // state may have left some slots empty that should be filled, or filled
    // some that we shouldn't touch.
    factoryPresetsInstalled = false;
}

// ---- Preset bank --------------------------------------------------------
void DriftAudioProcessor::savePresetSlot (int slot)
{
    if (slot < 0 || slot >= kNumPresets) return;
    presetSlots[slot].reset();
    juce::MemoryOutputStream out (presetSlots[slot], false);
    out.writeInt (kStateMagic);
    out.writeInt (kStateVersion);
    writeAllParams (out, *this);
    currentPresetSlot.store (slot);
}

bool DriftAudioProcessor::loadPresetSlot (int slot)
{
    if (slot < 0 || slot >= kNumPresets) return false;
    if (presetSlots[slot].getSize() < 8) return false;
    juce::MemoryInputStream in (presetSlots[slot], false);
    if (in.readInt() != kStateMagic) return false;
    const int v = in.readInt();
    if (v > kStateVersion) return false;
    readAllParams (in, v, *this);
    currentPresetSlot.store (slot);
    return true;
}

bool DriftAudioProcessor::isPresetSlotFilled (int slot) const noexcept
{
    if (slot < 0 || slot >= kNumPresets) return false;
    return presetSlots[slot].getSize() >= 8;
}

// ---- Factory preset bank ------------------------------------------------
// Ten hand-picked starting points, each chosen to exercise a different
// corner of the feature set so a user clicking through them hears every
// state / mode / motion shape / voicing / DIFFUSE / spread combo.
namespace
{
    constexpr DriftAudioProcessor::PresetSpec kFactoryPresets [DriftAudioProcessor::kNumPresets] = {
        // 0 — CLEAN ECHO: basic delay, no colour. Baseline / "is it working?"
        { "CLEAN ECHO",
          0.20f, 0.50f, 0.00f, 0.50f, 0.55f, 0.40f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.50f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadOff,
          0.30f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 1 — WARM CHORUS: MOD mode + SINE motion + COMPRESSED state +
        // WARM voicing. Exercises modulation + comp limiter + filter.
        { "WARM CHORUS",
          0.35f, 0.50f, 0.10f, 0.55f, 0.45f, 0.50f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateCompressed,
          0.70f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.45f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 2 — ROOM SLAP: very short delay + SATURATED for a tape-slap feel.
        { "ROOM SLAP",
          0.55f, 0.18f, 0.00f, 0.45f, 0.20f, 0.45f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateSaturated,
          0.50f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadOff,
          0.40f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 3 — WIDE TRAILS: LONG echoes with stereo widening, COMPRESSED
        // limiter sag, slow SINE. Tests the SPREAD WIDE mode. TILT darker
        // so the fb-damp LP pulls the tail toward "creamy" territory.
        { "WIDE TRAILS",
          0.20f, 0.30f, 0.50f, 0.30f, 0.65f, 0.45f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateCompressed,
          0.35f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.50f, 0.10f, 0.50f, 0.30f, 1.00f,
          false, false, false },

        // 4 — OCTAVE STEPS: SCALE quantisation + SQUARE motion = a pitch
        // sequencer that jumps between octaves on each LFO cycle.
        { "OCTAVE STEPS",
          0.20f, 0.50f, 0.30f, 0.50f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.55f,
          DriftAudioProcessor::ScaleOctave, DriftAudioProcessor::MotionSquare,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadOff,
          0.30f, 0.45f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 5 — DYNAMIC FLEX: ENV motion + BIAS + ANALOG voicing. Picks +
        // chords push the modulation forward; sustained notes glide.
        { "DYNAMIC FLEX",
          0.45f, 0.55f, 0.10f, 0.55f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateBias,
          0.70f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionEnv,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadOff,
          0.40f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 6 — SMEARY WASH: max CLUSTER (zone 3 drifting diffusion) + heavy
        // DIFFUSE allpass + DIGITAL. Darker TILT and lower COLOR so the
        // feedback-damp LP can produce the dreamy creamy tail.
        { "SMEARY WASH",
          0.12f, 0.40f, 0.80f, 0.28f, 0.60f, 0.48f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.45f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.25f, 0.15f, 0.50f, 0.65f, 1.00f,
          false, false, false },

        // 7 — ANALOG DREAM: ANALOG voicing + SATURATED. Lo-fi tape feel
        // from the dark voicing alone; 0.5X bit-crush was creating an
        // audible HF ring (S&H aliasing at 11 kHz), so we drop that and
        // pull TILT a touch darker to preserve the warm-tape vibe.
        { "ANALOG DREAM",
          0.55f, 0.40f, 0.30f, 0.32f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateSaturated,
          0.50f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadSubtle,
          0.50f, 0.30f, 0.50f, 0.20f, 1.00f,
          false, false, false },

        // 8 — BROKEN TAPE: BIAS at high TEXTURE + wet-only output. Crumbling
        // dissolves, no dry passthrough.
        { "BROKEN TAPE",
          0.50f, 0.35f, 0.50f, 0.30f, 0.70f, 1.00f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateBias,
          0.55f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.70f, 0.20f, 0.50f, 0.40f, 0.00f,        // dryLevel=0, wet-only
          false, false, false },

        // 9 — INFINITE BLOOM: very long sustaining feedback + heavy DIFFUSE +
        // CLUSTER. Slowly evolving "hold a chord and walk away" pad. Lower
        // tilt + lower color + slightly reduced feedback so the LP-damped
        // tail blooms gently instead of building harsh HF over many passes.
        { "INFINITE BLOOM",
          0.15f, 0.50f, 0.60f, 0.32f, 0.82f, 0.48f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.40f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.25f, 0.10f, 0.50f, 0.80f, 1.00f,
          false, false, false },

        // 10 — DUB ECHO: short slapback + ping-pong + COMP. Classic dub
        // bounce between channels with a bit of feedback for repeats.
        { "DUB ECHO",
          0.45f, 0.35f, 0.00f, 0.45f, 0.55f, 0.45f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateCompressed,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadPingPong,
          0.40f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 11 — CAVE: long dark wash with slow sine motion. Subterranean,
        // moody. BIAS state crumbles the tail over time.
        { "CAVE",
          0.20f, 0.60f, 0.50f, 0.20f, 0.70f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateBias,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.10f, 0.50f, 0.55f, 1.00f,
          false, false, false },

        // 12 — SHIMMER: SCALE OCTAVE + SQUARE motion makes the delay tap
        // jump between octave-quantised times → octave-jump shimmer effect.
        { "SHIMMER",
          0.15f, 0.40f, 0.30f, 0.65f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateDigital,
          0.40f,
          DriftAudioProcessor::ScaleOctave, DriftAudioProcessor::MotionSquare,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.35f, 0.50f, 0.20f, 1.00f,
          false, false, false },

        // 13 — PING PONG: classic L/R alternating echoes. Clean, no motion.
        { "PING PONG",
          0.30f, 0.35f, 0.00f, 0.50f, 0.50f, 0.45f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadPingPong,
          0.30f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 14 — WET DREAM: dense, ping-pong with cluster + diffuse. The
        // creamy washy preset the user kept asking for after the buzz fixes.
        { "WET DREAM",
          0.20f, 0.50f, 0.55f, 0.35f, 0.60f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateCompressed,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadPingPong,
          0.30f, 0.20f, 0.50f, 0.50f, 1.00f,
          false, false, false },

        // 15 — NIGHT FOG: very dark + SQUARE-motion lazy stepping + ANALOG.
        // Glacial movement, dense diffuse field.
        { "NIGHT FOG",
          0.10f, 0.55f, 0.40f, 0.18f, 0.75f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.50f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSquare,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.05f, 0.40f, 0.60f, 1.00f,
          false, false, false },

        // 16 — KORG SDD: short MOD with subtle SINE motion, SAT state,
        // FOCUS voicing — emulates the SDD-3000's signature chorused echo.
        { "KORG SDD",
          0.30f, 0.25f, 0.00f, 0.50f, 0.45f, 0.40f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateSaturated,
          0.40f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingFocus, DriftAudioProcessor::SpreadOff,
          0.40f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 17 — PCM42: long delay + BIAS sag + ANALOG voicing + 0.5X bit
        // crush. Lexicon PCM42 territory — dark, crunchy, beautiful.
        { "PCM42",
          0.50f, 0.45f, 0.35f, 0.30f, 0.65f, 0.50f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateBias,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadSubtle,
          0.55f, 0.20f, 0.50f, 0.30f, 1.00f,
          true, false, false },                       // 0.5X on

        // 18 — STUTTER: SQUARE motion + CHROMATIC scale at fast rate = a
        // glitchy semitone-jumping stutter pattern.
        { "STUTTER",
          0.30f, 0.20f, 0.00f, 0.50f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.60f,
          DriftAudioProcessor::ScaleChromatic, DriftAudioProcessor::MotionSquare,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadOff,
          0.30f, 0.65f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 19 — MELODIC: CHROMATIC scale + SINE motion = the delay tap glides
        // through quantised pitches melodically.
        { "MELODIC",
          0.20f, 0.45f, 0.20f, 0.55f, 0.50f, 0.45f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateDigital,
          0.35f,
          DriftAudioProcessor::ScaleChromatic, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.25f, 0.50f, 0.10f, 1.00f,
          false, false, false },

        // 20 — CRYSTAL: pristine bright HiFi, slight cluster sparkle. The
        // "no character, just clean" preset.
        { "CRYSTAL",
          0.15f, 0.50f, 0.25f, 0.65f, 0.45f, 0.40f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 21 — FOREVER: feedback approaching unity, heavy DIFFUSE. The
        // "hold a chord and the room keeps it forever" preset.
        { "FOREVER",
          0.18f, 0.60f, 0.55f, 0.30f, 0.88f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.15f, 0.50f, 0.70f, 1.00f,
          false, false, false },

        // 22 — RAINFALL: ENV motion + DIFFUSE TYPE alt = dense raindrops
        // triggered by note onsets.
        { "RAINFALL",
          0.20f, 0.40f, 0.65f, 0.32f, 0.55f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateCompressed,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionEnv,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.20f, 0.50f, 0.50f, 1.00f,
          false, true, false },                       // DIFFUSE TYPE alt

        // 23 — SLAPBACK: very short, low feedback. Rockabilly / Sun Records
        // single-bounce slap.
        { "SLAPBACK",
          0.40f, 0.15f, 0.00f, 0.50f, 0.10f, 0.35f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateSaturated,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingFocus, DriftAudioProcessor::SpreadOff,
          0.30f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

        // 24 — WHISPER: barely-there long delay. Dark, low wet — the kind
        // of background tail you only notice when it stops.
        { "WHISPER",
          0.10f, 0.55f, 0.20f, 0.30f, 0.50f, 0.25f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.20f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.10f, 0.50f, 0.35f, 1.00f,
          false, false, false },

        // 25 — HALL: convolution-style reverberant hall, dense diffuse +
        // cluster + FOCUS voicing for mid-forward spaciousness.
        { "HALL",
          0.18f, 0.50f, 0.60f, 0.42f, 0.55f, 0.50f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateCompressed,
          0.25f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingFocus, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.15f, 0.55f, 0.65f, 1.00f,
          false, false, false },

        // 26 — TAPE LOOP: LOOP mode (engine internally pins feedback ≥ 0.92
        // so the loop doesn't decay between passes). For chord-hold pads.
        { "TAPE LOOP",
          0.40f, 0.45f, 0.00f, 0.40f, 0.90f, 0.50f,
          DriftAudioProcessor::ModeLoop, DriftAudioProcessor::StateBias,
          0.20f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadOff,
          0.30f, 0.10f, 0.50f, 0.20f, 1.00f,
          false, false, false },

        // 27 — CHURCH: extra-long delay, max DIFFUSE + DIFFUSE TYPE alt =
        // cathedral-scale acoustic space. Slow sine motion shimmers the
        // tail.
        { "CHURCH",
          0.12f, 0.70f, 0.50f, 0.22f, 0.78f, 0.55f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.08f, 0.50f, 0.85f, 1.00f,
          false, true, false },                       // DIFFUSE TYPE alt

        // 28 — EAE BIAS: COLOR up + preamp BOOST alt + BIAS state + ANALOG
        // voicing. Dirty, distorted, EAE Sending+Halberd preamp through a
        // crumbling tape echo.
        { "EAE BIAS",
          0.60f, 0.50f, 0.30f, 0.32f, 0.65f, 0.50f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateBias,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingAnalog, DriftAudioProcessor::SpreadSubtle,
          0.55f, 0.18f, 0.50f, 0.45f, 1.00f,
          false, false, true },                       // preampBoost alt

        // 29 — AIR: barely-there, just a hint of bright reverb. The "did
        // you even turn it on?" preset.
        { "AIR",
          0.05f, 0.45f, 0.10f, 0.60f, 0.30f, 0.30f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.30f, 0.50f, 0.15f, 1.00f,
          false, false, false },

        // ===== 30..34 — Gesture / wiring test presets =====================

        // 30 — OVERLOAD TEST: MOD mode at moderate COLOR + FEEDBACK. Hold
        // the BYPASS footswitch while playing → MOD-mode OVERLOAD ramps
        // COLOR + FEEDBACK to max for momentary chaos. Release to revert.
        { "OVERLOAD TEST",
          0.30f, 0.40f, 0.10f, 0.50f, 0.40f, 0.50f,
          DriftAudioProcessor::ModeMod, DriftAudioProcessor::StateSaturated,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingFocus, DriftAudioProcessor::SpreadSubtle,
          0.40f, 0.30f, 0.50f, 0.10f, 1.00f,
          false, false, false },

        // 31 — INFINITE HOLD: SHORT mode at musical FEEDBACK. Hold the
        // BYPASS footswitch → HOLD freezes the buffer and forces
        // feedback to unity. Play overtop the held drone.
        { "INFINITE HOLD",
          0.20f, 0.45f, 0.25f, 0.45f, 0.50f, 0.45f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateDigital,
          0.20f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.20f, 0.50f, 0.20f, 1.00f,
          false, false, false },

        // 32 — LONG HOLD: LONG mode, dark voicing. Hold BYPASS → freezes
        // a long evolving drone. Great for chord-hold pads.
        { "LONG HOLD",
          0.18f, 0.55f, 0.40f, 0.30f, 0.60f, 0.50f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateCompressed,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingWarm, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.12f, 0.50f, 0.40f, 1.00f,
          false, false, false },

        // 33 — CARRY OVER: LONG mode with long delay + decent feedback.
        // Play a phrase, then cycle MODE to LOOP — the existing buffer
        // carries over as the initial loop content.
        { "CARRY OVER",
          0.22f, 0.50f, 0.25f, 0.40f, 0.65f, 0.50f,
          DriftAudioProcessor::ModeLong, DriftAudioProcessor::StateDigital,
          0.30f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionSine,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadSubtle,
          0.30f, 0.15f, 0.50f, 0.30f, 1.00f,
          false, false, false },

        // 34 — PREAMP DRY: high COLOR, low WET. Wet barely audible —
        // mainly the preamp colouring the DRY path. Toggle DRY CLEAN in
        // the Options Menu to A/B preamp-on vs preamp-off on dry.
        { "PREAMP DRY",
          0.55f, 0.45f, 0.00f, 0.50f, 0.30f, 0.10f,
          DriftAudioProcessor::ModeShort, DriftAudioProcessor::StateSaturated,
          0.00f,
          DriftAudioProcessor::ScaleOff, DriftAudioProcessor::MotionOff,
          DriftAudioProcessor::VoicingHiFi, DriftAudioProcessor::SpreadOff,
          0.30f, 0.30f, 0.50f, 0.00f, 1.00f,
          false, false, false },

    };
}

const DriftAudioProcessor::PresetSpec& DriftAudioProcessor::getFactoryPreset (int slot) noexcept
{
    return kFactoryPresets [juce::jlimit (0, kNumPresets - 1, slot)];
}

const char* DriftAudioProcessor::getFactoryPresetName (int slot) noexcept
{
    return getFactoryPreset (slot).name;
}

void DriftAudioProcessor::applyPresetSpec (const PresetSpec& s)
{
    color    .store (s.color);
    time     .store (s.time);
    cluster  .store (s.cluster);
    tilt     .store (s.tilt);
    feedback .store (s.feedback);
    wet      .store (s.wet);
    mode     .store (juce::jlimit (0, kNumModes  - 1, s.mode));
    state    .store (juce::jlimit (0, kNumStates - 1, s.state));
    modDepth .store (s.modDepth);
    scaleMode .store (juce::jlimit (0, kNumScales       - 1, s.scale));
    motionType.store (juce::jlimit (0, kNumMotionShapes - 1, s.motion));
    voicing   .store (juce::jlimit (0, kNumVoicings     - 1, s.voicing));
    spreadMode.store (juce::jlimit (0, kNumSpreadModes  - 1, s.spreadMode));
    texture   .store (s.texture);
    motionRate.store (s.motionRate);
    crossover .store (s.crossover);
    diffuse   .store (s.diffuse);
    dryLevel  .store (s.dryLevel);
    bitCrush05X.store (s.bitCrush05X);
    diffuseType.store (s.diffuseType);
    preampBoost.store (s.preampBoost);
    // Clear tap-tempo state when applying a preset — presets define their
    // own delay-time via mode + TIME fader, and stale tap centres would
    // override that.
    tapTempoActive  .store (false);
    tapCentreSeconds.store (0.250);
}

void DriftAudioProcessor::installFactoryPresetsIfEmpty()
{
    if (factoryPresetsInstalled) return;
    factoryPresetsInstalled = true;

    // Snapshot the user's current param state so we can restore after
    // baking the factories — installation mutates atomics temporarily.
    juce::MemoryBlock userSnapshot;
    {
        juce::MemoryOutputStream out (userSnapshot, false);
        out.writeInt (kStateMagic);
        out.writeInt (kStateVersion);
        writeAllParams (out, *this);
    }

    for (int i = 0; i < kNumPresets; ++i)
    {
        if (presetSlots[i].getSize() >= 8) continue;   // user already saved here — keep
        applyPresetSpec (kFactoryPresets[i]);
        savePresetSlot (i);
    }

    // Restore the snapshot — currentPresetSlot is incidentally left at 9
    // after the install loop; reset it so the UI doesn't claim "you're on
    // preset 9" right after launch.
    juce::MemoryInputStream in (userSnapshot, false);
    if (in.readInt() == kStateMagic)
    {
        const int v = in.readInt();
        if (v <= kStateVersion)
            readAllParams (in, v, *this);
    }
    currentPresetSlot.store (-1);
}

void DriftAudioProcessor::resetAllParameters()
{
    // Reset to TRUE PASSTHROUGH defaults — wet=0 means no audible effect on
    // the host signal regardless of the other faders' positions.
    // INTENTIONALLY does NOT touch bypass or linkEnabled — those are
    // routing/mode state (whether the device is in-circuit, whether it's
    // chained to a companion app), not audio parameters. RESET is for
    // wiping the engine's knobs back to neutral without disturbing the
    // user's connection topology.
    color   .store (0.00f);
    time    .store (0.50f);
    cluster .store (0.00f);
    tilt    .store (0.50f);
    feedback.store (0.00f);
    wet     .store (0.00f);
    mode    .store (ModeShort);
    state   .store (StateSaturated);     // Big Time's "default"
    scaleMode .store (ScaleOff);
    motionType.store (MotionOff);
    voicing   .store (VoicingHiFi);
    spreadMode.store (SpreadOff);
    texture   .store (0.50f);
    motionRate.store (0.30f);
    modDepth  .store (0.50f);
    crossover .store (0.50f);
    diffuse   .store (0.00f);
    dryLevel  .store (1.00f);
    bitCrush05X.store (false);
    diffuseType.store (false);
    preampBoost.store (false);
    tapTempoActive  .store (false);
    tapCentreSeconds.store (0.250);
    internalBpm.store (120.0);
}

juce::File DriftAudioProcessor::getLastSessionFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("DRIFT")
             .getChildFile ("last-session.driftstate");
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DriftAudioProcessor();
}
