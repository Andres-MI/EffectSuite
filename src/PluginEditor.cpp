#include "PluginEditor.h"

namespace
{
/** Long knob captions get a short form when the cell is too narrow to show them at a legible size. */
juce::String knobCaption (const juce::String& full, int cellWidth)
{
    if (cellWidth >= 36)
        return full;

    static const std::pair<const char*, const char*> shorts[] = {
        { "Feedback", "Fdbk" }, { "Sustain", "Sust" }, { "Attack", "Atk" }, { "Resonance", "Res" },
        { "Position", "Pos" }, { "Threshold", "Thr" }, { "Depth", "Depth" } };
    for (auto& s : shorts)
        if (full == s.first)
            return s.second;
    return full;
}

const juce::Colour bgColour      { 0xff14161a };
const juce::Colour boardColour   { 0xff0e1013 };
const juce::Colour accentColour  { 0xffe8a33d };
const juce::Colour textColour    { 0xffd8dce2 };
const juce::Colour dimTextColour { 0xff8b929c };
const juce::Colour okColour      { 0xff4cd07a };
const juce::Colour darkInk       { 0xff15171a };

constexpr float meterFloorDb = -60.0f;

/** Small text never goes below a legible size, whatever the window scale. */
float smallFont (float base, float scale)   { return juce::jmax (9.0f, base * scale); }

juce::Colour contrastFor (juce::Colour c)
{
    return c.getPerceivedBrightness() > 0.62f ? darkInk : juce::Colours::white;
}

/** Enclosure colour of a switched-off pedal: its own colour, only mildly darkened. */
juce::Colour bypassedColour (juce::Colour c)
{
    return c.withMultipliedSaturation (0.92f).withMultipliedBrightness (0.78f);
}

/** The colour as drawn: unchanged when on, mildly darkened when off. */
juce::Colour shaded (juce::Colour c, bool on)
{
    return on ? c : c.withMultipliedBrightness (0.78f);
}

/** Chip fill of a switched-off block: a muted, darker version of its colour, never grey. */
juce::Colour mutedChipColour (juce::Colour c)
{
    return c.withMultipliedSaturation (0.8f).withMultipliedBrightness (0.45f);
}

const juce::Colour ledRed { 0xffff2a1e };

void drawLight (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
{
    if (on)
    {
        const float glow = radius * 3.2f;
        juce::ColourGradient halo (colour.withAlpha (0.75f), centre.x, centre.y,
                                   colour.withAlpha (0.0f), centre.x + glow, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (centre.x - glow, centre.y - glow, glow * 2.0f, glow * 2.0f);
    }

    g.setColour (juce::Colour (0xff08090a));
    g.fillEllipse (centre.x - radius * 1.3f, centre.y - radius * 1.3f, radius * 2.6f, radius * 2.6f);
    g.setColour (on ? colour.brighter (0.3f) : colour.withMultipliedSaturation (0.6f).withBrightness (0.18f));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    if (on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillEllipse (centre.x - radius * 0.5f, centre.y - radius * 0.6f, radius * 0.6f, radius * 0.5f);
    }
}

/** Woven cloth: two sets of diagonal lines clipped to the area. */
void drawGrille (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour cloth, float pitch, float corner)
{
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, corner);
    g.reduceClipRegion (clip);
    g.setColour (cloth);
    g.fillRect (area);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    for (float x = area.getX() - area.getHeight(); x < area.getRight(); x += pitch)
    {
        g.drawLine (x, area.getY(), x + area.getHeight(), area.getBottom(), 1.0f);
        g.drawLine (x, area.getBottom(), x + area.getHeight(), area.getY(), 1.0f);
    }
}
}

struct EffectSuiteEditor::PedalDef
{
    struct KnobDef
    {
        const char* label;
        const char* paramId;
        ValueStyle style = ValueStyle::unit;
    };

    const char* name;       // UTF-8, already upper case (String::toUpperCase leaves accented letters alone)
    const char* chip;       // short name for the signal-flow strip
    juce::uint32 colour;
    const char* onParamId;
    const char* comboParamId;   // nullptr if the pedal has no selector
    std::vector<KnobDef> knobs;
    bool sliders = false;       // vertical sliders (graphic EQ) instead of knobs
    bool looper = false;        // looper layout and behaviour
};

//==============================================================================
EffectSuiteEditor::RigLookAndFeel::RigLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff22252b));
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f49));
    setColour (juce::ComboBox::arrowColourId, dimTextColour);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff23262d));
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.darker (0.4f));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e36));
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::Label::textColourId, textColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff343942));
    setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    setColour (juce::Slider::thumbColourId, juce::Colour (0xff22252b));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff23262d));
    setColour (juce::BubbleComponent::outlineColourId, accentColour);
    setColour (juce::TooltipWindow::textColourId, textColour);
}

void EffectSuiteEditor::RigLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                                          float pos, float startAngle, float endAngle,
                                                          juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax (2.0f, radius * 0.13f);
    const float arcR = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, stroke);

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, stroke);

    const float bodyR = arcR - thickness * 1.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f48), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff16181c), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff050506));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    juce::Path pointer;
    const float pw = juce::jmax (2.0f, bodyR * 0.14f);
    pointer.addRoundedRectangle (-pw * 0.5f, -bodyR + 1.5f, pw, bodyR * 0.55f, pw * 0.5f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void EffectSuiteEditor::RigLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                                          float sliderPos, float minPos, float maxPos,
                                                          juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        return;
    }

    // Graphic-EQ fader: slot, centre (0 dB) mark and a flat cap
    const float cx = (float) x + (float) w * 0.5f;
    const float slotW = juce::jmax (3.0f, (float) w * 0.14f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (cx - slotW * 0.5f, (float) y, slotW, (float) h, slotW * 0.5f);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (0.6f));
    g.fillRect (cx - (float) w * 0.38f, (float) y + (float) h * 0.5f - 0.5f, (float) w * 0.76f, 1.0f);

    const float capW = juce::jmin ((float) w * 0.8f, (float) h * 0.5f), capH = juce::jmax (8.0f, capW * 0.5f);
    const auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, sliderPos });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4f58), cx, cap.getY(),
                                             juce::Colour (0xff17191d), cx, cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (juce::Colour (0xff050506));
    g.drawRoundedRectangle (cap, 2.0f, 1.0f);
    g.setColour (juce::Colours::white);
    g.fillRect (cap.getX() + 2.0f, sliderPos - 1.0f, capW - 4.0f, 2.0f);
}

void EffectSuiteEditor::RigLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                                          bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (r.getHeight() * 0.6f, r.getWidth() * 0.28f, 22.0f);
    auto sw = juce::Rectangle<float> (d * 1.8f, d).withCentre ({ r.getX() + d * 0.9f + 2.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    g.setColour (on ? accentColour : juce::Colour (0xff343942));
    g.fillRoundedRectangle (sw, d * 0.5f);
    const float kx = on ? sw.getRight() - d * 0.5f : sw.getX() + d * 0.5f;
    g.setColour (highlighted ? juce::Colours::white : juce::Colour (0xffeceff3));
    g.fillEllipse (kx - d * 0.38f, sw.getCentreY() - d * 0.38f, d * 0.76f, d * 0.76f);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (juce::jlimit (9.0f, 18.0f, d * 0.8f)));
    g.drawFittedText (b.getButtonText(), r.withLeft (sw.getRight() + d * 0.3f).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.7f);
}

juce::Font EffectSuiteEditor::RigLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    // Scales with the box so long model names still fit at the minimum editor size
    return juce::FontOptions (juce::jlimit (9.5f, 15.0f, (float) box.getHeight() * 0.6f));
}

juce::Font EffectSuiteEditor::RigLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jlimit (9.5f, 15.0f, (float) buttonHeight * 0.55f));
}

//==============================================================================
void EffectSuiteEditor::Meter::pushLevel (float linearPeak)
{
    const float db = juce::Decibels::gainToDecibels (linearPeak, -100.0f);
    levelDb = db > levelDb ? db : juce::jmax (db, levelDb - 2.0f); // ~30 Hz timer
    if (db >= holdDb || --holdCounter <= 0)
    {
        if (db >= holdDb) { holdDb = db; holdCounter = 45; }
        else holdDb = juce::jmax (db, holdDb - 1.5f);
    }
    repaint();
}

void EffectSuiteEditor::Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0f1115));
    g.fillRoundedRectangle (r, 3.0f);
    r = r.reduced (2.0f);

    auto yFor = [&] (float db)
    {
        const float p = juce::jlimit (0.0f, 1.0f, (db - meterFloorDb) / -meterFloorDb);
        return r.getBottom() - p * r.getHeight();
    };

    const float top = yFor (levelDb);
    const float redY = yFor (-1.0f);
    const float yellowY = yFor (-12.0f);
    auto fill = [&] (float y0, float y1, juce::Colour c)
    {
        y0 = juce::jmax (y0, top);
        if (y1 > y0) { g.setColour (c); g.fillRect (r.getX(), y0, r.getWidth(), y1 - y0); }
    };
    fill (redY, r.getBottom(), okColour);
    fill (yellowY, r.getBottom(), okColour);
    fill (redY, yellowY, accentColour);
    fill (top, redY, juce::Colour (0xffe5484d));

    if (holdDb > meterFloorDb)
    {
        g.setColour (holdDb > -1.0f ? juce::Colour (0xffe5484d) : textColour);
        g.fillRect (r.getX(), yFor (holdDb) - 1.0f, r.getWidth(), 2.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f })
        g.fillRect (r.getX(), yFor (db), r.getWidth() * 0.3f, 1.0f);
}

//==============================================================================
void EffectSuiteEditor::FootSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 2.0f;
    r = r.withSizeKeepingCentre (d, d);
    const auto c = r.getCentre();

    // Nut / bezel
    g.setColour (juce::Colour (0xff0a0b0c));
    g.fillEllipse (r);
    auto bezel = r.reduced (d * 0.05f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef1f4), bezel.getX(), bezel.getY(),
                                             juce::Colour (0xff5a6068), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);

    // Plunger: lit from the opposite side so it reads as domed; it sinks slightly when pressed
    auto cap = r.reduced (d * (down ? 0.2f : 0.17f));
    g.setColour (juce::Colour (0xff2a2d31));
    g.fillEllipse (cap.expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xffb9bec5 : 0xffdfe3e8), c.x, cap.getY(),
                                             juce::Colour (down ? 0xff70767e : 0xff8b9199), c.x, cap.getBottom(), false));
    g.fillEllipse (cap);

    if (highlighted && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (cap);
    }
}

void EffectSuiteEditor::PowerSwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const float corner = r.getWidth() * 0.18f;

    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (r, corner);
    auto rocker = r.reduced (r.getWidth() * 0.12f);
    auto top = rocker.removeFromTop (rocker.getHeight() * 0.5f);

    // The pressed half is darker; the raised half catches the light
    const juce::Colour base = on ? juce::Colour (0xffd8432f) : juce::Colour (0xff4a4e55);
    g.setColour (on ? base.darker (0.5f) : base.brighter (highlighted ? 0.35f : 0.2f));
    g.fillRoundedRectangle (top, corner * 0.6f);
    g.setColour (on ? base.brighter (highlighted ? 0.3f : 0.1f) : base.darker (0.5f));
    g.fillRoundedRectangle (rocker, corner * 0.6f);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    const float m = r.getWidth() * 0.12f;
    g.fillRect (juce::Rectangle<float> (juce::jmax (1.5f, m * 0.5f), m * 2.0f).withCentre (rocker.getCentre()));
    g.drawEllipse (juce::Rectangle<float> (m * 2.0f, m * 2.0f).withCentre (top.getCentre()), juce::jmax (1.0f, m * 0.4f));
}

