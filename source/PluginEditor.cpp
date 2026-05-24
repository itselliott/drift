#include "PluginEditor.h"

namespace
{
    // Window dimensions — landscape "tabletop module" proportions, six tall
    // fader slots, wood cheeks on the L/R edges, 7-seg display near the
    // bottom. Bigger than a normal stompbox per the Big Time / Automatone
    // language: rack-gear seriousness, not pedalboard footprint.
    constexpr int kWindowW = 760;
    constexpr int kWindowH = 600;
    constexpr int kWoodW   = 30;   // width of each wooden side cheek
    constexpr int kBezel   = 14;   // inset on the faceplate before content

    // ---- Palette: charcoal slate faceplate + warm walnut side rails -------
    // Body frame = thin brushed-steel rim that holds the faceplate + wood.
    const auto kBodyHi    = juce::Colour::fromRGB (0x4a, 0x4c, 0x52);
    const auto kBodyMid   = juce::Colour::fromRGB (0x32, 0x34, 0x3a);
    const auto kBodyDark  = juce::Colour::fromRGB (0x14, 0x15, 0x18);

    // Faceplate = muted charcoal/slate (the dark front-panel of a rack unit).
    const auto kPanelHi   = juce::Colour::fromRGB (0x3a, 0x3d, 0x44);
    const auto kPanelMid  = juce::Colour::fromRGB (0x2a, 0x2d, 0x32);
    const auto kPanelLo   = juce::Colour::fromRGB (0x1a, 0x1d, 0x22);

    // Wood side cheeks — warm natural walnut gradient. Grain noise drawn
    // procedurally on top so we don't ship an image asset.
    const auto kWoodHi    = juce::Colour::fromRGB (0xb8, 0x7a, 0x48);
    const auto kWoodMid   = juce::Colour::fromRGB (0x8a, 0x54, 0x30);
    const auto kWoodDark  = juce::Colour::fromRGB (0x4e, 0x2e, 0x18);
    const auto kWoodSeam  = juce::Colour::fromRGB (0x18, 0x10, 0x08);

    const auto kSeam      = juce::Colour::fromRGB (0x10, 0x11, 0x14);
    const auto kScrew     = juce::Colour::fromRGB (0xc8, 0xc8, 0xcc);  // bright steel hex screws
    const auto kScrewDark = juce::Colour::fromRGB (0x18, 0x18, 0x1c);

    // Text — LIGHT on the dark faceplate (was dark on cream).
    const auto kText      = juce::Colour::fromRGB (0xd8, 0xd9, 0xde);
    const auto kTextDim   = juce::Colour::fromRGB (0x80, 0x82, 0x88);
    const auto kTextTag   = juce::Colour::fromRGB (0x6a, 0x6c, 0x72);  // sub-labels

    // 7-segment / OLED display — kOled* is mutable so the wordmark double-
    // click easter-egg can re-tint everything across the chassis.
    const auto   kDispBg  = juce::Colour::fromRGB (0x06, 0x07, 0x09);
    juce::Colour kDispRim = juce::Colour::fromRGB (0x42, 0x10, 0x08);
    juce::Colour kDispTxt = juce::Colour::fromRGB (0xff, 0x4a, 0x32);   // vintage red
    juce::Colour kDispDim = juce::Colour::fromRGB (0x60, 0x18, 0x10);
    juce::Colour kAccent  = juce::Colour::fromRGB (0xff, 0xa6, 0x2a);   // amber, themable
    const auto   kBypass  = juce::Colour::fromRGB (0xe5, 0x3a, 0x3a);

    // Chrome (for footswitches + screws + fader rims).
    const auto kChromeHi   = juce::Colour::fromRGB (0xee, 0xee, 0xf0);
    const auto kChromeMid  = juce::Colour::fromRGB (0xa8, 0xa8, 0xae);
    const auto kChromeLo   = juce::Colour::fromRGB (0x46, 0x46, 0x4a);

    // Tiny accent LEDs scattered across the panel — the "colored micro-lines"
    // that make the Big Time chassis feel like a secret control surface.
    const auto kLedRed     = juce::Colour::fromRGB (0xff, 0x4a, 0x3a);
    const auto kLedBlue    = juce::Colour::fromRGB (0x3a, 0xa6, 0xff);
    const auto kLedPurple  = juce::Colour::fromRGB (0xa8, 0x4a, 0xff);
    const auto kLedGreen   = juce::Colour::fromRGB (0x4a, 0xc8, 0x6a);
    const auto kLedWhite   = juce::Colour::fromRGB (0xe8, 0xe8, 0xee);

    const char* kFaderLabels [DriftAudioProcessorEditor::kNumFaders] = {
        "COLOR", "TIME", "CLUSTER", "TILT", "FEEDBACK", "WET"
    };

    // Per-fader micro-LED accent colour (small dot above each column) — the
    // Big Time chassis has tiny coloured paint marks above each fader, and
    // these LEDs echo that ordering.
    const juce::Colour kFaderLedColours [DriftAudioProcessorEditor::kNumFaders] = {
        kLedRed, kLedWhite, kLedBlue, kLedPurple, kAccent, kLedGreen
    };

    // 7-segment digit drawing. Each digit is 7 thin rectangles (segments);
    // the bitmap below maps 0-9 + ' ' to which segments are lit. Order:
    //   bit 0 = top, 1 = top-R, 2 = bot-R, 3 = bottom, 4 = bot-L,
    //   bit 5 = top-L, 6 = middle.
    constexpr int kSegMap [12] = {
        0b0111111, // 0: t + tr + br + b + bl + tl
        0b0000110, // 1: tr + br
        0b1011011, // 2: t + tr + m + bl + b
        0b1001111, // 3: t + tr + m + br + b
        0b1100110, // 4: tl + m + tr + br
        0b1101101, // 5: t + tl + m + br + b
        0b1111101, // 6: t + tl + m + bl + br + b
        0b0000111, // 7: t + tr + br
        0b1111111, // 8: all
        0b1101111, // 9: t + tl + tr + m + br + b
        0b0000000, //  : blank
        0b1000000  // -: middle bar only (dash)
    };

    inline void draw7SegDigit (juce::Graphics& g, juce::Rectangle<float> b,
                               int digit, juce::Colour litCol, juce::Colour dimCol)
    {
        if (digit < 0 || digit > 11) digit = 10;
        const int mask = kSegMap[digit];
        const float w     = b.getWidth();
        const float h     = b.getHeight();
        const float thick = juce::jmin (w, h) * 0.13f;
        const float inset = thick * 0.55f;
        const float midY  = b.getCentreY();

        // Convenience: stroke a horizontal segment. Unlit segments aren't
        // drawn at all — gives a clean LCD-style display where only the
        // active digits are visible (no "ghost 8" silhouettes behind them).
        auto hbar = [&] (float y, bool lit)
        {
            if (! lit) return;
            const float x1 = b.getX() + inset;
            const float x2 = b.getRight() - inset;
            juce::Path p;
            p.startNewSubPath (x1, y);
            p.lineTo (x1 + thick * 0.4f, y - thick * 0.4f);
            p.lineTo (x2 - thick * 0.4f, y - thick * 0.4f);
            p.lineTo (x2,                y);
            p.lineTo (x2 - thick * 0.4f, y + thick * 0.4f);
            p.lineTo (x1 + thick * 0.4f, y + thick * 0.4f);
            p.closeSubPath();
            g.setColour (litCol);
            g.fillPath (p);
        };
        auto vbar = [&] (float x, float y1, float y2, bool lit)
        {
            if (! lit) return;
            juce::Path p;
            p.startNewSubPath (x, y1);
            p.lineTo (x - thick * 0.4f, y1 + thick * 0.4f);
            p.lineTo (x - thick * 0.4f, y2 - thick * 0.4f);
            p.lineTo (x,                y2);
            p.lineTo (x + thick * 0.4f, y2 - thick * 0.4f);
            p.lineTo (x + thick * 0.4f, y1 + thick * 0.4f);
            p.closeSubPath();
            g.setColour (litCol);
            g.fillPath (p);
        };
        juce::ignoreUnused (dimCol);

        // Segments — dim shadow + lit overlay.
        hbar (b.getY()      + inset,        (mask & 0b0000001) != 0);   // top
        hbar (midY,                         (mask & 0b1000000) != 0);   // middle
        hbar (b.getBottom() - inset,        (mask & 0b0001000) != 0);   // bottom
        vbar (b.getX()      + inset, b.getY() + inset, midY,                (mask & 0b0100000) != 0); // top-L
        vbar (b.getRight()  - inset, b.getY() + inset, midY,                (mask & 0b0000010) != 0); // top-R
        vbar (b.getX()      + inset, midY,             b.getBottom() - inset, (mask & 0b0010000) != 0); // bot-L
        vbar (b.getRight()  - inset, midY,             b.getBottom() - inset, (mask & 0b0000100) != 0); // bot-R
    }

    // Render a non-negative integer as right-justified 7-seg digits within
    // `area`. `numDigits` = how many slots (extra slots show as blank).
    // Passing value = -1 renders dashes ("- -") in every slot — used to
    // mean "no value to show", e.g. preset slot when the user has tweaked
    // away from a saved preset.
    inline void draw7SegNumber (juce::Graphics& g, juce::Rectangle<float> area,
                                int value, int numDigits,
                                juce::Colour lit, juce::Colour dim)
    {
        const float gap = area.getWidth() * 0.05f / juce::jmax (1, numDigits);
        const float dw  = (area.getWidth() - gap * (float) (numDigits - 1)) / (float) numDigits;
        const float dh  = juce::jmin (area.getHeight(), dw * 1.7f);
        const float dy  = area.getCentreY() - dh * 0.5f;

        const bool dashes = (value < 0);
        if (value < 0) value = 0;
        for (int i = numDigits - 1; i >= 0; --i)
        {
            int digit;
            if (dashes)               digit = 11;                            // dash glyph
            else if (value > 0 || i == numDigits - 1) digit = value % 10;
            else                       digit = 10;                            // blank
            value /= 10;
            const float dx = area.getX() + (float) i * (dw + gap);
            draw7SegDigit (g, { dx, dy, dw, dh }, digit, lit, dim);
        }
    }
}

//==============================================================================
// DriftFootswitch
//
void DriftFootswitch::mouseDown (const juce::MouseEvent& e)
{
    holdConsumed = false;
    if (onPressHoldStart)
        startTimer (holdThresholdMs);
    juce::TextButton::mouseDown (e);
}

