// Pruebas de medida del motor de audio. Se ejecutan sin tarjeta de sonido:
// comprueban con números lo que no se puede comprobar de oído en un servidor.
//
//   EffectSuiteMeasure                 ejecuta las pruebas (código de salida != 0 si alguna falla)
//   EffectSuiteMeasure --render <dir>  además escribe WAV de demostración en <dir>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "dsp/GuitarChain.h"
#include "dsp/PitchTracker.h"

namespace
{
constexpr double pi = 3.14159265358979323846;
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("   [%s] %s\n", ok ? " OK " : "FALLO", what);
    if (! ok) ++failures;
}

double dB (double v) { return 20.0 * std::log10 (std::max (v, 1e-12)); }

/** Pasa una señal por la cadena en bloques de tamaño blockSize. Deja en x el canal izquierdo
    y, si se pide, devuelve el derecho en rightOut. */
void runChain (fx::GuitarChain& chain, std::vector<float>& x, const fx::ChainParams& p, int blockSize = 64,
               std::vector<float>* rightOut = nullptr)
{
    std::vector<float> left ((size_t) blockSize), right ((size_t) blockSize);
    if (rightOut != nullptr) rightOut->assign (x.size(), 0.0f);

    for (size_t pos = 0; pos < x.size(); pos += (size_t) blockSize)
    {
        const size_t n = std::min ((size_t) blockSize, x.size() - pos);
        chain.process (x.data() + pos, left.data(), right.data(), (int) n, p);
        std::copy (left.begin(), left.begin() + (long) n, x.begin() + (long) pos);
        if (rightOut != nullptr) std::copy (right.begin(), right.begin() + (long) n, rightOut->begin() + (long) pos);
    }
}

std::vector<float> sine (double hz, double fs, double amp, size_t n)
{
    std::vector<float> x (n);
    for (size_t i = 0; i < n; ++i) x[i] = (float) (amp * std::sin (2.0 * pi * hz * (double) i / fs));
    return x;
}

/** Espectro de potencia de N = 2^order muestras (ventana rectangular: usar tonos en bins exactos). */
std::vector<double> powerSpectrum (const float* x, int order)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) (2 * n), 0.0f);
    std::copy (x, x + n, buf.begin());
    fft.performFrequencyOnlyForwardTransform (buf.data());
    std::vector<double> p ((size_t) (n / 2));
    for (int i = 0; i < n / 2; ++i) p[(size_t) i] = (double) buf[(size_t) i] * buf[(size_t) i];
    return p;
}

fx::ChainParams cleanParams()
{
    fx::ChainParams p;
    p.gateOn = false; p.odOn = false; p.ampOn = false; p.cabOn = false;
    return p;
}

//==============================================================================
void testToneStack()
{
    std::printf ("\n1. Control de tonos: modelo digital frente al circuito analógico\n");
    const fx::ToneStackValues* stacks[] { &fx::tonestacks::american, &fx::tonestacks::british, &fx::tonestacks::modern };
    const char* names[] { "american", "british", "modern" };
    double worst = 0.0, maxGain = 0.0;

    for (int s = 0; s < 3; ++s)
    {
        fx::ToneStack ts;
        ts.setValues (*stacks[s]);
        ts.prepare (192000.0);

        for (double b : { 0.0, 0.5, 1.0 })
            for (double m : { 0.0, 0.5, 1.0 })
                for (double t : { 0.0, 0.5, 1.0 })
                {
                    ts.setControls (b, m, t);

                    for (double f = 40.0; f < 10000.0; f *= 1.1)
                    {
                        const double a = std::abs (ts.analogResponse (f)), d = std::abs (ts.digitalResponse (f));
                        maxGain = std::max (maxGain, a);
                        if (a > 1e-4) worst = std::max (worst, std::abs (dB (a) - dB (d)));
                    }
                }

        ts.setControls (0.5, 0.5, 0.5);
        std::printf ("   %-9s todo a la mitad: 100 Hz %6.1f dB | 500 Hz %6.1f dB | 3 kHz %6.1f dB\n", names[s],
                     dB (std::abs (ts.digitalResponse (100.0))), dB (std::abs (ts.digitalResponse (500.0))),
                     dB (std::abs (ts.digitalResponse (3000.0))));
    }

    std::printf ("   error máximo digital/analógico: %.3f dB, ganancia máxima: %.3f\n", worst, maxGain);
    check (worst < 0.5, "el filtro digital sigue al circuito (< 0,5 dB entre 40 Hz y 10 kHz)");
    check (maxGain <= 1.0001, "red pasiva: nunca amplifica");

    // Proceso en el tiempo: la respuesta medida coincide con la calculada
    fx::ToneStack ts;
    ts.setValues (fx::tonestacks::british);
    ts.prepare (192000.0);
    ts.setControls (0.5, 0.5, 0.5);
    double peak = 0.0;
    for (int i = 0; i < 192000; ++i)
    {
        const double y = ts.process (std::sin (2.0 * pi * 1000.0 * i / 192000.0));
        if (i > 96000) peak = std::max (peak, std::abs (y));
    }
    check (std::abs (dB (peak) - dB (std::abs (ts.digitalResponse (1000.0)))) < 0.05, "la respuesta en el tiempo coincide con la calculada");
}

