// Pruebas de la capa de plugin: parámetros, guardado y carga de estado, pedalera y presets.
// Se ejecutan sin ventana ni tarjeta de sonido. Código de salida != 0 si alguna falla.

#include <cstdio>
#include "PluginProcessor.h"

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("   [%s] %s\n", ok ? " OK " : "FALLO", what);
    if (! ok) ++failures;
}

struct Rendered { double rmsDb = -200.0, peak = 0.0; bool finite = true; std::vector<float> left; };

/** Pasa unas notas por el plugin como lo haría un host y mide la salida de los últimos 0,8 s. */
Rendered render (EffectSuiteProcessor& plugin, double hz = 196.0, double seconds = 2.0, int blockSize = 128)
{
    const double fs = 48000.0;
    plugin.setPlayConfigDetails (2, 2, fs, blockSize);
    plugin.prepareToPlay (fs, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    Rendered result;
    const int total = (int) (fs * seconds);
    double sum = 0.0;
    int counted = 0;

    for (int pos = 0; pos < total; pos += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            // Notas que se apagan, como una cuerda pulsada: una cada 0,4 s, con tres armónicos
            const double t = (pos + i) / fs, sinceAttack = std::fmod (t, 0.4);
            const double phase = 2.0 * juce::MathConstants<double>::pi * hz * t;
            const double string = std::sin (phase) + 0.5 * std::sin (2.0 * phase) + 0.25 * std::sin (3.0 * phase);
            const float v = (float) (0.17 * string * std::exp (-sinceAttack / 0.25) * std::min (1.0, sinceAttack / 0.002));
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }

        plugin.processBlock (buffer, midi);

        for (int i = 0; i < blockSize; ++i)
        {
            const float l = buffer.getSample (0, i), r = buffer.getSample (1, i);
            result.finite = result.finite && std::isfinite (l) && std::isfinite (r);
            result.left.push_back (l);

            if (pos + i >= total - 38400)
            {
                sum += (double) l * l;
                ++counted;
                result.peak = std::max ({ result.peak, (double) std::abs (l), (double) std::abs (r) });
            }
        }
    }

    result.rmsDb = 10.0 * std::log10 (std::max (sum / std::max (1, counted), 1e-20));
    return result;
}

float value (EffectSuiteProcessor& plugin, const char* id) { return plugin.apvts.getRawParameterValue (id)->load(); }

void set (EffectSuiteProcessor& plugin, const char* id, float real)
{
    auto* p = plugin.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (real));
}

juce::MemoryBlock save (EffectSuiteProcessor& plugin)
{
    juce::MemoryBlock block;
    plugin.getStateInformation (block);
    return block;
}

double difference (const std::vector<float>& a, const std::vector<float>& b)
{
    double worst = 0.0;
    for (size_t i = a.size() / 2; i < std::min (a.size(), b.size()); ++i) worst = std::max (worst, (double) std::abs (a[i] - b[i]));
    return worst;
}