void DriftFootswitch::mouseUp (const juce::MouseEvent& e)
{
    stopTimer();
    if (holdConsumed)
    {
        // The hold gesture already fired its start callback; tell the gesture
        // to wrap up and SUPPRESS the click toggle that TextButton::mouseUp
        // would otherwise apply.
        if (onPressHoldEnd) onPressHoldEnd();
        holdConsumed = false;
        // Don't call base mouseUp — we don't want the toggle to flip.
        return;
    }
    juce::TextButton::mouseUp (e);
}

void DriftFootswitch::timerCallback()
{
    stopTimer();
    holdConsumed = true;
    if (onPressHoldStart) onPressHoldStart();
}

void DriftFootswitch::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto& lnf = getLookAndFeel();
    lnf.drawButtonBackground (g, *this,
                              findColour (getToggleState() ? buttonOnColourId : buttonColourId),
                              isMouseOver, isButtonDown);

    const float side = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.42f;
    const float cx   = (float) getWidth()  * 0.5f;
    const float cy   = (float) getHeight() * 0.5f;
    const auto  b    = juce::Rectangle<float> (cx - side * 0.5f, cy - side * 0.5f, side, side);

    g.setColour (getToggleState() ? shapeOn : shapeOff);
    juce::Path p;
    switch (shape)
    {
        case Shape::Circle:   p.addEllipse (b); break;
        case Shape::Triangle:
            p.addTriangle (b.getX(),     b.getBottom(),
                           b.getRight(), b.getBottom(),
                           b.getCentreX(), b.getY());
            break;
    }
    g.fillPath (p);
}

//==============================================================================
// BrushedMetalFaderLookAndFeel
//
void DriftAudioProcessorEditor::BrushedMetalFaderLookAndFeel::drawLinearSlider (
    juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    const juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // ---- Recessed channel (dark vertical slot the fader cap slides in) ----
    auto channel = bounds.withSizeKeepingCentre (10.0f, bounds.getHeight() - 24.0f);
    juce::ColourGradient channelGrad (
        juce::Colour::fromRGB (0x05, 0x05, 0x07), channel.getX(), channel.getY(),
        juce::Colour::fromRGB (0x18, 0x18, 0x1c), channel.getRight(), channel.getY(),
        false);
    g.setGradientFill (channelGrad);
    g.fillRoundedRectangle (channel, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (channel.reduced (0.5f), 4.0f, 0.6f);

    // ---- Tick marks every 10% of travel -----------------------------------
    for (int i = 0; i <= 10; ++i)
    {
        const float yT = channel.getY() + channel.getHeight() * (float) i / 10.0f;
        const float majorW = (i % 5 == 0) ? 9.0f : 6.0f;
        g.setColour (kText.withAlpha (i % 5 == 0 ? 0.65f : 0.40f));
        g.drawLine (bounds.getCentreX() - channel.getWidth() * 0.5f - 2.0f - majorW,
                    yT,
                    bounds.getCentreX() - channel.getWidth() * 0.5f - 2.0f,
                    yT, i % 5 == 0 ? 1.0f : 0.7f);
        g.drawLine (bounds.getCentreX() + channel.getWidth() * 0.5f + 2.0f,
                    yT,
                    bounds.getCentreX() + channel.getWidth() * 0.5f + 2.0f + majorW,
                    yT, i % 5 == 0 ? 1.0f : 0.7f);
    }

    // ---- Fader cap --------------------------------------------------------
    const float capH = 32.0f;
    const float capW = (float) w * 0.78f;
    const juce::Rectangle<float> cap (bounds.getCentreX() - capW * 0.5f,
                                      sliderPos - capH * 0.5f,
                                      capW, capH);

    // Dark base shadow.
    g.setColour (juce::Colours::black.withAlpha (0.40f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 4.0f);

    // Brushed metal body — three-stop gradient (light → mid → dark).
    juce::ColourGradient capGrad (
        juce::Colour::fromRGB (0xd0, 0xd0, 0xd4), cap.getX(), cap.getY(),
        juce::Colour::fromRGB (0x40, 0x40, 0x44), cap.getX(), cap.getBottom(),
        false);
    capGrad.addColour (0.45, juce::Colour::fromRGB (0x90, 0x90, 0x94));
    capGrad.addColour (0.55, juce::Colour::fromRGB (0x70, 0x70, 0x74));
    g.setGradientFill (capGrad);
    g.fillRoundedRectangle (cap, 3.0f);

    // Brushing texture — faint horizontal striations.
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    for (int b = 0; b < 6; ++b)
    {
        const float yt = cap.getY() + cap.getHeight() * ((float) b + 1.0f) / 7.0f;
        g.drawHorizontalLine ((int) yt, cap.getX() + 2.0f, cap.getRight() - 2.0f);
    }

    // Accent indicator stripe across the cap centre — the "fader read line".
    const auto accent = slider.findColour (juce::Slider::thumbColourId);
    g.setColour (accent.withAlpha (0.95f));
    const juce::Rectangle<float> stripe (cap.getX() + 3.0f, cap.getCentreY() - 1.5f,
                                         cap.getWidth() - 6.0f, 3.0f);
    g.fillRect (stripe);
    // Stripe glow halo.
    g.setColour (accent.withAlpha (0.18f));
    g.fillRect (stripe.expanded (1.0f, 1.0f));

    // Top/bottom bevels.
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (cap.getX() + 2.0f, cap.getY() + 1.0f,
                cap.getRight() - 2.0f, cap.getY() + 1.0f, 0.9f);
    g.setColour (juce::Colours::black.withAlpha (0.50f));
    g.drawLine (cap.getX() + 2.0f, cap.getBottom() - 1.0f,
                cap.getRight() - 2.0f, cap.getBottom() - 1.0f, 0.9f);
}

//==============================================================================
// ModePillLookAndFeel
//
void DriftAudioProcessorEditor::ModePillLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool /*over*/, bool /*down*/)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const bool on  = b.getToggleState();
    const auto onCol  = b.findColour (juce::TextButton::buttonOnColourId);
    const auto offCol = b.findColour (juce::TextButton::buttonColourId);

    // Outer pill shadow.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.2f), 4.0f);

    if (on)
    {
        // Halo glow around the active pill.
        for (int i = 3; i > 0; --i)
        {
            g.setColour (onCol.withAlpha (0.10f / (float) i));
            g.fillRoundedRectangle (bounds.expanded ((float) i * 1.2f), 5.0f);
        }
        // Filled pill — top half lighter for a 3D bevel.
        juce::ColourGradient grad (onCol.brighter (0.18f), bounds.getX(), bounds.getY(),
                                   onCol.darker  (0.12f), bounds.getX(), bounds.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 3.0f);
        // Specular highlight along the top edge.
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        auto hi = bounds;
        g.fillRoundedRectangle (hi.removeFromTop (hi.getHeight() * 0.45f), 3.0f);
    }
    else
    {
        // Dark inset (recessed look).
        g.setColour (offCol);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 0.7f);
    }
}

void DriftAudioProcessorEditor::ModePillLookAndFeel::drawButtonText (
    juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    g.setColour (on ? juce::Colours::black.withAlpha (0.88f)
                    : juce::Colour::fromRGB (0xc0, 0xa8, 0x80));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds(),
                juce::Justification::centred, false);
}

//==============================================================================
// FootswitchLookAndFeel
//
void DriftAudioProcessorEditor::FootswitchLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool /*over*/, bool isDown)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const float size  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto circle = juce::Rectangle<float> (bounds.getCentreX() - size * 0.5f,
                                                bounds.getCentreY() - size * 0.5f,
                                                size, size);
    // 1) Bright polished chrome outer rim. Radial gradient sweeps from a
    // hot white highlight in the upper-left through a deep shadow on the
    // opposite side — the classic stomp-switch "knurled aluminium hat".
    juce::ColourGradient rim (
        kChromeHi,                                   circle.getX() + size * 0.20f,
                                                     circle.getY() + size * 0.18f,
        kChromeLo,                                   circle.getRight() - size * 0.10f,
                                                     circle.getBottom() - size * 0.10f,
        true);
    rim.addColour (0.4,  kChromeMid);
    rim.addColour (0.75, kChromeLo);
    g.setGradientFill (rim);
    g.fillEllipse (circle);

    // 2) Knurled rim ring — small radial ticks to suggest the milled metal
    // texture of a real stomp switch.
    const int knurlCount = 32;
    const float kr1 = size * 0.46f;
    const float kr2 = size * 0.42f;
    g.setColour (kChromeLo.withAlpha (0.45f));
    for (int i = 0; i < knurlCount; ++i)
    {
        const float a  = juce::MathConstants<float>::twoPi * (float) i
                       / (float) knurlCount;
        const float sa = std::sin (a), ca = std::cos (a);
        g.drawLine (circle.getCentreX() + sa * kr1,
                    circle.getCentreY() - ca * kr1,
                    circle.getCentreX() + sa * kr2,
                    circle.getCentreY() - ca * kr2,
                    0.6f);
    }

    // 3) Inner dark well — recessed black cap where the foot lands.
    auto well = circle.reduced (size * 0.16f);
    juce::ColourGradient wellGrad (
        juce::Colour::fromRGB (0x22, 0x22, 0x26), well.getX(), well.getY(),
        juce::Colour::fromRGB (0x05, 0x05, 0x08), well.getRight(), well.getBottom(),
        true);
    g.setGradientFill (wellGrad);
    g.fillEllipse (well);

    // Pushed-down dim.
    if (isDown)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (well);
    }

    // Specular highlight (small bright crescent on the upper-left of the well).
    g.setColour (juce::Colours::white.withAlpha (isDown ? 0.08f : 0.22f));
    g.fillEllipse (well.getX() + well.getWidth() * 0.18f,
                   well.getY() + well.getHeight() * 0.10f,
                   well.getWidth()  * 0.40f,
                   well.getHeight() * 0.22f);

    // 4) Crisp outer rim line + inner well rim line.
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawEllipse (circle, 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (well,   0.8f);
}

