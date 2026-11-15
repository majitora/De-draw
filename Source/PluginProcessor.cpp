#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout DeDrawProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "dur", 1 }, "Duration",
           StringArray { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bar", "4 bar", "ms" }, 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "ms", 1 }, "Free time (ms)",
           NormalisableRange<float> (10.0f, 4000.0f, 0.0f, 0.4f), 500.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "fbOn", 1 }, "Feedback On", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fb", 1 }, "Feedback",
           NormalisableRange<float> (0.0f, 0.95f), 0.45f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
           NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "out", 1 }, "Output (dB)",
           NormalisableRange<float> (-24.0f, 12.0f), 0.0f));
    const StringArray types { "Off", "Bitcrusher", "Distortion", "Lowpass" };
    const int defType[3] = { 1, 2, 0 };
    for (int s = 0; s < 3; ++s)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "fx" + String (s + 1) + "t", 1 },
               "FX" + String (s + 1) + " type", types, defType[s]));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "fx" + String (s + 1) + "a", 1 },
               "FX" + String (s + 1) + " amount", NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    }
    return l;
}

DeDrawProcessor::DeDrawProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < kSteps; ++i)
    {
        vol[(size_t) i].store (0.0f);
        pan[(size_t) i].store (0.0f);
        fxl[(size_t) i].store ((float) i / (float) (kSteps - 1));
    }
    const int idx[13]  = { 0, 3, 7, 9, 12, 14, 17, 19, 22, 24, 27, 29, 31 };
    const float h[13]  = { 150, 120, 96, 70, 84, 52, 60, 38, 44, 26, 30, 16, 10 };
    const float cy[13] = { 14, 34, 24, 40, 10, 26, 36, 18, 30, 12, 25, 38, 22 };
    for (int k = 0; k < 13; ++k)
    {
        vol[(size_t) idx[k]].store (h[k] / 165.0f);
        pan[(size_t) idx[k]].store ((cy[k] - 25.0f) / 15.0f);
    }
}

bool DeDrawProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DeDrawProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    buf.assign ((size_t) (41.0 * sr) + 8, 0.0f);
    wr = 0;
    smoothD = -1.0;
    std::fill (&cnt[0][0], &cnt[0][0] + 3 * kSteps, 0.0f);
    std::fill (&held[0][0], &held[0][0] + 3 * kSteps, 0.0f);
    std::fill (&zst[0][0], &zst[0][0] + 3 * kSteps, 0.0f);
}

void DeDrawProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = b.getNumSamples();
    if (b.getNumChannels() < 2 || buf.empty())
        return;
    float* L = b.getWritePointer (0);
    float* R = b.getWritePointer (1);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bb = pos->getBpm())
                bpm = juce::jlimit (20.0, 400.0, *bb);

    static const double beats[7] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    const int di = juce::jlimit (0, 7, (int) apvts.getRawParameterValue ("dur")->load());
    double durSec = di < 7 ? beats[di] * 60.0 / bpm : apvts.getRawParameterValue ("ms")->load() * 0.001;
    durSec = juce::jlimit (0.004, 40.0, durSec);
    if (smoothD < 0.0) smoothD = durSec;
    const double coef = 1.0 - std::exp (-1.0 / (0.08 * sr));

    const bool fbOn = apvts.getRawParameterValue ("fbOn")->load() > 0.5f;
    const float fb = apvts.getRawParameterValue ("fb")->load();
    const float mixv = apvts.getRawParameterValue ("mix")->load();
    const float outg = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load());

    static const char* const typeIds[3] = { "fx1t", "fx2t", "fx3t" };
    static const char* const amtIds[3]  = { "fx1a", "fx2a", "fx3a" };
    int type[3]; float slotAmt[3];
    for (int s = 0; s < 3; ++s)
    {
        type[s] = juce::jlimit (0, 3, (int) apvts.getRawParameterValue (typeIds[s])->load());
        slotAmt[s] = apvts.getRawParameterValue (amtIds[s])->load();
    }

    float v[kSteps], gl[kSteps], gr[kSteps];
    float amt[3][kSteps], lev[3][kSteps], hold[3][kSteps], drv[3][kSteps], lpc[3][kSteps];
    float sumV = 0.0f;
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < kSteps; ++i)
    {
        v[i] = vol[(size_t) i].load();
        if (v[i] < 0.001f) v[i] = 0.0f;
        const float p = juce::jlimit (-1.0f, 1.0f, pan[(size_t) i].load());
        const float ang = (p + 1.0f) * pi * 0.25f;
        gl[i] = std::cos (ang);
        gr[i] = std::sin (ang);
        sumV += v[i];
        const float f = juce::jlimit (0.0f, 1.0f, fxl[(size_t) i].load());
        for (int s = 0; s < 3; ++s)
        {
            const float a = type[s] == 0 ? 0.0f : slotAmt[s] * f;
            amt[s][i] = a;
            lev[s][i]  = std::pow (2.0f, 16.0f - 12.0f * a);
            hold[s][i] = 1.0f + a * 15.0f;
            drv[s][i]  = 1.0f + a * 19.0f;
            const float fc = 18000.0f * std::pow (0.02f, a);
            lpc[s][i] = 1.0f - std::exp (-2.0f * pi * fc / (float) sr);
        }
    }
    const float wetNorm = 1.0f / std::max (1.0f, 0.5f * sumV);
    const float fbNorm  = 1.0f / std::max (1.0f, sumV);
    const int N = (int) buf.size();
    float peak = 0.0f;

    for (int k = 0; k < n; ++k)
    {
        smoothD += (durSec - smoothD) * coef;
        const float inL = L[k], inR = R[k];
        const float mono = 0.5f * (inL + inR);
        float tl = 0.0f, tr = 0.0f, tm = 0.0f;

        for (int i = 0; i < kSteps; ++i)
        {
            if (v[i] <= 0.0f) continue;
            const double d = std::max (1.0, smoothD * (double) (i + 1) / (double) kSteps * sr);
            double rp = (double) wr - d;
            while (rp < 0.0) rp += (double) N;
            int i0 = (int) rp;
            if (i0 >= N) i0 = 0;
            const float fr = (float) (rp - (double) i0);
            int i1 = i0 + 1; if (i1 >= N) i1 = 0;
            float x = (buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0])) * v[i];

            for (int s = 0; s < 3; ++s)
            {
                const float a = amt[s][i];
                if (a < 0.002f) continue;
                if (type[s] == 1)
                {
                    cnt[s][i] += 1.0f;
                    if (cnt[s][i] >= hold[s][i]) { cnt[s][i] -= hold[s][i]; held[s][i] = x; }
                    const float half = lev[s][i] * 0.5f;
                    x = std::round (held[s][i] * half) / half;
                }
                else if (type[s] == 2)
                {
                    x = x * (1.0f - a) + a * 0.9f * std::tanh (drv[s][i] * x);
                }
                else if (type[s] == 3)
                {
                    zst[s][i] += lpc[s][i] * (x - zst[s][i]);
                    x = zst[s][i];
                }
            }
            tm += x;
            tl += x * gl[i];
            tr += x * gr[i];
        }

        const float fbs = fbOn ? std::tanh (tm * fbNorm * fb) : 0.0f;
        buf[(size_t) wr] = mono + fbs;
        if (++wr >= N) wr = 0;

        const float wl = tl * wetNorm, wrr = tr * wetNorm;
        peak = std::max (peak, std::max (std::abs (wl), std::abs (wrr)));
        L[k] = (inL * (1.0f - mixv) + wl * mixv) * outg;
        R[k] = (inR * (1.0f - mixv) + wrr * mixv) * outg;
    }
    level.store (std::max (level.load() * 0.85f, peak));
}

juce::String DeDrawProcessor::pack (const Layer& a)
{
    juce::StringArray t;
    for (int i = 0; i < kSteps; ++i) t.add (juce::String (a[(size_t) i].load(), 3));
    return t.joinIntoString (",");
}

void DeDrawProcessor::unpack (const juce::String& s, Layer& a)
{
    auto t = juce::StringArray::fromTokens (s, ",", "");
    for (int i = 0; i < kSteps && i < t.size(); ++i) a[(size_t) i].store (t[i].getFloatValue());
}

void DeDrawProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("vol", pack (vol), nullptr);
    state.setProperty ("pan", pack (pan), nullptr);
    state.setProperty ("fxl", pack (fxl), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void DeDrawProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto s = juce::ValueTree::fromXml (*xml);
        if (! s.isValid()) return;
        apvts.replaceState (s);
        if (s.hasProperty ("vol")) unpack (s["vol"].toString(), vol);
        if (s.hasProperty ("pan")) unpack (s["pan"].toString(), pan);
        if (s.hasProperty ("fxl")) unpack (s["fxl"].toString(), fxl);
    }
}

juce::AudioProcessorEditor* DeDrawProcessor::createEditor() { return new DeDrawEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeDrawProcessor(); }
