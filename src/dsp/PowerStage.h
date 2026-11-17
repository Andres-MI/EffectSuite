#pragma once
#include "OnePole.h"
#include "Saturator.h"

namespace fx
{
/** Etapa de potencia a válvulas conectada a un altavoz.

    Una etapa de pentodos entrega corriente, y el altavoz no es una resistencia: su impedancia
    sube en la resonancia del cono y en agudos, así que ahí suena más fuerte. La realimentación
    negativa del ampli lo corrige mientras la etapa trabaja en limpio; cuando satura, la
    realimentación deja de actuar y aparecen de golpe esos graves y agudos. Esa interacción es
    buena parte del "empuje" de un ampli a volumen alto.
*/
class PowerStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        sagAttack  = 1.0 - std::exp (-1.0 / (0.005 * fs));
        sagRelease = 1.0 - std::exp (-1.0 / (0.150 * fs));
        feedbackLp.setCutoff (5000.0, fs);
        presenceHp.setCutoff (2500.0, fs);
        updateSpeaker();
        reset();
    }

    void reset() noexcept
    {
        shaper.reset(); resonance.reset(); inductance.reset(); feedbackLp.reset(); presenceHp.reset();
        sagEnv = 0.0;
        fedBack = 0.0;
    }

    /** feedbackAmount: 0 = sin realimentación (muy coloreado) a 1 = mucha (plano y controlado). */
    void setCharacter (double feedbackAmount, double sagAmount, bool bassSpeaker) noexcept
    {
        feedback = feedbackAmount;
        sag = sagAmount;

        if (bassSpeaker != isBass)
        {
            isBass = bassSpeaker;
            updateSpeaker();
        }
    }

    void setPresence (double amount) noexcept { presence = amount; }

    inline double process (double x) noexcept
    {
        // Válvulas de potencia: recorte simétrico, con la alimentación cayendo cuando se le exige (sag)
        const double supply = 1.0 - sag * sagEnv;
        const double current = supply * shaper.process ((x * (1.0 + feedback) - fedBack) / supply, 0.0, 0.0);
        sagEnv += (current * current > sagEnv ? sagAttack : sagRelease) * (current * current - sagEnv);

        // Tensión en bornes del altavoz = corriente por su impedancia
        const double voltage = inductance.process (resonance.process (current));

        // Realimentación desde la salida; el control de presencia le quita agudos para que el ampli los dé
        const double sensed = feedbackLp.lp (voltage);
        fedBack = feedback * (sensed - presence * presenceHp.hp (sensed));
        return voltage;
    }

private:
    void updateSpeaker() noexcept
    {
        resonance.set (isBass ? 55.0 : 100.0, 1.3, 8.0, fs);       // resonancia del cono en su caja
        inductance.setShelf (true, 3500.0, 6.0, fs);               // la bobina: la impedancia sube en agudos
    }

    double fs = 48000.0, feedback = 0.5, sag = 0.2, presence = 0.0;
    double sagEnv = 0.0, sagAttack = 1.0, sagRelease = 1.0, fedBack = 0.0;
    bool isBass = false;
    Saturator shaper;
    PeakFilter resonance, inductance;
    OnePole feedbackLp, presenceHp;
};
}
