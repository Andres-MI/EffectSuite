#pragma once
#include "OnePole.h"

namespace fx
{
/** Variante de distorsión: mismo circuito base con distintos componentes y ecualización. */
struct DistortionVoicing
{
    const char* name;
    double legMin;          // resistencia mínima de la rama a masa: fija la ganancia máxima (1 + 1M / legMin)
    double legCap;          // condensador de esa rama: recorta graves antes de saturar
    double opampGbw;        // producto ganancia-ancho de banda: a más ganancia, menos agudos
    double scoopDb;         // vaciado de medios a la salida (negativo), 0 = sin ecualización
    double lowBlend;        // graves limpios (< 120 Hz) añadidos a la salida, para bajo
    double toneMinHz, toneRange;
    double outputGain;
    bool tiltTone = false;      // control de tono que mezcla graves y agudos con un valle en medios
    double peakLowHz = 0.0, peakLowDb = 0.0;        // realce fijo de graves a la salida
    double peakHighHz = 0.0, peakHighDb = 0.0;      // realce de medios agudos; si tonePeak, el tono lo gradúa
    bool tonePeak = false;
    double cleanBlend = 0.0;    // señal limpia completa mezclada a la salida, para bajo
};

namespace distortions
{
    inline constexpr DistortionVoicing classic { "Classic", 4.7e3, 47e-9,  3e6,   0.0,  0.0, 1000.0, 8.0, 1.0 };
    inline constexpr DistortionVoicing rodent  { "Rodent",  560.0, 2.2e-6, 1.2e6, 0.0,  0.0, 600.0,  12.0, 1.0 };
    inline constexpr DistortionVoicing metal   { "Metal",   1.0e3, 330e-9, 3e6,  -10.0, 0.0, 1500.0, 5.0, 1.3 };
    inline constexpr DistortionVoicing bass    { "Bass Distortion", 4.7e3, 330e-9, 3e6, 0.0, 1.2, 900.0, 6.0, 0.9 };

    inline constexpr DistortionVoicing orange  { "Orange",  1.5e3, 100e-9, 3e6, 0.0, 0.0, 1000.0, 8.0, 1.5, true };
    inline constexpr DistortionVoicing swedish { "Swedish", 1.0e3, 150e-9, 3e6, 0.0, 0.0, 6500.0, 1.0, 0.75, false, 100.0, 9.0, 1100.0, 16.0, true };
    inline constexpr DistortionVoicing bassModern { "Bass Modern", 1.5e3, 100e-9, 3e6, 0.0, 0.0, 1800.0, 4.0, 0.6, false, 0.0, 0.0, 2800.0, 4.0, false, 0.9 };

    // El orden es el del parámetro guardado en los presets: añadir siempre al final.
    inline constexpr const DistortionVoicing* all[] { &classic, &rodent, &metal, &bass, &orange, &swedish, &bassModern };
    inline constexpr int count = 7;
}

/** Distorsión de recorte duro: operacional con mucha ganancia seguido de diodos a masa.

    Circuito modelado: etapa no inversora de ganancia 1 + 1M / (rama a masa), que
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
        lowLp.setCutoff (120.0, fs);
        level.prepare (fs);
        applyVoicing();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset(); toneLp.reset(); opampLp.reset(); lowLp.reset(); scoop.reset();
        tiltLow.reset(); tiltHigh.reset(); peakLow.reset(); peakHigh.reset();
        v = fPrev = 0.0;
        level.snap (level.target);
    }

    void setVoicing (int index) noexcept
    {
        index = index < 0 ? 0 : (index >= distortions::count ? distortions::count - 1 : index);

        if (distortions::all[index] == voicing)
            return;

        voicing = distortions::all[index];
        applyVoicing();
        reset();
    }

    /** Controles de 0 a 1. */
    void setControls (double gainPos, double tone, double levelPos) noexcept
    {
        level.target = 1.6 * levelPos * levelPos * voicing->outputGain;

        if (gainPos != gainControl) { gainControl = gainPos; updateGain(); }
        if (tone != toneControl)    { toneControl = tone;    updateTone(); }
    }

    inline double process (double x) noexcept
    {
        // Operacional: los graves reciben menos ganancia, el ancho de banda baja con la ganancia
        // y la salida no pasa de la alimentación
        const double amplified = x + (gain - 1.0) * opampLp.lp (inputHp.hp (x));
        const double u = rail * std::tanh (amplified / rail);

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

        double y;

        if (voicing->tiltTone)
            y = 1.7 * (1.0 - tiltMix) * tiltLow.lp (vn) + 1.3 * tiltMix * tiltHigh.hp (vn);     // graves frente a agudos, con valle en medios
        else
            y = toneLp.lp (vn);

        if (voicing->scoopDb != 0.0)   y = scoop.process (y);
        if (voicing->peakLowDb != 0.0)  y = peakLow.process (y);
        if (voicing->peakHighDb != 0.0) y = peakHigh.process (y);

        return (y + voicing->lowBlend * lowLp.lp (x) + voicing->cleanBlend * x) * level.next();
    }

private:
    void applyVoicing() noexcept
    {
        scoop.set (700.0, 0.8, voicing->scoopDb, fs);
        tiltLow.setCutoff (234.0, fs);
        tiltHigh.setCutoff (1060.0, fs);
        peakLow.set (voicing->peakLowHz > 0.0 ? voicing->peakLowHz : 100.0, 1.0, voicing->peakLowDb, fs);
        updateGain();
        updateTone();
    }

    void updateGain() noexcept
    {
        const double pos = gainControl < 0.0 ? 0.5 : gainControl;
        const double leg = voicing->legMin + 500e3 * (1.0 - pos) * (1.0 - pos);
        gain = 1.0 + 1e6 / leg;
        inputHp.setCutoff (1.0 / (2.0 * pi * leg * voicing->legCap), fs);
        opampLp.setCutoff (std::max (1500.0, std::min (20000.0, voicing->opampGbw / gain)), fs);
    }

    void updateTone() noexcept
    {
        const double tone = toneControl < 0.0 ? 0.5 : toneControl;
        toneLp.setCutoff (voicing->toneMinHz * std::pow (voicing->toneRange, tone), fs);
        tiltMix = tone;
        peakHigh.set (voicing->peakHighHz > 0.0 ? voicing->peakHighHz : 1000.0, 1.2,
                      voicing->tonePeak ? voicing->peakHighDb * (0.25 + 0.75 * tone) : voicing->peakHighDb, fs);
    }

    static constexpr double pi = 3.14159265358979323846;
    static constexpr double R = 10e3, C = 1e-9, rail = 4.0;
    static constexpr double Is = 2.52e-9;                       // 1N914
    static constexpr double invNVt = 1.0 / (1.752 * 25.85e-3);

    const DistortionVoicing* voicing = &distortions::classic;
    double fs = 48000.0, k = 0.0, gain = 10.0;
    double v = 0.0, fPrev = 0.0, gainControl = -1.0, toneControl = -1.0;
    double tiltMix = 0.5;
    OnePole inputHp, toneLp, opampLp, lowLp, tiltLow, tiltHigh;
    PeakFilter scoop, peakLow, peakHigh;
    Smoothed level;
};
}
