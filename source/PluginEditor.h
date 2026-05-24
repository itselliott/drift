#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <vector>
#include "PluginProcessor.h"

//==============================================================================
/**
    Footswitch — paints a vector shape on a circular base. No font-glyph
    dependency, so the icon always renders. Shape::Circle = BYPASS (rec/comp
    style toggle), Shape::Triangle = TAP TEMPO (the right-hand pedal switch).
*/
struct DriftFootswitch : juce::TextButton, private juce::Timer
{
    enum class Shape { Circle, Triangle };

    DriftFootswitch (Shape s, juce::Colour off, juce::Colour on)
        : shape (s), shapeOff (off), shapeOn (on) {}

    void setShape (Shape s) noexcept { shape = s; repaint(); }
    void setShapeColours (juce::Colour off, juce::Colour on)
    {
        shapeOff = off; shapeOn = on; repaint();
    }

    // Press-and-hold callbacks. After `holdThresholdMs` of continuous press,
    // onPressHoldStart() fires; the gesture stays active until the user
    // releases, at which point onPressHoldEnd() fires AND the normal click
    // toggle is suppressed (so a held bypass doesn't also toggle bypass).
    std::function<void()> onPressHoldStart;
    std::function<void()> onPressHoldEnd;
    int holdThresholdMs = 400;

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;
    void mouseDown   (const juce::MouseEvent&) override;
    void mouseUp     (const juce::MouseEvent&) override;
    void timerCallback() override;

private:
    Shape        shape;
    juce::Colour shapeOff;
    juce::Colour shapeOn;
    bool         holdConsumed = false;
};

