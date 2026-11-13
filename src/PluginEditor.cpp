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

juce::Font EffectSuiteEditor::KnobLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    // Scales with the box so long model names still fit at the minimum editor size
    return juce::FontOptions (juce::jmin (15.0f, (float) box.getHeight() * 0.6f));
}

juce::Label* EffectSuiteEditor::KnobLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    // Value text follows the text box height so it scales with the editor
    l->setFont (juce::FontOptions ((float) s.getTextBoxHeight() * 0.75f));
    l->setMinimumHorizontalScale (0.7f);
    return l;
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

    const auto dB = ValueStyle::decibels;

    addPanel (pInput, "Entrada", {});
    addKnob (pInput, "Gain", "inputGain", dB);

    addPanel (pGate, "Puerta", "gateOn");
    addKnob (pGate, "Threshold", "gateThreshold", dB);

    addPanel (pComp, "Compresor", "compOn");
    addKnob (pComp, "Sustain", "compSustain");
    addKnob (pComp, "Attack", "compAttack");
    addKnob (pComp, "Level", "compLevel");

    addPanel (pOverdrive, "Overdrive", "odOn");
    addKnob (pOverdrive, "Drive", "odDrive");
    addKnob (pOverdrive, "Tone", "odTone");
    addKnob (pOverdrive, "Level", "odLevel");

    // Already upper case: String::toUpperCase() leaves the accented letter alone
    addPanel (pDist, juce::CharPointer_UTF8 ("DISTORSI\xc3\x93N"), "distOn");
    addKnob (pDist, "Gain", "distGain");
    addKnob (pDist, "Tone", "distTone");
    addKnob (pDist, "Level", "distLevel");

    addPanel (pFuzz, "Fuzz", "fuzzOn");
    addKnob (pFuzz, "Fuzz", "fuzzAmount");
    addKnob (pFuzz, "Tone", "fuzzTone");
    addKnob (pFuzz, "Level", "fuzzLevel");

    addPanel (pAmp, "Amplificador", "ampOn");
    addKnob (pAmp, "Gain", "ampGain");
    addKnob (pAmp, "Bass", "ampBass");
    addKnob (pAmp, "Mid", "ampMid");
    addKnob (pAmp, "Treble", "ampTreble");
    addKnob (pAmp, "Presence", "ampPresence");
    addKnob (pAmp, "Master", "ampMaster");

    addPanel (pCab, "Pantalla", "cabOn");

    addPanel (pChorus, "Chorus", "chorusOn");
    addKnob (pChorus, "Rate", "chorusRate");
    addKnob (pChorus, "Depth", "chorusDepth");
    addKnob (pChorus, "Mix", "chorusMix");

    addPanel (pDelay, "Delay", "delayOn");
    addKnob (pDelay, "Time", "delayTime", ValueStyle::milliseconds);
    addKnob (pDelay, "Feedback", "delayFeedback");
    addKnob (pDelay, "Mix", "delayMix");

    addPanel (pReverb, "Reverb", "reverbOn");
    addKnob (pReverb, "Size", "reverbSize");
    addKnob (pReverb, "Mix", "reverbMix");

    addPanel (pOutput, "Salida", {});
    addKnob (pOutput, "Output", "outputLevel", dB);

    tunerMute.setButtonText ("Silenciar");
    addAndMakeVisible (tunerMute);
    tunerMuteA = std::make_unique<ButtonAttachment> (processor.apvts, "tunerMute", tunerMute);

    setupCombo (inputMode, "inputMode", inputModeA);
    setupCombo (ampModel,  "ampModel",  ampModelA);
    setupCombo (quality,   "quality",   qualityA);
    setupCombo (odModel,   "odModel",   odModelA);
    setupCombo (distModel, "distModel", distModelA);
    setupCombo (fuzzModel, "fuzzModel", fuzzModelA);
    setupCombo (cabModel,  "cabModel",  cabModelA);
    panels[pOverdrive].combo = &odModel;
    panels[pDist].combo = &distModel;
    panels[pFuzz].combo = &fuzzModel;

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
    setupSmallLabel (cabModelLabel, "Integrada");

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

    setResizable (true, true);
    setResizeLimits (840, 504, 1800, 1080);
    getConstrainer()->setFixedAspectRatio (1200.0 / 720.0);
    setSize (1200, 720);

    startTimerHz (30);
}

EffectSuiteEditor::~EffectSuiteEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EffectSuiteEditor::addPanel (PanelIndex index, const juce::String& title, const juce::String& onParamId)
{
    auto& panel = panels[(size_t) index];
    panel.title = title;
    panel.onParamId = onParamId;

    if (onParamId.isNotEmpty())
    {
        panel.toggle = std::make_unique<juce::ToggleButton> ("On");
        addAndMakeVisible (*panel.toggle);
        panel.toggleAttachment = std::make_unique<ButtonAttachment> (processor.apvts, onParamId, *panel.toggle);
        panel.active = panel.toggle->getToggleState();
    }
}

