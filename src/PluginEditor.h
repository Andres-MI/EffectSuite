#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class EffectSuiteEditor final : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit EffectSuiteEditor (EffectSuiteProcessor&);
    ~EffectSuiteEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    class KnobLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        KnobLookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
    };

    class Meter final : public juce::Component
    {
    public:
        void pushLevel (float linearPeak);
        void paint (juce::Graphics&) override;
    private:
        float levelDb = -100.0f;
        float holdDb = -100.0f;
        int holdCounter = 0;
    };

    enum class ValueStyle { unit, decibels, milliseconds };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment; // last: destroyed first
    };

    struct Panel
    {
        juce::String title;
        juce::String onParamId;                        // empty: always active
        juce::Rectangle<float> bounds;
        juce::Rectangle<int> content;                  // area below the title
        std::vector<Knob*> knobs;
        juce::ComboBox* combo = nullptr;               // optional model selector (not owned)
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<ButtonAttachment> toggleAttachment; // after toggle: destroyed first
        bool active = true;
    };

    enum PanelIndex { pInput, pGate, pComp, pOverdrive, pDist, pFuzz,
                      pAmp, pCab, pChorus, pDelay, pReverb, pOutput, numPanels };

    void timerCallback() override;
    void addPanel (PanelIndex, const juce::String& title, const juce::String& onParamId);
    void addKnob (PanelIndex, const juce::String& text, const juce::String& paramId,
                  ValueStyle = ValueStyle::unit);
    void setupCombo (juce::ComboBox&, const juce::String& paramId, std::unique_ptr<ComboAttachment>&);
    void layoutKnob (Knob&, juce::Rectangle<int>);
    void layoutKnobRow (const std::vector<Knob*>&, size_t first, size_t count, juce::Rectangle<int>);
    void chooseImpulseResponse();
    void updateIrLabel();
    void updateTuner();
    float uiScale() const { return (float) getHeight() / 720.0f; }

    EffectSuiteProcessor& processor;
    KnobLookAndFeel lnf;

    // Components
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ComboBox inputMode, ampModel, quality, odModel, distModel, fuzzModel, cabModel;
    juce::ToggleButton tunerMute;
    juce::TextButton loadIr { juce::CharPointer_UTF8 ("Cargar IR\xe2\x80\xa6") }, clearIr { "Quitar" };
    juce::Label irName, inputModeLabel, ampModelLabel, qualityLabel, cabModelLabel;
    Meter inMeter, outMeter;
    std::unique_ptr<juce::FileChooser> chooser;

    // Attachments (after the components they reference)
    std::unique_ptr<ButtonAttachment> tunerMuteA;
    std::unique_ptr<ComboAttachment> inputModeA, ampModelA, qualityA, odModelA, distModelA, fuzzModelA, cabModelA;

    // Each panel owns its bypass toggle and that toggle's attachment; knobs own their attachments
    std::array<Panel, numPanels> panels;
    juce::Rectangle<int> topBar, tunerArea;

    // Tuner state
    int timerTick = 0;
    float tunerHz = 0.0f;
    int tunerMissed = 100;
    float tunerCents = 0.0f;
    juce::String tunerNote = "--";
    juce::String lastIrName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSuiteEditor)
};
