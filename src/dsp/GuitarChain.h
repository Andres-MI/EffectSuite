#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Amp.h"
#include "NoiseGate.h"
#include "Overdrive.h"

namespace fx
{
struct ChainParams
{
    float inputGainDb = 0.0f;
    bool  gateOn = true;
    float gateThresholdDb = -60.0f;
    bool  odOn = false;
    float odDrive = 0.5f, odTone = 0.5f, odLevel = 0.7f;
    bool  ampOn = true;
    int   ampModel = 0;
    float ampGain = 0.5f, ampBass = 0.5f, ampMid = 0.5f, ampTreble = 0.5f, ampPresence = 0.5f, ampMaster = 0.5f;
    bool  cabOn = true;
    float outputDb = 0.0f;
    int   quality = 1;          // 0 = 2x, 1 = 4x, 2 = 8x
};

/** Cadena completa en mono: puerta -> overdrive -> amplificador -> pantalla.

    El overdrive y el amplificador se procesan sobremuestreados para que la
    distorsión no genere aliasing. Nada de lo que hace process() reserva memoria.
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

    /** Procesa numSamples (<= maxBlockSize de prepare) en el sitio. */
    void process (float* data, int numSamples, const ChainParams& p) noexcept
    {
        jassert (numSamples <= maxBlock);
        const int quality = juce::jlimit (0, 2, p.quality);

        if (quality != activeQuality)
        {
            activeQuality = quality;
            const double osRate = fs * (double) (2 << quality);
            oversamplers[(size_t) quality]->reset();
            overdrive.prepare (osRate);
            amp.prepare (osRate);
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
            amp.reset();
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i] * inputGain.getNextValue();
            data[i] = p.gateOn ? gate.process (x) : x;
        }

        juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
        auto& os = *oversamplers[(size_t) quality];
        auto osBlock = os.processSamplesUp (block);
        float* d = osBlock.getChannelPointer (0);
        const int osSamples = (int) osBlock.getNumSamples();

        if (p.odOn && p.ampOn)
            for (int i = 0; i < osSamples; ++i) d[i] = (float) amp.process (overdrive.process (d[i]));
        else if (p.odOn)
            for (int i = 0; i < osSamples; ++i) d[i] = (float) overdrive.process (d[i]);
        else if (p.ampOn)
            for (int i = 0; i < osSamples; ++i) d[i] = (float) amp.process (d[i]);

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

        for (int i = 0; i < numSamples; ++i)
            data[i] *= outputGain.getNextValue();
    }

private:
    double fs = 48000.0;
    int maxBlock = 0, activeQuality = -1;
    bool firstBlock = true;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    NoiseGate gate;
    Overdrive overdrive;
    Amp amp;
    juce::dsp::Convolution convolution;
    std::array<juce::dsp::IIR::Filter<float>, 6> cabFilters;
    std::atomic<bool> userIrLoaded { false };
    juce::SmoothedValue<float> inputGain { 1.0f }, outputGain { 1.0f };
};
}
