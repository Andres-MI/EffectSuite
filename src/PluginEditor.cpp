#include "PluginEditor.h"

namespace
{
const juce::Colour bgColour      { 0xff14161a };
const juce::Colour panelColour   { 0xff1e2127 };
const juce::Colour accentColour  { 0xffe8a33d };
const juce::Colour textColour    { 0xffd8dce2 };
const juce::Colour dimTextColour { 0xff8b929c };
const juce::Colour okColour      { 0xff4cd07a };

constexpr float meterFloorDb = -60.0f;
}

//==============================================================================
EffectSuiteEditor::KnobLookAndFeel::KnobLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2e36));
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3f49));
    setColour (juce::ComboBox::arrowColourId, dimTextColour);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff23262d));
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.darker (0.4f));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e36));
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::Label::textColourId, textColour);
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void EffectSuiteEditor::KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                                           float pos, float startAngle, float endAngle,
                                                           juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax (2.0f, radius * 0.14f);
    const float arcR = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff343942));
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accentColour.withMultipliedAlpha (enabled ? 1.0f : 0.4f));
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float bodyR = arcR - thickness * 1.1f;
    g.setColour (juce::Colour (0xff2b2f37));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff4a505b));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    juce::Path pointer;
    const float pl = bodyR * 0.8f, pw = juce::jmax (2.0f, bodyR * 0.12f);
    pointer.addRoundedRectangle (-pw * 0.5f, -bodyR + 2.0f, pw, pl * 0.55f, pw * 0.5f);
    g.setColour (textColour);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void EffectSuiteEditor::KnobLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                                           bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (r.getHeight() * 0.6f, 22.0f);
    auto sw = juce::Rectangle<float> (d * 1.8f, d).withCentre ({ r.getX() + d * 0.9f + 2.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    g.setColour (on ? accentColour : juce::Colour (0xff343942));
    g.fillRoundedRectangle (sw, d * 0.5f);
    const float kx = on ? sw.getRight() - d * 0.5f : sw.getX() + d * 0.5f;
    g.setColour (highlighted ? juce::Colours::white : juce::Colour (0xffeceff3));
    g.fillEllipse (kx - d * 0.38f, sw.getCentreY() - d * 0.38f, d * 0.76f, d * 0.76f);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (juce::jlimit (11.0f, 18.0f, r.getHeight() * 0.5f)));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (sw.getRight() + 6.0f), juce::Justification::centredLeft, true);
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
EffectSuiteEditor::EffectSuiteEditor (EffectSuiteProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (inputGain,     "Gain",      "inputGain");
    setupKnob (gateThreshold, "Threshold", "gateThreshold");
    setupKnob (odDrive,       "Drive",     "odDrive");
    setupKnob (odTone,        "Tone",      "odTone");
    setupKnob (odLevel,       "Level",     "odLevel");
    setupKnob (ampGain,       "Gain",      "ampGain");
    setupKnob (ampBass,       "Bass",      "ampBass");
    setupKnob (ampMid,        "Mid",       "ampMid");
    setupKnob (ampTreble,     "Treble",    "ampTreble");
    setupKnob (ampPresence,   "Presence",  "ampPresence");
    setupKnob (ampMaster,     "Master",    "ampMaster");
    setupKnob (outputLevel,   "Output",    "outputLevel");

    setupToggle (gateOn,    "Activa", "gateOn",    gateOnA);
    setupToggle (odOn,      "Activo", "odOn",      odOnA);
    setupToggle (ampOn,     "Activo", "ampOn",     ampOnA);
    setupToggle (cabOn,     "Activa", "cabOn",     cabOnA);
    setupToggle (tunerMute, "Silenciar", "tunerMute", tunerMuteA);

    setupCombo (inputMode, "inputMode", inputModeA);
    setupCombo (ampModel,  "ampModel",  ampModelA);
    setupCombo (quality,   "quality",   qualityA);

    auto setupSmallLabel = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setColour (juce::Label::textColourId, dimTextColour);
        addAndMakeVisible (l);
    };
    setupSmallLabel (inputModeLabel, "Canal");
    setupSmallLabel (ampModelLabel, "Modelo");
    setupSmallLabel (qualityLabel, "Calidad");

    addAndMakeVisible (loadIr);
    addAndMakeVisible (clearIr);
    loadIr.onClick = [this] { chooseImpulseResponse(); };
    clearIr.onClick = [this] { processor.clearImpulseResponse(); updateIrLabel(); };

    irName.setJustificationType (juce::Justification::centred);
    irName.setColour (juce::Label::textColourId, textColour);
    irName.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (irName);
    updateIrLabel();

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    const char* titles[] = { "Entrada", "Puerta", "Overdrive", "Amplificador", "Pantalla", "Salida" };
    juce::ToggleButton* toggles[] = { nullptr, &gateOn, &odOn, &ampOn, &cabOn, nullptr };
    for (size_t i = 0; i < panels.size(); ++i)
    {
        panels[i].title = titles[i];
        panels[i].toggle = toggles[i];
    }

    setResizable (true, true);
    setResizeLimits (770, 294, 1650, 630);
    getConstrainer()->setFixedAspectRatio (1100.0 / 420.0);
    setSize (1100, 420);

    startTimerHz (30);
}