//==============================================================================
void testState()
{
    std::printf ("\n1. Guardar y recuperar el estado\n");
    EffectSuiteProcessor a;
    set (a, "ampModel", 4); set (a, "ampGain", 0.73f); set (a, "delayTime", 612.0f); set (a, "eq6k4", -7.5f);
    a.setOnBoard ("fuzz", true); set (a, "fuzzOn", 1); set (a, "fuzzModel", 2);
    a.setOnBoard ("phaser", true);
    a.movePedal ("fuzz", 0);
    const auto block = save (a);

    EffectSuiteProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());

    bool same = true;
    for (auto* p : a.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            same = same && std::abs (ranged->getValue() - b.apvts.getParameter (ranged->getParameterID())->getValue()) < 1e-6f;

    check (same, "todos los parámetros vuelven con el mismo valor");
    check (a.getBoardRow (false) == b.getBoardRow (false) && a.getBoardRow (true) == b.getBoardRow (true),
           "la pedalera vuelve con los mismos pedales y en el mismo orden");
    check (difference (render (a).left, render (b).left) < 1e-4, "suena igual antes y después de recargar");

    // Estados dañados o de otra cosa no rompen nada
    EffectSuiteProcessor c;
    const char garbage[] = "esto no es un estado";
    c.setStateInformation (garbage, (int) sizeof (garbage));
    c.setStateInformation (nullptr, 0);
    check (render (c).finite, "un estado ilegible se ignora");

    // Un parámetro no numérico en un estado dañado no deja el plugin mudo
    {
        EffectSuiteProcessor source, target;
        auto tree = source.apvts.copyState();
        for (auto child : tree)
            if (child.getProperty ("id").toString() == "ampGain" || child.getProperty ("id").toString() == "outputLevel")
                child.setProperty ("value", std::numeric_limits<float>::quiet_NaN(), nullptr);
        juce::MemoryBlock nanBlock;
        if (auto xml = tree.createXml()) juce::AudioProcessor::copyXmlToBinary (*xml, nanBlock);
        target.setStateInformation (nanBlock.getData(), (int) nanBlock.getSize());
        const auto out = render (target);
        check (out.finite && out.rmsDb > -40.0 && std::isfinite (value (target, "ampGain")), "un valor no numérico en el estado se sustituye y el plugin sigue sonando");
    }

    // Un IR o una captura que ya no existen caen a la pantalla y al ampli integrados
    EffectSuiteProcessor d;
    auto state = d.apvts.copyState();
    state.setProperty ("irFile", "/no/existe/pantalla.wav", nullptr);
    state.setProperty ("namFile", "/no/existe/captura.nam", nullptr);
    juce::MemoryBlock missing;
    if (auto xml = state.createXml()) juce::AudioProcessor::copyXmlToBinary (*xml, missing);
    d.setStateInformation (missing.getData(), (int) missing.getSize());
    check (d.getImpulseResponseName().isEmpty() && d.getNeuralModelName().isEmpty() && render (d).finite,
           "si faltan el IR o la captura guardados, se usan los integrados");
}

//==============================================================================
void testBoard()
{
    std::printf ("\n2. Pedalera: añadir, quitar y reordenar\n");
    EffectSuiteProcessor plugin;
    check (plugin.getBoardRow (false) == juce::StringArray ({ "gate", "comp", "od" })
           && plugin.getBoardRow (true) == juce::StringArray ({ "chorus", "delay", "reverb" }), "pedalera inicial");

    plugin.setOnBoard ("fuzz", true);
    plugin.setOnBoard ("eq", true);
    check (plugin.getBoardRow (false).contains ("fuzz") && plugin.getBoardRow (true).contains ("eq"), "añadir coloca cada pedal en su fila");

    set (plugin, "odOn", 1);
    plugin.setOnBoard ("od", false);
    check (! plugin.isOnBoard ("od") && value (plugin, "odOn") < 0.5f, "quitar un pedal también lo apaga");

    plugin.movePedal ("fuzz", 0);
    check (plugin.getBoardRow (false) == juce::StringArray ({ "fuzz", "gate", "comp" }), "mover un pedal al principio de su fila");
    plugin.movePedal ("fuzz", 99);
    check (plugin.getBoardRow (false) == juce::StringArray ({ "gate", "comp", "fuzz" }), "una posición fuera de rango lo deja al final");
    plugin.movePedal ("eq", 1);
    check (plugin.getBoardRow (true) == juce::StringArray ({ "chorus", "eq", "delay", "reverb" }) && plugin.getBoardRow (false).size() == 3,
           "mover en una fila no altera la otra");

    // Un pedal encendido desde fuera (automatización) aparece en la pedalera
    EffectSuiteProcessor automated;
    const int revision = automated.getBoardRevision();
    set (automated, "phaserOn", 1);
    automated.applyPendingMidi();       // lo que hace el temporizador
    check (automated.getBoardRow (false).contains ("phaser") && automated.getBoardRevision() != revision,
           "un pedal encendido por automatización aparece en la pedalera");

    // El orden cambia el sonido de verdad: compresor antes o después del overdrive
    EffectSuiteProcessor x, y;
    for (auto* p : { &x, &y })
    {
        p->setOnBoard ("od", true); set (*p, "odOn", 1); set (*p, "odDrive", 0.8f);
        set (*p, "compOn", 1); set (*p, "compSustain", 0.9f); set (*p, "gateOn", 0);
    }
    y.movePedal ("od", 0);
    check (x.getBoardRow (false).indexOf ("comp") < x.getBoardRow (false).indexOf ("od")
           && y.getBoardRow (false).indexOf ("od") < y.getBoardRow (false).indexOf ("comp")
           && difference (render (x).left, render (y).left) > 1e-3, "cambiar el orden de dos pedales cambia el sonido");
}

