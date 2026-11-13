#pragma once
#include "OnePole.h"
#include "Saturator.h"
#include "ToneStack.h"

namespace fx
{
/** Descripción de un amplificador: cadena de etapas de válvula, control de tonos y etapa de potencia. */
struct AmpVoicing
{
    const char* name;
    int numStages;
    double stageGain[4];
    double stageBias[4];       // asimetría de cada etapa (armónicos pares)
    double couplingHz[4];      // paso alto del condensador de acoplo a la salida de cada etapa
    double millerHz[4];        // paso bajo por capacidad Miller de cada etapa
    int toneStackAfter;        // índice de la etapa tras la que va el control de tonos
    ToneStackValues toneStack;
    double toneStackMakeup;
    double inputHpHz;
    double gainPotMin;         // rango del potenciómetro de ganancia (situado antes de la etapa 1)
    double powerDrive;
    double sag;
    double outputTrim;
};

namespace voicings
{
    inline constexpr AmpVoicing cleanAmerican {
        "Clean American", 2,
        { 1.2, 5.0, 0, 0 }, { 0.25, 0.20, 0, 0 }, { 20.0, 25.0, 0, 0 }, { 14000.0, 11000.0, 0, 0 },
        0, tonestacks::american, 7.0, 30.0, 0.05, 1.0, 0.10, 1.2
    };

    inline constexpr AmpVoicing britishCrunch {
        "British Crunch", 3,
        { 1.5, 30.0, 3.0, 0 }, { 0.30, 0.35, 0.10, 0 }, { 150.0, 40.0, 20.0, 0 }, { 11000.0, 8500.0, 9500.0, 0 },
        2, tonestacks::british, 4.0, 60.0, 0.01, 2.0, 0.20, 0.25
    };

    inline constexpr AmpVoicing highGain {
        "High Gain", 4,
        { 1.5, 60.0, 16.0, 4.0 }, { 0.30, 0.40, 0.30, 0.20 }, { 300.0, 120.0, 60.0, 30.0 }, { 9500.0, 7500.0, 7000.0, 8500.0 },
        3, tonestacks::modern, 4.0, 100.0, 0.002, 1.5, 0.12, 0.22
    };

    inline constexpr AmpVoicing tweed {
        "Tweed", 3,
        { 1.5, 10.0, 2.5, 0 }, { 0.30, 0.30, 0.15, 0 }, { 25.0, 30.0, 20.0, 0 }, { 12000.0, 9000.0, 10000.0, 0 },
        2, tonestacks::tweed, 4.0, 30.0, 0.02, 2.2, 0.30, 0.3
    };

    inline constexpr AmpVoicing classAChime {
        "Class A Chime", 2,
        { 1.5, 8.0, 0, 0 }, { 0.30, 0.25, 0, 0 }, { 60.0, 40.0, 0, 0 }, { 16000.0, 13000.0, 0, 0 },
        0, tonestacks::chime, 5.0, 50.0, 0.04, 2.8, 0.35, 0.4
    };

    inline constexpr AmpVoicing modernLead {
        "Modern Lead", 4,
        { 1.5, 70.0, 20.0, 4.0 }, { 0.25, 0.30, 0.25, 0.15 }, { 180.0, 90.0, 45.0, 25.0 }, { 8500.0, 6500.0, 6000.0, 7500.0 },
        3, tonestacks::british, 4.0, 70.0, 0.002, 1.2, 0.18, 0.22
    };

    inline constexpr AmpVoicing bassTube {
        "Bass Tube", 3,
        { 1.2, 6.0, 2.5, 0 }, { 0.20, 0.25, 0.10, 0 }, { 12.0, 12.0, 10.0, 0 }, { 9000.0, 7000.0, 7000.0, 0 },
        0, tonestacks::bass, 6.0, 18.0, 0.03, 1.6, 0.25, 0.45
    };

    inline constexpr AmpVoicing bassClean {
        "Bass Clean", 2,
        { 0.6, 3.0, 0, 0 }, { 0.05, 0.05, 0, 0 }, { 10.0, 10.0, 0, 0 }, { 16000.0, 14000.0, 0, 0 },
        0, tonestacks::bass, 6.0, 15.0, 0.08, 1.0, 0.0, 2.8
    };

