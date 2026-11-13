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

    layout.add (std::make_unique<Bool> (id ("odOn"), "Overdrive On", false));
    knob ("odDrive", "Overdrive Drive", 0.5f);
    knob ("odTone", "Overdrive Tone", 0.5f);
    knob ("odLevel", "Overdrive Level", 0.7f);

    juce::StringArray models;
    for (auto* v : fx::voicings::all)
        models.add (v->name);

    layout.add (std::make_unique<Bool> (id ("ampOn"), "Amp On", true));
    layout.add (std::make_unique<Choice> (id ("ampModel"), "Amp Model", models, 0));
    knob ("ampGain", "Amp Gain", 0.5f);
    knob ("ampBass", "Amp Bass", 0.5f);
    knob ("ampMid", "Amp Mid", 0.5f);
    knob ("ampTreble", "Amp Treble", 0.5f);
    knob ("ampPresence", "Amp Presence", 0.5f);
    knob ("ampMaster", "Amp Master", 0.5f);

    layout.add (std::make_unique<Bool> (id ("cabOn"), "Cabinet On", true));
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
               ref ("odOn"), ref ("odDrive"), ref ("odTone"), ref ("odLevel"),
               ref ("ampOn"), ref ("ampModel"), ref ("ampGain"), ref ("ampBass"), ref ("ampMid"),
               ref ("ampTreble"), ref ("ampPresence"), ref ("ampMaster"),
               ref ("cabOn"), ref ("outputLevel"), ref ("quality"), ref ("tunerMute") };
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
    mono.setSize (1, maxBlock, false, false, true);
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

    float* m = mono.getWritePointer (0);
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

        chain.process (m, n, p);

        if (mute)
            juce::FloatVectorOperations::clear (m, n);

        outPeak = juce::jmax (outPeak, juce::FloatVectorOperations::findMaximum (m, n),
                              -juce::FloatVectorOperations::findMinimum (m, n));

        for (int ch = 0; ch < numOut; ++ch)
            buffer.copyFrom (ch, start, m, n);
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
