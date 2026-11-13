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

NeuralAmp::NeuralAmp() = default;
NeuralAmp::~NeuralAmp() = default;

void NeuralAmp::prepare (double sampleRate, int maxBlockSize)
{
    lock.enter();
    fs = sampleRate;
    maxBlock = maxBlockSize;
    scratch.assign ((size_t) maxBlock, 0.0f);

    if (model != nullptr)
    {
        try { model->Reset (fs, maxBlock); }
        catch (...) { model.reset(); loaded.store (false); }
    }

    lock.exit();
}

NeuralAmp::LoadResult NeuralAmp::load (const std::string& path)
{
    LoadResult result;
    std::unique_ptr<nam::DSP> fresh;
    double rate = 0.0;
    int block = 0;

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

        result.modelSampleRate = fresh->GetExpectedSampleRate() > 0.0 ? fresh->GetExpectedSampleRate() : 0.0;
        fresh->Reset (rate, block);
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
    std::swap (model, fresh);
    outputGain = gain;
    loaded.store (true);
    lock.exit();

    result.ok = true;
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

    if (model != nullptr && numSamples <= (int) scratch.size())
    {
        float* in[] { data };
        float* out[] { scratch.data() };
        model->process (in, out, numSamples);

        for (int i = 0; i < numSamples; ++i)
            data[i] = scratch[(size_t) i] * outputGain;
    }

    lock.exit();
}
}
