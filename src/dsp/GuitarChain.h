#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cstring>
#include <vector>
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
    bool  mute = false;         // silencia la salida (afinador), con fundido

    // Orden de los pedales dentro de cada tramo: permutaciones de los índices de PrePedal y PostPedal
    std::array<juce::uint8, 8> preOrder  { 0, 1, 2, 3, 4, 5, 6, 7 };
    std::array<juce::uint8, 6> postOrder { 0, 1, 2, 3, 4, 5 };
};

/** Pedales de antes del ampli, en su orden por defecto. */
enum PrePedal : juce::uint8 { preGate, preWah, preComp, preOct, prePhaser, preOd, preDist, preFuzz, numPrePedals };
/** Pedales de después del ampli, en su orden por defecto. */
enum PostPedal : juce::uint8 { postEq, postFlanger, postChorus, postTrem, postDelay, postReverb, numPostPedals };

/** Fundido de 10 ms para encender y apagar un bloque sin chasquido. */
struct Fade
{
    void prepare (double sampleRate) noexcept { step = (float) (1.0 / (0.010 * sampleRate)); }
    void snap (bool on) noexcept              { gain = target = on ? 1.0f : 0.0f; }

    /** Devuelve true si el bloque acaba de pasar de apagado del todo a encendido: hay que limpiar su estado. */
    bool set (bool on) noexcept
    {
        const bool waking = on && gain <= 0.0f && target <= 0.0f;
        target = on ? 1.0f : 0.0f;
        return waking;
    }

    bool active() const noexcept { return gain > 0.0f || target > 0.0f; }

    inline float next() noexcept
    {
        if (gain < target)      gain = gain + step < target ? gain + step : target;
        else if (gain > target) gain = gain - step > target ? gain - step : target;
        return gain;
    }

    float gain = 0.0f, target = 0.0f, step = 1.0f;
};