void EffectSuiteEditor::addKnob (PanelIndex index, const juce::String& text, const juce::String& paramId,
                                 ValueStyle style)
{
    auto& k = *knobs.emplace_back (std::make_unique<Knob>());
    panels[(size_t) index].knobs.push_back (&k);

    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, dimTextColour);
    k.label.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, k.slider);

    // The attachment installs the parameter's own text conversion; replace it with the display format
    k.slider.textFromValueFunction = [style] (double v) -> juce::String
    {
        switch (style)
        {
            case ValueStyle::decibels:     return juce::String (v, 1) + " dB";
            case ValueStyle::milliseconds: return juce::String (juce::roundToInt (v)) + " ms";
            case ValueStyle::unit:         break;
        }
        return juce::String (v, 2);
    };
    k.slider.valueFromTextFunction = [] (const juce::String& t)
    {
        return t.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getDoubleValue();
    };
    k.slider.updateText();

    // Double-click resets to the parameter default
    if (auto* param = processor.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void EffectSuiteEditor::setupCombo (juce::ComboBox& c, const juce::String& paramId,
                                    std::unique_ptr<ComboAttachment>& a)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId)))
        c.addItemList (choice->choices, 1);
    addAndMakeVisible (c);
    a = std::make_unique<ComboAttachment> (processor.apvts, paramId, c);
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
    const bool userIr = lastIrName.isNotEmpty();
    irName.setText (userIr ? lastIrName : juce::String ("Sin IR"), juce::dontSendNotification);
    irName.setColour (juce::Label::textColourId, userIr ? textColour : dimTextColour);
    clearIr.setEnabled (userIr);
    // The built-in cabinet is not in use while a user IR is loaded
    cabModel.setEnabled (! userIr);
    cabModelLabel.setEnabled (! userIr);
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
        repaint (topBar);
    }

    // Follow the parameters (not just clicks) so host automation also dims the panels
    for (auto& panel : panels)
    {
        if (panel.onParamId.isEmpty())
            continue;

        const bool on = processor.apvts.getRawParameterValue (panel.onParamId)->load() > 0.5f;
        if (panel.active != on)
        {
            panel.active = on;
            repaint (panel.bounds.getSmallestIntegerContainer());
        }
    }

    if (processor.getImpulseResponseName() != lastIrName)
        updateIrLabel();
}

//==============================================================================
void EffectSuiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    const float scale = uiScale();

    // top bar
    g.setColour (juce::Colour (0xff191c21));
    g.fillRect (topBar);
    g.setColour (accentColour);
    g.fillRect (topBar.withTop (topBar.getBottom() - 2));

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
        g.drawText (p.title.toUpperCase(), p.bounds.withTrimmedLeft (12.0f * scale).withHeight (30.0f * scale),
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
        g.fillRoundedRectangle (p.bounds, 8.0f * uiScale());
    }
}

void EffectSuiteEditor::layoutKnob (Knob& k, juce::Rectangle<int> area)
{
    const float s = uiScale();
    const int labelH = juce::roundToInt (20 * s);
    const int boxH = juce::roundToInt (20 * s);
    // Keep label, knob and value together as one centred group instead of spreading them over a tall cell
    const int knobSize = juce::jmin (area.getWidth(), area.getHeight() - labelH - boxH, juce::roundToInt (120 * s));
    area = area.withSizeKeepingCentre (area.getWidth(), knobSize + labelH + boxH);
    k.label.setBounds (area.removeFromTop (labelH));
    k.label.setFont (juce::FontOptions (14.0f * s));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, area.getWidth(), boxH);
    k.slider.setBounds (area);
}

void EffectSuiteEditor::layoutKnobRow (const std::vector<Knob*>& list, size_t first, size_t count,
                                       juce::Rectangle<int> area)
{
    const int w = area.getWidth() / (int) count;
    for (size_t i = 0; i < count; ++i)
        layoutKnob (*list[first + i], area.removeFromLeft (w));
}