EffectSuiteEditor::~EffectSuiteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EffectSuiteEditor::setupKnob (Knob& k, const juce::String& text, const juce::String& paramId)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, dimTextColour);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);
    // Double-click resets to the parameter default
    if (auto* param = processor.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void EffectSuiteEditor::setupToggle (juce::ToggleButton& b, const juce::String& text, const juce::String& paramId,
                                     std::unique_ptr<ButtonAttachment>& a)
{
    b.setButtonText (text);
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
    a = std::make_unique<ButtonAttachment> (processor.apvts, paramId, b);
}

void EffectSuiteEditor::setupCombo (juce::ComboBox& c, const juce::String& paramId,
                                    std::unique_ptr<ComboAttachment>& a)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.addItemList (choice->choices, 1);
    addAndMakeVisible (c);
    a = std::make_unique<ComboAttachment> (processor.apvts, paramId, c);
}

bool EffectSuiteEditor::isParamOn (const char* id) const
{
    return processor.apvts.getRawParameterValue (id)->load() > 0.5f;
}

//==============================================================================
void EffectSuiteEditor::chooseImpulseResponse()
{
    chooser = std::make_unique<juce::FileChooser> ("Seleccionar respuesta a impulso",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                   "*.wav;*.aif;*.aiff");
    juce::Component::SafePointer<EffectSuiteEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto f = fc.getResult();
        if (f != juce::File())
            safe->processor.loadImpulseResponse (f);
        safe->updateIrLabel();
    });
}

void EffectSuiteEditor::updateIrLabel()
{
    lastIrName = processor.getImpulseResponseName();
    irName.setText (lastIrName.isEmpty() ? juce::String ("Integrada") : lastIrName, juce::dontSendNotification);
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

    static int tick = 0;
    if ((++tick % 2) == 0)
    {
        updateTuner();
        repaint (topBar);
    }

    const bool states[] = { true, isParamOn ("gateOn"), isParamOn ("odOn"), isParamOn ("ampOn"),
                            isParamOn ("cabOn"), true };
    bool changed = false;
    for (size_t i = 0; i < panels.size(); ++i)
        if (panels[i].active != states[i])
        {
            panels[i].active = states[i];
            changed = true;
        }
    if (changed)
        repaint();

    if (processor.getImpulseResponseName() != lastIrName)
        updateIrLabel();
}