void EffectSuiteEditor::RemoveButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool hot = highlighted || down;
    g.setColour (juce::Colours::black.withAlpha (hot ? 0.85f : 0.3f));
    g.fillEllipse (r);
    g.setColour ((hot ? juce::Colour (0xffff6b6b) : juce::Colours::white).withAlpha (hot ? 1.0f : 0.6f));
    const auto c = r.getCentre();
    const float a = r.getWidth() * 0.2f;
    const float t = juce::jmax (1.3f, r.getWidth() * 0.09f);
    g.drawLine (c.x - a, c.y - a, c.x + a, c.y + a, t);
    g.drawLine (c.x - a, c.y + a, c.x + a, c.y - a, t);
}

void EffectSuiteEditor::StripButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto full = getLocalBounds().toFloat();
    auto r = full.reduced (1.0f);
    const float a = isEnabled() ? 1.0f : 0.35f;
    const float h = r.getHeight();
    const bool textShown = showText && getWidth() > (int) (h * 1.8f);
    const auto box = textShown ? r.removeFromLeft (h) : r.withSizeKeepingCentre (juce::jmin (h, r.getWidth()), h);
    const juce::Colour red (0xffe5484d);

    if (kind != Kind::record)
    {
        g.setColour (juce::Colour (0xff2a2e36).brighter (down ? 0.25f : (highlighted ? 0.12f : 0.0f)).withAlpha (a));
        g.fillRoundedRectangle (full.reduced (0.5f), 4.0f);
        g.setColour (juce::Colour (0xff3a3f49).withAlpha (a));
        g.drawRoundedRectangle (full.reduced (0.5f), 4.0f, 1.0f);
    }

    const auto ic = box.reduced (h * 0.22f);
    if (kind == Kind::record)
    {
        const float pulse = active ? 0.7f + 0.3f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.009f) : 1.0f;
        g.setColour ((active || highlighted ? red : juce::Colour (0xffb04a4e)).withAlpha (a));
        g.drawEllipse (box.reduced (h * 0.05f), juce::jmax (1.5f, h * 0.08f));
        g.setColour (red.withAlpha (pulse * a));
        if (active)
            g.fillRoundedRectangle (ic.reduced (h * 0.04f), 2.0f);
        else
            g.fillEllipse (ic.reduced (h * 0.02f));
    }
    else if (kind == Kind::folder)
    {
        g.setColour (accentColour.withAlpha (a));
        g.fillRoundedRectangle (ic.withTrimmedTop (ic.getHeight() * 0.2f), 2.0f);
        g.fillRoundedRectangle (ic.withHeight (ic.getHeight() * 0.35f).withWidth (ic.getWidth() * 0.5f), 1.5f);
    }
    else
    {
        juce::Path tri;
        tri.addTriangle (ic.getX() + ic.getWidth() * 0.1f, ic.getY(), ic.getX() + ic.getWidth() * 0.1f, ic.getBottom(),
                         ic.getRight(), ic.getCentreY());
        g.setColour (okColour.withAlpha (a));
        g.fillPath (tri);
    }

    if (textShown)
    {
        g.setColour ((active ? red : textColour).withAlpha (a));
        g.setFont (juce::FontOptions (juce::jlimit (9.5f, 15.0f, h * 0.6f)));
        g.drawFittedText (active && activeText.isNotEmpty() ? activeText : text, r.toNearestInt(),
                          kind == Kind::record ? juce::Justification::centredLeft : juce::Justification::centred, 1, 0.8f);
    }
}

void EffectSuiteEditor::AddTile::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float corner = 10.0f * scale;
    const auto ink = hover ? accentColour : juce::Colour (0xff6d747e);

    if (hover)
    {
        g.setColour (accentColour.withAlpha (0.08f));
        g.fillRoundedRectangle (r, corner);
    }

    juce::Path outline, dashed;
    outline.addRoundedRectangle (r, corner);
    const float dashes[] = { 7.0f * scale + 2.0f, 5.0f * scale + 2.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (ink);
    g.fillPath (dashed);

    const float d = juce::jmin (r.getWidth() * 0.2f, r.getHeight() * 0.11f);
    const auto c = juce::Point<float> (r.getCentreX(), r.getCentreY() - r.getHeight() * 0.07f);
    g.drawLine (c.x - d, c.y, c.x + d, c.y, juce::jmax (2.0f, d * 0.16f));
    g.drawLine (c.x, c.y - d, c.x, c.y + d, juce::jmax (2.0f, d * 0.16f));

    if (scale < 0.72f || r.getWidth() < 70.0f)
        return;   // too small for a legible caption: the plus sign is enough

    g.setFont (juce::FontOptions (smallFont (12.5f, scale), juce::Font::bold));
    g.drawFittedText (juce::CharPointer_UTF8 ("A\xc3\xb1""adir pedal"),
                      juce::Rectangle<int> ((int) r.getX() + 4, (int) (c.y + d + 12.0f * scale),
                                            (int) r.getWidth() - 8, (int) (22.0f * scale)),
                      juce::Justification::centred, 2, 0.7f);
}

//==============================================================================
EffectSuiteEditor::Stompbox::Stompbox (EffectSuiteEditor& e, const PedalDef& def)
    : editor (e), name (juce::CharPointer_UTF8 (def.name)),
      id (juce::String (def.onParamId).dropLastCharacters (2)), chipText (def.chip), colour (def.colour), sliders (def.sliders), looper (def.looper)
{
    for (auto& k : def.knobs)
        knobs.push_back (editor.createKnob (*this, k.label, k.paramId, k.style, sliders));

    if (def.comboParamId != nullptr)
    {
        hasCombo = true;
        editor.fillCombo (combo, def.comboParamId);
        addAndMakeVisible (combo);
        comboAttachment = std::make_unique<ComboAttachment> (editor.processor.apvts, def.comboParamId, combo);
    }

    footSwitch.setTooltip (looper ? juce::String::fromUTF8 ("Grabar / reproducir / sobregrabar")
                                  : name);
    addAndMakeVisible (footSwitch);
    if (! looper)
        switchAttachment = std::make_unique<ButtonAttachment> (editor.processor.apvts, def.onParamId, footSwitch);

    removeButton.setTooltip (juce::CharPointer_UTF8 ("Quitar de la pedalera"));
    removeButton.onClick = [this] { editor.removeFromBoard (id); };
    addAndMakeVisible (removeButton);

    if (looper)
    {
        // Not an on/off pedal: it is "on" while it is on the board
        footSwitch.setClickingTogglesState (false);
        footSwitch.onClick = [this] { editor.processor.looperPress(); updateLooper(); editor.blockStateChanged(); };
            stopButton.setTooltip (juce::String::fromUTF8 ("Parar la reproducción"));
        stopButton.onClick = [this] { editor.processor.looperStop(); updateLooper(); editor.blockStateChanged(); };
        clearButton.setTooltip ("Borrar el bucle");
        clearButton.onClick = [this] { editor.processor.looperClear(); updateLooper(); editor.blockStateChanged(); };
        addAndMakeVisible (stopButton);
        addAndMakeVisible (clearButton);
        lastOn = true;
        refresh();
        updateLooper();
        return;
    }

    setMouseCursor (juce::MouseCursor::DraggingHandCursor);   // the enclosure can be grabbed; children keep their own cursors

    lastOn = footSwitch.getToggleState();
    refresh();

    // Fires for clicks, automation and state restore alike (the attachment toggles the button)
    footSwitch.onStateChange = [this]
    {
        if (footSwitch.getToggleState() != lastOn)
        {
            lastOn = footSwitch.getToggleState();
            refresh();
            editor.blockStateChanged();
        }
    };
}

bool EffectSuiteEditor::Stompbox::updateLooper()
{
    if (! looper)
        return false;

    const int state = (int) editor.processor.getLooperState();
    const double seconds = editor.processor.getLooperSeconds();
    const float pos = editor.processor.getLooperPosition();
    const bool running = state == (int) fx::Looper::State::recording || state == (int) fx::Looper::State::playing
                      || state == (int) fx::Looper::State::overdubbing;
    const bool stateChanged = state != shownLooperState;

    if (stateChanged || std::abs (seconds - shownLooperSeconds) >= 0.05)
        repaint (statusArea.getUnion (lengthArea).getUnion (ledArea));
    if (stateChanged || (running && std::abs (pos - shownLooperPos) > 0.002f))
        repaint (barArea.expanded (0, 2));

    shownLooperState = state;
    shownLooperSeconds = seconds;
    shownLooperPos = pos;
    stopButton.setEnabled (state != (int) fx::Looper::State::empty && state != (int) fx::Looper::State::stopped);
    clearButton.setEnabled (state != (int) fx::Looper::State::empty);
    return stateChanged;
}

void EffectSuiteEditor::Stompbox::mouseDown (const juce::MouseEvent& e)
{
    if (looper)
        return;   // the looper stays last in the chain: it is not draggable

    // Only reached for the enclosure itself: knobs, combo, footswitch and the x button take their own clicks
    // Knob labels do not intercept the mouse, so exclude them here: only the enclosure and the name start a drag
    bool onLabel = false;
    for (auto& k : knobs)
        onLabel = onLabel || k->label.getBounds().contains (e.getPosition());

    dragArmed = ! onLabel && ! e.mods.isPopupMenu() && e.mods.isLeftButtonDown();

    if (! e.mods.isPopupMenu())
        return;

    juce::PopupMenu menu;
    menu.addItem (1, juce::CharPointer_UTF8 ("Quitar de la pedalera"));
    juce::Component::SafePointer<Stompbox> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe] (int result)
    {
        if (result == 1 && safe != nullptr)
            safe->editor.removeFromBoard (safe->id);
    });
}

juce::MouseCursor EffectSuiteEditor::Stompbox::getMouseCursor()
{
    if (looper)
        return juce::MouseCursor::NormalCursor;

    const auto p = getMouseXYRelative();
    for (auto& k : knobs)
        if (k->label.getBounds().contains (p))
            return juce::MouseCursor::NormalCursor;
    return juce::Component::getMouseCursor();
}

void EffectSuiteEditor::Stompbox::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragArmed)
        return;

    const auto pos = e.getEventRelativeTo (&editor).getPosition();

    if (! dragging)
    {
        if (e.getDistanceFromDragStart() < juce::roundToInt (6.0f * juce::jmax (1.0f, scale)))
            return;

        dragging = true;
        editor.beginPedalDrag (*this, pos);
    }

    editor.updatePedalDrag (*this, pos);
}

void EffectSuiteEditor::Stompbox::mouseUp (const juce::MouseEvent&)
{
    dragArmed = false;

    if (dragging)
    {
        dragging = false;
        editor.endPedalDrag (*this, true);
    }
}

void EffectSuiteEditor::Stompbox::refresh()
{
    const auto ink = contrastFor (colour);

    for (auto& k : knobs)
    {
        k->label.setColour (juce::Label::textColourId, ink);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, looper ? juce::Colour (0xffe5484d) : ink);
        k->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.35f));
    }

    repaint();
}