//==============================================================================
// AboutOverlay — opaque card centred on the editor with credits + links.
//
class DriftAudioProcessorEditor::AboutOverlay : public juce::Component
{
public:
    explicit AboutOverlay (DriftAudioProcessorEditor& e) : owner (e)
    {
        setInterceptsMouseClicks (true, true);

        title.setText ("DF-T", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions { 42.0f, juce::Font::bold }));
        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, kAccent);
        addAndMakeVisible (title);

        subtitle.setText ("Hybrid analog/digital echo  ·  v1.0", juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions { 13.0f, juce::Font::plain }));
        subtitle.setJustificationType (juce::Justification::centred);
        subtitle.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        addAndMakeVisible (subtitle);

        body.setText (
            "Inspired by the early-80s rack delays — Lexicon PCM 70/42, Korg SDD-3000 — "
            "and the Chase Bliss / Electronic Audio Experiments Big Time pedal.\n\n"
            "An analog preamp feeds a digital delay engine, which feeds an analog limiter "
            "in the feedback loop. The limiter compresses, re-amplifies, and compresses "
            "again — repeats slowly eat themselves.\n\n"
            "Pairs with SP·L on the same insert chain.",
            juce::dontSendNotification);
        body.setFont (juce::Font (juce::FontOptions { 12.0f, juce::Font::plain }));
        body.setJustificationType (juce::Justification::topLeft);
        body.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
        addAndMakeVisible (body);

        auto wireLink = [this] (juce::TextButton& b, const juce::String& url)
        {
            b.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            b.setColour (juce::TextButton::textColourOffId, kAccent);
            b.setColour (juce::TextButton::textColourOnId,  kAccent.brighter (0.2f));
            b.onClick = [url] { juce::URL (url).launchInDefaultBrowser(); };
            addAndMakeVisible (b);
        };
        ghLink   .setButtonText ("GitHub");
        kofiLink .setButtonText ("Ko-fi");
        bugLink  .setButtonText ("Report a Bug");
        wireLink (ghLink,   "https://github.com/itselliott/drift");
        wireLink (kofiLink, "https://ko-fi.com/itselliott");
        bugLink.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        bugLink.setColour (juce::TextButton::textColourOffId, kAccent);
        bugLink.onClick = [] {
            juce::URL ("mailto:elliottdevs@gmail.com"
                       "?subject=DRIFT%20Bug%20Report"
                       "&body=DRIFT%20version%3A%201.0.0%0AOS%3A%20%0A%0A"
                       "What%20happened%3A%0A%0A"
                       "Steps%20to%20reproduce%3A%0A").launchInDefaultBrowser();
        };
        addAndMakeVisible (bugLink);

        dismissBtn.setButtonText ("Got it");
        dismissBtn.setColour (juce::TextButton::buttonColourId, kAccent);
        dismissBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        dismissBtn.onClick = [this] { owner.aboutOverlay.reset(); };
        addAndMakeVisible (dismissBtn);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.65f));
        const auto card = getCardBounds();
        g.setColour (juce::Colour::fromRGB (0x18, 0x14, 0x12));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (kAccent.withAlpha (0.55f));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.2f);
    }

    void resized() override
    {
        auto card = getCardBounds().toNearestInt().reduced (24, 20);
        title.setBounds    (card.removeFromTop (44));
        subtitle.setBounds (card.removeFromTop (20));
        card.removeFromTop (10);
        body.setBounds     (card.removeFromTop (130));
        card.removeFromTop (12);
        auto links = card.removeFromTop (28);
        const int linkW = links.getWidth() / 3;
        ghLink  .setBounds (links.removeFromLeft (linkW));
        kofiLink.setBounds (links.removeFromLeft (linkW));
        bugLink .setBounds (links);
        card.removeFromTop (12);
        dismissBtn.setBounds (card.removeFromTop (32).withSizeKeepingCentre (120, 32));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getCardBounds().contains (e.position))
            owner.aboutOverlay.reset();
    }

private:
    juce::Rectangle<float> getCardBounds() const
    {
        const float w = juce::jmin (440.0f, (float) getWidth() - 60.0f);
        const float h = 340.0f;
        return { (getWidth() - w) * 0.5f, (getHeight() - h) * 0.5f, w, h };
    }

    DriftAudioProcessorEditor& owner;
    juce::Label      title, subtitle, body;
    juce::TextButton ghLink, kofiLink, bugLink, dismissBtn;
};

//==============================================================================
// OptionsOverlay — exposes the six option-menu toggles (TRAILS / DRY KILL /
// DRY CLEAN / SCALE IGNORE / STEP / CLOCK OUT). Each is a click-to-toggle
// pill. Click outside the card or press ESC to dismiss.
//
class DriftAudioProcessorEditor::OptionsOverlay : public juce::Component
{
public:
    explicit OptionsOverlay (DriftAudioProcessorEditor& e) : owner (e)
    {
        setInterceptsMouseClicks (true, true);
        setWantsKeyboardFocus    (true);

        title.setText ("OPTIONS", juce::dontSendNotification);
        title.setFont (juce::Font (juce::FontOptions { 26.0f, juce::Font::bold }));
        title.setJustificationType (juce::Justification::centred);
        title.setColour (juce::Label::textColourId, kAccent);
        addAndMakeVisible (title);

        subtitle.setText ("Per-Big-Time engine flags — persist with state save.",
                          juce::dontSendNotification);
        subtitle.setFont (juce::Font (juce::FontOptions { 11.0f, juce::Font::plain }));
        subtitle.setJustificationType (juce::Justification::centred);
        subtitle.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
        addAndMakeVisible (subtitle);

        auto& p = owner.processorRef;
        wireToggle (trailsBtn,      "TRAILS",       p.isTrailsOn(),
                    [&p] (bool b) { p.setTrails (b); });
        wireToggle (dryKillBtn,     "DRY KILL",     p.isDryKillOn(),
                    [&p] (bool b) { p.setDryKill (b); });
        wireToggle (dryCleanBtn,    "DRY CLEAN",    p.isDryCleanOn(),
                    [&p] (bool b) { p.setDryClean (b); });
        wireToggle (scaleIgnoreBtn, "SCALE IGNORE", p.isScaleIgnoreOn(),
                    [&p] (bool b) { p.setScaleIgnore (b); });
        wireToggle (stepBtn,        "STEP",         p.isStepModeOn(),
                    [&p] (bool b) { p.setStepMode (b); });
        wireToggle (clockOutBtn,    "MIDI CLOCK OUT", p.isMidiClockOutOn(),
                    [&p] (bool b) { p.setMidiClockOut (b); });

        dismissBtn.setButtonText ("Done");
        dismissBtn.setColour (juce::TextButton::buttonColourId, kAccent);
        dismissBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        dismissBtn.onClick = [this] { owner.optionsOverlay.reset(); };
        addAndMakeVisible (dismissBtn);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.65f));
        const auto card = getCardBounds();
        g.setColour (juce::Colour::fromRGB (0x16, 0x14, 0x18));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (kAccent.withAlpha (0.55f));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.2f);
    }

    void resized() override
    {
        auto card = getCardBounds().toNearestInt().reduced (24, 20);
        title.setBounds    (card.removeFromTop (34));
        subtitle.setBounds (card.removeFromTop (18));
        card.removeFromTop (10);

        juce::TextButton* toggles[6] = {
            &trailsBtn, &dryKillBtn, &dryCleanBtn, &scaleIgnoreBtn,
            &stepBtn,   &clockOutBtn
        };
        for (int i = 0; i < 6; ++i)
            toggles[i]->setBounds (card.removeFromTop (30).reduced (4, 3));

        card.removeFromTop (8);
        dismissBtn.setBounds (card.removeFromTop (32).withSizeKeepingCentre (120, 32));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getCardBounds().contains (e.position))
            owner.optionsOverlay.reset();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.isKeyCode (juce::KeyPress::escapeKey))
        {
            owner.optionsOverlay.reset();
            return true;
        }
        return false;
    }

private:
    juce::Rectangle<float> getCardBounds() const
    {
        const float w = juce::jmin (380.0f, (float) getWidth() - 60.0f);
        const float h = 340.0f;
        return { (getWidth() - w) * 0.5f, (getHeight() - h) * 0.5f, w, h };
    }

    void wireToggle (juce::TextButton& b, const juce::String& label,
                     bool initialState, std::function<void (bool)> onToggle)
    {
        b.setButtonText           (label);
        b.setClickingTogglesState (true);
        b.setToggleState          (initialState, juce::dontSendNotification);
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x22, 0x20, 0x28));
        b.setColour (juce::TextButton::buttonOnColourId, kAccent);
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.6f));
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        b.onClick = [&b, fn = std::move (onToggle)] { fn (b.getToggleState()); };
        addAndMakeVisible (b);
    }

    DriftAudioProcessorEditor& owner;
    juce::Label      title, subtitle;
    juce::TextButton trailsBtn, dryKillBtn, dryCleanBtn, scaleIgnoreBtn,
                     stepBtn, clockOutBtn, dismissBtn;
};