//==============================================================================
double thdOf (const std::vector<double>& spec, int fundamentalBin)
{
    double harm = 0.0;
    for (int b = 2 * fundamentalBin; b < (int) spec.size(); b += fundamentalBin) harm += spec[(size_t) b];
    return std::sqrt (harm / spec[(size_t) fundamentalBin]);
}

void testOverdrive()
{
    std::printf ("\n2. Overdrive: distorsión según el control Drive (1 kHz, 20 mV)\n");
    const double fs = 192000.0;
    const int order = 15, n = 1 << order, bin = 171;      // ~1002 Hz
    const double f0 = bin * fs / n;
    double thd[3] {}, last = 0.0;
    bool finite = true;
    int idx = 0;

    for (double drive : { 0.0, 0.5, 1.0 })
    {
        fx::Overdrive od;
        od.prepare (fs);
        od.setControls (drive, 0.5, 0.7);
        od.reset();
        auto x = sine (f0, fs, 0.02, (size_t) (2 * n));
        double peak = 0.0;
        for (auto& v : x) { v = (float) od.process (v); finite = finite && std::isfinite (v); peak = std::max (peak, (double) std::abs (v)); }
        thd[idx] = thdOf (powerSpectrum (x.data() + n, order), bin);
        std::printf ("   drive %.1f: THD %5.1f %%, pico de salida %.2f V\n", drive, 100.0 * thd[idx], peak);
        last = peak;
        ++idx;
    }

    check (finite, "salida siempre finita");
    check (thd[0] < thd[1] && thd[1] < thd[2], "la distorsión crece con Drive");
    check (thd[0] < 0.03 && thd[2] > 0.10, "al mínimo casi limpio (< 3 %), a tope satura (> 10 %)");
    check (last < 1.5, "el recorte de diodos limita el nivel");
}

//==============================================================================
void testAliasing()
{
    std::printf ("\n3. Aliasing audible (< 20 kHz): ampli High Gain, ganancia 0.8, sin pantalla\n");
    const double fs = 48000.0;
    const int order = 16, n = 1 << order;
    double alias[2][3] {};
    const int bins[2] { 1699, 6827 };       // ~1244 Hz (traste 24 de la 1ª cuerda) y ~5 kHz (caso extremo)

    for (int t = 0; t < 2; ++t)
        for (int q = 0; q < 3; ++q)
        {
            const int bin = bins[t];
            fx::GuitarChain chain;
            chain.prepare (fs, 64);
            auto p = cleanParams();
            p.ampOn = true; p.ampModel = 2; p.ampGain = 0.8f; p.quality = q;
            auto x = sine (bin * fs / n, fs, 0.25, (size_t) (2 * n));
            runChain (chain, x, p);
            const auto spec = powerSpectrum (x.data() + n, order);

            // Solo se cuenta la banda audible (< 20 kHz): por encima queda la banda de
            // transición del filtro de sobremuestreo, que no se oye.
            const int audible = (int) (20000.0 * n / fs);
            double total = 0.0, legit = 0.0;
            for (int b = 1; b < audible; ++b) total += spec[(size_t) b];
            for (int b = bin; b < audible - 1; b += bin)
                for (int k = -1; k <= 1; ++k) legit += spec[(size_t) (b + k)];

            alias[t][q] = 10.0 * std::log10 (std::max (total - legit, 1e-30) / total);
            std::printf ("   tono de %4.0f Hz, sobremuestreo %dx: aliasing %6.1f dB respecto a la señal\n",
                         bin * fs / n, 2 << q, alias[t][q]);
        }

    check (alias[0][1] < alias[0][0] && alias[0][2] < alias[0][1], "más sobremuestreo = menos aliasing");
    check (alias[0][1] < -50.0, "nota más aguda de la guitarra en calidad Normal (4x): aliasing por debajo de -50 dB");
}

