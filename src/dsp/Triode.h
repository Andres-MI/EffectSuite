#pragma once
#include <cmath>
#include <vector>

namespace fx
{
/** Etapa de previo con un triodo 12AX7 en cátodo común, calculada desde su circuito.

    Circuito: alimentación de 300 V, resistencia de placa de 100k, resistencia de cátodo de
    1,5k con condensador de desacoplo, y resistencia de rejilla. La corriente de placa sigue
    la ecuación de Koren, que reproduce las curvas medidas de la válvula.

    Resolver esa ecuación en cada muestra es caro, así que se resuelve una vez al arrancar
    para toda una rejilla de tensiones (rejilla frente a cátodo) y se guarda en una tabla,
    junto con su antiderivada para poder evaluarla sin aliasing.
*/
class TriodeTable
{
public:
    static const TriodeTable& get()
    {
        static const TriodeTable table;
        return table;
    }

    static constexpr double supply = 300.0, plateR = 100e3, cathodeR = 1500.0;
    static constexpr double gridSeriesR = 470e3, gridConductR = 2.7e3;
    static constexpr double driveMin = -8.0, driveMax = 8.0, cathodeMin = 0.4, cathodeMax = 4.0;
    static constexpr int numDrive = 2048, numCathode = 10;

    double restCathode = 1.3, restPlate = 190.0, restGain = 60.0;      // punto de reposo y ganancia de pequeña señal
    double driveStep = 0.0;

    /** Tensión de placa para una excitación (tensión que llega a la rejilla menos la de cátodo). */
    double plate (double drive, double cathode) const noexcept      { return lookup (plateV, drive, cathode, false); }
    /** Antiderivada de plate() respecto a la excitación. */
    double plateIntegral (double drive, double cathode) const noexcept { return lookup (integral, drive, cathode, true); }

    /** Corriente de placa de un 12AX7 (ecuación de Koren), en amperios. */
    static double plateCurrent (double gridToCathode, double plateToCathode) noexcept
    {
        if (plateToCathode <= 0.0)
            return 0.0;

        constexpr double mu = 100.0, exponent = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
        const double inner = kp * (1.0 / mu + gridToCathode / std::sqrt (kvb + plateToCathode * plateToCathode));
        const double e1 = plateToCathode / kp * (inner > 30.0 ? inner : std::log1p (std::exp (inner)));
        return e1 > 0.0 ? 2.0 * std::pow (e1, exponent) / kg1 : 0.0;
    }

    /** Parte de la excitación que la rejilla no deja pasar: en cuanto la rejilla se hace positiva
        respecto al cátodo empieza a conducir y la resistencia en serie se queda con casi toda la tensión. */
    static double gridDrop (double drive) noexcept
    {
        constexpr double knee = 0.25;
        const double soft = drive > 8.0 ? drive : knee * std::log1p (std::exp (drive / knee));
        return soft * gridSeriesR / (gridSeriesR + gridConductR);
    }

private:
    TriodeTable()
    {
        driveStep = (driveMax - driveMin) / (numDrive - 1);
        plateV.resize ((size_t) (numDrive * numCathode));
        integral.resize (plateV.size());

        for (int k = 0; k < numCathode; ++k)
        {
            const double cathode = cathodeMin + (cathodeMax - cathodeMin) * k / (numCathode - 1);
            double sum = 0.0, previous = 0.0;

            for (int i = 0; i < numDrive; ++i)
            {
                const double drive = driveMin + driveStep * i;
                const double v = solvePlate (drive - gridDrop (drive), cathode);
                if (i > 0) sum += 0.5 * (v + previous) * driveStep;
                previous = v;
                plateV[(size_t) (k * numDrive + i)] = v;
                integral[(size_t) (k * numDrive + i)] = sum;
            }
        }

        // Punto de reposo: sin señal, el cátodo se queda en la tensión que produce su propia corriente
        for (int i = 0; i < 200; ++i)
        {
            const double v = solvePlate (-restCathode, restCathode);
            restCathode += 0.5 * (cathodeR * (supply - v) / plateR - restCathode);
        }

        restPlate = plate (-restCathode, restCathode);
        restGain = std::abs (plate (-restCathode + 0.05, restCathode) - plate (-restCathode - 0.05, restCathode)) / 0.1;
    }