//==============================================================================
// Editor
//
DriftAudioProcessorEditor::DriftAudioProcessorEditor (DriftAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (kWindowW, kWindowH);
    setWantsKeyboardFocus (true);

    // ---- Faders ------------------------------------------------------------
    auto initFader = [this] (juce::Slider& s, const char* label, juce::Label& lbl, juce::Label& val)
    {
        s.setSliderStyle    (juce::Slider::LinearVertical);
        s.setRange          (0.0, 1.0, 0.0);
        s.setDoubleClickReturnValue (true, 0.5);
        s.setTextBoxStyle   (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour         (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
        s.setColour         (juce::Slider::trackColourId,      kAccent);
        s.setColour         (juce::Slider::thumbColourId,      kAccent);
        s.setLookAndFeel    (&faderLnf);
        addAndMakeVisible   (s);

        lbl.setText             (label, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour           (juce::Label::textColourId, kText);
        lbl.setFont             (juce::Font (juce::FontOptions { 11.5f, juce::Font::bold }));
        addAndMakeVisible       (lbl);

        val.setJustificationType(juce::Justification::centred);
        val.setColour           (juce::Label::textColourId, kTextDim);
        val.setFont             (juce::Font (juce::FontOptions { 10.0f, juce::Font::bold }));
        addAndMakeVisible       (val);
    };
    for (int i = 0; i < kNumFaders; ++i)
        initFader (faders[i], kFaderLabels[i], faderLabels[i], faderValues[i]);

    // Initial values from processor.
    faders[FaderColor]   .setValue (processorRef.getColor(),    juce::dontSendNotification);
    faders[FaderTime]    .setValue (processorRef.getTime(),     juce::dontSendNotification);
    faders[FaderCluster] .setValue (processorRef.getCluster(),  juce::dontSendNotification);
    faders[FaderTilt]    .setValue (processorRef.getTilt(),     juce::dontSendNotification);
    faders[FaderFeedback].setValue (processorRef.getFeedback(), juce::dontSendNotification);
    faders[FaderWet]     .setValue (processorRef.getWet(),      juce::dontSendNotification);

    // Fader-to-atomic bindings switch between main and alt parameters when
    // SHIFT is toggled. The lambdas branch on altMenuActive at call time
    // (re-read every change), so the SHIFT toggle doesn't need to rewire
    // the connections — just flip the flag + repaint.
    faders[FaderColor].onValueChange = [this] {
        const float v = (float) faders[FaderColor].getValue();
        if (altMenuActive) processorRef.setTexture (v);
        else               processorRef.setColor   (v);
    };
    faders[FaderTime].onValueChange = [this] {
        const float v = (float) faders[FaderTime].getValue();
        if (altMenuActive) processorRef.setMotionRate (v);
        else               processorRef.setTime       (v);
    };
    faders[FaderCluster].onValueChange = [this] {
        const float v = (float) faders[FaderCluster].getValue();
        if (altMenuActive) processorRef.setModDepth (v);   // motion DEPTH
        else               processorRef.setCluster  (v);
    };
    faders[FaderTilt].onValueChange = [this] {
        const float v = (float) faders[FaderTilt].getValue();
        if (altMenuActive) processorRef.setCrossover (v);
        else               processorRef.setTilt      (v);
    };
    faders[FaderFeedback].onValueChange = [this] {
        const float v = (float) faders[FaderFeedback].getValue();
        if (altMenuActive) processorRef.setDiffuse  (v);
        else               processorRef.setFeedback (v);
    };
    faders[FaderWet].onValueChange = [this] {
        const float v = (float) faders[FaderWet].getValue();
        if (altMenuActive) processorRef.setDryLevel (v);
        else               processorRef.setWet      (v);
    };

    // ---- Cycle buttons: SCALE / MOTION / MODE / VOICING / STATE -----------
    // All five share the same pill style; their click handlers bump the
    // corresponding atomic by one (mod the count). MODE and MOTION are
    // HoldDetectButtons — a 2-second hold fires a reset gesture instead.
    auto styleCycleBtn = [this] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,    juce::Colour::fromRGB (0x1c, 0x14, 0x10));
        b.setColour (juce::TextButton::buttonOnColourId,  kAccent);
        b.setColour (juce::TextButton::textColourOffId,   juce::Colour::fromRGB (0xc0, 0xa8, 0x80));
        b.setColour (juce::TextButton::textColourOnId,    juce::Colours::black);
        b.setLookAndFeel (&pillLnf);
        addAndMakeVisible (b);
    };
    styleCycleBtn (scaleButton);
    styleCycleBtn (motionButton);
    styleCycleBtn (modeButton);
    styleCycleBtn (voicingButton);
    styleCycleBtn (stateButton);
    scaleButton  .onClick = [this] { cycleScale();   };
    motionButton .onClick = [this] { cycleMotion();  };
    modeButton   .onClick = [this] { cycleMode();    };
    voicingButton.onClick = [this] { cycleVoicing(); };
    stateButton  .onClick = [this] { cycleState();   };
    motionButton.onLongPress = [this] { resetMotion(); };
    modeButton  .onLongPress = [this] { resetMode();   };

    // ---- SHIFT — Alt Menu toggle ------------------------------------------
    // Distinct dark-purple palette so it reads as a meta-control, not just
    // another cycle button.
    shiftButton.setButtonText           ("SHIFT");
    shiftButton.setClickingTogglesState (true);
    shiftButton.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x20, 0x18, 0x28));
    shiftButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0xa8, 0x4a, 0xff));
    shiftButton.setColour (juce::TextButton::textColourOffId,  juce::Colour::fromRGB (0xb0, 0x9a, 0xc8));
    shiftButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    shiftButton.setLookAndFeel (&pillLnf);
    shiftButton.onClick = [this] { toggleAltMenu(); };
    addAndMakeVisible (shiftButton);

    // Prime the cycle-button labels to match the processor's current state.
    syncCycleButtonTexts();

    // ---- Footswitches ------------------------------------------------------
    bypassButton.setLookAndFeel        (&footLnf);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setToggleState         (processorRef.isBypassed(), juce::dontSendNotification);
    // shapeOff (engine ON, signal processed) = dim grey; shapeOn (bypass
    // engaged, signal passing through) = bright red. So the lit-red state
    // says "currently bypassing" — opposite of pedal LED convention but
    // matches what the user wants this readout to communicate.
    bypassButton.setShapeColours        (juce::Colour::fromRGB (0x55, 0x55, 0x58), kBypass);
    bypassButton.onClick = [this] {
        processorRef.setBypassed (bypassButton.getToggleState());
        repaint();
    };
    addAndMakeVisible (bypassButton);

    tapButton.setLookAndFeel (&footLnf);
    tapButton.setShapeColours (kAccent, kAccent.brighter (0.2f));
    tapButton.onClick = [this] { registerTap(); };
    addAndMakeVisible (tapButton);

    bypassLabel.setText             ("BYPASS", juce::dontSendNotification);
    bypassLabel.setJustificationType(juce::Justification::centred);
    bypassLabel.setColour           (juce::Label::textColourId, kText);
    bypassLabel.setFont             (juce::Font (juce::FontOptions { 11.0f, juce::Font::bold }));
    addAndMakeVisible               (bypassLabel);

    tapLabel.setText             ("TAP", juce::dontSendNotification);
    tapLabel.setJustificationType(juce::Justification::centred);
    tapLabel.setColour           (juce::Label::textColourId, kText);
    tapLabel.setFont             (juce::Font (juce::FontOptions { 11.0f, juce::Font::bold }));
    addAndMakeVisible            (tapLabel);

    // ---- LINQ pill (next to wordmark) ------------------------------------
    // The button always reads "LINQ"; its background colour is the status
    // indicator: dim grey = off, amber = waiting for partner, green =
    // linked, red = sample-rate mismatch. The separate text label is gone.
    linkButton.setButtonText           ("LINQ");
    linkButton.setClickingTogglesState (true);
    linkButton.setToggleState          (processorRef.isLinkEnabled(), juce::dontSendNotification);
    linkButton.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x1c, 0x14, 0x10));
    linkButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGB (0x4a, 0xc8, 0x6a));  // initial green; runtime override below
    linkButton.setLookAndFeel (&pillLnf);
    linkButton.onClick = [this] {
        processorRef.setLinkEnabled (linkButton.getToggleState());
    };
    addAndMakeVisible (linkButton);

    // ---- RESET pill (header) — wipes every fader/mode/state to defaults --
    resetButton.setButtonText ("RESET");
    resetButton.setColour (juce::TextButton::buttonColourId,
                           juce::Colour::fromRGB (0x32, 0x14, 0x14));   // dark red
    resetButton.setColour (juce::TextButton::buttonOnColourId, kBypass);
    resetButton.setColour (juce::TextButton::textColourOffId, kBypass);
    resetButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    resetButton.setLookAndFeel (&pillLnf);
    resetButton.onClick = [this] {
        processorRef.resetAllParameters();
        // The 30 Hz timer will sync every fader + mode/state button on its
        // next tick; calling repaint() flushes the chassis immediately so
        // there's no perceptible lag between click and visible reset.
        repaint();
        grabKeyboardFocus();
    };
    addAndMakeVisible (resetButton);

    // ---- HOLD button (footer) — dedicated gesture toggle ------------------
    // Mode-dispatched: MOD=OVERLOAD, SHORT/LONG=HOLD, LOOP=DELETE. Sticky
    // toggle on click (engage / release). 'H' hotkey mirrors the click.
    holdButton.setButtonText ("HOLD");
    holdButton.setClickingTogglesState (true);
    holdButton.setColour (juce::TextButton::buttonColourId,
                          juce::Colour::fromRGB (0x18, 0x14, 0x10));
    holdButton.setColour (juce::TextButton::buttonOnColourId, kAccent);
    holdButton.setColour (juce::TextButton::textColourOffId,
                          juce::Colour::fromRGB (0xc8, 0xb0, 0x82));
    holdButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    holdButton.setLookAndFeel (&pillLnf);
    holdButton.onClick = [this] { toggleHoldGesture(); };
    addAndMakeVisible (holdButton);

    // ---- Preset cycler ----------------------------------------------------
    // Prev / next arrow buttons + the loaded preset's name in between.
    auto stylePresetArrow = [this] (juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour::fromRGB (0x18, 0x14, 0x10));
        b.setColour (juce::TextButton::buttonOnColourId, kAccent);
        b.setColour (juce::TextButton::textColourOffId,
                     juce::Colour::fromRGB (0xc8, 0xb0, 0x82));
        b.setColour (juce::TextButton::textColourOnId,  juce::Colours::black);
        b.setLookAndFeel (&pillLnf);
        addAndMakeVisible (b);
    };
    stylePresetArrow (presetPrevButton, "<");
    stylePresetArrow (presetNextButton, ">");
    presetPrevButton.onClick = [this] { cyclePreset (-1); };
    presetNextButton.onClick = [this] { cyclePreset (+1); };

    presetNameLabel.setText ("-", juce::dontSendNotification);
    presetNameLabel.setJustificationType (juce::Justification::centred);
    presetNameLabel.setColour (juce::Label::textColourId, kTextTag);
    presetNameLabel.setFont   (juce::Font (juce::FontOptions { 8.0f, juce::Font::bold }));
    addAndMakeVisible (presetNameLabel);
    updatePresetNameLabel();

    // ---- Focus theft prevention + grab focus for keyboard shortcuts -------
    disableChildrenStealingFocus (this);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer (this)] {
        if (safe != nullptr) safe->grabKeyboardFocus();
    });

    startTimerHz (30);
}

DriftAudioProcessorEditor::~DriftAudioProcessorEditor()
{
    stopTimer();
    for (auto& f : faders) f.setLookAndFeel (nullptr);
    scaleButton  .setLookAndFeel (nullptr);
    motionButton .setLookAndFeel (nullptr);
    modeButton   .setLookAndFeel (nullptr);
    voicingButton.setLookAndFeel (nullptr);
    stateButton  .setLookAndFeel (nullptr);
    shiftButton  .setLookAndFeel (nullptr);
    bypassButton .setLookAndFeel (nullptr);
    tapButton    .setLookAndFeel (nullptr);
    linkButton   .setLookAndFeel (nullptr);
    resetButton  .setLookAndFeel (nullptr);
    holdButton   .setLookAndFeel (nullptr);
    presetPrevButton.setLookAndFeel (nullptr);
    presetNextButton.setLookAndFeel (nullptr);
    aboutOverlay  .reset();
    optionsOverlay.reset();
}

//==============================================================================
void DriftAudioProcessorEditor::disableChildrenStealingFocus (juce::Component* root)
{
    for (auto* c : root->getChildren())
    {
        c->setMouseClickGrabsKeyboardFocus (false);
        disableChildrenStealingFocus (c);
    }
}

void DriftAudioProcessorEditor::cycleScale()
{
    processorRef.setScale ((processorRef.getScale() + 1) % DriftAudioProcessor::kNumScales);
    syncCycleButtonTexts();
    repaint();
}

void DriftAudioProcessorEditor::cycleMotion()
{
    processorRef.setMotion ((processorRef.getMotion() + 1) % DriftAudioProcessor::kNumMotionShapes);
    syncCycleButtonTexts();
    repaint();
}

