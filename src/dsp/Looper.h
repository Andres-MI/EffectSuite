#pragma once
#include <atomic>
#include <cmath>
#include <vector>

namespace fx
{
/** Looper: graba una frase, la repite en bucle y permite grabar capas encima.

    Un solo pulsador hace el ciclo habitual: vacío -> grabar -> reproducir -> sobregrabar -> reproducir...
    Las órdenes llegan desde la interfaz por una variable atómica y se ejecutan en el hilo de audio.
    La memoria (hasta maxSeconds en estéreo) se reserva en prepare().
*/
class Looper
{
public:
    enum class State { empty, recording, playing, overdubbing, stopped };
    static constexpr double maxSeconds = 60.0;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        capacity = (int) std::min (maxSeconds * fs, 6.0e6);     // tope de memoria a frecuencias muy altas
        left.assign ((size_t) capacity, 0.0f);
        right.assign ((size_t) capacity, 0.0f);
        fadeSamples = (int) (0.004 * fs);
        clearNow();
    }

    // --- Interfaz (cualquier hilo)
    void pressMain() noexcept  { command.store (1); }
    void pressStop() noexcept  { command.store (2); }
    void pressClear() noexcept { command.store (3); }

    State getState() const noexcept           { return (State) state.load (std::memory_order_relaxed); }
    double getLengthSeconds() const noexcept  { return fs > 0.0 ? (double) shownLength.load (std::memory_order_relaxed) / fs : 0.0; }
    /** Posición dentro del bucle, de 0 a 1 (o del tiempo máximo, mientras se graba la primera capa). */
    float getPosition() const noexcept
    {
        const int total = getState() == State::recording ? capacity : shownLength.load (std::memory_order_relaxed);
        return total > 0 ? (float) shownPosition.load (std::memory_order_relaxed) / (float) total : 0.0f;
    }

    /** Hilo de audio. Añade el bucle a left/right; lo que entra es lo que se graba. */
    void process (float* outL, float* outR, int numSamples, float level, bool audible) noexcept
    {
        switch (command.exchange (0))
        {
            case 1:
                if (current == State::empty)                 { current = State::recording; length = 0; position = 0; }
                else if (current == State::recording)        closeLoop (State::playing);
                else if (current == State::playing)          current = State::overdubbing;
                else if (current == State::overdubbing)      current = State::playing;
                else if (current == State::stopped)          { current = State::playing; position = 0; }
                break;

            case 2:
                if (current == State::recording)             closeLoop (State::stopped);
                else if (current == State::playing || current == State::overdubbing) current = State::stopped;
                break;

            case 3: clearNow(); break;
            default: break;
        }

        const float gain = audible ? level : 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (current == State::recording)
            {
                left[(size_t) position] = outL[i];
                right[(size_t) position] = outR[i];

                if (++position >= capacity)
                    closeLoop (State::playing);
            }
            else if (current == State::playing || current == State::overdubbing)
            {
                const float l = left[(size_t) position], r = right[(size_t) position];

                if (current == State::overdubbing)
                {
                    left[(size_t) position] = l + outL[i];
                    right[(size_t) position] = r + outR[i];
                }

                outL[i] += gain * l;
                outR[i] += gain * r;

                if (++position >= length)
                    position = 0;
            }
        }

        state.store ((int) current, std::memory_order_relaxed);
        shownLength.store (length, std::memory_order_relaxed);
        shownPosition.store (position, std::memory_order_relaxed);
    }

private:
    void clearNow() noexcept
    {
        current = State::empty;
        length = position = 0;
        state.store ((int) current);
        shownLength.store (0);
        shownPosition.store (0);
    }

    /** Fija la duración del bucle y suaviza sus extremos para que la vuelta no dé un chasquido. */
    void closeLoop (State next) noexcept
    {
        length = position;

        if (length < 2 * fadeSamples + 2)
        {
            clearNow();
            return;
        }

        for (int i = 0; i < fadeSamples; ++i)
        {
            const float g = (float) i / (float) fadeSamples;
            left[(size_t) i] *= g;                          right[(size_t) i] *= g;
            left[(size_t) (length - 1 - i)] *= g;           right[(size_t) (length - 1 - i)] *= g;
        }

        position = 0;
        current = next;
    }

    double fs = 48000.0;
    int capacity = 0, length = 0, position = 0, fadeSamples = 192;
    State current = State::empty;
    std::vector<float> left, right;
    std::atomic<int> command { 0 }, state { 0 }, shownLength { 0 }, shownPosition { 0 };
};
}