/** Comprobación de valor finito que no depende de las optimizaciones de coma flotante del compilador. */
inline bool isFiniteSample (float v) noexcept
{
    juce::uint32 bits;
    std::memcpy (&bits, &v, sizeof (bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

/** Cadena completa, entrada mono y salida estéreo:

    [pedales de antes del ampli] -> amplificador -> pantalla -> [pedales de después del ampli]

    Antes: puerta, wah, compresor, octavador, phaser, overdrive, distorsión y fuzz.
    Después: ecualizador, flanger, chorus, tremolo, delay y reverb.
    Dentro de cada tramo el orden es libre (ChainParams::preOrder y postOrder).

    Todo el tramo de antes del ampli y el amplificador se procesan sobremuestreados para
    que la distorsión no genere aliasing, sea cual sea el orden. El tramo de después es
    estéreo. Nada de lo que hace process() reserva memoria.

    Encender o apagar un bloque hace un fundido de 10 ms entre la señal con y sin él, y
    al encenderlo se limpia su estado. Cambiar de modelo o de calidad baja la salida a
    cero en 4 ms, hace el cambio y la vuelve a subir. Si algo produce un valor no finito,
    la cadena se limpia sola y sigue.
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

        neural.prepare (fs, maxBlockSize);
        neuralIn.reset (fs, 0.02);
        neuralOut.reset (fs, 0.02);
        equalizer.prepare (fs);
        equalizerR.prepare (fs);
        flanger.prepare (fs);
        flangerR.prepare (fs);
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

        for (size_t i = 0; i < cabStages.size(); ++i)
        {
            cabStages[i].coefficients = cabFilters[0][i];   // con los coeficientes puestos, reset() reserva aquí y no en process()
            cabStages[i].prepare (spec);
            cabStages[i].reset();
        }

        scratchA.assign ((size_t) maxBlockSize, 0.0f);
        scratchB.assign ((size_t) maxBlockSize, 0.0f);

        for (auto* f : { &neuralFade, &cabFade, &eqFade, &flangerFade, &chorusFade, &tremFade, &delayFade, &reverbFade, &muteFade })
            f->prepare (fs);

        dipStep = (float) (1.0 / (0.004 * fs));
        activeQuality = -1;
        firstBlock = true;
    }

    /** Latencia que añade el sobremuestreo, en muestras, para la calidad indicada. */
    float getLatencySamples (int quality) const noexcept
    {
        return oversamplers[(size_t) oversamplerIndex (quality)]->getLatencyInSamples();
    }

    /** Índice de sobremuestreo real para una calidad pedida. A frecuencias de muestreo altas hace
        falta menos factor para el mismo resultado, y el coste sería desproporcionado. */
    int oversamplerIndex (int quality) const noexcept
    {
        const int reduction = fs > 150000.0 ? 2 : (fs > 75000.0 ? 1 : 0);
        return juce::jmax (0, juce::jlimit (0, 2, quality) - reduction);
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

        // Ajustes que no se pueden cambiar de forma continua: se aplican con la salida a cero
        const Discrete wanted { oversamplerIndex (p.quality), p.odModel, p.distModel, p.fuzzModel, p.ampModel,
                                juce::jlimit (0, numCabs - 1, p.cabModel), p.wahMode,
                                neural.isLoaded(), userIrLoaded.load (std::memory_order_relaxed),
                                validOrder (p.preOrder), validOrder (p.postOrder) };

        if (firstBlock)
            applied = wanted;
        else if (wanted != applied)
            dipTarget = 0.0f;

        const int quality = applied.quality;

        if (quality != activeQuality)
        {
            activeQuality = quality;
            const double osRate = fs * (double) (2 << quality);
            oversamplers[(size_t) quality]->reset();
            gate.prepare (osRate);
            wah.prepare (osRate);
            compressor.prepare (osRate);
            octaver.prepare (osRate);
            phaser.prepare (osRate);
            overdrive.prepare (osRate);
            distortion.prepare (osRate);
            fuzz.prepare (osRate);
            amp.prepare (osRate);

            for (auto* f : { &gateFade, &wahFade, &compFade, &octFade, &phaserFade, &odFade, &distFade, &fuzzFade, &ampFade })
                f->prepare (osRate);
        }

        overdrive.setVoicing (applied.odModel);
        distortion.setVoicing (applied.distModel);
        fuzz.setVoicing (applied.fuzzModel);
        amp.setVoicing (applied.ampModel);

        wah.setControls (applied.wahMode, p.wahPosition, p.wahQ);
        octaver.setControls (p.octSub, p.octDry);
        phaser.setControls (p.phaserRate, p.phaserDepth, p.phaserFeedback);
        equalizer.setControls (p.eqLow, p.eqLowMid, p.eqHighMid, p.eqHigh);
        equalizerR.setControls (p.eqLow, p.eqLowMid, p.eqHighMid, p.eqHigh);
        flanger.setControls (p.flangerRate, p.flangerDepth, p.flangerFeedback);
        flangerR.setControls (p.flangerRate, p.flangerDepth, p.flangerFeedback);
        tremolo.setControls (p.tremRate, p.tremDepth);
        compressor.setControls (p.compSustain, p.compAttack, p.compLevel);
        overdrive.setControls (p.odDrive, p.odTone, p.odLevel);
        distortion.setControls (p.distGain, p.distTone, p.distLevel);
        fuzz.setControls (p.fuzzAmount, p.fuzzTone, p.fuzzLevel);
        amp.setControls (p.ampGain, p.ampBass, p.ampMid, p.ampTreble, p.ampPresence, p.ampMaster);
        chorus.setControls (p.chorusRate, p.chorusDepth, p.chorusMix);
        delay.setControls (p.delayTimeMs, p.delayFeedback, p.delayMix);
        gate.setThresholdDb (p.gateThresholdDb);

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

        inputGain.setTargetValue (juce::Decibels::decibelsToGain (p.inputGainDb));
        outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));
        neuralIn.setTargetValue (juce::Decibels::decibelsToGain (40.0f * (p.ampGain - 0.5f)));
        neuralOut.setTargetValue (juce::Decibels::decibelsToGain (40.0f * (p.ampMaster - 0.5f)));

        const bool ampModelled = p.ampOn && ! applied.neural;
        const bool ampNeural = p.ampOn && applied.neural;

        if (firstBlock)
        {
            firstBlock = false;
            inputGain.setCurrentAndTargetValue (inputGain.getTargetValue());
            outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
            neuralIn.setCurrentAndTargetValue (neuralIn.getTargetValue());
            neuralOut.setCurrentAndTargetValue (neuralOut.getTargetValue());
            for (size_t i = 0; i < cabStages.size(); ++i)
                cabStages[i].coefficients = cabFilters[(size_t) applied.cabModel][i];

            resetBlocks();
            dipGain = dipTarget = 1.0f;

            gateFade.snap (p.gateOn);       wahFade.snap (p.wahOn);         compFade.snap (p.compOn);
            octFade.snap (p.octOn);         phaserFade.snap (p.phaserOn);   odFade.snap (p.odOn);
            distFade.snap (p.distOn);       fuzzFade.snap (p.fuzzOn);       ampFade.snap (ampModelled);
            neuralFade.snap (ampNeural);    cabFade.snap (p.cabOn);         eqFade.snap (p.eqOn);
            flangerFade.snap (p.flangerOn); chorusFade.snap (p.chorusOn);   tremFade.snap (p.tremOn);
            delayFade.snap (p.delayOn);     reverbFade.snap (p.reverbOn);   muteFade.snap (p.mute);
        }

        // Al encender un bloque que estaba apagado del todo se limpia lo que le quedase dentro
        if (gateFade.set (p.gateOn))        gate.reset();
        if (wahFade.set (p.wahOn))          wah.reset();
        if (compFade.set (p.compOn))        compressor.reset();
        if (octFade.set (p.octOn))          octaver.reset();
        if (phaserFade.set (p.phaserOn))    phaser.reset();
        if (odFade.set (p.odOn))            overdrive.reset();
        if (distFade.set (p.distOn))        distortion.reset();
        if (fuzzFade.set (p.fuzzOn))        fuzz.reset();
        if (ampFade.set (ampModelled))      amp.reset();
        neuralFade.set (ampNeural);
        if (cabFade.set (p.cabOn))          resetCabinet();
        if (eqFade.set (p.eqOn))            { equalizer.reset(); equalizerR.reset(); }
        if (flangerFade.set (p.flangerOn))  { flanger.reset(); flangerR.reset(); }
        if (chorusFade.set (p.chorusOn))    chorus.reset();
        if (tremFade.set (p.tremOn))        tremolo.reset();
        if (delayFade.set (p.delayOn))      delay.reset();
        if (reverbFade.set (p.reverbOn))    reverb.reset();
        muteFade.set (p.mute);

        // --- Entrada
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            data[i] = isFiniteSample (x) ? x * inputGain.getNextValue() : (inputGain.getNextValue(), 0.0f);
        }

        // --- Pedales de antes del ampli y ampli modelado, sobremuestreados y en el orden elegido
        juce::dsp::AudioBlock<float> block (&data, 1, (size_t) numSamples);
        auto& os = *oversamplers[(size_t) quality];
        auto osBlock = os.processSamplesUp (block);
        float* d = osBlock.getChannelPointer (0);
        const int osSamples = (int) osBlock.getNumSamples();

        for (const auto pedal : applied.preOrder)
        {
            switch (pedal)
            {
                case preGate:   if (gateFade.active())   for (int i = 0; i < osSamples; ++i) d[i] += gateFade.next()   * (gate.process (d[i]) - d[i]); break;
                case preWah:    if (wahFade.active())    for (int i = 0; i < osSamples; ++i) d[i] += wahFade.next()    * (wah.process (d[i]) - d[i]); break;
                case preComp:   if (compFade.active())   for (int i = 0; i < osSamples; ++i) d[i] += compFade.next()   * (compressor.process (d[i]) - d[i]); break;
                case preOct:    if (octFade.active())    for (int i = 0; i < osSamples; ++i) d[i] += octFade.next()    * (octaver.process (d[i]) - d[i]); break;
                case prePhaser: if (phaserFade.active()) for (int i = 0; i < osSamples; ++i) d[i] += phaserFade.next() * (phaser.process (d[i]) - d[i]); break;
                case preOd:     if (odFade.active())     for (int i = 0; i < osSamples; ++i) d[i] += odFade.next()     * ((float) overdrive.process (d[i]) - d[i]); break;
                case preDist:   if (distFade.active())   for (int i = 0; i < osSamples; ++i) d[i] += distFade.next()   * ((float) distortion.process (d[i]) - d[i]); break;
                case preFuzz:   if (fuzzFade.active())   for (int i = 0; i < osSamples; ++i) d[i] += fuzzFade.next()   * ((float) fuzz.process (d[i]) - d[i]); break;
                default: break;
            }
        }

        if (ampFade.active())
            for (int i = 0; i < osSamples; ++i)
                d[i] += ampFade.next() * ((float) amp.process (d[i]) - d[i]);

        os.processSamplesDown (block);

        // --- Captura neuronal en lugar del ampli modelado
        if (neuralFade.active())
        {
            float* wet = scratchA.data();
            for (int i = 0; i < numSamples; ++i) wet[i] = data[i] * neuralIn.getNextValue();
            neural.process (wet, numSamples);
            for (int i = 0; i < numSamples; ++i) data[i] += neuralFade.next() * (wet[i] * neuralOut.getNextValue() - data[i]);
        }

        // --- Pantalla
        if (cabFade.active())
        {
            float* wet = scratchA.data();
            std::copy (data, data + numSamples, wet);
            juce::dsp::AudioBlock<float> wetBlock (&wet, 1, (size_t) numSamples);
            juce::dsp::ProcessContextReplacing<float> context (wetBlock);

            if (applied.userIr)
                convolution.process (context);
            else
                for (auto& stage : cabStages)
                    stage.process (context);

            for (int i = 0; i < numSamples; ++i) data[i] += cabFade.next() * (wet[i] - data[i]);
        }

        // --- Pedales de después del ampli: estéreo y en el orden elegido
        for (int i = 0; i < numSamples; ++i)
            left[i] = right[i] = data[i];

        for (const auto pedal : applied.postOrder)
        {
            switch (pedal)
            {
                case postEq:
                    if (eqFade.active())
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float m = eqFade.next();
                            left[i]  += m * (equalizer.process (left[i]) - left[i]);
                            right[i] += m * (equalizerR.process (right[i]) - right[i]);
                        }
                    break;

                case postFlanger:
                    if (flangerFade.active())
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float m = flangerFade.next();
                            left[i]  += m * (flanger.process (left[i]) - left[i]);
                            right[i] += m * (flangerR.process (right[i]) - right[i]);
                        }
                    break;

                case postChorus:
                    if (chorusFade.active())
                        for (int i = 0; i < numSamples; ++i)
                        {
                            // El chorus se alimenta del centro y añade a cada lado su diferencia, sin estrechar el estéreo
                            const float mid = 0.5f * (left[i] + right[i]);
                            float l, r;
                            chorus.process (mid, l, r);
                            const float m = chorusFade.next();
                            left[i]  += m * (l - mid);
                            right[i] += m * (r - mid);
                        }
                    break;

                case postTrem:
                    if (tremFade.active())
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float g = 1.0f + tremFade.next() * (tremolo.nextGain() - 1.0f);
                            left[i] *= g;
                            right[i] *= g;
                        }
                    break;

                case postDelay:
                    if (delayFade.active())
                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float wet = delayFade.next() * delay.process (0.5f * (left[i] + right[i]));
                            left[i] += wet;
                            right[i] += wet;
                        }
                    break;

                case postReverb:
                    if (reverbFade.active())
                    {
                        float* wetL = scratchA.data();
                        float* wetR = scratchB.data();
                        std::copy (left, left + numSamples, wetL);
                        std::copy (right, right + numSamples, wetR);
                        reverb.processStereo (wetL, wetR, numSamples);

                        for (int i = 0; i < numSamples; ++i)
                        {
                            const float m = reverbFade.next();
                            left[i]  += m * (wetL[i] - left[i]);
                            right[i] += m * (wetR[i] - right[i]);
                        }
                    }
                    break;

                default: break;
            }
        }

        // --- Salida: nivel, silencio del afinador, bajada para los cambios de modelo y techo de seguridad
        bool finite = true;

        for (int i = 0; i < numSamples; ++i)
        {
            if (dipGain < dipTarget)      dipGain = juce::jmin (dipTarget, dipGain + dipStep);
            else if (dipGain > dipTarget) dipGain = juce::jmax (dipTarget, dipGain - dipStep);

            const float g = outputGain.getNextValue() * dipGain * (1.0f - muteFade.next());
            left[i] = ceiling (left[i] * g);
            right[i] = ceiling (right[i] * g);
            finite = finite && isFiniteSample (left[i]) && isFiniteSample (right[i]);
        }

        if (dipTarget <= 0.0f && dipGain <= 0.0f)
        {
            // Salida ya en silencio: se hace el cambio pendiente y el siguiente bloque vuelve a subir
            const bool cabChanged = wanted.cabModel != applied.cabModel || wanted.userIr != applied.userIr;
            applied = wanted;
            dipTarget = 1.0f;

            if (cabChanged)
            {
                for (size_t i = 0; i < cabStages.size(); ++i)
                    cabStages[i].coefficients = cabFilters[(size_t) applied.cabModel][i];

                resetCabinet();
            }
        }

        if (! finite)
        {
            // Algo ha producido NaN o infinito: silencio en este bloque y estado limpio para el siguiente
            std::fill (left, left + numSamples, 0.0f);
            std::fill (right, right + numSamples, 0.0f);
            resetBlocks();
            oversamplers[(size_t) quality]->reset();
        }
    }