void EffectSuiteEditor::Stompbox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float corner = 10.0f * scale;
    const auto body = lastOn ? colour : bypassedColour (colour);

    g.setGradientFill (juce::ColourGradient (body.brighter (0.22f), 0.0f, r.getY(),
                                             body.darker (0.3f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (body.darker (0.75f));
    g.drawRoundedRectangle (r, corner, juce::jmax (1.5f, 2.0f * scale));
    g.setColour (juce::Colours::white.withAlpha (lastOn ? 0.3f : 0.12f));
    g.drawRoundedRectangle (r.reduced (2.5f * scale), corner - 2.0f * scale, 1.0f);

    // Name plate
    const auto ink = contrastFor (colour);
    g.setColour (ink);
    g.setFont (juce::FontOptions (juce::jmax (10.0f, 15.0f * scale), juce::Font::bold));
    g.drawFittedText (name, nameArea, juce::Justification::centred, 1, 0.7f);
    g.setColour (ink.withAlpha (0.35f));
    g.fillRect (nameArea.toFloat().withHeight (1.0f).reduced (4.0f * scale, 0.0f));

    if (! looper)
    {
        drawLight (g, ledArea.toFloat().getCentre(), 5.0f * scale, ledRed, lastOn);
        return;
    }

    using State = fx::Looper::State;
    const auto st = (State) shownLooperState;
    const bool hasLoop = st != State::empty;
    const bool running = st == State::recording || st == State::playing || st == State::overdubbing;
    const juce::Colour stateColour = st == State::recording ? ledRed
                                   : st == State::playing ? okColour
                                   : st == State::overdubbing ? accentColour : dimTextColour;

    // Red accent along the top edge
    g.setColour (ledRed.withAlpha (0.9f));
    g.fillRoundedRectangle (r.getX() + 8.0f * scale, r.getY() + 2.0f * scale, r.getWidth() - 16.0f * scale,
                            juce::jmax (2.0f, 3.0f * scale), 1.5f);

    const char* texts[] = { "VAC\xc3\x8dO", "GRABANDO", "REPRODUCIENDO", "SOBREGRABANDO", "PARADO" };
    g.setColour (stateColour);
    g.setFont (juce::FontOptions (smallFont (11.5f, scale), juce::Font::bold));
    g.drawFittedText (juce::CharPointer_UTF8 (texts[juce::jlimit (0, 4, shownLooperState)]), statusArea,
                      juce::Justification::centred, 1, 0.55f);

    // Progress bar
    const auto bar = barArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
    if (running && hasLoop)
    {
        g.setColour (stateColour);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * juce::jlimit (0.0f, 1.0f, shownLooperPos))),
                                bar.getHeight() * 0.5f);
    }

    g.setColour (textColour.withAlpha (hasLoop ? 0.9f : 0.4f));
    g.setFont (juce::FontOptions (smallFont (11.0f, scale)));
    g.drawFittedText (hasLoop ? juce::String (shownLooperSeconds, 1) + " s" : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")),
                      lengthArea, juce::Justification::centred, 1, 0.7f);

    drawLight (g, ledArea.toFloat().getCentre(), 5.0f * scale, stateColour, running);
}

void EffectSuiteEditor::Stompbox::paintOverChildren (juce::Graphics& g)
{
    // The pedal being dragged looks lifted: bright outline over a translucent body
    if (dragging)
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (accentColour);
        g.drawRoundedRectangle (r, 10.0f * scale, juce::jmax (2.0f, 3.0f * scale));
    }
}

void EffectSuiteEditor::Stompbox::resized()
{
    const float s = scale;
    auto r = getLocalBounds().reduced (juce::roundToInt (8 * s));

    const int rb = juce::jmax (14, juce::roundToInt (16 * s));
    removeButton.setBounds (getWidth() - rb - juce::roundToInt (4 * s), juce::roundToInt (4 * s), rb, rb);
    r.removeFromTop (juce::roundToInt (10 * s));

    const int switchSize = juce::jmin (juce::roundToInt (58 * s), r.getWidth());
    footSwitch.setBounds (r.removeFromBottom (switchSize).withSizeKeepingCentre (switchSize, switchSize));
    ledArea = r.removeFromBottom (juce::roundToInt (22 * s));
    nameArea = r.removeFromBottom (juce::roundToInt (24 * s));
    r.removeFromBottom (juce::roundToInt (4 * s));
    controlsArea = r;

    if (looper)
    {
        const int lh = juce::roundToInt (smallFont (11.5f, s) + 2.0f);
        statusArea = r.removeFromTop (lh);
        r.removeFromTop (juce::roundToInt (2 * s));
        barArea = r.removeFromTop (juce::jmax (3, juce::roundToInt (6 * s))).reduced (juce::roundToInt (4 * s), 0);
        r.removeFromTop (juce::roundToInt (1 * s));
        lengthArea = r.removeFromTop (lh);
        r.removeFromTop (juce::roundToInt (3 * s));

        // Level knob on the left, the two small buttons stacked on the right
        auto knobCell = r.removeFromLeft (r.getWidth() / 2);
        const int labelH = juce::roundToInt (smallFont (11.5f, s) + 3.0f);
        const bool showLabel = knobCell.getHeight() >= labelH + 22;
        knobs[0]->label.setVisible (showLabel);
        placeKnob (*knobs[0], knobCell, showLabel ? labelH : 0, smallFont (11.5f, s), juce::roundToInt (60 * s));

        const int bh = juce::jmin (juce::roundToInt (26 * s), (r.getHeight() - 2) / 2);
        auto col = r.withSizeKeepingCentre (r.getWidth(), bh * 2 + 2).reduced (juce::roundToInt (2 * s), 0);
        stopButton.setBounds (col.removeFromTop (bh));
        col.removeFromTop (2);
        clearButton.setBounds (col.removeFromTop (bh));
        return;
    }

    if (hasCombo)
    {
        combo.setBounds (r.removeFromTop (juce::roundToInt (24 * s)));
        r.removeFromTop (juce::roundToInt (4 * s));
    }

    const int n = (int) knobs.size();
    const int labelH = juce::roundToInt (smallFont (11.5f, s) + 3.0f);
    const float font = smallFont (11.5f, s);

    if (sliders)
    {
        const int w = r.getWidth() / n;
        int sliderIndex = 0;
        for (auto& k : knobs)
        {
            // Too narrow for every caption: show every other one
            k->label.setVisible (w >= 26 || (sliderIndex++ % 2) == 0);
            auto cell = r.removeFromLeft (w);
            k->label.setBounds (cell.removeFromBottom (labelH));
            k->label.setText (knobCaption (k->fullText, w), juce::dontSendNotification);
            k->label.setFont (juce::FontOptions (font));
            k->slider.setBounds (cell.reduced (juce::roundToInt (2 * s), juce::roundToInt (4 * s)));
        }
        return;
    }

    // One or two rows of knobs, whichever gives the larger knob
    int bestRows = 1, bestSize = 0;
    for (int rows = 1; rows <= 2 && rows <= n; ++rows)
    {
        const int perRow = (n + rows - 1) / rows;
        const int size = juce::jmin (r.getWidth() / perRow, r.getHeight() / rows - labelH);
        if (size > bestSize)
        {
            bestSize = size;
            bestRows = rows;
        }
    }

    const int maxKnob = juce::roundToInt (78 * s);
    const int perRow = (n + bestRows - 1) / bestRows;
    const int cellW = r.getWidth() / perRow;
    const int rowH = r.getHeight() / bestRows;
    int index = 0;

    for (auto& k : knobs)
        k->label.setText (knobCaption (k->fullText, cellW), juce::dontSendNotification);

    for (int row = 0; row < bestRows; ++row)
    {
        const int count = juce::jmin (perRow, n - index);
        auto rowArea = r.removeFromTop (rowH).withSizeKeepingCentre (cellW * count, rowH);
        for (int i = 0; i < count; ++i)
            placeKnob (*knobs[(size_t) index++], rowArea.removeFromLeft (cellW), labelH, font, maxKnob);
    }
}

//==============================================================================
EffectSuiteEditor::AmpUnit::AmpUnit (EffectSuiteEditor& e) : editor (e)
{
    for (auto k : { std::pair<const char*, const char*> { "Gain", "ampGain" }, { "Bass", "ampBass" },
                    { "Mid", "ampMid" }, { "Treble", "ampTreble" }, { "Presence", "ampPresence" },
                    { "Master", "ampMaster" } })
        knobs.push_back (editor.createKnob (*this, k.first, k.second, ValueStyle::unit));

    editor.fillCombo (model, "ampModel");
    addAndMakeVisible (model);
    modelAttachment = std::make_unique<ComboAttachment> (editor.processor.apvts, "ampModel", model);

    // Motor de válvulas: "Circuito" (nuevo) o "Clásico", para poder compararlos de oído
    editor.fillCombo (engine, "ampEngine");
    engine.setTooltip (juce::String::fromUTF8 ("Motor de válvulas: Circuito calcula cada válvula y la etapa de potencia con el altavoz; Clásico es el modelo anterior"));
    addAndMakeVisible (engine);
    engineAttachment = std::make_unique<ComboAttachment> (editor.processor.apvts, "ampEngine", engine);

    addAndMakeVisible (loadCapture);
    addAndMakeVisible (clearCapture);
    loadCapture.onClick = [this] { chooseCapture(); };
    clearCapture.onClick = [this] { editor.processor.clearNeuralModel(); updateCaptureState(); };
    captureName.setJustificationType (juce::Justification::centred);
    captureName.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (captureName);

    power.setTooltip ("Amplificador");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<ButtonAttachment> (editor.processor.apvts, "ampOn", power);

    lastOn = power.getToggleState();
    refresh();
    updateCaptureState();

    power.onStateChange = [this]
    {
        if (power.getToggleState() != lastOn)
        {
            lastOn = power.getToggleState();
            refresh();
            editor.blockStateChanged();
        }
    };
}

void EffectSuiteEditor::AmpUnit::refresh()
{
    for (auto& k : knobs)
    {
        k->label.setColour (juce::Label::textColourId, darkInk);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, shaded (juce::Colour (0xff8c2a1c), lastOn));
        k->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::black.withAlpha (0.3f));
    }

    repaint();
}

void EffectSuiteEditor::AmpUnit::chooseCapture()
{
    chooser = std::make_unique<juce::FileChooser> ("Seleccionar captura Neural Amp Modeler",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   "*.nam");
    juce::Component::SafePointer<AmpUnit> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
        {
            const auto error = safe->editor.processor.loadNeuralModel (f);
            if (error.isNotEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "No se pudo cargar la captura", error);
        }
        safe->updateCaptureState();
    });
}

bool EffectSuiteEditor::AmpUnit::captureStateIsStale() const
{
    return editor.processor.getNeuralModelName() != lastCaptureName
        || editor.processor.getNeuralModelNote() != lastCaptureNote;
}

