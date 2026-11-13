#pragma once
#include "OnePole.h"

namespace fx
{
/** Distorsión de recorte duro: operacional con mucha ganancia seguido de diodos a masa.

    Circuito modelado: etapa no inversora de ganancia 1 + 1M / (4,7k + Rpot), que
    satura contra la alimentación de 9 V, y a su salida una resistencia de 10k hacia
    dos diodos de silicio en antiparalelo con un condensador de 1 nF. La ecuación
    del nudo se integra con la regla trapezoidal y se resuelve por Newton-Raphson.

    1.0 de señal digital equivale a 1 V.
*/
class Distortion
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        k = 1.0 / (2.0 * fs * C);
        level.prepare (fs);
        updateGain();
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
    void setControls (double gainPos, double tone, double levelPos) noexcept
    {
        level.target = 1.6 * levelPos * levelPos;

        if (gainPos != gainControl) { gainControl = gainPos; updateGain(); }
        if (tone != toneControl)    { toneControl = tone;    updateTone(); }
    }

    inline double process (double x) noexcept
    {
        // Operacional: los graves reciben menos ganancia y la salida no pasa de la alimentación
        const double u = rail * std::tanh ((x + (gain - 1.0) * inputHp.hp (x)) / rail);

        // C dV/dt = (u - V)/R - 2 Is sinh(V / nVt)
        const double base = v + k * fPrev;
        double vn = v;

        for (int iter = 0; iter < 12; ++iter)
        {
            const double e = std::exp (vn * invNVt), einv = 1.0 / e;
            const double f = (u - vn) / R - Is * (e - einv);
            const double g = vn - base - k * f;
            const double dg = 1.0 + k * (1.0 / R + Is * invNVt * (e + einv));
            double step = g / dg;
            step = step > 0.1 ? 0.1 : (step < -0.1 ? -0.1 : step);
            vn -= step;

            if (std::abs (step) < 1e-7)
                break;
        }

        const double e = std::exp (vn * invNVt);
        fPrev = (u - vn) / R - Is * (e - 1.0 / e);
        v = vn;

        return toneLp.lp (vn) * level.next();
    }

private:
    void updateGain() noexcept
    {
        const double pos = gainControl < 0.0 ? 0.5 : gainControl;
        const double leg = 4.7e3 + 500e3 * (1.0 - pos) * (1.0 - pos);     // rama a masa de la realimentación
        gain = 1.0 + 1e6 / leg;                                          // de x3 a x214
        inputHp.setCutoff (1.0 / (2.0 * pi * leg * 47e-9), fs);          // de 7 Hz a 720 Hz
    }

    void updateTone() noexcept
    {
        toneLp.setCutoff (1000.0 * std::pow (8.0, toneControl < 0.0 ? 0.5 : toneControl), fs);
    }

    static constexpr double pi = 3.14159265358979323846;
    static constexpr double R = 10e3, C = 1e-9, rail = 4.0;
    static constexpr double Is = 2.52e-9;                       // 1N914
    static constexpr double invNVt = 1.0 / (1.752 * 25.85e-3);

    double fs = 48000.0, k = 0.0, gain = 10.0;
    double v = 0.0, fPrev = 0.0, gainControl = -1.0, toneControl = -1.0;
    OnePole inputHp, toneLp;
    Smoothed level;
};
}