void DriftAudioProcessorEditor::cycleMode()
{
    // Pressing MODE while tap-tempo is active deactivates tap (per Big
    // Time manual — "press MODE to turn tap tempo off"). Otherwise it
    // advances the delay range.
    if (processorRef.isTapTempoActive())
    {
        processorRef.setTapTempoActive (false);
        syncCycleButtonTexts();
        repaint();
        return;
    }
    const int oldMode = processorRef.getMode();
    const int newMode = (oldMode + 1) % DriftAudioProcessor::kNumModes;
    processorRef.setMode (newMode);
    // Loop carry-over: only fires on a deliberate LONG→LOOP advance via
    // this cycle button. Preset loads / MIDI CC mode changes don't trigger
    // it, so LOOP-mode presets start fresh.
    if (oldMode == DriftAudioProcessor::ModeLong
     && newMode == DriftAudioProcessor::ModeLoop)
        processorRef.requestLoopCarryover();
    syncCycleButtonTexts();
    repaint();
}

void DriftAudioProcessorEditor::cycleVoicing()
{
    processorRef.setVoicing ((processorRef.getVoicing() + 1) % DriftAudioProcessor::kNumVoicings);
    syncCycleButtonTexts();
    repaint();
}

void DriftAudioProcessorEditor::cycleState()
{
    processorRef.setState ((processorRef.getState() + 1) % DriftAudioProcessor::kNumStates);
    syncCycleButtonTexts();
    repaint();
}

void DriftAudioProcessorEditor::resetMode()
{
    // Hold-MODE 2s → "simple delay" reset. We use the existing factory-
    // defaults helper (which already preserves bypass + LINK per the user's
    // earlier ask). Tap-tempo also clears so TIME goes back to its mode-
    // mapped range.
    processorRef.resetAllParameters();
    processorRef.setMode (DriftAudioProcessor::ModeShort);
    processorRef.setTapTempoActive (false);
    syncCycleButtonTexts();
    syncFadersToBindings();
    repaint();
    grabKeyboardFocus();
}

void DriftAudioProcessorEditor::resetMotion()
{
    // Hold-MOTION 2s → motion-to-default reset. Sine, mid rate, mid depth.
    processorRef.setMotion    (DriftAudioProcessor::MotionSine);
    processorRef.setMotionRate (0.30f);
    processorRef.setModDepth   (0.50f);
    syncCycleButtonTexts();
    syncFadersToBindings();
    repaint();
    grabKeyboardFocus();
}

void DriftAudioProcessorEditor::toggleAltMenu()
{
    altMenuActive = shiftButton.getToggleState();
    syncFadersToBindings();
    syncCycleButtonTexts();
    // Faders also need their main labels swapped to alt names — handled
    // here so the change is instantaneous on click.
    static const char* mainLabels [kNumFaders] = { "COLOR", "TIME", "CLUSTER",
                                                    "TILT", "FEEDBACK", "WET" };
    static const char* altLabels  [kNumFaders] = { "TEXTURE", "RATE", "DEPTH",
                                                    "CROSS", "DIFFUSE", "DRY" };
    for (int i = 0; i < kNumFaders; ++i)
        faderLabels[i].setText (altMenuActive ? altLabels[i] : mainLabels[i],
                                juce::dontSendNotification);
    repaint();
    grabKeyboardFocus();
}

void DriftAudioProcessorEditor::syncFadersToBindings()
{
    // Jump every fader to the value of whichever atomic it's currently
    // bound to. Suppress notifications so the slider doesn't echo back.
    auto setIf = [this] (int idx, float value)
    {
        if (! faders[idx].isMouseButtonDown())
            faders[idx].setValue (value, juce::dontSendNotification);
    };
    if (altMenuActive)
    {
        setIf (FaderColor,    processorRef.getTexture());
        setIf (FaderTime,     processorRef.getMotionRate());
        setIf (FaderCluster,  processorRef.getModDepth());
        setIf (FaderTilt,     processorRef.getCrossover());
        setIf (FaderFeedback, processorRef.getDiffuse());
        setIf (FaderWet,      processorRef.getDryLevel());
    }
    else
    {
        setIf (FaderColor,    processorRef.getColor());
        setIf (FaderTime,     processorRef.getTime());
        setIf (FaderCluster,  processorRef.getCluster());
        setIf (FaderTilt,     processorRef.getTilt());
        setIf (FaderFeedback, processorRef.getFeedback());
        setIf (FaderWet,      processorRef.getWet());
    }
}

void DriftAudioProcessorEditor::toggleHoldGesture()
{
    // Sticky toggle: button is now ON or OFF — dispatch the gesture by
    // current MODE. LOOP mode's DELETE is a one-shot, so on engage we fire
    // it and immediately pop the button back off.
    const bool engaged = holdButton.getToggleState();
    const int  m       = processorRef.getMode();

    if (! engaged)
    {
        // Disengaging all while-held gestures.
        processorRef.setOverloadActive (false);
        processorRef.setHoldActive     (false);
        return;
    }

    if (m == DriftAudioProcessor::ModeMod)
    {
        processorRef.setOverloadActive (true);
    }
    else if (m == DriftAudioProcessor::ModeShort
          || m == DriftAudioProcessor::ModeLong)
    {
        processorRef.setHoldActive (true);
    }
    else if (m == DriftAudioProcessor::ModeLoop)
    {
        // One-shot DELETE — snap TIME to centre per the Big Time manual,
        // then immediately pop the toggle so the user knows it's a fire-
        // and-forget action.
        processorRef.requestLoopDelete();
        processorRef.setTime (0.5f);
        faders[FaderTime].setValue (0.5, juce::sendNotification);
        holdButton.setToggleState (false, juce::dontSendNotification);
    }
}

void DriftAudioProcessorEditor::cyclePreset (int direction)
{
    int cur = processorRef.getCurrentPresetSlot();
    // First click after launch: -1 → go to slot 0 on next, slot 9 on prev.
    if (cur < 0)
        cur = (direction > 0) ? -1 : 0;

    const int n = DriftAudioProcessor::kNumPresets;
    const int next = ((cur + direction) % n + n) % n;
    if (processorRef.loadPresetSlot (next))
    {
        syncFadersToBindings();
        syncCycleButtonTexts();
        updatePresetNameLabel();
        repaint();
    }
    grabKeyboardFocus();
}

void DriftAudioProcessorEditor::updatePresetNameLabel()
{
    const int cur = processorRef.getCurrentPresetSlot();
    if (cur < 0 || cur >= DriftAudioProcessor::kNumPresets)
        presetNameLabel.setText ("-", juce::dontSendNotification);
    else
        presetNameLabel.setText (DriftAudioProcessor::getFactoryPresetName (cur),
                                 juce::dontSendNotification);
}

void DriftAudioProcessorEditor::syncCycleButtonTexts()
{
    if (altMenuActive)
    {
        scaleButton  .setButtonText (DriftAudioProcessor::getSpreadLabel  (processorRef.getSpreadMode()));
        motionButton .setButtonText (processorRef.is05XActive()           ? "0.5X ON"  : "0.5X OFF");
        modeButton   .setButtonText (processorRef.isDiffuseTypeDoubled()  ? "DIFF x2"  : "DIFF x1");
        voicingButton.setButtonText (processorRef.isPreampBoosted()       ? "+12 ON"   : "+12 OFF");
        stateButton  .setButtonText (DriftAudioProcessor::getStateLabel   (processorRef.getState()));
    }
    else
    {
        scaleButton  .setButtonText (DriftAudioProcessor::getScaleLabel   (processorRef.getScale()));
        motionButton .setButtonText (DriftAudioProcessor::getMotionLabel  (processorRef.getMotion()));
        modeButton   .setButtonText (DriftAudioProcessor::getModeLabel    (processorRef.getMode()));
        voicingButton.setButtonText (DriftAudioProcessor::getVoicingLabel (processorRef.getVoicing()));
        stateButton  .setButtonText (DriftAudioProcessor::getStateLabel   (processorRef.getState()));
    }
}

void DriftAudioProcessorEditor::registerTap()
{
    // Footswitch gesture is MODE-dependent:
    //   LOOP  → cycle the looper state (Stopped→Record→Play→Overdub→Play…)
    //   MOD   → toggle MOTION on/off (TAP in MOD is a motion gesture)
    //   SHORT/LONG → tap-tempo, which also drives TAP-SETS-TIME
    const int mode = processorRef.getMode();

    if (mode == DriftAudioProcessor::ModeLoop)
    {
        // Single-tap gesture for looper transitions — no interval averaging
        // needed. The audio thread advances state on its next block.
        processorRef.requestLoopCycle();
        tapIntervals.clear();
        lastTapMs = juce::Time::getMillisecondCounterHiRes();
        repaint();
        return;
    }

    if (mode == DriftAudioProcessor::ModeMod)
    {
        // TAP in MOD = MOTION toggle. Cycles between Off and the user's
        // previously-selected motion shape (default Sine).
        const int current = processorRef.getMotion();
        if (current == DriftAudioProcessor::MotionOff)
            processorRef.setMotion (DriftAudioProcessor::MotionSine);
        else
            processorRef.setMotion (DriftAudioProcessor::MotionOff);
        syncCycleButtonTexts();
        lastTapMs = juce::Time::getMillisecondCounterHiRes();
        repaint();
        return;
    }

    // SHORT / LONG — tap-tempo with TAP-SETS-TIME.
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (lastTapMs > 0.0 && (now - lastTapMs) < 2000.0)
    {
        tapIntervals.push_back (now - lastTapMs);
        if (tapIntervals.size() > 4) tapIntervals.erase (tapIntervals.begin());
        double avgMs = 0.0;
        for (auto v : tapIntervals) avgMs += v;
        avgMs /= (double) tapIntervals.size();
        const double newBpm = juce::jlimit (40.0, 240.0, 60000.0 / avgMs);
        processorRef.setBpm (newBpm);
        processorRef.setTapCentreSeconds (avgMs / 1000.0);
        processorRef.setTapTempoActive   (true);
        processorRef.setTime             (0.5f);   // snap-to-centre
    }
    else
    {
        tapIntervals.clear();
    }
    lastTapMs = now;
    repaint();
}

//==============================================================================
void DriftAudioProcessorEditor::applyRandomTheme()
{
    auto& rnd = juce::Random::getSystemRandom();
    const float h = rnd.nextFloat();
    kAccent  = juce::Colour::fromHSV (h, 0.78f, 0.95f, 1.0f);
    kDispTxt = juce::Colour::fromHSV (h, 0.70f, 0.95f, 1.0f);
    kDispDim = juce::Colour::fromHSV (h, 0.65f, 0.55f, 1.0f);
    kDispRim = juce::Colour::fromHSV (h, 0.85f, 0.25f, 1.0f);
    retintControls();
    repaint();
}

