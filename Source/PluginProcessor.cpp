#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const char* const SECTION_ON_NAMES[] = { "Mic Technique on", "Tortured on", "Persona on", "Y2K Phase on", "Echo Chamber on", "The Cathedral on", "Mastered by me on" };

juce::NormalisableRange<float> rangeFor (const fel::KnobDef& k)
{
    if (k.logCurve)
    {
        juce::NormalisableRange<float> r (k.min, k.max,
            [] (float s, float e, float n) { return s * std::pow (e / s, n); },
            [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); });
        return r;
    }
    if (k.step > 0) return { k.min, k.max, k.step };
    return { k.min, k.max };
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout FelProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using PID = juce::ParameterID;

    layout.add (std::make_unique<juce::AudioParameterFloat> (PID { "ego", 1 }, "The Ego (macro)", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (fel::egoStage (v)) + " " + juce::String (juce::roundToInt (v * 100)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("0123456789.").getFloatValue() / 100.0f; })));
    layout.add (std::make_unique<juce::AudioParameterBool> (PID { "humble", 1 }, "Humble mode (bypass)", false));

    for (int s = 0; s < fel::NUM_SECTIONS; ++s)
    {
        const auto& sec = fel::SECTIONS[s];
        layout.add (std::make_unique<juce::AudioParameterBool> (PID { sec.onParam, 1 }, SECTION_ON_NAMES[s], true));
        if (sec.typeParam != nullptr)
        {
            juce::StringArray opts = juce::StringArray::fromTokens (sec.typeOptions, "|", "");
            layout.add (std::make_unique<juce::AudioParameterChoice> (PID { sec.typeParam, 1 }, juce::String (sec.typeLabel) + " (" + juce::String::fromUTF8 (sec.what) + ")", opts, 0));
        }
        for (int i = 0; i < sec.numKnobs; ++i)
        {
            const fel::KnobDef* k = &fel::KNOBS[sec.firstKnob + i];
            const juce::String fmt (k->fmt);
            auto attrs = juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([k] (float v, int) { return juce::String (fel::formatValue (*k, v)); })
                .withValueFromStringFunction ([fmt] (const juce::String& t)
                {
                    const float v = t.retainCharacters ("-0123456789.").getFloatValue();
                    return (fmt == "pct" || fmt == "width") ? v / 100.0f : (fmt == "hz" && t.containsIgnoreCase ("k")) ? v * 1000.0f : v;
                });
            layout.add (std::make_unique<juce::AudioParameterFloat> (PID { k->id, 1 }, juce::String (k->what) + " (" + k->joke + ")", rangeFor (*k), k->def, attrs));
        }
        if (sec.bpm)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (PID { "bpm", 1 }, "Delay tempo (the band follows my tempo)", juce::NormalisableRange<float> (40.0f, 220.0f, 1.0f), 120.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " BPM"; })));
            layout.add (std::make_unique<juce::AudioParameterBool> (PID { "bpmSync", 1 }, "Delay follows session tempo", true));
        }
    }
    return layout;
}

FelProcessor::FelProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FinsEgoLift", createLayout())
{
    humbleParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("humble"));
    ego = apvts.getRawParameterValue ("ego");
    humble = apvts.getRawParameterValue ("humble");
    bpmSync = apvts.getRawParameterValue ("bpmSync");

    // Every engine field that has a matching parameter is bound once, in a fixed order.
    for (const auto& m : fel::PARAM_MEMBERS)
        if (auto* raw = apvts.getRawParameterValue (m.id))
            bindings.emplace_back (m.ptr, raw);

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);

    loadPreset (0);
    presetEdited = false;
    // Report a sensible latency before the host calls prepareToPlay; prepareToPlay updates it for the real rate.
    engine.prepare (48000.0);
    setLatencySamples (engine.getLatency());
}

FelProcessor::~FelProcessor()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

bool FelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out == juce::AudioChannelSet::stereo())
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    if (out == juce::AudioChannelSet::mono())
        return in == juce::AudioChannelSet::mono();
    return false;
}

fel::Params FelProcessor::userParams() const
{
    fel::Params p;
    for (const auto& b : bindings) p.*(b.first) = (double) b.second->load();
    return p;
}