//==============================================================================
void testLatency()
{
    std::printf ("\n4. Latencia añadida por el procesado\n");
    const double fs = 48000.0;
    bool ok = true;

    for (int q = 0; q < 3; ++q)
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        auto p = cleanParams();
        p.quality = q;
        std::vector<float> x (4096, 0.0f);
        x[1000] = 1.0f;
        runChain (chain, x, p);
        const int peak = (int) (std::max_element (x.begin(), x.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); }) - x.begin()) - 1000;
        const float reported = chain.getLatencySamples (q);
        std::printf ("   %dx: declarada %.2f muestras (%.3f ms), pico del impulso a %d muestras\n",
                     2 << q, reported, 1000.0 * reported / fs, peak);
        ok = ok && reported < 8.0f && std::abs (peak - reported) < 3.0f;
    }

    check (ok, "menos de 8 muestras (0,17 ms) y coincide con la declarada al DAW");
}

//==============================================================================
void testCpu()
{
    std::printf ("\n5. Coste de CPU (todos los pedales encendidos a la vez, bloques de 64 muestras a 48 kHz)\n");
    const double fs = 48000.0, seconds = 10.0;
    double factor[3] {};

    for (int q = 0; q < 3; ++q)
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        fx::ChainParams p;
        p.odOn = true; p.ampOn = true; p.ampModel = 2; p.ampGain = 0.7f; p.cabOn = true; p.gateOn = true; p.quality = q;
        p.compOn = p.distOn = p.fuzzOn = p.chorusOn = p.delayOn = p.reverbOn = true;
        auto x = sine (196.0, fs, 0.3, (size_t) (fs * seconds));
        const auto t0 = std::chrono::steady_clock::now();
        runChain (chain, x, p);
        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        factor[q] = seconds / elapsed;
        std::printf ("   %dx: %.1f veces más rápido que tiempo real (%.1f %% de un núcleo)\n", 2 << q, factor[q], 100.0 / factor[q]);
    }

    check (factor[1] > 4.0, "calidad Normal usa menos del 25 % de un núcleo en esta máquina");
}

//==============================================================================
void testTuner()
{
    std::printf ("\n6. Afinador\n");
    double worst = 0.0, worstHz = 0.0, worstFs = 0.0;
    bool detected = true;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (double f : { 41.20, 73.42, 82.41, 110.0, 146.83, 196.0, 246.94, 329.63, 659.26, 987.77 })
        {
            fx::PitchTracker tracker;
            tracker.prepare (fs);
            std::vector<float> x ((size_t) (fs * 0.5));
            for (size_t i = 0; i < x.size(); ++i)
            {
                double v = 0.0;
                for (int h = 1; h <= 6; ++h)
                    v += (h == 1 ? 0.6 : 1.0 / h) * std::sin (2.0 * pi * f * h * (double) i / fs + 0.7 * h);
                x[i] = (float) (0.1 * v);
            }
            for (size_t pos = 0; pos < x.size(); pos += 64) tracker.push (x.data() + pos, (int) std::min ((size_t) 64, x.size() - pos));
            const double got = tracker.detect();
            if (got <= 0.0) { detected = false; std::printf ("   %.2f Hz a %.0f Hz: no detectada\n", f, fs); continue; }
            const double cents = std::abs (1200.0 * std::log2 (got / f));
            if (cents > worst) { worst = cents; worstHz = f; worstFs = fs; }
        }

    fx::PitchTracker silent;
    silent.prepare (48000.0);
    std::vector<float> zeros (48000, 0.0f);
    silent.push (zeros.data(), (int) zeros.size());

    std::printf ("   error máximo: %.2f cents (en %.2f Hz a %.0f Hz; probado de 41 a 988 Hz, a 44,1 / 48 / 96 kHz)\n", worst, worstHz, worstFs);
    check (detected, "detecta todas las notas");
    check (worst < 2.0, "precisión mejor que 2 cents");
    check (silent.detect() == 0.0f, "en silencio no inventa notas");
}

