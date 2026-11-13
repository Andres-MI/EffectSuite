#pragma once
#include <cmath>
#include "OnePole.h"
#include "TimeEffects.h"

namespace fx
{
/** Wah: filtro paso banda resonante. En modo manual lo mueve el control Position
    (automatizable, pensado para un pedal de expresión MIDI); en modo Auto lo mueve
    la fuerza con la que se toca. */
class Wah
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        attack  = 1.0 - std::exp (-1.0 / (0.004 * fs));
        release = 1.0 - std::exp (-1.0 / (0.120 * fs));
        position.prepare (fs, 15.0);
        reset();
    }

    void reset() noexcept { ic1 = ic2 = env = 0.0; position.snap (position.target); }

    /** mode: 0 = manual, 1 = auto. positionPos y q de 0 a 1. */
    void setControls (int mode, double positionPos, double qPos) noexcept
    {
        autoMode = mode == 1;
        control = positionPos;
        k = 1.0 / (2.0 + 8.0 * qPos);
        if (! autoMode) position.target = positionPos;
    }

    inline float process (float x) noexcept
    {
        if (autoMode)
        {
            const double a = std::abs ((double) x);
            env += (a > env ? attack : release) * (a - env);
            const double p = env * (2.0 + 18.0 * control);
            position.target = p > 1.0 ? 1.0 : p;
        }

        const double g = std::tan (pi * 350.0 * std::pow (2200.0 / 350.0, position.next()) / fs);
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return (float) (1.2 * v1 * (0.3 + k));     // paso banda con el pico compensado según la resonancia
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double fs = 48000.0, k = 0.2, ic1 = 0.0, ic2 = 0.0, env = 0.0, attack = 1.0, release = 1.0, control = 0.5;
    bool autoMode = false;
    Smoothed position;
};

/** Octavador analógico: añade una octava por debajo. Monofónico: funciona con notas sueltas. */
class Octaver
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto& f : trackLp) f.setCutoff (280.0, fs);
        for (auto& f : subLp)   f.setCutoff (220.0, fs);
        envRelease = std::exp (-1.0 / (0.050 * fs));
        sub.prepare (fs);
        dry.prepare (fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : trackLp) f.reset();
        for (auto& f : subLp)   f.reset();
        env = 0.0; positive = false; flip = 1.0;
        sub.snap (sub.target);
        dry.snap (dry.target);
    }

    /** Niveles de 0 a 1. */
    void setControls (double subLevel, double dryLevel) noexcept
    {
        sub.target = 4.0 * subLevel;
        dry.target = 2.0 * dryLevel * dryLevel;
    }

    inline float process (float x) noexcept
    {
        const double t = trackLp[1].lp (trackLp[0].lp (x));       // fundamental aislada
        const double a = std::abs (t);
        env = a > env ? a : env * envRelease;

        // Divisor de frecuencia: cambia de signo en cada ciclo completo
        if (! positive && t > 0.3 * env)       { positive = true; flip = -flip; }
        else if (positive && t < -0.3 * env)   { positive = false; }

        const double octave = env > 1e-4 ? subLp[1].lp (subLp[0].lp (t * flip)) : subLp[1].lp (subLp[0].lp (0.0));
        return (float) (dry.next() * x + sub.next() * octave);
    }

private:
    double fs = 48000.0, env = 0.0, envRelease = 0.0, flip = 1.0;
    bool positive = false;
    OnePole trackLp[2], subLp[2];
    Smoothed sub, dry;
};

