#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Amp.h"
#include "Compressor.h"
#include "Distortion.h"
#include "Fuzz.h"
#include "Modulation.h"
#include "NeuralAmp.h"
#include "NoiseGate.h"
#include "Overdrive.h"
#include "TimeEffects.h"

namespace fx
{
struct ChainParams
{
    float inputGainDb = 0.0f;
    bool  gateOn = true;
    float gateThresholdDb = -60.0f;
    bool  wahOn = false;
    int   wahMode = 0;          // 0 = manual, 1 = auto
    float wahPosition = 0.5f, wahQ = 0.5f;
    bool  compOn = false;
    float compSustain = 0.5f, compAttack = 0.3f, compLevel = 0.5f;
    bool  octOn = false;
    float octSub = 0.6f, octDry = 0.7f;
    bool  phaserOn = false;
    float phaserRate = 0.3f, phaserDepth = 0.7f, phaserFeedback = 0.4f;
    bool  odOn = false;
    int   odModel = 0;
    float odDrive = 0.5f, odTone = 0.5f, odLevel = 0.7f;
    bool  distOn = false;
    int   distModel = 0;
    float distGain = 0.5f, distTone = 0.5f, distLevel = 0.5f;
    bool  fuzzOn = false;
    int   fuzzModel = 0;
    float fuzzAmount = 0.6f, fuzzTone = 0.5f, fuzzLevel = 0.5f;
    bool  ampOn = true;
    int   ampModel = 0;
    float ampGain = 0.5f, ampBass = 0.5f, ampMid = 0.5f, ampTreble = 0.5f, ampPresence = 0.5f, ampMaster = 0.5f;
    bool  cabOn = true;
    int   cabModel = 0;
    bool  eqOn = false;
    float eqLow = 0.0f, eqLowMid = 0.0f, eqHighMid = 0.0f, eqHigh = 0.0f;      // dB
    bool  flangerOn = false;
    float flangerRate = 0.3f, flangerDepth = 0.6f, flangerFeedback = 0.5f;
    bool  chorusOn = false;
    float chorusRate = 0.3f, chorusDepth = 0.5f, chorusMix = 0.6f;
    bool  tremOn = false;
    float tremRate = 0.5f, tremDepth = 0.6f;
    bool  delayOn = false;
    float delayTimeMs = 380.0f, delayFeedback = 0.35f, delayMix = 0.3f;
    bool  reverbOn = false;
    float reverbSize = 0.5f, reverbMix = 0.25f;
    float outputDb = 0.0f;
    int   quality = 1;          // 0 = 2x, 1 = 4x, 2 = 8x
};

/** Cadena completa, entrada mono y salida estéreo:

    puerta -> wah -> compresor -> octavador -> phaser -> overdrive -> distorsión -> fuzz
           -> amplificador -> pantalla -> ecualizador -> flanger -> chorus -> tremolo -> delay -> reverb

    Los pedales de saturación y el amplificador se procesan sobremuestreados para
    que la distorsión no genere aliasing. Nada de lo que hace process() reserva memoria.
*/
class GuitarChain
{
public:
    static constexpr int numCabs = 5;
    static constexpr const char* cabNames[numCabs] { "Guitar 4x12", "Guitar 2x12 Open", "Guitar 1x12", "Bass 8x10", "Bass 1x15" };

