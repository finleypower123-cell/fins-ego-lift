#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace felui
{
namespace col
{
    const juce::Colour voidc { 0xff0d0614 }, chassis { 0xff22112f }, plateTop { 0xff321a49 }, well { 0xff150a20 };
    const juce::Colour line { 0x29c9a0ff }, ink { 0xfff1e7ff }, ink2 { 0xffbba6dc }, ink3 { 0xff8770a8 };
    const juce::Colour lilac { 0xffc99dff }, lilacDeep { 0xff7e52c8 }, cold { 0xff88d8ff }, lcd { 0xff090d1c }, red { 0xffff4f74 };
}

juce::Font uiFont (float h, bool bold = true, float hScale = 0.86f);
juce::Font lcdFont (float h);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (13.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (15.0f); }
    juce::Font getPopupMenuFont() override { return uiFont (16.0f, true, 0.9f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

class Knob : public juce::Component
{
public:
    Knob (FelProcessor&, const fel::KnobDef&, std::function<void (const juce::String&)> info);
    void resized() override;
    void refresh (double effNorm, int revType, int dlyType);
    const fel::KnobDef& def;
    juce::Slider slider;
private:
    juce::Label read, joke, what;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    std::function<void (const juce::String&)> onInfo;
    int lastRev = -1, lastDly = -1;
};

class Segmented : public juce::Component
{
public:
    Segmented (juce::RangedAudioParameter&, const juce::StringArray& options, std::function<void (const juce::String&)> info);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment att;
    std::function<void (const juce::String&)> onInfo;
    int current = 0;
};

class Section : public juce::Component
{
public:
    Section (FelProcessor&, int index, std::function<void (const juce::String&)> info);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fel::Params& user, const fel::Params& eff);
    const fel::SectionDef& def;
private:
    FelProcessor& proc;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::unique_ptr<Segmented> seg;
    juce::OwnedArray<Knob> knobs;
    // tempo (delay section only)
    juce::Slider bpm;
    juce::ToggleButton sync { "Follow session" };
    juce::Label bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt;
    bool lastOn = true;
};

class Meters : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevels (float in, float out, float gr);
private:
    float dIn = 0, dOut = 0, dGr = 0;
};

class Lcd : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    juce::String preset, joke, info, status;
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (FelProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1200, H = 904;
private:
    void timerCallback() override;
    void setInfo (const juce::String&);
    FelProcessor& proc;
    LookAndFeel lnf;
    juce::ComboBox presets;
    juce::TextButton prev { "<" }, next { ">" }, humbleBtn { "Humble mode" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> humbleAtt;
    juce::Slider egoKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> egoAtt;
    juce::Label egoStage, egoSub;
    Lcd lcd;
    Meters meters;
    juce::OwnedArray<Section> sections;
    int infoTicks = 0, lastPreset = -1;
};
} // namespace felui

class FelEditor : public juce::AudioProcessorEditor
{
public:
    explicit FelEditor (FelProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (felui::col::voidc); }
private:
    felui::Content content;
    juce::ComponentBoundsConstrainer constrainer;
};