void EffectSuiteEditor::AmpUnit::updateCaptureState()
{
    lastCaptureName = editor.processor.getNeuralModelName();
    lastCaptureNote = editor.processor.getNeuralModelNote();
    const bool loaded = lastCaptureName.isNotEmpty();

    captureName.setText (loaded ? lastCaptureName : juce::String ("Sin captura"), juce::dontSendNotification);
    captureName.setColour (juce::Label::textColourId,
                           lastCaptureNote.isNotEmpty() ? juce::Colour (0xffffb347)
                                                        : (loaded ? textColour : dimTextColour));
    captureName.setTooltip (lastCaptureNote.isNotEmpty() ? lastCaptureNote : lastCaptureName);
    clearCapture.setEnabled (loaded);

    model.setEnabled (! loaded);
    model.setAlpha (loaded ? 0.4f : 1.0f);
    engine.setEnabled (! loaded);
    engine.setAlpha (loaded ? 0.4f : 1.0f);

    // Bass, Mid, Treble and Presence do nothing with a capture; Gain and Master become input/output level
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const bool tone = i >= 1 && i <= 4;
        knobs[i]->slider.setEnabled (! (loaded && tone));
        knobs[i]->label.setEnabled (! (loaded && tone));
        knobs[i]->slider.setAlpha (loaded && tone ? 0.35f : 1.0f);
        knobs[i]->label.setAlpha (loaded && tone ? 0.35f : 1.0f);
    }
    knobs.front()->label.setText (loaded ? "Entrada" : "Gain", juce::dontSendNotification);
    knobs.back()->label.setText (loaded ? "Salida" : "Master", juce::dontSendNotification);
    repaint();
}

void EffectSuiteEditor::AmpUnit::paint (juce::Graphics& g)
{
    const float s = scale;
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float corner = 10.0f * s;

    // Cabinet shell
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26272b), 0.0f, r.getY(),
                                             juce::Colour (0xff131416), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, corner, juce::jmax (1.5f, 2.0f * s));

    // Grille with the badge
    const auto grille = grilleArea.toFloat();
    drawGrille (g, grille, shaded (juce::Colour (0xff4a3a2a), lastOn), 6.0f * s, 5.0f * s);

    auto badge = grille.withWidth (grille.getWidth() * 0.3f).reduced (12.0f * s, grille.getHeight() * 0.2f);
    g.setColour (shaded (juce::Colour (0xffeadfbd), lastOn));
    g.fillRoundedRectangle (badge, 3.0f * s);
    g.setColour (darkInk);
    g.setFont (juce::FontOptions (juce::jmax (10.0f, 16.0f * s), juce::Font::bold));
    g.drawFittedText ("AMPLIFICADOR", badge.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);

    g.setColour (lastOn && model.isEnabled() ? juce::Colours::white : dimTextColour);
    g.setFont (juce::FontOptions (smallFont (12.0f, s)));
    g.drawText ("Modelo", juce::Rectangle<int> (model.getX() - juce::roundToInt (70 * s), model.getY(),
                                                juce::roundToInt (62 * s), model.getHeight()),
                juce::Justification::centredRight);

    // Piping between grille and control plate
    g.setColour (shaded (juce::Colour (0xffc9a24a), lastOn));
    g.fillRect (grille.getX(), grille.getBottom() + 2.0f * s, grille.getWidth(), juce::jmax (1.5f, 2.0f * s));

    // Control plate: cream, a little darker when off
    const auto plate = plateArea.toFloat();
    g.setGradientFill (juce::ColourGradient (shaded (juce::Colour (0xffe6d8ae), lastOn),
                                             0.0f, plate.getY(),
                                             shaded (juce::Colour (0xffb9a672), lastOn),
                                             0.0f, plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 5.0f * s);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (plate, 5.0f * s, 1.0f);

    drawLight (g, lightArea.toFloat().getCentre(), 7.0f * s, juce::Colour (0xffff7a1a), lastOn);

    g.setColour (darkInk);
    g.setFont (juce::FontOptions (smallFont (10.5f, s), juce::Font::bold));
    g.drawText ("POWER", juce::Rectangle<int> (lightArea.getX(), plateArea.getBottom() - juce::roundToInt (20 * s),
                                               lightArea.getWidth(), juce::roundToInt (16 * s)),
                juce::Justification::centred);
}

void EffectSuiteEditor::AmpUnit::resized()
{
    const float s = scale;
    auto r = getLocalBounds().reduced (juce::roundToInt (9 * s));

    grilleArea = r.removeFromTop (juce::roundToInt ((float) r.getHeight() * 0.3f));
    r.removeFromTop (juce::roundToInt (7 * s));
    plateArea = r;

    // Right part of the grille: model combo on top, capture controls below
    const int ctlH = juce::jmin (juce::roundToInt (26 * s), (grilleArea.getHeight() - 6) / 2);
    const int gap = juce::roundToInt (6 * s);
    auto strip = grilleArea.withTrimmedRight (juce::roundToInt (12 * s))
                           .removeFromRight (juce::roundToInt (330 * s))
                           .withSizeKeepingCentre (juce::roundToInt (330 * s), ctlH * 2 + gap);
    model.setBounds (strip.removeFromTop (ctlH).removeFromRight (juce::roundToInt (250 * s)));

    // El selector de motor va a la izquierda del rótulo "Modelo", fuera de esa franja
    engine.setBounds (model.getX() - juce::roundToInt (70 * s) - gap - juce::roundToInt (118 * s), model.getY(),
                      juce::roundToInt (118 * s), ctlH);
    strip.removeFromTop (gap);
    loadCapture.setBounds (strip.removeFromLeft (juce::roundToInt (120 * s)));
    strip.removeFromLeft (gap);
    clearCapture.setBounds (strip.removeFromRight (juce::roundToInt (62 * s)));
    strip.removeFromRight (gap);
    captureName.setBounds (strip);
    captureName.setFont (juce::FontOptions (smallFont (12.0f, s)));

    auto p = plateArea.reduced (juce::roundToInt (10 * s), juce::roundToInt (5 * s));
    auto powerColumn = p.removeFromLeft (juce::roundToInt (70 * s));
    lightArea = powerColumn.removeFromTop (juce::roundToInt (30 * s));
    powerColumn.removeFromBottom (juce::roundToInt (20 * s));
    const int swH = juce::jmin (powerColumn.getHeight(), juce::roundToInt (50 * s));
    power.setBounds (powerColumn.withSizeKeepingCentre (juce::roundToInt ((float) swH * 0.66f), swH));

    p.removeFromLeft (juce::roundToInt (8 * s));
    const int labelH = juce::roundToInt (smallFont (13.0f, s) + 3.0f);
    const int cellW = p.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
        placeKnob (*k, p.removeFromLeft (cellW), labelH, smallFont (13.0f, s), juce::roundToInt (96 * s));
}

//==============================================================================
EffectSuiteEditor::CabUnit::CabUnit (EffectSuiteEditor& e) : editor (e)
{
    editor.fillCombo (cabModel, "cabModel");
    addAndMakeVisible (cabModel);
    cabModelAttachment = std::make_unique<ComboAttachment> (editor.processor.apvts, "cabModel", cabModel);

    cabModelLabel.setText ("Integrada", juce::dontSendNotification);
    cabModelLabel.setJustificationType (juce::Justification::centredLeft);
    cabModelLabel.setColour (juce::Label::textColourId, dimTextColour);
    cabModelLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (cabModelLabel);

    addAndMakeVisible (loadIr);
    addAndMakeVisible (clearIr);
    loadIr.onClick = [this] { chooseImpulseResponse(); };
    clearIr.onClick = [this] { editor.processor.clearImpulseResponse(); updateIrState(); };

    irName.setJustificationType (juce::Justification::centred);
    irName.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (irName);
    updateIrState();

    power.setTooltip ("Pantalla");
    addAndMakeVisible (power);
    powerAttachment = std::make_unique<ButtonAttachment> (editor.processor.apvts, "cabOn", power);
    lastOn = power.getToggleState();

    power.onStateChange = [this]
    {
        if (power.getToggleState() != lastOn)
        {
            lastOn = power.getToggleState();
            repaint();
            editor.blockStateChanged();
        }
    };
}

void EffectSuiteEditor::CabUnit::chooseImpulseResponse()
{
    chooser = std::make_unique<juce::FileChooser> ("Seleccionar respuesta a impulso",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   "*.wav;*.aif;*.aiff");
    juce::Component::SafePointer<CabUnit> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File() && ! safe->editor.processor.loadImpulseResponse (f))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo cargar la IR",
                                                    juce::String::fromUTF8 ("El archivo no es un audio WAV o AIFF válido."), {}, safe.getComponent());
        safe->updateIrState();
    });
}

void EffectSuiteEditor::CabUnit::updateIrState()
{
    lastIrName = editor.processor.getImpulseResponseName();
    const bool userIr = lastIrName.isNotEmpty();
    irName.setText (userIr ? lastIrName : juce::String ("Sin IR"), juce::dontSendNotification);
    irName.setColour (juce::Label::textColourId, userIr ? textColour : dimTextColour);
    clearIr.setEnabled (userIr);
    // The built-in cabinet is not in use while a user IR is loaded
    cabModel.setEnabled (! userIr);
    cabModelLabel.setEnabled (! userIr);
}

void EffectSuiteEditor::CabUnit::paint (juce::Graphics& g)
{
    const float s = scale;
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float corner = 10.0f * s;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26272b), 0.0f, r.getY(),
                                             juce::Colour (0xff131416), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, corner, juce::jmax (1.5f, 2.0f * s));

    // Speaker grille with the outline of the cone behind the cloth
    const auto grille = grilleArea.toFloat();
    drawGrille (g, grille, shaded (juce::Colour (0xff4a3a2a), lastOn), 6.0f * s, 5.0f * s);
    const float d = juce::jmin (grille.getWidth(), grille.getHeight()) * 0.82f;
    const auto cone = juce::Rectangle<float> (d, d).withCentre (grille.getCentre());
    g.setColour (juce::Colours::black.withAlpha (lastOn ? 0.4f : 0.3f));
    g.fillEllipse (cone);
    g.setColour (shaded (juce::Colour (0xffc9a24a), lastOn).withAlpha (0.55f));
    g.drawEllipse (cone, juce::jmax (1.5f, 2.0f * s));
    g.drawEllipse (cone.reduced (d * 0.12f), 1.0f);
    g.fillEllipse (cone.reduced (d * 0.36f));
    g.setColour (shaded (juce::Colour (0xffc9a24a), lastOn));
    g.drawRoundedRectangle (grille, 5.0f * s, juce::jmax (1.5f, 2.0f * s));

    // Control panel
    const auto panel = panelArea.toFloat();
    g.setColour (juce::Colour (lastOn ? 0xff1d2026 : 0xff17181b));
    g.fillRoundedRectangle (panel, 5.0f * s);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (panel, 5.0f * s, 1.0f);

    g.setColour (shaded (accentColour, lastOn));
    g.setFont (juce::FontOptions (juce::jmax (10.0f, 16.0f * s), juce::Font::bold));
    g.drawText ("PANTALLA", titleArea, juce::Justification::centredLeft);

    drawLight (g, lightArea.toFloat().getCentre(), 6.0f * s, juce::Colour (0xffff7a1a), lastOn);
}