void DriftAudioProcessorEditor::retintControls()
{
    for (auto& f : faders)
    {
        f.setColour (juce::Slider::trackColourId, kAccent);
        f.setColour (juce::Slider::thumbColourId, kAccent);
        f.repaint();
    }
    auto retintCycle = [] (juce::TextButton& b, juce::Colour accent)
    {
        b.setColour (juce::TextButton::buttonOnColourId, accent);
        b.repaint();
    };
    retintCycle (scaleButton,   kAccent);
    retintCycle (motionButton,  kAccent);
    retintCycle (modeButton,    kAccent);
    retintCycle (voicingButton, kAccent);
    retintCycle (stateButton,   kAccent);
    // SHIFT keeps its purple "meta-control" identity, not the theme accent.
    tapButton.setShapeColours (kAccent, kAccent.brighter (0.2f));
    tapButton.repaint();
    // linkButton intentionally NOT retinted — it's a status indicator
    // (off/amber/green/red) and stays universal across themes.
}

//==============================================================================
void DriftAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (wordmarkArea.contains (e.getPosition()))
        applyRandomTheme();
}

bool DriftAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        registerTap();
        return true;
    }
    const auto t = key.getTextCharacter();
    if (t == 'b' || t == 'B')
    {
        bypassButton.setToggleState (! bypassButton.getToggleState(), juce::sendNotification);
        return true;
    }
    if (t == 'h' || t == 'H')
    {
        // Mirror a click on the HOLD button — flips the toggle and dispatches
        // the mode-specific gesture (MOD=OVERLOAD, SHORT/LONG=HOLD, LOOP=DELETE).
        holdButton.setToggleState (! holdButton.getToggleState(), juce::dontSendNotification);
        toggleHoldGesture();
        return true;
    }
    if (t == 'd' || t == 'D')
    {
        // Quick way to wipe the current loop without dragging out the
        // right-foot HOLD gesture. Only meaningful in LOOP mode.
        if (processorRef.getMode() == DriftAudioProcessor::ModeLoop)
        {
            processorRef.requestLoopDelete();
            repaint();
            return true;
        }
    }
    if (t == 'o' || t == 'O')
    {
        if (optionsOverlay) optionsOverlay.reset();
        else                showOptionsOverlay();
        return true;
    }

    // ---- Preset slot shortcuts ----------------------------------------
    // Number keys 0..9 load a preset. Shift+0..9 saves into that slot.
    // 0 maps to slot 0 (NICE DELAY in Big Time's factory bank); 1..9 map
    // to slots 1..9. We don't ship factory presets yet, so empty slots
    // are no-ops on load.
    const int keyCode = key.getKeyCode();
    if (keyCode >= '0' && keyCode <= '9')
    {
        const int slot = keyCode - '0';
        if (key.getModifiers().isShiftDown())
        {
            processorRef.savePresetSlot (slot);
            // Flash the chassis briefly so the user sees the save fired.
            repaint();
            return true;
        }
        if (processorRef.loadPresetSlot (slot))
        {
            syncFadersToBindings();
            syncCycleButtonTexts();
            updatePresetNameLabel();
            repaint();
            return true;
        }
    }

    return false;
}

void DriftAudioProcessorEditor::timerCallback()
{
    // Keep fader positions in sync with the processor — MIDI CCs or
    // host automation may have moved them without going through the UI.
    auto syncFader = [this] (int idx, float currentVal)
    {
        if (std::abs ((float) faders[idx].getValue() - currentVal) > 0.001f
            && ! faders[idx].isMouseButtonDown())
        {
            faders[idx].setValue (currentVal, juce::dontSendNotification);
        }
    };
    syncFader (FaderColor,    processorRef.getColor());
    syncFader (FaderTime,     processorRef.getTime());
    syncFader (FaderCluster,  processorRef.getCluster());
    syncFader (FaderTilt,     processorRef.getTilt());
    syncFader (FaderFeedback, processorRef.getFeedback());
    syncFader (FaderWet,      processorRef.getWet());

    // Value readouts under each fader.
    faderValues[FaderColor]   .setText (juce::String ((int) (processorRef.getColor()   * 100.0f)) + "%", juce::dontSendNotification);
    faderValues[FaderTime]    .setText (juce::String ((int) processorRef.getCurrentDelayMs()) + " ms", juce::dontSendNotification);
    faderValues[FaderCluster] .setText (juce::String ((int) (processorRef.getCluster() * 100.0f)) + "%", juce::dontSendNotification);
    {
        const float t = processorRef.getTilt();
        faderValues[FaderTilt].setText (t < 0.49f ? juce::String::formatted ("DK %d", (int) ((0.5f - t) * 100.0f * 2.0f))
                                       : t > 0.51f ? juce::String::formatted ("BR %d", (int) ((t - 0.5f) * 100.0f * 2.0f))
                                       : juce::String ("FLAT"),
                                       juce::dontSendNotification);
    }
    faderValues[FaderFeedback].setText (juce::String ((int) (processorRef.getFeedback() * 100.0f)) + "%", juce::dontSendNotification);
    faderValues[FaderWet]     .setText (juce::String ((int) (processorRef.getWet()      * 100.0f)) + "%", juce::dontSendNotification);

    // Cycle-button labels keep up with external changes (host automation,
    // MIDI CC, preset recall). syncCycleButtonTexts is alt-aware — when
    // SHIFT is on, the buttons read their alt atomics instead.
    syncCycleButtonTexts();

    // Faders also need to track the right atomic when SHIFT is on so that
    // external changes to alt params show on the right knobs.
    syncFadersToBindings();

    // Bypass toggle keeps up with host bypass automation.
    if (bypassButton.getToggleState() != processorRef.isBypassed())
        bypassButton.setToggleState (processorRef.isBypassed(), juce::dontSendNotification);

    // LINK status LED + label. Off / Waiting / Linked / SR mismatch.
    if (linkButton.getToggleState() != processorRef.isLinkEnabled())
        linkButton.setToggleState (processorRef.isLinkEnabled(), juce::dontSendNotification);
    // LINQ status drives the button's ON colour. The button text stays
    // "LINQ" always — the colour is the LED, the body of the pill is the
    // light. OFF stays in the OFF colour regardless (handled by pillLnf
    // via the buttonColourId, which is the dim grey we set at startup).
    const bool linkOn = processorRef.isLinkEnabled();
    juce::Colour linkCol;
    if (! linkOn)
        linkCol = juce::Colour::fromRGB (0x1c, 0x14, 0x10);   // dim — not visible (button is OFF)
    else if (! processorRef.isLinkProducerAlive())
        linkCol = juce::Colour::fromRGB (0xff, 0xb4, 0x4a);   // amber — waiting
    else if (! processorRef.isLinkSampleRateMatched())
        linkCol = juce::Colour::fromRGB (0xe5, 0x3a, 0x3a);   // red — broken
    else
        linkCol = juce::Colour::fromRGB (0x4a, 0xc8, 0x6a);   // green — linked
    if (linkButton.findColour (juce::TextButton::buttonOnColourId) != linkCol)
    {
        linkButton.setColour (juce::TextButton::buttonOnColourId, linkCol);
        linkButton.repaint();
    }

    repaint (oledArea);

    // Re-paint each fader column so the LED meter strip beside it tracks
    // the live fader value. The slider's own bounds repaint when you drag,
    // but the LED dots are drawn by the editor's paint() outside the
    // slider's region — without this they'd stay frozen at startup state.
    for (int i = 0; i < kNumFaders; ++i)
        repaint (faderColumnBounds[i]);
}