    GuitarChain()
    {
        using OS = juce::dsp::Oversampling<float>;

        for (size_t i = 0; i < oversamplers.size(); ++i)
            oversamplers[i] = std::make_unique<OS> ((size_t) 1, i + 1, OS::filterHalfBandPolyphaseIIR, true, false);
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        maxBlock = maxBlockSize;

        for (auto& os : oversamplers)
        {
            os->initProcessing ((size_t) maxBlockSize);
            os->reset();
        }

        gate.prepare (fs);
        compressor.prepare (fs);
        neural.prepare (fs, maxBlockSize);
        neuralIn.reset (fs, 0.02);
        neuralOut.reset (fs, 0.02);
        wah.prepare (fs);
        octaver.prepare (fs);
        phaser.prepare (fs);
        equalizer.prepare (fs);
        flanger.prepare (fs);
        tremolo.prepare (fs);
        chorus.prepare (fs);
        delay.prepare (fs);
        reverb.setSampleRate (fs);
        reverb.reset();
        reverbSize = reverbMix = -1.0f;
        inputGain.reset (fs, 0.02);
        outputGain.reset (fs, 0.02);

        const juce::dsp::ProcessSpec spec { fs, (juce::uint32) maxBlockSize, 1 };
        convolution.prepare (spec);

        using Coefs = juce::dsp::IIR::Coefficients<float>;
        const float f = (float) fs, top = 0.45f * f;
        auto peak = [f] (float hz, float q, float db) { return Coefs::makePeakFilter (f, hz, q, juce::Decibels::decibelsToGain (db)); };
        auto low = [f, top] (float hz, float q) { return Coefs::makeLowPass (f, juce::jmin (hz, top), q); };
        auto high = [f] (float hz, float q) { return Coefs::makeHighPass (f, hz, q); };

        // Pantallas integradas: paso alto, tres resonancias y caída de agudos del altavoz
        cabFilters[0] = { high (80.0f, 0.8f), peak (110.0f, 1.0f, 3.0f), peak (450.0f, 1.0f, -3.0f), peak (2600.0f, 1.5f, 5.0f), low (5200.0f, 0.9f), low (6500.0f, 0.7f) };   // Guitar 4x12
        cabFilters[1] = { high (95.0f, 0.7f), peak (140.0f, 1.2f, 2.0f), peak (800.0f, 1.0f, -2.0f), peak (3200.0f, 1.4f, 4.0f), low (6000.0f, 0.8f), low (7500.0f, 0.7f) };   // Guitar 2x12 Open
        cabFilters[2] = { high (110.0f, 0.7f), peak (170.0f, 1.2f, 2.5f), peak (1200.0f, 0.8f, 2.0f), peak (3500.0f, 1.6f, 3.0f), low (5500.0f, 0.8f), low (7000.0f, 0.7f) };  // Guitar 1x12
        cabFilters[3] = { high (42.0f, 0.8f), peak (70.0f, 1.0f, 2.5f), peak (500.0f, 0.8f, -2.5f), peak (2200.0f, 1.2f, 3.0f), low (4000.0f, 0.8f), low (5000.0f, 0.7f) };     // Bass 8x10
        cabFilters[4] = { high (35.0f, 0.8f), peak (85.0f, 1.0f, 3.5f), peak (350.0f, 0.8f, -2.0f), peak (1500.0f, 1.0f, 1.5f), low (2800.0f, 0.8f), low (3600.0f, 0.7f) };     // Bass 1x15

        for (auto& stage : cabStages)
        {
            stage.prepare (spec);
            stage.reset();
        }

        activeQuality = -1;
        firstBlock = true;
    }

    /** Latencia que añade el sobremuestreo, en muestras, para la calidad indicada. */
    float getLatencySamples (int quality) const noexcept
    {
        return oversamplers[(size_t) juce::jlimit (0, 2, quality)]->getLatencyInSamples();
    }

    /** Carga una respuesta a impulso de pantalla. Llamar desde el hilo de mensajes. */
    void loadImpulseResponse (const juce::File& file)
    {
        using C = juce::dsp::Convolution;
        convolution.loadImpulseResponse (file, C::Stereo::no, C::Trim::yes, 0, C::Normalise::yes);
        userIrLoaded.store (true);
    }

    void clearImpulseResponse() noexcept    { userIrLoaded.store (false); }
    bool hasImpulseResponse() const noexcept { return userIrLoaded.load(); }

