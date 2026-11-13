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

/** Pasa una señal por la cadena en bloques de tamaño blockSize. */
void runChain (fx::GuitarChain& chain, std::vector<float>& x, const fx::ChainParams& p, int blockSize = 64)
{
    for (size_t pos = 0; pos < x.size(); pos += (size_t) blockSize)
        chain.process (x.data() + pos, (int) std::min ((size_t) blockSize, x.size() - pos), p);
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
    std::printf ("\n5. Coste de CPU (cadena completa, bloques de 64 muestras a 48 kHz)\n");
    const double fs = 48000.0, seconds = 10.0;
    double factor[3] {};

    for (int q = 0; q < 3; ++q)
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        fx::ChainParams p;
        p.odOn = true; p.ampOn = true; p.ampModel = 2; p.ampGain = 0.7f; p.cabOn = true; p.gateOn = true; p.quality = q;
        auto x = sine (196.0, fs, 0.3, (size_t) (fs * seconds));
        const auto t0 = std::chrono::steady_clock::now();
        runChain (chain, x, p);
        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        factor[q] = seconds / elapsed;
        std::printf ("   %dx: %.1f veces más rápido que tiempo real (%.1f %% de un núcleo)\n", 2 << q, factor[q], 100.0 / factor[q]);
    }

    check (factor[1] > 5.0, "calidad Normal usa menos del 20 % de un núcleo en esta máquina");
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
    std::vector<float> block (256);
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

        const int n = 1 + (int) (u (rng) * 255.0f);
        const float amp = (b % 50 == 0) ? 4.0f : 0.5f;      // con +24 dB de entrada y todo apagado, el pico lineal es 63
        for (int i = 0; i < n; ++i) block[(size_t) i] = amp * (2.0f * u (rng) - 1.0f);
        chain.process (block.data(), n, p);
        for (int i = 0; i < n; ++i) { finite = finite && std::isfinite (block[(size_t) i]); peak = std::max (peak, std::abs (block[(size_t) i])); }
    }

    std::printf ("   pico máximo de salida: %.2f\n", peak);
    check (finite, "nunca produce NaN ni infinitos");
    check (peak < 70.0f, "la salida nunca supera la ganancia lineal máxima");
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

void writeWav (const juce::File& file, const std::vector<float>& x, double fs)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr) { std::printf ("   no se pudo escribir %s\n", file.getFullPathName().toRawUTF8()); return; }
    auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (fs).withNumChannels (1).withBitsPerSample (24));
    if (writer == nullptr) return;
    const float* channels[] { x.data() };
    writer->writeFromFloatArrays (channels, 1, (int) x.size());
    std::printf ("   escrito %s\n", file.getFullPathName().toRawUTF8());
}

void renderDemos (const juce::File& dir)
{
    std::printf ("\nDemostraciones de audio (guitarra sintética)\n");
    dir.createDirectory();
    const double fs = 48000.0;
    const auto dry = syntheticRiff (fs);
    writeWav (dir.getChildFile ("0_entrada_sin_procesar.wav"), dry, fs);

    struct Demo { const char* file; int model; float gain; bool od; };
    for (auto d : { Demo { "1_clean_american.wav", 0, 0.45f, false }, Demo { "2_british_crunch.wav", 1, 0.6f, false },
                    Demo { "3_british_crunch_con_overdrive.wav", 1, 0.6f, true }, Demo { "4_high_gain.wav", 2, 0.65f, false } })
    {
        fx::GuitarChain chain;
        chain.prepare (fs, 64);
        fx::ChainParams p;
        p.ampModel = d.model; p.ampGain = d.gain; p.odOn = d.od; p.odDrive = 0.6f; p.quality = 2;
        auto x = dry;
        runChain (chain, x, p);
        writeWav (dir.getChildFile (d.file), x, fs);
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

    if (argc == 3 && juce::String (argv[1]) == "--render")
        renderDemos (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));

    std::printf ("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASAN" : "HAY PRUEBAS QUE FALLAN");
    return failures == 0 ? 0 : 1;
}