void FelProcessor::pushParams (bool init)
{
    fel::Params user = userParams();
    const float hb = hostBpm.load();
    if (bpmSync->load() > 0.5f && hb > 1.0f) user.bpm = hb;
    lastBpmUsed = (float) user.bpm;
    fel::Params eff = fel::applyEgo (user, ego->load());
    eff.bypass = (humble->load() > 0.5f || forcedBypass.load()) ? 1.0 : 0.0;
    eff.fxOnly = 0;
    engine.setParams (eff, init);
}

void FelProcessor::parameterChanged (const juce::String&, float)
{
    dirty = true;
    if (! loadingPreset.load()) presetEdited = true;
}

void FelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    pushParams (true);
    dirty = false;
    scratch.assign ((size_t) juce::jmax (samplesPerBlock, 512), 0.0f);
    setLatencySamples (engine.getLatency());
}

void FelProcessor::run (juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if (n <= 0 || outs <= 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
            {
                hostBpm = (float) *bpm;
                if (bpmSync->load() > 0.5f && std::abs ((float) *bpm - lastBpmUsed) > 0.01f) dirty = true;
            }

    if (dirty.exchange (false)) pushParams (false);

    float* L = buffer.getWritePointer (0);
    const float* inR = ins > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0);
    if (outs > 1)
    {
        engine.process (L, inR, L, buffer.getWritePointer (1), n);
    }
    else
    {
        for (int s = 0; s < n; s += (int) scratch.size())
        {
            const int c = juce::jmin ((int) scratch.size(), n - s);
            engine.process (L + s, inR + s, L + s, scratch.data(), c);
            for (int i = 0; i < c; ++i) L[s + i] = 0.5f * (L[s + i] + scratch[(size_t) i]);
        }
    }
    for (int ch = 2; ch < outs; ++ch) buffer.clear (ch, 0, n);

    const auto m = engine.takeMeters();
    meterIn = juce::jmax (meterIn.load(), (float) m.in);
    meterOut = juce::jmax (meterOut.load(), (float) m.out);
    meterGr = juce::jmin (meterGr.load(), (float) m.gr);
}

void FelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (forcedBypass.exchange (false)) dirty = true;
    run (buffer);
}

void FelProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Stay latency-compensated while bypassed: the engine outputs the delayed dry signal.
    if (! forcedBypass.exchange (true)) dirty = true;
    run (buffer);
}

void FelProcessor::loadPreset (int index)
{
    index = juce::jlimit (0, fel::NUM_PRESETS - 1, index);
    const fel::Params p = fel::presetParams (index);
    const fel::Params defaults;
    loadingPreset = true;
    auto set = [this] (const char* id, double v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
            prm->endChangeGesture();
        }
    };
    for (const auto& m : fel::PARAM_MEMBERS)
    {
        const juce::String id (m.id);
        if (id == "bpm" || id == "bypass" || id == "fxOnly" || id.endsWith ("On")) continue;   // presets keep tempo and module switches
        set (m.id, p.*(m.ptr));
    }
    for (int s = 0; s < fel::NUM_SECTIONS; ++s) set (fel::SECTIONS[s].onParam, 1.0);
    set ("ego", fel::PRESETS[index].ego);
    juce::ignoreUnused (defaults);
    loadingPreset = false;
    currentPreset = index;
    presetEdited = false;
    dirty = true;
}

void FelProcessor::setCurrentProgram (int index) { loadPreset (index); }

const juce::String FelProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, fel::NUM_PRESETS) ? juce::String (fel::PRESETS[index].name) : juce::String();
}

void FelProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.setProperty ("edited", presetEdited.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FelProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            loadingPreset = true;
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            // Push every value explicitly so each parameter lands exactly on its saved value.
            for (auto* p : getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    const auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                    if (child.isValid() && child.hasProperty ("value"))
                        rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
                }
            loadingPreset = false;
            currentPreset = juce::jlimit (0, fel::NUM_PRESETS - 1, (int) tree.getProperty ("preset", 0));
            presetEdited = (bool) tree.getProperty ("edited", false);
            dirty = true;
        }
}

juce::AudioProcessorEditor* FelProcessor::createEditor() { return new FelEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FelProcessor(); }