void EffectSuiteEditor::CabUnit::resized()
{
    const float s = scale;
    auto r = getLocalBounds().reduced (juce::roundToInt (9 * s));

    grilleArea = r.removeFromLeft (juce::jmin (r.getHeight(), juce::roundToInt ((float) r.getWidth() * 0.36f)));
    r.removeFromLeft (juce::roundToInt (7 * s));
    panelArea = r;

    auto p = r.reduced (juce::roundToInt (10 * s), juce::roundToInt (6 * s));
    const int rowH = p.getHeight() / 4;
    const int ctlH = juce::jmin (rowH - juce::roundToInt (4 * s), juce::roundToInt (30 * s));
    const int gap = juce::roundToInt (6 * s);

    auto row = p.removeFromTop (rowH);
    const int swH = juce::jmin (rowH - 2, juce::roundToInt (40 * s));
    power.setBounds (row.removeFromRight (juce::roundToInt ((float) swH * 0.66f)).withSizeKeepingCentre (
                         juce::roundToInt ((float) swH * 0.66f), swH));
    lightArea = row.removeFromRight (juce::roundToInt (34 * s));
    titleArea = row;

    row = p.removeFromTop (rowH).withSizeKeepingCentre (p.getWidth(), ctlH);
    cabModelLabel.setBounds (row.removeFromLeft (juce::roundToInt (72 * s)));
    cabModelLabel.setFont (juce::FontOptions (smallFont (13.0f, s)));
    cabModel.setBounds (row);

    row = p.removeFromTop (rowH).withSizeKeepingCentre (p.getWidth(), ctlH);
    loadIr.setBounds (row.removeFromLeft ((row.getWidth() - gap) * 3 / 5));
    row.removeFromLeft (gap);
    clearIr.setBounds (row);

    irName.setBounds (p.withSizeKeepingCentre (p.getWidth(), ctlH));
    irName.setFont (juce::FontOptions (smallFont (13.0f, s)));
}

//==============================================================================
void EffectSuiteEditor::FlowStrip::addChip (const juce::String& text, juce::Colour colour, juce::Button& target,
                                            std::function<bool()> lit)
{
    chips.push_back ({ text, colour, &target, std::move (lit), {} });
}

void EffectSuiteEditor::FlowStrip::resized()
{
    if (chips.empty())
        return;

    auto r = getLocalBounds().toFloat();
    const float endW = juce::jmax (26.0f, 34.0f * scale);       // "IN" / "OUT" captions
    r = r.reduced (endW, 0.0f);

    const float cell = juce::jmin (160.0f * scale, r.getWidth() / (float) chips.size());
    const float arrow = juce::jmax (8.0f, 16.0f * scale);
    const float chipH = juce::jmin (r.getHeight() - 4.0f, 26.0f * scale);

    for (size_t i = 0; i < chips.size(); ++i)
        chips[i].bounds = juce::Rectangle<float> (r.getX() + cell * (float) i + arrow * 0.5f,
                                                  r.getCentreY() - chipH * 0.5f, cell - arrow, chipH);
}

void EffectSuiteEditor::FlowStrip::paint (juce::Graphics& g)
{
    if (chips.empty())
        return;

    const auto r = getLocalBounds().toFloat();
    const float cy = r.getCentreY();
    const juce::Colour wire (0xff5a616b);

    g.setColour (dimTextColour);
    g.setFont (juce::FontOptions (smallFont (11.0f, scale), juce::Font::bold));
    g.drawText ("IN", r.withWidth (chips.front().bounds.getX() - 8.0f * scale), juce::Justification::centredLeft);
    // OUT sits right after the last chip, so a short chain still reads as one line
    const float wireEnd = chips.back().bounds.getRight() + juce::jmax (8.0f, 12.0f * scale);
    g.drawText ("OUT", r.withLeft (wireEnd + 4.0f * scale), juce::Justification::centredLeft);

    // Wire with an arrow head in front of every chip and before OUT
    g.setColour (wire);
    const float wireStart = r.getX() + juce::jmax (16.0f, 22.0f * scale);
    g.fillRect (wireStart, cy - 0.75f, wireEnd - wireStart, 1.5f);

    const float a = juce::jmax (3.0f, 4.0f * scale);
    auto arrowAt = [&] (float tipX)
    {
        juce::Path head;
        head.addTriangle (tipX, cy, tipX - a * 1.5f, cy - a, tipX - a * 1.5f, cy + a);
        g.fillPath (head);
    };
    for (auto& chip : chips)
        arrowAt (chip.bounds.getX() - 1.0f);
    arrowAt (wireEnd);

    g.setFont (juce::FontOptions (smallFont (11.5f, scale), juce::Font::bold));

    for (auto& chip : chips)
    {
        const bool on = chip.lit ? chip.lit() : chip.target->getToggleState();
        const float corner = chip.bounds.getHeight() * 0.3f;

        const auto fill = on ? chip.colour : mutedChipColour (chip.colour);
        g.setColour (fill);
        g.fillRoundedRectangle (chip.bounds, corner);
        g.setColour (on ? chip.colour.brighter (0.6f) : chip.colour.withMultipliedBrightness (0.7f));
        g.drawRoundedRectangle (chip.bounds.reduced (0.5f), corner, on ? 1.5f : 1.0f);

        g.setColour (on ? contrastFor (chip.colour) : contrastFor (fill).withAlpha (0.85f));
        g.drawFittedText (chip.text, chip.bounds.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.7f);
    }
}

int EffectSuiteEditor::FlowStrip::chipAt (juce::Point<float> p) const
{
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].bounds.expanded (2.0f).contains (p))
            return (int) i;
    return -1;
}

void EffectSuiteEditor::FlowStrip::mouseDown (const juce::MouseEvent& e)
{
    // Clicking the real switch keeps the parameter attachment, host gesture and repaint logic in one place
    if (const int i = chipAt (e.position); i >= 0)
        chips[(size_t) i].target->triggerClick();
}

void EffectSuiteEditor::FlowStrip::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (chipAt (e.position) >= 0 ? juce::MouseCursor::PointingHandCursor
                                             : juce::MouseCursor::NormalCursor);
}

//==============================================================================
EffectSuiteEditor::EffectSuiteEditor (EffectSuiteProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);

    const auto dB = ValueStyle::decibels;
    const auto ms = ValueStyle::milliseconds;

    // Signal order. Names are UTF-8 and already upper case.
    const PedalDef pre[] = {
        { "PUERTA",    "PUERTA", 0xff4f6f9c, "gateOn",   nullptr,     { { "Threshold", "gateThreshold", dB } } },
        { "WAH",       "WAH",    0xffc3c9d1, "wahOn",    "wahMode",   { { "Position", "wahPosition" }, { "Resonance", "wahQ" } } },
        { "COMPRESOR", "COMP",   0xffc8372d, "compOn",   nullptr,     { { "Sustain", "compSustain" }, { "Attack", "compAttack" }, { "Level", "compLevel" } } },
        { "OCTAVADOR", "OCT",    0xffa5682f, "octOn",    nullptr,     { { "Sub", "octSub" }, { "Dry", "octDry" } } },
        { "PHASER",    "PHASER", 0xffd4488f, "phaserOn", nullptr,     { { "Rate", "phaserRate" }, { "Depth", "phaserDepth" }, { "Feedback", "phaserFeedback" } } },
        { "OVERDRIVE", "OD",     0xff3fa34d, "odOn",     "odModel",   { { "Drive", "odDrive" }, { "Tone", "odTone" }, { "Level", "odLevel" } } },
        { "DISTORSI\xc3\x93N", "DIST", 0xffe9812a, "distOn", "distModel", { { "Gain", "distGain" }, { "Tone", "distTone" }, { "Level", "distLevel" } } },
        { "FUZZ",      "FUZZ",   0xff7d4bc2, "fuzzOn",   "fuzzModel", { { "Fuzz", "fuzzAmount" }, { "Tone", "fuzzTone" }, { "Level", "fuzzLevel" } } },
    };
    const PedalDef post[] = {
        { "ECUALIZADOR", "EQ",    0xffddd2ae, "eqOn",      nullptr, { { "100", "eq100", dB }, { "200", "eq200", dB }, { "400", "eq400", dB }, { "800", "eq800", dB }, { "1.6k", "eq1k6", dB }, { "3.2k", "eq3k2", dB }, { "6.4k", "eq6k4", dB } }, true },
        { "FLANGER",     "FLANG", 0xffe2c22e, "flangerOn", nullptr, { { "Rate", "flangerRate" }, { "Depth", "flangerDepth" }, { "Feedback", "flangerFeedback" } } },
        { "CHORUS",      "CHORUS", 0xff2f6fd6, "chorusOn", nullptr, { { "Rate", "chorusRate" }, { "Depth", "chorusDepth" }, { "Mix", "chorusMix" } } },
        { "TREMOLO",     "TREM",  0xff7d9440, "tremOn",    nullptr, { { "Rate", "tremRate" }, { "Depth", "tremDepth" } } },
        { "DELAY",       "DELAY", 0xff1f9d93, "delayOn",   "delayModel", { { "Time", "delayTime", ms }, { "Feedback", "delayFeedback" }, { "Mix", "delayMix" } } },
        { "REVERB",      "REVERB", 0xff7fc4ea, "reverbOn", "reverbModel", { { "Size", "reverbSize" }, { "Mix", "reverbMix" } } },
        { "LOOPER",      "LOOP",  0xff4a4d55, "looperOn",  nullptr, { { "Level", "looperLevel" } }, false, true },
    };

    flow = std::make_unique<FlowStrip>();

    for (auto& def : pre)
        addAndMakeVisible (*prePedals.emplace_back (std::make_unique<Stompbox> (*this, def)));

    amp = std::make_unique<AmpUnit> (*this);
    cab = std::make_unique<CabUnit> (*this);
    addAndMakeVisible (*amp);
    addAndMakeVisible (*cab);

    for (auto& def : post)
        addAndMakeVisible (*postPedals.emplace_back (std::make_unique<Stompbox> (*this, def)));

    addPre = std::make_unique<AddTile>();
    addPost = std::make_unique<AddTile>();
    addPre->onClick  = [this] { showAddMenu (true, *addPre); };
    addPost->onClick = [this] { showAddMenu (false, *addPost); };
    addAndMakeVisible (*addPre);
    addAndMakeVisible (*addPost);

    addAndMakeVisible (*flow);

    // Top bar
    inputGain = createKnob (*this, "Entrada", "inputGain", dB);
    outputLevel = createKnob (*this, "Salida", "outputLevel", dB);

    tunerMute.setButtonText ("Silenciar");
    addAndMakeVisible (tunerMute);
    tunerMuteA = std::make_unique<ButtonAttachment> (processor.apvts, "tunerMute", tunerMute);

    fillCombo (inputMode, "inputMode");
    fillCombo (quality, "quality");
    addAndMakeVisible (inputMode);
    addAndMakeVisible (quality);
    inputModeA = std::make_unique<ComboAttachment> (processor.apvts, "inputMode", inputMode);
    qualityA = std::make_unique<ComboAttachment> (processor.apvts, "quality", quality);

    for (auto item : { std::pair<juce::Label*, const char*> { &inputModeLabel, "Canal" }, { &qualityLabel, "Calidad" } })
    {
        item.first->setText (item.second, juce::dontSendNotification);
        item.first->setJustificationType (juce::Justification::centredLeft);
        item.first->setColour (juce::Label::textColourId, dimTextColour);
        addAndMakeVisible (*item.first);
    }

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // Preset bar
    cpuLabel.setJustificationType (juce::Justification::centredRight);
    cpuLabel.setColour (juce::Label::textColourId, dimTextColour);
    cpuLabel.setTooltip (juce::String::fromUTF8 ("Carga del procesado de audio. Si se acerca al 100 %, baja la calidad, quita pedales o sube el búfer."));
    addAndMakeVisible (cpuLabel);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    presetLabel.setColour (juce::Label::textColourId, dimTextColour);
    addAndMakeVisible (presetLabel);

    presetBox.setTextWhenNothingSelected ("Sin preset");
    presetBox.setTooltip ("Preset");
    presetBox.onChange = [this] { presetChosen(); };
    addAndMakeVisible (presetBox);

    saveButton.setTooltip ("Guardar el sonido actual como preset");
    saveButton.onClick = [this] { askPresetName(); };
    addAndMakeVisible (saveButton);

    deleteButton.setTooltip ("Borrar el preset de usuario seleccionado");
    deleteButton.onClick = [this] { askDeletePreset(); };
    deleteButton.setEnabled (false);
    addAndMakeVisible (deleteButton);

    // Recording and metronome
    recordButton.text = "Grabar";
    recordButton.activeText = "Parar";
    recordButton.setTooltip ("Grabar lo que suena (pulsa otra vez para parar)");
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible (recordButton);

    recordTime.setText ("0:00.0", juce::dontSendNotification);
    recordTime.setJustificationType (juce::Justification::centredLeft);
    recordTime.setColour (juce::Label::textColourId, dimTextColour);
    recordTime.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (recordTime);

    folderButton.text = "Carpeta";
    folderButton.setTooltip ("Abrir la carpeta de grabaciones");
    folderButton.onClick = [this]
    {
        auto folder = processor.getRecordingFolder();
        folder.createDirectory();
        folder.revealToUser();
    };
    addAndMakeVisible (folderButton);

    listenButton.text = "Escuchar";
    listenButton.setTooltip (juce::String::fromUTF8 ("Escuchar la última toma con el reproductor del sistema"));
    listenButton.onClick = [this] { processor.getLastTake().startAsProcess(); };
    addAndMakeVisible (listenButton);

    metroToggle.setButtonText (juce::String::fromUTF8 ("Metrónomo"));
    metroToggle.setTooltip (juce::String::fromUTF8 ("Metrónomo"));
    addAndMakeVisible (metroToggle);
    metroToggleA = std::make_unique<ButtonAttachment> (processor.apvts, "metroOn", metroToggle);

    metroBpm.setSliderStyle (juce::Slider::LinearBar);
    metroBpm.setTextBoxIsEditable (false);
    metroBpm.setColour (juce::Slider::trackColourId, accentColour.withAlpha (0.35f));
    metroBpm.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff22252b));
    metroBpm.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a3f49));
    metroBpm.setTooltip (juce::String::fromUTF8 ("Tempo del metrónomo (arrastra o usa la rueda)"));
    addAndMakeVisible (metroBpm);
    metroBpmA = std::make_unique<SliderAttachment> (processor.apvts, "metroBpm", metroBpm);
    metroBpm.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " bpm"; };
    metroBpm.updateText();

    metroLevel.setSliderStyle (juce::Slider::LinearHorizontal);
    metroLevel.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    metroLevel.setColour (juce::Slider::trackColourId, accentColour);
    metroLevel.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff343942));
    metroLevel.setColour (juce::Slider::thumbColourId, textColour);
    metroLevel.setTooltip (juce::String::fromUTF8 ("Volumen del metrónomo"));
    addAndMakeVisible (metroLevel);
    metroLevelA = std::make_unique<SliderAttachment> (processor.apvts, "metroLevel", metroLevel);

    refreshBoard();
    refreshPresetList();
    updateRecordingUi (true);

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    const double aspect = (double) baseWidth / (double) baseHeight;
    getConstrainer()->setFixedAspectRatio (aspect);

    // Initial size: the remembered width (or the default), never larger than what fits the screen
    // with room left for the title bar and the standalone "audio input muted" banner
    int width = baseWidth;
    if (const int saved = processor.getEditorWidth(); saved > 0)
        width = saved;

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds;
        width = juce::jmin (width, juce::roundToInt (area.getWidth()) - 20,
                            (int) ((area.getHeight() - 90.0f) * (float) aspect));
    }

    width = juce::jlimit (minWidth, maxWidth, width);
    setSize (width, juce::jmax (minHeight, juce::roundToInt ((double) width / aspect)));
    sizeRestored = true;

    startTimerHz (30);
}