//==============================================================================
void testGate()
{
    std::printf ("\n7. Puerta de ruido\n");
    const double fs = 48000.0;
    auto p = cleanParams();
    p.gateOn = true; p.gateThresholdDb = -60.0f;

    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    fx::GuitarChain chain;
    chain.prepare (fs, 64);
    std::vector<float> noise (48000);
    for (auto& v : noise) v = dist (rng) * 0.0001f;                       // ruido a -80 dB
    runChain (chain, noise, p);
    double peak = 0.0;
    for (size_t i = 24000; i < noise.size(); ++i) peak = std::max (peak, (double) std::abs (noise[i]));
    std::printf ("   ruido de -80 dB con umbral en -60 dB: sale a %.1f dB\n", dB (peak));
    check (peak < 1e-6, "cierra con el ruido de fondo");

    auto tone = sine (220.0, fs, 0.1, 48000);
    runChain (chain, tone, p);
    peak = 0.0;
    for (size_t i = 24000; i < tone.size(); ++i) peak = std::max (peak, (double) std::abs (tone[i]));
    check (std::abs (dB (peak) - dB (0.1)) < 0.2, "deja pasar la guitarra sin cambiar el nivel");
}

//==============================================================================
void testLevels()
{
    std::printf ("\n8. Niveles de salida por modelo (La 220 Hz a -12 dBFS, controles a la mitad)\n");
    const double fs = 48000.0;
    bool ok = true;

    for (int model = 0; model < fx::voicings::count; ++model)
        for (float gain : { 0.2f, 0.5f, 0.9f })
        {
            fx::GuitarChain chain;
            chain.prepare (fs, 64);
            fx::ChainParams p;
            p.gateOn = false; p.ampModel = model; p.ampGain = gain;
            const int order = 15, n = 1 << order, bin = 150;
            auto x = sine (bin * fs / n, fs, 0.25, (size_t) (2 * n));
            runChain (chain, x, p);
            double peak = 0.0, sum = 0.0;
            for (int i = n; i < 2 * n; ++i) { peak = std::max (peak, (double) std::abs (x[(size_t) i])); sum += (double) x[(size_t) i] * x[(size_t) i]; }
            const double thd = thdOf (powerSpectrum (x.data() + n, order), bin);
            std::printf ("   %-15s gain %.1f: pico %6.1f dBFS, RMS %6.1f dBFS, THD %5.1f %%\n",
                         fx::voicings::all[model]->name, gain, dB (peak), dB (std::sqrt (sum / n)), 100.0 * thd);
            ok = ok && peak < 1.0 && dB (peak) > -30.0;
        }

    check (ok, "todos los modelos salen entre -30 y 0 dBFS (sin saturar la salida)");
}

