#include "NeuralAmp.h"

#include <cmath>
#include <filesystem>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace fx
{
namespace
{
    /** Las capturas indican su sonoridad; se igualan todas a este nivel para que no haya saltos de volumen. */
    constexpr double targetLoudnessDb = -18.0;

    float gainFor (nam::DSP& dsp)
    {
        if (! dsp.HasLoudness())
            return 1.0f;

        const double db = targetLoudnessDb - dsp.GetLoudness();
        return (float) std::pow (10.0, std::fmin (24.0, std::fmax (-24.0, db)) / 20.0);
    }
}

//==============================================================================
void StreamResampler::prepare (double sourceRate, double destRate) noexcept
{
    ratio = sourceRate / destRate;
    filter = destRate < sourceRate;

    if (filter)
    {
        // Paso bajo Butterworth de 2.º orden (aplicado dos veces) al 45 % de la frecuencia de destino
        const double w = 2.0 * 3.14159265358979323846 * 0.45 * destRate / sourceRate;
        const double alpha = std::sin (w) / (2.0 * 0.7071067811865476), c = std::cos (w), a0 = 1.0 + alpha;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
    }

    reset();
}

void StreamResampler::reset() noexcept
{
    position = 0.0;
    for (auto& h : history) h = 0.0f;
    for (auto& stage : z) stage[0] = stage[1] = 0.0;
}

int StreamResampler::process (const float* in, int numIn, float* out, int maxOut) noexcept
{
    int count = 0;

    for (int i = 0; i < numIn; ++i)
    {
        double x = in[i];

        if (filter)
        {
            for (auto& stage : z)
            {
                const double y = b0 * x + stage[0];
                stage[0] = b1 * x - a1 * y + stage[1];
                stage[1] = b2 * x - a2 * y;
                x = y;
            }
        }

        history[0] = history[1]; history[1] = history[2]; history[2] = history[3]; history[3] = (float) x;

        // Con la muestra nueva se puede interpolar entre history[1] y history[2]
        while (position < 1.0 && count < maxOut)
        {
            const float t = (float) position;
            const float a = -0.5f * history[0] + 1.5f * history[1] - 1.5f * history[2] + 0.5f * history[3];
            const float b = history[0] - 2.5f * history[1] + 2.0f * history[2] - 0.5f * history[3];
            const float c = -0.5f * history[0] + 0.5f * history[2];
            out[count++] = ((a * t + b) * t + c) * t + history[1];
            position += ratio;
        }

        position -= 1.0;
    }

    return count;
}

//==============================================================================
NeuralAmp::NeuralAmp() = default;
NeuralAmp::~NeuralAmp() = default;

NeuralAmp::Setup NeuralAmp::setupFor (nam::DSP& dsp, double sampleRate, int maxBlockSize)
{
    Setup setup;
    // Las capturas antiguas no indican su frecuencia: por convenio del formato son de 48 kHz
    const double expected = dsp.GetExpectedSampleRate() > 0.0 ? dsp.GetExpectedSampleRate() : 48000.0;
    setup.resample = std::abs (expected - sampleRate) > 1.0 && ! dsp.SupportsArbitrarySampleRate();
    setup.modelRate = setup.resample ? expected : sampleRate;
    setup.modelBlock = setup.resample ? (int) std::ceil (maxBlockSize * expected / sampleRate) + 16 : maxBlockSize;
    dsp.Reset (setup.modelRate, setup.modelBlock);
    return setup;
}

/** Con el cerrojo tomado. */
void NeuralAmp::applySetup (const Setup& setup)
{
    resampling = setup.resample;
    scratch.assign ((size_t) std::max (maxBlock, setup.modelBlock), 0.0f);
    toModel.assign ((size_t) setup.modelBlock, 0.0f);
    fromModel.assign ((size_t) maxBlock + 64, 0.0f);
    fifo.assign ((size_t) (2 * maxBlock + 256), 0.0f);
    up.prepare (fs, setup.modelRate);
    down.prepare (setup.modelRate, fs);

    // Unas muestras de colchón: cada bloque puede producir una muestra más o menos de las que se piden
    fifoRead = 0;
    fifoCount = resampling ? 8 : 0;
}

void NeuralAmp::prepare (double sampleRate, int maxBlockSize)
{
    lock.enter();
    fs = sampleRate;
    maxBlock = maxBlockSize;
    Setup setup;
    setup.modelRate = fs;
    setup.modelBlock = maxBlock;

    if (model != nullptr)
    {
        try { setup = setupFor (*model, fs, maxBlock); }
        catch (...) { model.reset(); loaded.store (false); }
    }

    applySetup (setup);
    lock.exit();
}

NeuralAmp::LoadResult NeuralAmp::load (const std::string& path)
{
    LoadResult result;
    std::unique_ptr<nam::DSP> fresh;
    double rate = 0.0;
    int block = 0;
    Setup setup;

    lock.enter();
    rate = fs;
    block = maxBlock;
    lock.exit();

    try
    {
        fresh = nam::get_dsp (std::filesystem::u8path (path));

        if (fresh == nullptr)
            throw std::runtime_error ("formato no reconocido");

        if (fresh->NumInputChannels() != 1 || fresh->NumOutputChannels() != 1)
            throw std::runtime_error ("solo se admiten capturas mono");

        // Una frecuencia absurda en el archivo haría reservar búferes enormes al remuestrear
        if (const double declared = fresh->GetExpectedSampleRate(); declared > 0.0 && (declared < 8000.0 || declared > 192000.0))
            throw std::runtime_error ("la captura declara una frecuencia de muestreo no válida");

        result.modelSampleRate = fresh->GetExpectedSampleRate() > 0.0 ? fresh->GetExpectedSampleRate() : 48000.0;
        setup = setupFor (*fresh, rate, block);
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
        return result;
    }
    catch (...)
    {
        result.error = "error desconocido al leer la captura";
        return result;
    }

    const float gain = gainFor (*fresh);

    lock.enter();

    if (rate != fs || block != maxBlock)
    {
        // El audio se ha vuelto a preparar mientras se cargaba: se ajusta a lo actual
        try { setup = setupFor (*fresh, fs, maxBlock); }
        catch (...) { lock.exit(); result.error = "no se pudo preparar la captura"; return result; }
    }

    std::swap (model, fresh);
    applySetup (setup);
    outputGain = gain;
    loaded.store (true);
    lock.exit();

    result.ok = true;
    result.resampled = setup.resample;
    return result;      // el modelo anterior se destruye aquí, fuera del cerrojo
}

void NeuralAmp::clear()
{
    std::unique_ptr<nam::DSP> old;

    lock.enter();
    std::swap (model, old);
    loaded.store (false);
    lock.exit();
}

void NeuralAmp::process (float* data, int numSamples) noexcept
{
    if (! lock.tryEnter())
        return;

    if (model != nullptr && numSamples <= maxBlock)
    {
        if (! resampling)
        {
            float* in[] { data };
            float* out[] { scratch.data() };
            model->process (in, out, numSamples);

            for (int i = 0; i < numSamples; ++i)
                data[i] = scratch[(size_t) i] * outputGain;
        }
        else
        {
            // Audio -> frecuencia de la captura -> red neuronal -> frecuencia del audio
            const int atModelRate = up.process (data, numSamples, toModel.data(), (int) toModel.size());
            float* in[] { toModel.data() };
            float* out[] { scratch.data() };
            model->process (in, out, atModelRate);
            const int produced = down.process (scratch.data(), atModelRate, fromModel.data(), (int) fromModel.size());

            const int capacity = (int) fifo.size();

            for (int i = 0; i < produced && fifoCount < capacity; ++i)
            {
                fifo[(size_t) ((fifoRead + fifoCount) % capacity)] = fromModel[(size_t) i];
                ++fifoCount;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                if (fifoCount > 0)
                {
                    data[i] = fifo[(size_t) fifoRead] * outputGain;
                    fifoRead = (fifoRead + 1) % capacity;
                    --fifoCount;
                }
                else
                {
                    data[i] = 0.0f;
                }
            }

            // Si el colchón crece por redondeos acumulados, se recorta para no añadir retardo
            while (fifoCount > 64)
            {
                fifoRead = (fifoRead + 1) % capacity;
                --fifoCount;
            }
        }
    }

    lock.exit();
}
}
