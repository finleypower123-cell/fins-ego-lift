#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FelModel.h"

class FelProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    FelProcessor();
    ~FelProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 25.0; }

    int getNumPrograms() override { return fel::NUM_PRESETS; }
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return humbleParam; }

    // Called from the editor (message thread).
    void loadPreset (int index);
    fel::Params userParams() const;   // what the knobs show
    double egoValue() const { return ego->load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterIn { 0 }, meterOut { 0 }, meterGr { 1 };
    std::atomic<float> hostBpm { 0 };
    std::atomic<int> currentPreset { 0 };
    std::atomic<bool> presetEdited { false };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float) override;
    void pushParams (bool init);
    void run (juce::AudioBuffer<float>& buffer);

    fel::Engine engine;
    std::atomic<bool> dirty { true }, forcedBypass { false }, loadingPreset { false };
    std::vector<float> scratch;
    juce::AudioParameterBool* humbleParam = nullptr;
    std::atomic<float>* ego = nullptr;
    std::atomic<float>* humble = nullptr;
    std::atomic<float>* bpmSync = nullptr;
    std::vector<std::pair<double fel::Params::*, std::atomic<float>*>> bindings;
    float lastBpmUsed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FelProcessor)
};
