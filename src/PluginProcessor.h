#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/GuitarChain.h"
#include "dsp/PitchTracker.h"
#include "Recorder.h"

class EffectSuiteProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
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
    bool acceptsMidi() const override { return true; }
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

    /** Carga de CPU del procesado de audio, de 0 a 1 (1 = no da tiempo a procesar un bloque antes del siguiente). */
    float getCpuLoad() const { return (float) loadMeasurer.getLoadAsProportion(); }
    /** Veces que un bloque ha tardado más de lo que dura (cortes de audio) desde que se preparó el audio. */
    int getOverloadCount() const { return loadMeasurer.getXRunCount(); }

    /** Frecuencia detectada por el afinador en Hz, o 0 si no hay nota. Llamar desde el hilo de la interfaz. */
    float detectPitch() { return pitchTracker.detect(); }

    /** Carga una respuesta a impulso de pantalla (WAV/AIFF). Devuelve false si el archivo no es válido. */
    bool loadImpulseResponse (const juce::File&);
    void clearImpulseResponse();
    /** Nombre del archivo de IR cargado, o cadena vacía si se usa la pantalla integrada. */
    juce::String getImpulseResponseName() const;

    /** Carga una captura Neural Amp Modeler (.nam), que sustituye al amplificador modelado.
        Devuelve una cadena vacía si se cargó, o el motivo del error. Hilo de mensajes. */
    juce::String loadNeuralModel (const juce::File&);
    void clearNeuralModel();
    /** Nombre del archivo de la captura cargada, o cadena vacía si se usa el ampli modelado. */
    juce::String getNeuralModelName() const;
    /** Aviso para mostrar junto al nombre (por ejemplo, frecuencia de muestreo distinta), o cadena vacía. */
    juce::String getNeuralModelNote() const;

    //==========================================================================
    // Pedalera: qué pedales están colocados. Un pedal que no está en la pedalera no se muestra
    // y está apagado. El orden de la señal es fijo; esto solo decide cuáles se ven.
    // Identificadores: gate, wah, comp, oct, phaser, od, dist, fuzz, eq, flanger, chorus, trem, delay, reverb.
    // El parámetro de encendido de cada uno es el identificador seguido de "On".

    static const juce::StringArray& getAllPedalIds();
    bool isOnBoard (const juce::String& pedalId) const;
    /** Coloca o quita un pedal. Al quitarlo también lo apaga. Hilo de mensajes. */
    void setOnBoard (const juce::String& pedalId, bool shouldBeOnBoard);
    /** Pedales colocados en una fila (antes o después del ampli), en el orden en que suenan. */
    juce::StringArray getBoardRow (bool afterAmp) const;
    /** Mueve un pedal colocado a la posición newIndex dentro de su fila (0 = el primero). Hilo de mensajes. */
    void movePedal (const juce::String& pedalId, int newIndex);
    /** Cambia cada vez que la pedalera cambia (también al cargar un estado): el editor lo consulta en su temporizador. */
    int getBoardRevision() const noexcept { return boardRevision.load(); }

    //==========================================================================
    // Grabación de tomas: cada toma son dos archivos WAV de 24 bits en la carpeta de grabaciones,
    // "<fecha> toma.wav" (sonido procesado, estéreo) y "<fecha> toma DI.wav" (guitarra limpia, mono).
    // El metrónomo se oye pero no se graba. Todo en el hilo de mensajes.

    /** Empieza una toma. Devuelve una cadena vacía, o el motivo por el que no se pudo. */
    juce::String startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return recorder.isRecording(); }
    double getRecordedSeconds() const noexcept { return recorder.getSecondsRecorded(); }
    /** Archivo del sonido procesado de la última toma (puede no existir si aún no se ha grabado nada). */
    juce::File getLastTake() const { return recorder.getLastTake(); }
    juce::File getLastDirectTake() const { return recorder.getLastDirectTake(); }
    juce::File getRecordingFolder() const;
    /** Solo para pruebas: usa otra carpeta de grabaciones. */
    void setRecordingFolder (const juce::File& folder) { recordingFolderOverride = folder; }

    //==========================================================================
    // MIDI. Control Change 11 (expresión) o 4 (pedal) mueve la posición del wah; Program Change
    // carga un preset por número: primero los de fábrica en su orden y después los de usuario.

    /** Aplica en el hilo de mensajes lo recibido por MIDI (lo llama el temporizador; público para las pruebas). */
    void applyPendingMidi();
    /** Preset que corresponde a un número de programa MIDI, o cadena vacía. */
    juce::String getPresetNameForProgram (int program, bool& isFactory) const;

    //==========================================================================
    // Presets. Un preset guarda pedales, ampli, pantalla, IR y captura; no toca los ajustes del
    // equipo (ganancia y canal de entrada, nivel de salida, calidad). Todo en el hilo de mensajes.

    static juce::StringArray getFactoryPresetNames();
    juce::StringArray getUserPresetNames() const;
    bool loadFactoryPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    /** Guarda el sonido actual como preset de usuario (sobrescribe si existe). Devuelve false si no se pudo escribir. */
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    /** Nombre del último preset cargado o guardado, o cadena vacía. */
    juce::String getCurrentPresetName() const;
    juce::File getUserPresetFolder() const;
    /** Solo para pruebas: usa otra carpeta de presets de usuario. */
    void setUserPresetFolder (const juce::File& folder) { presetFolderOverride = folder; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyState (const juce::ValueTree&, bool keepRigSettings);
    void resetSoundToDefaults();
    void updateOrder();
    juce::File presetFile (const juce::String& name) const;
    juce::File presetFolderOverride;
    std::atomic<juce::uint32> preOrderPacked { 0x76543210u }, postOrderPacked { 0x543210u };
    juce::StringArray readBoard() const;
    void writeBoard (const juce::StringArray&);
    std::atomic<int> boardRevision { 0 };
    void timerCallback() override;
    fx::ChainParams readParams() const;

    fx::GuitarChain chain;
    fx::PitchTracker pitchTracker;
    Recorder recorder;
    juce::File recordingFolderOverride;

    // Metrónomo
    double clickPhase = 0.0, clickAngle = 0.0, clickAngleStep = 0.0;
    float clickEnvelope = 0.0f;
    int clickBeat = 0;
    juce::AudioProcessLoadMeasurer loadMeasurer;
    juce::AudioBuffer<float> work;      // canal 0 = entrada mono, 1 y 2 = salida izquierda y derecha
    int maxBlock = 0;
    std::atomic<int> latencyQuality { -1 };
    std::atomic<bool> latencyChanged { false };
    std::atomic<float> midiWah { 0.0f };
    std::atomic<bool> midiWahPending { false };
    std::atomic<int> midiProgram { -1 };
    juce::File irFile, namFile;
    double namSampleRate = 0.0;

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
                           *outputLevel, *quality, *tunerMute, *metroOn, *metroBpm, *metroLevel;
    } params {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSuiteProcessor)
};
