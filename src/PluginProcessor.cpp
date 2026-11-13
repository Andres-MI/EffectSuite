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

    layout.add (std::make_unique<Bool> (id ("compOn"), "Compressor On", false));
    knob ("compSustain", "Compressor Sustain", 0.5f);
    knob ("compAttack", "Compressor Attack", 0.3f);
    knob ("compLevel", "Compressor Level", 0.5f);

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
    layout.add (std::make_unique<Bool> (id ("chorusOn"), "Chorus On", false));
    knob ("chorusRate", "Chorus Rate", 0.3f);
    knob ("chorusDepth", "Chorus Depth", 0.5f);
    knob ("chorusMix", "Chorus Mix", 0.6f);

    layout.add (std::make_unique<Bool> (id ("delayOn"), "Delay On", false));
    layout.add (std::make_unique<Float> (id ("delayTime"), "Delay Time", Range (20.0f, 1500.0f, 1.0f, 0.5f), 380.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("ms")));
    knob ("delayFeedback", "Delay Feedback", 0.35f);
    knob ("delayMix", "Delay Mix", 0.3f);

    layout.add (std::make_unique<Bool> (id ("reverbOn"), "Reverb On", false));
    knob ("reverbSize", "Reverb Size", 0.5f);
    knob ("reverbMix", "Reverb Mix", 0.25f);

    layout.add (std::make_unique<Float> (id ("outputLevel"), "Output Level", Range (-24.0f, 12.0f, 0.1f), 0.0f,
                                         juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<Choice> (id ("quality"), "Quality",
                                          juce::StringArray { "Eco (2x)", "Normal (4x)", "High (8x)" }, 1));
    layout.add (std::make_unique<Bool> (id ("tunerMute"), "Tuner Mute", false));

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
               ref ("outputLevel"), ref ("quality"), ref ("tunerMute") };
}

EffectSuiteProcessor::~EffectSuiteProcessor()
{
    cancelPendingUpdate();
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
    work.setSize (3, maxBlock, false, false, true);
    chain.prepare (sampleRate, maxBlock);
    pitchTracker.prepare (sampleRate);

    const int quality = (int) params.quality->load();
    latencyQuality.store (quality);
    setLatencySamples (juce::roundToInt (chain.getLatencySamples (quality)));
}

fx::ChainParams EffectSuiteProcessor::readParams() const
{
    fx::ChainParams p;
    p.inputGainDb     = params.inputGain->load();
    p.gateOn          = params.gateOn->load() > 0.5f;
    p.gateThresholdDb = params.gateThreshold->load();
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
    p.quality         = (int) params.quality->load();
    return p;
}

void EffectSuiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numSamples == 0 || numIn == 0 || maxBlock == 0)
        return;

    const auto p = readParams();
    const int inputMode = (int) params.inputMode->load();
    const bool mute = params.tunerMute->load() > 0.5f;

    if (p.quality != latencyQuality.exchange (p.quality))
        triggerAsyncUpdate();

    float* m = work.getWritePointer (0);
    float* outL = work.getWritePointer (1);
    float* outR = work.getWritePointer (2);
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

        chain.process (m, outL, outR, n, p);

        if (mute)
        {
            juce::FloatVectorOperations::clear (outL, n);
            juce::FloatVectorOperations::clear (outR, n);
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

    if (inPeak > inputPeak.load (std::memory_order_relaxed))   inputPeak.store (inPeak, std::memory_order_relaxed);
    if (outPeak > outputPeak.load (std::memory_order_relaxed)) outputPeak.store (outPeak, std::memory_order_relaxed);
}

void EffectSuiteProcessor::handleAsyncUpdate()
{
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
    irFile = file;
    return true;
}

void EffectSuiteProcessor::clearImpulseResponse()
{
    chain.clearImpulseResponse();
    irFile = juce::File();
}

juce::String EffectSuiteProcessor::getImpulseResponseName() const
{
    return irFile == juce::File() ? juce::String() : irFile.getFileName();
}

void EffectSuiteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("irFile", irFile.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EffectSuiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (! state.isValid() || ! state.hasType (apvts.state.getType()))
            return;

        const juce::String path = state.getProperty ("irFile").toString();
        apvts.replaceState (state);

        if (path.isNotEmpty() && juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
            loadImpulseResponse (juce::File (path));
        else
            clearImpulseResponse();
    }
}

juce::AudioProcessorEditor* EffectSuiteProcessor::createEditor()
{
    return new EffectSuiteEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EffectSuiteProcessor();
}