/** Phaser de cuatro etapas pasa-todo con realimentación. */
class Phaser
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; depth.prepare (fs, 50.0); reset(); }

    void reset() noexcept
    {
        for (int i = 0; i < 4; ++i) x1[i] = y1[i] = 0.0;
        last = 0.0; phase = 0.0;
        depth.snap (depth.target);
    }

    /** Controles de 0 a 1. */
    void setControls (double rate, double depthPos, double feedbackPos) noexcept
    {
        increment = 2.0 * pi * (0.1 * std::pow (80.0, rate)) / fs;      // de 0,1 a 8 Hz
        depth.target = depthPos;
        feedback = 0.75 * feedbackPos;
    }

    inline float process (float x) noexcept
    {
        const double sweep = 0.5 + 0.5 * std::sin (phase);
        phase += increment;
        if (phase > 2.0 * pi) phase -= 2.0 * pi;

        const double hz = 300.0 * std::pow (10.0, sweep * depth.next());     // de 300 Hz a 3 kHz
        const double t = std::tan (pi * hz / fs), a = (t - 1.0) / (t + 1.0);

        double y = x + feedback * last;
        for (int i = 0; i < 4; ++i)
        {
            const double out = a * y + x1[i] - a * y1[i];
            x1[i] = y;
            y1[i] = out;
            y = out;
        }

        last = y;
        return (float) (0.5 * (x + y));
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double fs = 48000.0, phase = 0.0, increment = 0.0, feedback = 0.0, last = 0.0;
    double x1[4] {}, y1[4] {};
    Smoothed depth;
};

/** Flanger: retardo muy corto modulado, con realimentación. */
class Flanger
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        line.prepare ((int) (0.02 * fs));
        depth.prepare (fs, 50.0);
        reset();
    }

    void reset() noexcept { line.reset(); phase = 0.0; last = 0.0f; depth.snap (depth.target); }

    /** Controles de 0 a 1. */
    void setControls (double rate, double depthPos, double feedbackPos) noexcept
    {
        increment = 2.0 * pi * (0.05 * std::pow (60.0, rate)) / fs;     // de 0,05 a 3 Hz
        depth.target = depthPos;
        feedback = (float) (0.85 * feedbackPos);
    }

    inline float process (float x) noexcept
    {
        line.write (x + feedback * last);
        const double sweep = 0.5 + 0.5 * std::sin (phase);
        phase += increment;
        if (phase > 2.0 * pi) phase -= 2.0 * pi;

        last = line.read (fs * (0.001 + 0.005 * sweep * depth.next()));    // de 1 a 6 ms
        return 0.7f * (x + last);
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double fs = 48000.0, phase = 0.0, increment = 0.0;
    float feedback = 0.0f, last = 0.0f;
    DelayLine line;
    Smoothed depth;
};

/** Tremolo: modulación de volumen. */
class Tremolo
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; depth.prepare (fs, 50.0); reset(); }
    void reset() noexcept { phase = 0.0; depth.snap (depth.target); }

    /** Controles de 0 a 1. */
    void setControls (double rate, double depthPos) noexcept
    {
        increment = 2.0 * pi * (1.0 * std::pow (12.0, rate)) / fs;      // de 1 a 12 Hz
        depth.target = depthPos;
    }

    inline float nextGain() noexcept
    {
        const double g = 1.0 - depth.next() * (0.5 - 0.5 * std::sin (phase));
        phase += increment;
        if (phase > 2.0 * pi) phase -= 2.0 * pi;
        return (float) g;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double fs = 48000.0, phase = 0.0, increment = 0.0;
    Smoothed depth;
};

/** Ecualizador de cuatro bandas: graves (100 Hz), medios graves (400 Hz), medios agudos (1,6 kHz) y agudos (5 kHz). */
class Equalizer
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto& b : bands) b.reset();
        update();
    }

    /** Ganancias en dB, de -12 a +12. */
    void setControls (float low, float lowMid, float highMid, float high) noexcept
    {
        if (low == gains[0] && lowMid == gains[1] && highMid == gains[2] && high == gains[3])
            return;

        gains[0] = low; gains[1] = lowMid; gains[2] = highMid; gains[3] = high;
        update();
    }

    inline float process (float x) noexcept
    {
        double y = x;
        for (auto& b : bands) y = b.process (y);
        return (float) y;
    }

private:
    void update() noexcept
    {
        bands[0].setShelf (false, 100.0, gains[0], fs);
        bands[1].set (400.0, 0.8, gains[1], fs);
        bands[2].set (1600.0, 0.8, gains[2], fs);
        bands[3].setShelf (true, 5000.0, gains[3], fs);
    }

    double fs = 48000.0;
    float gains[4] { 0, 0, 0, 0 };
    PeakFilter bands[4];
};
}
