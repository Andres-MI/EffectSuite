#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/GuitarChain.h"
#include "dsp/PitchTracker.h"

class EffectSuiteProcessor final : public juce::AudioProcessor,
                                   private juce::AsyncUpdater
{
public:
    EffectSuiteProcessor();
    ~EffectSuiteProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EffectSuite"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Interfaz para el editor

    juce::AudioProcessorValueTreeState apvts;

    /** Picos de entrada y salida (lineales) desde la última lectura. El editor los lee con exchange (0). */
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

    /** Frecuencia detectada por el afinador en Hz, o 0 si no hay nota. Llamar desde el hilo de la interfaz. */
    float detectPitch() { return pitchTracker.detect(); }

    /** Carga una respuesta a impulso de pantalla (WAV/AIFF). Devuelve false si el archivo no es válido. */
    bool loadImpulseResponse (const juce::File&);
    void clearImpulseResponse();
    /** Nombre del archivo de IR cargado, o cadena vacía si se usa la pantalla integrada. */
    juce::String getImpulseResponseName() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    fx::ChainParams readParams() const;

    fx::GuitarChain chain;
    fx::PitchTracker pitchTracker;
    juce::AudioBuffer<float> work;      // canal 0 = entrada mono, 1 y 2 = salida izquierda y derecha
    int maxBlock = 0;
    std::atomic<int> latencyQuality { -1 };
    juce::File irFile;

    struct ParamRefs
    {
        std::atomic<float> *inputGain, *inputMode, *gateOn, *gateThreshold,
                           *wahOn, *wahMode, *wahPosition, *wahQ,
                           *octOn, *octSub, *octDry,
                           *phaserOn, *phaserRate, *phaserDepth, *phaserFeedback,
                           *eqOn, *eqLow, *eqLowMid, *eqHighMid, *eqHigh,
                           *flangerOn, *flangerRate, *flangerDepth, *flangerFeedback,
                           *tremOn, *tremRate, *tremDepth,
                           *compOn, *compSustain, *compAttack, *compLevel,
                           *odOn, *odModel, *odDrive, *odTone, *odLevel,
                           *distOn, *distModel, *distGain, *distTone, *distLevel,
                           *fuzzOn, *fuzzModel, *fuzzAmount, *fuzzTone, *fuzzLevel,
                           *ampOn, *ampModel, *ampGain, *ampBass, *ampMid, *ampTreble, *ampPresence, *ampMaster,
                           *cabOn, *cabModel,
                           *chorusOn, *chorusRate, *chorusDepth, *chorusMix,
                           *delayOn, *delayTime, *delayFeedback, *delayMix,
                           *reverbOn, *reverbSize, *reverbMix,
                           *outputLevel, *quality, *tunerMute;
    } params {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSuiteProcessor)
};