//==============================================================================
void EffectSuiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    const float scale = (float) getHeight() / 420.0f;

    // top bar
    g.setColour (juce::Colour (0xff191c21));
    g.fillRect (topBar);
    g.setColour (accentColour);
    g.fillRect (topBar.removeFromBottom (2));

    g.setColour (textColour);
    g.setFont (juce::FontOptions (28.0f * scale, juce::Font::bold));
    g.drawText ("EffectSuite", topBar.withTrimmedLeft ((int) (20 * scale)).withWidth ((int) (230 * scale)),
                juce::Justification::centredLeft);

    // tuner
    auto t = tunerArea.toFloat();
    g.setColour (juce::Colour (0xff0f1115));
    g.fillRoundedRectangle (t, 6.0f);

    auto noteArea = t.removeFromLeft (t.getWidth() * 0.28f);
    const bool hasNote = tunerHz > 0.0f;
    const bool inTune = hasNote && std::abs (tunerCents) <= 3.0f;
    g.setColour (hasNote ? (inTune ? okColour : textColour) : dimTextColour);
    g.setFont (juce::FontOptions (26.0f * scale, juce::Font::bold));
    g.drawText (tunerNote, noteArea, juce::Justification::centred);

    auto bar = t.reduced (8.0f * scale, 10.0f * scale);
    auto hzArea = bar.removeFromRight (bar.getWidth() * 0.2f);
    g.setColour (dimTextColour);
    g.setFont (juce::FontOptions (13.0f * scale));
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

    // panels
    for (auto& p : panels)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (p.bounds, 8.0f * scale);
        g.setColour (juce::Colour (0xff2f343d));
        g.drawRoundedRectangle (p.bounds.reduced (0.5f), 8.0f * scale, 1.0f);
        g.setColour (accentColour.withAlpha (p.active ? 1.0f : 0.4f));
        g.setFont (juce::FontOptions (15.0f * scale, juce::Font::bold));
        g.drawText (p.title.toUpperCase(), p.bounds.withTrimmedLeft (12.0f * scale).withHeight (28.0f * scale),
                    juce::Justification::centredLeft);
    }
}

void EffectSuiteEditor::paintOverChildren (juce::Graphics& g)
{
    // Dim bypassed sections (the toggle itself stays readable because the veil is drawn below it via clip)
    for (auto& p : panels)
    {
        if (p.active)
            continue;
        juce::Graphics::ScopedSaveState ss (g);
        if (p.toggle != nullptr)
            g.excludeClipRegion (p.toggle->getBounds());
        g.setColour (bgColour.withAlpha (0.55f));
        g.fillRoundedRectangle (p.bounds, 8.0f * (float) getHeight() / 420.0f);
    }
}

void EffectSuiteEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    const int labelH = juce::jmax (12, (int) (area.getHeight() * 0.14f));
    k.label.setBounds (area.removeFromTop (labelH));
    const int boxH = juce::jlimit (12, 22, (int) (area.getHeight() * 0.17f));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (40, area.getWidth() - 8), boxH);
    k.slider.setBounds (area);
    k.label.setFont (juce::FontOptions ((float) juce::jlimit (11, 16, labelH)));
}

