#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace nam { class DSP; }

namespace fx
{
/** Reproduce capturas de amplificadores reales en formato Neural Amp Modeler (.nam).

    La carga se hace fuera del hilo de audio; el modelo nuevo se intercambia con el
    activo bajo un cerrojo que el hilo de audio solo intenta tomar (nunca espera).
*/
class NeuralAmp
{
public:
    struct LoadResult
    {
        bool ok = false;
        std::string error;
        double modelSampleRate = 0.0;      // 0 si la captura no lo indica
    };

    NeuralAmp();
    ~NeuralAmp();

    /** Llamar antes de procesar y cada vez que cambie la frecuencia o el tamaño de bloque. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Carga una captura. No llamar desde el hilo de audio. */
    LoadResult load (const std::string& path);

    /** Descarga la captura activa. No llamar desde el hilo de audio. */
    void clear();

    bool isLoaded() const noexcept { return loaded.load (std::memory_order_relaxed); }

    /** Hilo de audio. Procesa en el sitio; si no hay captura (o se está cambiando) no toca la señal. */
    void process (float* data, int numSamples) noexcept;

private:
    struct Lock
    {
        bool tryEnter() noexcept { return ! flag.test_and_set (std::memory_order_acquire); }
        void enter() noexcept    { while (flag.test_and_set (std::memory_order_acquire)) {} }
        void exit() noexcept     { flag.clear (std::memory_order_release); }
        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    };

    std::unique_ptr<nam::DSP> model;
    std::vector<float> scratch;
    Lock lock;
    std::atomic<bool> loaded { false };
    float outputGain = 1.0f;
    double fs = 48000.0;
    int maxBlock = 512;
};
}