//==============================================================================
void DriftAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto outer = getLocalBounds();

    // ---- 1) Outer brushed-steel frame ------------------------------------
    juce::ColourGradient bodyGrad (kBodyHi, 0.0f, 0.0f,
                                   kBodyDark, 0.0f, (float) outer.getHeight(), false);
    bodyGrad.addColour (0.5, kBodyMid);
    g.setGradientFill (bodyGrad);
    g.fillRoundedRectangle (outer.toFloat(), 12.0f);

    // ---- 2) Warm wood side cheeks (L + R) --------------------------------
    auto drawWoodCheek = [&] (juce::Rectangle<float> r, int seed)
    {
        // Vertical gradient: light at the inner edge, darker at the outer.
        juce::ColourGradient wood (kWoodHi,   r.getX(),     r.getY(),
                                   kWoodDark, r.getRight(), r.getY(), false);
        wood.addColour (0.55, kWoodMid);
        g.setGradientFill (wood);
        g.fillRect (r);

        // Procedural grain — irregular long vertical streaks.
        juce::Random rng (seed);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        for (int i = 0; i < 18; ++i)
        {
            const float x  = r.getX() + rng.nextFloat() * r.getWidth();
            const float y0 = r.getY() + rng.nextFloat() * r.getHeight() * 0.2f;
            const float h  = r.getHeight() * (0.5f + rng.nextFloat() * 0.5f);
            const float w  = 0.6f + rng.nextFloat() * 1.4f;
            g.fillRect (x, y0, w, h);
        }
        g.setColour (kWoodHi.withAlpha (0.15f));
        for (int i = 0; i < 10; ++i)
        {
            const float x  = r.getX() + rng.nextFloat() * r.getWidth();
            const float y0 = r.getY() + rng.nextFloat() * r.getHeight() * 0.4f;
            const float h  = r.getHeight() * (0.3f + rng.nextFloat() * 0.5f);
            g.fillRect (x, y0, 0.5f, h);
        }
        // Dark seam where the wood meets the faceplate.
        g.setColour (kWoodSeam.withAlpha (0.85f));
        g.drawVerticalLine ((int) (r.getCentreX() < (float) outer.getCentreX() ? r.getRight()
                                                                               : r.getX()),
                            r.getY(), r.getBottom());
    };
    const auto leftWood  = juce::Rectangle<float> (4.0f, 4.0f,
                                                   (float) kWoodW,
                                                   (float) outer.getHeight() - 8.0f);
    const auto rightWood = juce::Rectangle<float> ((float) outer.getWidth() - (float) kWoodW - 4.0f,
                                                   4.0f, (float) kWoodW,
                                                   (float) outer.getHeight() - 8.0f);
    drawWoodCheek (leftWood,  0x10A3);
    drawWoodCheek (rightWood, 0x5C42);

    // ---- 3) Charcoal slate faceplate -------------------------------------
    const auto faceOuter = juce::Rectangle<float> ((float) (kWoodW + 4),
                                                   4.0f,
                                                   (float) outer.getWidth() - 2.0f * (float) (kWoodW + 4),
                                                   (float) outer.getHeight() - 8.0f);
    juce::ColourGradient faceGrad (kPanelHi, faceOuter.getCentreX(), faceOuter.getY(),
                                   kPanelLo, faceOuter.getCentreX(), faceOuter.getBottom(),
                                   false);
    faceGrad.addColour (0.5, kPanelMid);
    g.setGradientFill (faceGrad);
    g.fillRect (faceOuter);
    g.setColour (kSeam.withAlpha (0.75f));
    g.drawRect (faceOuter, 0.7f);

    // ---- 4) Hex-head chrome screws at the corners ------------------------
    const float screwR = 7.0f;
    auto drawHexScrew = [&] (juce::Point<float> c)
    {
        // Outer chrome rim.
        juce::ColourGradient rim (kChromeHi, c.x - screwR, c.y - screwR,
                                  kChromeLo, c.x + screwR, c.y + screwR, true);
        rim.addColour (0.5, kChromeMid);
        g.setGradientFill (rim);
        g.fillEllipse (c.x - screwR, c.y - screwR, screwR * 2.0f, screwR * 2.0f);
        // Dark hex recess.
        g.setColour (kScrewDark);
        juce::Path hex;
        const float hr = screwR * 0.55f;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f
                          + juce::MathConstants<float>::pi / 6.0f;
            const float x = c.x + std::cos (a) * hr;
            const float y = c.y + std::sin (a) * hr;
            if (i == 0) hex.startNewSubPath (x, y); else hex.lineTo (x, y);
        }
        hex.closeSubPath();
        g.fillPath (hex);
        // Outer ring.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (c.x - screwR, c.y - screwR, screwR * 2.0f, screwR * 2.0f, 0.8f);
    };
    const float scInset = 13.0f;
    drawHexScrew ({ (float) kWoodW + 4.0f + scInset,                 scInset });
    drawHexScrew ({ (float) outer.getWidth() - (float) kWoodW - 4.0f - scInset, scInset });
    drawHexScrew ({ (float) kWoodW + 4.0f + scInset,
                    (float) outer.getHeight() - scInset });
    drawHexScrew ({ (float) outer.getWidth() - (float) kWoodW - 4.0f - scInset,
                    (float) outer.getHeight() - scInset });

    // Faceplate inner content area (used for sub-section layout below).
    const auto face = faceOuter.toNearestInt().reduced (kBezel);

    // ---- 5) Header: tiny technical wordmark + sub-tagline ----------------
    auto header = juce::Rectangle<int> (face.getX(), face.getY(),
                                        face.getWidth(), 38);
    // Wordmark now occupies only ~92 px so the LINK pill can sit
    // immediately to its right (matches SP-L's layout).
    wordmarkArea = header.removeFromLeft (92);
    g.setColour (kText);
    g.setFont   (juce::Font (juce::FontOptions { 30.0f, juce::Font::bold }));
    g.drawText  ("DF-T", wordmarkArea, juce::Justification::centredLeft, false);

    // Top-right: small block of accent micro-LEDs + serial-number style text.
    const auto headerRight = header;
    {
        const float ledR = 2.6f;
        const float ledY = (float) headerRight.getCentreY();
        const float ledX0 = (float) headerRight.getRight() - 8.0f;
        const juce::Colour ledRow[] = { kLedRed, kLedBlue, kLedPurple, kLedWhite };
        for (int i = 0; i < 4; ++i)
        {
            const float x = ledX0 - (float) i * (ledR * 2.0f + 4.0f);
            g.setColour (ledRow[i].withAlpha (0.85f));
            g.fillEllipse (x - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawEllipse (x - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f, 0.4f);
        }
        g.setColour (kTextTag);
        g.setFont   (juce::Font (juce::FontOptions { 7.5f, juce::Font::bold }));
        g.drawText  ("DRIFT/01  ITSELLIOTT  US",
                     headerRight.withTrimmedRight (8 + 4 * (int) (ledR * 2.0f + 4.0f)),
                     juce::Justification::centredRight, false);
    }

    // ---- 6) "ECHO ENGINE" section header bar -----------------------------
    auto echoBar = juce::Rectangle<int> (face.getX(), face.getY() + 42,
                                         face.getWidth(), 16);
    g.setColour (kPanelHi.darker (0.4f));
    g.drawHorizontalLine (echoBar.getCentreY(), (float) echoBar.getX(),
                          (float) echoBar.getRight());
    g.setColour (kText);
    g.setFont   (juce::Font (juce::FontOptions { 9.0f, juce::Font::bold }));
    // Fill the section title with the panel colour so the line passes
    // through the text without visually crossing it.
    {
        auto titleRect = echoBar.withSizeKeepingCentre (96, 14);
        g.setColour (kPanelMid);
        g.fillRect (titleRect);
        g.setColour (kText);
        g.drawText ("ECHO ENGINE", titleRect, juce::Justification::centred, false);
    }

    // ---- 7) Per-fader sub-labels + LED column meters ---------------------
    for (int i = 0; i < kNumFaders; ++i)
    {
        const auto col = faderColumnBounds[i];
        if (col.isEmpty()) continue;

        // Engineering-style sub-tag above the column.
        g.setColour (kTextTag);
        g.setFont   (juce::Font (juce::FontOptions { 8.0f, juce::Font::bold }));
        const char* sub = nullptr;
        switch (i)
        {
            case FaderColor:    sub = "PREAMP";    break;
            case FaderTime:     sub = "DELAY";     break;
            case FaderCluster:  sub = "DIFFUSION"; break;
            case FaderTilt:     sub = "TILT EQ";   break;
            case FaderFeedback: sub = "REGEN";     break;
            case FaderWet:      sub = "MIX";       break;
        }
        if (sub != nullptr)
            g.drawText (sub, col.withHeight (12).reduced (0, 1),
                        juce::Justification::centred, false);

        // Per-fader micro-LED indicator (between sub-tag and main label).
        {
            const float ledR = 2.5f;
            const float cx   = (float) col.getCentreX();
            const float cy   = (float) col.getY() + 18.0f;
            const auto  led  = kFaderLedColours[i];
            // Halo glow.
            for (int k = 3; k > 0; --k)
                g.setColour (led.withAlpha (0.10f / (float) k)),
                g.fillEllipse (cx - ledR * (1.0f + k * 0.4f),
                               cy - ledR * (1.0f + k * 0.4f),
                               ledR * 2.0f * (1.0f + k * 0.4f),
                               ledR * 2.0f * (1.0f + k * 0.4f));
            g.setColour (led);
            g.fillEllipse (cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawEllipse (cx - ledR, cy - ledR, ledR * 2.0f, ledR * 2.0f, 0.4f);
        }

        // LED column meter — sits to the LEFT of the fader, showing fader
        // position as a 13-step bar graph. The Automatone signifier.
        {
            const int kNumLeds = 13;
            const auto faderRect = faders[i].getBounds();
            if (! faderRect.isEmpty())
            {
                const float stripX = (float) col.getX() + 6.0f;
                const float stripY = (float) faderRect.getY() + 8.0f;
                const float stripH = (float) faderRect.getHeight() - 16.0f;
                const float dotR   = 2.2f;
                const float value  = juce::jlimit (0.0f, 1.0f,
                                                   (float) faders[i].getValue());
                const int   active = (int) std::round (value * (float) (kNumLeds - 1));
                const auto litCol  = kFaderLedColours[i];
                for (int k = 0; k < kNumLeds; ++k)
                {
                    const float yT = stripY + stripH * (float) k / (float) (kNumLeds - 1);
                    const bool on = (kNumLeds - 1 - k) <= active;
                    g.setColour (on ? litCol.withAlpha (0.95f)
                                    : juce::Colour::fromRGB (0x18, 0x18, 0x1e));
                    g.fillEllipse (stripX - dotR, yT - dotR, dotR * 2.0f, dotR * 2.0f);
                    if (on)
                    {
                        g.setColour (litCol.withAlpha (0.20f));
                        g.fillEllipse (stripX - dotR * 1.8f, yT - dotR * 1.8f,
                                       dotR * 3.6f, dotR * 3.6f);
                    }
                }
            }
        }
    }

    // ---- 8) Cycle-button column labels -----------------------------------
    // Engineering-style tag above each of the 5 cycle buttons. Names switch
    // when SHIFT is engaged so the user can tell which atomic each button
    // is currently controlling. Computed off the button bounds set in
    // resized() so the labels track with the row's position.
    {
        const juce::TextButton* const cycleBtns[5] = {
            &scaleButton, &motionButton, &modeButton, &voicingButton, &stateButton
        };
        const char* const mainTags [5] = { "SCALE", "MOTION", "MODE", "VOICING", "STATE" };
        const char* const altTags  [5] = { "SPREAD", "0.5X", "DIFFUSE T", "+12dB", "—" };
        g.setColour (kTextTag);
        g.setFont   (juce::Font (juce::FontOptions { 8.0f, juce::Font::bold }));
        for (int i = 0; i < 5; ++i)
        {
            const auto b = cycleBtns[i]->getBounds();
            if (b.isEmpty()) continue;
            const juce::Rectangle<int> tag (b.getX(), b.getY() - 12, b.getWidth(), 10);
            g.drawText (altMenuActive ? altTags[i] : mainTags[i], tag,
                        juce::Justification::centred, false);
        }
    }

    // ---- 8b) Alt-menu "A" indicator next to SHIFT ------------------------
    // Per the Big Time manual the display blinks "A" while the Alt Menu is
    // active. We use a small green LED dot beside the SHIFT button so the
    // user has an unambiguous visual confirmation of the binding switch.
    if (altMenuActive)
    {
        const auto sb = shiftButton.getBounds();
        const float r = 4.0f;
        const float cx = (float) sb.getRight() + 8.0f;
        const float cy = (float) sb.getCentreY();
        for (int k = 3; k > 0; --k)
            g.setColour (kLedGreen.withAlpha (0.10f / (float) k)),
            g.fillEllipse (cx - r * (1.0f + k * 0.4f),
                           cy - r * (1.0f + k * 0.4f),
                           r * 2.0f * (1.0f + k * 0.4f),
                           r * 2.0f * (1.0f + k * 0.4f));
        g.setColour (kLedGreen);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // ---- 9) Twin OLED panels — tight to digit content, no inner labels ----
    // Sized so each digit fills nearly the full body height. The previous
    // version had dead space because it reserved a slug for "MS"/"SLOT"
    // labels inside the box — those labels are gone now; the header above
    // each box already identifies what it shows. The preset cycler arrows
    // sit directly beneath the PRESET box instead of in the footer.
    {
        constexpr int dispH       = 34;
        constexpr int delayDispW  = 112;
        constexpr int presetDispW = 56;
        constexpr int gap         = 14;
        const int     totalW      = delayDispW + gap + presetDispW;
        const int     dispY       = face.getBottom() - 163;
        const int     leftX       = face.getX() + face.getWidth() / 2 - totalW / 2;
        const int     rightX      = leftX + delayDispW + gap;

        const auto disp       = juce::Rectangle<int> (leftX,  dispY, delayDispW,  dispH);
        const auto presetDisp = juce::Rectangle<int> (rightX, dispY, presetDispW, dispH);

        // ---- Header tags above each OLED --------------------------------
        const auto labelL = juce::Rectangle<int> (disp.getX(),       disp.getY()       - 13, disp.getWidth(),       11);
        const auto labelR = juce::Rectangle<int> (presetDisp.getX(), presetDisp.getY() - 13, presetDisp.getWidth(), 11);
        g.setColour (kTextTag);
        g.setFont   (juce::Font (juce::FontOptions { 8.0f, juce::Font::bold }));
        g.drawText  ("DELAY (MS)", labelL, juce::Justification::centred, false);
        g.drawText  ("PRESET",     labelR, juce::Justification::centred, false);

        // ---- Helper: paint a themed display body ------------------------
        auto paintBody = [&] (juce::Rectangle<int> r)
        {
            const auto rf = r.toFloat();
            g.setColour (kDispBg);
            g.fillRect  (rf);
            g.setColour (juce::Colours::black);
            g.drawRect  (rf, 1.0f);
            g.setColour (kDispRim);
            g.drawRect  (rf.reduced (1.0f), 1.2f);
        };

        // ---- DELAY TIME body — digits fill the box ----------------------
        paintBody (disp);
        {
            const int ms = juce::jlimit (0, 9999, (int) processorRef.getCurrentDelayMs());
            draw7SegNumber (g, disp.reduced (5, 3).toFloat(), ms, 4, kDispTxt, kDispDim);
        }

        // ---- PRESET body — just two tight digits, no inner label --------
        paintBody (presetDisp);
        {
            const int slot = processorRef.getCurrentPresetSlot();
            const auto digits = presetDisp.reduced (5, 3).toFloat();
            if (slot < 0)
                draw7SegNumber (g, digits, -1, 2, kDispDim, kDispDim);
            else
                draw7SegNumber (g, digits, slot + 1, 2, kDispTxt, kDispDim);
        }

        // ---- Bypass indicator dot on the DELAY TIME panel ---------------
        if (processorRef.isBypassed())
        {
            const float dotR = 2.5f;
            g.setColour (kBypass);
            g.fillEllipse ((float) disp.getRight() - dotR * 2.0f - 4.0f,
                           (float) disp.getY() + 4.0f, dotR * 2.0f, dotR * 2.0f);
        }

        // Stash union (incl. header labels) so the timer repaints everything.
        oledArea = labelL.getUnion (labelR).getUnion (disp).getUnion (presetDisp);
    }

    // ---- 10) Footswitch sub-labels (drawn here so they sit on faceplate) -
    // The actual ICON/circle is drawn by the FootswitchLookAndFeel; we just
    // paint "BYPASS" / "TAP TEMPO" text in the panel area between them.
}

void DriftAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // Subtle outer-frame highlight on top of everything.
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (getLocalBounds().reduced (1).toFloat(), 14.0f, 1.0f);
}

//==============================================================================
void DriftAudioProcessorEditor::resized()
{
    // Faceplate content rect — same calc as paint() uses for `face`. Lives
    // inside the wood cheeks + bezel.
    const juce::Rectangle<int> face (kWoodW + 4 + kBezel, kBezel + 4,
                                     getWidth()  - 2 * (kWoodW + 4 + kBezel),
                                     getHeight() - 2 * (kBezel + 4));
    auto layout = face;

    // ---- Top: header (38) + gap + ECHO ENGINE bar (16) + gap -------------
    // RESET pill — small, lives just right of the wordmark so it's reachable
    // without crowding the accent-LED serial-number strip on the far right.
    resetButton.setBounds (face.getX() + 200, face.getY() + 9, 64, 20);
    layout.removeFromTop (38);  // header (wordmark + tagline, painted)
    layout.removeFromTop (4);
    layout.removeFromTop (16);  // echo engine section bar (painted)
    layout.removeFromTop (4);

    // ---- Bottom: footswitches (82) + small gap ---------------------------
    layout.removeFromBottom (4);
    auto footer = layout.removeFromBottom (82);
    {
        const int swSize = 70;
        const int swY    = footer.getY() + 4;
        const auto bypassRect = juce::Rectangle<int> (footer.getX() + 36, swY,
                                                      swSize, swSize);
        const auto tapRect    = juce::Rectangle<int> (footer.getRight() - 36 - swSize,
                                                      swY, swSize, swSize);
        bypassButton.setBounds (bypassRect);
        tapButton   .setBounds (tapRect);
        bypassLabel.setBounds (bypassRect.getX(), bypassRect.getBottom() + 1,
                               bypassRect.getWidth(), 12);
        tapLabel   .setBounds (tapRect.getX(),    tapRect.getBottom() + 1,
                               tapRect.getWidth(), 12);

        // HOLD pill — sticky-toggle gesture button. Sits centred between
        // BYPASS and TAP in the footer (no more LINK here — moved to the
        // header next to the wordmark per the user's "less prominent" ask).
        const int  holdW = 64;
        const int  holdH = 22;
        const auto holdRect = juce::Rectangle<int> (footer.getCentreX() - holdW / 2,
                                                    bypassRect.getCentreY() - holdH / 2,
                                                    holdW, holdH);
        holdButton.setBounds (holdRect);
    }

    // LINQ pill — small, next to DF-T wordmark in the header row (matches
    // SP-L's placement). The pill itself IS the indicator — its colour
    // shows status (off / waiting / linked / broken). No separate label.
    {
        const int linkW = 52;
        const int linkH = 18;
        const int linkX = face.getX() + 96;            // just right of "DF-T"
        const int linkY = face.getY() + 9 + (20 - linkH) / 2;
        linkButton.setBounds (linkX, linkY, linkW, linkH);
        linkStatusLabel.setBounds (0, 0, 0, 0);        // hidden — kept in the class for state-load compat
    }

    // ---- Twin 7-seg displays + preset cycler beneath PRESET OLED ---------
    // Painted block: DELAY(MS) label + body, PRESET label + body.
    // Live components: prev/next chevrons directly under the PRESET OLED,
    // preset NAME label centred below the two displays.
    layout.removeFromBottom (8);
    layout.removeFromBottom (90);  // OLED block (header + bodies + arrows + name)
    {
        // OLED geometry mirrors the paint() block exactly.
        constexpr int dispH       = 34;
        constexpr int delayDispW  = 112;
        constexpr int presetDispW = 56;
        constexpr int gapW        = 14;
        const int     totalW      = delayDispW + gapW + presetDispW;
        const int     dispY       = face.getBottom() - 163;
        const int     leftX       = face.getX() + face.getWidth() / 2 - totalW / 2;
        const int     presetX     = leftX + delayDispW + gapW;
        const int     presetBot   = dispY + dispH;

        // Small discrete chevrons under the PRESET OLED — half-width each
        // so they fit beneath the 56-wide preset body with a 2-px gap.
        constexpr int arrowH = 16;
        constexpr int arrowW = 26;
        const int     arrowY = presetBot + 4;
        // Centre the pair under the preset OLED.
        const int     prevX  = presetX + presetDispW / 2 - arrowW - 1;
        const int     nextX  = presetX + presetDispW / 2 + 1;
        presetPrevButton.setBounds (prevX, arrowY, arrowW, arrowH);
        presetNextButton.setBounds (nextX, arrowY, arrowW, arrowH);

        // Preset name as a small tag centred under both displays.
        presetNameLabel.setBounds (leftX, arrowY + arrowH + 3, totalW, 11);
    }

    // ---- Cycle-button row: SCALE / MOTION / MODE / VOICING / STATE -------
    auto cycleRow = layout.removeFromBottom (36);
    {
        constexpr int btnW = 80;
        constexpr int gap  = 6;
        const int totalW = btnW * 5 + gap * 4;
        const int xStart = cycleRow.getCentreX() - totalW / 2;
        juce::TextButton* const cycleBtns[5] = {
            &scaleButton, &motionButton, &modeButton, &voicingButton, &stateButton
        };
        for (int i = 0; i < 5; ++i)
            cycleBtns[i]->setBounds (xStart + i * (btnW + gap),
                                     cycleRow.getY() + 4,
                                     btnW, cycleRow.getHeight() - 8);
    }

    // SHIFT — placed in the top header strip alongside RESET.
    shiftButton.setBounds (face.getX() + 272, face.getY() + 9, 64, 20);
    layout.removeFromBottom (4);   // gap
    layout.removeFromBottom (14);  // mode/state section bar (painted)
    layout.removeFromBottom (4);

    // ---- Fader grid fills the remaining middle ---------------------------
    auto faderArea = layout;
    const int margin = 8;
    const int colW = (faderArea.getWidth() - margin * 2) / kNumFaders;
    for (int i = 0; i < kNumFaders; ++i)
    {
        const int xL = faderArea.getX() + margin + i * colW;
        const auto col = juce::Rectangle<int> (xL, faderArea.getY(),
                                               colW, faderArea.getHeight());
        faderColumnBounds[i] = col;

        // Reserve top 14 px for the PREAMP/DELAY/etc. sub-tag, next 12 px
        // for the micro-LED (both painted), then the main label.
        auto inner = col;
        inner.removeFromTop (14);                       // sub-tag (painted)
        inner.removeFromTop (12);                       // micro-LED row (painted)
        faderLabels[i].setBounds (inner.removeFromTop (16));
        faderValues[i].setBounds (inner.removeFromBottom (16));

        // Fader sits to the RIGHT of the LED column meter (which is painted
        // at col.x + 6). Leave a 12 px gutter on the left + 6 px on the right.
        faders[i].setBounds (inner.withTrimmedLeft (12).withTrimmedRight (6));
    }

    if (aboutOverlay)   aboutOverlay  ->setBounds (getLocalBounds());
    if (optionsOverlay) optionsOverlay->setBounds (getLocalBounds());
}

//==============================================================================
void DriftAudioProcessorEditor::showOptionsOverlay()
{
    optionsOverlay.reset (new OptionsOverlay (*this));
    addAndMakeVisible (optionsOverlay.get());
    optionsOverlay->setBounds (getLocalBounds());
    optionsOverlay->toFront (true);
    optionsOverlay->grabKeyboardFocus();
}

void DriftAudioProcessorEditor::showAboutOverlay()
{
    aboutOverlay.reset (new AboutOverlay (*this));
    addAndMakeVisible (aboutOverlay.get());
    aboutOverlay->setBounds (getLocalBounds());
    aboutOverlay->toFront (true);
}
