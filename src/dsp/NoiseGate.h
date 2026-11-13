#pragma once
#include <cmath>

namespace fx
{
/** Puerta de ruido con histéresis y tiempo de retención. */
class NoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        envRelease = std::exp (-1.0 / (0.010 * fs));
        attack     = 1.0 - std::exp (-1.0 / (0.001 * fs));
        release    = 1.0 - std::exp (-1.0 / (0.120 * fs));
        holdSamples = (int) (0.080 * fs);
        dcCoef = 1.0 - std::exp (-2.0 * 3.14159265358979323846 * 20.0 / fs);
        reset();
    }

    void reset() noexcept { env = 0.0f; gain = 0.0f; hold = 0; open = false; dc = 0.0; }

    void setThresholdDb (float db) noexcept
    {
        openLevel  = std::pow (10.0f, db / 20.0f);
        closeLevel = openLevel * 0.5f;
    }

    inline float process (float x) noexcept
    {
        // El detector ignora la componente continua, que si no mantendría la puerta abierta
        dc += dcCoef * (x - dc);
        const float a = std::abs (x - (float) dc);
        env = a > env ? a : (float) (env * envRelease);

        if (env >= openLevel)        { open = true; hold = holdSamples; }
        else if (env < closeLevel)   { if (hold > 0) --hold; else open = false; }

        const float target = open ? 1.0f : 0.0f;
        gain += (float) (target > gain ? attack : release) * (target - gain);
        return x * gain;
    }

private:
    double fs = 48000.0, envRelease = 0.0, attack = 1.0, release = 1.0, dc = 0.0, dcCoef = 0.0;
    float env = 0.0f, gain = 0.0f, openLevel = 0.001f, closeLevel = 0.0005f;
    int hold = 0, holdSamples = 0;
    bool open = false;
};
}
