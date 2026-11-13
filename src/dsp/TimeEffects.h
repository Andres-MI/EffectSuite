#pragma once
#include <cmath>
#include <vector>
#include "OnePole.h"

namespace fx
{
/** Línea de retardo con lectura interpolada. La memoria se reserva en prepare(). */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 16;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Lee la muestra escrita hace delaySamples (>= 1) muestras. */
    inline float read (double delaySamples) const noexcept
    {
        const int whole = (int) delaySamples;
        const float frac = (float) (delaySamples - whole);
        const float a = buffer[(size_t) ((writePos - whole) & mask)];
        const float b = buffer[(size_t) ((writePos - whole - 1) & mask)];
        return a + frac * (b - a);
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

/** Chorus estéreo: una línea de retardo modulada, leída con el LFO en cuadratura para cada lado. */
class Chorus
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        line.prepare ((int) (0.05 * fs));
        mix.prepare (fs);
        depth.prepare (fs, 50.0);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        phase = 0.0;
        mix.snap (mix.target);
        depth.snap (depth.target);
    }

    /** Controles de 0 a 1. */
    void setControls (double rate, double depthPos, double mixPos) noexcept
    {
        increment = 2.0 * pi * (0.1 * std::pow (50.0, rate)) / fs;      // de 0,1 a 5 Hz
        depth.target = 0.004 * fs * depthPos;                           // hasta +-4 ms
        mix.target = 0.5 * mixPos;
    }

    inline void process (float x, float& left, float& right) noexcept
    {
        line.write (x);
        const double d = depth.next(), base = 0.012 * fs;
        const float wetL = line.read (base + d * std::sin (phase));
        const float wetR = line.read (base + d * std::cos (phase));
        phase += increment;
        if (phase > 2.0 * pi) phase -= 2.0 * pi;

        const float m = (float) mix.next();
        left  = x + m * (wetL - x);
        right = x + m * (wetR - x);
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double fs = 48000.0, phase = 0.0, increment = 0.0;
    DelayLine line;
    Smoothed mix, depth;
};

/** Delay con realimentación filtrada, al estilo de un delay analógico. */
class Delay
{
public:
    static constexpr double maxSeconds = 1.6;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        line.prepare ((int) (maxSeconds * fs));
        feedbackLp.setCutoff (4000.0, fs);
        time.prepare (fs, 120.0);
        mix.prepare (fs);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        feedbackLp.reset();
        time.snap (time.target);
        mix.snap (mix.target);
    }

    /** timeMs en milisegundos; feedback y mix de 0 a 1. */
    void setControls (double timeMs, double feedbackPos, double mixPos) noexcept
    {
        const double ms = timeMs < 10.0 ? 10.0 : (timeMs > 1500.0 ? 1500.0 : timeMs);
        time.target = 0.001 * ms * fs;
        feedback = 0.9 * feedbackPos;
        mix.target = mixPos;
    }

    /** Devuelve solo la señal retardada, ya multiplicada por la mezcla. */
    inline float process (float x) noexcept
    {
        const float wet = line.read (time.next());
        line.write (x + (float) (feedback * feedbackLp.lp (wet)));
        return wet * (float) mix.next();
    }

private:
    double fs = 48000.0, feedback = 0.0;
    DelayLine line;
    OnePole feedbackLp;
    Smoothed time, mix;
};
}
