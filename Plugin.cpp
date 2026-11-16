#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace
{
struct Control
{
    const char* id;
    const char* name;
    float minimum, maximum, initial;
};

constexpr std::array<Control, 4> controls {{
    {"time", "Time (ms)", 45, 1000, 340},
    {"feedback", "Repeats (%)", 0, 92, 42},
    {"flutter", "Wow & Flutter (%)", 0, 100, 24},
    {"mix", "Mix (%)", 0, 100, 27}
}};

using State = juce::AudioProcessorValueTreeState;

State::ParameterLayout makeParameters()
{
    State::ParameterLayout layout;
    for (const auto& control : controls)
    {
        const juce::ParameterID id {control.id, 1};
        layout.add(std::make_unique<juce::AudioParameterFloat>(id, control.name,
            juce::NormalisableRange<float>{control.minimum, control.maximum, 1.0f},
            control.initial));
    }
    return layout;
}

class FrootEditor final : public juce::AudioProcessorEditor
{
public:
    FrootEditor(juce::AudioProcessor& processor, State& state) : AudioProcessorEditor(processor)
    {
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            const auto& control = controls[knobIndices[i]];
            auto& knob = knobs[i];
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 23);
            knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffffb475));
            knob.setName(control.name);
            addAndMakeVisible(knob);
            knobLinks[i] = std::make_unique<State::SliderAttachment>(state, control.id, knob);
        }
        setSize(580, 264);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff221416));
        g.setColour(juce::Colours::white);
        g.setFont(26.0f);
        g.drawText("Froot by the Fut", 20, 12, 580, 38, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff43271f));
        g.fillRoundedRectangle(20, 60, 540, 184, 12);
        g.setColour(juce::Colours::white);
        g.setFont(17.0f);
        g.drawText("Tape echo", 34, 67, 120, 25, juce::Justification::centredLeft);
        g.setFont(13.0f);
        for (size_t i = 0; i < knobs.size(); ++i)
            g.drawText(controls[knobIndices[i]].name, knobX[i], 208, 106, 23, juce::Justification::centred);
    }

    void resized() override
    {
        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i].setBounds(knobX[i], 102, 106, 103);
    }

private:
    static constexpr std::array<size_t, 4> knobIndices {0, 1, 2, 3};
    static constexpr std::array<int, 4> knobX {36, 169, 302, 435};
    std::array<juce::Slider, 4> knobs;
    std::array<std::unique_ptr<State::SliderAttachment>, 4> knobLinks;
};

class FrootEcho final : public juce::AudioProcessor
{
public:
    FrootEcho() : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        state(*this, nullptr, "FrootEcho", makeParameters())
    {
        for (size_t i = 0; i < controls.size(); ++i)
            values[i] = state.getRawParameterValue(controls[i].id);
    }

    const juce::String getName() const override { return "Froot by the Fut"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 85.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new FrootEditor(*this, state); }
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto output = layout.getMainOutputChannelSet();
        return layout.getMainInputChannelSet() == output
            && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
    }

    void prepareToPlay(double sampleRate, int) override
    {
        rate = static_cast<float>(sampleRate);
        history.setSize(2, static_cast<int>(1.02 * sampleRate) + 8);
        reset();
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            knobs[i].reset(sampleRate, i == 0 ? 0.12 : 0.025);
            knobs[i].setCurrentAndTargetValue(values[i]->load());
        }
        tapeTone = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 3300.0f / rate);
    }

    void reset() override
    {
        history.clear();
        tone.fill(0.0f);
        write = 0;
        wow = flutter = 0.0f;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;
        const auto length = history.getNumSamples();
        const auto cycle = juce::MathConstants<float>::twoPi;
        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i].setTargetValue(values[i]->load());

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto time = knobs[0].getNextValue();
            const auto feedback = knobs[1].getNextValue() * 0.01f;
            const auto depth = knobs[2].getNextValue() * 0.01f;
            const auto mix = knobs[3].getNextValue() * 0.01f;
            const auto modulation = depth * (3.6f * std::sin(wow) + 0.65f * std::sin(flutter));
            auto index = static_cast<float>(write) - (time + modulation) * 0.001f * rate;
            if (index < 0.0f)
                index += static_cast<float>(length);
            const auto left = static_cast<int>(index);
            const auto fraction = index - static_cast<float>(left);
            const auto right = (left + 1) % length;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto dry = buffer.getSample(channel, sample);
                const auto delayed = juce::jmap(fraction, history.getSample(channel, left),
                                                         history.getSample(channel, right));
                auto& filtered = tone[static_cast<size_t>(channel)];
                filtered += tapeTone * (delayed - filtered);
                history.setSample(channel, write, std::tanh(0.85f * (dry + feedback * filtered)) / 0.85f);
                buffer.setSample(channel, sample, dry + mix * (filtered - dry));
            }
            write = (write + 1) % length;
            wow += cycle * 0.42f / rate;
            flutter += cycle * 6.1f / rate;
            if (wow >= cycle) wow -= cycle;
            if (flutter >= cycle) flutter -= cycle;
        }
    }
    void getStateInformation(juce::MemoryBlock& destination) override
    {
        if (auto xml = state.copyState().createXml())
            copyXmlToBinary(*xml, destination);
    }

    void setStateInformation(const void* data, int bytes) override
    {
        if (auto xml = getXmlFromBinary(data, bytes))
            if (xml->hasTagName(state.state.getType()))
                state.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    State state;
    std::array<std::atomic<float>*, 4> values {};
    juce::AudioBuffer<float> history;
    std::array<juce::SmoothedValue<float>, 4> knobs;
    std::array<float, 2> tone {};
    float rate = 44100.0f;
    float tapeTone = 0.0f;
    float wow = 0.0f;
    float flutter = 0.0f;
    int write = 0;
};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrootEcho();
}





