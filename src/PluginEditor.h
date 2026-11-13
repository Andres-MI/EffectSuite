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

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment; // last: destroyed first
    };

    struct Panel
    {
        juce::String title;
        juce::Rectangle<float> bounds;
        juce::ToggleButton* toggle = nullptr;
        bool active = true;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const juce::String& text, const juce::String& paramId);
    void setupToggle (juce::ToggleButton&, const juce::String& text, const juce::String& paramId,
                      std::unique_ptr<ButtonAttachment>&);
    void setupCombo (juce::ComboBox&, const juce::String& paramId, std::unique_ptr<ComboAttachment>&);
    void layoutKnob (Knob&, juce::Rectangle<int>);
    void chooseImpulseResponse();
    void updateIrLabel();
    bool isParamOn (const char* id) const;
    void updateTuner();

    EffectSuiteProcessor& processor;
    KnobLookAndFeel lnf;

    // Components
    Knob inputGain, gateThreshold, odDrive, odTone, odLevel,
         ampGain, ampBass, ampMid, ampTreble, ampPresence, ampMaster, outputLevel;
    juce::ComboBox inputMode, ampModel, quality;
    juce::ToggleButton gateOn, odOn, ampOn, cabOn, tunerMute;
    juce::TextButton loadIr { juce::CharPointer_UTF8 ("Cargar IR\xe2\x80\xa6") }, clearIr { "Quitar" };
    juce::Label irName, inputModeLabel, ampModelLabel, qualityLabel;
    Meter inMeter, outMeter;
    std::unique_ptr<juce::FileChooser> chooser;

    // Attachments (after the components they reference)
    std::unique_ptr<ButtonAttachment> gateOnA, odOnA, ampOnA, cabOnA, tunerMuteA;
    std::unique_ptr<ComboAttachment> inputModeA, ampModelA, qualityA;

    std::array<Panel, 6> panels;
    juce::Rectangle<int> topBar, tunerArea;

    // Tuner state
    bool audioSeen = false;
    float tunerHz = 0.0f;
    int tunerMissed = 100;
    float tunerCents = 0.0f;
    juce::String tunerNote = "--";
    juce::String lastIrName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSuiteEditor)
};
