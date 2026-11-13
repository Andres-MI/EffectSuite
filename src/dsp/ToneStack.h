#pragma once
#include <cmath>
#include <complex>

namespace fx
{
/** Valores de componentes del control de tonos pasivo (topología Fender/Marshall/Vox). */
struct ToneStackValues
{
    double R1, R2, R3, R4;   // R1 = agudos, R2 = graves, R3 = medios, R4 = resistencia de slope
    double C1, C2, C3;
};

namespace tonestacks
{
    inline constexpr ToneStackValues american { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 };
    inline constexpr ToneStackValues british  { 220e3, 1e6,   22e3, 33e3,  470e-12, 22e-9,  22e-9 };
    inline constexpr ToneStackValues modern   { 250e3, 1e6,   25e3, 47e3,  470e-12, 20e-9,  20e-9 };
}

/** Control de tonos de tres bandas modelado a partir del circuito real.

    Función de transferencia analógica de tercer orden (D. Yeh, "Discretization of
    the '59 Fender Bassman Tone Stack", DAFx-06) discretizada con la transformada
    bilineal. Los tres potenciómetros interactúan entre sí igual que en el circuito.
*/
class ToneStack
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
        update();
    }

    void reset() noexcept { s1 = s2 = s3 = 0.0; }

    void setValues (const ToneStackValues& v) noexcept { values = v; update(); }

    /** Posiciones de los potenciómetros, de 0 a 1. */
    void setControls (double bass, double mid, double treble) noexcept
    {
        if (bass == bassPos && mid == midPos && treble == treblePos)
            return;

        bassPos = bass; midPos = mid; treblePos = treble;
        update();
    }

    inline double process (double x) noexcept
    {
        const double y = B[0] * x + s1;
        s1 = B[1] * x - A[1] * y + s2;
        s2 = B[2] * x - A[2] * y + s3;
        s3 = B[3] * x - A[3] * y;
        return y;
    }

    /** Coeficientes analógicos: H(s) = (b1 s + b2 s² + b3 s³) / (a0 + a1 s + a2 s² + a3 s³). */
    void analogCoefficients (double b[4], double a[4]) const noexcept
    {
        const double R1 = values.R1, R2 = values.R2, R3 = values.R3, R4 = values.R4;
        const double C1 = values.C1, C2 = values.C2, C3 = values.C3;

        const double t = clamp01 (treblePos);
        const double m = clamp01 (midPos);
        const double l = std::exp ((clamp01 (bassPos) - 1.0) * 3.4);   // potenciómetro logarítmico

        b[0] = 0.0;
        b[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);

        b[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
             - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
             + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);

        b[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
             - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + t * C1 * C2 * C3 * R1 * R3 * R4
             - t * m * C1 * C2 * C3 * R1 * R3 * R4
             + t * l * C1 * C2 * C3 * R1 * R2 * R4;

        a[0] = 1.0;
        a[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4)
             + m * C3 * R3 + l * (C1 * R2 + C2 * R2);

        a[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
             - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
             + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4
                + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);

        a[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
             - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
             + l * C1 * C2 * C3 * R1 * R2 * R4
             + C1 * C2 * C3 * R1 * R3 * R4;
    }

    std::complex<double> analogResponse (double hz) const noexcept
    {
        double b[4], a[4];
        analogCoefficients (b, a);
        const std::complex<double> s (0.0, 2.0 * pi * hz);
        return (b[1] * s + b[2] * s * s + b[3] * s * s * s)
             / (a[0] + a[1] * s + a[2] * s * s + a[3] * s * s * s);
    }

    std::complex<double> digitalResponse (double hz) const noexcept
    {
        const auto z1 = std::polar (1.0, -2.0 * pi * hz / fs);
        const auto z2 = z1 * z1, z3 = z2 * z1;
        return (B[0] + B[1] * z1 + B[2] * z2 + B[3] * z3)
             / (1.0 + A[1] * z1 + A[2] * z2 + A[3] * z3);
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static double clamp01 (double v) noexcept { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    void update() noexcept
    {
        double b[4], a[4];
        analogCoefficients (b, a);

        const double c = 2.0 * fs, c2 = c * c, c3 = c2 * c;

        const double B0 = -b[1] * c - b[2] * c2 - b[3] * c3;
        const double B1 = -b[1] * c + b[2] * c2 + 3.0 * b[3] * c3;
        const double B2 =  b[1] * c + b[2] * c2 - 3.0 * b[3] * c3;
        const double B3 =  b[1] * c - b[2] * c2 + b[3] * c3;
        const double A0 = -a[0] - a[1] * c - a[2] * c2 - a[3] * c3;
        const double A1 = -3.0 * a[0] - a[1] * c + a[2] * c2 + 3.0 * a[3] * c3;
        const double A2 = -3.0 * a[0] + a[1] * c + a[2] * c2 - 3.0 * a[3] * c3;
        const double A3 = -a[0] + a[1] * c - a[2] * c2 + a[3] * c3;

        B[0] = B0 / A0; B[1] = B1 / A0; B[2] = B2 / A0; B[3] = B3 / A0;
        A[0] = 1.0;     A[1] = A1 / A0; A[2] = A2 / A0; A[3] = A3 / A0;
    }

    ToneStackValues values = tonestacks::american;
    double fs = 48000.0;
    double bassPos = 0.5, midPos = 0.5, treblePos = 0.5;
    double B[4] { 0, 0, 0, 0 }, A[4] { 1, 0, 0, 0 };
    double s1 = 0.0, s2 = 0.0, s3 = 0.0;
};
}
