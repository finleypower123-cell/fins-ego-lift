#include "PluginEditor.h"

namespace felui
{
juce::Font uiFont (float h, bool bold, float hScale)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain).withHorizontalScale (hScale));
}

juce::Font lcdFont (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
}

static void arc (juce::Graphics& g, juce::Point<float> c, float r, float a0, float a1, float width)
{
    if (a1 - a0 < 0.01f) return;
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

// ============================================================================ LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff120819));
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::arrowColourId, col::lilac);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d0f2a));
    setColour (juce::PopupMenu::textColourId, col::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::lilac);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1a0b29));
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a0b29));
    setColour (juce::Label::textColourId, col::ink);
    setColour (juce::Slider::textBoxTextColourId, col::cold);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff120819));
    setColour (juce::Slider::textBoxOutlineColourId, col::line);
    setColour (juce::ToggleButton::textColourId, col::ink2);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    if (s.getProperties().contains ("ego"))
    {
        const int n = 29, lit = juce::roundToInt (pos * (n - 1));
        for (int i = 0; i < n; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (n - 1);
            const bool on = i <= lit && pos > 0.001f;
            const auto p = polar (c, r - 4.0f, a);
            const juce::Colour lc = ! on ? juce::Colour (0xff2c1c3d) : i > n * 0.82f ? col::red : i > n * 0.55f ? juce::Colour (0xffe3b8ff) : juce::Colour (0xffb07ef5);
            if (on) { g.setColour (lc.withAlpha (0.35f)); g.fillEllipse (p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f); }
            g.setColour (lc); g.fillEllipse (p.x - 3.4f, p.y - 3.4f, 6.8f, 6.8f);
        }
        const float cr = r * 0.66f;
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (c.x - cr, c.y - cr + 6, cr * 2, cr * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa08bc0), c.x - cr * 0.35f, c.y - cr * 0.5f, juce::Colour (0xff1d1230), c.x + cr * 0.6f, c.y + cr, true));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int i = 0; i < 48; i += 2) { juce::Path wedge; wedge.addPieSegment (c.x - cr * 0.8f, c.y - cr * 0.8f, cr * 1.6f, cr * 1.6f, i * juce::MathConstants<float>::twoPi / 48, (i + 1) * juce::MathConstants<float>::twoPi / 48, 0.0f); g.fillPath (wedge); }
        g.setColour (col::ink);
        g.drawLine ({ polar (c, cr * 0.25f, ang), polar (c, cr * 0.86f, ang) }, 4.0f);
        return;
    }

    g.setColour (juce::Colour (0xff2e1d40)); arc (g, c, r - 3.0f, a0, a1, 3.5f);
    g.setColour (col::lilac.withAlpha (0.3f)); arc (g, c, r - 3.0f, a0, ang, 7.0f);
    g.setColour (col::lilac); arc (g, c, r - 3.0f, a0, ang, 3.5f);
    const auto eff = s.getProperties()["eff"];
    if (! eff.isVoid())
    {
        const float e = (float) (double) eff;
        if (std::abs (e - pos) > 0.004f) { g.setColour (col::cold.withAlpha (0.9f)); arc (g, c, r + 0.5f, a0, a0 + e * (a1 - a0), 1.6f); }
    }
    const float cr = r * 0.64f;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (c.x - cr, c.y - cr + 3, cr * 2, cr * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a86b6), c.x - cr * 0.3f, c.y - cr * 0.5f, juce::Colour (0xff1d1230), c.x + cr * 0.6f, c.y + cr, true));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);
    g.setColour (col::ink);
    g.drawLine ({ polar (c, cr * 0.3f, ang), polar (c, cr * 0.88f, ang) }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    if (b.getProperties().contains ("power"))
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff321c47), 0, r.getY(), juce::Colour (0xff1d0f2b), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r.reduced (0.5f), 7.0f);
        g.setColour (hi ? col::lilac.withAlpha (0.5f) : col::line); g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        const auto c = r.getCentre();
        if (on) { g.setColour (col::red.withAlpha (0.35f)); g.fillEllipse (c.x - 8, c.y - 8, 16, 16); }
        g.setColour (on ? col::red : juce::Colour (0xff3b1a2a)); g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9, 9);
        return;
    }
    const auto led = juce::Rectangle<float> (r.getX() + 4, r.getCentreY() - 4.5f, 9, 9);
    if (on) { g.setColour (col::cold.withAlpha (0.35f)); g.fillEllipse (led.expanded (3)); }
    g.setColour (on ? col::cold : juce::Colour (0xff2c1c3d)); g.fillEllipse (led);
    g.setColour (col::ink2); g.setFont (uiFont (13.0f));
    g.drawText (b.getButtonText().toUpperCase(), r.withTrimmedLeft (20), juce::Justification::centredLeft);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool seg = b.getProperties().contains ("seg");
    if (seg)
    {
        g.setColour (on ? col::lilac : juce::Colour (0xff1d0f2a)); g.fillRect (r);
        return;
    }
    if (b.getProperties().contains ("humble") && on)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b2440), 0, r.getY(), juce::Colour (0xff3f1325), 0, r.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b2154), 0, r.getY(), juce::Colour (0xff26133a), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r.translated (0, down ? 1.0f : 0.0f), 7.0f);
    g.setColour (hi ? col::lilac.withAlpha (0.45f) : col::line); g.drawRoundedRectangle (r, 7.0f, 1.0f);
    if (b.getProperties().contains ("humble"))
    {
        const auto led = juce::Rectangle<float> (r.getX() + 10, r.getCentreY() - 4.5f, 9, 9);
        if (on) { g.setColour (col::red.withAlpha (0.4f)); g.fillEllipse (led.expanded (3)); }
        g.setColour (on ? col::red : juce::Colour (0xff3b1a2a)); g.fillEllipse (led);
    }
}