void EffectSuiteEditor::resized()
{
    auto area = getLocalBounds();
    const float s = uiScale();
    const int pad = juce::roundToInt (8 * s);
    const int titleH = juce::roundToInt (30 * s);
    const int rowH = juce::roundToInt (28 * s);

    topBar = area.removeFromTop (juce::roundToInt (64 * s));
    {
        auto tb = topBar.reduced (juce::roundToInt (14 * s), juce::roundToInt (9 * s));
        tb.removeFromLeft (juce::roundToInt (230 * s));
        auto q = tb.removeFromRight (juce::roundToInt (210 * s));
        qualityLabel.setBounds (q.removeFromLeft (juce::roundToInt (58 * s)));
        qualityLabel.setFont (juce::FontOptions (13.0f * s));
        quality.setBounds (q.reduced (0, juce::roundToInt (6 * s)));
        tb.removeFromRight (juce::roundToInt (10 * s));
        tunerMute.setBounds (tb.removeFromRight (juce::roundToInt (140 * s)));
        tb.removeFromRight (juce::roundToInt (10 * s));
        tunerArea = tb;
    }

    // Two rows of six panels, in signal order
    area.reduce (pad, pad);
    auto rowTop = area.removeFromTop (juce::roundToInt ((float) area.getHeight() * 0.42f));
    area.removeFromTop (pad);

    const float weights[2][6] = { { 1.45f, 1.1f, 1.6f, 1.6f, 1.6f, 1.6f },
                                  { 2.5f, 1.6f, 1.55f, 1.55f, 1.3f, 1.1f } };
    const juce::Rectangle<int> rows[2] = { rowTop, area };

    for (int row = 0; row < 2; ++row)
    {
        float total = 0.0f;
        for (float w : weights[row])
            total += w;

        const float avail = (float) rows[row].getWidth() - (float) pad * 5.0f;
        float x = (float) rows[row].getX();

        for (int i = 0; i < 6; ++i)
        {
            auto& panel = panels[(size_t) (row * 6 + i)];
            const float w = avail * weights[row][i] / total;
            panel.bounds = { x, (float) rows[row].getY(), w, (float) rows[row].getHeight() };
            x += w + (float) pad;

            auto inner = panel.bounds.toNearestInt().reduced (pad, 0);
            if (panel.toggle != nullptr)
                panel.toggle->setBounds (inner.withHeight (titleH)
                                              .withTrimmedLeft (inner.getWidth() - juce::roundToInt (64 * s)));
            panel.content = inner.withTrimmedTop (titleH).withTrimmedBottom (pad);
        }
    }

    const int meterW = juce::roundToInt (20 * s);

    // Top-row pedals: one row of knobs
    for (auto index : { pGate, pComp, pOverdrive, pDist, pFuzz })
    {
        auto& panel = panels[(size_t) index];
        auto r = panel.content;
        if (panel.combo != nullptr)
            panel.combo->setBounds (r.removeFromTop (rowH).reduced (0, juce::roundToInt (1 * s)));
        layoutKnobRow (panel.knobs, 0, panel.knobs.size(), r);
    }

    // Bottom-row pedals are taller than wide: two knobs on top, the rest centred below
    for (auto index : { pChorus, pDelay, pReverb })
    {
        auto& panel = panels[(size_t) index];
        auto r = panel.content;
        const int half = (r.getHeight() - pad) / 2;
        auto top = r.removeFromTop (half);
        auto bottom = r.removeFromBottom (half);

        if (panel.knobs.size() == 2)
        {
            layoutKnob (*panel.knobs[0], top);
            layoutKnob (*panel.knobs[1], bottom);
        }
        else
        {
            layoutKnobRow (panel.knobs, 0, 2, top);
            layoutKnob (*panel.knobs[2], bottom.withSizeKeepingCentre (bottom.getWidth() / 2, bottom.getHeight()));
        }
    }

    // Entrada: knob, channel combo, meter
    {
        auto r = panels[pInput].content;
        inMeter.setBounds (r.removeFromRight (meterW));
        r.removeFromRight (pad);
        auto row = r.removeFromBottom (rowH * 2);
        inputModeLabel.setBounds (row.removeFromTop (rowH));
        inputModeLabel.setFont (juce::FontOptions (13.0f * s));
        inputMode.setBounds (row.reduced (0, juce::roundToInt (2 * s)));
        layoutKnob (*panels[pInput].knobs[0], r);
    }
    // Amplificador: model combo, two rows of three knobs with a gap between them
    {
        auto r = panels[pAmp].content;
        auto row = r.removeFromTop (rowH);
        ampModelLabel.setBounds (row.removeFromLeft (juce::roundToInt (58 * s)));
        ampModelLabel.setFont (juce::FontOptions (13.0f * s));
        ampModel.setBounds (row.reduced (0, juce::roundToInt (1 * s)));
        r.removeFromTop (pad);
        const int half = (r.getHeight() - pad * 2) / 2;
        layoutKnobRow (panels[pAmp].knobs, 0, 3, r.removeFromTop (half));
        layoutKnobRow (panels[pAmp].knobs, 3, 3, r.removeFromBottom (half));
    }
    // Pantalla
    {
        auto r = panels[pCab].content;
        const int bh = juce::roundToInt (32 * s);
        cabModelLabel.setBounds (r.removeFromTop (juce::roundToInt (20 * s)));
        cabModelLabel.setFont (juce::FontOptions (13.0f * s));
        cabModel.setBounds (r.removeFromTop (rowH).reduced (0, juce::roundToInt (1 * s)));
        r.removeFromTop (pad * 2);
        loadIr.setBounds (r.removeFromTop (bh));
        r.removeFromTop (pad);
        irName.setBounds (r.removeFromTop (bh));
        irName.setFont (juce::FontOptions (14.0f * s));
        r.removeFromTop (pad);
        clearIr.setBounds (r.removeFromTop (bh));
    }
    // Salida
    {
        auto r = panels[pOutput].content;
        outMeter.setBounds (r.removeFromRight (meterW));
        r.removeFromRight (pad);
        layoutKnob (*panels[pOutput].knobs[0], r);
    }
}