EffectSuiteEditor::~EffectSuiteEditor()
{
    stopTimer();
    animator.cancelAllAnimations (false);
    setLookAndFeel (nullptr);
}

std::unique_ptr<EffectSuiteEditor::Knob> EffectSuiteEditor::createKnob (juce::Component& parent, const juce::String& text,
                                                                        const juce::String& paramId, ValueStyle style,
                                                                        bool vertical)
{
    auto k = std::make_unique<Knob>();

    k->slider.setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    // No value boxes, so knobs can be as large as possible: the value pops up on hover and while dragging
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setPopupDisplayEnabled (true, true, this, 1500);
    parent.addAndMakeVisible (k->slider);

    k->fullText = text;
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, dimTextColour);
    k->label.setMinimumHorizontalScale (0.7f);
    k->label.setBorderSize ({});
    k->label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (k->label);

    k->attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k->slider);

    // The attachment installs the parameter's own text conversion; replace it with the display format
    k->slider.textFromValueFunction = [style] (double v) -> juce::String
    {
        switch (style)
        {
            case ValueStyle::decibels:     return juce::String (v, 1) + " dB";
            case ValueStyle::milliseconds: return juce::String (juce::roundToInt (v)) + " ms";
            case ValueStyle::unit:         break;
        }
        return juce::String (v, 2);
    };
    k->slider.updateText();

    // Double-click resets to the parameter default
    if (auto* param = processor.apvts.getParameter (paramId))
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    return k;
}

void EffectSuiteEditor::fillCombo (juce::ComboBox& c, const juce::String& paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.addItemList (choice->choices, 1);
}

void EffectSuiteEditor::placeKnob (Knob& k, juce::Rectangle<int> cell, int labelHeight, float fontSize, int maxKnobSize)
{
    // Label and knob stay together as one centred group
    const int size = juce::jmax (8, juce::jmin (cell.getWidth(), cell.getHeight() - labelHeight, maxKnobSize));
    auto group = cell.withSizeKeepingCentre (cell.getWidth(), size + labelHeight);
    k.label.setBounds (group.removeFromTop (labelHeight));
    k.label.setFont (juce::FontOptions (fontSize));
    k.slider.setBounds (group.withSizeKeepingCentre (size, size));
}

void EffectSuiteEditor::refreshBoard()
{
    lastBoardRevision = processor.getBoardRevision();

    // A refresh ends any drag in progress (the layout is rebuilt below)
    if (draggedPedal != nullptr)
        endPedalDrag (*draggedPedal, false);

    bool preMissing = false, postMissing = false;
    for (auto* row : { &prePedals, &postPedals })
        for (auto& p : *row)
        {
            const bool on = processor.isOnBoard (p->getId());
            p->setVisible (on);
            (row == &prePedals ? preMissing : postMissing) |= ! on;
        }
    addPre->setVisible (preMissing);
    addPost->setVisible (postMissing);

    // Chips follow the signal order of the board
    flow->clearChips();
    auto addChips = [this] (bool afterAmp)
    {
        for (auto& id : boardRow (afterAmp))
            if (auto* p = findPedal (id))
            {
                if (p->isLooper())
                    flow->addChip (p->getChipText(), p->getColour(), p->getSwitch(), [this]
                    {
                        const auto st = processor.getLooperState();
                        return st == fx::Looper::State::recording || st == fx::Looper::State::playing
                            || st == fx::Looper::State::overdubbing;
                    });
                else
                    flow->addChip (p->getChipText(), p->getColour(), p->getSwitch());
            }
    };
    addChips (false);
    flow->addChip ("AMP", accentColour, amp->getSwitch());
    flow->addChip ("CAB", juce::Colour (0xffd9b06a), cab->getSwitch());
    addChips (true);

    if (getWidth() > 0)
    {
        resized();
        flow->resized();
    }
    repaint();
}

juce::StringArray EffectSuiteEditor::boardRow (bool post) const
{
    auto row = processor.getBoardRow (post);
    if (const int i = row.indexOf ("looper"); i >= 0 && i != row.size() - 1)
    {
        row.remove (i);
        row.add ("looper");
    }
    return row;
}

EffectSuiteEditor::Stompbox* EffectSuiteEditor::findPedal (const juce::String& id) const
{
    for (auto* row : { &prePedals, &postPedals })
        for (auto& p : *row)
            if (p->getId() == id)
                return p.get();
    return nullptr;
}

bool EffectSuiteEditor::isPostPedal (const Stompbox& pedal) const
{
    for (auto& p : postPedals)
        if (p.get() == &pedal)
            return true;
    return false;
}

void EffectSuiteEditor::layoutPedalRow (bool post, bool animate)
{
    auto& pedals = post ? postPedals : prePedals;
    auto& tile = post ? *addPost : *addPre;
    const auto board = post ? postBoard : preBoard;
    const float s = uiScale();
    auto px = [s] (float v) { return juce::roundToInt (v * s); };

    auto r = board.reduced (px (6), px (5)).withTrimmedTop (px (18) - px (3));
    const int gap = px (8);
    const float tileFrac = 0.65f;
    const float maxW = (float) (r.getWidth() - gap * 5) / 6.0f;   // width of a pedal in a row of six

    // Signal order from the processor, not the fixed order of the pedal list
    std::vector<Stompbox*> shown, others;
    int hole = -1;
    for (auto& id : boardRow (post))
        if (auto* p = findPedal (id))
        {
            if (p == draggedPedal)
                hole = dragTargetIndex >= 0 ? dragTargetIndex : (int) shown.size();
            else
                others.push_back (p);
            shown.push_back (p);
        }

    const bool hasTile = shown.size() < pedals.size();
    const int items = (int) shown.size() + (hasTile ? 1 : 0);
    const float units = (float) shown.size() + (hasTile ? tileFrac : 0.0f);
    const float w = juce::jmin (maxW, (float) (r.getWidth() - gap * juce::jmax (0, items - 1)) / juce::jmax (0.1f, units));

    rowX0[post ? 1 : 0] = (float) r.getX();
    rowStep[post ? 1 : 0] = w + (float) gap;

    float x = (float) r.getX();
    size_t next = 0;
    for (int slot = 0; slot < (int) shown.size(); ++slot, x += w + (float) gap)
    {
        if (slot == hole)
            continue;   // the dragged pedal's gap

        if (next >= others.size())
            break;

        auto* p = others[next++];
        p->scale = s;
        const juce::Rectangle<int> b (juce::roundToInt (x), r.getY(), juce::roundToInt (w), r.getHeight());

        if (animate)
        {
            if (p->getBounds() != b)
                animator.animateComponent (p, b, 1.0f, 140, false, 1.0, 1.0);
        }
        else
        {
            animator.cancelAnimation (p, false);
            p->setBounds (b);
        }
    }

    if (draggedPedal != nullptr && hole >= 0)
    {
        draggedPedal->scale = s;
        draggedPedal->setBounds (draggedPedal->getX(), r.getY(), juce::roundToInt (w), r.getHeight());
    }

    tile.scale = s;
    tile.setVisible (hasTile);
    tile.setBounds (juce::roundToInt ((float) r.getX() + (w + (float) gap) * (float) shown.size()),
                    r.getY(), juce::roundToInt (w * tileFrac), r.getHeight());
}

void EffectSuiteEditor::beginPedalDrag (Stompbox& pedal, juce::Point<int> editorPos)
{
    draggedPedal = &pedal;
    dragTargetIndex = -1;
    dragGrabX = editorPos.x - pedal.getX();
    animator.cancelAnimation (&pedal, false);
    pedal.setAlpha (0.75f);
    pedal.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    pedal.toFront (false);
    pedal.repaint();
}