// ============================================================================ Knob
Knob::Knob (FelProcessor& p, const fel::KnobDef& k, std::function<void (const juce::String&)> info)
    : def (k), onInfo (std::move (info))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (230);
    slider.setDoubleClickReturnValue (true, k.def);
    slider.setTitle (juce::String (k.joke) + ": " + k.what);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, k.id, slider);

    read.setFont (lcdFont (17.0f)); read.setColour (juce::Label::textColourId, col::cold); read.setJustificationType (juce::Justification::centred);
    joke.setFont (uiFont (12.5f)); joke.setJustificationType (juce::Justification::centredTop); joke.setMinimumHorizontalScale (0.75f);
    joke.setText (juce::String (k.joke).toUpperCase(), juce::dontSendNotification);
    what.setFont (uiFont (10.5f, true, 0.9f)); what.setColour (juce::Label::textColourId, col::ink3); what.setJustificationType (juce::Justification::centredTop);
    what.setMinimumHorizontalScale (0.7f);
    for (auto* l : { &read, &joke, &what }) { l->setInterceptsMouseClicks (false, false); l->setBorderSize ({}); addAndMakeVisible (*l); }

    slider.onValueChange = [this]
    {
        read.setText (fel::formatValue (def, slider.getValue(), lastRev < 0 ? 0 : lastRev, lastDly < 0 ? 0 : lastDly), juce::dontSendNotification);
        if (slider.isMouseButtonDown() || slider.hasKeyboardFocus (false))
            onInfo (juce::String (def.joke).toUpperCase() + " -> " + what.getText() + " " + read.getText());
    };
    slider.onDragStart = [this] { onInfo (juce::String (def.joke).toUpperCase() + " -> " + what.getText() + " " + read.getText()); };
}

void Knob::resized()
{
    const int w = getWidth(), dial = 54, top = juce::jmax (0, (getHeight() - 120) / 2);
    slider.setBounds ((w - dial) / 2, top, dial, dial);
    read.setBounds (0, top + 55, w, 17);
    joke.setBounds (2, top + 73, w - 4, 34);
    what.setBounds (2, top + 107, w - 4, 13);
}

