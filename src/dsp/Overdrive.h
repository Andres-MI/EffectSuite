#pragma once
#include "OnePole.h"

namespace fx
{
/** Variante de overdrive: mismos principios de circuito, distintos componentes. */
struct OverdriveVoicing
{
    const char* name;
    double inputCap;        // con 4,7k fija la frecuencia a partir de la cual se recorta
    int diodesPos, diodesNeg;   // diodos en serie por polaridad (distintos = recorte asimétrico)
    double cleanBlend;      // señal limpia añadida a la salida
    double lowBlend;        // graves limpios (< 150 Hz) añadidos a la salida, para bajo
    double toneMinHz, toneRange;
    double outputGain;
};

namespace overdrives
{
    inline constexpr OverdriveVoicing midBoost    { "Mid Boost",    47e-9,  1, 1, 0.0, 0.0, 720.0, 6.5, 1.0 };
    inline constexpr OverdriveVoicing blues       { "Blues",        220e-9, 1, 2, 0.0, 0.0, 900.0, 7.0, 0.6 };
    inline constexpr OverdriveVoicing transparent { "Transparent",  100e-9, 2, 2, 0.6, 0.0, 1200.0, 6.0, 0.6 };
    inline constexpr OverdriveVoicing bass        { "Bass Drive",   330e-9, 1, 1, 0.0, 1.0, 800.0, 6.0, 0.9 };

    inline constexpr const OverdriveVoicing* all[] { &midBoost, &blues, &transparent, &bass };
    inline constexpr int count = 4;
}

/** Overdrive de diodos en el lazo de realimentación.

    Etapa de recorte modelada desde el circuito: un amplificador operacional no
    inversor cuya realimentación es Rf en paralelo con un condensador y diodos
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
        lowLp.setCutoff (150.0, fs);
        dcBlock.setCutoff (12.0, fs);
        level.prepare (fs);
        applyVoicing();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset();
        toneLp.reset();
        lowLp.reset();
        dcBlock.reset();
        v = fPrev = 0.0;
        level.snap (level.target);
    }

    void setVoicing (int index) noexcept
    {
        index = index < 0 ? 0 : (index >= overdrives::count ? overdrives::count - 1 : index);

        if (overdrives::all[index] == voicing)
            return;

        voicing = overdrives::all[index];
        applyVoicing();
        reset();
    }

    /** Controles de 0 a 1. */
    void setControls (double drive, double tone, double levelPos) noexcept
    {
        Rf = 51e3 + 500e3 * drive * drive;      // potenciómetro de 500k, ley aproximadamente logarítmica
        level.target = 2.0 * levelPos * levelPos * voicing->outputGain;

        if (tone != tonePos)
        {
            tonePos = tone;
            updateTone();
        }
    }

    inline double process (double x) noexcept
    {
        const double i = inputHp.hp (x) / Rin;       // corriente que entra en la red de realimentación

        // C dV/dt = i - V/Rf - Is (exp(V/(np nVt)) - exp(-V/(nn nVt)))
        const double base = v + k * fPrev;
        double vn = v;

        for (int iter = 0; iter < 12; ++iter)
        {
            const double ep = std::exp (vn * aPos), en = std::exp (-vn * aNeg);
            const double f = i - vn / Rf - Is * (ep - en);
            const double g = vn - base - k * f;
            const double dg = 1.0 + k * (1.0 / Rf + Is * (aPos * ep + aNeg * en));
            double step = g / dg;
            step = step > 0.1 ? 0.1 : (step < -0.1 ? -0.1 : step);
            vn -= step;

            if (std::abs (step) < 1e-7)
                break;
        }

        fPrev = i - vn / Rf - Is * (std::exp (vn * aPos) - std::exp (-vn * aNeg));
        v = vn;

        // Salida del operacional = señal limpia + tensión recortada de la realimentación
        // El recorte asimétrico deja componente continua: se quita con un paso alto de 12 Hz
        const double wet = dcBlock.hp (toneLp.lp (x + vn)) + voicing->cleanBlend * x + voicing->lowBlend * lowLp.lp (x);
        return wet * level.next();
    }

private:
    void applyVoicing() noexcept
    {
        inputHp.setCutoff (1.0 / (2.0 * pi * Rin * voicing->inputCap), fs);
        aPos = invNVt / voicing->diodesPos;
        aNeg = invNVt / voicing->diodesNeg;
        updateTone();
    }

    void updateTone() noexcept
    {
        toneLp.setCutoff (voicing->toneMinHz * std::pow (voicing->toneRange, tonePos < 0.0 ? 0.5 : tonePos), fs);
    }

    static constexpr double pi = 3.14159265358979323846;
    static constexpr double Rin = 4.7e3, Cf = 51e-12;
    static constexpr double Is = 2.52e-9;                       // 1N914
    static constexpr double invNVt = 1.0 / (1.752 * 25.85e-3);

    const OverdriveVoicing* voicing = &overdrives::midBoost;
    double fs = 48000.0, k = 0.0, Rf = 51e3, aPos = invNVt, aNeg = invNVt;
    double v = 0.0, fPrev = 0.0, tonePos = -1.0;
    OnePole inputHp, toneLp, lowLp, dcBlock;
    Smoothed level;
};
}
