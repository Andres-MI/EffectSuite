#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout EffectSuiteProcessor::createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    APVTS::ParameterLayout layout;
    auto names = [] (auto& voicings)
    {
        juce::StringArray list;
        for (auto* v : voicings) list.add (v->name);
        return list;
    };
    auto id = [] (const char* name) { return juce::ParameterID { name, 1 }; };
    auto knob = [&] (const char* name, const char* label, float def)
    {
        layout.add (std::make_unique<Float> (id (name), label, Range (0.0f, 1.0f, 0.001f), def));
    };

    layout.add (std::make_unique<Float> (id ("inputGain"), "Input Gain", Range (-24.0f, 24.0f, 0.1f), 0.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<Choice> (id ("inputMode"), "Input Channel",
                                          juce::StringArray { "Left (1)", "Right (2)", "Left + Right" }, 0));

    layout.add (std::make_unique<Bool> (id ("gateOn"), "Gate On", true));
    layout.add (std::make_unique<Float> (id ("gateThreshold"), "Gate Threshold", Range (-90.0f, -20.0f, 0.1f), -65.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<Bool> (id ("wahOn"), "Wah On", false));
    layout.add (std::make_unique<Choice> (id ("wahMode"), "Wah Mode", juce::StringArray { "Manual", "Auto" }, 1));
    knob ("wahPosition", "Wah Position", 0.5f);
    knob ("wahQ", "Wah Resonance", 0.5f);

    layout.add (std::make_unique<Bool> (id ("compOn"), "Compressor On", false));
    knob ("compSustain", "Compressor Sustain", 0.5f);
    knob ("compAttack", "Compressor Attack", 0.3f);
    knob ("compLevel", "Compressor Level", 0.5f);

    layout.add (std::make_unique<Bool> (id ("octOn"), "Octaver On", false));
    knob ("octSub", "Octaver Sub", 0.6f);
    knob ("octDry", "Octaver Dry", 0.7f);

    layout.add (std::make_unique<Bool> (id ("phaserOn"), "Phaser On", false));
    knob ("phaserRate", "Phaser Rate", 0.3f);
    knob ("phaserDepth", "Phaser Depth", 0.7f);
    knob ("phaserFeedback", "Phaser Feedback", 0.4f);

    layout.add (std::make_unique<Bool> (id ("odOn"), "Overdrive On", false));
    layout.add (std::make_unique<Choice> (id ("odModel"), "Overdrive Model", names (fx::overdrives::all), 0));
    knob ("odDrive", "Overdrive Drive", 0.5f);
    knob ("odTone", "Overdrive Tone", 0.5f);
    knob ("odLevel", "Overdrive Level", 0.7f);

    layout.add (std::make_unique<Bool> (id ("distOn"), "Distortion On", false));
    layout.add (std::make_unique<Choice> (id ("distModel"), "Distortion Model", names (fx::distortions::all), 0));
    knob ("distGain", "Distortion Gain", 0.5f);
    knob ("distTone", "Distortion Tone", 0.5f);
    knob ("distLevel", "Distortion Level", 0.5f);

    layout.add (std::make_unique<Bool> (id ("fuzzOn"), "Fuzz On", false));
    layout.add (std::make_unique<Choice> (id ("fuzzModel"), "Fuzz Model", names (fx::fuzzes::all), 0));
    knob ("fuzzAmount", "Fuzz Amount", 0.6f);
    knob ("fuzzTone", "Fuzz Tone", 0.5f);
    knob ("fuzzLevel", "Fuzz Level", 0.5f);

    layout.add (std::make_unique<Bool> (id ("ampOn"), "Amp On", true));
    layout.add (std::make_unique<Choice> (id ("ampModel"), "Amp Model", names (fx::voicings::all), 0));
    knob ("ampGain", "Amp Gain", 0.5f);
    knob ("ampBass", "Amp Bass", 0.5f);
    knob ("ampMid", "Amp Mid", 0.5f);
    knob ("ampTreble", "Amp Treble", 0.5f);
    knob ("ampPresence", "Amp Presence", 0.5f);
    knob ("ampMaster", "Amp Master", 0.5f);

    layout.add (std::make_unique<Bool> (id ("cabOn"), "Cabinet On", true));
    layout.add (std::make_unique<Choice> (id ("cabModel"), "Cabinet Model",
                                          juce::StringArray (fx::GuitarChain::cabNames, fx::GuitarChain::numCabs), 0));
    layout.add (std::make_unique<Bool> (id ("eqOn"), "EQ On", false));
    for (auto band : { std::pair<const char*, const char*> { "eq100", "EQ 100 Hz" }, { "eq200", "EQ 200 Hz" }, { "eq400", "EQ 400 Hz" },
                       { "eq800", "EQ 800 Hz" }, { "eq1k6", "EQ 1.6 kHz" }, { "eq3k2", "EQ 3.2 kHz" }, { "eq6k4", "EQ 6.4 kHz" } })
        layout.add (std::make_unique<Float> (id (band.first), band.second, Range (-12.0f, 12.0f, 0.1f), 0.0f,
                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<Bool> (id ("flangerOn"), "Flanger On", false));
    knob ("flangerRate", "Flanger Rate", 0.3f);
    knob ("flangerDepth", "Flanger Depth", 0.6f);
    knob ("flangerFeedback", "Flanger Feedback", 0.5f);

    layout.add (std::make_unique<Bool> (id ("tremOn"), "Tremolo On", false));
    knob ("tremRate", "Tremolo Rate", 0.5f);
    knob ("tremDepth", "Tremolo Depth", 0.6f);

    layout.add (std::make_unique<Bool> (id ("chorusOn"), "Chorus On", false));
    knob ("chorusRate", "Chorus Rate", 0.3f);
    knob ("chorusDepth", "Chorus Depth", 0.5f);
    knob ("chorusMix", "Chorus Mix", 0.6f);

    layout.add (std::make_unique<Bool> (id ("delayOn"), "Delay On", false));
    layout.add (std::make_unique<Choice> (id ("delayModel"), "Delay Model", juce::StringArray { juce::String::fromUTF8 ("Analógico"), "Cinta" }, 0));
    layout.add (std::make_unique<Float> (id ("delayTime"), "Delay Time", Range (20.0f, 1500.0f, 1.0f, 0.5f), 380.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("ms")));
    knob ("delayFeedback", "Delay Feedback", 0.35f);
    knob ("delayMix", "Delay Mix", 0.3f);

    layout.add (std::make_unique<Bool> (id ("reverbOn"), "Reverb On", false));
    layout.add (std::make_unique<Choice> (id ("reverbModel"), "Reverb Model", juce::StringArray { "Sala", "Muelles" }, 0));
    knob ("reverbSize", "Reverb Size", 0.5f);
    knob ("reverbMix", "Reverb Mix", 0.25f);

    layout.add (std::make_unique<Bool> (id ("looperOn"), "Looper On", false));
    knob ("looperLevel", "Looper Level", 0.8f);

    layout.add (std::make_unique<Float> (id ("outputLevel"), "Output Level", Range (-24.0f, 12.0f, 0.1f), 0.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<Choice> (id ("quality"), "Quality",
                                          juce::StringArray { juce::String::fromUTF8 ("Mínimo (1x)"), "Eco (2x)", "Normal (4x)", "Alta (8x)" }, 2));
    layout.add (std::make_unique<Bool> (id ("tunerMute"), "Tuner Mute", false));

    layout.add (std::make_unique<Bool> (id ("metroOn"), "Metronome On", false));
    layout.add (std::make_unique<Float> (id ("metroBpm"), "Metronome Tempo", Range (40.0f, 240.0f, 1.0f), 100.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("bpm")));
    knob ("metroLevel", "Metronome Level", 0.5f);

    return layout;
}

EffectSuiteProcessor::EffectSuiteProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EffectSuite", createLayout())
{
    auto ref = [this] (const char* name) { return apvts.getRawParameterValue (name); };

    params = { ref ("inputGain"), ref ("inputMode"), ref ("gateOn"), ref ("gateThreshold"),
               ref ("wahOn"), ref ("wahMode"), ref ("wahPosition"), ref ("wahQ"),
               ref ("octOn"), ref ("octSub"), ref ("octDry"),
               ref ("phaserOn"), ref ("phaserRate"), ref ("phaserDepth"), ref ("phaserFeedback"),
               ref ("eqOn"),
               ref ("flangerOn"), ref ("flangerRate"), ref ("flangerDepth"), ref ("flangerFeedback"),
               ref ("tremOn"), ref ("tremRate"), ref ("tremDepth"),
               ref ("compOn"), ref ("compSustain"), ref ("compAttack"), ref ("compLevel"),
               ref ("odOn"), ref ("odModel"), ref ("odDrive"), ref ("odTone"), ref ("odLevel"),
               ref ("distOn"), ref ("distModel"), ref ("distGain"), ref ("distTone"), ref ("distLevel"),
               ref ("fuzzOn"), ref ("fuzzModel"), ref ("fuzzAmount"), ref ("fuzzTone"), ref ("fuzzLevel"),
               ref ("ampOn"), ref ("ampModel"), ref ("ampGain"), ref ("ampBass"), ref ("ampMid"),
               ref ("ampTreble"), ref ("ampPresence"), ref ("ampMaster"),
               ref ("cabOn"), ref ("cabModel"),
               ref ("chorusOn"), ref ("chorusRate"), ref ("chorusDepth"), ref ("chorusMix"),
               ref ("delayOn"), ref ("delayTime"), ref ("delayFeedback"), ref ("delayMix"),
               ref ("reverbOn"), ref ("reverbSize"), ref ("reverbMix"),
               ref ("outputLevel"), ref ("quality"), ref ("tunerMute"),
               ref ("metroOn"), ref ("metroBpm"), ref ("metroLevel") };

    int band = 0;
    for (auto* name : { "eq100", "eq200", "eq400", "eq800", "eq1k6", "eq3k2", "eq6k4" })
        eqBandParams[band++] = ref (name);

    delayModelParam = ref ("delayModel");
    reverbModelParam = ref ("reverbModel");
    looperOnParam = ref ("looperOn");
    looperLevelParam = ref ("looperLevel");

    updateOrder();
    startTimerHz (30);
}

EffectSuiteProcessor::~EffectSuiteProcessor()
{
    stopTimer();
}

bool EffectSuiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s)
    {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };

    return ok (in) && ok (out);
}

void EffectSuiteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (16, samplesPerBlock);
    work.setSize (4, maxBlock, false, false, true);     // 0 entrada mono, 1 y 2 salida, 3 copia limpia para grabar
    clickPhase = 0.0; clickEnvelope = 0.0f; clickBeat = 0;
    chain.prepare (sampleRate, maxBlock);
    pitchTracker.prepare (sampleRate);
    loadMeasurer.reset (sampleRate, samplesPerBlock);

    // Si el audio cambia de frecuencia a mitad de toma, el archivo quedaría con la frecuencia equivocada
    if (recorder.isRecording() && std::abs (recorder.getSampleRate() - sampleRate) > 0.5)
        recorder.stop();

    const int quality = (int) params.quality->load() - 1;
    latencyQuality.store (quality);
    setLatencySamples (juce::roundToInt (chain.getLatencySamples (quality)));
}