//==============================================================================
void testPresets()
{
    std::printf ("\n3. Presets\n");
    const auto names = EffectSuiteProcessor::getFactoryPresetNames();
    std::printf ("   %d presets de fábrica\n", names.size());

    double lo = 1e9, hi = -1e9;
    bool allOk = names.size() >= 12;

    for (auto& name : names)
    {
        EffectSuiteProcessor plugin;
        set (plugin, "inputGain", 6.0f); set (plugin, "quality", 0);
        const bool loaded = plugin.loadFactoryPreset (name);
        const bool bass = name.startsWith ("Bajo");
        const auto out = render (plugin, bass ? 98.0 : 196.0);
        const bool ok = loaded && out.finite && out.rmsDb > -40.0 && out.peak <= 1.0
                        && plugin.getCurrentPresetName() == name
                        && std::abs (value (plugin, "inputGain") - 6.0f) < 0.01f && value (plugin, "quality") < 0.5f;
        std::printf ("   %-32s %6.1f dBFS RMS, pico %5.1f dBFS%s\n", name.toRawUTF8(), out.rmsDb,
                     20.0 * std::log10 (std::max (out.peak, 1e-9)), ok ? "" : "   <-- FALLO");
        allOk = allOk && ok;
        lo = std::min (lo, out.rmsDb); hi = std::max (hi, out.rmsDb);
    }

    check (allOk, "todos cargan, suenan, no saturan la salida y respetan los ajustes del equipo");
    std::printf ("   diferencia de volumen entre presets: %.1f dB\n", hi - lo);
    check (hi - lo < 10.0, "volumen parejo entre presets (menos de 10 dB entre el más flojo, el auto-wah, y el más fuerte)");
    check (! EffectSuiteProcessor().loadFactoryPreset ("No existe"), "un nombre inexistente se rechaza");

    // Presets de usuario en una carpeta temporal
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("effectsuite_test_presets");
    folder.deleteRecursively();

    EffectSuiteProcessor a;
    a.setUserPresetFolder (folder);
    a.loadFactoryPreset (names[4]);
    set (a, "ampGain", 0.81f);
    a.setOnBoard ("flanger", true); set (a, "flangerOn", 1);
    a.movePedal ("flanger", 0);
    check (a.saveUserPreset ("Mi sonido / prueba") && a.getUserPresetNames().size() == 1, "guardar un preset de usuario (con caracteres no válidos en el nombre)");
    const auto saved = a.getUserPresetNames()[0];
    const auto reference = render (a).left;

    EffectSuiteProcessor b;
    b.setUserPresetFolder (folder);
    set (b, "outputLevel", -6.0f);
    check (b.loadUserPreset (saved) && std::abs (value (b, "ampGain") - 0.81f) < 1e-4f && b.getBoardRow (true) == a.getBoardRow (true)
           && std::abs (value (b, "outputLevel") + 6.0f) < 0.01f && b.getCurrentPresetName() == saved,
           "cargarlo en otra instancia recupera el sonido y la pedalera sin tocar el nivel de salida");
    set (b, "outputLevel", 0.0f);
    check (difference (reference, render (b).left) < 1e-4, "el preset cargado suena igual que el guardado");

    // Cargar un preset no arrastra restos del anterior
    b.loadFactoryPreset (names[0]);
    EffectSuiteProcessor fresh;
    fresh.loadFactoryPreset (names[0]);
    bool same = true;
    for (auto* p : fresh.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            same = same && std::abs (ranged->getValue() - b.apvts.getParameter (ranged->getParameterID())->getValue()) < 1e-6f;
    check (same && b.getBoardRow (false) == fresh.getBoardRow (false) && b.getBoardRow (true) == fresh.getBoardRow (true),
           "cargar un preset tras otro da lo mismo que cargarlo en limpio");

    folder.getChildFile ("Vacio.espreset").replaceWithText ("<EffectSuite/>");
    check (! b.loadUserPreset ("Vacio"), "un archivo de preset sin contenido se rechaza");
    folder.getChildFile ("Vacio.espreset").deleteFile();

    check (b.deleteUserPreset (saved) && b.getUserPresetNames().isEmpty() && ! b.loadUserPreset (saved), "borrar un preset de usuario");
    folder.deleteRecursively();
}

//==============================================================================
void testHostBehaviour()
{
    std::printf ("\n4. Comportamiento en un host\n");
    EffectSuiteProcessor plugin;
    const double fs = 44100.0;
    bool ok = true;

    // Mono y estéreo, y tamaños de bloque distintos del anunciado
    for (int channels : { 1, 2 })
    {
        plugin.setPlayConfigDetails (channels, channels, fs, 256);
        plugin.prepareToPlay (fs, 256);
        juce::MidiBuffer midi;

        for (int size : { 256, 1, 37, 256, 1024, 0 })
        {
            juce::AudioBuffer<float> buffer (channels, size);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i) buffer.setSample (ch, i, 0.1f * (float) std::sin (0.03 * i));
            plugin.processBlock (buffer, midi);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i) ok = ok && std::isfinite (buffer.getSample (ch, i));
        }
    }
    check (ok, "mono y estéreo, con bloques de 0, 1, 37 y 1024 muestras");

    // Captura neuronal a través del plugin
    const juce::File capture (juce::String (EFFECTSUITE_TEST_DATA) + "/wavenet_a1_standard.nam");
    EffectSuiteProcessor withCapture;
    const auto before = render (withCapture).left;
    const auto error = withCapture.loadNeuralModel (capture);
    const auto after = render (withCapture);
    check (error.isEmpty() && withCapture.getNeuralModelName() == "wavenet_a1_standard" && after.finite && difference (before, after.left) > 1e-3,
           "cargar una captura neuronal cambia el sonido");
    const auto block = save (withCapture);
    EffectSuiteProcessor restored;
    restored.setStateInformation (block.getData(), (int) block.getSize());
    check (restored.getNeuralModelName() == "wavenet_a1_standard", "la captura se recupera al recargar el proyecto");
    check (withCapture.loadNeuralModel (juce::File ("/no/existe.nam")).isNotEmpty(), "una captura inexistente devuelve un error");

    // MIDI: pedal de expresión para el wah y cambio de preset
    EffectSuiteProcessor midiPlugin;
    midiPlugin.setUserPresetFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("effectsuite_test_empty"));
    midiPlugin.setPlayConfigDetails (2, 2, 48000.0, 128);
    midiPlugin.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> buffer (2, 128);
    buffer.clear();
    juce::MidiBuffer messages;
    messages.addEvent (juce::MidiMessage::controllerEvent (1, 11, 96), 0);
    midiPlugin.processBlock (buffer, messages);
    midiPlugin.applyPendingMidi();
    check (std::abs (value (midiPlugin, "wahPosition") - 96.0f / 127.0f) < 0.01f, "el controlador MIDI 11 mueve la posición del wah");
    messages.clear();
    messages.addEvent (juce::MidiMessage::programChange (1, 5), 10);
    midiPlugin.processBlock (buffer, messages);
    midiPlugin.applyPendingMidi();
    check (midiPlugin.getCurrentPresetName() == EffectSuiteProcessor::getFactoryPresetNames()[5], "un cambio de programa MIDI carga el preset de ese número");
    messages.clear();
    messages.addEvent (juce::MidiMessage::programChange (1, 120), 0);
    midiPlugin.processBlock (buffer, messages);
    midiPlugin.applyPendingMidi();
    check (midiPlugin.getCurrentPresetName() == EffectSuiteProcessor::getFactoryPresetNames()[5], "un número de programa sin preset no cambia nada");
}
}