    /** Tensión de placa en la que la corriente por la resistencia de placa iguala la de la válvula. */
    static double solvePlate (double gridToCathode, double cathode) noexcept
    {
        double low = cathode, high = supply;

        for (int i = 0; i < 48; ++i)
        {
            const double mid = 0.5 * (low + high);
            const double excess = (supply - mid) / plateR - plateCurrent (gridToCathode, mid - cathode);
            (excess > 0.0 ? low : high) = mid;      // sobra corriente de la resistencia: la placa tiene que subir
        }

        return 0.5 * (low + high);
    }

    double lookup (const std::vector<double>& table, double drive, double cathode, bool isIntegral) const noexcept
    {
        const double kPos = (cathode < cathodeMin ? cathodeMin : (cathode > cathodeMax ? cathodeMax : cathode)) - cathodeMin;
        const double kIndex = kPos * (numCathode - 1) / (cathodeMax - cathodeMin);
        const int k = kIndex >= numCathode - 1 ? numCathode - 2 : (int) kIndex;
        const double kFrac = kIndex - k;

        const double clamped = drive < driveMin ? driveMin : (drive > driveMax ? driveMax : drive);
        const double dIndex = (clamped - driveMin) / driveStep;
        const int d = dIndex >= numDrive - 1 ? numDrive - 2 : (int) dIndex;
        const double dFrac = dIndex - d;

        const double* a = table.data() + k * numDrive + d;
        const double* b = a + numDrive;
        double value = (1.0 - kFrac) * (a[0] + dFrac * (a[1] - a[0])) + kFrac * (b[0] + dFrac * (b[1] - b[0]));

        // Fuera de la tabla la placa ya no se mueve, así que su antiderivada sigue en línea recta
        if (isIntegral && clamped != drive)
            value += (drive - clamped) * lookup (plateV, clamped, cathode, false);

        return value;
    }

    std::vector<double> plateV, integral;
};

/** Una etapa de triodo con memoria: el cátodo y el condensador de acoplo se cargan con la señal,
    así que la etapa cambia de punto de trabajo cuando se la aprieta, como la válvula real. */
class TriodeStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double T = 1.0 / sampleRate;
        cathodeStep = T / 22e-6;                                // condensador de cátodo de 22 uF
        couplingStep = T / 47e-9;                               // condensador de acoplo de 47 nF
        couplingLeak = T / (1e6 * 47e-9);                       // se descarga por la resistencia de rejilla de 1M
        reset();
    }

    void reset() noexcept
    {
        cathode = table.restCathode;
        blocking = 0.0;
        previousDrive = -table.restCathode;
    }

    /** Entrada y salida normalizadas: ganancia unidad con señal pequeña, salida invertida.
        Con hasBlocking, la corriente de rejilla carga el condensador de acoplo y desplaza la polarización. */
    inline double process (double x, bool hasBlocking) noexcept
    {
        const double drive = inputScale * x - blocking - cathode;

        // Tensión de placa promediada entre la muestra anterior y esta (antiderivada): sin aliasing
        const double delta = drive - previousDrive;
        const double plate = std::abs (delta) > table.driveStep
                               ? (table.plateIntegral (drive, cathode) - table.plateIntegral (previousDrive, cathode)) / delta
                               : table.plate (0.5 * (drive + previousDrive), cathode);
        previousDrive = drive;

        // El cátodo sigue despacio a la corriente media
        cathode += cathodeStep * ((TriodeTable::supply - plate) / TriodeTable::plateR - cathode / TriodeTable::cathodeR);

        if (hasBlocking)
        {
            const double gridCurrent = TriodeTable::gridDrop (drive) / TriodeTable::gridSeriesR;
            blocking += couplingStep * gridCurrent - couplingLeak * blocking;
        }

        return (plate - table.restPlate) * outputScale;
    }

private:
    static constexpr double inputScale = 1.5;       // 1,0 de señal normalizada = 1,5 V en la rejilla: el margen antes de saturar
    const TriodeTable& table = TriodeTable::get();
    const double outputScale = 1.0 / (inputScale * table.restGain);
    double cathode = 1.3, blocking = 0.0, previousDrive = -1.3;
    double cathodeStep = 0.0, couplingStep = 0.0, couplingLeak = 0.0;
};
}
