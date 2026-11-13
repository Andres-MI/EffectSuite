#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Amp.h"
#include "Compressor.h"
#include "Distortion.h"
#include "Fuzz.h"
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
    bool  compOn = false;
    float compSustain = 0.5f, compAttack = 0.3f, compLevel = 0.5f;
    bool  odOn = false;
    float odDrive = 0.5f, odTone = 0.5f, odLevel = 0.7f;
    bool  distOn = false;
    float distGain = 0.5f, distTone = 0.5f, distLevel = 0.5f;
    bool  fuzzOn = false;
    float fuzzAmount = 0.6f, fuzzTone = 0.5f, fuzzLevel = 0.5f;
    bool  ampOn = true;
    int   ampModel = 0;
    float ampGain = 0.5f, ampBass = 0.5f, ampMid = 0.5f, ampTreble = 0.5f, ampPresence = 0.5f, ampMaster = 0.5f;
    bool  cabOn = true;
    bool  chorusOn = false;
    float chorusRate = 0.3f, chorusDepth = 0.5f, chorusMix = 0.6f;
    bool  delayOn = false;
    float delayTimeMs = 380.0f, delayFeedback = 0.35f, delayMix = 0.3f;
    bool  reverbOn = false;
    float reverbSize = 0.5f, reverbMix = 0.25f;
    float outputDb = 0.0f;
    int   quality = 1;          // 0 = 2x, 1 = 4x, 2 = 8x
};

/** Cadena completa, entrada mono y salida estéreo:

    puerta -> compresor -> overdrive -> distorsión -> fuzz -> amplificador -> pantalla
           -> chorus -> delay -> reverb

    Los pedales de saturación y el amplificador se procesan sobremuestreados para
    que la distorsión no genere aliasing. Nada de lo que hace process() reserva memoria.
*/
class GuitarChain
{
public:
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
        cabFilters[0].coefficients = Coefs::makeHighPass (fs, 80.0f, 0.8f);
        cabFilters[1].coefficients = Coefs::makePeakFilter (fs, 110.0f, 1.0f, juce::Decibels::decibelsToGain (3.0f));
        cabFilters[2].coefficients = Coefs::makePeakFilter (fs, 450.0f, 1.0f, juce::Decibels::decibelsToGain (-3.0f));
        cabFilters[3].coefficients = Coefs::makePeakFilter (fs, 2600.0f, 1.5f, juce::Decibels::decibelsToGain (5.0f));
        cabFilters[4].coefficients = Coefs::makeLowPass (fs, juce::jmin (5200.0f, 0.45f * (float) fs), 0.9f);
        cabFilters[5].coefficients = Coefs::makeLowPass (fs, juce::jmin (6500.0f, 0.45f * (float) fs), 0.7f);

        for (auto& f : cabFilters)
        {
            f.prepare (spec);
            f.reset();
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
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float x = data[i] * inputGain.getNextValue();
            if (p.gateOn) x = gate.process (x);
            if (p.compOn) x = compressor.process (x);
            data[i] = x;
        }

        juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
        auto& os = *oversamplers[(size_t) quality];
        auto osBlock = os.processSamplesUp (block);
        float* d = osBlock.getChannelPointer (0);
        const int osSamples = (int) osBlock.getNumSamples();

        if (p.odOn || p.distOn || p.fuzzOn || p.ampOn)
        {
            for (int i = 0; i < osSamples; ++i)
            {
                double x = d[i];
                if (p.odOn)   x = overdrive.process (x);
                if (p.distOn) x = distortion.process (x);
                if (p.fuzzOn) x = fuzz.process (x);
                if (p.ampOn)  x = amp.process (x);
                d[i] = (float) x;
            }
        }

        os.processSamplesDown (block);

        if (p.cabOn)
        {
            juce::dsp::ProcessContextReplacing<float> context (block);

            if (userIrLoaded.load (std::memory_order_relaxed))
                convolution.process (context);
            else
                for (auto& f : cabFilters)
                    f.process (context);
        }

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
    Chorus chorus;
    Delay delay;
    juce::Reverb reverb;
    float reverbSize = -1.0f, reverbMix = -1.0f;
    juce::dsp::Convolution convolution;
    std::array<juce::dsp::IIR::Filter<float>, 6> cabFilters;
    std::atomic<bool> userIrLoaded { false };
    juce::SmoothedValue<float> inputGain { 1.0f }, outputGain { 1.0f };
};
}