void Knob::refresh (double effNorm, int revType, int dlyType)
{
    const double cur = slider.getProperties()["eff"].isVoid() ? -1.0 : (double) slider.getProperties()["eff"];
    if (std::abs (cur - effNorm) > 0.001) { slider.getProperties().set ("eff", effNorm); slider.repaint(); }
    if (revType != lastRev || dlyType != lastDly)
    {
        lastRev = revType; lastDly = dlyType;
        what.setText (juce::String (fel::whatLabel (def, revType, dlyType)).toUpperCase(), juce::dontSendNotification);
        read.setText (fel::formatValue (def, slider.getValue(), revType, dlyType), juce::dontSendNotification);
    }
}

// ============================================================================ Segmented
Segmented::Segmented (juce::RangedAudioParameter& prm, const juce::StringArray& opts, std::function<void (const juce::String&)> info)
    : att (prm, [this] (float v)
      {
          current = juce::roundToInt (v);
          for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == current, juce::dontSendNotification);
      }),
      onInfo (std::move (info))
{
    for (int i = 0; i < opts.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (opts[i].toUpperCase()));
        b->getProperties().set ("seg", true);
        b->setColour (juce::TextButton::textColourOffId, col::ink2);
        b->onClick = [this, i, name = opts[i]] { att.setValueAsCompleteGesture ((float) i); onInfo (name.toUpperCase()); };
        addAndMakeVisible (b);
    }
    att.sendInitialUpdate();
}

void Segmented::resized()
{
    const float w = (float) getWidth() / (float) juce::jmax (1, buttons.size());
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setBounds (juce::roundToInt (i * w), 0, juce::roundToInt ((i + 1) * w) - juce::roundToInt (i * w), getHeight());
}

void Segmented::paint (juce::Graphics&) {}

// ============================================================================ Section
Section::Section (FelProcessor& p, int index, std::function<void (const juce::String&)> info)
    : def (fel::SECTIONS[index]), proc (p)
{
    power.getProperties().set ("power", true);
    power.setTitle (juce::String (def.name) + " on or off");
    addAndMakeVisible (power);
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, def.onParam, power);
    power.onClick = [this, info] { info (juce::String (def.name).toUpperCase() + (power.getToggleState() ? " ON" : " OFF")); };

    if (def.typeParam != nullptr)
    {
        const juce::String label (def.typeLabel);
        seg = std::make_unique<Segmented> (*p.apvts.getParameter (def.typeParam), juce::StringArray::fromTokens (def.typeOptions, "|", ""),
                                           [info, label] (const juce::String& o) { info (label.toUpperCase() + " -> " + o); });
        addAndMakeVisible (*seg);
    }
    for (int i = 0; i < def.numKnobs; ++i)
        addAndMakeVisible (knobs.add (new Knob (p, fel::KNOBS[def.firstKnob + i], info)));

    if (def.bpm)
    {
        bpm.setSliderStyle (juce::Slider::IncDecButtons);
        bpm.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 26);
        bpm.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        bpm.setTitle ("Delay tempo");
        addAndMakeVisible (bpm);
        bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "bpm", bpm);
        bpm.setTextValueSuffix ("");
        addAndMakeVisible (sync);
        syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bpmSync", sync);
        bpmLabel.setFont (uiFont (11.0f)); bpmLabel.setColour (juce::Label::textColourId, col::ink3);
        bpmLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (bpmLabel);
    }
}

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::well); g.fillRoundedRectangle (r, 11.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 14, false));
    g.fillRoundedRectangle (r, 11.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r, 11.0f, 1.0f);
    g.setColour (col::ink); g.setFont (uiFont (13.0f, true, 1.0f));
    g.drawText (juce::String (def.name).toUpperCase(), 48, 11, getWidth() - 60, 16, juce::Justification::centredLeft);
    g.setColour (col::ink3); g.setFont (uiFont (14.0f, false, 0.92f));
    g.drawText (juce::String::fromUTF8 (def.what), 48, 28, getWidth() - 60, 16, juce::Justification::centredLeft);
    if (seg != nullptr)
    {
        g.setColour (col::ink3); g.setFont (uiFont (11.0f, true, 0.95f));
        g.drawText (juce::String (def.typeLabel).toUpperCase(), 12, 52, 90, 18, juce::Justification::centredLeft);
        g.setColour (col::line); g.drawRoundedRectangle (seg->getBounds().toFloat().expanded (0.5f), 6.0f, 1.0f);
    }
    if (! power.getToggleState()) { g.setColour (col::well.withAlpha (0.55f)); g.fillRect (getLocalBounds().withTrimmedTop (46).reduced (4, 4)); }
}