//==============================================================================
void testStability()
{
    std::printf ("\n9. Estabilidad con controles aleatorios y señal extrema\n");
    const double fs = 44100.0;
    fx::GuitarChain chain;
    chain.prepare (fs, 256);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    std::vector<float> block (256), left (256), right (256);
    bool finite = true;
    float peak = 0.0f;

    for (int b = 0; b < 4000; ++b)
    {
        fx::ChainParams p;
        p.inputGainDb = -24.0f + 48.0f * u (rng);
        p.gateOn = u (rng) > 0.5f; p.gateThresholdDb = -90.0f + 70.0f * u (rng);
        p.odOn = u (rng) > 0.3f; p.odDrive = u (rng); p.odTone = u (rng); p.odLevel = u (rng);
        p.ampOn = u (rng) > 0.2f; p.ampModel = (int) (u (rng) * 2.99f);
        p.ampGain = u (rng); p.ampBass = u (rng); p.ampMid = u (rng); p.ampTreble = u (rng); p.ampPresence = u (rng); p.ampMaster = u (rng);
        p.cabOn = u (rng) > 0.3f; p.quality = (int) (u (rng) * 2.99f);
        p.compOn = u (rng) > 0.5f; p.compSustain = u (rng); p.compAttack = u (rng); p.compLevel = u (rng);
        p.distOn = u (rng) > 0.5f; p.distGain = u (rng); p.distTone = u (rng); p.distLevel = u (rng);
        p.fuzzOn = u (rng) > 0.5f; p.fuzzAmount = u (rng); p.fuzzTone = u (rng); p.fuzzLevel = u (rng);
        p.chorusOn = u (rng) > 0.5f; p.chorusRate = u (rng); p.chorusDepth = u (rng); p.chorusMix = u (rng);
        p.delayOn = u (rng) > 0.5f; p.delayTimeMs = 20.0f + 1480.0f * u (rng); p.delayFeedback = u (rng); p.delayMix = u (rng);
        p.reverbOn = u (rng) > 0.5f; p.reverbSize = u (rng); p.reverbMix = u (rng);

        const int n = 1 + (int) (u (rng) * 255.0f);
        const float amp = (b % 50 == 0) ? 4.0f : 0.5f;
        for (int i = 0; i < n; ++i) block[(size_t) i] = amp * (2.0f * u (rng) - 1.0f);
        chain.process (block.data(), left.data(), right.data(), n, p);
        for (int i = 0; i < n; ++i)
        {
            finite = finite && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]);
            peak = std::max ({ peak, std::abs (left[(size_t) i]), std::abs (right[(size_t) i]) });
        }
    }

    std::printf ("   pico máximo de salida: %.2f\n", peak);
    check (finite, "nunca produce NaN ni infinitos");
    check (peak < 2000.0f, "la salida se mantiene acotada (sin realimentaciones que crezcan)");
}

