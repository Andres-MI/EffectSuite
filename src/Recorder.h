#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

/** Graba a disco, a la vez, el sonido procesado (estéreo) y la guitarra limpia (mono, "DI").

    El hilo de audio solo copia muestras a una cola; un hilo aparte las escribe en los
    archivos WAV de 24 bits. La guitarra limpia permite volver a pasar la misma toma por
    otro ampli más adelante.
*/
class Recorder
{
public:
    Recorder() { writerThread.startThread(); }
    ~Recorder() { stop(); writerThread.stopThread (2000); }

    /** Empieza una toma en la carpeta indicada. Devuelve una cadena vacía, o el motivo del error. Hilo de mensajes. */
    juce::String start (const juce::File& folder, double sampleRate)
    {
        stop();

        if (sampleRate <= 0.0)
            return "El audio no está en marcha";

        if (! folder.createDirectory())
            return "No se pudo crear la carpeta de grabaciones";

        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
        const auto wetFile = folder.getNonexistentChildFile (stamp + " toma", ".wav");
        const auto dryFile = folder.getChildFile (wetFile.getFileNameWithoutExtension() + " DI.wav");

        auto wetWriter = createWriter (wetFile, sampleRate, 2);
        auto dryWriter = createWriter (dryFile, sampleRate, 1);

        if (wetWriter == nullptr || dryWriter == nullptr)
        {
            wetWriter.reset();
            dryWriter.reset();
            wetFile.deleteFile();
            dryFile.deleteFile();
            return "No se pudieron crear los archivos de la toma";
        }

        lastWetFile = wetFile;
        lastDryFile = dryFile;
        rate = sampleRate;
        samplesWritten.store (0);
        samplesDropped.store (0);

        const juce::SpinLock::ScopedLockType lock (writerLock);
        wet = std::move (wetWriter);
        dry = std::move (dryWriter);
        active.store (true);
        return {};
    }

    /** Termina la toma y cierra los archivos. Hilo de mensajes. */
    void stop()
    {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> oldWet, oldDry;

        {
            const juce::SpinLock::ScopedLockType lock (writerLock);
            active.store (false);
            oldWet = std::move (wet);
            oldDry = std::move (dry);
        }

        // Al destruirse fuera del cerrojo vuelcan lo que quede en la cola y cierran el archivo
        oldWet.reset();
        oldDry.reset();
    }

    bool isRecording() const noexcept { return active.load (std::memory_order_relaxed); }
    double getSecondsRecorded() const noexcept { return rate > 0.0 ? (double) samplesWritten.load() / rate : 0.0; }
    /** Segundos que no se pudieron grabar en la última toma porque el disco no dio abasto. */
    double getSecondsDropped() const noexcept { return rate > 0.0 ? (double) samplesDropped.load() / rate : 0.0; }
    double getSampleRate() const noexcept { return rate; }
    juce::File getLastTake() const { return lastWetFile; }
    juce::File getLastDirectTake() const { return lastDryFile; }

    /** Hilo de audio. No espera nunca: si se está arrancando o parando, se salta el bloque. */
    void write (const float* direct, const float* left, const float* right, int numSamples) noexcept
    {
        if (! active.load (std::memory_order_relaxed))
            return;

        const juce::SpinLock::ScopedTryLockType lock (writerLock);

        if (! lock.isLocked() || wet == nullptr || dry == nullptr)
            return;

        const float* stereo[] { left, right };
        const float* mono[] { direct };
        // Los dos archivos avanzan juntos o no avanza ninguno: así no se desalinean si el disco se atasca
        if (wet->write (stereo, numSamples))
        {
            if (dry->write (mono, numSamples))
            {
                samplesWritten.fetch_add (numSamples, std::memory_order_relaxed);
                return;
            }
        }

        samplesDropped.fetch_add (numSamples, std::memory_order_relaxed);
    }

private:
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> createWriter (const juce::File& file, double sampleRate, int channels)
    {
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return nullptr;

        juce::WavAudioFormat format;
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                          .withSampleRate (sampleRate)
                                                          .withNumChannels (channels)
                                                          .withBitsPerSample (24));
        if (writer == nullptr)
            return nullptr;

        // Cola de unos 3 segundos entre el hilo de audio y el disco
        return std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, (int) (sampleRate * 3.0));
    }

    juce::TimeSliceThread writerThread { "EffectSuite grabación" };
    juce::SpinLock writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> wet, dry;
    std::atomic<bool> active { false };
    std::atomic<juce::int64> samplesWritten { 0 }, samplesDropped { 0 };
    double rate = 0.0;
    juce::File lastWetFile, lastDryFile;
};