void Section::resized()
{
    power.setBounds (12, 12, 26, 26);
    int top = 52;
    if (seg != nullptr)
    {
        const int n = juce::StringArray::fromTokens (def.typeOptions, "|", "").size();
        const int optW = def.bpm ? 70 : juce::jmin (78, (getWidth() - 112) / juce::jmax (1, n));
        seg->setBounds (100, 49, n * optW, 26);
        top = 84;
        if (def.bpm)
        {
            const int x = seg->getRight() + 12;
            bpmLabel.setBounds (0, 0, 0, 0);
            bpm.setBounds (x, 49, 104, 26);
            sync.setBounds (x + 112, 49, getWidth() - (x + 112) - 8, 26);
        }
    }
    const int n = knobs.size();
    const float cw = (float) (getWidth() - 20) / (float) juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds (10 + juce::roundToInt (i * cw), top, juce::roundToInt (cw), getHeight() - top - 6);
}

void Section::refresh (const fel::Params& user, const fel::Params& eff)
{
    const int rt = (int) user.revType, dt = (int) user.dlyType;
    for (auto* k : knobs)
    {
        auto* prm = proc.apvts.getParameter (k->def.id);
        double e = *fel::paramPtr (const_cast<fel::Params&> (eff), k->def.id);
        k->refresh (prm->convertTo0to1 ((float) e), rt, dt);
    }
    if (def.bpm)
    {
        const float hb = proc.hostBpm.load();
        const bool synced = sync.getToggleState() && hb > 1.0f;
        bpm.setEnabled (! synced);
        sync.setButtonText (synced ? "Session " + juce::String (hb, 1) + " BPM" : "Follow session");
    }
    if (power.getToggleState() != lastOn) { lastOn = power.getToggleState(); repaint(); }
}

// ============================================================================ Meters / LCD
void Meters::setLevels (float in, float out, float gr)
{
    auto lv = [] (float x) { return juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (x + 1e-9f) + 60.0f) / 60.0f); };
    dIn = juce::jmax (lv (in), dIn - 0.02f); dOut = juce::jmax (lv (out), dOut - 0.02f);
    dGr = juce::jmax (juce::jlimit (0.0f, 1.0f, -20.0f * std::log10 (juce::jmax (gr, 1e-6f)) / 12.0f), dGr - 0.03f);
    repaint();
}

