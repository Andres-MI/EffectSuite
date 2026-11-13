#pragma once
#include <cmath>

namespace fx
{
/** Saturación f(x) = tanh(x + bias) - tanh(bias), asimétrica cuando bias != 0.

    Se evalúa con antiderivada (ADAA de primer orden): en lugar de muestrear la
    curva, se promedia entre la muestra anterior y la actual. Eso reduce mucho el
    aliasing de la distorsión a cambio de media muestra (sobremuestreada) de retardo.

    biasTanh debe ser tanh(bias), precalculado por quien llama.
*/
struct Saturator
{
    void reset() noexcept { x1 = 0.0; F1 = 0.0; primed = false; }

    inline double process (double x, double bias, double biasTanh) noexcept
    {
        const double F = logCosh (x + bias) - x * biasTanh;

        if (! primed) { primed = true; x1 = x; F1 = F; }

        const double dx = x - x1;
        const double y = std::abs (dx) > 1e-5 ? (F - F1) / dx
                                              : std::tanh (0.5 * (x + x1) + bias) - biasTanh;
        x1 = x;
        F1 = F;
        return y;
    }

    static inline double logCosh (double v) noexcept
    {
        const double a = std::abs (v);
        return a - 0.6931471805599453 + std::log1p (std::exp (-2.0 * a));
    }

    double x1 = 0.0, F1 = 0.0;
    bool primed = false;
};
}