fx::ChainParams EffectSuiteProcessor::readParams() const
{
    fx::ChainParams p;
    p.inputGainDb     = params.inputGain->load();
    p.gateOn          = params.gateOn->load() > 0.5f;
    p.gateThresholdDb = params.gateThreshold->load();
    p.wahOn           = params.wahOn->load() > 0.5f;
    p.wahMode         = (int) params.wahMode->load();
    // Mientras el valor del pedal MIDI no se haya trasladado al parámetro, manda el pedal: así el wah responde al instante
    p.wahPosition     = midiWahPending.load (std::memory_order_relaxed) ? midiWah.load (std::memory_order_relaxed)
                                                                         : params.wahPosition->load();
    p.wahQ            = params.wahQ->load();
    p.octOn           = params.octOn->load() > 0.5f;
    p.octSub          = params.octSub->load();
    p.octDry          = params.octDry->load();
    p.phaserOn        = params.phaserOn->load() > 0.5f;
    p.phaserRate      = params.phaserRate->load();
    p.phaserDepth     = params.phaserDepth->load();
    p.phaserFeedback  = params.phaserFeedback->load();
    p.eqOn            = params.eqOn->load() > 0.5f;
    for (int i = 0; i < fx::Equalizer::numBands; ++i)
        p.eqBands[i] = eqBandParams[i]->load();

    p.delayModel      = (int) delayModelParam->load();
    p.reverbModel     = (int) reverbModelParam->load();
    p.looperOn        = looperOnParam->load() > 0.5f;
    p.looperLevel     = looperLevelParam->load();
    p.flangerOn       = params.flangerOn->load() > 0.5f;
    p.flangerRate     = params.flangerRate->load();
    p.flangerDepth    = params.flangerDepth->load();
    p.flangerFeedback = params.flangerFeedback->load();
    p.tremOn          = params.tremOn->load() > 0.5f;
    p.tremRate        = params.tremRate->load();
    p.tremDepth       = params.tremDepth->load();
    p.compOn          = params.compOn->load() > 0.5f;
    p.compSustain     = params.compSustain->load();
    p.compAttack      = params.compAttack->load();
    p.compLevel       = params.compLevel->load();
    p.distOn          = params.distOn->load() > 0.5f;
    p.distGain        = params.distGain->load();
    p.distTone        = params.distTone->load();
    p.distLevel       = params.distLevel->load();
    p.fuzzOn          = params.fuzzOn->load() > 0.5f;
    p.fuzzAmount      = params.fuzzAmount->load();
    p.fuzzTone        = params.fuzzTone->load();
    p.fuzzLevel       = params.fuzzLevel->load();
    p.chorusOn        = params.chorusOn->load() > 0.5f;
    p.chorusRate      = params.chorusRate->load();
    p.chorusDepth     = params.chorusDepth->load();
    p.chorusMix       = params.chorusMix->load();
    p.delayOn         = params.delayOn->load() > 0.5f;
    p.delayTimeMs     = params.delayTime->load();
    p.delayFeedback   = params.delayFeedback->load();
    p.delayMix        = params.delayMix->load();
    p.reverbOn        = params.reverbOn->load() > 0.5f;
    p.reverbSize      = params.reverbSize->load();
    p.reverbMix       = params.reverbMix->load();
    p.odModel         = (int) params.odModel->load();
    p.distModel       = (int) params.distModel->load();
    p.fuzzModel       = (int) params.fuzzModel->load();
    p.cabModel        = (int) params.cabModel->load();
    p.odOn            = params.odOn->load() > 0.5f;
    p.odDrive         = params.odDrive->load();
    p.odTone          = params.odTone->load();
    p.odLevel         = params.odLevel->load();
    p.ampOn           = params.ampOn->load() > 0.5f;
    p.ampModel        = (int) params.ampModel->load();
    p.ampGain         = params.ampGain->load();
    p.ampBass         = params.ampBass->load();
    p.ampMid          = params.ampMid->load();
    p.ampTreble       = params.ampTreble->load();
    p.ampPresence     = params.ampPresence->load();
    p.ampMaster       = params.ampMaster->load();
    p.cabOn           = params.cabOn->load() > 0.5f;
    p.outputDb        = params.outputLevel->load();
    p.quality         = (int) params.quality->load() - 1;
    p.mute            = params.tunerMute->load() > 0.5f;

    const auto pre = preOrderPacked.load (std::memory_order_relaxed), post = postOrderPacked.load (std::memory_order_relaxed);
    for (size_t i = 0; i < p.preOrder.size(); ++i)  p.preOrder[i]  = (juce::uint8) ((pre >> (4 * i)) & 0xf);
    for (size_t i = 0; i < p.postOrder.size(); ++i) p.postOrder[i] = (juce::uint8) ((post >> (4 * i)) & 0xf);
    return p;
}

void EffectSuiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isController() && (message.getControllerNumber() == 11 || message.getControllerNumber() == 4))
        {
            midiWah.store ((float) message.getControllerValue() / 127.0f);
            midiWahPending.store (true);
        }
        else if (message.isProgramChange())
        {
            midiProgram.store (message.getProgramChangeNumber());
        }
    }

    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer loadTimer (loadMeasurer, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numSamples == 0 || numIn == 0 || maxBlock == 0)
        return;

    const auto p = readParams();
    const int inputMode = (int) params.inputMode->load();

    // El aviso de latencia al host se hace desde un temporizador: aquí no se puede bloquear
    if (p.quality != latencyQuality.exchange (p.quality))
        latencyChanged.store (true);

    float* m = work.getWritePointer (0);
    float* outL = work.getWritePointer (1);
    float* outR = work.getWritePointer (2);
    float* direct = work.getWritePointer (3);
    const bool metronome = params.metroOn->load() > 0.5f;
    const double samplesPerBeat = getSampleRate() * 60.0 / juce::jlimit (40.0, 240.0, (double) params.metroBpm->load());
    const float clickGain = 0.35f * params.metroLevel->load() * params.metroLevel->load();
    float inPeak = 0.0f, outPeak = 0.0f;

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - start);
        const float* left = buffer.getReadPointer (0, start);
        const float* right = numIn > 1 ? buffer.getReadPointer (1, start) : left;

        if (numIn < 2 || inputMode == 0)
            juce::FloatVectorOperations::copy (m, left, n);
        else if (inputMode == 1)
            juce::FloatVectorOperations::copy (m, right, n);
        else
            juce::FloatVectorOperations::add (m, left, right, n);

        inPeak = juce::jmax (inPeak, juce::FloatVectorOperations::findMaximum (m, n),
                             -juce::FloatVectorOperations::findMinimum (m, n));
        pitchTracker.push (m, n);

        juce::FloatVectorOperations::copy (direct, m, n);
        chain.process (m, outL, outR, n, p);
        recorder.write (direct, outL, outR, n);

        if (metronome)
        {
            // Clic corto en cada tiempo, más agudo en el primero de cada cuatro. Va después de grabar: no queda en la toma
            for (int i = 0; i < n; ++i)
            {
                if (clickPhase <= 0.0)
                {
                    clickPhase += samplesPerBeat;
                    clickEnvelope = 1.0f;
                    clickAngle = 0.0;
                    clickAngleStep = juce::MathConstants<double>::twoPi * (clickBeat == 0 ? 1600.0 : 1000.0) / getSampleRate();
                    clickBeat = (clickBeat + 1) % 4;
                }

                clickPhase -= 1.0;
                const float click = clickGain * clickEnvelope * (float) std::sin (clickAngle);
                clickAngle += clickAngleStep;
                clickEnvelope *= 0.9985f;
                outL[i] = juce::jlimit (-1.0f, 1.0f, outL[i] + click);
                outR[i] = juce::jlimit (-1.0f, 1.0f, outR[i] + click);
            }
        }
        else
        {
            clickPhase = 0.0;
            clickBeat = 0;
        }

        for (const float* out : { outL, outR })
            outPeak = juce::jmax (outPeak, juce::FloatVectorOperations::findMaximum (out, n),
                                  -juce::FloatVectorOperations::findMinimum (out, n));

        if (numOut == 1)
        {
            float* dest = buffer.getWritePointer (0, start);
            for (int i = 0; i < n; ++i)
                dest[i] = 0.5f * (outL[i] + outR[i]);
        }
        else
        {
            buffer.copyFrom (0, start, outL, n);
            buffer.copyFrom (1, start, outR, n);

            for (int ch = 2; ch < numOut; ++ch)
                buffer.clear (ch, start, n);
        }
    }

    // El medidor de entrada muestra el nivel que llega a los pedales, es decir, después de la ganancia de entrada
    inPeak = std::isfinite (inPeak) ? inPeak * juce::Decibels::decibelsToGain (p.inputGainDb) : 1.0f;
    if (! std::isfinite (outPeak)) outPeak = 1.0f;

    if (inPeak > inputPeak.load (std::memory_order_relaxed))   inputPeak.store (inPeak, std::memory_order_relaxed);
    if (outPeak > outputPeak.load (std::memory_order_relaxed)) outputPeak.store (outPeak, std::memory_order_relaxed);
}