    /** Carga una captura Neural Amp Modeler, que sustituye al amplificador modelado mientras esté cargada.
        Con una captura activa, Gain actúa como nivel de entrada a la captura (+-20 dB) y Master como
        nivel de salida (+-20 dB); el control de tonos no interviene. Llamar desde el hilo de mensajes. */
    NeuralAmp::LoadResult loadNeuralModel (const std::string& path) { return neural.load (path); }
    void clearNeuralModel()                                         { neural.clear(); }
    bool hasNeuralModel() const noexcept                            { return neural.isLoaded(); }

    /** Procesa numSamples (<= maxBlockSize de prepare).

        data es la entrada mono y se usa como búfer de trabajo; la salida queda en left y right.
    */
    void process (float* data, float* left, float* right, int numSamples, const ChainParams& p) noexcept
    {
        jassert (numSamples <= maxBlock);
        const int quality = juce::jlimit (0, 2, p.quality);

        if (quality != activeQuality)
        {
            activeQuality = quality;
            const double osRate = fs * (double) (2 << quality);
            oversamplers[(size_t) quality]->reset();
            overdrive.prepare (osRate);
            distortion.prepare (osRate);
            fuzz.prepare (osRate);
            amp.prepare (osRate);
        }

        overdrive.setVoicing (p.odModel);
        distortion.setVoicing (p.distModel);
        fuzz.setVoicing (p.fuzzModel);
        wah.setControls (p.wahMode, p.wahPosition, p.wahQ);
        octaver.setControls (p.octSub, p.octDry);
        phaser.setControls (p.phaserRate, p.phaserDepth, p.phaserFeedback);
        equalizer.setControls (p.eqLow, p.eqLowMid, p.eqHighMid, p.eqHigh);
        flanger.setControls (p.flangerRate, p.flangerDepth, p.flangerFeedback);
        tremolo.setControls (p.tremRate, p.tremDepth);
        compressor.setControls (p.compSustain, p.compAttack, p.compLevel);
        distortion.setControls (p.distGain, p.distTone, p.distLevel);
        fuzz.setControls (p.fuzzAmount, p.fuzzTone, p.fuzzLevel);
        chorus.setControls (p.chorusRate, p.chorusDepth, p.chorusMix);
        delay.setControls (p.delayTimeMs, p.delayFeedback, p.delayMix);

        if (p.reverbSize != reverbSize || p.reverbMix != reverbMix)
        {
            reverbSize = p.reverbSize;
            reverbMix = p.reverbMix;
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.3f + 0.68f * reverbSize;
            rp.damping = 0.5f;
            rp.wetLevel = 0.33f * reverbMix;
            rp.dryLevel = 0.5f;          // ganancia unidad en juce::Reverb
            rp.width = 1.0f;
            reverb.setParameters (rp);
        }

        overdrive.setControls (p.odDrive, p.odTone, p.odLevel);
        amp.setVoicing (p.ampModel);
        amp.setControls (p.ampGain, p.ampBass, p.ampMid, p.ampTreble, p.ampPresence, p.ampMaster);
        gate.setThresholdDb (p.gateThresholdDb);
        inputGain.setTargetValue (juce::Decibels::decibelsToGain (p.inputGainDb));
        outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));