void Meters::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::well); g.fillRoundedRectangle (r, 11.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r, 11.0f, 1.0f);
    const float top = 12, bottom = r.getBottom() - 24, h = bottom - top;
    g.setFont (lcdFont (13.0f)); g.setColour (col::ink3);
    for (int i = 0; i <= 5; ++i) g.drawText (juce::String (-12 * i), 6, juce::roundToInt (top + h * i / 5.0f - 6), 26, 12, juce::Justification::centredRight);
    const char* labels[3] = { "IN", "OUT", "LIM" };
    const float vals[3] = { dIn, dOut, dGr };
    for (int i = 0; i < 3; ++i)
    {
        const juce::Rectangle<float> bar (38.0f + i * 20.0f, top, 13.0f, h);
        g.setColour (juce::Colour (0xff0a0611)); g.fillRoundedRectangle (bar, 3.0f);
        if (i < 2)
        {
            const auto fill = bar.withTop (bar.getBottom() - h * vals[i]);
            g.setGradientFill (juce::ColourGradient (col::lilacDeep, 0, bar.getBottom(), col::red, 0, bar.getY(), false));
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (fill.toNearestInt()); g.fillRect (bar);
        }
        else
        {
            g.setColour (col::red); g.fillRect (bar.withHeight (h * vals[i]));
        }
        g.setColour (juce::Colour (0xd90d0614));
        for (float yy = bar.getY(); yy < bar.getBottom(); yy += 5.0f) g.fillRect (bar.getX(), yy, bar.getWidth(), 1.0f);
        g.setColour (col::ink3); g.setFont (uiFont (11.0f));
        g.drawText (labels[i], juce::roundToInt (bar.getCentreX() - 15), juce::roundToInt (bottom + 5), 30, 14, juce::Justification::centred);
    }
}

void Lcd::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff04050b)); g.fillRoundedRectangle (r, 10.0f);
    g.setColour (col::lcd); g.fillRoundedRectangle (r.reduced (2.0f), 9.0f);
    const int x = 16, w = getWidth() - 32;
    g.setColour (col::cold); g.setFont (lcdFont (24.0f));
    g.drawText (preset, x, 12, w, 28, juce::Justification::centredLeft, true);
    g.setColour (col::cold.withAlpha (0.72f)); g.setFont (lcdFont (17.0f));
    g.drawText (joke, x, 42, w, 22, juce::Justification::centredLeft, true);
    g.setColour (col::cold.withAlpha (0.25f)); g.drawHorizontalLine (70, (float) x, (float) (x + w));
    g.setColour (col::cold.withAlpha (0.75f)); g.setFont (lcdFont (16.0f));
    g.drawText (status, x, 78, w, 20, juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xffc9ecff)); g.setFont (lcdFont (18.0f));
    g.drawText (info, x, 102, w, 24, juce::Justification::centredLeft, true);
    g.setColour (col::cold.withAlpha (0.045f));
    for (int yy = 3; yy < getHeight() - 2; yy += 3) g.drawHorizontalLine (yy, 3.0f, r.getRight() - 3.0f);
}