juce::File EffectSuiteProcessor::getRecordingFolder() const
{
    if (recordingFolderOverride != juce::File())
        return recordingFolderOverride;

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EffectSuite").getChildFile ("Grabaciones");
}

juce::String EffectSuiteProcessor::startRecording()
{
    return recorder.start (getRecordingFolder(), getSampleRate());
}

void EffectSuiteProcessor::stopRecording()
{
    recorder.stop();
}

void EffectSuiteProcessor::applyPendingMidi()
{
    // Primero se baja la marca y después se lee el valor: si llega otro mensaje entre medias,
    // vuelve a subirla y se aplica en la siguiente pasada, en lugar de perderse
    if (midiWahPending.exchange (false))
        if (auto* position = apvts.getParameter ("wahPosition"))
            position->setValueNotifyingHost (midiWah.load());

    // Un pedal encendido desde fuera (automatización del host) tiene que aparecer en la pedalera
    juce::uint32 onMask = 0;
    const auto& ids = getAllPedalIds();

    for (int i = 0; i < ids.size(); ++i)
        if (apvts.getRawParameterValue (ids[i] + "On")->load() > 0.5f)
            onMask |= 1u << i;

    if (onMask != lastOnMask)
    {
        lastOnMask = onMask;
        bool missing = false;
        juce::StringArray stored;

        {
            const juce::ScopedLock lock (stateLock);
            stored.addTokens (apvts.state.getProperty ("board", "gate,comp,od,chorus,delay,reverb").toString(), ",", "");
        }

        for (int i = 0; i < ids.size(); ++i)
            missing = missing || ((onMask >> i) & 1u && ! stored.contains (ids[i]));

        if (missing)
            writeBoard (readBoard());
    }

    const int program = midiProgram.exchange (-1);

    if (program >= 0)
    {
        bool factory = false;
        const auto name = getPresetNameForProgram (program, factory);

        if (name.isNotEmpty())
        {
            if (factory) loadFactoryPreset (name);
            else         loadUserPreset (name);
        }
    }
}

