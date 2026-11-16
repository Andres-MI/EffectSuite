#pragma once
#include "OnePole.h"
#include "Saturator.h"

namespace fx
{
struct FuzzVoicing
{
    const char* name;
    double inputHpHz;
    double octave;          // 0..1: cantidad de rectificación de onda completa (octava arriba)
    double lowBlend;        // graves limpios (< 120 Hz) añadidos a la salida, para bajo
    double toneMinHz, toneRange;
    double outputGain;
    double bias1 = 0.6, bias2 = -0.2;   // asimetría de cada etapa; 0 = recorte simétrico
    double secondGain = 3.0;            // ganancia entre las dos etapas
    bool tiltTone = false;              // control de tono que mezcla graves y agudos con un valle en medios
};

namespace fuzzes
{
    inline constexpr FuzzVoicing vintage { "Vintage",   40.0, 0.0, 0.0, 1500.0, 5.0, 1.0 };
    inline constexpr FuzzVoicing octave  { "Octave",    80.0, 0.8, 0.0, 2000.0, 4.0, 1.0 };
    inline constexpr FuzzVoicing bass    { "Bass Fuzz", 20.0, 0.0, 1.5, 900.0,  5.0, 0.9 };

    inline constexpr FuzzVoicing big     { "Big Fuzz",  60.0, 0.0, 0.0, 1500.0, 5.0, 1.6, 0.05, -0.05, 12.0, true };

    // El orden es el del parámetro guardado en los presets: añadir siempre al final.
    inline constexpr const FuzzVoicing* all[] { &vintage, &octave, &bass, &big };
    inline constexpr int count = 4;
}

/** Fuzz de dos etapas muy asimétricas.

    Modelo de comportamiento (no de circuito): dos saturaciones en cascada con
    polarización opuesta, que dan el recorte casi cuadrado y rico en armónicos
    pares de un fuzz de transistores. La variante Octave rectifica la señal antes
    de saturarla, lo que dobla la frecuencia.
*/
class Fuzz
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        couplingHp.setCutoff (30.0, fs);
        rectifierHp.setCutoff (60.0, fs);
        dcBlock.setCutoff (12.0, fs);
        lowLp.setCutoff (120.0, fs);
        gain.prepare (fs);
        level.prepare (fs);
        applyVoicing();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset(); couplingHp.reset(); toneLp.reset(); lowLp.reset(); rectifierHp.reset(); dcBlock.reset();
        tiltLow.reset(); tiltHigh.reset();
        rectX = rectF = 0.0;
        first.reset(); second.reset();
        gain.snap (gain.target);
        level.snap (level.target);
    }

    void setVoicing (int index) noexcept
    {
        index = index < 0 ? 0 : (index >= fuzzes::count ? fuzzes::count - 1 : index);

        if (fuzzes::all[index] == voicing)
            return;

        voicing = fuzzes::all[index];
        applyVoicing();
        reset();
    }

    /** Controles de 0 a 1. */
    void setControls (double amount, double tone, double levelPos) noexcept
    {
        gain.target = 4.0 * std::pow (150.0, amount);
        level.target = 0.6 * levelPos * levelPos * voicing->outputGain;

        if (tone != toneControl) { toneControl = tone; updateTone(); }
    }

    inline double process (double x) noexcept
    {
        double u = inputHp.hp (x) * gain.next();

        if (voicing->octave > 0.0)
        {
            // Rectificación de onda completa (dobla la frecuencia), evaluada con antiderivada
            // para que el pico de la rectificación no genere aliasing, y sin su componente continua
            const double F = 0.5 * u * std::abs (u), du = u - rectX;
            const double rectified = std::abs (du) > 1e-6 ? (F - rectF) / du : std::abs (u);
            rectX = u;
            rectF = F;
            u += voicing->octave * (2.0 * rectifierHp.hp (rectified) - u);
        }

        double y = first.process (u, voicing->bias1, tanhBias1);
        y = second.process (couplingHp.hp (y) * voicing->secondGain, voicing->bias2, tanhBias2);

        const double toned = voicing->tiltTone ? 1.6 * (1.0 - tiltMix) * tiltLow.lp (y) + 1.2 * tiltMix * tiltHigh.hp (y)
                                               : toneLp.lp (y);
        return (dcBlock.hp (toned) + voicing->lowBlend * lowLp.lp (x)) * level.next();
    }

private:
    void applyVoicing() noexcept
    {
        inputHp.setCutoff (voicing->inputHpHz, fs);
        tiltLow.setCutoff (400.0, fs);
        tiltHigh.setCutoff (1500.0, fs);
        tanhBias1 = std::tanh (voicing->bias1);
        tanhBias2 = std::tanh (voicing->bias2);
        updateTone();
    }

    void updateTone() noexcept
    {
        tiltMix = toneControl < 0.0 ? 0.5 : toneControl;
        toneLp.setCutoff (voicing->toneMinHz * std::pow (voicing->toneRange, tiltMix), fs);
    }

    double tanhBias1 = 0.0, tanhBias2 = 0.0, tiltMix = 0.5;

    const FuzzVoicing* voicing = &fuzzes::vintage;
    double fs = 48000.0, toneControl = -1.0;
    OnePole inputHp, couplingHp, toneLp, lowLp, rectifierHp, dcBlock, tiltLow, tiltHigh;
    double rectX = 0.0, rectF = 0.0;
    Saturator first, second;
    Smoothed gain, level;
};
}