// ============================================================================ Content
Content::Content (FelProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < fel::NUM_PRESETS; ++i)
        presets.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + juce::String (fel::PRESETS[i].name).toUpperCase(), i + 1);
    presets.onChange = [this] { const int id = presets.getSelectedId(); if (id > 0 && id - 1 != proc.currentPreset.load()) proc.loadPreset (id - 1); setInfo ("PRESET " + juce::String (id).paddedLeft ('0', 2) + " LOADED"); };
    presets.setTitle ("Preset");
    addAndMakeVisible (presets);
    prev.onClick = [this] { proc.loadPreset ((proc.currentPreset.load() + fel::NUM_PRESETS - 1) % fel::NUM_PRESETS); };
    next.onClick = [this] { proc.loadPreset ((proc.currentPreset.load() + 1) % fel::NUM_PRESETS); };
    prev.setTitle ("Previous preset"); next.setTitle ("Next preset");
    addAndMakeVisible (prev); addAndMakeVisible (next);

    humbleBtn.getProperties().set ("humble", true);
    humbleBtn.setClickingTogglesState (true);
    humbleBtn.setColour (juce::TextButton::textColourOnId, col::ink);
    humbleBtn.setConnectedEdges (0);
    addAndMakeVisible (humbleBtn);
    humbleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "humble", humbleBtn);
    humbleBtn.onClick = [this] { setInfo (humbleBtn.getToggleState() ? "HUMBLE MODE: RAW VOCAL, SAME TIMING. WE WILL NEVER SPEAK OF THIS." : "EGO RESTORED. AS IT SHOULD BE."); };

    egoKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    egoKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    egoKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    egoKnob.setMouseDragSensitivity (260);
    egoKnob.setDoubleClickReturnValue (true, 0.5);
    egoKnob.getProperties().set ("ego", true);
    egoKnob.setTitle ("The Ego macro");
    addAndMakeVisible (egoKnob);
    egoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "ego", egoKnob);
    egoKnob.onDragStart = [this] { setInfo ("THE EGO -> " + juce::String (fel::egoStage (egoKnob.getValue()))); };
    egoStage.setFont (juce::Font (juce::FontOptions (21.0f, juce::Font::bold | juce::Font::italic).withHorizontalScale (0.9f)));
    egoStage.setColour (juce::Label::textColourId, col::lilac);
    egoSub.setFont (uiFont (12.5f, false, 0.92f)); egoSub.setColour (juce::Label::textColourId, col::ink3);
    for (auto* l : { &egoStage, &egoSub }) { l->setJustificationType (juce::Justification::centredLeft); l->setBorderSize ({}); addAndMakeVisible (*l); }

    addAndMakeVisible (lcd);
    addAndMakeVisible (meters);
    for (int i = 0; i < fel::NUM_SECTIONS; ++i)
        addAndMakeVisible (sections.add (new Section (p, i, [this] (const juce::String& s) { setInfo (s); })));

    lcd.info = "Drag a knob. The thin blue arcs show where The Ego has pushed it.";
    setSize (W, H);
    resized();
    startTimerHz (30);
}

Content::~Content() { stopTimer(); setLookAndFeel (nullptr); }

void Content::setInfo (const juce::String& s) { lcd.info = s; infoTicks = 140; lcd.repaint(); }

void Content::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (col::plateTop, 0, 0, juce::Colour (0xff1b0c27), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int x = 0; x < W; x += 3) g.drawVerticalLine (x, 0.0f, (float) H);
    for (auto pt : { juce::Point<float> (9, 9), { (float) W - 21, 9 }, { 9, (float) H - 21 }, { (float) W - 21, (float) H - 21 } })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa493bb), pt.x + 4, pt.y + 3, juce::Colour (0xff3a2a4d), pt.x + 12, pt.y + 12, true));
        g.fillEllipse (pt.x, pt.y, 12, 12);
        g.setColour (juce::Colour (0xff1b1124)); g.drawLine (pt.x + 2.5f, pt.y + 7.5f, pt.x + 9.5f, pt.y + 4.5f, 1.6f);
    }
    g.setColour (col::ink3); g.setFont (uiFont (12.0f, true, 1.0f));
    g.drawText ("EGO LABS  .  MODEL FEL-5000  .  HAND-WIRED BY A GENIUS", 28, 14, 600, 16, juce::Justification::centredLeft);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, 32, juce::Colour (0xff9a69e2), 0, 74, false));
    g.setFont (juce::Font (juce::FontOptions (46.0f, juce::Font::bold).withHorizontalScale (1.04f)));
    g.drawText ("FIN'S EGO LIFT", 26, 30, 600, 50, juce::Justification::centredLeft);
    g.setColour (col::ink2); g.setFont (uiFont (13.0f, false, 1.0f));
    g.drawText ("V O C A L   L I F T", 28, 80, 160, 18, juce::Justification::centredLeft);
    g.setColour (col::lilac); g.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold | juce::Font::italic).withHorizontalScale (0.92f)));
    g.drawText ("SUPER MEGA SONIC", 182, 74, 260, 30, juce::Justification::centredLeft);
    g.setColour (col::red); g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("5000", 410, 72, 100, 32, juce::Justification::centredLeft);
    g.setColour (col::line); g.drawHorizontalLine (100, 16.0f, (float) W - 16);
    g.setFont (uiFont (13.0f, true, 1.0f));
    auto er = juce::Rectangle<float> (968, 108, 216, 136);
    g.setColour (col::well); g.fillRoundedRectangle (er, 11.0f);
    g.setColour (col::line); g.drawRoundedRectangle (er, 11.0f, 1.0f);
    g.setColour (col::ink); g.drawText ("T H E   E G O", 1090, 118, 100, 16, juce::Justification::centredLeft);
}