void EffectSuiteEditor::resized()
{
    auto area = getLocalBounds();
    const float s = (float) getHeight() / 420.0f;
    const int pad = juce::roundToInt (8 * s);
    const int titleH = juce::roundToInt (30 * s);
    const int rowH = juce::roundToInt (28 * s);

    topBar = area.removeFromTop (juce::roundToInt (64 * s));
    {
        auto tb = topBar.reduced (juce::roundToInt (14 * s), juce::roundToInt (9 * s));
        tb.removeFromLeft (juce::roundToInt (230 * s));
        auto q = tb.removeFromRight (juce::roundToInt (210 * s));
        qualityLabel.setBounds (q.removeFromLeft (juce::roundToInt (54 * s)));
        qualityLabel.setFont (juce::FontOptions (13.0f * s));
        quality.setBounds (q.reduced (0, juce::roundToInt (4 * s)));
        tb.removeFromRight (juce::roundToInt (10 * s));
        tunerMute.setBounds (tb.removeFromRight (juce::roundToInt (130 * s)));
        tb.removeFromRight (juce::roundToInt (10 * s));
        tunerArea = tb;
    }

    area.reduce (pad, pad);
    const float weights[] = { 1.25f, 1.0f, 1.65f, 3.0f, 1.55f, 1.2f };
    float total = 0;
    for (float w : weights) total += w;
    const float gap = (float) pad;
    const float avail = (float) area.getWidth() - gap * 5.0f;

    float x = (float) area.getX();
    juce::Rectangle<int> pr[6];
    for (int i = 0; i < 6; ++i)
    {
        const float w = avail * weights[i] / total;
        panels[(size_t) i].bounds = { x, (float) area.getY(), w, (float) area.getHeight() };
        pr[i] = panels[(size_t) i].bounds.toNearestInt().reduced (pad, 0).withTrimmedBottom (pad);
        pr[i].removeFromTop (0);
        x += w + gap;
    }

    auto content = [&] (int i)
    {
        auto r = pr[i];
        r.removeFromTop (titleH);
        return r;
    };

    // Entrada: knob, combo, meter
    {
        auto r = content (0);
        auto m = r.removeFromRight (juce::roundToInt (22 * s));
        inMeter.setBounds (m.reduced (0, 2));
        r.removeFromRight (pad);
        auto row = r.removeFromBottom (rowH * 2);
        inputModeLabel.setBounds (row.removeFromTop (rowH));
        inputModeLabel.setFont (juce::FontOptions (13.0f * s));
        inputMode.setBounds (row.reduced (0, 2));
        layoutKnob (inputGain, r);
    }
    // Puerta
    {
        auto r = content (1);
        auto head = panels[1].bounds.toNearestInt().reduced (pad, 0);
        gateOn.setBounds (head.withHeight (titleH).withTrimmedLeft (head.getWidth() / 2));
        layoutKnob (gateThreshold, r);
    }
    // Overdrive
    {
        auto r = content (2);
        auto head = panels[2].bounds.toNearestInt().reduced (pad, 0);
        odOn.setBounds (head.withHeight (titleH).withTrimmedLeft (head.getWidth() / 2));
        const int w = r.getWidth() / 3;
        layoutKnob (odDrive, r.removeFromLeft (w));
        layoutKnob (odTone, r.removeFromLeft (w));
        layoutKnob (odLevel, r);
    }
    // Amplificador
    {
        auto r = content (3);
        auto head = panels[3].bounds.toNearestInt().reduced (pad, 0);
        ampOn.setBounds (head.withHeight (titleH).withTrimmedLeft (head.getWidth() * 2 / 3));
        auto row = r.removeFromTop (rowH);
        ampModelLabel.setBounds (row.removeFromLeft (juce::roundToInt (56 * s)));
        ampModelLabel.setFont (juce::FontOptions (13.0f * s));
        ampModel.setBounds (row.reduced (0, 1));
        auto top = r.removeFromTop (r.getHeight() / 2);
        const int w = r.getWidth() / 3;
        Knob* k[] = { &ampGain, &ampBass, &ampMid, &ampTreble, &ampPresence, &ampMaster };
        for (int i = 0; i < 3; ++i)
        {
            layoutKnob (*k[i], top.removeFromLeft (w));
            layoutKnob (*k[i + 3], r.removeFromLeft (w));
        }
    }
    // Pantalla
    {
        auto r = content (4);
        auto head = panels[4].bounds.toNearestInt().reduced (pad, 0);
        cabOn.setBounds (head.withHeight (titleH).withTrimmedLeft (head.getWidth() / 2));
        const int bh = juce::roundToInt (34 * s);
        r.removeFromTop (pad);
        loadIr.setBounds (r.removeFromTop (bh));
        r.removeFromTop (pad);
        irName.setBounds (r.removeFromTop (bh));
        irName.setFont (juce::FontOptions (14.0f * s));
        r.removeFromTop (pad);
        clearIr.setBounds (r.removeFromTop (bh));
    }
    // Salida
    {
        auto r = content (5);
        auto m = r.removeFromRight (juce::roundToInt (22 * s));
        outMeter.setBounds (m.reduced (0, 2));
        r.removeFromRight (pad);
        layoutKnob (outputLevel, r);
    }
}
