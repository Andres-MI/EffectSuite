#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace nam { class DSP; }

namespace fx
{
/** Cambio de frecuencia de muestreo en continuo (interpolación cúbica), con un paso bajo
    previo cuando se baja de frecuencia. Admite bloques de cualquier tamaño. */
class StreamResampler
{
public:
    /** ratio = muestras de entrada por cada muestra de salida. */
    void prepare (double sourceRate, double destRate) noexcept;
    void reset() noexcept;
    /** Consume numIn muestras y escribe las que correspondan en out (como mucho maxOut). Devuelve cuántas. */
    int process (const float* in, int numIn, float* out, int maxOut) noexcept;

private:
    double ratio = 1.0, position = 0.0;
    float history[4] {};
    bool filter = false;
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z[2][2] {};
};

/** Reproduce capturas de amplificadores reales en formato Neural Amp Modeler (.nam).

    La carga se hace fuera del hilo de audio; el modelo nuevo se intercambia con el
    activo bajo un cerrojo que el hilo de audio solo intenta tomar (nunca espera).

    Cada captura está hecha a una frecuencia de muestreo (normalmente 48 kHz). Si el audio
    va a otra, la señal se convierte a la de la captura, se procesa y se devuelve a la del
    audio, para que suene igual. Esa conversión añade unas 10 muestras de retardo.
*/
class NeuralAmp
{
public:
    struct LoadResult
    {
        bool ok = false;
        std::string error;
        double modelSampleRate = 0.0;      // 48000 si la captura no lo indica
        bool resampled = false;            // true si se remuestrea porque el audio va a otra frecuencia
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

    /** Prepara el modelo y los búferes para la frecuencia y bloque dados. Devuelve si hay que remuestrear. */
    struct Setup
    {
        bool resample = false;
        double modelRate = 0.0;
        int modelBlock = 0;
    };
    static Setup setupFor (nam::DSP&, double sampleRate, int maxBlockSize);
    void applySetup (const Setup&);

    std::unique_ptr<nam::DSP> model;
    std::vector<float> scratch, toModel, fromModel, fifo;
    StreamResampler up, down;
    bool resampling = false;
    int fifoRead = 0, fifoCount = 0;
    Lock lock;
    std::atomic<bool> loaded { false };
    float outputGain = 1.0f;
    double fs = 48000.0;
    int maxBlock = 512;
};
}