void EffectSuiteEditor::updatePedalDrag (Stompbox& pedal, juce::Point<int> editorPos)
{
    if (draggedPedal != &pedal)
        return;

    const bool post = isPostPedal (pedal);
    const auto board = post ? postBoard : preBoard;
    const int oldTarget = dragTargetIndex;
    dragTargetIndex = -1;

    // The pedal follows the pointer sideways, inside its own row
    pedal.setTopLeftPosition (juce::jlimit (board.getX(), juce::jmax (board.getX(), board.getRight() - pedal.getWidth()),
                                            editorPos.x - dragGrabX), pedal.getY());

    if (board.contains (editorPos))
    {
        int count = boardRow (post).size();
        if (boardRow (post).contains ("looper"))
            --count;   // nothing is dropped after the looper
        const float step = rowStep[post ? 1 : 0];

        if (count > 0 && step > 0.0f)
            dragTargetIndex = juce::jlimit (0, count - 1,
                                            (int) std::floor (((float) editorPos.x - rowX0[post ? 1 : 0]) / step));
    }

    // The other pedals slide to open the gap at the drop position
    if (dragTargetIndex != oldTarget)
        layoutPedalRow (post, true);
}

void EffectSuiteEditor::endPedalDrag (Stompbox& pedal, bool commit)
{
    if (draggedPedal != &pedal)
        return;

    const int target = dragTargetIndex;
    const auto from = pedal.getBounds();
    draggedPedal = nullptr;
    dragTargetIndex = -1;
    pedal.setAlpha (1.0f);
    pedal.repaint();

    if (commit && target >= 0)
    {
        const bool post = isPostPedal (pedal);
        if (processor.getBoardRow (post).indexOf (pedal.getId()) != target)
            processor.movePedal (pedal.getId(), target);
    }

    // Always re-lay the row out: puts a released pedal back in its slot and applies a move
    refreshBoard();

    // ...and the released pedal glides into its slot
    if (pedal.isVisible() && pedal.getBounds() != from)
    {
        const auto to = pedal.getBounds();
        pedal.setBounds (from);
        animator.animateComponent (&pedal, to, 1.0f, 140, false, 1.0, 1.0);
    }
}

void EffectSuiteEditor::refreshPresetList()
{
    if (presetBox.isPopupActive())
        return;   // retried on the next tick (the name or revision check stays stale)

    factoryNames = EffectSuiteProcessor::getFactoryPresetNames();
    userNames = processor.getUserPresetNames();

    constexpr int factoryBase = 1000, userBase = 2000;
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading (juce::CharPointer_UTF8 ("De f\xc3\xa1""brica"));
    for (int i = 0; i < factoryNames.size(); ++i)
        presetBox.addItem (factoryNames[i], factoryBase + i);

    presetBox.addSectionHeading ("Mis presets");
    for (int i = 0; i < userNames.size(); ++i)
        presetBox.addItem (userNames[i], userBase + i);
    if (userNames.isEmpty())
    {
        presetBox.addItem ("(ninguno)", 3000);
        presetBox.setItemEnabled (3000, false);
    }

    syncPresetSelection();
}

void EffectSuiteEditor::syncPresetSelection()
{
    constexpr int factoryBase = 1000, userBase = 2000;
    const auto name = processor.getCurrentPresetName();
    shownPresetName = name;

    const int f = name.isEmpty() ? -1 : factoryNames.indexOf (name);
    const int u = name.isEmpty() ? -1 : userNames.indexOf (name);

    // Factory and user presets may share a name: keep the section last chosen here when both exist
    if (f >= 0 && u >= 0)      { /* presetIsUser decides */ }
    else if (u >= 0)           presetIsUser = true;
    else if (f >= 0)           presetIsUser = false;

    int id = 0;
    if (f >= 0 || u >= 0)
        id = (presetIsUser && u >= 0) ? userBase + u : (f >= 0 ? factoryBase + f : userBase + u);

    presetBox.setSelectedId (id, juce::dontSendNotification);
    deleteButton.setEnabled (id >= userBase && id < 3000);
}

void EffectSuiteEditor::presetChosen()
{
    constexpr int factoryBase = 1000, userBase = 2000;
    const int id = presetBox.getSelectedId();
    bool ok = false;

    if (id >= userBase && id < 3000 && id - userBase < userNames.size())
    {
        presetIsUser = true;
        ok = processor.loadUserPreset (userNames[id - userBase]);
    }
    else if (id >= factoryBase && id < userBase && id - factoryBase < factoryNames.size())
    {
        presetIsUser = false;
        ok = processor.loadFactoryPreset (factoryNames[id - factoryBase]);
    }
    else
        return;

    if (! ok)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset",
                                                "No se pudo cargar el preset");

    refreshBoard();
    cab->updateIrState();
    amp->updateCaptureState();
    syncPresetSelection();
    storeEditorWidth();   // loading a preset replaces the state tree
}

void EffectSuiteEditor::askPresetName()
{
    const auto current = processor.getCurrentPresetName();
    const bool prefill = presetIsUser && current.isNotEmpty() && userNames.contains (current);

    auto* window = new juce::AlertWindow ("Guardar preset", "Nombre del preset:",
                                          juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", prefill ? current : juce::String(), {});
    window->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<EffectSuiteEditor> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> safeWindow (window);
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([safe, safeWindow] (int result)
    {
        if (result != 1 || safe == nullptr || safeWindow == nullptr)
            return;

        const auto name = safeWindow->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            safe->saveCurrentAs (name);
    }), true);
}

void EffectSuiteEditor::saveCurrentAs (const juce::String& name)
{
    if (processor.saveUserPreset (name))
    {
        presetIsUser = true;
        refreshPresetList();
        syncPresetSelection();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Guardar preset",
                                                "No se pudo guardar el preset", {}, this);
    }
}

void EffectSuiteEditor::askDeletePreset()
{
    const int id = presetBox.getSelectedId();
    if (id < 2000 || id >= 3000 || id - 2000 >= userNames.size())
        return;

    const auto name = userNames[id - 2000];
    juce::Component::SafePointer<EffectSuiteEditor> safe (this);

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Borrar preset")
                                      .withMessage (juce::String (juce::CharPointer_UTF8 ("\xc2\xbf""Borrar el preset \"")) + name + "\"?")
                                      .withButton ("Borrar", 1)
                                      .withButton ("Cancelar", 0)
                                      .withAssociatedComponent (this),
                                  [safe, name] (int result)
    {
        if (result != 1 || safe == nullptr)
            return;

        if (! safe->processor.deleteUserPreset (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Borrar preset",
                                                    "No se pudo borrar el preset", {}, safe.getComponent());
        safe->presetIsUser = false;
        safe->refreshPresetList();
    });
}

void EffectSuiteEditor::removeFromBoard (const juce::String& id)
{
    processor.setOnBoard (id, false);
    refreshBoard();
}

void EffectSuiteEditor::showAddMenu (bool preRow, juce::Component& target)
{
    juce::PopupMenu menu;
    juce::StringArray ids;

    for (auto& p : preRow ? prePedals : postPedals)
    {
        if (processor.isOnBoard (p->getId()))
            continue;

        ids.add (p->getId());
        auto label = p->getName().toLowerCase();
        label = label.substring (0, 1).toUpperCase() + label.substring (1);

        juce::Path swatch;
        swatch.addRoundedRectangle (0.0f, 0.0f, 16.0f, 16.0f, 4.0f);
        auto icon = std::make_unique<juce::DrawablePath>();
        icon->setPath (swatch);
        icon->setFill (p->getColour());

        juce::PopupMenu::Item item (label);
        item.setID (ids.size());
        item.setImage (std::move (icon));
        menu.addItem (std::move (item));
    }

    if (ids.isEmpty())
        return;

    juce::Component::SafePointer<EffectSuiteEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                        [safe, ids] (int result)
    {
        if (result < 1 || safe == nullptr)
            return;

        const auto& id = ids[result - 1];
        safe->processor.setOnBoard (id, true);
        if (auto* param = safe->processor.apvts.getParameter (id + "On"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (1.0f);
            param->endChangeGesture();
        }
        safe->refreshBoard();
    });
}

void EffectSuiteEditor::blockStateChanged()
{
    if (flow != nullptr)
        flow->repaint();
}

void EffectSuiteEditor::updateTuner()
{
    const float hz = processor.detectPitch();
    if (hz > 20.0f)
    {
        tunerMissed = 0;
        tunerHz = hz;
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const int n = (int) std::lround (midi);
        tunerCents = juce::jlimit (-50.0f, 50.0f, (midi - (float) n) * 100.0f);
        tunerNote = juce::MidiMessage::getMidiNoteName (n, true, true, 4);
    }
    else if (++tunerMissed > 8)
    {
        tunerMissed = 100;
        tunerHz = 0.0f;
        tunerNote = "--";
        tunerCents = 0.0f;
    }
}

void EffectSuiteEditor::timerCallback()
{
    inMeter.pushLevel (processor.inputPeak.exchange (0.0f));
    outMeter.pushLevel (processor.outputPeak.exchange (0.0f));

    if ((++timerTick % 2) == 0)
    {
        updateTuner();
        repaint (tunerArea);
    }

    updateRecordingUi (false);

    if (auto* looperPedal = findPedal ("looper"); looperPedal != nullptr && looperPedal->isVisible())
        if (looperPedal->updateLooper() && flow != nullptr)
            flow->repaint();

    if ((timerTick % 10) == 0)
    {
        // Carga de CPU y cortes de audio, tres veces por segundo
        const int percent = juce::roundToInt (100.0f * processor.getCpuLoad());
        const int overloads = processor.getOverloadCount();
        juce::String text = "CPU " + juce::String (percent) + " %";

        if (overloads > 0)
            text << juce::String::fromUTF8 ("  ·  ") << overloads << (overloads == 1 ? " corte" : " cortes");

        cpuLabel.setText (text, juce::dontSendNotification);
        cpuLabel.setColour (juce::Label::textColourId, percent >= 85 ? ledRed : (percent >= 60 ? accentColour : dimTextColour));
    }

    // Rebuild the board and the preset list only when something changed
    const bool boardChanged = processor.getBoardRevision() != lastBoardRevision;
    if (boardChanged && draggedPedal == nullptr)
        refreshBoard();
    if ((boardChanged || processor.getCurrentPresetName() != shownPresetName) && draggedPedal == nullptr)
        refreshPresetList();

    // The IR can also change through state restore
    if (processor.getImpulseResponseName() != cab->getShownIrName())
        cab->updateIrState();

    // Same for the amp capture
    if (amp->captureStateIsStale())
        amp->updateCaptureState();
}