//==============================================================================
void testRecording()
{
    std::printf ("\n5. Grabación de tomas y metrónomo\n");
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("effectsuite_test_takes");
    folder.deleteRecursively();

    EffectSuiteProcessor plugin;
    plugin.setRecordingFolder (folder);
    check (plugin.startRecording().isNotEmpty() && ! plugin.isRecording(), "sin audio en marcha no se puede grabar y se explica por qué");

    const double fs = 48000.0;
    const int block = 128;
    plugin.setPlayConfigDetails (2, 2, fs, block);
    plugin.prepareToPlay (fs, block);
    set (plugin, "metroOn", 1); set (plugin, "metroBpm", 240.0f); set (plugin, "metroLevel", 1.0f);
    set (plugin, "gateOn", 0);

    const auto error = plugin.startRecording();
    check (error.isEmpty() && plugin.isRecording(), "empezar una toma");

    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    std::vector<float> input, heard;

    for (int pos = 0; pos < 48000; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const float v = (float) (0.1 * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (pos + i) / fs));
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, 0.0f);
            input.push_back (v);
        }

        plugin.processBlock (buffer, midi);
        for (int i = 0; i < block; ++i) heard.push_back (buffer.getSample (0, i));
    }

    const double seconds = plugin.getRecordedSeconds();
    plugin.stopRecording();
    check (! plugin.isRecording() && std::abs (seconds - (double) input.size() / fs) < 0.01, "parar la toma; el contador marca la duración real");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> wet (formats.createReaderFor (plugin.getLastTake()));
    std::unique_ptr<juce::AudioFormatReader> dry (formats.createReaderFor (plugin.getLastDirectTake()));
    const bool readable = wet != nullptr && dry != nullptr;
    check (readable && wet->numChannels == 2 && dry->numChannels == 1 && wet->bitsPerSample == 24 && dry->bitsPerSample == 24
           && wet->lengthInSamples == (juce::int64) input.size() && dry->lengthInSamples == (juce::int64) input.size()
           && std::abs (wet->sampleRate - fs) < 1.0,
           "quedan dos WAV de 24 bits: procesado en estéreo y guitarra limpia en mono, con la duración exacta");

    if (readable)
    {
        juce::AudioBuffer<float> wetData (2, (int) input.size()), dryData (1, (int) input.size());
        wet->read (&wetData, 0, (int) input.size(), 0, true, true);
        dry->read (&dryData, 0, (int) input.size(), 0, true, false);

        double dryError = 0.0, wetLevel = 0.0, clickDifference = 0.0;
        for (size_t i = 0; i < input.size(); ++i)
        {
            dryError = std::max (dryError, (double) std::abs (dryData.getSample (0, (int) i) - input[i]));
            wetLevel = std::max (wetLevel, (double) std::abs (wetData.getSample (0, (int) i)));
            clickDifference = std::max (clickDifference, (double) std::abs (heard[i] - wetData.getSample (0, (int) i)));
        }

        check (dryError < 1e-4, "la guitarra limpia grabada es exactamente lo que entró");
        check (wetLevel > 0.01, "el sonido procesado grabado no está vacío");
        check (clickDifference > 0.05, "el metrónomo se oye pero no queda grabado en la toma");
        double heardPeak = 0.0;
        for (float v : heard) heardPeak = std::max (heardPeak, (double) std::abs (v));
        check (heardPeak <= 1.0, "el metrónomo no saca la salida de 0 dBFS");
        check (plugin.getRecordingDropoutSeconds() == 0.0, "la toma no tiene huecos");
    }

    // Dos tomas seguidas no se pisan
    plugin.startRecording();
    const auto second = plugin.getLastTake();
    plugin.stopRecording();
    check (folder.findChildFiles (juce::File::findFiles, false, "*.wav").size() == 4 && second.existsAsFile(), "cada toma tiene sus propios archivos");
    folder.deleteRecursively();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("EffectSuite: pruebas de la capa de plugin\n");

    testState();
    testBoard();
    testPresets();
    testHostBehaviour();
    testRecording();

    std::printf ("\n%s\n", failures == 0 ? "TODAS LAS PRUEBAS PASAN" : "HAY PRUEBAS QUE FALLAN");
    return failures == 0 ? 0 : 1;
}
