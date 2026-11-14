#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

/** Pedalboard-style editor: a top bar (input, tuner, output), a clickable signal-flow strip,
    a row of stompboxes before the amp, the amp head and cabinet, and a row of stompboxes after. */
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

    enum class ValueStyle { unit, decibels, milliseconds };
    struct PedalDef;

    //==========================================================================
    class RigLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        RigLookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos,
                               float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
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

    /** Round metallic stomp switch. */
    class FootSwitch final : public juce::Button
    {
    public:
        FootSwitch() : juce::Button ({}) { setClickingTogglesState (true); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    /** Rocker power switch for the amp and the cabinet. */
    class PowerSwitch final : public juce::Button
    {
    public:
        PowerSwitch() : juce::Button ({}) { setClickingTogglesState (true); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    /** Small round "x" in the corner of a pedal; subtle until hovered. */
    class RemoveButton final : public juce::Button
    {
    public:
        RemoveButton() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    /** Dashed tile at the end of a pedal row that opens the "add pedal" menu. */
    class AddTile final : public juce::Component
    {
    public:
        AddTile() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

        std::function<void()> onClick;
        float scale = 1.0f;
    private:
        bool hover = false;
    };

    /** One pedal: coloured enclosure, controls, name, LED and footswitch. */
    class Stompbox final : public juce::Component
    {
    public:
        Stompbox (EffectSuiteEditor&, const PedalDef&);

        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        const juce::String& getId() const noexcept       { return id; }
        const juce::String& getChipText() const noexcept { return chipText; }
        const juce::String& getName() const noexcept     { return name; }
        juce::Button& getSwitch() noexcept      { return footSwitch; }
        juce::Colour getColour() const noexcept { return colour; }

        float scale = 1.0f;

    private:
        void refresh();

        EffectSuiteEditor& editor;
        juce::String name, id, chipText;
        juce::Colour colour;
        bool sliders = false, hasCombo = false, lastOn = false;
        bool dragArmed = false, dragging = false;

        std::vector<std::unique_ptr<Knob>> knobs;
        juce::ComboBox combo;
        FootSwitch footSwitch;
        RemoveButton removeButton;
        std::unique_ptr<ComboAttachment> comboAttachment;   // after the components
        std::unique_ptr<ButtonAttachment> switchAttachment;

        juce::Rectangle<int> controlsArea, nameArea, ledArea;
    };

    /** Amp head: grille with the model selector, control plate with power switch and six knobs. */
    class AmpUnit final : public juce::Component
    {
    public:
        explicit AmpUnit (EffectSuiteEditor&);

        void paint (juce::Graphics&) override;
        void resized() override;
        juce::Button& getSwitch() noexcept { return power; }
        void updateCaptureState();
        bool captureStateIsStale() const;

        float scale = 1.0f;

    private:
        void refresh();
        void chooseCapture();

        EffectSuiteEditor& editor;
        bool lastOn = false;
        juce::String lastCaptureName, lastCaptureNote;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::ComboBox model;
        juce::Label captureName;
        juce::TextButton loadCapture { juce::CharPointer_UTF8 ("Cargar captura\xe2\x80\xa6") }, clearCapture { "Quitar" };
        std::unique_ptr<juce::FileChooser> chooser;
        PowerSwitch power;
        std::unique_ptr<ComboAttachment> modelAttachment;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::Rectangle<int> grilleArea, plateArea, lightArea;
    };

    /** Speaker cabinet: grille, power switch, built-in cabinet selector and user IR loading. */
    class CabUnit final : public juce::Component
    {
    public:
        explicit CabUnit (EffectSuiteEditor&);

        void paint (juce::Graphics&) override;
        void resized() override;
        juce::Button& getSwitch() noexcept { return power; }
        void updateIrState();
        const juce::String& getShownIrName() const noexcept { return lastIrName; }

        float scale = 1.0f;

    private:
        void chooseImpulseResponse();

        EffectSuiteEditor& editor;
        bool lastOn = false;
        juce::String lastIrName;
        juce::ComboBox cabModel;
        juce::Label cabModelLabel, irName;
        juce::TextButton loadIr { juce::CharPointer_UTF8 ("Cargar IR\xe2\x80\xa6") }, clearIr { "Quitar" };
        PowerSwitch power;
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<ComboAttachment> cabModelAttachment;
        std::unique_ptr<ButtonAttachment> powerAttachment;
        juce::Rectangle<int> grilleArea, panelArea, titleArea, lightArea;
    };

    /** Thin row of chips, one per block in signal order; lit when the block is on, click to toggle. */
    class FlowStrip final : public juce::Component
    {
    public:
        void clearChips() { chips.clear(); }
        void addChip (const juce::String& text, juce::Colour, juce::Button& target);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        float scale = 1.0f;

    private:
        struct Chip
        {
            juce::String text;
            juce::Colour colour;
            juce::Button* target = nullptr;
            juce::Rectangle<float> bounds;
        };
        int chipAt (juce::Point<float>) const;
        std::vector<Chip> chips;
    };

    //==========================================================================
    void timerCallback() override;
    std::unique_ptr<Knob> createKnob (juce::Component& parent, const juce::String& text,
                                      const juce::String& paramId, ValueStyle, bool vertical = false);
    void fillCombo (juce::ComboBox&, const juce::String& paramId);
    static void placeKnob (Knob&, juce::Rectangle<int> cell, int labelHeight, float fontSize, int maxKnobSize);
    void blockStateChanged();
    void refreshBoard();
    void removeFromBoard (const juce::String& id);
    void showAddMenu (bool preRow, juce::Component& target);
    void updateTuner();
    float uiScale() const { return (float) getHeight() / (float) baseHeight; }

    // Pedal drag and drop (driven by the Stompbox mouse handlers)
    Stompbox* findPedal (const juce::String& id) const;
    bool isPostPedal (const Stompbox&) const;
    void beginPedalDrag (Stompbox&, juce::Point<int> editorPos);
    void updatePedalDrag (Stompbox&, juce::Point<int> editorPos);
    void endPedalDrag (Stompbox&, bool commit);

    // Presets
    void refreshPresetList();
    void syncPresetSelection();
    void presetChosen();
    void askPresetName();
    void saveCurrentAs (const juce::String& name);
    void askDeletePreset();

    static constexpr int baseWidth = 1320, baseHeight = 936;

    EffectSuiteProcessor& processor;
    RigLookAndFeel lnf;

    // Top bar
    std::unique_ptr<Knob> inputGain, outputLevel;
    juce::ComboBox inputMode, quality;
    juce::ToggleButton tunerMute;
    juce::Label inputModeLabel, qualityLabel;
    Meter inMeter, outMeter;
    std::unique_ptr<ButtonAttachment> tunerMuteA;          // after the components they reference
    std::unique_ptr<ComboAttachment> inputModeA, qualityA;

    // The rig. Each unit owns its controls and their attachments.
    std::vector<std::unique_ptr<Stompbox>> prePedals, postPedals;
    std::unique_ptr<AddTile> addPre, addPost;
    int lastBoardRevision = -1;
    std::unique_ptr<AmpUnit> amp;
    std::unique_ptr<CabUnit> cab;
    std::unique_ptr<FlowStrip> flow;                       // points at the units' switches: destroyed first

    // Preset bar
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Guardar" }, deleteButton { "Borrar" };
    juce::StringArray factoryNames, userNames;
    juce::String shownPresetName;
    bool presetIsUser = false;

    // Drag state
    Stompbox* draggedPedal = nullptr;
    int dragTargetIndex = -1, dragGrabX = 0;
    juce::Rectangle<int> dropMarker;

    juce::Rectangle<int> topBar, presetBar, tunerArea, preBoard, ampBoard, postBoard;

    // Tuner state
    int timerTick = 0;
    float tunerHz = 0.0f;
    int tunerMissed = 100;
    float tunerCents = 0.0f;
    juce::String tunerNote = "--";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSuiteEditor)
};