        if (firstBlock)
        {
            firstBlock = false;
            inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
            outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
            overdrive.reset();
            distortion.reset();
            fuzz.reset();
            amp.reset();
            chorus.reset();
            delay.reset();
            wah.reset();
            octaver.reset();
            phaser.reset();
            flanger.reset();
            tremolo.reset();
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i] * inputGain.getNextValue();
            if (p.gateOn)   x = gate.process (x);
            if (p.wahOn)    x = wah.process (x);
            if (p.compOn)   x = compressor.process (x);
            if (p.octOn)    x = octaver.process (x);
            if (p.phaserOn) x = phaser.process (x);
            data[i] = x;
        }

        juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
        auto& os = *oversamplers[(size_t) quality];
        auto osBlock = os.processSamplesUp (block);
        float* d = osBlock.getChannelPointer (0);
        const int osSamples = (int) osBlock.getNumSamples();

        const bool useNeural = p.ampOn && neural.isLoaded();
        const bool useAmp = p.ampOn && ! useNeural;

        if (p.odOn || p.distOn || p.fuzzOn || useAmp)
        {
            for (int i = 0; i < osSamples; ++i)
            {
                double x = d[i];
                if (p.odOn)   x = overdrive.process (x);
                if (p.distOn) x = distortion.process (x);
                if (p.fuzzOn) x = fuzz.process (x);
                if (useAmp)   x = amp.process (x);
                d[i] = (float) x;
            }
        }

        os.processSamplesDown (block);

        if (useNeural)
        {
            neuralIn.setTargetValue (juce::Decibels::decibelsToGain (40.0f * (p.ampGain - 0.5f)));
            neuralOut.setTargetValue (juce::Decibels::decibelsToGain (40.0f * (p.ampMaster - 0.5f)));

            for (int i = 0; i < numSamples; ++i) data[i] *= neuralIn.getNextValue();
            neural.process (data, numSamples);
            for (int i = 0; i < numSamples; ++i) data[i] *= neuralOut.getNextValue();
        }

        if (p.cabOn)
        {
            juce::dsp::ProcessContextReplacing<float> context (block);

            if (userIrLoaded.load (std::memory_order_relaxed))
                convolution.process (context);
            else
            {
                // Los filtros comparten estado entre pantallas: al cambiar de modelo solo cambian los coeficientes
                const auto& model = cabFilters[(size_t) juce::jlimit (0, numCabs - 1, p.cabModel)];

                for (size_t i = 0; i < cabStages.size(); ++i)
                {
                    cabStages[i].coefficients = model[i];
                    cabStages[i].process (context);
                }
            }
        }

        if (p.eqOn)
            for (int i = 0; i < numSamples; ++i)
                data[i] = equalizer.process (data[i]);

        if (p.flangerOn)
            for (int i = 0; i < numSamples; ++i)
                data[i] = flanger.process (data[i]);

        if (p.chorusOn)
        {
            for (int i = 0; i < numSamples; ++i)
                chorus.process (data[i], left[i], right[i]);
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
                left[i] = right[i] = data[i];
        }

        if (p.tremOn)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float g = tremolo.nextGain();
                left[i] *= g;
                right[i] *= g;
            }
        }

        if (p.delayOn)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float wet = delay.process (0.5f * (left[i] + right[i]));
                left[i] += wet;
                right[i] += wet;
            }
        }

        if (p.reverbOn)
            reverb.processStereo (left, right, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outputGain.getNextValue();
            left[i] *= g;
            right[i] *= g;
        }
    }

private:
    double fs = 48000.0;
    int maxBlock = 0, activeQuality = -1;
    bool firstBlock = true;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    NoiseGate gate;
    Compressor compressor;
    Overdrive overdrive;
    Distortion distortion;
    Fuzz fuzz;
    Amp amp;
    NeuralAmp neural;
    juce::SmoothedValue<float> neuralIn { 1.0f }, neuralOut { 1.0f };
    Wah wah;
    Octaver octaver;
    Phaser phaser;
    Equalizer equalizer;
    Flanger flanger;
    Tremolo tremolo;
    Chorus chorus;
    Delay delay;
    juce::Reverb reverb;
    float reverbSize = -1.0f, reverbMix = -1.0f;
    juce::dsp::Convolution convolution;
    std::array<std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 6>, numCabs> cabFilters;
    std::array<juce::dsp::IIR::Filter<float>, 6> cabStages;
    std::atomic<bool> userIrLoaded { false };
    juce::SmoothedValue<float> inputGain { 1.0f }, outputGain { 1.0f };
};
}