//==============================================================================
void testPedals()
{
    std::printf ("\n10. Pedales nuevos\n");
    const double fs = 48000.0;
    const int order = 15, n = 1 << order, bin = 300;          // ~439 Hz
    const double f0 = bin * fs / n;

    // Compresor: una señal 20 dB más fuerte debe salir bastante menos de 20 dB más fuerte
    {
        double outDb[2] {};
        int i = 0;
        for (double amp : { 0.02, 0.2 })
        {
            fx::GuitarChain chain;
            chain.prepare (fs, 64);
            auto p = cleanParams();
            p.compOn = true; p.compSustain = 0.7f;
            auto x = sine (f0, fs, amp, (size_t) (2 * n));
            runChain (chain, x, p);
            double peak = 0.0;
            for (int k = n; k < 2 * n; ++k) peak = std::max (peak, (double) std::abs (x[(size_t) k]));
            outDb[i++] = dB (peak);
        }
        std::printf ("   compresor: +20 dB a la entrada dan +%.1f dB a la salida\n", outDb[1] - outDb[0]);
        check (outDb[1] - outDb[0] > 3.0 && outDb[1] - outDb[0] < 8.0, "compresor: relación cercana a 4:1");
    }

    // Distorsión y fuzz: saturan, están acotados y con poco aliasing audible
    struct Case { const char* name; bool dist; };
    for (auto c : { Case { "distorsión", true }, Case { "fuzz", false } })
    {
        double thd[2] {}, peak = 0.0, alias = 0.0;
        int i = 0;
        for (float amount : { 0.0f, 1.0f })
        {
            fx::GuitarChain chain;
            chain.prepare (fs, 64);
            auto p = cleanParams();
            p.distOn = c.dist; p.fuzzOn = ! c.dist; p.distGain = amount; p.fuzzAmount = amount; p.quality = 1;
            auto x = sine (f0, fs, 0.1, (size_t) (2 * n));
            runChain (chain, x, p);
            const auto spec = powerSpectrum (x.data() + n, order);
            thd[i++] = thdOf (spec, bin);
            for (int k = n; k < 2 * n; ++k) peak = std::max (peak, (double) std::abs (x[(size_t) k]));

            const int audible = (int) (20000.0 * n / fs);
            double total = 0.0, legit = 0.0;
            for (int b = 1; b < audible; ++b) total += spec[(size_t) b];
            for (int b = bin; b < audible - 1; b += bin) for (int k = -1; k <= 1; ++k) legit += spec[(size_t) (b + k)];
            alias = 10.0 * std::log10 (std::max (total - legit, 1e-30) / total);
        }
        std::printf ("   %s: THD %.1f %% al mínimo, %.1f %% al máximo; pico %.2f; aliasing audible a 4x %.1f dB\n",
                     c.name, 100.0 * thd[0], 100.0 * thd[1], peak, alias);
        check (thd[1] > thd[0] && thd[1] > 0.25, c.dist ? "distorsión: satura fuerte al máximo" : "fuzz: satura fuerte al máximo");
        check (peak < 1.0 && alias < -50.0, c.dist ? "distorsión: nivel acotado y aliasing < -50 dB" : "fuzz: nivel acotado y aliasing < -50 dB");
    }

    // Delay: un impulso se repite al tiempo indicado, con la mezcla y la realimentación indicadas
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        auto p = cleanParams();
        p.delayOn = true; p.delayTimeMs = 250.0f; p.delayMix = 0.5f; p.delayFeedback = 0.5f;
        std::vector<float> x (48000, 0.0f);
        x[100] = 1.0f;
        runChain (chain, x, p);
        const int latency = (int) std::lround (chain.getLatencySamples (p.quality));
        auto peakNear = [&] (int centre) { float m = 0.0f; int at = 0; for (int k = centre - 40; k < centre + 40; ++k) if (std::abs (x[(size_t) k]) > m) { m = std::abs (x[(size_t) k]); at = k; } return std::pair<float, int> (m, at); };
        const auto dry = peakNear (100 + latency), first = peakNear (100 + latency + 12000), second = peakNear (100 + latency + 24000);
        std::printf ("   delay de 250 ms: primera repetición a %.1f ms (nivel %.2f), segunda a nivel %.2f\n",
                     1000.0 * (first.second - dry.second) / fs, first.first / dry.first, second.first / dry.first);
        check (std::abs (first.second - dry.second - 12000) <= 2, "delay: la repetición llega al tiempo exacto");
        check (first.first / dry.first > 0.3 && second.first < first.first && second.first > 0.0f, "delay: las repeticiones decaen");
    }

    // Chorus y reverb: estéreo real, y la reverb deja cola que se apaga
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        auto p = cleanParams();
        p.chorusOn = true;
        auto x = sine (f0, fs, 0.2, 48000);
        std::vector<float> right;
        runChain (chain, x, p, 64, &right);
        double diff = 0.0, peak = 0.0;
        for (size_t k = 24000; k < x.size(); ++k) { diff = std::max (diff, (double) std::abs (x[k] - right[k])); peak = std::max (peak, (double) std::abs (x[k])); }
        std::printf ("   chorus: diferencia máxima entre canales %.3f, pico %.3f\n", diff, peak);
        check (diff > 0.01 && peak < 0.3, "chorus: salida estéreo sin subir el nivel");
    }
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        auto p = cleanParams();
        p.reverbOn = true; p.reverbSize = 0.6f; p.reverbMix = 0.5f;
        std::vector<float> x (4 * 48000, 0.0f);
        for (int k = 0; k < 4800; ++k) x[(size_t) k] = (float) (0.3 * std::sin (2.0 * pi * 440.0 * k / fs));
        runChain (chain, x, p);
        auto rms = [&] (int from, int to) { double sum = 0.0; for (int k = from; k < to; ++k) sum += (double) x[(size_t) k] * x[(size_t) k]; return std::sqrt (sum / (to - from)); };
        const double early = rms (9600, 19200), late = rms (3 * 48000, 4 * 48000);
        std::printf ("   reverb: cola a 0,2-0,4 s %.1f dB; a 3-4 s %.1f dB\n", dB (early), dB (late));
        check (early > 1e-3 && late < 0.1 * early, "reverb: deja cola y se apaga");
    }
}