    inline constexpr AmpVoicing bassRock {
        "Bass Rock", 3,
        { 1.5, 20.0, 3.0, 0 }, { 0.30, 0.35, 0.15, 0 }, { 25.0, 20.0, 12.0, 0 }, { 8000.0, 6000.0, 6500.0, 0 },
        2, tonestacks::bass, 6.0, 25.0, 0.01, 2.0, 0.25, 0.25
    };

    // El orden es el del parámetro guardado en los presets: añadir siempre al final.
    inline constexpr const AmpVoicing* all[] { &cleanAmerican, &britishCrunch, &highGain, &tweed, &classAChime,
                                               &modernLead, &bassTube, &bassClean, &bassRock };
    inline constexpr int count = 9;
}

/** Amplificador de válvulas: previo multietapa, control de tonos de circuito y etapa de potencia con sag. */
class Amp
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        gainPot.prepare (fs);
        master.prepare (fs);
        sagAttack  = 1.0 - std::exp (-1.0 / (0.005 * fs));
        sagRelease = 1.0 - std::exp (-1.0 / (0.150 * fs));
        presenceHp.setCutoff (2500.0, fs);
        toneStack.prepare (fs);
        applyVoicing();
        reset();
    }

    void reset() noexcept
    {
        inputHp.reset();
        presenceHp.reset();
        toneStack.reset();

        for (int i = 0; i < 4; ++i) { coupling[i].reset(); miller[i].reset(); shaper[i].reset(); }

        powerShaper.reset();

        sagEnv = 0.0;
        gainPot.snap (gainPot.target);
        master.snap (master.target);
    }

    void setVoicing (int index) noexcept
    {
        index = index < 0 ? 0 : (index >= voicings::count ? voicings::count - 1 : index);

        if (voicings::all[index] == voicing)
            return;

        voicing = voicings::all[index];
        applyVoicing();
        reset();
    }

    /** Controles de 0 a 1. */
    void setControls (double gain, double bass, double mid, double treble, double presence, double masterPos) noexcept
    {
        gainPot.target = voicing->gainPotMin * std::pow (1.0 / voicing->gainPotMin, gain);
        master.target = 2.0 * masterPos * masterPos;
        presenceAmount = 1.5 * presence;
        toneStack.setControls (bass, mid, treble);
    }

    inline double process (double x) noexcept
    {
        const auto& v = *voicing;
        x = inputHp.hp (x);

        for (int s = 0; s < v.numStages; ++s)
        {
            if (s == 1)
                x *= gainPot.next();

            x = -shaper[s].process (x * v.stageGain[s], v.stageBias[s], biasTanh[s]) * biasNorm[s];
            x = miller[s].lp (coupling[s].hp (x));

            if (s == v.toneStackAfter)
                x = toneStack.process (x) * v.toneStackMakeup;
        }

        // Etapa de potencia push-pull: recorte simétrico, presencia y caída de la fuente (sag)
        x *= master.next() * v.powerDrive;
        x += presenceAmount * presenceHp.hp (x);

        const double supply = 1.0 - v.sag * sagEnv;
        const double y = supply * powerShaper.process (x / supply, 0.0, 0.0);
        const double level = y * y;
        sagEnv += (level > sagEnv ? sagAttack : sagRelease) * (level - sagEnv);

        return y * v.outputTrim;
    }

private:
    void applyVoicing() noexcept
    {
        const auto& v = *voicing;
        inputHp.setCutoff (v.inputHpHz, fs);
        toneStack.setValues (v.toneStack);

        for (int s = 0; s < v.numStages; ++s)
        {
            coupling[s].setCutoff (v.couplingHz[s], fs);
            miller[s].setCutoff (v.millerHz[s], fs);
            biasTanh[s] = std::tanh (v.stageBias[s]);
            biasNorm[s] = 1.0 / (1.0 - biasTanh[s] * biasTanh[s]);     // ganancia de pequeña señal unitaria
        }
    }

    const AmpVoicing* voicing = &voicings::cleanAmerican;
    double fs = 48000.0;
    OnePole inputHp, presenceHp, coupling[4], miller[4];
    Saturator shaper[4], powerShaper;
    double biasNorm[4] { 1, 1, 1, 1 }, biasTanh[4] { 0, 0, 0, 0 };
    ToneStack toneStack;
    Smoothed gainPot, master;
    double presenceAmount = 0.0, sagEnv = 0.0, sagAttack = 1.0, sagRelease = 1.0;
};
}
