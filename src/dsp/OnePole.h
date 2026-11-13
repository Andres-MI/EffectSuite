#pragma once
#include <algorithm>
#include <cmath>

namespace fx
{
/** Filtro de un polo (topología TPT). Estable aunque se module el corte. */
struct OnePole
{
    void setCutoff (double hz, double sampleRate) noexcept
    {
        const double g = std::tan (3.14159265358979323846 * std::min (hz, 0.49 * sampleRate) / sampleRate);
        G = g / (1.0 + g);
    }

    void reset() noexcept { s = 0.0; }

    inline double lp (double x) noexcept
    {
        const double v = (x - s) * G;
        const double y = v + s;
        s = y + v;
        return y;
    }

    inline double hp (double x) noexcept { return x - lp (x); }

    double G = 0.0, s = 0.0;
};

/** Filtro de segundo orden: campana (peaking) o estantería. */
struct PeakFilter
{
    void set (double hz, double q, double gainDb, double sampleRate) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * std::min (hz, 0.45 * sampleRate) / sampleRate;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        const double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = -2.0 * c / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = b1;
        a2 = (1.0 - alpha / A) / a0;
    }

    /** Estantería (shelf) de graves o de agudos. */
    void setShelf (bool high, double hz, double gainDb, double sampleRate) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * std::min (hz, 0.45 * sampleRate) / sampleRate;
        const double c = std::cos (w), sq = 2.0 * std::sqrt (A) * std::sin (w) * 0.7071067811865476;
        const double sign = high ? -1.0 : 1.0;      // la de agudos es la de graves con el coseno invertido
        const double a0 = (A + 1.0) + sign * (A - 1.0) * c + sq;
        b0 = A * ((A + 1.0) - sign * (A - 1.0) * c + sq) / a0;
        b1 = sign * 2.0 * A * ((A - 1.0) - sign * (A + 1.0) * c) / a0;
        b2 = A * ((A + 1.0) - sign * (A - 1.0) * c - sq) / a0;
        a1 = -sign * 2.0 * ((A - 1.0) + sign * (A + 1.0) * c) / a0;
        a2 = ((A + 1.0) + sign * (A - 1.0) * c - sq) / a0;
    }

    void reset() noexcept { s1 = s2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, s1 = 0.0, s2 = 0.0;
};

/** Suavizado exponencial de un parámetro, muestra a muestra. */
struct Smoothed
{
    void prepare (double sampleRate, double timeMs = 20.0) noexcept
    {
        coef = 1.0 - std::exp (-1.0 / (0.001 * timeMs * sampleRate));
    }

    void snap (double v) noexcept { target = current = v; }
    inline double next() noexcept { current += coef * (target - current); return current; }

    double target = 0.0, current = 0.0, coef = 1.0;
};
}
