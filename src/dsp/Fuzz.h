#pragma once
#include "OnePole.h"
#include "Saturator.h"

namespace fx
{
/** Fuzz de dos etapas muy asimétricas.

    Modelo de comportamiento (no de circuito): dos saturaciones en cascada con
    polarización opuesta, que dan el recorte casi cuadrado y rico en armónicos
    pares de un fuzz de transistores.
*/
class Fuzz
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        inputHp.setCutoff (40.0, fs);
        couplingHp.setCutoff (30.0, fs);
        gain.prepare (fs);
        level.prepare (fs);
        updateTone();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset(); couplingHp.reset(); toneLp.reset();
        first.reset(); second.reset();
        gain.snap (gain.target);
        level.snap (level.target);
    }

    /** Controles de 0 a 1. */
    void setControls (double amount, double tone, double levelPos) noexcept
    {
        gain.target = 4.0 * std::pow (150.0, amount);
        level.target = 0.6 * levelPos * levelPos;

        if (tone != toneControl) { toneControl = tone; updateTone(); }
    }

    inline double process (double x) noexcept
    {
        double y = first.process (inputHp.hp (x) * gain.next(), bias1, tanhBias1);
        y = second.process (couplingHp.hp (y) * 3.0, bias2, tanhBias2);
        return toneLp.lp (y) * level.next();
    }

private:
    void updateTone() noexcept
    {
        toneLp.setCutoff (1500.0 * std::pow (5.0, toneControl < 0.0 ? 0.5 : toneControl), fs);
    }

    static constexpr double bias1 = 0.6, bias2 = -0.2;
    const double tanhBias1 = std::tanh (bias1), tanhBias2 = std::tanh (bias2);

    double fs = 48000.0, toneControl = -1.0;
    OnePole inputHp, couplingHp, toneLp;
    Saturator first, second;
    Smoothed gain, level;
};
}