juce::String EffectSuiteProcessor::getPresetNameForProgram (int program, bool& isFactory) const
{
    const auto factory = getFactoryPresetNames();
    isFactory = program < factory.size();

    if (program < 0)
        return {};

    if (isFactory)
        return factory[program];

    const auto user = getUserPresetNames();
    return user[program - factory.size()];      // cadena vacía si no existe
}

void EffectSuiteProcessor::timerCallback()
{
    applyPendingMidi();

    if (latencyChanged.exchange (false))
        setLatencySamples (juce::roundToInt (chain.getLatencySamples (latencyQuality.load())));
}

bool EffectSuiteProcessor::loadImpulseResponse (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    reader.reset();
    chain.loadImpulseResponse (file);
    const juce::ScopedLock lock (stateLock);
    irFile = file;
    return true;
}

void EffectSuiteProcessor::clearImpulseResponse()
{
    chain.clearImpulseResponse();
    const juce::ScopedLock lock (stateLock);
    irFile = juce::File();
}

juce::String EffectSuiteProcessor::getImpulseResponseName() const
{
    const juce::ScopedLock lock (stateLock);
    return irFile == juce::File() ? juce::String() : irFile.getFileName();
}

const juce::StringArray& EffectSuiteProcessor::getAllPedalIds()
{
    static const juce::StringArray ids { "gate", "wah", "comp", "oct", "phaser", "od", "dist", "fuzz",
                                         "eq", "flanger", "chorus", "trem", "delay", "reverb", "looper" };
    return ids;
}

juce::StringArray EffectSuiteProcessor::readBoard() const
{
    juce::StringArray board;
    const juce::ScopedLock lock (stateLock);

    if (apvts.state.hasProperty ("board"))
        board.addTokens (apvts.state.getProperty ("board").toString(), ",", "");
    else
        board = { "gate", "comp", "od", "chorus", "delay", "reverb" };      // pedalera inicial

    board.removeEmptyStrings();

    // Un pedal encendido siempre está en la pedalera (por ejemplo, en proyectos guardados con una versión anterior)
    for (auto& id : getAllPedalIds())
        if (auto* on = apvts.getRawParameterValue (id + "On"))
            if (on->load() > 0.5f)
                board.addIfNotAlreadyThere (id);

    return board;
}

void EffectSuiteProcessor::writeBoard (const juce::StringArray& board)
{
    {
        const juce::ScopedLock lock (stateLock);
        apvts.state.setProperty ("board", board.joinIntoString (","), nullptr);
    }

    updateOrder();
    ++boardRevision;
}

/** El orden de la señal es el de la pedalera; los pedales no colocados (apagados) van detrás. */
void EffectSuiteProcessor::updateOrder()
{
    const auto board = readBoard();
    const auto& all = getAllPedalIds();

    auto pack = [&] (int first, int count)
    {
        juce::Array<int> order;

        for (auto& id : board)
        {
            const int index = all.indexOf (id) - first;
            if (index >= 0 && index < count) order.addIfNotAlreadyThere (index);
        }

        for (int i = 0; i < count; ++i) order.addIfNotAlreadyThere (i);

        juce::uint32 packed = 0;
        for (int i = 0; i < count; ++i) packed |= (juce::uint32) order[i] << (4 * i);
        return packed;
    };

    preOrderPacked.store (pack (0, fx::numPrePedals));
    postOrderPacked.store (pack (fx::numPrePedals, fx::numPostPedals));
}

juce::StringArray EffectSuiteProcessor::getBoardRow (bool afterAmp) const
{
    juce::StringArray row;
    const auto& all = getAllPedalIds();

    for (auto& id : readBoard())
        if (all.contains (id) && (all.indexOf (id) >= fx::numPrePedals) == afterAmp)
            row.addIfNotAlreadyThere (id);

    // El looper suena siempre al final, así que también se muestra el último
    if (row.contains ("looper"))
    {
        row.removeString ("looper");
        row.add ("looper");
    }

    return row;
}

void EffectSuiteProcessor::movePedal (const juce::String& pedalId, int newIndex)
{
    const auto& all = getAllPedalIds();

    if (! all.contains (pedalId) || ! isOnBoard (pedalId))
        return;

    const bool afterAmp = all.indexOf (pedalId) >= fx::numPrePedals;
    auto row = getBoardRow (afterAmp);
    if (pedalId == "looper")
        return;

    row.removeString (pedalId);
    row.insert (juce::jlimit (0, row.size() - (row.contains ("looper") ? 1 : 0), newIndex), pedalId);

    auto board = afterAmp ? getBoardRow (false) : row;
    board.addArray (afterAmp ? row : getBoardRow (true));
    writeBoard (board);
}

bool EffectSuiteProcessor::isOnBoard (const juce::String& pedalId) const
{
    return readBoard().contains (pedalId);
}

void EffectSuiteProcessor::setOnBoard (const juce::String& pedalId, bool shouldBeOnBoard)
{
    if (! getAllPedalIds().contains (pedalId))
        return;

    if (! shouldBeOnBoard)
        if (auto* on = apvts.getParameter (pedalId + "On"))
            on->setValueNotifyingHost (0.0f);

    auto board = readBoard();

    if (shouldBeOnBoard) board.addIfNotAlreadyThere (pedalId);
    else                 board.removeString (pedalId);

    writeBoard (board);
}

