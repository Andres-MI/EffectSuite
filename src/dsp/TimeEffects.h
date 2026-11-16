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
        tapeLp.setCutoff (2800.0, fs);
        tapeHp.setCutoff (140.0, fs);
        time.prepare (fs, 120.0);
        mix.prepare (fs);
        reset();
    }

    void reset() noexcept
    {
        line.reset();
        feedbackLp.reset();
        tapeLp.reset();
        tapeHp.reset();
        wow = flutter = 0.0;
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

    /** true = eco de cinta: el tiempo fluctúa un poco, y cada repetición sale más oscura, con menos graves y algo saturada. */
    void setTape (bool shouldBeTape) noexcept { tape = shouldBeTape; }

    /** Devuelve solo la señal retardada, ya multiplicada por la mezcla. */
    inline float process (float x) noexcept
    {
        if (tape)
        {
            wow += 6.283185307179586 * 0.55 / fs;       if (wow > 6.283185307179586) wow -= 6.283185307179586;
            flutter += 6.283185307179586 * 6.3 / fs;    if (flutter > 6.283185307179586) flutter -= 6.283185307179586;
            const double speed = 1.0 + 0.0022 * std::sin (wow) + 0.0006 * std::sin (flutter);
            const double head = tapeHp.hp (tapeLp.lp (line.read (time.next() * speed)));
            line.write (x + (float) std::tanh (1.1 * feedback * head));
            return (float) (head * mix.next());
        }

        const float wet = line.read (time.next());
        line.write (x + (float) (feedback * feedbackLp.lp (wet)));
        return wet * (float) mix.next();
    }

private:
    double fs = 48000.0, feedback = 0.0;
    DelayLine line;
    OnePole feedbackLp, tapeLp, tapeHp;
    Smoothed time, mix;
    double wow = 0.0, flutter = 0.0;
    bool tape = false;
};
}
