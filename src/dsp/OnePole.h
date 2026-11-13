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