juce::String EffectSuiteProcessor::loadNeuralModel (const juce::File& file)
{
    if (! file.existsAsFile())
        return "No se encuentra el archivo";

    const auto result = chain.loadNeuralModel (file.getFullPathName().toStdString());

    if (! result.ok)
        return juce::String::fromUTF8 (result.error.c_str());

    const juce::ScopedLock lock (stateLock);
    namFile = file;
    namSampleRate = result.modelSampleRate;
    return {};
}

void EffectSuiteProcessor::clearNeuralModel()
{
    chain.clearNeuralModel();
    const juce::ScopedLock lock (stateLock);
    namFile = juce::File();
    namSampleRate = 0.0;
}

juce::String EffectSuiteProcessor::getNeuralModelName() const
{
    const juce::ScopedLock lock (stateLock);
    return namFile == juce::File() ? juce::String() : namFile.getFileNameWithoutExtension();
}

juce::String EffectSuiteProcessor::getNeuralModelNote() const
{
    // Las capturas se remuestrean a la frecuencia del audio, así que ya no hay nada que avisar
    return {};
}

int EffectSuiteProcessor::getEditorWidth() const
{
    const juce::ScopedLock lock (stateLock);
    return (int) apvts.state.getProperty ("editorWidth", 0);
}

void EffectSuiteProcessor::setEditorWidth (int width)
{
    const juce::ScopedLock lock (stateLock);

    if ((int) apvts.state.getProperty ("editorWidth", 0) != width)
        apvts.state.setProperty ("editorWidth", width, nullptr);
}

void EffectSuiteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock (stateLock);
    auto state = apvts.copyState();
    state.setProperty ("irFile", irFile.getFullPathName(), nullptr);
    state.setProperty ("namFile", namFile.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EffectSuiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyState (juce::ValueTree::fromXml (*xml), false);
}

namespace
{
    /** Ajustes del equipo, que un preset no debe cambiar. */
    const char* const rigSettings[] { "inputGain", "inputMode", "outputLevel", "quality", "tunerMute", "metroOn", "metroBpm", "metroLevel" };

    bool isRigSetting (const juce::String& id)
    {
        for (auto* r : rigSettings) if (id == r) return true;
        return false;
    }

    void setParameterValue (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float realValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }
}

/** Devuelve pedales, ampli y pantalla a sus valores por defecto; los parámetros que falten en un
    estado antiguo se quedan así en lugar de heredar lo que hubiera antes. */
void EffectSuiteProcessor::resetSoundToDefaults()
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isRigSetting (ranged->getParameterID()))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
}

void EffectSuiteProcessor::applyState (const juce::ValueTree& source, bool keepRigSettings)
{
    if (! source.isValid() || ! source.hasType (apvts.state.getType()))
        return;

    auto state = source.createCopy();
    float rig[std::size (rigSettings)] {};

    for (size_t i = 0; i < std::size (rigSettings); ++i)
        rig[i] = apvts.getRawParameterValue (rigSettings[i])->load();

    const juce::String path = state.getProperty ("irFile").toString();
    const juce::String namPath = state.getProperty ("namFile").toString();

    resetSoundToDefaults();

    {
        const juce::ScopedLock lock (stateLock);
        apvts.replaceState (state);
    }

    // Un valor no numérico en un estado dañado dejaría la cadena muda: se sustituye por el de fábrica
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (! std::isfinite (ranged->getValue()) || ! std::isfinite (ranged->convertFrom0to1 (ranged->getValue())))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());

    if (keepRigSettings)
        for (size_t i = 0; i < std::size (rigSettings); ++i)
            setParameterValue (apvts, rigSettings[i], rig[i]);

    updateOrder();
    ++boardRevision;

    if (! (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile()
           && loadImpulseResponse (juce::File (path))))
        clearImpulseResponse();

    if (namPath.isEmpty() || ! juce::File::isAbsolutePath (namPath) || loadNeuralModel (juce::File (namPath)).isNotEmpty())
        clearNeuralModel();
}

//==============================================================================
// Presets

namespace
{
    struct FactoryPreset
    {
        const char* name;
        const char* board;      // pedales colocados y encendidos, en orden
        std::vector<std::pair<const char*, float>> values;
    };