void Content::resized()
{
    prev.setBounds (640, 26, 38, 36);
    presets.setBounds (684, 26, 456, 36);
    next.setBounds (1146, 26, 38, 36);
    humbleBtn.setBounds (1004, 66, 180, 28);
    lcd.setBounds (16, 108, 828, 136);
    meters.setBounds (856, 108, 100, 136);
    egoKnob.setBounds (974, 114, 112, 124);
    egoStage.setBounds (1090, 150, 92, 26);
    egoSub.setBounds (1090, 178, 92, 50);
    const juce::Rectangle<int> b[7] = { { 16, 256, 440, 206 }, { 468, 256, 352, 206 }, { 16, 472, 516, 206 }, { 832, 256, 352, 206 },
                                        { 544, 472, 640, 206 }, { 16, 688, 598, 206 }, { 626, 688, 558, 206 } };
    for (int i = 0; i < sections.size() && i < 7; ++i) sections[i]->setBounds (b[i]);
}

void Content::timerCallback()
{
    meters.setLevels (proc.meterIn.exchange (0.0f), proc.meterOut.exchange (0.0f), proc.meterGr.exchange (1.0f));

    const fel::Params user = proc.userParams();
    const double egoV = proc.egoValue();
    const fel::Params eff = fel::applyEgo (user, egoV);
    for (auto* s : sections) s->refresh (user, eff);

    egoStage.setText (fel::egoStage (egoV), juce::dontSendNotification);
    int egoCount = 0; for (const auto& k : fel::KNOBS) if (k.egoType != 0) ++egoCount;
    egoSub.setText (juce::String (juce::roundToInt (egoV * 100)) + "%  .  moves " + juce::String (egoCount) + " controls at once", juce::dontSendNotification);

    const int pi = proc.currentPreset.load();
    const bool edited = proc.presetEdited.load();
    if (presets.getSelectedId() != pi + 1) presets.setSelectedId (pi + 1, juce::dontSendNotification);
    const juce::String pName = "P" + juce::String (pi + 1).paddedLeft ('0', 2) + " " + juce::String (fel::PRESETS[pi].name).toUpperCase() + (edited ? " *" : "");
    const juce::String pJoke = juce::String (fel::PRESETS[pi].joke) + (edited ? " (Edited. Naturally.)" : "");
    const int latMs = juce::roundToInt (1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate()));
    const float hb = proc.hostBpm.load();
    const juce::String status = "LATENCY " + juce::String (latMs) + " MS, COMPENSATED BY THE HOST" + (hb > 1 ? "  .  SESSION " + juce::String (hb, 1) + " BPM" : "");
    if (pName != lcd.preset || pJoke != lcd.joke || status != lcd.status) { lcd.preset = pName; lcd.joke = pJoke; lcd.status = status; lcd.repaint(); }
    if (infoTicks > 0 && --infoTicks == 0)
    {
        lcd.info = user.bypass > 0.5 || humbleBtn.getToggleState() ? "Humble mode: raw vocal. We will never speak of this." : "Drag a knob. The thin blue arcs show where The Ego has pushed it.";
        lcd.repaint();
    }
}
} // namespace felui

// ============================================================================ Editor
FelEditor::FelEditor (FelProcessor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    constrainer.setFixedAspectRatio ((double) felui::Content::W / felui::Content::H);
    constrainer.setSizeLimits (600, 452, 1800, 1356);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (1020, 769);
}

void FelEditor::resized()
{
    const float s = (float) getWidth() / (float) felui::Content::W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, felui::Content::W, felui::Content::H);
}