private:
    struct Discrete
    {
        int quality = 1, odModel = 0, distModel = 0, fuzzModel = 0, ampModel = 0, cabModel = 0, wahMode = 0;
        bool neural = false, userIr = false;
        std::array<juce::uint8, 8> preOrder  { 0, 1, 2, 3, 4, 5, 6, 7 };
        std::array<juce::uint8, 6> postOrder { 0, 1, 2, 3, 4, 5 };
        bool operator== (const Discrete&) const = default;
    };

    /** Devuelve el orden pedido si es una permutación válida; si no, el orden por defecto. */
    template <size_t N>
    static std::array<juce::uint8, N> validOrder (const std::array<juce::uint8, N>& order) noexcept
    {
        unsigned seen = 0;
        for (auto v : order) if (v < N) seen |= 1u << v;

        if (seen == (1u << N) - 1u)
            return order;

        std::array<juce::uint8, N> fallback;
        for (size_t i = 0; i < N; ++i) fallback[i] = (juce::uint8) i;
        return fallback;
    }

    /** Techo de seguridad: lineal hasta -1 dBFS y a partir de ahí se curva sin llegar nunca a 0 dBFS. */
    static inline float ceiling (float x) noexcept
    {
        constexpr float knee = 0.89f, room = 1.0f - knee;
        const float a = std::abs (x);
        if (a <= knee) return x;
        const float y = knee + room * std::tanh ((a - knee) / room);
        return x < 0.0f ? -y : y;
    }

    void resetCabinet() noexcept
    {
        for (auto& stage : cabStages) stage.reset();
        convolution.reset();
    }

    void resetBlocks() noexcept
    {
        gate.reset(); wah.reset(); compressor.reset(); octaver.reset(); phaser.reset();
        overdrive.reset(); distortion.reset(); fuzz.reset(); amp.reset();
        equalizer.reset(); equalizerR.reset(); flanger.reset(); flangerR.reset(); chorus.reset(); tremolo.reset(); delay.reset();
        reverb.reset();
        resetCabinet();
    }

    double fs = 48000.0;
    int maxBlock = 0, activeQuality = -1;
    bool firstBlock = true;
    Discrete applied;
    float dipGain = 1.0f, dipTarget = 1.0f, dipStep = 1.0f;
    Fade gateFade, wahFade, compFade, octFade, phaserFade, odFade, distFade, fuzzFade, ampFade, neuralFade,
         cabFade, eqFade, flangerFade, chorusFade, tremFade, delayFade, reverbFade, muteFade;
    std::vector<float> scratchA, scratchB;

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
    Equalizer equalizer, equalizerR;
    Flanger flanger, flangerR;
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
