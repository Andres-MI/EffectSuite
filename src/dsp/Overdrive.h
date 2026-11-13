#pragma once
#include "OnePole.h"

namespace fx
{
/** Overdrive de diodos en el lazo de realimentación (estilo "pantalla verde").

    Etapa de recorte modelada desde el circuito: un amplificador operacional no
    inversor cuya realimentación es Rf en paralelo con un condensador y dos diodos
    en antiparalelo. La ecuación no lineal del nudo se integra con la regla
    trapezoidal y se resuelve por Newton-Raphson en cada muestra.

    1.0 de señal digital equivale a 1 V.
*/
class Overdrive
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        k = 1.0 / (2.0 * fs * Cf);
        inputHp.setCutoff (1.0 / (2.0 * pi * Rin * Cin), fs);   // ~720 Hz: los graves no se recortan
        level.prepare (fs);
        updateTone();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset();
        toneLp.reset();
        v = fPrev = 0.0;
        level.snap (level.target);
    }

    /** Controles de 0 a 1. */
    void setControls (double drive, double tone, double levelPos) noexcept
    {
        Rf = 51e3 + 500e3 * drive * drive;      // potenciómetro de 500k, ley aproximadamente logarítmica
        level.target = 2.0 * levelPos * levelPos;

        if (tone != tonePos)
        {
            tonePos = tone;
            updateTone();
        }
    }

    inline double process (double x) noexcept
    {
        const double i = inputHp.hp (x) / Rin;       // corriente que entra en la red de realimentación

        // C dV/dt = i - V/Rf - 2 Is sinh(V / nVt)
        const double base = v + k * fPrev;
        double vn = v;

        for (int iter = 0; iter < 12; ++iter)
        {
            const double arg = vn * invNVt;
            const double e = std::exp (arg), einv = 1.0 / e;
            const double f = i - vn / Rf - Is * (e - einv);
            const double g = vn - base - k * f;
            const double dg = 1.0 + k * (1.0 / Rf + Is * invNVt * (e + einv));
            double step = g / dg;
            step = step > 0.1 ? 0.1 : (step < -0.1 ? -0.1 : step);
            vn -= step;

            if (std::abs (step) < 1e-7)
                break;
        }

        const double e = std::exp (vn * invNVt);
        fPrev = i - vn / Rf - Is * (e - 1.0 / e);
        v = vn;

        // Salida del operacional = señal limpia + tensión recortada de la realimentación
        return toneLp.lp (x + vn) * level.next();
    }

private:
    void updateTone() noexcept
    {
        // Aproximación del control de tono: paso bajo de 720 Hz (cerrado) a ~4,7 kHz (abierto)
        toneLp.setCutoff (720.0 * std::pow (6.5, tonePos), fs);
    }

    static constexpr double pi = 3.14159265358979323846;
    static constexpr double Rin = 4.7e3, Cin = 47e-9, Cf = 51e-12;
    static constexpr double Is = 2.52e-9;                       // 1N914
    static constexpr double invNVt = 1.0 / (1.752 * 25.85e-3);

    double fs = 48000.0, k = 0.0, Rf = 51e3;
    double v = 0.0, fPrev = 0.0, tonePos = -1.0;
    OnePole inputHp, toneLp;
    Smoothed level;
};
}