//==============================================================================
class DriftAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit DriftAudioProcessorEditor (DriftAudioProcessor&);
    ~DriftAudioProcessorEditor() override;

    void paint              (juce::Graphics&) override;
    void paintOverChildren  (juce::Graphics&) override;
    void resized() override;

    bool keyPressed       (const juce::KeyPress&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // The standalone Options menu reaches in here to pop the About card.
    void showAboutOverlay();

    enum FaderIdx { FaderColor = 0, FaderTime, FaderCluster, FaderTilt,
                    FaderFeedback, FaderWet, kNumFaders };

    // TextButton variant that emits a long-press callback after a 2-second
    // mousedown — used by MODE and MOTION to provide the Big Time "hold 2s
    // to reset" gestures. A short press fires onClick as normal.
    struct HoldDetectButton : juce::TextButton
    {
        std::function<void()> onLongPress;

        void mouseDown (const juce::MouseEvent& e) override
        {
            downMs = juce::Time::getMillisecondCounterHiRes();
            juce::TextButton::mouseDown (e);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const double held = juce::Time::getMillisecondCounterHiRes() - downMs;
            if (held >= 2000.0 && onLongPress)
                onLongPress();
            else
                juce::TextButton::mouseUp (e);
        }

    private:
        double downMs = 0.0;
    };

private:
    void timerCallback() override;

    // Cycle helpers — bump the corresponding atomic by 1 (mod count).
    void cycleScale();
    void cycleMotion();
    void cycleMode();        // also clears tap-tempo if active (per manual)
    void cycleVoicing();
    void cycleState();

    // 2-second-hold reset handlers.
    void resetMode();        // hold MODE 2s → simple-delay default
    void resetMotion();      // hold MOTION 2s → motion-to-default

    // SHIFT — toggle the Alt Menu. Switches the six faders' bindings
    // between main and alt atomics + relabels everything.
    void toggleAltMenu();
    void syncFadersToBindings();   // jump fader visuals to current binding
    void syncCycleButtonTexts();   // refresh the 5 cycle-button labels

    // Preset cycling helpers — used by the prev/next buttons and the
    // number-key shortcuts.
    void cyclePreset (int direction);    // direction = -1 or +1
    void updatePresetNameLabel();        // refresh after a load / save

    void registerTap();
    void applyRandomTheme();
    void retintControls();
    void disableChildrenStealingFocus (juce::Component* root);

    // ------------------------------------------------------------------------
    DriftAudioProcessor& processorRef;
    std::array<juce::Slider, kNumFaders> faders;
    std::array<juce::Label,  kNumFaders> faderLabels;
    std::array<juce::Label,  kNumFaders> faderValues;
    juce::Rectangle<int>                 faderColumnBounds[kNumFaders] {};

    // ---- Cycle buttons: SCALE / MOTION / MODE / VOICING / STATE -----------
    // MODE and MOTION are HoldDetectButtons so a 2-second hold triggers a
    // reset (per the Big Time manual — hold MODE 2s = "simple delay reset",
    // hold MOTION 2s = motion-to-default reset).
    juce::TextButton  scaleButton;
    HoldDetectButton  motionButton;
    HoldDetectButton  modeButton;
    juce::TextButton  voicingButton;
    juce::TextButton  stateButton;

    // SHIFT — click to toggle the Alt Menu. While active, the six faders
    // rebind to their alt parameters (TEXTURE / RATE / DEPTH / CROSSOVER /
    // DIFFUSE / DRY) and the cycle buttons rebind to their alt assignments
    // (SPREAD / 0.5X / DIFFUSE TYPE / +12 dB).
    juce::TextButton  shiftButton;
    bool              altMenuActive = false;
    // While Alt Menu is off, we need the main-mode label strings cached so
    // we can pop them back into the buttons cheaply. The cycle-button texts
    // are refreshed every timer tick from processor state, so we don't
    // actually need to cache values — just `altMenuActive` controls which
    // atomic each button reads/writes.

    // ---- Footswitches ------------------------------------------------------
    DriftFootswitch bypassButton { DriftFootswitch::Shape::Circle,
                                   juce::Colour::fromRGB (0xe5, 0x3a, 0x3a),
                                   juce::Colour::fromRGB (0xee, 0xee, 0xe6) };
    DriftFootswitch tapButton    { DriftFootswitch::Shape::Triangle,
                                   juce::Colour::fromRGB (0xee, 0xee, 0xe6),
                                   juce::Colour::fromRGB (0xff, 0xa6, 0x2a) };
    juce::Label bypassLabel;
    juce::Label tapLabel;

    // LINK toggle: pulls audio from a companion DriftLink producer (SP·L)
    // instead of the host audio input. Lives between the two footswitches.
    juce::TextButton linkButton;
    juce::Label      linkStatusLabel;   // "OFF" / "WAITING" / "LINKED" / "SR MISMATCH"

    // RESET — restores all faders + mode/state to passthrough defaults.
    // Themed in the destructive-action red palette so the user understands
    // it's a "wipe everything" button before they click it.
    juce::TextButton resetButton;

    // HOLD — dedicated sticky-toggle gesture button. Engages a mode-specific
    // momentary effect:
    //   MOD mode  → OVERLOAD: ramps COLOR + FEEDBACK to max while engaged
    //   SHORT/LNG → HOLD:     freezes buffer + forces infinite feedback
    //   LOOP mode → DELETE:   wipes the loop + snaps TIME to centre (one-shot)
    // Hotkey 'H' toggles it from the keyboard. Lives next to BYPASS in the
    // footer so the user doesn't have to hold the bypass switch for these.
    juce::TextButton holdButton;
    void toggleHoldGesture();    // called by button onClick and 'H' hotkey

    // Preset cycler: prev / next buttons + name label between them. Sits
    // above the LINK pill in the footer. Loads the 10 factory presets.
    juce::TextButton presetPrevButton, presetNextButton;
    juce::Label      presetNameLabel;

    // Tap-tempo state.
    double              lastTapMs = -1.0;
    std::vector<double> tapIntervals;

    // ---- OLED -------------------------------------------------------------
    juce::Rectangle<int> oledArea;

    // ---- Wordmark (double-click to randomise the theme) -------------------
    juce::Rectangle<int> wordmarkArea;

    // ---- LookAndFeels ------------------------------------------------------
    struct BrushedMetalFaderLookAndFeel : juce::LookAndFeel_V4
    {
        // The accent colour is read from each slider's thumbColourId at paint
        // time (set by retintControls), so the LookAndFeel itself is stateless.
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider&) override;
    };
    BrushedMetalFaderLookAndFeel faderLnf;

    struct ModePillLookAndFeel : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour& bg, bool isOver, bool isDown) override;
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions { juce::jmin (12.0f, (float) buttonHeight * 0.40f),
                                                   juce::Font::bold });
        }
    };
    ModePillLookAndFeel pillLnf;

    struct FootswitchLookAndFeel : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                   const juce::Colour& bg, bool isOver, bool isDown) override;
    };
    FootswitchLookAndFeel footLnf;

    // About / credits overlay — pops on the standalone Options menu's About
    // entry or the i-button (no chrome on click, dismissed by background tap).
    class AboutOverlay; // forward decl
    std::unique_ptr<AboutOverlay> aboutOverlay;

    // Options Menu overlay — pops on the 'O' key. Exposes the per-Big-Time
    // option toggles (TRAILS / DRY KILL / DRY CLEAN / SCALE IGNORE / STEP /
    // CLOCK OUT) plus dismiss.
    class OptionsOverlay; // forward decl
    std::unique_ptr<OptionsOverlay> optionsOverlay;
    void showOptionsOverlay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftAudioProcessorEditor)
};