    // Modelos: ampli 0 Clean American, 1 British Crunch, 2 High Gain, 3 Tweed, 4 Class A Chime, 5 Modern Lead,
    //          6 Bass Tube, 7 Bass Clean, 8 Bass Rock. Pantalla 0 4x12, 1 2x12, 2 1x12, 3 Bass 8x10, 4 Bass 1x15.
    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> presets {
            { "Guitarra: Limpio cristalino", "comp,chorus,reverb",
              { { "ampModel", 0 }, { "ampGain", 0.50f }, { "ampMaster", 0.65f }, { "ampTreble", 0.60f }, { "cabModel", 1 }, { "compSustain", 0.40f }, { "chorusMix", 0.45f }, { "reverbMix", 0.25f } } },
            { "Guitarra: Blues con cuerpo", "od,reverb",
              { { "ampModel", 3 }, { "ampGain", 0.50f }, { "ampMaster", 0.31f }, { "cabModel", 2 }, { "odModel", 1 }, { "odDrive", 0.45f }, { "odLevel", 0.60f }, { "reverbMix", 0.18f } } },
            { "Guitarra: Brillo clase A", "trem,reverb",
              { { "ampModel", 4 }, { "ampGain", 0.55f }, { "ampMaster", 0.41f }, { "cabModel", 1 }, { "tremRate", 0.45f }, { "tremDepth", 0.45f }, { "reverbMix", 0.22f } } },
            { "Guitarra: Crunch británico", "gate",
              { { "ampModel", 1 }, { "ampGain", 0.55f }, { "ampMid", 0.60f }, { "cabModel", 0 } } },
            { "Guitarra: Rock clásico", "gate,od,delay",
              { { "ampModel", 1 }, { "ampGain", 0.60f }, { "cabModel", 0 }, { "odModel", 0 }, { "odDrive", 0.30f }, { "odLevel", 0.75f }, { "delayTime", 340.0f }, { "delayMix", 0.20f } } },
            { "Guitarra: Metal moderno", "gate,od",
              { { "ampModel", 2 }, { "ampGain", 0.65f }, { "ampMaster", 0.64f }, { "ampBass", 0.55f }, { "ampMid", 0.40f }, { "ampTreble", 0.60f }, { "cabModel", 0 },
                { "gateThreshold", -55.0f }, { "odModel", 0 }, { "odDrive", 0.10f }, { "odLevel", 0.80f } } },
            { "Guitarra: Solista", "gate,delay,reverb",
              { { "ampModel", 5 }, { "ampGain", 0.60f }, { "ampMaster", 0.66f }, { "ampMid", 0.65f }, { "cabModel", 0 }, { "delayTime", 420.0f }, { "delayFeedback", 0.35f }, { "delayMix", 0.25f }, { "reverbMix", 0.20f } } },
            { "Guitarra: Fuzz psicodélico", "fuzz,phaser,reverb",
              { { "ampModel", 0 }, { "ampGain", 0.35f }, { "cabModel", 0 }, { "fuzzModel", 0 }, { "fuzzAmount", 0.65f }, { "fuzzLevel", 0.75f }, { "phaserRate", 0.25f }, { "reverbMix", 0.25f } } },
            { "Guitarra: Funk auto-wah", "comp,wah",
              { { "ampModel", 0 }, { "ampGain", 0.55f }, { "ampMaster", 0.70f }, { "cabModel", 1 }, { "wahMode", 1 }, { "wahPosition", 0.70f }, { "wahQ", 0.45f }, { "compSustain", 0.75f } } },
            { "Guitarra: Ambiente", "comp,chorus,delay,reverb",
              { { "ampModel", 0 }, { "ampGain", 0.45f }, { "ampMaster", 0.65f }, { "cabModel", 1 }, { "chorusMix", 0.50f }, { "delayTime", 500.0f }, { "delayFeedback", 0.50f }, { "delayMix", 0.35f },
                { "reverbSize", 0.85f }, { "reverbMix", 0.45f } } },
            { "Bajo: Redondo a válvulas", "comp",
              { { "ampModel", 6 }, { "ampGain", 0.50f }, { "cabModel", 3 }, { "compSustain", 0.45f } } },
            { "Bajo: Limpio moderno", "comp,eq",
              { { "ampModel", 7 }, { "ampGain", 0.50f }, { "ampMaster", 0.45f }, { "cabModel", 4 }, { "compSustain", 0.50f }, { "eq100", 2.0f }, { "eq400", -3.0f }, { "eq3k2", 3.0f } } },
            { "Bajo: Rock", "comp,od",
              { { "ampModel", 8 }, { "ampGain", 0.50f }, { "cabModel", 3 }, { "odModel", 3 }, { "odDrive", 0.40f } } },
            { "Bajo: Fuzz", "fuzz",
              { { "ampModel", 6 }, { "ampGain", 0.40f }, { "cabModel", 3 }, { "fuzzModel", 2 }, { "fuzzAmount", 0.60f } } },
            { "Bajo: Sub octava", "comp,oct,chorus",
              { { "ampModel", 7 }, { "ampGain", 0.50f }, { "ampMaster", 0.42f }, { "cabModel", 3 }, { "octSub", 0.50f }, { "octDry", 0.70f }, { "chorusMix", 0.35f } } },

            // Añadidos con la segunda tanda de modelos. Amplis: 9 Plexi, 10 Thick Crunch, 11 Jazz Clean, 12 Rectified,
            // 13 Brit Clean 100, 14 Bass Studio, 15 Bass DI. Overdrive: 4 Asym Drive, 5 Treble Boost.
            // Distorsión: 4 Orange, 5 Swedish, 6 Bass Modern. Fuzz: 3 Big Fuzz.
            { "Guitarra: Plexi clásico", "od,delay",
              { { "ampModel", 9 }, { "ampGain", 0.60f }, { "cabModel", 0 }, { "odModel", 5 }, { "odDrive", 0.45f }, { "delayModel", 1 }, { "delayTime", 320.0f }, { "delayMix", 0.18f } } },
            { "Guitarra: Surf con muelles", "trem,reverb",
              { { "ampModel", 0 }, { "ampGain", 0.50f }, { "ampMaster", 0.55f }, { "ampTreble", 0.65f }, { "cabModel", 1 }, { "reverbModel", 1 }, { "reverbSize", 0.60f }, { "reverbMix", 0.35f },
                { "tremRate", 0.55f }, { "tremDepth", 0.40f } } },
            { "Guitarra: Limpio jazz", "comp,chorus",
              { { "ampModel", 11 }, { "ampGain", 0.50f }, { "ampTreble", 0.45f }, { "cabModel", 1 }, { "compSustain", 0.35f }, { "chorusMix", 0.55f }, { "chorusDepth", 0.45f } } },
            { "Guitarra: Muro de fuzz", "fuzz,reverb",
              { { "ampModel", 13 }, { "ampGain", 0.45f }, { "cabModel", 0 }, { "fuzzModel", 3 }, { "fuzzAmount", 0.70f }, { "fuzzTone", 0.45f }, { "fuzzLevel", 0.30f }, { "reverbMix", 0.15f } } },
            { "Guitarra: Stoner", "gate,fuzz",
              { { "ampModel", 10 }, { "ampGain", 0.55f }, { "ampBass", 0.65f }, { "cabModel", 0 }, { "fuzzModel", 3 }, { "fuzzAmount", 0.45f }, { "fuzzLevel", 0.30f } } },
            { "Guitarra: Metal sueco", "gate,dist",
              { { "ampModel", 1 }, { "ampGain", 0.35f }, { "cabModel", 0 }, { "gateThreshold", -52.0f }, { "distModel", 5 }, { "distGain", 0.90f }, { "distTone", 0.85f }, { "distLevel", 0.45f } } },
            { "Guitarra: Rectificado", "gate,od",
              { { "ampModel", 12 }, { "ampGain", 0.65f }, { "ampMid", 0.35f }, { "ampMaster", 0.62f }, { "cabModel", 0 }, { "gateThreshold", -55.0f }, { "odModel", 4 }, { "odDrive", 0.15f }, { "odLevel", 0.75f } } },
            { "Guitarra: Distorsión naranja", "dist,eq",
              { { "ampModel", 0 }, { "ampGain", 0.40f }, { "ampMaster", 0.60f }, { "cabModel", 0 }, { "distModel", 4 }, { "distGain", 0.60f }, { "distTone", 0.45f }, { "distLevel", 0.38f }, { "eq400", 2.0f }, { "eq3k2", -2.0f } } },
            { "Bajo: Estudio", "comp",
              { { "ampModel", 14 }, { "ampGain", 0.50f }, { "ampMaster", 0.40f }, { "cabModel", 4 }, { "compSustain", 0.40f } } },
            { "Bajo: Directo con gruñido", "comp",
              { { "ampModel", 15 }, { "ampGain", 0.60f }, { "ampPresence", 0.65f }, { "cabModel", 3 }, { "compSustain", 0.45f } } },
            { "Bajo: Moderno agresivo", "comp,dist,eq",
              { { "ampModel", 7 }, { "ampGain", 0.50f }, { "ampMaster", 0.45f }, { "cabModel", 3 }, { "distModel", 6 }, { "distGain", 0.65f }, { "distTone", 0.60f }, { "distLevel", 0.40f }, { "eq100", 2.0f }, { "eq400", -2.5f } } },
        };
        return presets;
    }
}

