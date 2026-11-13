#pragma once
#include <cmath>

namespace fx
{
/** Compresor de pedal: detector de pico, relación 4:1 con codo suave y compensación automática. */
class Compressor
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        release = 1.0 - std::exp (-1.0 / (0.150 * fs));
        updateAttack();
        reset();
    }

    void reset() noexcept { env = 0.0; }

    /** Controles de 0 a 1. */
    void setControls (double sustain, double attackPos, double levelPos) noexcept
    {
        threshold = -15.0 - 30.0 * sustain;
        makeup = 0.4 * -threshold * slope;
        level = 4.0 * levelPos * levelPos;

        if (attackPos != attackControl)
        {
            attackControl = attackPos;
            updateAttack();
        }
    }

    inline float process (float x) noexcept
    {
        const double a = std::abs ((double) x);
        env += (a > env ? attack : release) * (a - env);

        const double over = 20.0 * std::log10 (env + 1e-9) - threshold;
        double reduction = 0.0;

        if (over >= 0.5 * knee)        reduction = slope * over;
        else if (over > -0.5 * knee)   reduction = slope * (over + 0.5 * knee) * (over + 0.5 * knee) / (2.0 * knee);

        lastReduction = reduction;
        return (float) (x * level * std::pow (10.0, (makeup - reduction) / 20.0));
    }

    /** Reducción de ganancia actual en dB (para pruebas y medidores). */
    double getReductionDb() const noexcept { return lastReduction; }

private:
    void updateAttack() noexcept
    {
        const double ms = 1.0 + 29.0 * (attackControl < 0.0 ? 0.3 : attackControl);
        attack = 1.0 - std::exp (-1.0 / (0.001 * ms * fs));
    }

    static constexpr double slope = 0.75, knee = 6.0;   // slope = 1 - 1/ratio, ratio 4:1

    double fs = 48000.0, env = 0.0, attack = 1.0, release = 1.0;
    double threshold = -30.0, makeup = 0.0, level = 1.0, attackControl = -1.0, lastReduction = 0.0;
};
}
