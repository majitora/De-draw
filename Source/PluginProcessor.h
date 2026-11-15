#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

class DeDrawProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kSteps = 32;
    using Layer = std::array<std::atomic<float>, kSteps>;

    DeDrawProcessor();
    ~DeDrawProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "de_draw"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    Layer& layerArr (int l) { return l == 0 ? vol : (l == 1 ? pan : fxl); }

    juce::AudioProcessorValueTreeState apvts;
    Layer vol, pan, fxl;          // per-echo: volume 0..1, pan -1..1 (-1 = left), fx amount 0..1
    std::atomic<float> level { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String pack (const Layer& a);
    static void unpack (const juce::String& s, Layer& a);

    std::vector<float> buf;
    int wr = 0;
    double sr = 44100.0;
    double smoothD = -1.0;
    float cnt[3][kSteps] {};
    float held[3][kSteps] {};
    float zst[3][kSteps] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeDrawProcessor)
};