juce::StringArray EffectSuiteProcessor::getFactoryPresetNames()
{
    juce::StringArray names;
    for (auto& preset : factoryPresets()) names.add (juce::String::fromUTF8 (preset.name));
    return names;
}

bool EffectSuiteProcessor::loadFactoryPreset (const juce::String& name)
{
    for (auto& preset : factoryPresets())
    {
        if (name != juce::String::fromUTF8 (preset.name))
            continue;

        resetSoundToDefaults();
        clearImpulseResponse();
        clearNeuralModel();

        juce::StringArray board;
        board.addTokens (preset.board, ",", "");

        for (auto& id : getAllPedalIds())
            setParameterValue (apvts, id + "On", board.contains (id) ? 1.0f : 0.0f);

        setParameterValue (apvts, "ampOn", 1.0f);
        setParameterValue (apvts, "cabOn", 1.0f);

        for (auto& value : preset.values)
            setParameterValue (apvts, value.first, value.second);

        {
            const juce::ScopedLock lock (stateLock);
            apvts.state.setProperty ("preset", name, nullptr);
        }

        writeBoard (board);
        return true;
    }

    return false;
}

juce::File EffectSuiteProcessor::getUserPresetFolder() const
{
    if (presetFolderOverride != juce::File())
        return presetFolderOverride;

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EffectSuite").getChildFile ("Presets");
}

juce::File EffectSuiteProcessor::presetFile (const juce::String& name) const
{
    const auto legal = juce::File::createLegalFileName (name.trim());
    return legal.isEmpty() ? juce::File() : getUserPresetFolder().getChildFile (legal + ".espreset");
}

juce::StringArray EffectSuiteProcessor::getUserPresetNames() const
{
    juce::StringArray names;

    for (auto& file : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.espreset"))
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

bool EffectSuiteProcessor::saveUserPreset (const juce::String& name)
{
    const auto file = presetFile (name);

    if (file == juce::File() || ! file.getParentDirectory().createDirectory())
        return false;

    const juce::ScopedLock lock (stateLock);
    apvts.state.setProperty ("preset", file.getFileNameWithoutExtension(), nullptr);
    auto state = apvts.copyState();
    state.setProperty ("irFile", irFile.getFullPathName(), nullptr);
    state.setProperty ("namFile", namFile.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        return xml->writeTo (file);

    return false;
}

bool EffectSuiteProcessor::loadUserPreset (const juce::String& name)
{
    const auto file = presetFile (name);

    if (file == juce::File() || ! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);
    auto state = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();

    // Un archivo sin ningún parámetro no es un preset: cargarlo solo borraría el sonido actual
    if (! state.isValid() || ! state.hasType (apvts.state.getType()) || state.getNumChildren() == 0)
        return false;

    state.setProperty ("preset", file.getFileNameWithoutExtension(), nullptr);
    applyState (state, true);
    return true;
}

bool EffectSuiteProcessor::deleteUserPreset (const juce::String& name)
{
    const auto file = presetFile (name);
    return file != juce::File() && file.existsAsFile() && file.deleteFile();
}

juce::String EffectSuiteProcessor::getCurrentPresetName() const
{
    const juce::ScopedLock lock (stateLock);
    return apvts.state.getProperty ("preset").toString();
}

juce::AudioProcessorEditor* EffectSuiteProcessor::createEditor()
{
    return new EffectSuiteEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EffectSuiteProcessor();
}
