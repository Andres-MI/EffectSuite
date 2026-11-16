#pragma once
#include <cmath>
#include <vector>
#include "OnePole.h"
#include "TimeEffects.h"

namespace fx
{
/** Reverb de muelles, como la de los amplis clásicos.

    Cada muelle es un lazo de retardo realimentado en el que la señal pasa por una cadena
    de filtros pasa-todo: retrasan más unas frecuencias que otras, y eso da el "chapoteo"
    característico. Tres muelles de longitudes distintas, repartidos en estéreo.
*/
class SpringReverb
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Con más frecuencia de muestreo hacen falta más etapas para la misma dispersión
        stages = std::min (maxStages, std::max (8, (int) std::lround (26.0 * fs / 48000.0)));
        const double lengthsMs[numSprings] { 33.0, 41.0, 47.0 };

        for (int s = 0; s < numSprings; ++s)
        {
            springs[s].line.prepare ((int) (0.06 * fs));
            springs[s].delaySamples = 0.001 * lengthsMs[s] * fs;
            springs[s].damping.setCutoff (4200.0 - 300.0 * s, fs);
        }

        inputHp.setCutoff (160.0, fs);
        inputLp.setCutoff (5500.0, fs);
        mix.prepare (fs);
        reset();
    }

    void reset() noexcept
    {
        for (auto& spring : springs)
        {
            spring.line.reset();
            spring.damping.reset();
            for (auto& z : spring.allpass) z = 0.0;
        }

        inputHp.reset();
        inputLp.reset();
        mix.snap (mix.target);
    }

    /** Controles de 0 a 1. */
    void setControls (double size, double mixPos) noexcept
    {
        feedback = 0.55 + 0.38 * size;
        mix.target = 1.6 * mixPos;
    }

    /** Añade la reverb a left y right. */
    inline void process (float& left, float& right) noexcept
    {
        const double in = inputLp.lp (inputHp.hp (0.5 * ((double) left + right)));
        double out[numSprings];

        for (int s = 0; s < numSprings; ++s)
        {
            auto& spring = springs[s];
            double y = spring.line.read (spring.delaySamples);

            for (int i = 0; i < stages; ++i)
            {
                const double v = y - coefficient * spring.allpass[i];
                y = coefficient * v + spring.allpass[i];
                spring.allpass[i] = v;
            }

            y = spring.damping.lp (y);
            spring.line.write ((float) (in + feedback * y));
            out[s] = y;
        }

        const float m = (float) mix.next();
        left  += m * (float) (0.6 * out[0] + 0.25 * out[2]);
        right += m * (float) (0.6 * out[1] + 0.25 * out[2]);
    }

private:
    static constexpr int numSprings = 3, maxStages = 110;
    static constexpr double coefficient = 0.62;

    struct Spring
    {
        DelayLine line;
        double delaySamples = 1000.0;
        OnePole damping;
        double allpass[maxStages] {};
    };

    double fs = 48000.0, feedback = 0.7;
    int stages = 26;
    Spring springs[numSprings];
    OnePole inputHp, inputLp;
    Smoothed mix;
};
}
