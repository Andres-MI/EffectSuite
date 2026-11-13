#pragma once
#include <atomic>
#include <cmath>
#include <vector>

namespace fx
{
/** Detector de tono para el afinador (algoritmo YIN).

    El hilo de audio escribe en un búfer circular; la interfaz llama a detect()
    desde su propio hilo. Una lectura puede coincidir con una escritura, lo que
    como mucho estropea una medida aislada del afinador.
*/
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decimation = std::max (1, (int) std::lround (sampleRate / 48000.0));
        fs = sampleRate / decimation;
        ring.assign (ringSize, 0.0f);
        work.assign (window, 0.0f);
        diff.assign (window / 2, 0.0f);
        writeIndex.store (0);
        acc = 0.0f;
        accCount = 0;
    }

    /** Hilo de audio. */
    void push (const float* data, int numSamples) noexcept
    {
        if (ring.empty())
            return;

        int w = writeIndex.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            acc += data[i];

            if (++accCount == decimation)
            {
                ring[(size_t) w] = acc / (float) decimation;
                w = (w + 1) & (ringSize - 1);
                acc = 0.0f;
                accCount = 0;
            }
        }

        writeIndex.store (w, std::memory_order_release);
    }

    /** Hilo de la interfaz. Devuelve la frecuencia en Hz, o 0 si no hay nota clara. */
    float detect() noexcept
    {
        if (ring.empty())       // todavía no se ha llamado a prepare()
            return 0.0f;

        const int w = writeIndex.load (std::memory_order_acquire);
        double energy = 0.0;

        for (int i = 0; i < window; ++i)
        {
            work[(size_t) i] = ring[(size_t) ((w - window + i) & (ringSize - 1))];
            energy += work[(size_t) i] * work[(size_t) i];
        }

        if (energy / window < 1e-6)     // por debajo de -60 dB RMS
            return 0.0f;

        const int half = window / 2;
        const int tauMin = (int) (fs / 1400.0);
        diff[0] = 1.0f;
        double running = 0.0;

        for (int tau = 1; tau < half; ++tau)
        {
            double sum = 0.0;

            for (int j = 0; j < half; ++j)
            {
                const double d = work[(size_t) j] - work[(size_t) (j + tau)];
                sum += d * d;
            }

            running += sum;
            diff[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
        }

        int best = -1;

        for (int tau = std::max (2, tauMin); tau < half - 1; ++tau)
        {
            if (diff[(size_t) tau] < 0.12f)
            {
                while (tau + 1 < half - 1 && diff[(size_t) (tau + 1)] < diff[(size_t) tau])
                    ++tau;

                best = tau;
                break;
            }
        }

        if (best < 0)
            return 0.0f;

        const double y0 = diff[(size_t) (best - 1)], y1 = diff[(size_t) best], y2 = diff[(size_t) (best + 1)];
        const double denom = y0 - 2.0 * y1 + y2;
        const double offset = std::abs (denom) > 1e-12 ? 0.5 * (y0 - y2) / denom : 0.0;
        return (float) (fs / (best + offset));
    }

private:
    static constexpr int ringSize = 8192, window = 4096;

    double fs = 48000.0;
    int decimation = 1, accCount = 0;
    float acc = 0.0f;
    std::vector<float> ring, work, diff;
    std::atomic<int> writeIndex { 0 };
};
}