//==============================================================================
void EffectSuiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    const float scale = uiScale();
    auto px_ = [scale] (float v) { return juce::roundToInt (v * scale); };

    // Top bar
    g.setColour (juce::Colour (0xff191c21));
    g.fillRect (topBar);
    g.setColour (accentColour);
    g.fillRect (topBar.withTop (topBar.getBottom() - 2));

    g.setColour (textColour);
    g.setFont (juce::FontOptions (27.0f * scale, juce::Font::bold));
    g.drawText ("EffectSuite", topBar.withTrimmedLeft (juce::roundToInt (16 * scale)).withWidth (juce::roundToInt (170 * scale)),
                juce::Justification::centredLeft);

    // Preset strip
    g.setColour (juce::Colour (0xff12141a));
    g.fillRect (presetBar);
    g.setColour (juce::Colour (0xff3a3f49));
    for (int x : { stripSepA, stripSepB })
        if (x >= 0)
            g.fillRect (x, presetBar.getY() + px_ (7), 1, presetBar.getHeight() - px_ (14));

    // Tuner
    auto t = tunerArea.toFloat();
    g.setColour (juce::Colour (0xff0f1115));
    g.fillRoundedRectangle (t, 6.0f);

    auto noteArea = t.removeFromLeft (t.getWidth() * 0.26f);
    const bool hasNote = tunerHz > 0.0f;
    const bool inTune = hasNote && std::abs (tunerCents) <= 3.0f;
    g.setColour (hasNote ? (inTune ? okColour : textColour) : dimTextColour);
    g.setFont (juce::FontOptions (26.0f * scale, juce::Font::bold));
    g.drawText (tunerNote, noteArea, juce::Justification::centred);

    auto bar = t.reduced (8.0f * scale, 10.0f * scale);
    auto hzArea = bar.removeFromRight (bar.getWidth() * 0.24f);
    g.setColour (dimTextColour);
    g.setFont (juce::FontOptions (smallFont (13.0f, scale)));
    g.drawText (hasNote ? juce::String (tunerHz, 1) + " Hz" : juce::String ("-- Hz"), hzArea, juce::Justification::centred);

    auto scaleArea = bar.reduced (4.0f * scale, 0.0f);
    const float cy = scaleArea.getCentreY();
    auto xFor = [&] (float cents) { return scaleArea.getX() + (cents + 50.0f) / 100.0f * scaleArea.getWidth(); };
    for (int c = -50; c <= 50; c += 10)
    {
        g.setColour (c == 0 ? textColour : juce::Colour (0xff4a505b));
        const float h = (c % 50 == 0 ? 0.35f : 0.2f) * scaleArea.getHeight();
        g.fillRect (xFor ((float) c) - 0.5f, cy - h, 1.0f, h * 2.0f);
    }
    g.setColour (okColour.withAlpha (0.25f));
    g.fillRect (xFor (-3.0f), cy - scaleArea.getHeight() * 0.4f, xFor (3.0f) - xFor (-3.0f), scaleArea.getHeight() * 0.8f);
    if (hasNote)
    {
        g.setColour (inTune ? okColour : accentColour);
        g.fillRoundedRectangle (xFor (tunerCents) - 2.0f * scale, cy - scaleArea.getHeight() * 0.45f,
                                4.0f * scale, scaleArea.getHeight() * 0.9f, 2.0f);
    }

    // Boards behind the three rows, each with its caption
    const juce::String captions[] = { "ANTES DEL AMPLI", "AMPLI", juce::CharPointer_UTF8 ("DESPU\xc3\x89S DEL AMPLI") };
    const juce::Rectangle<int> boards[] = { preBoard, ampBoard, postBoard };
    const int captionH = juce::roundToInt (18 * scale);

    for (int i = 0; i < 3; ++i)
    {
        const auto board = boards[i].toFloat();
        g.setColour (boardColour);
        g.fillRoundedRectangle (board, 8.0f * scale);
        g.setColour (juce::Colour (0xff23262c));
        g.drawRoundedRectangle (board.reduced (0.5f), 8.0f * scale, 1.0f);
        g.setColour (dimTextColour);
        g.setFont (juce::FontOptions (smallFont (11.5f, scale), juce::Font::bold));
        g.drawText (captions[i], boards[i].withHeight (captionH).withTrimmedLeft (juce::roundToInt (10 * scale)),
                    juce::Justification::centredLeft);
    }
}

void EffectSuiteEditor::resized()
{
    auto area = getLocalBounds();
    const float s = uiScale();
    auto px = [s] (float v) { return juce::roundToInt (v * s); };
    const int pad = px (6);

    //==========================================================================
    topBar = area.removeFromTop (px (84));
    {
        auto tb = topBar.reduced (px (14), px (8));
        tb.removeFromBottom (2);
        tb.removeFromLeft (px (176));

        const int labelH = juce::roundToInt (smallFont (12.0f, s) + 3.0f);
        const int comboH = px (26);
        const int meterW = px (14);
        const int knobCell = px (62);

        auto stacked = [&] (juce::Label& label, juce::ComboBox& box, juce::Rectangle<int> column)
        {
            column = column.withSizeKeepingCentre (column.getWidth(), labelH + comboH);
            label.setBounds (column.removeFromTop (labelH));
            label.setFont (juce::FontOptions (smallFont (12.0f, s)));
            box.setBounds (column);
        };

        // Input on the left
        placeKnob (*inputGain, tb.removeFromLeft (knobCell), labelH, smallFont (12.0f, s), px (60));
        tb.removeFromLeft (px (8));
        stacked (inputModeLabel, inputMode, tb.removeFromLeft (juce::jmax (70, px (118))));
        tb.removeFromLeft (px (8));
        inMeter.setBounds (tb.removeFromLeft (meterW));
        tb.removeFromLeft (px (16));

        // Output on the right
        outMeter.setBounds (tb.removeFromRight (meterW));
        tb.removeFromRight (px (8));
        placeKnob (*outputLevel, tb.removeFromRight (knobCell), labelH, smallFont (12.0f, s), px (60));
        tb.removeFromRight (px (14));
        stacked (qualityLabel, quality, tb.removeFromRight (juce::jmax (84, px (128))));
        tb.removeFromRight (px (12));
        tunerMute.setBounds (tb.removeFromRight (px (128)).withSizeKeepingCentre (px (128), px (44)));
        tb.removeFromRight (px (10));

        tunerArea = tb.withSizeKeepingCentre (tb.getWidth(), juce::jmin (tb.getHeight(), px (54)));
    }

    //==========================================================================
    presetBar = area.removeFromTop (px (38));
    {
        auto pb = presetBar.reduced (px (14), px (4));
        const int gap = px (8);
        const int h = pb.getHeight();

        cpuLabel.setBounds (pb.removeFromRight (px (190)));
        cpuLabel.setFont (juce::FontOptions (smallFont (12.0f, s)));
        pb.removeFromRight (gap);

        // Whether the recording group can show its captions depends on the room left for the preset box
        auto groupWidth = [&] (bool text)
        {
            const int btn = juce::jmax (h, px (22));
            return text ? px (92) + px (56) + px (88) + px (92) + px (122) + px (80) + px (62) + gap * 8 + px (24)
                        : btn * 3 + juce::jmax (40, px (50)) + px (44) + juce::jmax (60, px (62)) + juce::jmax (36, px (46)) + gap * 8 + px (12);
        };
        const int fixedLeft = px (62) + px (10) + px (96) + gap + px (96) + gap;
        const bool text = pb.getWidth() - groupWidth (true) - fixedLeft >= px (190);
        const int btn = text ? 0 : juce::jmax (h, px (22));

        auto group = pb.removeFromRight (juce::jmin (pb.getWidth() / 2, groupWidth (text)));
        auto take = [&] (int w) { auto b = group.removeFromLeft (w); group.removeFromLeft (gap); return b; };
        auto take2 = [&] (int w) { auto b = group.removeFromLeft (w); group.removeFromLeft (gap / 2); return b; };

        stripSepA = group.getX();
        group.removeFromLeft (gap);
        recordButton.showText = folderButton.showText = listenButton.showText = text;
        recordButton.setBounds (take (text ? px (92) : btn));
        recordTime.setBounds (take2 (text ? px (56) : juce::jmax (40, px (50))));
        recordTime.setFont (juce::FontOptions (smallFont (13.0f, s), juce::Font::bold));
        folderButton.setBounds (take (text ? px (88) : btn));
        listenButton.setBounds (take (text ? px (92) : btn));
        stripSepB = group.getX() - gap / 2 - 1;
        metroToggle.setButtonText (text ? juce::String::fromUTF8 ("Metrónomo") : juce::String());
        metroToggle.setBounds (take (text ? px (122) : px (44)));
        metroBpm.setBounds (take2 (text ? px (80) : juce::jmax (60, px (62))).withSizeKeepingCentre (text ? px (80) : juce::jmax (60, px (62)), juce::jmin (h, px (24))));
        metroLevel.setBounds (group.withSizeKeepingCentre (group.getWidth(), juce::jmin (h, px (22))));

        pb.removeFromRight (gap);
        presetLabel.setBounds (pb.removeFromLeft (px (62)));
        presetLabel.setFont (juce::FontOptions (smallFont (12.0f, s)));
        presetBox.setBounds (pb.removeFromLeft (juce::jmin (px (340), pb.getWidth() - px (10) - px (96) * 2 - gap * 2)));
        pb.removeFromLeft (px (10));
        saveButton.setBounds (pb.removeFromLeft (px (96)));
        pb.removeFromLeft (gap);
        deleteButton.setBounds (pb.removeFromLeft (px (96)));
    }

    area.reduce (px (10), 0);
    area.removeFromTop (pad);
    flow->scale = s;
    flow->setBounds (area.removeFromTop (px (34)));
    area.removeFromTop (pad);
    area.removeFromBottom (px (8));

    const int captionH = px (18);
    const int pedalRowH = juce::roundToInt ((float) (area.getHeight() - pad * 2) * 0.36f);

    preBoard = area.removeFromTop (pedalRowH);
    area.removeFromTop (pad);
    postBoard = area.removeFromBottom (pedalRowH);
    area.removeFromBottom (pad);
    ampBoard = area;

    layoutPedalRow (false, false);
    layoutPedalRow (true, false);

    {
        auto r = ampBoard.reduced (px (6), px (5)).withTrimmedTop (captionH - px (3));
        amp->scale = s;
        cab->scale = s;
        amp->setBounds (r.removeFromLeft (juce::roundToInt ((float) r.getWidth() * 0.6f)));
        r.removeFromLeft (px (8));
        cab->setBounds (r);
    }

    storeEditorWidth();
}

void EffectSuiteEditor::toggleRecording()
{
    if (processor.isRecording())
    {
        processor.stopRecording();

        if (const double lost = processor.getRecordingDropoutSeconds(); lost > 0.0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, juce::String::fromUTF8 ("La toma tiene huecos"),
                                                    juce::String::fromUTF8 ("El disco no pudo seguir el ritmo y faltan ")
                                                        + juce::String (lost, 1) + juce::String::fromUTF8 (" s de la grabación. Repite la toma."), {}, this);
    }
    else if (const auto error = processor.startRecording(); error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No se pudo grabar",
                                                error, {}, this);
    updateRecordingUi (true);
}

void EffectSuiteEditor::updateRecordingUi (bool force)
{
    // The processor is the single source of truth: nothing is cached except what was last drawn
    const bool rec = processor.isRecording();
    const bool changed = force || rec != lastRecording;
    lastRecording = rec;

    if (rec)
    {
        const int tenths = (int) (processor.getRecordedSeconds() * 10.0);
        recordTime.setText (juce::String::formatted ("%d:%02d.%d", tenths / 600, (tenths % 600) / 10, tenths % 10),
                            juce::dontSendNotification);
        recordButton.repaint();   // pulse
    }

    if (changed)
    {
        recordButton.active = rec;
        recordButton.repaint();
        recordTime.setColour (juce::Label::textColourId, rec ? juce::Colour (0xffe5484d) : dimTextColour);
        if (! rec && force)
            recordTime.setText ("0:00.0", juce::dontSendNotification);
    }

    if (changed || (timerTick % 10) == 0)
    {
        lastTakeExists = processor.getLastTake().existsAsFile();
        listenButton.setEnabled (lastTakeExists && ! rec);
    }
}

void EffectSuiteEditor::storeEditorWidth()
{
    if (sizeRestored && getWidth() > 0
        && processor.getEditorWidth() != getWidth())
        processor.setEditorWidth (getWidth());
}