//==============================================================================
/** Cuerda pulsada sintética (Karplus-Strong) para oír los modelos sin una guitarra real. */
std::vector<float> syntheticRiff (double fs)
{
    struct Note { double start, hz; };
    std::vector<Note> notes;
    const double chords[][3] { { 82.41, 123.47, 164.81 }, { 82.41, 123.47, 164.81 }, { 98.0, 146.83, 196.0 },
                               { 110.0, 164.81, 220.0 }, { 82.41, 123.47, 164.81 }, { 73.42, 110.0, 146.83 } };
    double t = 0.1;
    for (auto& c : chords) { for (double hz : c) notes.push_back ({ t, hz }); t += 0.6; }
    for (double hz : { 329.63, 392.0, 440.0, 493.88, 440.0, 392.0, 329.63 }) { notes.push_back ({ t, hz }); t += 0.3; }

    std::vector<float> out ((size_t) (fs * (t + 1.5)), 0.0f);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> dist (-1.0f, 1.0f);

    for (auto& note : notes)
    {
        const double period = fs / note.hz;
        const int len = (int) period;
        const double frac = period - len;
        std::vector<float> line ((size_t) len + 1);
        float lp = 0.0f;
        for (auto& v : line) { lp += 0.35f * (dist (rng) - lp); v = lp; }      // púa: ruido filtrado
        size_t idx = 0;
        float prev = 0.0f;

        for (size_t i = (size_t) (note.start * fs); i < out.size(); ++i)
        {
            const size_t next = (idx + 1) % line.size();
            const float y = (float) ((1.0 - frac) * line[idx] + frac * line[next]);
            out[i] += 0.5f * y;
            line[idx] = 0.997f * 0.5f * (y + prev);
            prev = y;
            idx = next;
        }
    }

    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    for (float& v : out) v *= 0.3f / peak;
    return out;
}

void writeWav (const juce::File& file, const std::vector<float>& left, const std::vector<float>& right, double fs)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr) { std::printf ("   no se pudo escribir %s\n", file.getFullPathName().toRawUTF8()); return; }
    auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (fs).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr) return;
    const float* channels[] { left.data(), right.data() };
    writer->writeFromFloatArrays (channels, 2, (int) left.size());
    std::printf ("   escrito %s\n", file.getFullPathName().toRawUTF8());
}

void renderDemos (const juce::File& dir)
{
    std::printf ("\nDemostraciones de audio (guitarra sintética)\n");
    dir.createDirectory();
    const double fs = 48000.0;
    const auto dry = syntheticRiff (fs);
    writeWav (dir.getChildFile ("0_entrada_sin_procesar.wav"), dry, dry, fs);

    auto base = [] (int model, float gain) { fx::ChainParams p; p.ampModel = model; p.ampGain = gain; p.quality = 2; return p; };
    struct Demo { const char* file; fx::ChainParams p; };
    std::vector<Demo> demos;
    demos.push_back ({ "1_clean_american.wav", base (0, 0.45f) });
    demos.push_back ({ "2_british_crunch.wav", base (1, 0.6f) });
    { auto p = base (1, 0.6f); p.odOn = true; p.odDrive = 0.6f; demos.push_back ({ "3_british_crunch_con_overdrive.wav", p }); }
    demos.push_back ({ "4_high_gain.wav", base (2, 0.65f) });
    { auto p = base (0, 0.4f); p.compOn = true; p.chorusOn = true; p.reverbOn = true; demos.push_back ({ "5_clean_compresor_chorus_reverb.wav", p }); }
    { auto p = base (0, 0.35f); p.distOn = true; p.distGain = 0.6f; demos.push_back ({ "6_clean_con_distorsion.wav", p }); }
    { auto p = base (0, 0.35f); p.fuzzOn = true; demos.push_back ({ "7_clean_con_fuzz.wav", p }); }
    { auto p = base (2, 0.6f); p.delayOn = true; p.reverbOn = true; p.reverbMix = 0.2f; demos.push_back ({ "8_high_gain_delay_reverb.wav", p }); }

    for (auto& d : demos)
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        auto left = dry;
        std::vector<float> right;
        runChain (chain, left, d.p, 64, &right);
        writeWav (dir.getChildFile (d.file), left, right, fs);
    }
}
}

int main (int argc, char** argv)
{
    juce::ScopedNoDenormals noDenormals;
    std::printf ("EffectSuite: medidas del motor de audio\n");

    testToneStack();
    testOverdrive();
    testAliasing();
    testLatency();
    testCpu();
    testTuner();
    testGate();
    testLevels();
    testStability();
    testPedals();

    if (argc == 3 && juce::String (argv[1]) == "--render")
        renderDemos (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));

    std::printf ("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASAN" : "HAY PRUEBAS QUE FALLAN");
    return failures == 0 ? 0 : 1;
}
